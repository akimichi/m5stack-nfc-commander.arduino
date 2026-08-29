/*
 * settings_menu — 設定項目の定義と値の巡回 (仕様書 F-10)
 *
 * 設定画面は「項目を選ぶ」「値を次の候補へ進める」の 2 操作だけで
 * 全項目を編集できるようにする。項目のラベル・現在値の表示文字列・
 * 値の巡回はハードウェアに依存しないため、ここに純粋ロジックとして置く (§10.1)。
 * 描画は ui、操作の受け付けは main が担当する。
 *
 * 実装段階: S-8b
 */
#pragma once

#include <cstdint>
#include <string>

#include "settings.h"

namespace nfccmd {

/// 設定画面に並べる項目
enum class SettingItem : uint8_t {
    OutputMode,
    UidCase,
    UidSeparator,
    FieldSeparator,
    Terminator,
    KeyDelay,
    Layout,
    NonAscii,
    KeySeq,
    Debounce,
    AbsentThreshold,
    PollInterval,

    Count,  //!< 項目数。値ではない
};

/// 設定画面の項目数
constexpr uint8_t kSettingItemCount = static_cast<uint8_t>(SettingItem::Count);

/// 項目の表示名
const char* settingItemLabel(SettingItem item);

/// 項目の現在値を表す文字列
std::string settingItemValueText(const Settings& settings, SettingItem item);

/*!
  @brief 項目の値を次の候補へ進める
  @note 末尾まで進んだら先頭へ戻る。候補はいずれも F-10 の許容範囲に収まる。
 */
void advanceSettingItem(Settings& settings, SettingItem item);

/// 項目を次へ進める (末尾なら先頭へ戻る)
SettingItem nextSettingItem(SettingItem item);

}  // namespace nfccmd
