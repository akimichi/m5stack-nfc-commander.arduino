/*
 * settings — 永続化する設定値 (仕様書 F-10)
 *
 * 各モジュールの設定をひとまとめにして保持する。
 * NVS への読み書きは settings_store が担当し、こちらは
 * ハードウェアに依存しない純粋なデータとして扱う (§10.1)。
 *
 * 実装段階: S-8
 */
#pragma once

#include <cstdint>

#include "debounce.h"
#include "formatter.h"
#include "hid_output.h"
#include "key_sequence.h"

namespace nfccmd {

/// 設定値の許容範囲 (F-10)
struct SettingsLimits {
    static constexpr uint16_t kKeyDelayMax  = 50;
    static constexpr uint16_t kDebounceMax  = 5000;
    static constexpr uint8_t kAbsentMin     = 1;
    static constexpr uint8_t kAbsentMax     = 20;
    static constexpr uint16_t kPollMsMin    = 50;
    static constexpr uint16_t kPollMsMax    = 500;
};

/// 永続化する設定 (F-10)
struct Settings {
    // 出力書式 (F-02 / F-04)
    OutputMode out_mode{OutputMode::UidOnly};
    UidCase uid_case{UidCase::Upper};
    UidSeparator uid_separator{UidSeparator::None};
    FieldSeparator field_separator{FieldSeparator::Tab};
    TerminatorKey terminator{TerminatorKey::Enter};

    // HID 出力 (F-05)
    uint16_t key_delay_ms{8};
    KeyboardLayout layout{KeyboardLayout::US};
    NonAsciiPolicy non_ascii{NonAsciiPolicy::Drop};

    // キーシーケンス送出 (F-13)
    // 既定を OFF にするのは、SD カードを差し替えるだけで PC に任意のキー操作を
    // 送れるようになるためである (§8-7)
    KeySeqMode key_seq{KeySeqMode::Off};

    // 読み取り (F-01 / F-06)
    uint16_t debounce_ms{500};
    uint8_t absent_threshold{3};
    uint16_t poll_ms{100};

    /*!
      @brief 許容範囲を外れた項目だけを既定値に戻す
      @note NVS の破損や旧版の値が残っていても起動できるようにするためであり、
            設定全体を破棄することはしない (F-10)。
     */
    void clampToValidRange();

    ///@name 各モジュールの設定へ変換する
    ///@{
    FormatConfig toFormatConfig() const;
    HidOutput::Config toHidConfig() const;
    ReadDebouncer::Config toDebouncerConfig() const;
    ///@}
};

}  // namespace nfccmd
