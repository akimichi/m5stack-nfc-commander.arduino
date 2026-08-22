/*
 * sanitizeForHid のテスト (仕様書 F-05 / §10.1)
 */
#include <unity.h>

#include "hid_sanitize.h"

namespace {

using nfccmd::NonAsciiPolicy;
using nfccmd::sanitizeForHid;

}  // namespace

void setUp(void)
{
}

void tearDown(void)
{
}

/// ASCII 印字可能文字だけの文字列は変化しない
void test_printable_ascii_passes_through(void)
{
    const auto r = sanitizeForHid("A37A1BDD", NonAsciiPolicy::Drop);
    TEST_ASSERT_EQUAL_STRING("A37A1BDD", r.text.c_str());
    TEST_ASSERT_EQUAL_UINT16(0, r.dropped_bytes);
}

/// 記号を含む ASCII もそのまま通る
void test_ascii_symbols_pass_through(void)
{
    const auto r = sanitizeForHid("a-b_c:d@e/f 1", NonAsciiPolicy::Drop);
    TEST_ASSERT_EQUAL_STRING("a-b_c:d@e/f 1", r.text.c_str());
    TEST_ASSERT_EQUAL_UINT16(0, r.dropped_bytes);
}

/// 空文字列は空文字列のまま
void test_empty_string(void)
{
    const auto r = sanitizeForHid("", NonAsciiPolicy::Drop);
    TEST_ASSERT_EQUAL_STRING("", r.text.c_str());
    TEST_ASSERT_EQUAL_UINT16(0, r.dropped_bytes);
}

/// 境界値: 0x1F は打鍵不能、0x20 (空白) は打鍵可能
void test_boundary_lower(void)
{
    const std::string in{"\x1F\x20"};
    const auto r = sanitizeForHid(in, NonAsciiPolicy::Drop);
    TEST_ASSERT_EQUAL_STRING(" ", r.text.c_str());
    TEST_ASSERT_EQUAL_UINT16(1, r.dropped_bytes);
}

/// 境界値: 0x7E (~) は打鍵可能、0x7F (DEL) は打鍵不能
void test_boundary_upper(void)
{
    const std::string in{"\x7E\x7F"};
    const auto r = sanitizeForHid(in, NonAsciiPolicy::Drop);
    TEST_ASSERT_EQUAL_STRING("~", r.text.c_str());
    TEST_ASSERT_EQUAL_UINT16(1, r.dropped_bytes);
}

/// 制御文字は打鍵対象にしない (終端キーは別途 HID で送るため)
void test_control_characters_are_dropped(void)
{
    const auto r = sanitizeForHid("AB\r\n\tCD", NonAsciiPolicy::Drop);
    TEST_ASSERT_EQUAL_STRING("ABCD", r.text.c_str());
    TEST_ASSERT_EQUAL_UINT16(3, r.dropped_bytes);
}

/// Drop: 日本語 (UTF-8) は取り除かれ、バイト数が数えられる
void test_japanese_is_dropped(void)
{
    // "あ" は UTF-8 で 3 バイト
    const auto r = sanitizeForHid("あ", NonAsciiPolicy::Drop);
    TEST_ASSERT_EQUAL_STRING("", r.text.c_str());
    TEST_ASSERT_EQUAL_UINT16(3, r.dropped_bytes);
}

/// Drop: ASCII と日本語が混在しても ASCII だけが残る
void test_mixed_text_keeps_ascii(void)
{
    const auto r = sanitizeForHid("ID:あ12", NonAsciiPolicy::Drop);
    TEST_ASSERT_EQUAL_STRING("ID:12", r.text.c_str());
    TEST_ASSERT_EQUAL_UINT16(3, r.dropped_bytes);
}

/// Replace: UTF-8 の 1 文字は 1 個の '?' になる
void test_replace_collapses_one_multibyte_char(void)
{
    const auto r = sanitizeForHid("あ", NonAsciiPolicy::Replace);
    TEST_ASSERT_EQUAL_STRING("?", r.text.c_str());
    TEST_ASSERT_EQUAL_UINT16(3, r.dropped_bytes);
}

/// Replace: 連続する打鍵不能バイト列はまとめて 1 個の '?' になる
void test_replace_collapses_consecutive_invalid_run(void)
{
    const auto r = sanitizeForHid("あいう", NonAsciiPolicy::Replace);
    TEST_ASSERT_EQUAL_STRING("?", r.text.c_str());
    TEST_ASSERT_EQUAL_UINT16(9, r.dropped_bytes);
}

/// Replace: ASCII で区切られた打鍵不能バイト列はそれぞれ '?' になる
void test_replace_separated_runs(void)
{
    const auto r = sanitizeForHid("あAい", NonAsciiPolicy::Replace);
    TEST_ASSERT_EQUAL_STRING("?A?", r.text.c_str());
    TEST_ASSERT_EQUAL_UINT16(6, r.dropped_bytes);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_printable_ascii_passes_through);
    RUN_TEST(test_ascii_symbols_pass_through);
    RUN_TEST(test_empty_string);
    RUN_TEST(test_boundary_lower);
    RUN_TEST(test_boundary_upper);
    RUN_TEST(test_control_characters_are_dropped);
    RUN_TEST(test_japanese_is_dropped);
    RUN_TEST(test_mixed_text_keeps_ascii);
    RUN_TEST(test_replace_collapses_one_multibyte_char);
    RUN_TEST(test_replace_collapses_consecutive_invalid_run);
    RUN_TEST(test_replace_separated_runs);
    return UNITY_END();
}
