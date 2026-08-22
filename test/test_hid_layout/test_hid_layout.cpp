/*
 * asciiToJisKeyStroke のテスト (仕様書 F-05 / §10.1)
 *
 * JIS 配列 (106/109) で各 ASCII 文字を出すための HID キーコードを検証する。
 * US 配列と位置が異なる記号が主眼である。
 */
#include <unity.h>

#include "hid_layout.h"

namespace {

using nfccmd::asciiToJisKeyStroke;

void assertKey(const char c, const uint8_t keycode, const bool shift)
{
    const auto ks = asciiToJisKeyStroke(c);
    TEST_ASSERT_TRUE_MESSAGE(ks.valid(), "keystroke should be valid");
    TEST_ASSERT_EQUAL_HEX8(keycode, ks.keycode);
    TEST_ASSERT_EQUAL(shift, ks.shift);
}

}  // namespace

void setUp(void)
{
}

void tearDown(void)
{
}

/// 数字は US と同じキー位置である
void test_digits(void)
{
    assertKey('1', 0x1E, false);
    assertKey('5', 0x22, false);
    assertKey('9', 0x26, false);
    assertKey('0', 0x27, false);  // 0 だけ並びの末尾
}

/// 英小文字
void test_lowercase_letters(void)
{
    assertKey('a', 0x04, false);
    assertKey('m', 0x10, false);
    assertKey('z', 0x1D, false);
}

/// 英大文字は Shift を伴う
void test_uppercase_letters(void)
{
    assertKey('A', 0x04, true);
    assertKey('F', 0x09, true);
    assertKey('Z', 0x1D, true);
}

/// 空白
void test_space(void)
{
    assertKey(' ', 0x2C, false);
}

/*!
  UID の出力に使う文字は、JIS でも US と同じキー位置である。
  仕様書 F-05 が「UID を出力する限りレイアウトの影響を受けない」と
  している根拠を検証する。
 */
void test_uid_characters_are_layout_independent(void)
{
    // 16 進数字 0-9
    const uint8_t digit_codes[] = {0x27, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26};
    for (int i = 0; i < 10; ++i) {
        assertKey(static_cast<char>('0' + i), digit_codes[i], false);
    }
    // 16 進の英字 A-F は 0x04〜0x09 + Shift
    for (int i = 0; i < 6; ++i) {
        assertKey(static_cast<char>('A' + i), static_cast<uint8_t>(0x04 + i), true);
    }
}

// --- US 配列と位置が異なる記号 ---------------------------------------------

/// '@' は US では Shift+2 だが、JIS では独立したキーにある
void test_at_sign(void)
{
    assertKey('@', 0x2F, false);
}

/// '"' は US では Shift+' だが、JIS では Shift+2 である
void test_double_quote(void)
{
    assertKey('"', 0x1F, true);
}

/// 数字段の Shift 記号が US とずれる
void test_shifted_digit_symbols(void)
{
    assertKey('!', 0x1E, true);   // Shift+1 (US と同じ)
    assertKey('#', 0x20, true);   // Shift+3 (US と同じ)
    assertKey('&', 0x23, true);   // Shift+6 (US は Shift+7)
    assertKey('\'', 0x24, true);  // Shift+7 (US は独立キー)
    assertKey('(', 0x25, true);   // Shift+8 (US は Shift+9)
    assertKey(')', 0x26, true);   // Shift+9 (US は Shift+0)
}

/// ':' と ';' は JIS では別々のキーにある
void test_colon_and_semicolon(void)
{
    assertKey(';', 0x33, false);
    assertKey(':', 0x34, false);  // US では Shift+;
}

/// '*' と '+' の位置
void test_asterisk_and_plus(void)
{
    assertKey('*', 0x34, true);  // Shift+:
    assertKey('+', 0x33, true);  // Shift+;
}

/// '=' は JIS では Shift+- である
void test_equal_sign(void)
{
    assertKey('-', 0x2D, false);
    assertKey('=', 0x2D, true);
}

/// '^' と '~' は JIS の独立キーにある
void test_caret_and_tilde(void)
{
    assertKey('^', 0x2E, false);
    assertKey('~', 0x2E, true);
}

/// 角括弧と波括弧は US より 1 つ右のキーにずれる
void test_brackets(void)
{
    assertKey('[', 0x30, false);
    assertKey(']', 0x31, false);
    assertKey('{', 0x30, true);
    assertKey('}', 0x31, true);
}

/// バックスラッシュとアンダースコアは INTERNATIONAL1 キーを使う
void test_backslash_and_underscore(void)
{
    assertKey('\\', 0x87, false);  // INTERNATIONAL1 (ろ)
    assertKey('_', 0x87, true);
}

/// 縦棒は INTERNATIONAL3 キーを使う
void test_vertical_bar(void)
{
    assertKey('|', 0x89, true);  // INTERNATIONAL3 (\ と |)
}

/// バッククォートは Shift+@ である
void test_backquote(void)
{
    assertKey('`', 0x2F, true);
}

/// 位置が US と変わらない記号も正しく引ける
void test_common_symbols(void)
{
    assertKey(',', 0x36, false);
    assertKey('.', 0x37, false);
    assertKey('/', 0x38, false);
    assertKey('<', 0x36, true);
    assertKey('>', 0x37, true);
    assertKey('?', 0x38, true);
    assertKey('$', 0x21, true);
    assertKey('%', 0x22, true);
}

// --- 異常系 -----------------------------------------------------------------

/// 制御文字は打鍵できない
void test_control_characters_are_invalid(void)
{
    TEST_ASSERT_FALSE(asciiToJisKeyStroke('\0').valid());
    TEST_ASSERT_FALSE(asciiToJisKeyStroke('\n').valid());
    TEST_ASSERT_FALSE(asciiToJisKeyStroke('\t').valid());
    TEST_ASSERT_FALSE(asciiToJisKeyStroke(static_cast<char>(0x1F)).valid());
}

/// 境界値: 0x7F (DEL) と非 ASCII は打鍵できない
void test_out_of_range_is_invalid(void)
{
    TEST_ASSERT_FALSE(asciiToJisKeyStroke(static_cast<char>(0x7F)).valid());
    TEST_ASSERT_FALSE(asciiToJisKeyStroke(static_cast<char>(0x80)).valid());
    TEST_ASSERT_FALSE(asciiToJisKeyStroke(static_cast<char>(0xE3)).valid());
}

/// 印字可能文字はすべて打鍵できる (0x20〜0x7E に穴が無い)
void test_all_printable_ascii_are_mapped(void)
{
    for (int c = 0x20; c <= 0x7E; ++c) {
        const auto ks = asciiToJisKeyStroke(static_cast<char>(c));
        TEST_ASSERT_TRUE_MESSAGE(ks.valid(), "every printable ASCII must be mapped");
    }
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_digits);
    RUN_TEST(test_lowercase_letters);
    RUN_TEST(test_uppercase_letters);
    RUN_TEST(test_space);
    RUN_TEST(test_uid_characters_are_layout_independent);
    RUN_TEST(test_at_sign);
    RUN_TEST(test_double_quote);
    RUN_TEST(test_shifted_digit_symbols);
    RUN_TEST(test_colon_and_semicolon);
    RUN_TEST(test_asterisk_and_plus);
    RUN_TEST(test_equal_sign);
    RUN_TEST(test_caret_and_tilde);
    RUN_TEST(test_brackets);
    RUN_TEST(test_backslash_and_underscore);
    RUN_TEST(test_vertical_bar);
    RUN_TEST(test_backquote);
    RUN_TEST(test_common_symbols);
    RUN_TEST(test_control_characters_are_invalid);
    RUN_TEST(test_out_of_range_is_invalid);
    RUN_TEST(test_all_printable_ascii_are_mapped);
    return UNITY_END();
}
