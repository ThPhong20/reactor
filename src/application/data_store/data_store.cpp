#include "application/data_store/data_store.h"

namespace app::data_store {

void Store::clear()
{
    next_ = count_ = 0;
    sequence_ = 0;
}

void Store::push(const sensor_data::Snapshot& sensors, std::uint64_t sampled_at_ms)
{
    // Store failures as well as successes; never replace a failed fresh sample
    // with an older valid value that could mislead the operator.
    records_[next_] = {sensors, sampled_at_ms, ++sequence_};
    next_ = (next_ + 1) % capacity;
    if (count_ < capacity) ++count_;
}

bool Store::latest(sensor_data::Record& out) const
{
    return newest(0, out);
}

bool Store::newest(std::size_t age, sensor_data::Record& out) const
{
    if (age >= count_) {
        out = {};
        return false;
    }
    out = records_[(next_ + capacity - 1 - age) % capacity];
    return true;
}

} // namespace app::data_store
