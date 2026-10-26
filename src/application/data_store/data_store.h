#pragma once

#include "common/sensor_data.h"
#include <array>
#include <cstddef>

namespace app::data_store {

// Fixed-size RAM history: oldest sample is overwritten when full. No flash wear
// or heap allocation. Call only from the application task; not thread-safe.
class Store {
public:
    static constexpr std::size_t capacity = 60;
    void clear();
    void push(const sensor_data::Snapshot& sensors, std::uint64_t sampled_at_ms);
    // Copy out to keep callers independent of ring-buffer overwrite.
    bool latest(sensor_data::Record& out) const;
    bool newest(std::size_t age, sensor_data::Record& out) const;
    std::size_t size() const { return count_; }

private:
    std::array<sensor_data::Record, capacity> records_{};
    std::size_t next_ = 0;
    std::size_t count_ = 0;
    std::uint64_t sequence_ = 0;
};

} // namespace app::data_store
