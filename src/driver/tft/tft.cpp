#include "driver/tft/tft.h"
#include "hal/lcd/lcd.h"

namespace drivers::tft {
namespace {
bool ready = false;
// Static workspace avoids placing the 11.25 KiB strip on the main task stack.
display_data::Pixels pixels{};
constexpr int glyph_width = 5;
constexpr int glyph_height = 7;
constexpr int font_scale = 2;
constexpr int left_margin = 6;
constexpr int top_margin = 5;
constexpr int character_advance = (glyph_width + 1) * font_scale;
static_assert(left_margin + display_data::text_columns * character_advance <= display_data::width,
              "Text must fit in the strip width");
static_assert(top_margin + glyph_height * font_scale <= display_data::row_height,
              "Font must fit in the strip height");
struct Glyph { char character; unsigned char columns[5]; };
// Small 5x7 font, column bits top to bottom. All display labels use uppercase.
constexpr Glyph font[] = {
    {'0',{0x3e,0x51,0x49,0x45,0x3e}}, {'1',{0,0x42,0x7f,0x40,0}},
    {'2',{0x62,0x51,0x49,0x49,0x46}}, {'3',{0x22,0x41,0x49,0x49,0x36}},
    {'4',{0x18,0x14,0x12,0x7f,0x10}}, {'5',{0x27,0x45,0x45,0x45,0x39}},
    {'6',{0x3c,0x4a,0x49,0x49,0x30}}, {'7',{1,0x71,9,5,3}},
    {'8',{0x36,0x49,0x49,0x49,0x36}}, {'9',{6,0x49,0x49,0x29,0x1e}},
    {'A',{0x7e,9,9,9,0x7e}}, {'B',{0x7f,0x49,0x49,0x49,0x36}},
    {'C',{0x3e,0x41,0x41,0x41,0x22}}, {'D',{0x7f,0x41,0x41,0x22,0x1c}},
    {'E',{0x7f,0x49,0x49,0x49,0x41}}, {'F',{0x7f,9,9,9,1}},
    {'G',{0x3e,0x41,0x49,0x49,0x7a}}, {'H',{0x7f,8,8,8,0x7f}},
    {'I',{0,0x41,0x7f,0x41,0}}, {'J',{0x20,0x40,0x41,0x3f,1}},
    {'K',{0x7f,8,0x14,0x22,0x41}}, {'L',{0x7f,0x40,0x40,0x40,0x40}},
    {'M',{0x7f,2,0x0c,2,0x7f}}, {'N',{0x7f,4,8,0x10,0x7f}},
    {'O',{0x3e,0x41,0x41,0x41,0x3e}}, {'P',{0x7f,9,9,9,6}},
    {'Q',{0x3e,0x41,0x51,0x21,0x5e}}, {'R',{0x7f,9,0x19,0x29,0x46}},
    {'S',{0x46,0x49,0x49,0x49,0x31}}, {'T',{1,1,0x7f,1,1}},
    {'U',{0x3f,0x40,0x40,0x40,0x3f}}, {'V',{0x1f,0x20,0x40,0x20,0x1f}},
    {'W',{0x3f,0x40,0x38,0x40,0x3f}}, {'X',{0x63,0x14,8,0x14,0x63}},
    {'Y',{7,8,0x70,8,7}}, {'Z',{0x61,0x51,0x49,0x45,0x43}},
    {'.',{0,0x60,0x60,0,0}}, {'-',{8,8,8,8,8}},
    {'/',{0x20,0x10,8,4,2}}, {':',{0,0x36,0x36,0,0}},
};

std::uint16_t color(display_data::Tone tone)
{
    switch (tone) {
    case display_data::Tone::Good: return 0x07e0;
    case display_data::Tone::Warning: return 0xffe0;
    case display_data::Tone::Error: return 0xf800;
    default: return 0xffff;
    }
}

void draw(const display_data::Row& row)
{
    pixels.fill(0); // Clear the entire strip, including old/longer values.
    const auto text_color = color(row.tone);
    for (std::size_t i = 0; i < display_data::text_columns && row.text[i]; ++i) {
        for (const auto& glyph : font) {
            if (glyph.character != row.text[i]) {
                continue;
            }
            for (int col = 0; col < glyph_width; ++col) {
                for (int bit = 0; bit < glyph_height; ++bit) {
                    if (!(glyph.columns[col] & (1 << bit))) {
                        continue;
                    }
                    const int x = left_margin + static_cast<int>(i) * character_advance + col * font_scale;
                    const int y = top_margin + bit * font_scale;
                    for (int dy = 0; dy < font_scale; ++dy) {
                        for (int dx = 0; dx < font_scale; ++dx) {
                            pixels[(y + dy) * display_data::width + x + dx] = text_color;
                        }
                    }
                }
            }
            break;
        }
    }
}
} // namespace

bool init(const system_config::DisplayConfig& config)
{
    ready = hal::lcd::init(config);
    return ready;
}

bool show(const display_data::Frame& frame)
{
    if (!ready) return false;
    for (std::size_t row = 0; row < frame.size(); ++row) {
        draw(frame[row]);
        if (!hal::lcd::write_row(static_cast<int>(row), pixels)) {
            ready = false;
            return false;
        }
    }
    // The first complete frame is written before the panel becomes visible.
    ready = hal::lcd::enable();
    return ready;
}
} // namespace drivers::tft
