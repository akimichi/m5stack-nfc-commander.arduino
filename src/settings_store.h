/*
 * settings_store — 設定の永続化 (仕様書 F-10)
 *
 * NVS (Preferences) への保存と復元を担当する。
 * 設定値そのものの妥当性検査は Settings::clampToValidRange() に委ねる。
 *
 * 実装段階: S-8
 */
#pragma once

#include "settings.h"

namespace nfccmd {

class SettingsStore {
public:
    /*!
      @brief 保存された設定を読み出す
      @param[out] out 読み出した設定。保存が無ければ既定値が入る
      @return 保存値を読み出せたら true、既定値を用いたら false
      @note 範囲外の項目は既定値へ丸めたうえで返す
     */
    bool load(Settings& out);

    /// 設定を保存する
    bool save(const Settings& settings);

    /// 保存内容を消去する
    bool clear();
};

}  // namespace nfccmd
