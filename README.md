# m5stack-nfc-commander

M5Stack CoreS3 と NFC Universal Unit (ST25R3916) を用いて、NFC カードの情報を
**USB HID キーボード入力**として PC に送出するデバイスである。

PC 側に専用アプリケーションは要らない。テキストエディタ、表計算ソフト、Web フォームなど、
キーボード入力を受け付ける場所にそのまま入力できる。バーコードリーダーの NFC 版にあたる。

## ハードウェア

| 項目 | 内容 |
|---|---|
| 本体 | M5Stack CoreS3 |
| リーダー | NFC Universal Unit (U216 / ST25R3916) |
| 接続 | Grove Port A (I2C / SDA=G2, SCL=G1 / アドレス 0x50) |

CoreS3 は PC に対して **USB Device** として振る舞う（USB Host 機能は使わない）。

## 対応カード

NFC-A (ISO/IEC 14443 Type A) のみを対象とする。

| 読み取る情報 | 対応するカード |
|---|---|
| UID | MIFARE Classic / Ultralight / NTAG / DESFire など NFC-A 全般 |
| NDEF Text | NTAG・Ultralight (NFC Forum Type 2)、ST25TA・DESFire (Type 4)、MIFARE Classic |

FeliCa (NFC-F)、ISO/IEC 14443 Type B、ISO/IEC 15693 は対象外である。

## ビルドと書き込み

PlatformIO CLI を使う。

```bash
# 開発ビルド (USB HID + USB CDC)
pio run -e debug -t upload

# 製品ビルド (USB HID のみ。PC に COM ポートが見えない)
pio run -e release -t upload

# ホスト側テスト (実機不要)
pio test -e native
```

**書き込みには手動でダウンロードモードに入れる必要がある。**
`ARDUINO_USB_MODE=0` の実機では esptool の自動リセットが効かないため、
そのまま `-t upload` すると `Failed to connect to ESP32-S3` で失敗する。

1. USB を接続したまま、電源ボタン (左側面) を押し続ける
2. **緑の LED が点灯した瞬間に離す** (およそ3秒。長すぎると電源が切れる)
3. `pio run -e debug -t upload` を実行する
4. 書き込み後は自動で復帰しないので、電源ボタンを短く押して起動する

**シリアルログは読めない。** ログは ESP-IDF のコンソール (UART0) に出る一方、
USB は TinyUSB が占有しているため、`pio device monitor` には何も流れてこない。
実機の状態は LCD 表示で確認する。

## 操作

### 読み取り画面

```
┌─────────────────────────────────┐
│ UID+NDEF      OUT ON    UNIT OK │  出力モード / 出力可否 / ユニット状態
├─────────────────────────────────┤
│         A37A1BDD                │  UID
│      MIFARE Classic 1K          │  カード種別
│           hello                 │  NDEF テキスト
├─────────────────────────────────┤
│ (メッセージ)          count:12  │
└─────────────────────────────────┘
```

| 操作 | 動作 |
|---|---|
| 画面を短く押す | HID 出力の有効 / 無効を切り替える |
| 画面を長押し | 設定画面を開く |

**起動直後は出力が無効**（`OUT OFF`）である。意図しない打鍵を防ぐため、
利用者が明示的に有効化するまで PC には何も送らない。

### 設定画面

画面下部を 3 分割したタッチ領域で操作する。

| ボタン | 動作 |
|---|---|
| NEXT | 次の項目へ移る |
| CHANGE | 選択中の項目の値を次の候補へ進める |
| BACK | 設定を保存して読み取り画面へ戻る |

設定は NVS に保存され、電源を切っても保持される。
設定画面を開いている間はカードの読み取りを停止する。

## 設定項目

| 項目 | 既定値 | 候補 |
|---|---|---|
| Output mode | UID | UID / NDEF / UID+NDEF / COMMAND |
| UID case | UPPER | UPPER / lower |
| UID separator | NONE | NONE / COLON / HYPHEN |
| Field separator | TAB | TAB / SPACE / COMMA |
| Terminator | ENTER | NONE / ENTER / TAB |
| Key delay | 8 ms | 0 / 4 / 8 / 16 / 32 / 50 ms |
| Layout | US | US / JIS |
| Non-ASCII | DROP | DROP / REPLACE |
| Key seq | OFF | OFF / ON |
| Debounce | 500 ms | 0 / 250 / 500 / 1000 / 2000 / 5000 ms |
| Absent count | 3 | 1 / 2 / 3 / 5 / 10 / 20 |
| Poll interval | 100 ms | 50 / 100 / 200 / 300 / 500 ms |

`Output mode` が `NDEF` のとき、NDEF を取得できないカードでは UID を出力する。

## カードごとの文字列送出

カード UID と送出する文字列の対応表を SD カードに置くと、かざしたカードに応じた
文字列を入力できる。出力モードを `COMMAND` にしたときだけ有効になる。

SD カード直下に **`/nfc_commands.csv`** を置く。

```csv
# 行頭が # の行と空行は読み飛ばす
04A2B3C4D5E680, Hello World
A3:7A:1B:DD,    user@example.com
04112233445566, a,b,c
```

- 1 列目は UID の 16 進表記。大文字小文字を問わず、`:` と `-` は無視する。
- 2 列目以降が送出する文字列。**最初のカンマより後ろすべて**が値になるため、
  文字列中にカンマを含められる。
- 前後の空白は取り除かれる。値の内部の空白は保たれる。
- 書式の誤った行は読み飛ばす。ファイル全体が無効になることはない。
- 同じ UID が複数回現れた場合は後の行が優先される。
- 登録できるのは 500 件まで。

**未登録のカードは UID を出力する。** SD カードやファイルが無い場合も同様で、
エラーにはならない。

> 対応表は平文で保存される。SD カードを抜けば内容を読み取れるため、
> パスワードなど秘匿性の高い文字列を登録する用途には適さない。

## キーシーケンス送出

設定の `Key seq` を `ON` にすると、対応表の値に書いた `{CTRL+C}` のような
トークンを、修飾キーを伴うキー操作として送出する。

```csv
04A2B3C4D5E680,{CTRL+A}{CTRL+C}
04A2B3C4D5E681,name{TAB}value{ENTER}
04A2B3C4D5E682,{ALT+F4}
04A2B3C4D5E683,literal brace: {{
```

- `{` と `}` で囲んだ部分が 1 つのキーストロークになる。それ以外はこれまでどおり
  1 文字ずつ打鍵される。
- 中身を `+` で分割し、**末尾が主キー、それ以外が修飾キー**である。
- 修飾キーは `CTRL` / `SHIFT` / `ALT` / `GUI` の 4 種。
- 主キーには ASCII 印字可能文字 1 文字か、次の名前を書ける。

  | 分類 | 名前 |
  |---|---|
  | 編集 | `ENTER` `TAB` `ESC` `SPACE` `BS` `DEL` `INS` |
  | 移動 | `UP` `DOWN` `LEFT` `RIGHT` `HOME` `END` `PGUP` `PGDN` |
  | 機能 | `F1` 〜 `F12` |

- 大文字小文字は区別しない。`{ctrl+c}` は `{CTRL+C}` と同じである。
- **英字は小文字として扱い、Shift は付けない。** `{CTRL+C}` は Ctrl+C であって
  Ctrl+Shift+C ではない。Shift が要るなら `{CTRL+SHIFT+C}` と明示する。
- リテラルの `{` は `{{` と書く。`}` はそのまま書ける。
- 解析できないトークン（閉じない `{`、未知のキー名、`{CTRL}` のように主キーが
  無いもの、修飾キーの重複）は、その範囲をそのまま打鍵し、LCD に `bad key token`
  と表示して警告音を鳴らす。

**`Key seq` の既定は `OFF`** であり、OFF のときは `{CTRL+C}` を 7 文字の文字列と
してそのまま打鍵する。従来の対応表の挙動は変わらない。

解釈するのは**対応表の値だけ**である。UID や NDEF テキストから得た文字列は、
未登録カードのフォールバック UID も含めて解釈しない。NDEF は第三者が書き込める
ため、カードに `{CTRL+ALT+DEL}` と書くだけで任意のキー操作を送れてしまうことを
防ぐためである。

> `Key seq` を `ON` にすると、SD カードを差し替えるだけで PC に任意のキー操作を
> 送れるようになる。必要なときだけ有効にすること。

## 制限事項

- **日本語などの非 ASCII 文字は出力できない。** HID キーボードは原理的に
  打鍵できないため、除去するか `?` に置き換える（`Non-ASCII` で選択）。
- **PC 側のキーボードレイアウトに合わせて `Layout` を設定する必要がある。**
  ただし UID (`0-9` `A-F`) と Enter / Tab は US と JIS で同じ位置にあるため、
  UID のみを出力する場合は影響を受けない。
- 鍵が既定値から変更された MIFARE Classic の NDEF は読み出せない。
  鍵の探索や総当たりは行わない。
- NDEF テキストの読み出しは 256 バイトまでとし、超過分は切り捨てる。
- カードを 2 枚以上同時にかざした場合は、どのカードも読み取らない。

## ドキュメント

詳細な仕様、設計判断の理由、テスト方針は [docs/specification.md](docs/specification.md) を参照。

## ライセンス

MIT License
