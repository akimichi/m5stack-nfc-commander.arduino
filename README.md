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
# 開発ビルド (USB HID + USB CDC。シリアルログを見られる)
pio run -e debug -t upload
pio device monitor

# 製品ビルド (USB HID のみ。PC に COM ポートが見えない)
pio run -e release -t upload

# ホスト側テスト (実機不要)
pio test -e native
```

書き込みに失敗する場合は、電源ボタンを約6秒長押しして電源を切り、
リセットボタンを押しながら電源を入れてダウンロードモードで起動する。

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
0411223344,     a,b,c
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
