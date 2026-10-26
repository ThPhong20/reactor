#include "application/data_store/data_store.h"
#include "application/display_manager/display_manager.h"
#include "hal/lcd/lcd.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
using sensor_data::Status;
using display_data::Tone;
unsigned init_calls = 0;
unsigned writes = 0;
unsigned enables = 0;
bool init_ok = true;
bool enable_ok = true;
int fail_row = -1;
std::vector<display_data::Pixels> drawn;
void require(bool condition, const char* message)
{
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
std::string text(const display_data::Frame& frame, int row) { return frame[row].text.data(); }
sensor_data::Record example()
{
    sensor_data::Record result;
    result.sequence = 1;
    result.sampled_at_ms = 1000;
    result.sensors.ph.status = Status::Ok;
    result.sensors.ph.value = 7;
    result.sensors.ec.status = Status::NeedsCalibration;
    result.sensors.ec.value = 1.413f;
    return result;
}
} // namespace

namespace hal::lcd {
bool init(const system_config::DisplayConfig&) { ++init_calls; return init_ok; }
bool write_row(int row, const display_data::Pixels& pixels)
{
    require(row == static_cast<int>(writes % 10), "rows must cover panel in order");
    ++writes;
    drawn.push_back(pixels);
    return row != fail_row;
}
bool enable() { ++enables; return enable_ok; }
} // namespace hal::lcd

int main(int argc, char** argv)
{
    require(argc == 2, "provide scenario");
    const std::string scenario = argv[1];
    auto record = example();
    namespace display = app::display_manager;
    if (scenario == "storage") {
        app::data_store::Store history;
        sensor_data::Record saved = record;
        require(!history.latest(saved) && saved.sequence == 0, "empty store must clear output");
        for (unsigned i = 1; i <= 65; ++i) {
            record.sensors.ph.raw = i;
            history.push(record.sensors, i * 1000);
        }
        require(history.size() == 60, "history is bounded");
        require(history.latest(saved) && saved.sequence == 65 && saved.sampled_at_ms == 65000,
                "latest timestamp/sequence after wrap");
        require(history.newest(59, saved) && saved.sequence == 6 && saved.sensors.ph.raw == 6,
                "oldest retained record after wrap");
        require(!history.newest(60, saved) && saved.sequence == 0, "no overwritten sample exposed");
        record.sensors.ph = {};
        record.sensors.ph.status = Status::AdcError;
        history.push(record.sensors, 66000);
        record.sensors.ec.value = 999; // The store must own a copy.
        require(history.latest(saved) && saved.sensors.ph.status == Status::AdcError &&
                std::isnan(saved.sensors.ph.value) && saved.sensors.ec.value < 2,
                "preserve fresh errors, no aliasing or old good values");
        history.clear();
        require(history.size() == 0 && !history.latest(saved), "clear history");
        history.push({}, 70000);
        require(history.latest(saved) && saved.sequence == 1, "sequence resets with history");
        return 0;
    }
    if (scenario == "format") {
        auto frame = display::format(record, 1000);
        require(text(frame, 2) == "PH 7.00" && text(frame, 4) == "EC 1.41 MS/CM", "values and units");
        require(text(frame, 3) == "CALIBRATED" && frame[3].tone == Tone::Good, "calibrated status");
        require(text(frame, 5) == "NEEDS CALIBRATION" && frame[5].tone == Tone::Warning, "estimate warning");
        require(text(frame, 6) == "TURB -- NTU" && text(frame, 7) == "NOT READY", "no invented turbidity");
        require(text(frame, 8) == "T 25.0 C MANUAL" && text(frame, 9) == "EC AT 25 C", "temperature meaning");
        require(text(display::format(record, 4000), 0) == "REACTOR SENSORS", "fresh boundary");
        require(text(display::format(record, 4001), 0) == "STALE DATA", "stale record label");
        require(text(display::format(record, 0), 0) == "STALE DATA", "future timestamp rejected");
        for (auto status : {Status::AdcError, Status::VoltageOutOfRange, Status::ValueOutOfRange}) {
            record.sensors.ph.status = status; // Keep old number to test suppression.
            frame = display::format(record, 1000);
            require(text(frame, 2) == "PH --" && frame[3].tone == Tone::Error, "failure must hide old value");
        }
        record.sensors.ph.status = Status::Ok;
        for (float invalid : {std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::infinity(), -1.0f}) {
            record.sensors.ph.value = invalid;
            frame = display::format(record, 1000);
            require(text(frame, 2) == "PH --" && text(frame, 3) == "OUT OF RANGE", "reject nonfinite/bad values");
        }
        frame = display::format({}, 1000);
        require(text(frame, 0) == "WAITING FOR DATA" && text(frame, 2) == "PH --", "boot screen");
        return 0;
    }
    if (scenario == "pins") {
        auto config = system_config::default_display_config();
        config.enabled = false;
        require(!display::init(config), "disabled config");
        for (int pin : {1, 2, 4, 5, 6, -1, 11, 19, 26, 35}) {
            config = system_config::default_display_config();
            config.dc = pin;
            require(!display::init(config), "reject reserved/duplicate/unset pin");
        }
        config = system_config::default_display_config();
        for (int offset : {-1, 81}) {
            config.y_offset = offset;
            require(!display::init(config), "reject vertical offset outside panel memory");
        }
        config = system_config::default_display_config();
        auto sensors = system_config::default_sensor_config();
        sensors.ec.gpio = 7;
        config.dc = 7;
        require(!display::init(config, sensors), "honor custom sensor pin assignments");
        config.enabled = false;
        require(system_config::valid_pin_assignments(sensors, config), "disabled TFT releases its pins");
        require(init_calls == 0, "invalid config must not touch hardware");
        require(!display::update(record, 1000) && writes == 0, "no output before init");
        config.enabled = true;
        config.dc = 8;
        require(display::init(config, sensors) && init_calls == 1, "allow nonconflicting custom pins");
        return 0;
    }
    if (scenario == "init_failure") {
        init_ok = false;
        require(!display::init(), "propagate LCD init failure");
        require(!display::update(record, 1000) && writes == 0, "do not write failed LCD");
        return 0;
    }
    require(display::init(), "default display init");
    if (scenario == "transfer_failure") {
        fail_row = 2;
        require(!display::update(record, 1000) && writes == 3 && enables == 0, "stop at failed transfer");
        require(!display::update(record, 2000) && writes == 3, "failure latched, no DMA buffer reuse");
        return 0;
    }
    if (scenario == "enable_failure") {
        enable_ok = false;
        require(!display::update(record, 1000) && writes == 10 && enables == 1, "display enable failure");
        require(!display::update(record, 2000) && writes == 10, "enable failure latched");
        return 0;
    }
    require(scenario == "render", "unknown scenario");
    require(display::update(record, 1000) && writes == 10 && enables == 1, "full screen delivered");
    require(std::count(drawn[2].begin(), drawn[2].end(), 0x07e0) > 0, "calibrated reading drawn green");
    require(std::count(drawn[4].begin(), drawn[4].end(), 0xffe0) > 0, "estimate drawn yellow");
    record.sensors.ph.status = Status::AdcError;
    require(display::update(record, 2000) && writes == 20, "screen updates after sensor failure");
    require(std::count(drawn[12].begin(), drawn[12].end(), 0x07e0) == 0 &&
            std::count(drawn[12].begin(), drawn[12].end(), 0xf800) > 0, "old green value erased, red error shown");
    return 0;
}
