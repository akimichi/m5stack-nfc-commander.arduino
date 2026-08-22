/*
 * CommandMap のテスト (仕様書 F-12 / §10.1)
 */
#include <unity.h>

#include <string>

#include "command_map.h"

namespace {

using nfccmd::CardInfo;
using nfccmd::CommandMap;

CardInfo makeCard(std::initializer_list<uint8_t> uid)
{
    CardInfo c;
    c.uid_size = static_cast<uint8_t>(uid.size());
    uint8_t i  = 0;
    for (auto b : uid) {
        c.uid[i++] = b;
    }
    return c;
}

const CardInfo kCardA = makeCard({0x04, 0xA2, 0xB3, 0xC4, 0xD5, 0xE6, 0x80});
const CardInfo kCardB = makeCard({0xA3, 0x7A, 0x1B, 0xDD});

}  // namespace

void setUp(void)
{
}

void tearDown(void)
{
}

// --- 正常な行 ---------------------------------------------------------------

/// UID と文字列の組を登録し、検索できる
void test_add_and_find(void)
{
    CommandMap map;
    TEST_ASSERT_TRUE(map.addFromCsvLine("04A2B3C4D5E680,Hello World"));
    TEST_ASSERT_EQUAL_UINT32(1, map.size());

    const auto* found = map.find(kCardA);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_STRING("Hello World", found->c_str());
}

/// 4 バイト UID も扱える
void test_add_4byte_uid(void)
{
    CommandMap map;
    TEST_ASSERT_TRUE(map.addFromCsvLine("A37A1BDD,test"));

    const auto* found = map.find(kCardB);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_STRING("test", found->c_str());
}

/// UID の区切り文字 ':' は無視される
void test_uid_with_colon_separator(void)
{
    CommandMap map;
    TEST_ASSERT_TRUE(map.addFromCsvLine("A3:7A:1B:DD,test"));
    TEST_ASSERT_NOT_NULL(map.find(kCardB));
}

/// UID の区切り文字 '-' は無視される
void test_uid_with_hyphen_separator(void)
{
    CommandMap map;
    TEST_ASSERT_TRUE(map.addFromCsvLine("A3-7A-1B-DD,test"));
    TEST_ASSERT_NOT_NULL(map.find(kCardB));
}

/// UID の小文字表記も同じカードとして扱う
void test_uid_lowercase(void)
{
    CommandMap map;
    TEST_ASSERT_TRUE(map.addFromCsvLine("a37a1bdd,test"));
    TEST_ASSERT_NOT_NULL(map.find(kCardB));
}

/// 値にカンマを含められる (最初のカンマより後ろすべてが値)
void test_value_may_contain_comma(void)
{
    CommandMap map;
    TEST_ASSERT_TRUE(map.addFromCsvLine("A37A1BDD,a,b,c"));

    const auto* found = map.find(kCardB);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_STRING("a,b,c", found->c_str());
}

/// 前後の空白は取り除かれる
void test_trims_whitespace(void)
{
    CommandMap map;
    TEST_ASSERT_TRUE(map.addFromCsvLine("  A37A1BDD  ,   hello   "));

    const auto* found = map.find(kCardB);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_STRING("hello", found->c_str());
}

/// 値の内部の空白は保たれる
void test_keeps_inner_whitespace(void)
{
    CommandMap map;
    TEST_ASSERT_TRUE(map.addFromCsvLine("A37A1BDD,hello   world"));
    TEST_ASSERT_EQUAL_STRING("hello   world", map.find(kCardB)->c_str());
}

// --- 読み飛ばす行 -----------------------------------------------------------

/// コメント行は読み飛ばす
void test_skips_comment_line(void)
{
    CommandMap map;
    TEST_ASSERT_FALSE(map.addFromCsvLine("# A37A1BDD,test"));
    TEST_ASSERT_FALSE(map.addFromCsvLine("   # comment"));
    TEST_ASSERT_EQUAL_UINT32(0, map.size());
}

/// 空行と空白のみの行は読み飛ばす
void test_skips_empty_line(void)
{
    CommandMap map;
    TEST_ASSERT_FALSE(map.addFromCsvLine(""));
    TEST_ASSERT_FALSE(map.addFromCsvLine("    "));
    TEST_ASSERT_EQUAL_UINT32(0, map.size());
}

/// カンマが無い行は読み飛ばす
void test_skips_line_without_comma(void)
{
    CommandMap map;
    TEST_ASSERT_FALSE(map.addFromCsvLine("A37A1BDD"));
    TEST_ASSERT_EQUAL_UINT32(0, map.size());
}

/// 値が空の行は読み飛ばす
void test_skips_empty_value(void)
{
    CommandMap map;
    TEST_ASSERT_FALSE(map.addFromCsvLine("A37A1BDD,"));
    TEST_ASSERT_FALSE(map.addFromCsvLine("A37A1BDD,    "));
    TEST_ASSERT_EQUAL_UINT32(0, map.size());
}

/// 桁数が奇数の UID は読み飛ばす
void test_skips_odd_length_uid(void)
{
    CommandMap map;
    TEST_ASSERT_FALSE(map.addFromCsvLine("A37A1BD,test"));
    TEST_ASSERT_EQUAL_UINT32(0, map.size());
}

/// 16 進以外の文字を含む UID は読み飛ばす
void test_skips_non_hex_uid(void)
{
    CommandMap map;
    TEST_ASSERT_FALSE(map.addFromCsvLine("A37A1BZZ,test"));
    TEST_ASSERT_FALSE(map.addFromCsvLine("hello,test"));
    TEST_ASSERT_EQUAL_UINT32(0, map.size());
}

/// NFC-A の UID 長 (4/7/10 バイト) 以外は読み飛ばす
void test_skips_invalid_uid_length(void)
{
    CommandMap map;
    TEST_ASSERT_FALSE(map.addFromCsvLine("A37A1B,test"));                  // 3 バイト
    TEST_ASSERT_FALSE(map.addFromCsvLine("A37A1BDDEE,test"));              // 5 バイト
    TEST_ASSERT_FALSE(map.addFromCsvLine("0102030405060708090A0B,test"));  // 11 バイト
    TEST_ASSERT_EQUAL_UINT32(0, map.size());
}

/// 境界値: 10 バイト UID は登録できる
void test_accepts_10byte_uid(void)
{
    CommandMap map;
    TEST_ASSERT_TRUE(map.addFromCsvLine("0102030405060708090A,test"));

    const auto card = makeCard({0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A});
    TEST_ASSERT_NOT_NULL(map.find(card));
}

// --- 検索 -------------------------------------------------------------------

/// 未登録のカードは見つからない
void test_find_returns_null_for_unknown_card(void)
{
    CommandMap map;
    map.addFromCsvLine("A37A1BDD,test");
    TEST_ASSERT_NULL(map.find(kCardA));
}

/// 空の対応表では何も見つからない
void test_find_on_empty_map(void)
{
    const CommandMap map;
    TEST_ASSERT_NULL(map.find(kCardB));
}

/// 先頭が同じでも UID 長が違えば別のカードとして扱う
void test_find_distinguishes_uid_length(void)
{
    CommandMap map;
    map.addFromCsvLine("04A2B3C4,short");

    const auto long_uid = makeCard({0x04, 0xA2, 0xB3, 0xC4, 0xD5, 0xE6, 0x80});
    TEST_ASSERT_NULL(map.find(long_uid));

    const auto short_uid = makeCard({0x04, 0xA2, 0xB3, 0xC4});
    TEST_ASSERT_NOT_NULL(map.find(short_uid));
}

// --- 重複と上限 -------------------------------------------------------------

/// 同じ UID が現れたら後の行で置き換える
void test_duplicate_uid_is_overwritten(void)
{
    CommandMap map;
    TEST_ASSERT_TRUE(map.addFromCsvLine("A37A1BDD,first"));
    TEST_ASSERT_TRUE(map.addFromCsvLine("A37A1BDD,second"));

    TEST_ASSERT_EQUAL_UINT32(1, map.size());
    TEST_ASSERT_EQUAL_STRING("second", map.find(kCardB)->c_str());
}

/// 上限を超えた行は登録されない
void test_respects_entry_limit(void)
{
    CommandMap map;
    // 4 バイト UID を連番で作り、上限まで登録する
    for (size_t i = 0; i < nfccmd::kMaxCommandEntries; ++i) {
        char line[64];
        snprintf(line, sizeof(line), "%08X,value", static_cast<unsigned>(i));
        TEST_ASSERT_TRUE(map.addFromCsvLine(line));
    }
    TEST_ASSERT_EQUAL_UINT32(nfccmd::kMaxCommandEntries, map.size());

    TEST_ASSERT_FALSE(map.addFromCsvLine("FFFFFFFF,overflow"));
    TEST_ASSERT_EQUAL_UINT32(nfccmd::kMaxCommandEntries, map.size());
}

/// 上限に達していても、既存 UID の置き換えはできる
void test_overwrite_works_at_limit(void)
{
    CommandMap map;
    for (size_t i = 0; i < nfccmd::kMaxCommandEntries; ++i) {
        char line[64];
        snprintf(line, sizeof(line), "%08X,value", static_cast<unsigned>(i));
        map.addFromCsvLine(line);
    }
    TEST_ASSERT_TRUE(map.addFromCsvLine("00000000,updated"));

    const auto card = makeCard({0x00, 0x00, 0x00, 0x00});
    TEST_ASSERT_EQUAL_STRING("updated", map.find(card)->c_str());
}

/// clear() で空になる
void test_clear(void)
{
    CommandMap map;
    map.addFromCsvLine("A37A1BDD,test");
    map.clear();

    TEST_ASSERT_TRUE(map.empty());
    TEST_ASSERT_NULL(map.find(kCardB));
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_add_and_find);
    RUN_TEST(test_add_4byte_uid);
    RUN_TEST(test_uid_with_colon_separator);
    RUN_TEST(test_uid_with_hyphen_separator);
    RUN_TEST(test_uid_lowercase);
    RUN_TEST(test_value_may_contain_comma);
    RUN_TEST(test_trims_whitespace);
    RUN_TEST(test_keeps_inner_whitespace);
    RUN_TEST(test_skips_comment_line);
    RUN_TEST(test_skips_empty_line);
    RUN_TEST(test_skips_line_without_comma);
    RUN_TEST(test_skips_empty_value);
    RUN_TEST(test_skips_odd_length_uid);
    RUN_TEST(test_skips_non_hex_uid);
    RUN_TEST(test_skips_invalid_uid_length);
    RUN_TEST(test_accepts_10byte_uid);
    RUN_TEST(test_find_returns_null_for_unknown_card);
    RUN_TEST(test_find_on_empty_map);
    RUN_TEST(test_find_distinguishes_uid_length);
    RUN_TEST(test_duplicate_uid_is_overwritten);
    RUN_TEST(test_respects_entry_limit);
    RUN_TEST(test_overwrite_works_at_limit);
    RUN_TEST(test_clear);
    return UNITY_END();
}
