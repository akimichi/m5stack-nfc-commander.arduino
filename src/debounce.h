/*
 * debounce — 同一カードの多重読み取り抑止 (仕様書 F-06)
 *
 * ハードウェアに依存しない純粋ロジックとして実装し、
 * 現在時刻を引数で注入することでホスト側テストを可能にする (§10.1)。
 *
 * 実装段階: S-4
 */
#pragma once

#include <cstdint>

#include "nfc_reader.h"

namespace nfccmd {

class ReadDebouncer {
public:
    struct Config {
        //! 同一UIDの連続出力を抑止する最小間隔 [ms]
        uint32_t debounce_ms{500};
        //! カード離脱と判定する、連続で検出できなかった回数
        uint8_t absent_threshold{3};
    };

    ReadDebouncer() = default;
    explicit ReadDebouncer(const Config& cfg) : cfg_{cfg}
    {
    }

    void config(const Config& cfg)
    {
        cfg_ = cfg;
    }

    const Config& config() const
    {
        return cfg_;
    }

    /*!
      @brief カードを検出したときに呼ぶ
      @param card 検出したカード
      @param now_ms 現在時刻 [ms]
      @return 出力すべきなら true、抑止すべきなら false
     */
    bool onCardDetected(const CardInfo& card, uint32_t now_ms);

    /*!
      @brief カードを検出しなかったときに呼ぶ
      @note 離脱の判定は連続回数のみで行うため、時刻を必要としない
     */
    void onNoCard();

    /// 内部状態を初期化する
    void reset();

    /// カードが電界内にあると認識しているか
    bool cardPresent() const
    {
        return card_present_;
    }

private:
    Config cfg_{};
    CardInfo last_card_{};
    bool has_last_{false};
    bool card_present_{false};
    uint8_t absent_count_{0};
    uint32_t last_output_ms_{0};
};

}  // namespace nfccmd
