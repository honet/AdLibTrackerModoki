# AdLibTrackerModoki 開発計画

最終更新: 2026-10-14

## 現在の状況

- フェーズ6e-4b (A2W の読み込みとマクロブラウザ) が完了。
- 次の作業は 6e-5 (BNK / CIF / FIB / FIN / IBK / INS / SBI / SGI の取り込み)。
- `data/` フォルダ (本家で作った確認用ファイル) はコミットしていない。

## 確認の方法 (共通)

- 出音: 本家で録音した WAV と比べる。録音は曲ごとに本家を起動し直して行い、比較の前に冒頭の無音が 0 であることを確かめる。
- 編集機能: 6a で保存処理が本家とバイト単位で一致することを確かめたので、それを使う。
  1. 「この曲を開いて、このキーをこの順に押し、F2 で保存」という手順を用意する。
  2. 本家でその操作をして保存したファイルを `data/` に置く。本家もキー操作を送るスクリプトで自動で操作できる (6d-2 から)。
     本家はコマンドラインで曲を開けないので、F3 のファイル選択画面で開く (AT2 のフォルダで ".." から始まる)。
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

### フェーズ6e-1: 楽器窓 (Ctrl+I)
- INSTRUMENT_CONTROL / instr_control_proc を移植: パターン窓 (トレース中を含む) とオーダー窓から開く
  - 左側に選んだ楽器のパラメータ、右側に 255 個の楽器の一覧、鳴っている楽器の印 (instr_control_ai)
  - Space によるマーク、Ctrl+W (入れ替え)、Ctrl+C / Shift+Ctrl+C / Ctrl+V / Shift+Ctrl+V / Alt+P / Alt+V、Alt+C のコピーメニュー
  - Enter による名前の編集、Shift+M/B/S/T/C/H (パーカッション)、Shift+O (4OP の組)、F5〜F9 などの再生操作
  - MIDI ボードの鍵盤による試聴 (INSTRUMENT_test)
- 楽器ファイル: F2 で A2I、Shift+F2 で A2F を保存、F3 で A2I / A2F を読み込む (core/song/InstrumentFile)
- 本家で保存した `data/test6e1.a2i` / `test6e1.a2f` とバイト単位で一致。`test6e1.a2m` は ins_4op_flags の未使用部分
  (本家は初期化していない作業用の値を書き込む) を除いて一致することを確認

### フェーズ6e-2: 楽器エディタ (楽器窓の TAB)
- INSTRUMENT_CONTROL_edit を移植 (ui/InstEdit.inl): 2OP / 4OP / パーカッション (SD/TT/TC/HH) の 3 種類の画面
  - ADSR・波形・KSL・出力レベル・パン・接続 (4OP の 4 種類を含む)・フィードバック・微調整・AM/VIB/EG/KSR・倍率の編集
  - ADSR のプレビュー (Ctrl+Space で切り替え。標準以外の画面モードでは下に常に表示)、接続図
  - Enter / TAB / Shift+TAB によるキャリア・モジュレータ (4OP は 4 つのスロット) の移動、Shift+Enter (もう一方の内容を写す)
  - Alt+A/D/S/R/O/W/K/P/C/F/T/E/M による項目の移動、Alt+0〜4 (試聴するオペレータ)、Ctrl+[ / Ctrl+] (前後の楽器)、
    Ctrl+Shift+[ / ] (マクロの速度)、Shift+M/B/S/T/C/H/O、F2 / Shift+F2 / F3、F7
  - 楽器ごとのカーソル位置の記憶 (remember_ins_pos、bank_position_list)
- 本家との比較は画面モード 2 (本家の adtrack2.ini の sdl_screen_mode=2) で行う。zap.rad (2OP・パーカッション) と
  square.a2m (flag_4op あり) で編集した `data/test6e2.a2m` / `test6e2b.a2m` が ins_4op_flags の未使用部分を除いて一致
- 楽器エディタの F1 (ヘルプ)・Ctrl+O (オクターブ設定) は、まだエディタを開き直すだけ

### フェーズ6e-3a: マクロエディタの画面と編集
- MACRO_EDITOR を移植 (ui/MacroEdit.inl): Ctrl+Q (FM レジスタのマクロ) と Ctrl+G (アルペジオ / ビブラートのマクロ)
  - 呼び出し: パターン窓 (トレース中を含む)・オーダー窓の Ctrl+Q / Ctrl+G、楽器窓の Shift+TAB / Ctrl+Q / Ctrl+E、
    楽器エディタの Ctrl+Q / Ctrl+E。あわせてパターン窓・オーダー窓の Ctrl+E (楽器エディタ) もつないだ
  - 上段の項目 (長さ・ループ・キーオフ・表の番号など)、グラフ、FM レジスタ表・アルペジオ表・ビブラート表 (スクロールバー付き)。
    標準以外の画面モードでは窓を広げる
  - 値の入力と +/-、Ins / Del / BkSpc、列の無効化 (Ctrl+BkSpc / Alt+S / Alt+R / *)、Ctrl+E/N/Z (Ctrl+Alt で全行を消す)、
    Ctrl+Alt+C/M (キャリアとモジュレータの写し合い)、アルペジオの音名入力、Ctrl+Home/End/PgUp/PgDn (Shift 付きはループ)
  - Ctrl+C / Shift+Ctrl+C / Ctrl+V / Alt+P、Alt+C のコピーメニュー、Ctrl+Enter (楽器から行へ)、
    Ctrl+Shift+Enter / Shift+Enter (楽器を選ぶ窓 INSTRUMENT_CONTROL_alt で他の楽器から / へ)
  - Ctrl+[ / ] (前後の楽器)、Ctrl+Shift+[ / ] (マクロの速度)、Alt+0〜4、F2 / Ctrl+S (A2F の保存)
  - 横スクロールバー (HScrollBar) を DialogIO に追加
- 本家と同じキー操作で zap.rad を編集した `data/test6e3.a2m` (画面モード 2) と `test6e3m0.a2m` (画面モード 0。
  本家の adtrack2.ini を一時的に sdl_screen_mode=0 にして作成) がバイト単位で一致。両モードで画面も一致
- マクロエディタの F2 (Ctrl+G 側、A2W)・Ctrl+F2・F3・Ctrl+M・Ctrl+O・F1 は、まだエディタを開き直すだけ

### フェーズ6e-3b: マクロエディタの試聴
- _macro_preview_init / _macro_preview_body と Space の処理を移植 (ui/MacroEdit.inl)
  - Space で試聴の状態に入り (Ctrl+Space はキーオフのループ付き)、Space か ESC で抜ける (ESC はエディタも閉じる)
  - 試聴中: MIDI ボードの鍵盤でマクロを動かしながら鳴らす、Ctrl+Space (ループの切り替え)、F7、
    Ctrl+[ / ] (前後の楽器)、Ctrl+Shift+[ / ] (マクロの速度)、Alt+0〜4
  - PREViEW / LOOP の点滅表示 (macro_preview_indic_proc を realtime_gfx_poll_proc から呼ぶ)。
    オクターブの数字はオリジナルのまま 30 行目に書く (画面モード 2 ではパターン窓の中に出る)
- 本家と同じキー操作で zap.rad を編集した `data/test6e3b.a2m` (画面モード 2) がバイト単位で一致。画面も一致
  (試聴中に楽器を切り替えると、エディタで変えたアルペジオ / ビブラートの表の番号が戻ってしまう本家の動作も同じ)
- 本家 SDL 版は試聴中に鍵盤を押すと Access Violation で落ちる (get_chanpos の `pBYTE(data)[idx]` が配列の中身を
  ポインタとして読む)。クローン版は配列を正しく読み (楽器窓の試聴と同じ get_chanpos)、音を鳴らす。
  鍵盤を押す手順はクローン版だけで行い、保存結果の比較から外した (音を鳴らしても曲データは変わらない)
- 本家の確認で鍵盤を押し続けるときは、PostMessage の WM_KEYUP が SDL に届かないため keybd_event で送る

### フェーズ6e-4a: A2B / A2W の保存と A2B の読み込み
- 楽器バンクの読み書き (core/song/InstrumentFile): SaveA2b / SaveA2w (_a2b_saver / _a2w_saver)、
  LoadA2bBank (形式 1〜10。1〜8 は import_old_instruments で取り込む)、LoadA2wBank (形式 1〜3。一覧の窓は 6e-4b)
- 楽器窓の Alt+F2 (A2B) / Ctrl+F2 (A2W)。A2W は進捗の窓 (COMPRESSiNG iNSTRUMENT BANK DATA...) を出す
- a2b_file_loader を移植 (ui/BankLoad.inl): バンクの一覧 (画面モード 0/3/4/5 は 74 桁、それ以外は 104 桁)、4OP の組の印、
  一覧の位置の記憶、Enter (4OP の組は強制) / Shift+Enter (取り込む先も組のときだけ組で) / Ctrl+Enter (楽器窓からはバンク全体)
- 一覧で鍵盤を押すと試聴する (test_instrument_alt。6e-4b の窓で使う場合の処理も含めて移植)
- 楽器窓・楽器エディタの F3 で A2B も選べるようにし、楽器窓の Shift+F3 (前回の楽器ファイルをそのまま読み込む) を追加。
  本家のファイル選択画面の Shift+Enter (一覧を出さずにバンク全体を読み込む) はコモンダイアログでは区別できないため、
  常に一覧を出す (Ctrl+Enter で同じことができる)
- 本家と同じキー操作で、zap.rad / square.a2m から保存した A2B / A2W がバイト単位で一致。square.a2m で 4OP の組を作って保存した
  A2B と、それを Enter / Shift+Enter / Ctrl+Enter で取り込んで保存した A2M は、ins_4op_flags の未使用部分を除いて一致。画面も一致
- 確認用の道具は `data/tools/` に置いた (キー操作の uitest.ps1、手順を本家用とクローン版用に分ける mkscript.py、
  展開して比べる a2diff.cpp)。スクラッチパッドは読み取りが禁止されているため使えない

### フェーズ6e-4b: A2W の読み込みとマクロブラウザ
- a2w_file_loader を移植 (ui/A2wLoad.inl)
  - 楽器マクロの窓: 楽器の一覧 (画面モード 0/3/4/5 は 72 桁、1 は 102 桁、2 は 121 桁) と、選んでいる楽器の FM レジスタのマクロ
    (Shift+矢印・PgUp/PgDn・Home/End、Ctrl+Home/End で表の位置、グラフ)。Enter (4OP の組は強制) / Shift+Enter / Ctrl+Enter (バンク全体)。
    使っているアルペジオ / ビブラートの表は曲の空いている表へ写す (空きがなければ RELATED ... WAS NOT LOADED)
  - アルペジオ / ビブラートのマクロの窓: 表の一覧とグラフ、Shift / Ctrl+Space で表を選ぶ、Shift / Ctrl+矢印などで表の中の位置。
    楽器マクロの窓から TAB で開き、Enter で現在の楽器に表を付ける
  - 一覧で鍵盤を押すと試聴する (test_instrument_alt)。窓の位置を bank_position に覚える
- MACRO_BROWSER (Ctrl+M): パターン窓・オーダー窓・楽器窓・マクロエディタから。曲のデータを A2W と同じ窓で見る
- マクロエディタの F2 (Ctrl+G 側は A2W)・Ctrl+F2 (A2W)・F3 (アルペジオ / ビブラートの位置なら A2W だけ)・Ctrl+M
- 楽器を読み込むコモンダイアログで A2W も選べるようにした
- 曲を読み込んだときに、現在の楽器・マクロエディタの位置・bank_position の各画面の位置などを本家と同じく戻す
- 6e-4a の A2B の一覧で、楽器が 253 個以上あると Menu の項目数が 256 を超えて桁あふれし、止まっていたのを直した
- 本家と同じキー操作で、nightdrv.a2m に $at2demo.a2w から楽器を取り込んだ A2M (楽器窓の F3 / Shift+F3、Ctrl+Enter)、
  Ctrl+M で表を付けた A2M、マクロエディタから保存した A2W 2 つと F3 / Ctrl+M を使った A2M がバイト単位で一致。画面も一致
  (画面モード 2。画面モード 0 はクローン版だけで確認)

### 実機演奏 (27d0c14, 2c1e7bf)
- IPlayerDriver: 再生駆動部の共通インターフェース (SampleClockDriver も実装)
- TimerDriver: 高精度ウェイタブルタイマーで timerHz() 周期に timerTick() と flush() を呼ぶ
- Opl3Hardware: 実機用の IOplDevice。c86ctl インターフェースで実機へ書き込む
- 音源メニューに「OPL3 実機」を追加。実機モードでは DirectSound を使わない

---

## 今後の計画

### フェーズ6e: 楽器とマクロ
- 6e-5 (次の作業): BNK / CIF / FIB / FIN / IBK / INS / SBI / SGI の取り込み

### フェーズ6f: その他の編集機能
- Ctrl+T (移調)、Ctrl+R (楽器の置き換え)、Ctrl+O (オクターブ設定)
- Ctrl+H (置換)、Ctrl+F (曲の設定)、Alt+L (行マーク設定)
- 範囲を選んでいないときの Ctrl+B (メッセージボード) と Ctrl+X (トラックの並べ替え)
- F1 (ヘルプ)

まだ実装していないフェーズのキーは何もしない。

### 保留中の課題
- 「コマンド入力なし」モード (command_typing = 0) での楽器・エフェクトの入力。設定画面ができるまで保留。
