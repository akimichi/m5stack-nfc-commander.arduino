/*
 * debounce の実装 (仕様書 F-06)
 */
#include "debounce.h"

namespace nfccmd {

bool ReadDebouncer::onCardDetected(const CardInfo& card, const uint32_t now_ms)
{
    absent_count_ = 0;

    const bool same_card = has_last_ && last_card_.sameUidAs(card);
    if (same_card) {
        // 同じカードが電界内に留まっている間は、最初の 1 回しか出力しない (F-06-1)
        if (card_present_) {
            return false;
        }
        // 離脱を検知した直後の再検出は、カード位置のふらつきとみなす (F-06-4)。
        // ここで last_output_ms_ は更新しない。更新すると、ふらつきが続く限り
        // 抑止期間が延び続けてしまうためである。
        if (now_ms - last_output_ms_ < cfg_.debounce_ms) {
            card_present_ = true;
            return false;
        }
    }

    // 異なるカードは debounce_ms を待たずに出力する (F-06-5)
    last_card_      = card;
    has_last_       = true;
    card_present_   = true;
    last_output_ms_ = now_ms;
    return true;
}

void ReadDebouncer::onNoCard()
{
    if (!card_present_) {
        return;
    }
    // 単発の検出漏れで離脱と誤判定しないよう、連続回数で判定する (F-06-2)
    if (++absent_count_ >= cfg_.absent_threshold) {
        card_present_ = false;
        absent_count_ = 0;
    }
}

void ReadDebouncer::reset()
{
    last_card_.clear();
    has_last_       = false;
    card_present_   = false;
    absent_count_   = 0;
    last_output_ms_ = 0;
}

}  // namespace nfccmd
