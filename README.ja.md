# AdLibTrackerModoki

[English](README.md)

OPL3 (YMF262) 用のトラッカー [Adlib Tracker II](http://www.adlibtracker.net/) (AT2) を、
Pascal から C++ へ移植した Windows 用のクローンです。

実用性を求めたものではないお遊びプロジェクトであり、
ほぼ100% AIエージェント(Claude code Opus5/5.5) によるコーディングです。

本家の画面と操作をなるべくそのまま再現しつつ、描画・音声出力を Windows のネイティブ API
(Direct2D / DirectSound) に置き換えています。

## 特徴

- 本家と同じテキスト画面・キー操作 (パターン・オーダー・楽器・マクロの編集、ヘルプなど)
- 曲ファイルの読み込み
  - AT2 形式: A2M / A2T / A2P
  - 他のトラッカー: HSC / RAD / S3M / FMK / DFM / MTK / CFF / AMD / XMS / SAT / SA2
- 曲ファイルの保存: A2M / A2T / A2P (本家とバイト単位で同じ内容)
- 楽器・楽器バンクの読み書き: A2I / A2F / A2B / A2W、他形式 (BNK / CIF / FIB / FIN / IBK / INS / SBI / SGI) の取り込み
- 選べる音源
  - AT2 互換エミュレータ (本家 SDL 版の opl3emu を移植。本家と同じ出音)
  - Nuked OPL3 1.8
  - ymfm (YMF262)
  - OPL3 実機 (c86ctl 経由)

### 本家との違い

- メニューバーを追加しています。
- ファイルを開く・保存するときは Windows のコモンダイアログを使います。
- 画面モード (90x40 / 120x50 / 180x60) と音源の切り替えはメニューで行い、設定は
  実行ファイルと同じフォルダの `AdLibTrackerModoki.ini` に保存されます。
- WAV の録音には対応していません。

## 動作環境

- Windows 10 / 11
- Direct3D 11 に対応したグラフィックス環境
- OPL3 実機で演奏する場合は c86ctl (`c86ctl.dll`) と対応するハードウェア

## ビルド

1. Visual Studio 2026 (C++ によるデスクトップ開発、プラットフォームツールセット v145) をインストールします。
2. `src/AdLibTrackerModoki/AdLibTrackerModoki.slnx` を開きます。
3. 構成 (Debug / Release)・プラットフォーム (x64 など) を選んでビルドします。

外部ライブラリはすべて `src/AdLibTrackerModoki/AdLibTrackerModoki/third_party/` に同梱しているため、
追加のインストールは必要ありません。

## フォルダ構成

```
src/AdLibTrackerModoki/        ソリューションと本体のソースコード
  AdLibTrackerModoki/
    core/                      曲データ・再生エンジン・OPL3 エミュレータ (OS に依存しない部分)
    platform/                  描画・音声出力・キー入力・実機ドライバ (Windows 依存の部分)
    ui/                        本家の画面・ダイアログの移植
    app/                       設定・音源の切り替え
    third_party/               同梱している外部ライブラリ
  tools/                       ソースコード生成用のスクリプト
at2-help.md / at2-help.ja.md   本家ヘルプの Markdown 版と日本語訳
original-bugs.md               移植中に見つけた本家の不具合と、クローン版での扱い
plan.md                        開発計画・進捗
```

## ライセンス

本ソフトウェアは、本家 Adlib Tracker II と同じく
[GNU General Public License バージョン 3](LICENSE) (またはそれ以降のバージョン) の下で配布します。

本ソフトウェアは Adlib Tracker II (subz3ro/Altair) のソースコードをもとにした派生物です。
画面フォント・ヘルプの文章なども本家に由来します。

### 同梱している外部ライブラリ

| ライブラリ | 著作者 | ライセンス |
|---|---|---|
| [Nuked OPL3](https://github.com/nukeykt/Nuked-OPL3) 1.8 | Nuke.YKT | LGPL 2.1 以降 ([LICENSE](src/AdLibTrackerModoki/AdLibTrackerModoki/third_party/nukedopl3/LICENSE)) |
| [ymfm](https://github.com/aaronsgiles/ymfm) | Aaron Giles | BSD 3-Clause ([LICENSE](src/AdLibTrackerModoki/AdLibTrackerModoki/third_party/ymfm/LICENSE)) |
| c86ctl (ヘッダのみ) | honet | BSD |

AT2 互換エミュレータは、本家 SDL 版の opl3emu.pas (Nuked OPL3 1.6 をもとにしたもの) の移植です。
