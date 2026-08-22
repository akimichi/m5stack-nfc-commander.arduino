/*
 * ui の実装
 *
 * 画面は 320x240 (横向き) を前提とする。
 */
#include "ui.h"

#include <M5Unified.h>

namespace nfccmd {

namespace {

auto& lcd = M5.Display;

// 画面レイアウト (F-08)
constexpr int kHeaderHeight = 28;
constexpr int kFooterHeight = 30;

// 設定画面のレイアウト (F-10)
constexpr int kSettingsButtonHeight = 40;  //!< 画面下部のボタン帯
constexpr int kSettingsRowHeight    = 28;  //!< 項目 1 行の高さ
constexpr int kSettingsButtonCount  = 3;

constexpr uint16_t kColorHeaderBg   = TFT_NAVY;
constexpr uint16_t kColorFooterBg   = 0x2104;  // 暗いグレー
constexpr uint16_t kColorMainBg     = TFT_BLACK;
constexpr uint16_t kColorText       = TFT_WHITE;
constexpr uint16_t kColorReady      = TFT_GREEN;
constexpr uint16_t kColorError      = TFT_RED;
constexpr uint16_t kColorUid        = TFT_YELLOW;
constexpr uint16_t kColorSubText    = TFT_CYAN;
constexpr uint16_t kColorWaitText   = 0x8410;  // 中間グレー
constexpr uint16_t kColorSelectedBg = 0x001F;  // 選択行の背景 (青)
constexpr uint16_t kColorButtonBg   = 0x2104;

int mainTop()
{
    return kHeaderHeight;
}

int mainHeight()
{
    return lcd.height() - kHeaderHeight - kFooterHeight;
}

void clearMain()
{
    lcd.fillRect(0, mainTop(), lcd.width(), mainHeight(), kColorMainBg);
}

/// 画面幅に収まらない文字列を末尾省略する
/// @note UTF-8 の途中で切れる場合があるが、表示のみの用途なので許容する
std::string fitToWidth(const std::string& s)
{
    const int max_w = lcd.width() - 8;
    if (lcd.textWidth(s.c_str()) <= max_w) {
        return s;
    }
    std::string t = s;
    while (!t.empty() && lcd.textWidth((t + "...").c_str()) > max_w) {
        t.pop_back();
    }
    return t + "...";
}

}  // namespace

void Ui::begin()
{
    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }
    showMainScreen();
}

void Ui::showMainScreen()
{
    lcd.fillScreen(kColorMainBg);
    drawHeader();
    drawFooter();
    showWaiting();
}

void Ui::drawHeader()
{
    lcd.fillRect(0, 0, lcd.width(), kHeaderHeight, kColorHeaderBg);

    const int center_y = kHeaderHeight / 2;
    lcd.setFont(&fonts::Font2);

    // 出力モードを左端に置く (F-08)
    lcd.setTextDatum(textdatum_t::middle_left);
    lcd.setTextColor(kColorSubText, kColorHeaderBg);
    lcd.drawString(mode_label_.c_str(), 6, center_y);

    // ユニット状態は右端に置く
    lcd.setTextDatum(textdatum_t::middle_right);
    lcd.setTextColor(unit_ready_ ? kColorReady : kColorError, kColorHeaderBg);
    lcd.drawString(unit_ready_ ? "UNIT OK" : "UNIT NG", lcd.width() - 6, center_y);

    // HID 出力の可否はその左に置く (F-07: ON=緑 / OFF=赤)
    lcd.setTextColor(output_enabled_ ? kColorReady : kColorError, kColorHeaderBg);
    lcd.drawString(output_enabled_ ? "OUT ON" : "OUT OFF", lcd.width() - 72, center_y);
}

void Ui::drawFooter()
{
    const int top = lcd.height() - kFooterHeight;
    lcd.fillRect(0, top, lcd.width(), kFooterHeight, kColorFooterBg);

    lcd.setFont(&fonts::Font2);
    lcd.setTextDatum(textdatum_t::middle_left);
    lcd.setTextColor(message_is_error_ ? kColorError : kColorText, kColorFooterBg);
    lcd.drawString(message_.c_str(), 6, top + kFooterHeight / 2);

    lcd.setTextDatum(textdatum_t::middle_right);
    lcd.setTextColor(kColorText, kColorFooterBg);
    lcd.drawString(("count:" + std::to_string(count_)).c_str(), lcd.width() - 6, top + kFooterHeight / 2);
}

void Ui::setUnitReady(const bool ready)
{
    if (unit_ready_ == ready) {
        return;
    }
    unit_ready_ = ready;
    drawHeader();
}

void Ui::setOutputEnabled(const bool enabled)
{
    if (output_enabled_ == enabled) {
        return;
    }
    output_enabled_ = enabled;
    drawHeader();
}

void Ui::setModeLabel(const std::string& label)
{
    if (mode_label_ == label) {
        return;
    }
    mode_label_ = label;
    drawHeader();
}

void Ui::showCard(const std::string& uid_hex, const std::string& type_name, const std::string& ndef_text)
{
    clearMain();

    const int center_y = mainTop() + mainHeight() / 2;
    const bool has_ndef = !ndef_text.empty();

    lcd.setTextDatum(textdatum_t::middle_center);

    // UID は最大 10 バイト = 20 文字。画面幅に収まらない場合はフォントを落とす
    lcd.setTextColor(kColorUid, kColorMainBg);
    lcd.setFont(&fonts::Font4);
    if (lcd.textWidth(uid_hex.c_str()) > lcd.width() - 8) {
        lcd.setFont(&fonts::Font2);
    }
    lcd.drawString(uid_hex.c_str(), lcd.width() / 2, center_y - (has_ndef ? 32 : 20));

    lcd.setFont(&fonts::Font2);
    lcd.setTextColor(kColorSubText, kColorMainBg);
    lcd.drawString(type_name.c_str(), lcd.width() / 2, center_y + (has_ndef ? 2 : 16));

    if (has_ndef) {
        lcd.setTextColor(kColorText, kColorMainBg);
        lcd.drawString(fitToWidth(ndef_text).c_str(), lcd.width() / 2, center_y + 28);
    }
}

void Ui::showWaiting()
{
    clearMain();

    lcd.setFont(&fonts::Font4);
    lcd.setTextDatum(textdatum_t::middle_center);
    lcd.setTextColor(kColorWaitText, kColorMainBg);
    lcd.drawString("Touch a card", lcd.width() / 2, mainTop() + mainHeight() / 2);
}

void Ui::showMessage(const std::string& msg, const bool is_error)
{
    if (message_ == msg && message_is_error_ == is_error) {
        return;
    }
    message_          = msg;
    message_is_error_ = is_error;
    drawFooter();
}

void Ui::setCount(const uint32_t count)
{
    if (count_ == count) {
        return;
    }
    count_ = count;
    drawFooter();
}

void Ui::showSettings(const Settings& settings, const SettingItem selected)
{
    const int list_top    = kHeaderHeight;
    const int list_height = lcd.height() - kHeaderHeight - kSettingsButtonHeight;
    const int visible     = list_height / kSettingsRowHeight;
    const int selected_index = static_cast<int>(selected);

    lcd.fillScreen(kColorMainBg);

    // ヘッダ: 画面名と「何番目/全体数」
    lcd.fillRect(0, 0, lcd.width(), kHeaderHeight, kColorHeaderBg);
    lcd.setFont(&fonts::Font2);
    lcd.setTextDatum(textdatum_t::middle_left);
    lcd.setTextColor(kColorText, kColorHeaderBg);
    lcd.drawString("SETTINGS", 6, kHeaderHeight / 2);

    lcd.setTextDatum(textdatum_t::middle_right);
    const auto position = std::to_string(selected_index + 1) + "/" + std::to_string(kSettingItemCount);
    lcd.drawString(position.c_str(), lcd.width() - 6, kHeaderHeight / 2);

    // 選択項目が見えるようにスクロールさせる。上から 3 行目に来るよう寄せる
    int first = selected_index - 2;
    if (first > kSettingItemCount - visible) {
        first = kSettingItemCount - visible;
    }
    if (first < 0) {
        first = 0;
    }

    lcd.setFont(&fonts::Font2);
    for (int row = 0; row < visible; ++row) {
        const int index = first + row;
        if (index >= kSettingItemCount) {
            break;
        }
        const auto item     = static_cast<SettingItem>(index);
        const bool is_selected = (index == selected_index);
        const int y            = list_top + row * kSettingsRowHeight;
        const uint16_t bg      = is_selected ? kColorSelectedBg : kColorMainBg;

        lcd.fillRect(0, y, lcd.width(), kSettingsRowHeight, bg);

        lcd.setTextDatum(textdatum_t::middle_left);
        lcd.setTextColor(kColorText, bg);
        lcd.drawString(settingItemLabel(item), 10, y + kSettingsRowHeight / 2);

        lcd.setTextDatum(textdatum_t::middle_right);
        lcd.setTextColor(is_selected ? TFT_YELLOW : kColorSubText, bg);
        lcd.drawString(settingItemValueText(settings, item).c_str(), lcd.width() - 10, y + kSettingsRowHeight / 2);
    }

    // 操作ボタン
    const int button_top   = lcd.height() - kSettingsButtonHeight;
    const int button_width = lcd.width() / kSettingsButtonCount;
    static constexpr const char* kLabels[kSettingsButtonCount] = {"NEXT", "CHANGE", "BACK"};

    lcd.fillRect(0, button_top, lcd.width(), kSettingsButtonHeight, kColorButtonBg);
    lcd.setTextDatum(textdatum_t::middle_center);
    lcd.setTextColor(kColorText, kColorButtonBg);
    for (int i = 0; i < kSettingsButtonCount; ++i) {
        if (i != 0) {
            lcd.drawFastVLine(i * button_width, button_top, kSettingsButtonHeight, kColorWaitText);
        }
        lcd.drawString(kLabels[i], i * button_width + button_width / 2, button_top + kSettingsButtonHeight / 2);
    }
}

int Ui::settingsButtonAt(const int x, const int y) const
{
    const int button_top = lcd.height() - kSettingsButtonHeight;
    if (y < button_top) {
        return -1;
    }
    const int button_width = lcd.width() / kSettingsButtonCount;
    const int index        = x / button_width;
    return (index >= 0 && index < kSettingsButtonCount) ? index : -1;
}

void Ui::beepDetect()
{
    M5.Speaker.tone(4000, 40);
}

void Ui::beepSuppressed()
{
    M5.Speaker.tone(1500, 40);
}

void Ui::beepWarn()
{
    // 二連音
    M5.Speaker.tone(2000, 40);
    M5.delay(60);
    M5.Speaker.tone(2000, 40);
}

void Ui::beepError()
{
    M5.Speaker.tone(1000, 200);
}

}  // namespace nfccmd
