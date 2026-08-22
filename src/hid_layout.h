/*
 * hid_layout — ASCII からキーボードレイアウト固有のキーストロークへの変換 (仕様書 F-05)
 *
 * USBHIDKeyboard::write() は US 配列前提の ASCII 変換表を持つため、
 * JIS 配列の PC に対しては記号がずれて入力される。JIS では自前の変換表を用い、
 * pressRaw() に生の HID キーコードを渡して打鍵する。
 *
 * ハードウェアに依存しない純粋ロジックとして実装する (§10.1)。
 *
 * 実装段階: S-7
 */
#pragma once

#include <cstdint>

namespace nfccmd {

/// PC 側のキーボードレイアウト (F-05)
enum class KeyboardLayout : uint8_t {
    US,   //!< US 配列 (既定)
    JIS,  //!< JIS 配列 (106/109 キー)
};

/// HID キーコードと Shift の要否
/// @note 実機側のコンパイラは C++14 相当で、既定メンバ初期化子を持つ構造体を
///       集成体として扱わない。対応表を constexpr 配列で書けるよう
///       constexpr コンストラクタを明示する。
struct KeyStroke {
    uint8_t keycode{0};  //!< HID Usage ID。0 は打鍵不能を表す
    bool shift{false};

    constexpr KeyStroke() = default;
    constexpr KeyStroke(const uint8_t k, const bool s) : keycode{k}, shift{s}
    {
    }

    constexpr bool valid() const
    {
        return keycode != 0;
    }
};

/*!
  @brief ASCII 印字可能文字を JIS 配列のキーストロークへ変換する
  @param c 変換する文字
  @return 打鍵情報。範囲外の文字なら valid()==false
 */
KeyStroke asciiToJisKeyStroke(char c);

}  // namespace nfccmd
