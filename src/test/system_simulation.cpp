// Runs the real application loop against deterministic simulated peripherals.
// Virtual time is a test input, not a measured ESP32 timing or an RTOS emulator.
#include "application/app.h"
#include "hal/adc/adc.h"
#include "hal/lcd/lcd.h"
#include "hal/pwm/pwm.h"
#include "hal/timers_watchdog/timers_watchdog.h"
#include "driver/serial/serial.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr unsigned iterations = 90;
constexpr int ph_gpio = 8;
constexpr int ec_gpio = 10;
constexpr std::size_t gpio_count = 11; // GPIO-indexed fake ADC1 inputs, 0..10.
struct Finished {};
unsigned cycle = 0;
std::uint64_t now_ms = 0;
std::uint64_t previous_sample_ms = 0;
std::uint64_t max_interval_ms = 0;
bool faults = false;
bool slow_sensor = false;
bool display_failed = false;
unsigned frames = 0;
unsigned ph_errors = 0;
unsigned ec_errors = 0;
unsigned rows_after_failure = 0;
std::array<bool, gpio_count> adc_ready{};
std::array<bool, 3> pump_ready{};
std::array<float, 3> duty{};
std::array<unsigned, gpio_count> adc_reads{};
std::array<std::uint16_t, display_data::width * display_data::height> screen{};
std::vector<std::string> messages;
std::filesystem::path output;
std::ofstream trace;
std::ofstream serial_log;

// Fail the test immediately when a required condition is false.
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

// Select the cycles with an injected pH ADC failure.
bool ph_failure() { return faults && cycle >= 20 && cycle < 25; }
// Select the cycles with an injected EC ADC failure.
bool ec_failure() { return faults && cycle >= 40 && cycle < 45; }
// Select the cycles with simulated slow TFT transfers.
bool slow_display() { return faults && cycle >= 10 && cycle < 20; }

// Search captured telemetry for an expected message.
bool logged(const std::string& text)
{
    return std::any_of(messages.begin(), messages.end(), [&](const auto& line) {
        return line.find(text) != std::string::npos;
    });
}

// Export the actual renderer's RGB565 pixels as an SVG made of horizontal runs.
// This is a framebuffer capture, not a separate drawing of the intended UI.
void capture(const char* name)
{
    std::ofstream file(output / name);
    require(static_cast<bool>(file), "cannot create framebuffer capture");
    file << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"480\" height=\"480\" viewBox=\"0 0 240 240\" shape-rendering=\"crispEdges\">\n";
    file << "<title>Simulated TFT framebuffer - synthetic sensor inputs</title>\n";
    for (int y = 0; y < display_data::height; ++y) {
        for (int x = 0; x < display_data::width;) {
            const auto color = screen[y * display_data::width + x];
            int end = x + 1;
            while (end < display_data::width && screen[y * display_data::width + end] == color) ++end;
            const int r = ((color >> 11) & 31) * 255 / 31;
            const int g = ((color >> 5) & 63) * 255 / 63;
            const int b = (color & 31) * 255 / 31;
            file << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << end-x
                 << "\" height=\"1\" fill=\"rgb(" << r << ',' << g << ',' << b << ")\"/>\n";
            x = end;
        }
    }
    file << "</svg>\n";
}
} // namespace

namespace hal::pwm {
// Validate the simulated peripheral setup and report success.
bool init(std::uint32_t frequency)
{
    require(frequency == 10000, "unexpected PWM frequency");
    return true;
}
// Check pump wiring and initialize its simulated output to zero.
bool configure_channel(unsigned channel, int gpio)
{
    require(channel < 3 && gpio == static_cast<int>(channel) + 4, "incorrect pump pins");
    pump_ready[channel] = true;
    duty[channel] = 0;
    return true;
}
// Track a simulated pump write and reject unintended startup.
bool set_duty(unsigned channel, float percent)
{
    require(channel < 3 && pump_ready[channel], "uninitialized pump write");
    duty[channel] = percent;
    require(percent == 0, "monitoring application unexpectedly started a pump");
    return true;
}
// Set one initialized simulated pump output to zero.
bool stop(unsigned channel)
{
    require(channel < 3 && pump_ready[channel], "uninitialized pump stop");
    duty[channel] = 0;
    return true;
}
} // namespace hal::pwm

namespace hal::adc {
// Validate and mark a simulated sensor ADC input ready.
bool configure(int gpio)
{
    require(gpio == ph_gpio || gpio == ec_gpio, "unexpected sensor GPIO");
    adc_ready[gpio] = true;
    return true;
}
// Advance virtual time and return synthetic ADC data or a failure.
bool read(int gpio, Sample& sample)
{
    require((gpio == ph_gpio || gpio == ec_gpio) && adc_ready[gpio], "uninitialized ADC input");
    ++adc_reads[gpio];
    now_ms += 4; // Assumed duration for each averaged ADC read, not a benchmark.
    if (slow_sensor && cycle == 30 && gpio == ec_gpio) now_ms += 4000;
    sample = {};
    if ((gpio == ph_gpio && ph_failure()) || (gpio == ec_gpio && ec_failure())) return false;
    // Synthetic board voltages. These exercise the real sensor conversions;
    // they are not a model of reaction chemistry, precipitation, or flow.
    const float wave = std::sin(static_cast<float>(cycle) * 0.1f);
    sample.raw = gpio == ph_gpio ? 2000 : 1000;
    sample.millivolts = gpio == ph_gpio ? 1500.0f + 40.0f * wave
                                : (1.413f + 0.1f * wave) * 164.0f / 1.5f;
    return true;
}
} // namespace hal::adc

namespace hal::lcd {
// Validate the simulated peripheral setup and report success.
bool init(const system_config::DisplayConfig& config)
{
    require(config.sclk == 7 && config.mosi == 9 && config.dc == 2 && config.reset == 1,
            "unexpected TFT wiring");
    return true;
}
// Simulate TFT transfer delay, framebuffer writes, and timeout.
bool write_row(int row, const display_data::Pixels& pixels)
{
    if (slow_sensor && row == 0 && (cycle == 30 || cycle == 31)) {
        const bool warns = std::count(pixels.begin(), pixels.end(), 0xffe0) > 0;
        require(warns == (cycle == 30), "slow acquisition must show stale data, then recover");
    }
    if (display_failed) ++rows_after_failure;
    require(row >= 0 && row < static_cast<int>(display_data::row_count), "invalid display row");
    if (faults && cycle == 60) {
        now_ms += 250; // Model the real HAL's timeout response, not its DMA implementation.
        display_failed = true;
        return false;
    }
    now_ms += slow_display() ? 20 : 2; // 200 ms vs 20 ms per full frame.
    std::copy(pixels.begin(), pixels.end(), screen.begin() + row * pixels.size());
    return true;
}
// Count completed frames and save selected framebuffer captures.
bool enable()
{
    ++frames;
    if (cycle == 5) capture("tft-normal.svg");
    if (cycle == 22) capture("tft-ph-check.svg");
    return true;
}
} // namespace hal::lcd

namespace drivers::serial {
// Capture telemetry and write it to the simulation serial log.
void write_line(const char* message)
{
    messages.emplace_back(message);
    serial_log << now_ms << " ms | " << message << '\n';
}
} // namespace drivers::serial

namespace hal::timers_watchdog {
// Return the current virtual clock time.
std::uint64_t uptime_ms() { return now_ms; }
// Validate one application cycle, record its trace, and advance virtual time.
void delay_ms(int interval_ms)
{
    require(interval_ms == 1000, "unexpected application delay");
    require(duty == std::array<float, 3>{0, 0, 0}, "pumps must remain off");
    require(logged("Turbidity unavailable"), "missing required turbidity must remain visible");
    require(logged("EC25 (manual T=25.00 C)"), "EC temperature assumption must be visible");
    require(logged("pH unavailable") == ph_failure(), "pH failure/recovery mismatch");
    require(logged("EC25 (manual T=25.00 C) unavailable") == ec_failure(), "EC failure/recovery mismatch");
    if (ph_failure()) {
        ++ph_errors;
        require(!logged("pH="), "stale pH exposed after ADC failure");
    }
    if (ec_failure()) ++ec_errors;
    if (faults && cycle == 60) require(logged("TFT disabled or failed"), "display failure not reported");
    const auto interval = cycle ? now_ms - previous_sample_ms : 0;
    max_interval_ms = std::max(max_interval_ms, interval);
    previous_sample_ms = now_ms;
    trace << cycle << ',' << now_ms << ',' << interval << ','
          << (ph_failure() ? "ADC_ERROR" : "ESTIMATE") << ','
          << (ec_failure() ? "ADC_ERROR" : "ESTIMATE") << ",NOT_READY,"
          << (display_failed ? "FAILED" : slow_display() ? "SLOW" : "OK") << ",0,0,0\n";
    messages.clear();
    now_ms += interval_ms;
    if (++cycle == iterations) throw Finished{};
}
} // namespace hal::timers_watchdog

// Run the actual application with simulated peripherals for 90 cycles.
int main(int argc, char** argv)
{
    try {
        require(argc == 3, "usage: system_simulation normal|faults|slow_sensor OUTPUT_DIRECTORY");
        const std::string mode = argv[1];
        require(mode == "normal" || mode == "faults" || mode == "slow_sensor", "unknown scenario");
        faults = mode == "faults";
        slow_sensor = mode == "slow_sensor";
        output = argv[2];
        std::filesystem::create_directories(output);
        trace.open(output / "trace.csv");
        serial_log.open(output / "serial.log");
        require(trace && serial_log, "cannot create simulation output");
        trace << "cycle,report_time_ms,report_interval_ms,ph,ec,turbidity,display,inlet_duty,outlet1_duty,outlet2_duty\n";
        app::init();
        require(logged("all pumps stopped") && logged("TFT initialized"), "startup checks failed");
        require(app::reactor_state() == app::state_machine::State::Idle, "monitoring boots to IDLE");
        require(!app::command(app::state_machine::Event::Start), "production configuration must reject START");
        require(logged("VERIFY PUMP FLOWS"), "blocked-start reason must be reported");
        messages.clear();
        try { app::run(); } catch (const Finished&) { /* End virtual run at its delay boundary. */ }
        require(cycle == iterations && adc_reads[ph_gpio] == iterations && adc_reads[ec_gpio] == iterations,
                "sampling did not continue for the full run");
        require(ph_errors == (faults ? 5U : 0U) && ec_errors == (faults ? 5U : 0U), "unexpected sensor errors");
        require(rows_after_failure == 0, "display retried after latched failure");
        require(frames == (faults ? 61U : 91U), "unexpected completed frame count");
        std::ofstream summary(output / "summary.txt");
        summary << "PASS: " << mode << " scenario\n"
                << "Real app.cpp, sensor/pump managers and drivers, RAM store, TFT renderer, telemetry.\n"
                << "Simulated HALs and virtual time; no FreeRTOS kernel or physical hardware.\n"
                << "Cycles: " << cycle << "; virtual elapsed ms: " << now_ms << '\n'
                << "pH/EC read attempts: " << adc_reads[ph_gpio] << '/' << adc_reads[ec_gpio] << '\n'
                << "Injected pH/EC error samples: " << ph_errors << '/' << ec_errors << '\n'
                << "Completed frames (including startup): " << frames << '\n'
                << "Maximum report interval ms: " << max_interval_ms << '\n'
                << "All pumps remained at zero duty.\n"
                << "Turbidity unavailable; theoretical pH not calculated.\n";
        summary.close();
        std::cout << std::ifstream(output / "summary.txt").rdbuf();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "SIMULATION FAILED: " << error.what() << '\n';
        return 1;
    }
}
