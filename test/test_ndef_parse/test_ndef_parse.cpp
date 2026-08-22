/*
 * ndef_parse のテスト (仕様書 F-03 / §10.1)
 *
 * TLV 構造:  T(1) L(1 or 3) V(L)
 *   0x00 = NULL TLV (1バイト), 0x03 = NDEF Message TLV, 0xFE = Terminator TLV
 *   L が 0xFF の場合は続く 2 バイトがビッグエンディアンの長さ
 *
 * NDEF Record 構造:
 *   byte0: MB(0x80) ME(0x40) CF(0x20) SR(0x10) IL(0x08) TNF(0x07)
 *   byte1: TYPE_LENGTH / 次: PAYLOAD_LENGTH (SR?1:4) / IL なら ID_LENGTH(1)
 *   続いて TYPE, ID, PAYLOAD
 */
#include <unity.h>

#include <string>
#include <vector>

#include "ndef_parse.h"

namespace {

using nfccmd::findNdefMessageTlv;
using nfccmd::findTextRecordPayload;

/// Short Record 形式の NDEF レコードを組み立てる
std::vector<uint8_t> makeShortRecord(const uint8_t tnf, const std::string& type, const std::string& payload,
                                     const bool mb = true, const bool me = true)
{
    std::vector<uint8_t> r;
    uint8_t flags = static_cast<uint8_t>(tnf & 0x07) | 0x10;  // SR
    if (mb) {
        flags |= 0x80;
    }
    if (me) {
        flags |= 0x40;
    }
    r.push_back(flags);
    r.push_back(static_cast<uint8_t>(type.size()));
    r.push_back(static_cast<uint8_t>(payload.size()));
    for (const char c : type) {
        r.push_back(static_cast<uint8_t>(c));
    }
    for (const char c : payload) {
        r.push_back(static_cast<uint8_t>(c));
    }
    return r;
}

/// NDEF Message TLV で包む
std::vector<uint8_t> wrapInTlv(const std::vector<uint8_t>& message)
{
    std::vector<uint8_t> t;
    t.push_back(0x03);
    t.push_back(static_cast<uint8_t>(message.size()));
    t.insert(t.end(), message.begin(), message.end());
    t.push_back(0xFE);
    return t;
}

std::string toString(const nfccmd::ByteSpan& s)
{
    return s.valid() ? std::string(reinterpret_cast<const char*>(s.data), s.size) : std::string{};
}

}  // namespace

void setUp(void)
{
}

void tearDown(void)
{
}

// --- TLV の解析 -------------------------------------------------------------

/// NDEF Message TLV の値部分を取り出す
void test_finds_ndef_message_tlv(void)
{
    const std::vector<uint8_t> tlv{0x03, 0x05, 'h', 'e', 'l', 'l', 'o', 0xFE};
    const auto span = findNdefMessageTlv(tlv.data(), tlv.size());

    TEST_ASSERT_TRUE(span.valid());
    TEST_ASSERT_EQUAL_UINT32(5, span.size);
    TEST_ASSERT_EQUAL_STRING("hello", toString(span).c_str());
}

/// NULL TLV (0x00) は読み飛ばす
void test_skips_null_tlv(void)
{
    const std::vector<uint8_t> tlv{0x00, 0x00, 0x03, 0x02, 'h', 'i', 0xFE};
    const auto span = findNdefMessageTlv(tlv.data(), tlv.size());

    TEST_ASSERT_TRUE(span.valid());
    TEST_ASSERT_EQUAL_STRING("hi", toString(span).c_str());
}

/// 未知のタグの TLV は長さ分だけ読み飛ばす
void test_skips_unknown_tlv(void)
{
    // タグ 0x01 (Lock Control) を 3 バイト分読み飛ばす
    const std::vector<uint8_t> tlv{0x01, 0x03, 0xAA, 0xBB, 0xCC, 0x03, 0x02, 'h', 'i', 0xFE};
    const auto span = findNdefMessageTlv(tlv.data(), tlv.size());

    TEST_ASSERT_TRUE(span.valid());
    TEST_ASSERT_EQUAL_STRING("hi", toString(span).c_str());
}

/// 長さ 0xFF の場合、続く 2 バイトをビッグエンディアンの長さとして扱う
void test_parses_three_byte_length(void)
{
    std::vector<uint8_t> tlv{0x03, 0xFF, 0x01, 0x00};  // 長さ 256
    for (int i = 0; i < 256; ++i) {
        tlv.push_back(static_cast<uint8_t>('A' + (i % 26)));
    }
    tlv.push_back(0xFE);

    const auto span = findNdefMessageTlv(tlv.data(), tlv.size());
    TEST_ASSERT_TRUE(span.valid());
    TEST_ASSERT_EQUAL_UINT32(256, span.size);
}

/// Terminator TLV しかなければ見つからない
void test_terminator_only_finds_nothing(void)
{
    const std::vector<uint8_t> tlv{0xFE};
    TEST_ASSERT_FALSE(findNdefMessageTlv(tlv.data(), tlv.size()).valid());
}

/// 未フォーマット領域 (全て 0x00) では見つからない
void test_all_zero_finds_nothing(void)
{
    const std::vector<uint8_t> tlv(48, 0x00);
    TEST_ASSERT_FALSE(findNdefMessageTlv(tlv.data(), tlv.size()).valid());
}

/// 異常系: 宣言された長さがバッファを超えている
void test_tlv_length_beyond_buffer_is_rejected(void)
{
    const std::vector<uint8_t> tlv{0x03, 0x20, 'h', 'i'};  // 32 バイトと宣言するが 2 バイトしかない
    TEST_ASSERT_FALSE(findNdefMessageTlv(tlv.data(), tlv.size()).valid());
}

/// 異常系: 空のバッファ
void test_empty_buffer_is_rejected(void)
{
    const uint8_t dummy = 0x03;
    TEST_ASSERT_FALSE(findNdefMessageTlv(&dummy, 0).valid());
    TEST_ASSERT_FALSE(findNdefMessageTlv(nullptr, 10).valid());
}

/// 長さ 0 の NDEF Message TLV は本文が無い
void test_zero_length_message(void)
{
    const std::vector<uint8_t> tlv{0x03, 0x00, 0xFE};
    const auto span = findNdefMessageTlv(tlv.data(), tlv.size());
    TEST_ASSERT_TRUE(span.valid());
    TEST_ASSERT_EQUAL_UINT32(0, span.size);
}

// --- NDEF Record の解析 -----------------------------------------------------

/// Text レコードのペイロードを取り出す
void test_finds_text_record(void)
{
    const auto rec  = makeShortRecord(0x01, "T", "\x02" "enhello");
    const auto span = findTextRecordPayload(rec.data(), rec.size());

    TEST_ASSERT_TRUE(span.valid());
    TEST_ASSERT_EQUAL_UINT32(8, span.size);
}

/// Text 以外のレコード (URI) は対象にしない
void test_ignores_uri_record(void)
{
    const auto rec = makeShortRecord(0x01, "U", "\x01" "example.com");
    TEST_ASSERT_FALSE(findTextRecordPayload(rec.data(), rec.size()).valid());
}

/// TNF が Well Known でないレコードは対象にしない
void test_ignores_non_wellknown_tnf(void)
{
    // TNF=0x02 (MIME) で type が "T"
    const auto rec = makeShortRecord(0x02, "T", "\x02" "enhello");
    TEST_ASSERT_FALSE(findTextRecordPayload(rec.data(), rec.size()).valid());
}

/// 複数レコードのうち、Text レコードだけを取り出す
void test_finds_text_record_among_multiple(void)
{
    auto msg       = makeShortRecord(0x01, "U", "\x01" "example.com", true, false);
    const auto rec = makeShortRecord(0x01, "T", "\x02" "enworld", false, true);
    msg.insert(msg.end(), rec.begin(), rec.end());

    const auto span = findTextRecordPayload(msg.data(), msg.size());
    TEST_ASSERT_TRUE(span.valid());
    TEST_ASSERT_EQUAL_UINT32(8, span.size);
    TEST_ASSERT_EQUAL_STRING("\x02" "enworld", toString(span).c_str());
}

/// ID フィールド付き (IL) のレコードでもペイロードを正しく取り出す
void test_handles_id_field(void)
{
    std::vector<uint8_t> rec;
    rec.push_back(0x80 | 0x40 | 0x10 | 0x08 | 0x01);  // MB|ME|SR|IL|TNF=1
    rec.push_back(0x01);                              // TYPE_LENGTH
    rec.push_back(0x03);                              // PAYLOAD_LENGTH
    rec.push_back(0x02);                              // ID_LENGTH
    rec.push_back('T');
    rec.push_back('i');
    rec.push_back('d');
    rec.push_back(0x00);
    rec.push_back('e');
    rec.push_back('n');

    const auto span = findTextRecordPayload(rec.data(), rec.size());
    TEST_ASSERT_TRUE(span.valid());
    TEST_ASSERT_EQUAL_UINT32(3, span.size);
}

/// SR が立っていないレコード (4 バイト長) も解釈できる
void test_handles_long_record(void)
{
    std::vector<uint8_t> rec;
    rec.push_back(0x80 | 0x40 | 0x01);  // MB|ME|TNF=1 (SR なし)
    rec.push_back(0x01);                // TYPE_LENGTH
    rec.push_back(0x00);                // PAYLOAD_LENGTH (BE 4バイト)
    rec.push_back(0x00);
    rec.push_back(0x00);
    rec.push_back(0x04);
    rec.push_back('T');
    rec.push_back(0x02);
    rec.push_back('e');
    rec.push_back('n');
    rec.push_back('X');

    const auto span = findTextRecordPayload(rec.data(), rec.size());
    TEST_ASSERT_TRUE(span.valid());
    TEST_ASSERT_EQUAL_UINT32(4, span.size);
}

/// 異常系: ペイロード長がバッファを超えるレコード
void test_record_payload_beyond_buffer_is_rejected(void)
{
    std::vector<uint8_t> rec{0x80 | 0x40 | 0x10 | 0x01, 0x01, 0x40, 'T', 'x'};  // 64 バイトと宣言
    TEST_ASSERT_FALSE(findTextRecordPayload(rec.data(), rec.size()).valid());
}

/// 異常系: 空のメッセージ
void test_empty_message_is_rejected(void)
{
    const uint8_t dummy = 0xD1;
    TEST_ASSERT_FALSE(findTextRecordPayload(&dummy, 0).valid());
    TEST_ASSERT_FALSE(findTextRecordPayload(nullptr, 10).valid());
}

// --- 結合 -------------------------------------------------------------------

/// TLV から Text レコードのペイロードまで辿れる
void test_tlv_to_text_record(void)
{
    const auto rec = makeShortRecord(0x01, "T", "\x02" "enhello");
    const auto tlv = wrapInTlv(rec);

    const auto msg = findNdefMessageTlv(tlv.data(), tlv.size());
    TEST_ASSERT_TRUE(msg.valid());

    const auto payload = findTextRecordPayload(msg.data, msg.size);
    TEST_ASSERT_TRUE(payload.valid());
    TEST_ASSERT_EQUAL_STRING("\x02" "enhello", toString(payload).c_str());
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_finds_ndef_message_tlv);
    RUN_TEST(test_skips_null_tlv);
    RUN_TEST(test_skips_unknown_tlv);
    RUN_TEST(test_parses_three_byte_length);
    RUN_TEST(test_terminator_only_finds_nothing);
    RUN_TEST(test_all_zero_finds_nothing);
    RUN_TEST(test_tlv_length_beyond_buffer_is_rejected);
    RUN_TEST(test_empty_buffer_is_rejected);
    RUN_TEST(test_zero_length_message);
    RUN_TEST(test_finds_text_record);
    RUN_TEST(test_ignores_uri_record);
    RUN_TEST(test_ignores_non_wellknown_tnf);
    RUN_TEST(test_finds_text_record_among_multiple);
    RUN_TEST(test_handles_id_field);
    RUN_TEST(test_handles_long_record);
    RUN_TEST(test_record_payload_beyond_buffer_is_rejected);
    RUN_TEST(test_empty_message_is_rejected);
    RUN_TEST(test_tlv_to_text_record);
    return UNITY_END();
}
