/*
 * ReadDebouncer のテスト (仕様書 F-06 / §10.1)
 *
 * 時刻を引数で注入することで、実時間を待たずに境界値を検証する。
 */
#include <unity.h>

#include "debounce.h"

namespace {

using nfccmd::CardInfo;
using nfccmd::ReadDebouncer;

/// UID バイト列からカード情報を作る
CardInfo makeCard(std::initializer_list<uint8_t> uid)
{
    CardInfo c;
    c.uid_size = static_cast<uint8_t>(uid.size());
    uint8_t i  = 0;
    for (auto b : uid) {
        c.uid[i++] = b;
    }
    c.type_name = "TEST";
    return c;
}

const CardInfo kCardA = makeCard({0xA3, 0x7A, 0x1B, 0xDD});
const CardInfo kCardB = makeCard({0x04, 0x11, 0x22, 0x33});

/// 離脱と判定されるまで onNoCard を繰り返す
void makeCardAbsent(ReadDebouncer& d, const uint8_t threshold = 3)
{
    for (uint8_t i = 0; i < threshold; ++i) {
        d.onNoCard();
    }
}

}  // namespace

void setUp(void)
{
}

void tearDown(void)
{
}

/// F-06-1: 初めて検出したカードは出力される
void test_first_detection_is_output(void)
{
    ReadDebouncer d;
    TEST_ASSERT_TRUE(d.onCardDetected(kCardA, 1000));
}

/// F-06-1: カードを置いたままだと、2 回目以降は出力されない
void test_card_kept_on_reader_is_output_once(void)
{
    ReadDebouncer d;
    TEST_ASSERT_TRUE(d.onCardDetected(kCardA, 1000));
    TEST_ASSERT_FALSE(d.onCardDetected(kCardA, 1100));
    TEST_ASSERT_FALSE(d.onCardDetected(kCardA, 5000));
    // debounce_ms (500ms) を大きく超えても、置いたままなら出力しない
    TEST_ASSERT_FALSE(d.onCardDetected(kCardA, 60000));
}

/// F-06-2: 離脱回数が閾値未満なら、抑止は解除されない
void test_absent_below_threshold_keeps_suppression(void)
{
    ReadDebouncer d;
    TEST_ASSERT_TRUE(d.onCardDetected(kCardA, 1000));

    d.onNoCard();  // 2 回だけ (閾値は 3)
    d.onNoCard();
    TEST_ASSERT_TRUE(d.cardPresent());
    TEST_ASSERT_FALSE(d.onCardDetected(kCardA, 10000));
}

/// F-06-3: 離脱を検知した後、同じカードを再びかざせば出力される
void test_reoutput_after_card_removed(void)
{
    ReadDebouncer d;
    TEST_ASSERT_TRUE(d.onCardDetected(kCardA, 1000));

    makeCardAbsent(d);
    TEST_ASSERT_FALSE(d.cardPresent());

    TEST_ASSERT_TRUE(d.onCardDetected(kCardA, 2000));
}

/// F-06-4: 離脱後でも、debounce_ms 以内の再検出はチャタリングとして抑止する
void test_chattering_within_debounce_is_suppressed(void)
{
    ReadDebouncer d;
    TEST_ASSERT_TRUE(d.onCardDetected(kCardA, 1000));

    makeCardAbsent(d);
    // 1000 + 499ms: debounce_ms (500) 未満
    TEST_ASSERT_FALSE(d.onCardDetected(kCardA, 1499));
}

/// F-06-4: 境界値。前回出力からちょうど debounce_ms 経過していれば出力する
void test_debounce_boundary_is_inclusive(void)
{
    ReadDebouncer d;
    TEST_ASSERT_TRUE(d.onCardDetected(kCardA, 1000));

    makeCardAbsent(d);
    TEST_ASSERT_TRUE(d.onCardDetected(kCardA, 1500));
}

/// F-06-4: チャタリングを抑止した後も、離脱すれば再び出力できる
void test_recovers_after_chattering(void)
{
    ReadDebouncer d;
    TEST_ASSERT_TRUE(d.onCardDetected(kCardA, 1000));

    makeCardAbsent(d);
    TEST_ASSERT_FALSE(d.onCardDetected(kCardA, 1200));  // チャタリングとして抑止

    makeCardAbsent(d);
    TEST_ASSERT_TRUE(d.onCardDetected(kCardA, 3000));
}

/// F-06-5: 異なる UID のカードは debounce_ms 以内でも即座に出力する
void test_different_uid_is_output_immediately(void)
{
    ReadDebouncer d;
    TEST_ASSERT_TRUE(d.onCardDetected(kCardA, 1000));
    // カード A が載ったままの認識でも、別のカードなら出力する
    TEST_ASSERT_TRUE(d.onCardDetected(kCardB, 1010));
    // 直後に A へ戻した場合も、直前の出力は B なので出力される
    TEST_ASSERT_TRUE(d.onCardDetected(kCardA, 1020));
}

/// 境界値: UID の長さが異なるカードは別のカードとして扱う
void test_uid_with_different_length_is_different_card(void)
{
    const CardInfo short_uid = makeCard({0x04, 0x11, 0x22, 0x33});
    const CardInfo long_uid  = makeCard({0x04, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66});

    ReadDebouncer d;
    TEST_ASSERT_TRUE(d.onCardDetected(short_uid, 1000));
    TEST_ASSERT_TRUE(d.onCardDetected(long_uid, 1010));
}

/// 設定した閾値が反映される
void test_configured_absent_threshold_is_used(void)
{
    ReadDebouncer d{ReadDebouncer::Config{500, 5}};
    TEST_ASSERT_TRUE(d.onCardDetected(kCardA, 1000));

    makeCardAbsent(d, 4);  // 閾値 5 に対して 4 回
    TEST_ASSERT_TRUE(d.cardPresent());

    d.onNoCard();  // 5 回目で離脱
    TEST_ASSERT_FALSE(d.cardPresent());
}

/// reset() で状態が初期化され、同じカードが再び出力される
void test_reset_clears_state(void)
{
    ReadDebouncer d;
    TEST_ASSERT_TRUE(d.onCardDetected(kCardA, 1000));
    TEST_ASSERT_FALSE(d.onCardDetected(kCardA, 1100));

    d.reset();
    TEST_ASSERT_FALSE(d.cardPresent());
    TEST_ASSERT_TRUE(d.onCardDetected(kCardA, 1200));
}

/// 異常系: millis() のラップアラウンドをまたいでも判定が破綻しない
void test_survives_millis_wraparound(void)
{
    ReadDebouncer d;
    const uint32_t near_max = 0xFFFFFF00;
    TEST_ASSERT_TRUE(d.onCardDetected(kCardA, near_max));

    makeCardAbsent(d);
    // ラップ後 0x00000100 は、経過時間 0x200 = 512ms > debounce_ms
    TEST_ASSERT_TRUE(d.onCardDetected(kCardA, 0x00000100));
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_first_detection_is_output);
    RUN_TEST(test_card_kept_on_reader_is_output_once);
    RUN_TEST(test_absent_below_threshold_keeps_suppression);
    RUN_TEST(test_reoutput_after_card_removed);
    RUN_TEST(test_chattering_within_debounce_is_suppressed);
    RUN_TEST(test_debounce_boundary_is_inclusive);
    RUN_TEST(test_recovers_after_chattering);
    RUN_TEST(test_different_uid_is_output_immediately);
    RUN_TEST(test_uid_with_different_length_is_different_card);
    RUN_TEST(test_configured_absent_threshold_is_used);
    RUN_TEST(test_reset_clears_state);
    RUN_TEST(test_survives_millis_wraparound);
    return UNITY_END();
}
