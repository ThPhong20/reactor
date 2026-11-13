#include "application/hardware_test/hardware_test.h"
#include "application/hardware_test/session.h"
#include "application/sensor_manager/sensor_manager.h"
#include "application/display_manager/display_manager.h"
#include "application/data_store/data_store.h"
#include "application/telemetry/telemetry.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "application/pump_manager/pump_manager.h"
#include "hal/test_console/test_console.h"
#include "hal/timers_watchdog/timers_watchdog.h"
#include "esp_task_wdt.h"
#include "sdkconfig.h"

#if !CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG || !CONFIG_ESP_TASK_WDT_INIT || !CONFIG_ESP_TASK_WDT_PANIC
#error "Hardware test requires native USB and task watchdog; use make hardware-build with its separate defaults"
#endif

namespace app::hardware_test {
namespace {
// TS-300B 10k/10k divider confirmed installed for physical testing (2026-11-13).
constexpr bool test_turbidity_enabled = true;
struct ControlStatus { bool running; bool fault; };
QueueHandle_t samples = nullptr;
QueueHandle_t control_status = nullptr;
// The monitoring task owns this history and all sensor/display state.
data_store::Store history;

// Sample and render in a low-priority task; never call actuator APIs here.
void monitor(void*)
{
    auto sensors = system_config::default_sensor_config();
    sensors.turbidity_enabled = test_turbidity_enabled;
    const auto display = system_config::default_display_config();
    const bool sensors_ready = sensor_manager::init(sensors, display);
    bool display_ready = display_manager::init(display, sensors);
    telemetry::report_sensor_initialization(sensors_ready);
    telemetry::report_display_status(display_ready);
    history.clear();
    while (true) {
        const auto started = hal::timers_watchdog::uptime_ms();
        history.push(sensor_manager::sample(), started);
        sensor_data::Record record;
        history.latest(record);
        // Copy before display work. If this task then stalls, control detects age.
        xQueueOverwrite(samples, &record);
        ControlStatus status{};
        xQueuePeek(control_status, &status, 0);
        if (display_ready && !display_manager::update(record,
                hal::timers_watchdog::uptime_ms(),
                status.fault ? "HW TEST FAULT" : status.running ? "HW TEST RUN" : "HW TEST IDLE",
                status.running ? "TIMED PUMPS ON" : "PUMPS OFF / TEST")) {
            display_ready = false;
            telemetry::report_display_status(false);
        }
        telemetry::report_sensors(record.sensors);
        hal::timers_watchdog::delay_ms(system_config::default_config().sample_interval_ms);
    }
}
} // namespace
// Poll bounded serial batches and deadlines from the single actuator owner task.
[[noreturn]] void run()
{
    const bool pumps_ready = pump_manager::init();
    const bool console_ready = hal::test_console::init();
    const bool watchdog_ready = esp_task_wdt_add(nullptr) == ESP_OK;
    samples = xQueueCreate(1, sizeof(sensor_data::Record));
    control_status = xQueueCreate(1, sizeof(ControlStatus));
    // The control task must remain runnable while the monitor waits for I/O.
    vTaskPrioritySet(nullptr, 3);
    const bool monitor_ready = samples && control_status &&
        xTaskCreate(monitor, "hardware_monitor", 8192, nullptr, 1, nullptr) == pdPASS;
    Session controller({pump_manager::set_manual_duty, pump_manager::stop_all},
                       pumps_ready && console_ready && watchdog_ready && monitor_ready,
                       test_turbidity_enabled);
    controller.stop();
    char line[96]{};
    std::size_t length = 0;
    bool discard_line = false;
    bool was_connected = false;
    std::uint64_t last_byte_ms = 0;
    while (true) {
        const auto now = hal::timers_watchdog::uptime_ms();
        sensor_data::Record latest;
        if (samples && xQueuePeek(samples, &latest, 0) == pdTRUE) controller.update(latest);
        const bool connected = console_ready && hal::test_console::connected();
        bool stopped = controller.tick(now);
        if (!connected && controller.running()) {
            controller.stop();
            stopped = true;
        }
        if (stopped) {
            length = 0;
            discard_line = false;
            if (console_ready) {
                hal::test_console::discard_input();
                hal::test_console::reply(controller.status(now));
            }
        }
        if (connected && !was_connected) {
            length = 0;
            discard_line = false;
            hal::test_console::discard_input();
            hal::test_console::reply("INTEGRATED HARDWARE TEST: pH/EC/TFT/history + timed pumps; electrode power disconnected; help; ! stops");
            hal::test_console::reply(controller.status(now));
        }
        was_connected = connected;
        if (length && now - last_byte_ms > 10000) {
            length = 0;
            discard_line = true; // An expired prefix must not turn its suffix into a command.
        }
        char bytes[32];
        const int count = connected ? hal::test_console::read(bytes, sizeof(bytes)) : 0;
        for (int i = 0; i < count; ++i) {
            const char byte = bytes[i];
            last_byte_ms = hal::timers_watchdog::uptime_ms();
            if (byte == '!') {
                hal::test_console::reply(controller.stop());
                length = 0;
                discard_line = false;
                hal::test_console::discard_input();
                break;
            }
            if (byte == '\r' || byte == '\n') {
                if (!discard_line && length) {
                    line[length] = '\0';
                    const bool was_running = controller.running();
                    hal::test_console::reply(controller.command(line, last_byte_ms));
                    length = 0;
                    if (was_running && !controller.running()) {
                        hal::test_console::discard_input();
                        break;
                    }
                }
                length = 0;
                discard_line = false;
            } else if (!discard_line) {
                if (byte < 32 || byte > 126 || length + 1 >= sizeof(line)) {
                    length = 0;
                    discard_line = true;
                    hal::test_console::reply("ERROR: invalid/overlong line; send Enter and retry");
                } else {
                    line[length++] = byte;
                }
            }
        }
        if (control_status) {
            const ControlStatus status{controller.running(), controller.faulted()};
            xQueueOverwrite(control_status, &status);
        }
        if (watchdog_ready && esp_task_wdt_reset() != ESP_OK) controller.disable();
        hal::timers_watchdog::delay_ms(10);
    }
}
} // namespace app::hardware_test
