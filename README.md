# Kana–Kanji Conversion (C++)

## 日本語

このリポジトリは、Mozc の辞書データ（`dictionary00.txt`〜`dictionary09.txt` および `connection_single_column.txt`）を取得し、**かな（ひらがな）入力から「かな漢字交じり」候補を生成するための C++ 実装**を提供します。

内部では UTF-16 ベースの LOUDS（Level-Order Unary Degree Sequence）を用いてトライ木を圧縮し、辞書ルックアップと候補生成を高速化します。

### できること

- Mozc 辞書のダウンロード（`mozc_dic_fetch`）
- 変換用アーティファクト生成  
  - 読み（yomi）→ termId の LOUDS（`yomi_termid.louds`）
  - 表記（tango）の LOUDS（`tango.louds`）
  - termId → トークン列（`token_array.bin`）
  - POS テーブル（`pos_table.bin`）
  - 接続コスト（`connection_single_column.bin`）
- かな→候補のデバッグ出力（`prefix_predict_cli`）
- LOUDS への common prefix search / termId 取得のデバッグ（`cps_cli`, `termid_cli`）

### 生成される成果物（Release で配布したい対象）

以下は「変換に必須」な最低限の成果物です。

- `yomi_termid.louds`
- `tango.louds`
- `token_array.bin`
- `pos_table.bin`
- `connection_single_column.bin`

---

## ローカルビルド手順（Ubuntu / WSL 推奨）

### 1) 依存パッケージ

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libcurl4-openssl-dev zlib1g-dev
```

### 2) ビルド

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

生成される主なバイナリ：

- `build/mozc_dic_fetch`
- `build/dictionary_builder`
- `build/tries_token_builder`
- `build/prefix_predict_cli`
- `build/cps_cli`
- `build/termid_cli`

---

## 辞書アーティファクト生成（推奨フロー）

### 1) Mozc 辞書データの取得

```bash
./build/mozc_dic_fetch
```

デフォルトでは `src/dictionary_builder/mozc_fetch/` に以下が保存されます：

- `dictionary00.txt`〜`dictionary09.txt`
- `connection_single_column.txt`

### 2) 接続コスト（connection）をバイナリ化

```bash
./build/dictionary_builder   --in src/dictionary_builder/mozc_fetch   --out build/mozc_reading.louds   --conn-out build/connection_single_column.bin
```

メモ：
- `--out`（読み LOUDS）は、将来の拡張や検証用途です（変換本体の必須要件は上記 5 ファイルです）。
- `connection_single_column.txt` の 1 行目をスキップする実装になっている場合があります（Mozc の形式に合わせた処理）。

### 3) 変換用アーティファクト生成（yomi/tango/token/POS）

```bash
./build/tries_token_builder   --in_dir src/dictionary_builder/mozc_fetch   --out_dir build
```

`build/` に以下が生成されます：

- `yomi_termid.louds`
- `tango.louds`
- `token_array.bin`
- `pos_table.bin`

---

## かな→候補（デバッグ出力）

### 1) 単発クエリ

```bash
./build/prefix_predict_cli   --yomi_termid build/yomi_termid.louds   --tango build/tango.louds   --tokens build/token_array.bin   --q "きょうはいいてんきです"   --limit 10
```

### 2) 標準入力（パイプ）

```bash
echo "きょうはいいてんきです" | ./build/prefix_predict_cli   --yomi_termid build/yomi_termid.louds   --tango build/tango.louds   --tokens build/token_array.bin   --stdin   --limit 10
```

メモ：
- `prefix_predict_cli` は現状「挙動確認・デバッグ向け」に詳細ログを出力します。
- `--no_dedup` を付けると、同一文字列候補の重複排除を無効化できます。

---

## LOUDS ツール（デバッグ）

### Common Prefix Search（UTF-8 入力 → 一致したキー列を表示）

```bash
./build/cps_cli --louds build/yomi_termid.louds --q "きょうはいいてんきです"
```

### TermId 取得（UTF-8 入力 → nodeIndex / termId などを表示）

```bash
./build/termid_cli --louds build/yomi_termid.louds --q "きょう"
```

---

## ライセンス

- **プログラム本体**：MIT License（`LICENSE`）
- **辞書データ**：Mozc のライセンス（`LICENSE-MOZC`）  
  - 本リポジトリは Mozc の辞書データそのものを同梱せず、`mozc_dic_fetch` により取得します。

---

## English

This repository provides a C++ pipeline for **Kana (Hiragana) → Kana–Kanji mixed conversion** using dictionary data from Mozc (`dictionary00.txt`–`dictionary09.txt` and `connection_single_column.txt`).

Internally, it uses UTF-16 based LOUDS (Level-Order Unary Degree Sequence) to compress tries and accelerate lookups and candidate generation.

### What you get

- Download Mozc dictionary data (`mozc_dic_fetch`)
- Build conversion artifacts:
  - Yomi (reading) → termId LOUDS (`yomi_termid.louds`)
  - Tango (surface form) LOUDS (`tango.louds`)
  - termId → token postings (`token_array.bin`)
  - POS table (`pos_table.bin`)
  - Connection costs (`connection_single_column.bin`)
- Debug conversion output (`prefix_predict_cli`)
- Debug LOUDS CPS / termId operations (`cps_cli`, `termid_cli`)

### Release artifacts (minimum required for conversion)

- `yomi_termid.louds`
- `tango.louds`
- `token_array.bin`
- `pos_table.bin`
- `connection_single_column.bin`

---

## Build (Ubuntu / WSL recommended)

### 1) Dependencies

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libcurl4-openssl-dev zlib1g-dev
```

### 2) Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Key binaries:

- `build/mozc_dic_fetch`
- `build/dictionary_builder`
- `build/tries_token_builder`
- `build/prefix_predict_cli`
- `build/cps_cli`
- `build/termid_cli`

---

## Generate dictionary artifacts (recommended flow)

### 1) Fetch Mozc dictionary files

```bash
./build/mozc_dic_fetch
```

By default, files are stored under `src/dictionary_builder/mozc_fetch/`:

- `dictionary00.txt`–`dictionary09.txt`
- `connection_single_column.txt`

### 2) Build connection cost binary

```bash
./build/dictionary_builder   --in src/dictionary_builder/mozc_fetch   --out build/mozc_reading.louds   --conn-out build/connection_single_column.bin
```

### 3) Build LOUDS tries + token array

```bash
./build/tries_token_builder   --in_dir src/dictionary_builder/mozc_fetch   --out_dir build
```

Outputs in `build/`:

- `yomi_termid.louds`
- `tango.louds`
- `token_array.bin`
- `pos_table.bin`

---

## Kana → candidates (debug)

### One-shot

```bash
./build/prefix_predict_cli   --yomi_termid build/yomi_termid.louds   --tango build/tango.louds   --tokens build/token_array.bin   --q "きょうはいいてんきです"   --limit 10
```

### stdin

```bash
echo "きょうはいいてんきです" | ./build/prefix_predict_cli   --yomi_termid build/yomi_termid.louds   --tango build/tango.louds   --tokens build/token_array.bin   --stdin   --limit 10
```

Notes:
- `prefix_predict_cli` is currently oriented toward debugging and prints verbose logs.
- Use `--no_dedup` to disable output de-duplication.

---

## License

- **Code**: MIT License (`LICENSE`)
- **Dictionary data**: Mozc license (`LICENSE-MOZC`)  
  - This repository does not bundle Mozc dictionary data; it is fetched via `mozc_dic_fetch`.
