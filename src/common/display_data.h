#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace display_data {
inline constexpr int width = 240;
inline constexpr int height = 240;
inline constexpr int row_height = 24;
inline constexpr std::size_t row_count = height / row_height;
inline constexpr std::size_t text_columns = 19;
static_assert(height % row_height == 0, "Display rows must cover the complete panel");
enum class Tone { Normal, Good, Warning, Error };
struct Row {
    std::array<char, text_columns + 1> text{};
    Tone tone = Tone::Normal;
};
using Frame = std::array<Row, row_count>;
using Pixels = std::array<std::uint16_t, width * row_height>;
} // namespace display_data
