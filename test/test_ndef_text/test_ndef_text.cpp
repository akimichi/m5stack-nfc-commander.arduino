/*
 * parseTextRecordPayload のテスト (仕様書 F-03 / §10.1)
 *
 * NDEF Text レコードのペイロード構造:
 *   [status][言語コード][本文]
 *   status: bit7 = 符号化 (0:UTF-8 / 1:UTF-16), bit5-0 = 言語コード長
 */
#include <unity.h>

#include <vector>

#include "ndef_text.h"

namespace {

using nfccmd::parseTextRecordPayload;

/// Text レコードのペイロードを組み立てる
std::vector<uint8_t> makePayload(const std::string& lang, const std::string& body, const bool utf16 = false)
{
    std::vector<uint8_t> p;
    uint8_t status = static_cast<uint8_t>(lang.size() & 0x3F);
    if (utf16) {
        status |= 0x80;
    }
    p.push_back(status);
    for (const char c : lang) {
        p.push_back(static_cast<uint8_t>(c));
    }
    for (const char c : body) {
        p.push_back(static_cast<uint8_t>(c));
    }
    return p;
}

}  // namespace

void setUp(void)
{
}

void tearDown(void)
{
}

/// 言語コード "en" を読み飛ばして本文を取り出す
void test_parses_utf8_text_with_en(void)
{
    const auto p = makePayload("en", "hello");
    const auto r = parseTextRecordPayload(p.data(), p.size());

    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_FALSE(r.truncated);
    TEST_ASSERT_EQUAL_STRING("hello", r.text.c_str());
}

/// 言語コードが "ja" でも本文の取り出し方は変わらない
void test_parses_text_with_ja(void)
{
    const auto p = makePayload("ja", "ID12345");
    const auto r = parseTextRecordPayload(p.data(), p.size());

    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_EQUAL_STRING("ID12345", r.text.c_str());
}

/// 境界値: 言語コード長が 0 でも本文を取り出せる
void test_parses_text_without_language_code(void)
{
    const auto p = makePayload("", "hello");
    const auto r = parseTextRecordPayload(p.data(), p.size());

    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_EQUAL_STRING("hello", r.text.c_str());
}

/// 5 文字の言語コード (例: "en-US") も正しく読み飛ばす
void test_parses_text_with_long_language_code(void)
{
    const auto p = makePayload("en-US", "hello");
    const auto r = parseTextRecordPayload(p.data(), p.size());

    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_EQUAL_STRING("hello", r.text.c_str());
}

/// 本文が空でも、レコードとしては解釈できる
void test_empty_body_is_valid_but_empty(void)
{
    const auto p = makePayload("en", "");
    const auto r = parseTextRecordPayload(p.data(), p.size());

    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_EQUAL_STRING("", r.text.c_str());
}

/// UTF-16 は打鍵できないため「NDEFなし」として扱う
void test_utf16_is_rejected(void)
{
    const auto p = makePayload("en", "hello", /*utf16=*/true);
    const auto r = parseTextRecordPayload(p.data(), p.size());

    TEST_ASSERT_FALSE(r.valid);
    TEST_ASSERT_EQUAL_STRING("", r.text.c_str());
}

/// 異常系: ペイロードが nullptr
void test_null_payload_is_rejected(void)
{
    const auto r = parseTextRecordPayload(nullptr, 10);
    TEST_ASSERT_FALSE(r.valid);
}

/// 異常系: ペイロード長が 0
void test_zero_length_is_rejected(void)
{
    const uint8_t dummy = 0x02;
    const auto r        = parseTextRecordPayload(&dummy, 0);
    TEST_ASSERT_FALSE(r.valid);
}

/// 異常系: 言語コード長がペイロード長を超えている (壊れたレコード)
void test_language_code_longer_than_payload_is_rejected(void)
{
    // status は言語コード長 10 を示すが、実際には 3 バイトしかない
    const uint8_t p[] = {0x0A, 'e', 'n'};
    const auto r      = parseTextRecordPayload(p, sizeof(p));
    TEST_ASSERT_FALSE(r.valid);
}

/// 境界値: 言語コードだけでちょうどペイロードが尽きる場合は本文が空
void test_payload_ends_exactly_after_language_code(void)
{
    const uint8_t p[] = {0x02, 'e', 'n'};
    const auto r      = parseTextRecordPayload(p, sizeof(p));
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_EQUAL_STRING("", r.text.c_str());
}

/// 上限を超えた本文は切り捨てられ、truncated が立つ
void test_body_is_truncated_at_limit(void)
{
    const auto p = makePayload("en", "0123456789");
    const auto r = parseTextRecordPayload(p.data(), p.size(), 5);

    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_TRUE(r.truncated);
    TEST_ASSERT_EQUAL_STRING("01234", r.text.c_str());
}

/// 境界値: 本文が上限ちょうどなら切り捨てない
void test_body_at_exact_limit_is_not_truncated(void)
{
    const auto p = makePayload("en", "01234");
    const auto r = parseTextRecordPayload(p.data(), p.size(), 5);

    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_FALSE(r.truncated);
    TEST_ASSERT_EQUAL_STRING("01234", r.text.c_str());
}

/// 本文に非 ASCII が含まれていてもここでは除去しない (サニタイズは hid_sanitize の責務)
void test_non_ascii_body_is_passed_through(void)
{
    const auto p = makePayload("ja", "あ");
    const auto r = parseTextRecordPayload(p.data(), p.size());

    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_EQUAL_UINT32(3, r.text.size());
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_parses_utf8_text_with_en);
    RUN_TEST(test_parses_text_with_ja);
    RUN_TEST(test_parses_text_without_language_code);
    RUN_TEST(test_parses_text_with_long_language_code);
    RUN_TEST(test_empty_body_is_valid_but_empty);
    RUN_TEST(test_utf16_is_rejected);
    RUN_TEST(test_null_payload_is_rejected);
    RUN_TEST(test_zero_length_is_rejected);
    RUN_TEST(test_language_code_longer_than_payload_is_rejected);
    RUN_TEST(test_payload_ends_exactly_after_language_code);
    RUN_TEST(test_body_is_truncated_at_limit);
    RUN_TEST(test_body_at_exact_limit_is_not_truncated);
    RUN_TEST(test_non_ascii_body_is_passed_through);
    return UNITY_END();
}
