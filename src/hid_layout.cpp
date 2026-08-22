/*
 * hid_layout の実装 (仕様書 F-05)
 *
 * JIS 配列 (106/109 キー) で ASCII 印字可能文字を打鍵するための対応表。
 * 英字は US と同じ位置にあるため式で求め、記号と数字のみ表で引く。
 */
#include "hid_layout.h"

namespace nfccmd {

namespace {

// ASCII 0x20〜0x40 に対応するキーコードと Shift の要否
constexpr KeyStroke kTable20To40[] = {
    {0x2C, false},  // 0x20 ' '
    {0x1E, true},   // 0x21 '!'  Shift+1
    {0x1F, true},   // 0x22 '"'  Shift+2 (US は Shift+')
    {0x20, true},   // 0x23 '#'  Shift+3
    {0x21, true},   // 0x24 '$'  Shift+4
    {0x22, true},   // 0x25 '%'  Shift+5
    {0x23, true},   // 0x26 '&'  Shift+6 (US は Shift+7)
    {0x24, true},   // 0x27 '\'' Shift+7 (US は独立キー)
    {0x25, true},   // 0x28 '('  Shift+8 (US は Shift+9)
    {0x26, true},   // 0x29 ')'  Shift+9 (US は Shift+0)
    {0x34, true},   // 0x2A '*'  Shift+:
    {0x33, true},   // 0x2B '+'  Shift+;
    {0x36, false},  // 0x2C ','
    {0x2D, false},  // 0x2D '-'
    {0x37, false},  // 0x2E '.'
    {0x38, false},  // 0x2F '/'
    {0x27, false},  // 0x30 '0'
    {0x1E, false},  // 0x31 '1'
    {0x1F, false},  // 0x32 '2'
    {0x20, false},  // 0x33 '3'
    {0x21, false},  // 0x34 '4'
    {0x22, false},  // 0x35 '5'
    {0x23, false},  // 0x36 '6'
    {0x24, false},  // 0x37 '7'
    {0x25, false},  // 0x38 '8'
    {0x26, false},  // 0x39 '9'
    {0x34, false},  // 0x3A ':'  独立キー (US は Shift+;)
    {0x33, false},  // 0x3B ';'
    {0x36, true},   // 0x3C '<'
    {0x2D, true},   // 0x3D '='  Shift+- (US は独立キー)
    {0x37, true},   // 0x3E '>'
    {0x38, true},   // 0x3F '?'
    {0x2F, false},  // 0x40 '@'  独立キー (US は Shift+2)
};

// ASCII 0x5B〜0x60
constexpr KeyStroke kTable5BTo60[] = {
    {0x30, false},  // 0x5B '['  US より 1 つ右
    {0x87, false},  // 0x5C '\'  INTERNATIONAL1 (ろ)
    {0x31, false},  // 0x5D ']'  US より 1 つ右
    {0x2E, false},  // 0x5E '^'  独立キー
    {0x87, true},   // 0x5F '_'  Shift+INTERNATIONAL1
    {0x2F, true},   // 0x60 '`'  Shift+@
};

// ASCII 0x7B〜0x7E
constexpr KeyStroke kTable7BTo7E[] = {
    {0x30, true},  // 0x7B '{'
    {0x89, true},  // 0x7C '|'  Shift+INTERNATIONAL3
    {0x31, true},  // 0x7D '}'
    {0x2E, true},  // 0x7E '~'  Shift+^
};

constexpr uint8_t kKeycodeA = 0x04;  //!< HID Usage ID の 'a'

}  // namespace

KeyStroke asciiToJisKeyStroke(const char c)
{
    const uint8_t u = static_cast<uint8_t>(c);

    // 印字可能文字以外は打鍵しない
    if (u < 0x20 || u > 0x7E) {
        return KeyStroke{};
    }

    // 英字は US と同じ位置にあり、キーコードが連続している
    if (u >= 'a' && u <= 'z') {
        return KeyStroke{static_cast<uint8_t>(kKeycodeA + (u - 'a')), false};
    }
    if (u >= 'A' && u <= 'Z') {
        return KeyStroke{static_cast<uint8_t>(kKeycodeA + (u - 'A')), true};
    }

    if (u <= 0x40) {
        return kTable20To40[u - 0x20];
    }
    if (u >= 0x5B && u <= 0x60) {
        return kTable5BTo60[u - 0x5B];
    }
    if (u >= 0x7B) {
        return kTable7BTo7E[u - 0x7B];
    }

    return KeyStroke{};
}

}  // namespace nfccmd
