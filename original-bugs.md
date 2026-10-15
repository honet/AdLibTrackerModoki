# 本家 (AdLib Tracker II SDL 2.4.25) のバグ

本クローン版の開発中に見つかった、本家のバグや意図しないと思われる動作の記録。
対象はソース `original/at2_sourcecode_06-02-2026/` とバイナリ `original/AdlibTracker2_SDL_2.4.25/`。

「クローン版の扱い」の意味:

- **再現**: 出音・保存結果・画面を本家と一致させるため、同じ動作にしている
- **回避**: 落ちる・止まる・メモリを壊すなど、再現する意味がないので安全な動作にしている

場所の欄の「→」の後は、クローン版で対応する処理のある場所。

---

## 異常終了・停止

| 現象 | 場所 | 原因 | クローン版の扱い |
|---|---|---|---|
| マクロエディタの試聴中 (Space) に MIDI ボードの鍵盤を押すと Access Violation で落ちる | adt2unit.pas `get_chanpos` (SDL 版) → ui/MacroEdit.inl | 型なし引数 `var data` を `pBYTE(data)[idx]` と読んでいる。配列の中身 (0) をポインタとして読むため nil を参照する (正しくは `pBYTE(@data)`。DOS 版のアセンブラは正しい)。パーカッション楽器の試聴は `get_chanpos` を通らないので落ちない | 回避 (配列を正しく読んで音を鳴らす) |
| tempo または macro_speedup が 0 だと止まる | adt2unit.pas のタイマー処理 → core/player/A2Player.cpp | 0 除算 | 回避 (1 として扱う) |
| オーダーの 2 つのジャンプが互いを指していると止まる | オーダーをたどる処理 → ui/MainScreen.cpp | 無限ループ | 回避 (回数の上限を設ける) |
| パターン窓で `While (chan < chan_pos) do Inc(hpos)` が止まらないことがある | PATTERN_edit → ui/MainScreen.cpp | ループの中で `chan` が変わらない | 回避 (このループを省く。止まらない場合の結果は同じ) |
| `DietStr` が size < 3 で終わらない | stringio.pas `DietStr` → ui/StringIO.cpp | 文字列を縮める処理が進まない | 回避 (size 文字で切る) |
| 置換 (Ctrl+H) の窓で、入力欄の位置をまだ一度も記録していない (起動直後に右キーや文字の入力だけで下の欄・PROMPT へ進んだ) 状態で、範囲の選択肢 (PATTERN など) から TAB / ↓ を押すと止まる | adt2extn.pas `REPLACE` → ui/EditTools.inl | `get_bank_position('?replace_window?posfx')` が 0 を返し、pos が 0 になる。どの分岐にも入らず getkey を呼ばないまま回り続ける | 回避 (0 のときは 1 にする) |
| 壊れた CFF ファイルでメモリを壊して止まる | iloaders.inc (CFF の LZTYR 展開) → core/song/CffLoader.cpp | 出力の大きさを確かめていない | 回避 (出力の上限を設ける) |

## 範囲外の読み書き

| 現象 | 場所 | クローン版の扱い |
|---|---|---|
| `_panning` (array[0..2]) を範囲外の添字で読む。メモリ上で直後に続く `_instr_data_ofs` の値が使われる | adt2unit.pas → core/player/A2Player.cpp | 再現 (直後の値を含めた 16 要素の表にする) |
| マクロエディタで表の番号 0 の `macro_table[0]` を読む (直前の `instr_macros[255]` の末尾が見える) | adt2ext4.pas / adt2ext5.pas → ui/MacroEdit.inl | 再現 |
| `pattern_order` を 128 以上の添字で読む (後続のフィールドが見える) | adt2unit.pas、S3M ローダー → core/player/A2Player.h、core/song/S3mLoader.cpp | 再現 |
| マクロ試聴でパーカッション楽器を鳴らすと `_m_chan_handle[1..18]` の [19], [20] に書き込み、`_4op_main_chan[1..6]` も範囲外 (16..20) で読む | adt2ext4.pas `_macro_preview_body` → ui/MacroEdit.inl | 回避 (配列を広げる。`_4op_main_chan` は範囲内のときだけ引く) |
| 楽器の試聴で `_4op_main_chan` を範囲外で読む | 楽器の試聴 → ui/InstEdit.inl | 回避 |
| バンクの楽器の試聴で、アルペジオ / ビブラートの表を `macro_table` の 1 件分 (アルペジオ + ビブラート) の大きさで写す。アルペジオなら同じ番号のビブラート、ビブラートなら次の番号のアルペジオまで書き、255 番のビブラートでは `macro_table` の後ろ (`pattern_order` など) にはみ出す (試聴の後に戻すのは `macro_table` だけ) | iloadins.inc `test_instrument_alt` → ui/BankLoad.inl | 再現 (曲データの同じ位置へ写す) |
| 楽器窓の鳴っている楽器の印で `iflag[1..18]` を 19, 20 で読む | instedit.inc → ui/InstEdit.inl | 回避 (0 とする) |
| 楽器エディタの表示で [1..23] の配列を 24 行目まで表示する (直後の配列の文字列が出るが、下の枠で隠れる) | instedit.inc → ui/InstEdit.inl | 再現 |
| 楽器エディタで panning が 3 以上のとき `ins_pan_str1` を範囲外で読む | instedit.inc → ui/InstEdit.inl | 回避 (表示しない) |
| エフェクト番号が範囲外のとき `fx_digits` (48 文字) の後ろのメモリを読む | adt2ext*.pas → ui/MainScreen.cpp | 回避 ('?' を表示) |
| キーオフ (0xFF) などで `note_layout` の後ろのメモリを読む | パターンの表示 → ui/MainScreen.cpp | 回避 (空欄) |
| LZW 展開の表 `le7a` が 5 要素 (9..13 ビット) だが、`le78` は 14 まで増える | depackio.pas → core/song/Depack.cpp | 回避 (14 ビット分を補う) |
| LZH 展開の `make_table` で `count[1..16]` の添字 0 に書き込む | depackio.pas → core/song/Depack.cpp | 回避 |
| 展開処理が入力の終端を超えて読む。RDC 展開は出力より前 (直前のメモリ) を参照することがある | depackio.pas → core/song/Depack.cpp | 回避 (範囲外は 0) |
| 展開結果がレコードより大きいと後続のフィールド・メモリへはみ出して書く (A2M / A2T 旧形式、MTK など) | iloaders.inc → core/song/LoaderCommon.cpp ほか | 回避 (レコードの大きさまでコピー) |
| AMD で patterns が 63 を超えると `track_order` が後続の変数へはみ出す | iloaders.inc → core/song/AmdLoader.cpp | 回避 (256 パターン分確保) |
| FMK のパン値 3 で `_conv_fmk_pan[0..2]` の外を読む。パターン・楽器の数が多いと `paraptr_*` がはみ出す | iloaders.inc → core/song/FmkLoader.cpp | 回避 (0 とする) |
| SAT の `ordr2` を範囲外で読む | iloaders.inc → core/song/SatLoader.cpp | 回避 (0 とする) |
| 画面メモリの範囲外にも書き込む | txtscrio.pas → ui/TxtScrIO.cpp | 回避 (画面メモリ内に限る) |
| 移調 (Ctrl+T)・楽器の置き換え (Ctrl+R)・トラックの並べ替え (Ctrl+X)・置換 (Ctrl+H) で曲全体 (SONG) を選ぶと、使っている最後のパターンの次 (`count_patterns` の値) まで処理する。128 個すべて使っていると、パターン 128 (パターンデータの後ろのメモリ) を読み書きする | adt2extn.pas `TRANSPOSE` / `REMAP` / `REARRANGE` / `REPLACE` → ui/EditTools.inl | 回避 (パターン 127 まで) |

## 初期化していない値・前回の値が残る

| 現象 | 場所 | クローン版の扱い |
|---|---|---|
| A2M 保存時、`ins_4op_flags` の num_4op より後ろに初期化していないスタックの値が書き込まれる | adt2unit.pas `set_4op_flag` / `reset_4op_flag` | 回避 (0。本家との比較ではこの部分を除く) |
| A2P 保存時、パターン名の後ろ (31 バイト目まで) に作業用バッファの前の内容が残る | adt2ext*.pas → core/song/ModuleSaver.cpp | 回避 (0) |
| 旧形式の A2P 読み込みで、グローバル変数 `old_hash_buffer` に前回の内容が残る | iloaders.inc → core/song/A2pLoader.cpp | 回避 (0 から始める) |
| グローバルなバッファ `buf1` の、読み込んだサイズより後ろに以前の内容が残る | iloaders.inc → core/song/LoaderCommon.h | 再現 |
| `Menu` の作業用配列 (1..16384) で、項目数より後ろに前回のメニューの値が残る | dialogio.pas → ui/DialogIO.cpp | 再現 |
| SAT の再開位置を、For ループを抜けた後のループ変数で書き込む (未定義の値) | iloaders.inc → core/song/SatLoader.cpp | 回避 (1 回も回らない場合は -1 とみなす) |
| エフェクト名を返す関数が、case に当てはまらない値のとき結果が未定義 | adt2ext*.pas → ui/MainScreen.cpp | 回避 (空欄) |
| マクロエディタの表示で、case に当てはまらない値のとき前回の文字列のまま表示する | adt2ext5.pas → ui/MacroEdit.inl | 再現 |
| FIB の一覧を作るときと Enter で取り込むとき、`temp_instrument` を 0 にせずに FM データだけを書くため、`perc_voice` に前に読んだ楽器 (BNK のパーカッションなど) の値が残り、そのまま曲の楽器に入る | adt2ext5.pas `fib_file_loader`、iloadins.inc `import_standard_instrument_alt` → ui/BnkLoad.inl | 再現 (`temp_instrument` をグローバル変数にしている) |
| BNK / FIB / IBK の一覧の試聴は、鍵盤を押した楽器を読む前に `temp_instrument` (前回読んだ楽器) が空かどうかを見る。一覧を作るときに最後に読んだ項目が空だと鳴らない | iloadins.inc `test_instrument_alt2` → ui/BnkLoad.inl | 再現 |
| 曲を読み込み直しても OPL エミュレータの状態をリセットしないため、前の曲の残りが次の曲の出音に混ざる | adt2unit.pas / opl3emu.pas | — (本家の録音は曲ごとに起動し直して取る) |

## 処理の誤り (結果が変わるもの)

| 現象 | 場所 | クローン版の扱い |
|---|---|---|
| ef_SwapVibrato が `vib_pos` ではなく `vib_table` と長さを比べる | adt2unit.pas → core/player/A2Player.cpp | 再現 |
| tremolo2 (エフェクト列 2) は `dir` ではなく `pos = 0` で向きを判定する | adt2unit.pas → core/player/A2Player.cpp | 再現 |
| ピークロックの 4OP 制限値で、Word に `(car SHL 16) + mod` を代入するためキャリア側が常に 0 になる | adt2unit.pas → core/player/A2Player.cpp | 再現 |
| TremOff の判定がエフェクト列 1 は `is_4op_chan`、列 2 は `_4op_vol_valid_chan` と食い違う | adt2unit.pas → core/player/A2Player.cpp | 再現 |
| エフェクト列 1 を処理するとエフェクト列 2 を見ない (else-if になっている) | adt2unit.pas → core/player/A2Player.cpp | 再現 |
| OPL3 エミュレータのアタックで `limit_value` の結果を捨てている (範囲制限が効かない) | opl3emu.pas → core/opl/Opl3EmulatorAt2.cpp | 再現 |
| OPL3 エミュレータで 4OP のペアへ block をコピーするのが A0 (下位) 書き込み時だけ | opl3emu.pas → core/opl/Opl3EmulatorAt2.cpp | 再現 |
| A2M / A2T 旧形式の読み込みで、2 ブロック目以降も 1 ブロック目のサイズ (b2len / b3len) を展開処理に渡す | iloaders.inc → core/song/A2mLoader.cpp、A2tLoader.cpp | 再現 |
| `cstr2str` が For ループ中に Delete するため、連続する '~' の 2 つ目が残る | adt2extn.pas → core/song/ModuleLoader.cpp | 再現 |
| A2F 保存で arpeggio_table / vibrato_table を 0 にする書き込みの位置がずれている (dis_fmreg_col で上書きされ、マクロはそのまま保存される) | instedit.inc → core/song/InstrumentFile.cpp | 再現 (結果が同じになるようにしている) |
| 再生操作で、nosync_by_default のとき Alt+F9 ではなく F8 を見ている | パターン窓のキー処理 → ui/MainScreen.cpp | 再現 |
| Ctrl+Z 用の保存が `SizeOf(tPATTERN_DATA) DIV 16` バイトで、1 パターンではなく 8 パターン分を保存する | adt2ext*.pas → ui/MainScreen.cpp | 再現 |
| パターン一覧の CRC 計算で、マークは戻すが 9 文字目は #14 のまま残る | adt2ext*.pas → ui/MainScreen.cpp | 再現 |
| マクロエディタの FM レジスタ表で、フラグ列の値を #251 と比べるため常に 0 になる | adt2ext5.pas → ui/MacroEdit.inl | 再現 |
| マクロエディタの試聴中に楽器を切り替えると (Ctrl+[ / ]、Alt+0〜4)、エディタで変えたアルペジオ / ビブラートの表の番号が、エディタを開き直した時点の値に戻る | adt2ext4.pas `_macro_preview_init` → ui/MacroEdit.inl | 再現 |
| マクロエディタを閉じるときの描き直しの範囲で、高さに `window_area_inc_y` ではなく `window_area_inc_x` を足している | adt2ext4.pas `MACRO_EDITOR` → ui/MacroEdit.inl | 再現 |
| マクロ試聴中のオクターブ表示を 30 行目に固定で書くため、画面モード 2 ではパターン窓の中に出る | adt2ext4.pas `MACRO_EDITOR` → ui/MacroEdit.inl | 再現 |
| バンクの楽器の試聴で、楽器が空のときは抜けるが、その前に書き換えた曲の `macro_table` と `instr_macros` を戻さない | iloadins.inc `test_instrument_alt` → ui/BankLoad.inl | 再現 |
| A2W のアルペジオ / ビブラートの窓で、A2W ファイルから開いたとき (マクロエディタの F3) も Enter で取り込まずに閉じる。曲のデータを見るだけのとき (マクロエディタの Ctrl+M) に Enter を ESC に変える処理が、楽器マクロの窓から TAB で開いたとき以外のすべてに効いている (本家で確認) | adt2ext5.pas `a2w_file_loader` → ui/A2wLoad.inl | 再現 |
| A2W から楽器や表を取り込む先の表を探すとき、空きか同じ内容の表がなければ長さ 0 の表を後ろから探すが、曲ではなく読み込んだバンク (temp_songdata) の表の長さを見ている | adt2ext5.pas `get_free_arpeggio_table_idx` / `get_free_vibrato_table_idx` → ui/A2wLoad.inl | 再現 |
| マクロエディタのアルペジオ / ビブラートの位置で F3 を押すと `_arp_vib_loader` が TRUE になり、A2W を読み込んだときにだけ FALSE に戻る。ファイル選択を取り消すか A2W 以外を読み込むと TRUE のまま残り、次に楽器窓などの F3 で A2W を開くとアルペジオ / ビブラートの窓が出る | adt2ext4.pas `MACRO_EDITOR`、adt2extn.pas `FILE_open` → ui/MacroEdit.inl、ui/BankLoad.inl | 再現 |
| A2W の楽器マクロの窓で、FM レジスタ表の列がパーカッションで無効かどうかを、表示している楽器ではなく現在の楽器 (current_inst) で判定する | adt2ext5.pas `_dis_fmreg_col` → ui/A2wLoad.inl | 再現 |
| BNK / FIB / IBK の一覧の項目数を使っている楽器の数 (`nm_valid`) にするが、先頭以外の使っていない項目も行に並べるため、途中に使っていない項目があると、その数だけ後ろの楽器が一覧から切れて選べない | adt2ext5.pas `bnk_file_loader` / `fib_file_loader` / `ibk_file_loader` → ui/BnkLoad.inl | 再現 |
| FIB の一覧の右下の表示で、全体の数を `header.nmins` ではなく 128 と出す | adt2ext5.pas `fib_file_loader` → ui/BnkLoad.inl | 再現 |
| FIB の楽器名が空 (#0 だけ) のとき、`CutStr` は #0 を取らないため DOS 名に切り替わらず、#0 が 27 個の名前になる | adt2ext5.pas `fib_file_loader` → ui/BnkLoad.inl | 再現 |
| BNK の FEEDBACK_FM の計算 `fm XOR 1 AND 1 + ...` は、XOR が + と同じ優先順位のため `(fm XOR 1) + ...` になり、fm が 2 以上だと下位ビット以外も足される | adt2ext5.pas `import_instrument_from_data_record` → ui/BnkLoad.inl | 再現 |
| 12 バイトより小さい INS ファイルは楽器データを何も取り込まないが、楽器名だけ変えて読み込んだことにする | adt2ext3.pas `ins_file_loader` → core/song/InstrumentFile.cpp | 再現 |
| SDL 版の `get_chanpos` は内側のループを 1 回目で抜けるため、4OP でない試聴は 1 チャンネルでしか鳴らない (和音にならない) | adt2unit.pas `get_chanpos` → ui/InstEdit.inl | 再現 |
| 楽器の置き換え (Ctrl+R) の一覧の試聴で、`INSTRUMENT_test` の instr2 に BYTE_NULL を渡すため、4OP の曲 (flag_4op が 0 でない) では楽器 255 と組にした 4OP の試聴になる。楽器 255 が空だと鳴らない | adt2extn.pas `REMAP_instr_control_proc` → ui/EditTools.inl | 再現 |
| 置換 (Ctrl+H) で固定ノート (fixed_note_flag 付き) を置き換えるとき、新しい音名の '?' を埋めた結果が音名表にない (例: 新しい音名 `E??` で `C#4` を置き換えると `E#4`) と、`_find_note` が元のノート値 (フラグ付き) を返し、それにもう一度 fixed_note_flag を足すため別のノート値になる (ソースで確認) | adt2extn.pas `REPLACE` → ui/EditTools.inl | 再現 |
| 置換 (Ctrl+H) で pattern_layout が 2 のとき、新しい音名にキーオフ (`^^` + #250) を入れても、実行時に #250 を '?' に変えるため `_find_note` がキーオフと見なさず、キーオフに置き換わらない (ソースで確認) | adt2extn.pas `REPLACE` → ui/EditTools.inl | 再現 |

## 入力

| 現象 | 場所 | クローン版の扱い |
|---|---|---|
| SDL のキー番号順に `keydown[scan]` を上書きするため、同じスキャンコードのキー (グレーの矢印キーとテンキーなど) は番号の大きい方の状態になる | adt2keyb.pas `TranslateKeycodes` → platform/input/Keyboard.cpp | 再現 |
| キーを取り出す時点の修飾キーの状態で変換するため、修飾キーを離した後に取り出すと修飾が失われる | adt2keyb.pas → platform/input/Keyboard.cpp | 回避 (押した時点の状態で変換する) |
