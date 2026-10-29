#include "application/telemetry/telemetry.h"

#include "driver/serial/serial.h"

#include <cstdio>

namespace app::telemetry {

namespace {
// Translate a sensor status into a serial diagnostic message.
const char* status_text(sensor_data::Status status)
{
    switch (status) {
    case sensor_data::Status::Ok: return "calibrated";
    case sensor_data::Status::NeedsCalibration: return "estimate; needs calibration";
    case sensor_data::Status::NotInitialized: return "not initialized";
    case sensor_data::Status::AdcError: return "ADC read failed";
    case sensor_data::Status::VoltageOutOfRange: return "voltage outside usable range";
    case sensor_data::Status::ValueOutOfRange: return "sensor value outside range";
    }
    return "unknown status";
}

// Print a sensor value or an unavailable message with diagnostics.
void report_reading(const char* name, const char* unit, const sensor_data::Reading& reading)
{
    char message[192];
    const auto status = sensor_data::effective_status(reading);
    if (sensor_data::has_value(reading)) {
        std::snprintf(message, sizeof(message), "%s=%.3f %s | pin=%.1f mV board=%.1f mV | %s",
                      name, reading.value, unit, reading.pin_mv, reading.board_mv, status_text(status));
    } else {
        std::snprintf(message, sizeof(message), "%s unavailable | raw=%d pin=%.1f mV board=%.1f mV | %s",
                      name, reading.raw, reading.pin_mv, reading.board_mv, status_text(status));
    }
    drivers::serial::write_line(message);
}
} // namespace

// Print the firmware startup message.
void report_startup()
{
    drivers::serial::write_line("Reactor firmware bootstrap (C++).");
}

// Print the FSM state and its blocker or fault reason.
void report_state(const char* state, const char* reason)
{
    char message[128];
    std::snprintf(message, sizeof(message), "FSM: %s | %s", state, reason);
    drivers::serial::write_line(message);
}

// Report whether TFT updates are available.
void report_display_status(bool ready)
{
    drivers::serial::write_line(ready ? "TFT initialized (ST7789 240x240)."
                                    : "TFT disabled or failed; sensor storage and serial reporting continue.");
}

// Report pump initialization and the stopped startup condition.
void report_pump_initialization(bool ready)
{
    drivers::serial::write_line(ready
        ? "Pump PWM initialized; all pumps stopped. Verify all three flows together before operation."
        : "Pump PWM initialization failed; pump commands disabled.");
}

// Report sensor initialization and currently missing features.
void report_sensor_initialization(bool ready)
{
    drivers::serial::write_line(ready ? "pH and EC ADC inputs initialized."
                                    : "One or more sensor inputs failed initialization; check reading status.");
    drivers::serial::write_line("pH has no temperature compensation. EC temperature is MANUAL; default 25 C.");
    drivers::serial::write_line("Required turbidity acquisition and EC-based theoretical pH are not implemented yet.");
}

// Print pH, EC, and turbidity readings with their status.
void report_sensors(const sensor_data::Snapshot& readings)
{
    report_reading("pH", "pH", readings.ph);
    char name[64];
    std::snprintf(name, sizeof(name), "EC25 (manual T=%.2f C)", readings.ec_temperature_c);
    report_reading(name, "mS/cm", readings.ec);
    report_reading("Turbidity", "NTU", readings.turbidity);
}

} // namespace app::telemetry
