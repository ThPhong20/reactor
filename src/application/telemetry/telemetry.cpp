#include "application/telemetry/telemetry.h"

#include "driver/serial/serial.h"

#include <cstdio>

namespace app::telemetry {

namespace {
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

void report_startup()
{
    drivers::serial::write_line("Reactor firmware bootstrap (C++).");
}

void report_display_status(bool ready)
{
    drivers::serial::write_line(ready ? "TFT initialized (ST7789 240x240)."
                                    : "TFT disabled or failed; sensor storage and serial reporting continue.");
}

void report_pump_initialization(bool ready)
{
    drivers::serial::write_line(ready
        ? "Pump PWM initialized; all pumps stopped. Outlet duties need manual calibration."
        : "Pump PWM initialization failed; pump commands disabled.");
}

void report_sensor_initialization(bool ready)
{
    drivers::serial::write_line(ready ? "pH and EC ADC inputs initialized."
                                    : "One or more sensor inputs failed initialization; check reading status.");
    drivers::serial::write_line("pH has no temperature compensation. EC temperature is MANUAL; default 25 C.");
    drivers::serial::write_line("Required turbidity acquisition and EC-based theoretical pH are not implemented yet.");
}

void report_sensors(const sensor_data::Snapshot& readings)
{
    report_reading("pH", "pH", readings.ph);
    char name[64];
    std::snprintf(name, sizeof(name), "EC25 (manual T=%.2f C)", readings.ec_temperature_c);
    report_reading(name, "mS/cm", readings.ec);
    report_reading("Turbidity", "NTU", readings.turbidity);
}

} // namespace app::telemetry
