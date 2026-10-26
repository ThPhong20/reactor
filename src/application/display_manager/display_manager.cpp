#include "application/display_manager/display_manager.h"
#include "driver/tft/tft.h"

#include <cmath>
#include <cstdio>

namespace app::display_manager {
namespace {
using sensor_data::Status;
using display_data::Tone;
bool ready = false;
constexpr std::uint64_t stale_after_ms = 3000;

void reading(display_data::Row& value_row, display_data::Row& status_row,
             const char* name, const char* unit, const sensor_data::Reading& sample)
{
    const auto effective_status = sensor_data::effective_status(sample);
    if (sensor_data::has_value(sample)) {
        std::snprintf(value_row.text.data(), value_row.text.size(), "%s %.2f%s", name, sample.value, unit);
    } else {
        std::snprintf(value_row.text.data(), value_row.text.size(), "%s --%s", name, unit);
    }
    const char* status = "INVALID VALUE";
    status_row.tone = Tone::Error;
    switch (effective_status) {
    case Status::NotInitialized: status = "NOT READY"; status_row.tone = Tone::Warning; break;
    case Status::AdcError: status = "ADC ERROR"; break;
    case Status::VoltageOutOfRange: status = "VOLTAGE ERROR"; break;
    case Status::ValueOutOfRange: status = "OUT OF RANGE"; break;
    case Status::NeedsCalibration:
    case Status::Ok:
        status = effective_status == Status::Ok ? "CALIBRATED" : "NEEDS CALIBRATION";
        status_row.tone = effective_status == Status::Ok ? Tone::Good : Tone::Warning;
        break;
    }
    std::snprintf(status_row.text.data(), status_row.text.size(), "%s", status);
    value_row.tone = status_row.tone;
}
} // namespace

display_data::Frame format(const sensor_data::Record& record, std::uint64_t now_ms)
{
    display_data::Frame frame{};
    const bool empty = record.sequence == 0;
    const bool stale = !empty && (now_ms < record.sampled_at_ms ||
                                 now_ms - record.sampled_at_ms > stale_after_ms);
    std::snprintf(frame[0].text.data(), frame[0].text.size(), "%s",
                  empty ? "WAITING FOR DATA" : stale ? "STALE DATA" : "REACTOR SENSORS");
    frame[0].tone = empty || stale ? Tone::Warning : Tone::Normal;
    std::snprintf(frame[1].text.data(), frame[1].text.size(), "%llu S",
                  static_cast<unsigned long long>(record.sampled_at_ms / 1000));
    auto sensors = empty ? sensor_data::Snapshot{} : record.sensors;
    reading(frame[2], frame[3], "PH", "", sensors.ph);
    reading(frame[4], frame[5], "EC", " MS/CM", sensors.ec);
    reading(frame[6], frame[7], "TURB", " NTU", sensors.turbidity);
    if (!empty && std::isfinite(sensors.ec_temperature_c)) {
        std::snprintf(frame[8].text.data(), frame[8].text.size(), "T %.1f C MANUAL", sensors.ec_temperature_c);
    } else {
        std::snprintf(frame[8].text.data(), frame[8].text.size(), "T -- C MANUAL");
    }
    std::snprintf(frame[9].text.data(), frame[9].text.size(), "EC AT 25 C");
    return frame;
}

bool init(const system_config::DisplayConfig& config, const system_config::SensorConfig& sensors)
{
    ready = false;
    if (!config.enabled || !system_config::valid_display_config(config) ||
        !system_config::valid_pin_assignments(sensors, config)) {
        return false;
    }
    ready = drivers::tft::init(config);
    return ready;
}

bool update(const sensor_data::Record& record, std::uint64_t now_ms)
{
    if (!ready) return false;
    // Latch a failed display off until reboot. Sampling/logging continue.
    ready = drivers::tft::show(format(record, now_ms));
    return ready;
}
} // namespace app::display_manager
