/*
 * ui — LCD 描画とブザーによるフィードバック
 *
 * 仕様書 §5.3 のモジュール分割における ui に相当する (F-08 / F-09)。
 *
 * 実装段階: S-8b (設定画面)
 */
#pragma once

#include <cstdint>
#include <string>

#include "settings_menu.h"

namespace nfccmd {

class Ui {
public:
    /// 画面を初期化し、枠を描画する
    void begin();

    /// ヘッダのユニット状態表示を更新する
    void setUnitReady(bool ready);

    /// ヘッダの HID 出力 ON/OFF 表示を更新する (F-07)
    void setOutputEnabled(bool enabled);

    /// ヘッダの出力モード表示を更新する (F-08)
    void setModeLabel(const std::string& label);

    /// 読み取ったカードをメイン領域に表示する
    /// @param ndef_text NDEF から取得したテキスト。無ければ空文字列
    void showCard(const std::string& uid_hex, const std::string& type_name, const std::string& ndef_text = {});

    /// メイン領域をカード待ち受け状態に戻す
    void showWaiting();

    /// 読み取り画面全体を描き直す
    void showMainScreen();

    ///@name 設定画面 (F-10)
    ///@{
    /// 設定画面を描画する
    void showSettings(const Settings& settings, SettingItem selected);

    /*!
      @brief 設定画面のボタン領域を判定する
      @return 0:NEXT / 1:CHANGE / 2:BACK。ボタン以外なら -1
     */
    int settingsButtonAt(int x, int y) const;
    ///@}

    /// フッタにメッセージを表示する (空文字で消去)
    void showMessage(const std::string& msg, bool is_error = false);

    /// フッタの通算読取件数を更新する
    void setCount(uint32_t count);

    ///@name ブザー (F-09)
    ///@{
    void beepDetect();      //!< 検出成功: 高音 短
    void beepSuppressed();  //!< 検出したが出力しなかった: 低音 短
    void beepWarn();        //!< 警告: 二連音
    void beepError();       //!< エラー: 低音 長
    ///@}

private:
    void drawHeader();
    void drawFooter();

    bool unit_ready_{false};
    bool output_enabled_{false};  //!< 起動直後は無効 (F-07)
    std::string mode_label_{"UID"};
    uint32_t count_{0};
    std::string message_{};
    bool message_is_error_{false};
};

}  // namespace nfccmd
