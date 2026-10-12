# AdLibTrackerModoki 開発計画

最終更新: 2026-10-12

## 現在の状況

- フェーズ6d (パターン一覧とオーダー編集) が完了。
- 次の作業は 6e (楽器とマクロ)。
- `data/` フォルダ (本家で作った確認用ファイル) はコミットしていない。

## 確認の方法 (共通)

- 出音: 本家で録音した WAV と比べる。録音は曲ごとに本家を起動し直して行い、比較の前に冒頭の無音が 0 であることを確かめる。
- 編集機能: 6a で保存処理が本家とバイト単位で一致することを確かめたので、それを使う。
  1. 「この曲を開いて、このキーをこの順に押し、F2 で保存」という手順を用意する。
  2. 本家でその操作をして保存したファイルを `data/` に置く。本家もキー操作を送るスクリプトで自動で操作できる (6d-2 から)。
     文字の入力は SendKeys ではなく、両方とも同じキー操作 (PostMessage) で送る。
  3. クローン版で同じ操作を自動で再現して保存し、2 つのファイルを 1 バイト単位で比べる。
- 段階ごとに確認してからコミットする。

---

## 完了したフェーズ

### フェーズ1: OPL3 エミュレーションと DirectSound 出力 (0c3b042)
- IOplDevice: 音源デバイスの抽象インターフェース (実機ドライバへ差し替え可能)
- Opl3Emulator: Nuked OPL3 1.8 による OPL3 エミュレーション (49716Hz)
- DirectSoundOutput: 専用スレッドでリングバッファを補充するストリーミング再生

### フェーズ2: A2M ローダーと再生エンジン (3311802)
- A2M ローダー (形式バージョン 1..14) と展開処理 (SixPack / LZW / LZSS / aPack / LZH)
- 再生エンジン A2Player (adt2unit.pas の再生部分の移植)
- SampleClockDriver: サンプル数を数えてエミュレータを駆動する (SDL 版と同じ方式)
- 開く・再生・停止のメニュー、ドラッグ&ドロップ、コマンドライン引数での読み込み

### AT2 互換 OPL3 エミュレータ (87f7a51)
- 本家 SDL 版の opl3emu.pas (NukedOPL3 1.6 ベース) を移植し、既定のエミュレータにした
- 「音源」メニューで Nuked OPL3 1.8 と切り替えられる
- 本家で録音した WAV (mystcave / crackit) と全区間でビット単位の一致を確認

### フェーズ3: テキスト画面・Direct2D 描画・キーボード入力 (47bad0c)
- TxtScrIO: txtscrio.pas (SDL 版) の移植。色切替記号付きの文字列表示、影付き枠、カーソル、3 つの画面モード
- Font8x16: AT2 / VGA フォントと 16 色パレット
- Direct2DRenderer: D3D11 + DXGI スワップチェーン + Direct2D、整数倍の最近傍拡大表示
- Keyboard: adt2keyb.pas (SDL 版) の移植。Windows のスキャンコードを BIOS 互換のキーコードへ変換
- System: adt2sys.pas 相当 (描画・フレームレート・メッセージ処理)

### フェーズ4: プレイヤー画面とメニューバー (60375ca)
- 本家のメイン画面 (STATUS / PATTERN ORDER / パターン窓 / アナライザ / ステータス行) を移植
- トレース表示と、停止中のカーソル移動・再生操作のキー
- メニューバー (ファイル / 再生 / 表示 / 音源)。画面モードとエミュレータを切り替えられる

### フェーズ5a: A2T / A2P の読み込み (f055dc6)
- A2T ローダー (形式 1..14)、A2P ローダー (形式 1..11)
- 拡張子による読み込みの振り分け (ModuleLoader)、ローダーの共通部分 (LoaderCommon)

### フェーズ5b: HSC / RAD / S3M の読み込み (4481c68)
- 各ローダーを移植。S3M のコマンド補正と c2spd による音程補正を含む
- hubbard.hsc / spiral.rad / adlib.s3m / joe_x.s3m の出音が本家と完全一致

### フェーズ5c: FMK / DFM / MTK / CFF / AMD / XMS / SAT / SA2 の読み込み (1685923)
- 各形式のローダーを移植 (CFF の LZTYR 展開、MTK の RDC 展開と CRC16 を含む)
- 各形式の 12 曲で出音が本家と完全一致

### フェーズ6a: ダイアログ・保存・終了確認・NUKE (23c08eb)
- LZH 圧縮 (adt2pack.pas) を移植。本家が保存した A2M 13 曲・A2T と出力が一致
- A2M / A2T / A2P の保存 (F2 / Ctrl+F2 / Alt+F2 / Shift+F2) と進捗表示
- dialogio.pas の Dialog を移植し、終了確認 (ESC / F10) と NUKE'M (F4 / Ctrl+A) を実装
- ファイルメニューに新規・上書き保存・名前を付けて保存などを追加

### フェーズ6b: デバッグ情報ダイアログ (b87cbdb)
- adt2extn.pas の DEBUG_INFO を移植 (Ctrl+D / Ctrl+Alt、3 ページの表示、Space によるステップ再生)
- Alt+0 のトラック ON/OFF ダイアログ
- Ctrl+Enter で次のオーダーへ進む処理

### フェーズ6c-1: パターンエディタの入力と行単位の編集 (bd93bcd)
- MIDI ボードによるノート入力と試聴、桁ごとの直接入力 (correct_range による範囲補正)
- MIDI ボードを使わないノート入力 (InputStr を移植)
- Insert / Delete、BackSpace、Space、`[` `]`、Shift+Enter (固定ノート)
- Ctrl+PgUp / PgDn の移調と NOTE OVERFLOW ダイアログ、Ctrl+Z の取り消し
- Alt+M、Ctrl+← / →、F11 / F12 の切り替え、状態行の 4OP の組の表示
- 本家で保存した `data/test6c1.a2m` とバイト単位で一致することを確認

### フェーズ6c-2: 範囲選択とブロック操作 (d52a5a0)
- Shift+矢印などによる範囲選択と強調表示、状態行の M.BLOCK 表示、Alt+Q / Alt+B
- Ctrl+C / Ctrl+V / Alt+P、Alt+V / Shift+Alt+V (空き欄への貼り付け・上下反転)
- Ctrl+X (切り取り)、Ctrl+N (消去)、Ctrl+B / Ctrl+D (行の挿入・削除)、テンキー + / − (音量)
- 範囲の移調 (Ctrl+PgUp / PgDn) と固定ノートの切り替え (Shift+Enter)
- dialogio.pas の Menu を移植し、Alt+C のコピーメニューを実装。Ctrl+K (キーオフ入力) も追加
- 本家で保存した `data/test6c2.a2m` とバイト単位で一致することを確認

### フェーズ6c-3: ノート記録と、編集しながらの再生操作
- ノート記録 (Ctrl+Space): 記録先トラックの増減 (Ctrl+← / →)、記録位置の補正 (Shift+↑ / ↓)、状態行の NRECM 表示
- 記録中の Ctrl+K / BackSpace / 桁入力は記録先トラックすべてに入る。Alt+Q で前回の記録範囲に戻す
- 記録中にノートを押すとカーソルの行から再生し、トレース中に押した鍵盤を記録する
- Shift+F6 (デバッグモード)、Alt+F6 (パターンの繰り返し再生)、Ctrl+F8 / Ctrl+F9 (カーソルの行から再生)
- 本家で保存した `data/test6c3.a2m` とバイト単位で一致することを確認 (再生中の記録はタイミングに依存するため対象外)

### フェーズ6d-1: オーダー窓の編集と窓の切り替え
- メインループを本家と同じ形にし、Enter でパターン窓と PATTERN ORDER 窓を行き来する
- PATTERN_ORDER_edit を移植: カーソル移動、16 進数の直接入力、テンキー + / −、Insert / Delete / BackSpace、Space / Ctrl+Space
- Ctrl+C / Alt+C (オーダー 1 つ・オーダー表) / Ctrl+V / Alt+P、トラックの ON/OFF、F5〜F9 / Shift+F6 (カーソルのオーダーから再生)、ファイル操作、デバッグ情報
- 画面の内容が変わらなくてもカーソルが点滅するよう、点滅を切り替える時期にも描き直す (本家の見た目に合わせる)
- 本家で保存した `data/test6d1.a2m` とバイト単位で一致することを確認

### フェーズ6d-2: パターン一覧と Shift+Alt+P
- Ctrl+P (PATTERN_LIST / pattern_list__proc): パターン窓 (トレース中を含む) とオーダー窓から開く
  - マーク (Space / Ctrl+Space / Shift+Ctrl+Space)、コピー・貼り付け (Ctrl+C / Ctrl+V / Shift+Ctrl+V / Alt+V)、Ctrl+N、Ctrl+W
  - Shift+Insert / Shift+Delete、Enter による名前の編集、USED / MARKED 表示、内容が変わったら未保存にする
  - 一覧からの A2P 保存 (F2 / Ctrl+S) と読み込み (F3 / Ctrl+L / Shift+F3)。マークがあればそのすべてへ読み込む
- Shift+Alt+P (PATTERN_LIST_alt と paste_object_alt)
- 本家で保存した `data/test6d2.a2m` / `data/test6d2.a2p` とバイト単位で一致することを確認

### 実機演奏 (27d0c14, 2c1e7bf)
- IPlayerDriver: 再生駆動部の共通インターフェース (SampleClockDriver も実装)
- TimerDriver: 高精度ウェイタブルタイマーで timerHz() 周期に timerTick() と flush() を呼ぶ
- Opl3Hardware: 実機用の IOplDevice。c86ctl インターフェースで実機へ書き込む
- 音源メニューに「OPL3 実機」を追加。実機モードでは DirectSound を使わない

---

## 今後の計画

### フェーズ6e: 楽器とマクロ (次の作業)
- Ctrl+I / E / Q / G / M

### フェーズ6f: その他の編集機能
- Ctrl+T (移調)、Ctrl+R (楽器の置き換え)、Ctrl+O (オクターブ設定)
- Ctrl+H (置換)、Ctrl+F (曲の設定)、Alt+L (行マーク設定)
- 範囲を選んでいないときの Ctrl+B (メッセージボード) と Ctrl+X (トラックの並べ替え)
- F1 (ヘルプ)

まだ実装していないフェーズのキーは何もしない。

### 保留中の課題
- 「コマンド入力なし」モード (command_typing = 0) での楽器・エフェクトの入力。設定画面ができるまで保留。
