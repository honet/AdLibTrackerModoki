# Adlib Tracker II ヘルプ (本家版)

本家 Adlib Tracker II 2.3.58 (SDL 版) の F1 ヘルプで表示されるテキストを Markdown にしたものです。
元データは `original/at2_sourcecode_06-02-2026/adt2text.pas` の `help_data` です。

- 本文は本家の英語表記 (`iNSTRUMENT` のような小文字 i を含む) をそのまま残しています。
- 本家の表示色は次のように置き換えています: キー色 (`~...~`) → `コード`、強調色 (`` `...` ``) → **太字**。
- 本家の独自フォントの文字は Unicode に置き換えています (矢印 ↑↓←→、`2OP` / `4OP`、`Hz`、`×` など)。
  `¹⁄` / `²⁄` は 4OP 楽器の 1 つ目 / 2 つ目のチャンネルを表す記号です。
- DOS 版だけの行 (`{$IFDEF GO32V2}`) は除き、SDL 版 (Windows) の内容を採用しています。
- 一部の行は `adtrack2.ini` の設定で表示が変わります。ここでは既定値で書き、もう一方を注記しています。
- `<a id="...">` は本家のヘルプのトピック名 (`HELP('topic')` で開く位置) です。

## 目次

- [GENERAL KEY REFERENCE](#general)
  - [WHEN iN NOTE RECORDER MODE](#note_recorder)
- [WAV RECORDER KEY REFERENCE](#wav_recorder)
- [PATTERN ORDER KEY REFERENCE](#pattern_order)
- [PATTERN EDiTOR KEY REFERENCE](#pattern_editor)
  - [BLOCK OPERATiONS iN PATTERN EDiTOR](#block_operations)
- [PATTERN LiST WiNDOW KEY REFERENCE](#pattern_list)
- [iNSTRUMENT CONTROL PANEL KEY REFERENCE](#instrument_control)
- [iNSTRUMENT EDiTOR WiNDOW KEY REFERENCE](#instrument_editor)
- [iNSTRUMENT MACRO EDiTOR WiNDOW KEY REFERENCE](#macro_editor)
- [APREGGiO/ViBRATO MACRO EDiTOR WiNDOW KEY REFERENCE](<#macro_editor_(av)>)
- [iNSTRUMENT MACRO BROWSER KEY REFERENCE](#macro_browser)
- [ARPEGGiO/ViBRATO MACRO BROWSER KEY REFERENCE](#macro_browser_av)
- [DEBUG iNFO WiNDOW KEY REFERENCE](#debug_info)
- [REMAP iNSTRUMENT WiNDOW KEY REFERENCE](#remap_dialog)
- [REARRANGE TRACKS WiNDOW KEY REFERENCE](#rearrange_dialog)
- [REPLACE WiNDOW KEY REFERENCE](#replace_dialog)
- [SONG VARiABLES WiNDOW KEY REFERENCE](#song_variables)
- [FiLE BROWSER KEY REFERENCE](#file_browser)
- [MESSAGE BOARD WiNDOW KEY REFERENCE](#message_board)
- [iNPUT FiELD KEY REFERENCE](#input_field)
- [MiDiBOARD KEY REFERENCE](#midiboard)
- [iNSTRUMENT REGiSTERS](#instrument_registers)
- [SUPPORTED EFFECTS](#effects_page1)

<a id="general"></a>

## GENERAL KEY REFERENCE

| Key | Function |
|---|---|
| `F1` | Help |
| `F2 (^S)` | Save file |
| `F3 (^L)` | Load file |
| `F4 (^A)` | Toggle Nuke'm dialog |
| `F5` | Play |
| `F6` | Pause |
| `F7` | Stop |
| `F8` | Play song from current pattern or order |
| `F9` | Play current pattern or order only |
| `[Ctrl] F8` | @F8 from current line ※1 |
| `[Ctrl] F9` | @F9 from current line ※1 |
| `[Alt] F6` | Single-play pattern ※1 |
| `[Alt] F5` | @F5 ※2 |
| `[Alt] F8` | @F8 ※2 |
| `[Alt] F9` | @F9 ※2 |
| `[Shift] F2` | Quick Save |
| `[Shift] F3` | Quick Load |
| `[Shift] F5` | @F5 with no Trace |
| `[Shift] F6` | Toggle Debug mode from position at cursor |
| `[Shift] F8` | @F8 with no Trace |
| `[Shift] F9` | @F9 with no Trace |
| `[Shift] Space` | Toggle MidiBoard mode ON/OFF |
| `^Space` | Toggle Note Recorder mode ON/OFF |
| `[Ctrl] Home,End` | Skip to previous/next pattern while Tracing |
| `+,-` | Same as above; play pattern from start |

- ※1 `[Ctrl] F8` / `[Ctrl] F9` / `[Alt] F6` の 3 行は本家では括弧でまとめられ、"(Pattern Editor)" "(`Shift` toggles trace)" と添えられています。
- ※2 `[Alt] F5` / `[Alt] F8` / `[Alt] F9` の 3 行は本家では括弧でまとめられ、"without synchronization" と添えられています。
  `adtrack2.ini` で `nosync_by_default=on` のときは "with synchronization" になります。
- `[Shift] F5` / `[Shift] F8` / `[Shift] F9` の "with no Trace" は `trace_by_default=on` (既定値) のときの表示です。
  `trace_by_default=off` のときは "with Trace" になります。

<a id="note_recorder"></a>

### WHEN iN NOTE RECORDER MODE

| Key | Function |
|---|---|
| `[Ctrl] ←,→` | Select group of tracks for recording |
| `[Alt] Q` | Quick reset last selected group of tracks |
| `Enter` | Start recording from current position `(*)` |
| `Space` | **Toggle** using custom instrument for all tracks (ref. `(*)`) |
| `[Alt] Space` | **Toggle** using present instruments in tracks (ref. `(*)`) |
| `MBoard keys` | Write notes to corresponding tracks (ref. `(*)`) |
| `F8,F9` | Toggle pattern repeat OFF/ON (ref. `(*)`) |
| `Backspace` | Clear note/instrument sequence in tracks |
| `^Backspace` | Clear complete note/instrument columns |
| `↑,↓` | Rewind/Fast-Forward while recording |
| `[Shift] ↑,↓` | Increase/Decrease row correction for writing notes |
| `[Shift] F6` | Continue in Debug mode from position at cursor |
| `F7` | Stop recording and reset starting position; current group of tracks can be modified |
| `[Alt] 1..9,0` | Toggle track channel ON/OFF (`Shift` toggles 1**X**) |
| `[Alt] R` | Reset flags on all tracks |
| `*` | Reverse ON/OFF on all tracks |

In case you need non-continuos track selection, you can choose  
from already selected group a subset of tracks where notes will be  
written by manipulating track ON/OFF flags.

### iF SONG iS PLAYED WiTH TRACE, iT CAN BE REMOVED WHiLE...

| Key | Function |
|---|---|
| `Enter` | Playback is paused and cursor stays on position |
| `Esc` | Cursor jumps to last position and playback continues |
| `[Shift] Esc` | Cursor stays on position and playback continues |

| Key | Function |
|---|---|
| `^Enter` | Play next pattern according to order |
| `[Ctrl] ← (↑)` | Rewind current pattern (with Trace) |
| `[Ctrl] → (↓)` | Fast-Forward (with Trace) |
| `[Ctrl]{Shift} ↑,↓` | Change playback speed up/down **{fine stepping}** |
| `[Ctrl]{Shift} [↑↓]` | Reset playback speed **{default speed}** |
| `[Ctrl][Alt] <hold down>` | Temporarily show Debug Info window |
| `^B` | Toggle Message Board window |
| `^D` | Toggle Debug Info window |
| `^Q` | Toggle Instrument Macro Editor window |
| `^G` | Toggle Arpeggio/Vibrato Macro Editor window |
| `^M` | Toggle Macro Browser window |
| `^F` | Toggle Song Variables window |
| `^H` | Toggle Replace window |
| `^I` | Toggle Instrument Control panel |
| `^E` | Toggle Instrument Editor window |
| `^O` | Toggle Octave Control panel |
| `^P` | Toggle Pattern List window |
| `^R` | Toggle Remap Instrument window |
| `^T` | Toggle Transpose window |
| `^X` | Toggle Rearrange Tracks window |
| `^1..^8` | Quick-set octave |
| `[Alt]{Shift} +,- {↑,↓}` | Adjust volume level of sound output |
| `[Alt] C` | Copy object to clipboard (with selection) |
| `[Alt] P` | Paste object from clipboard |
| `[Alt] M` | Toggle marking lines ON/OFF |
| `[Alt] L` | Toggle Line Marking Setup window |
| `[Alt] 1..9,0` | Toggle track channel ON/OFF (`Shift` toggles 1**X**) |
| `[Alt] S` | Set all OFF except current track (solo) |
| `[Alt] R` | Reset flags on all tracks |
| `*` | Reverse ON/OFF on all tracks |
| `F10` | Quit program |
| `F11` | Toggle typing mode in Pattern Editor (**AT**→**FT**→**ST**) |
| `F12` | Toggle **line feed** in Pattern Editor |
| `[Shift] F12` | Toggle **jump to marked line** in Pattern Editor |
| `[Ctrl][Tab] [...] (*)` | Scroll Volume Analyzer section (if necessary) |

(*) `↑,↓,PgUp,PgDown`

<a id="wav_recorder"></a>

## WAV RECORDER KEY REFERENCE

| Key | Function |
|---|---|
| `[Alt\|Ctrl]{Shift} F11` | Toggle WAV recording ON |
| `[Alt\|Ctrl]{Shift} F12` | Toggle WAV recording OFF |

### FUNCTiONALiTY OF ALTERNATiVE KEYS

| Key | Function |
|---|---|
| `Alt` | Toggle normal recording mode |
| `Ctrl` | Toggle 'per track' recording mode |
| `Shift` | Toggle Fade in / Fade out sound processing |

POSSiBLE COMBiNATiONS: Alt,Ctrl,Alt+Shift,Ctrl+Shift

If 'per track' recording mode is activated and song playback is stopped,  
you can exclude/include corresponding tracks from/to being recorded  
with ordinary track selection procedure:

| Key | Function |
|---|---|
| `[Alt] 1..9,0` | Toggle track channel ON/OFF (`Shift` toggles 1**X**) |
| `[Alt] S` | Set all OFF except current track (solo) |
| `[Alt] R` | Reset flags on all tracks |

<a id="pattern_order"></a>

## PATTERN ORDER KEY REFERENCE

| Key | Function |
|---|---|
| `↑,↓,←,→` | Cursor navigation |
| `PgUp,PgDn` | Move up/down 32 patterns |
| `Home,End` | Move to the top/end of pattern order |
| `Tab,[Shift] Tab` | Move to next/previous entry |
| `Insert` | Insert entry |
| `Delete` | Delete entry |
| `Backspace` | Clear entry |
| `^Space` | Enter skip mark |
| `^C` | Copy entry to clipboard |
| `^V` | Paste entry from clipboard |
| `+,-` | Adjust entry |
| `^F2` | Save module in tiny format |
| `Enter` | Switch to Pattern Editor |

ORDER ENTRiES: 0-7F  
► 80-FF = jump to pattern order 0-7F  
syntax: order\_number[hex](+80h); e.g. "9A" jumps to order 1A

<a id="pattern_editor"></a>

## PATTERN EDiTOR KEY REFERENCE

| Key | Function |
|---|---|
| `↑,↓,←,→` | Cursor navigation |
| `PgUp,PgDn` | Move up/down 16 lines |
| `Home,End` | Move to the top/end of current pattern |
| `Tab,[Shift] Tab` | Move to next/previous track |
| `[Shift] PgDn,PgUp (+,-)` | Move to next/previous pattern |
| `[Shift] Home,End` | Move fwd./bckwd. to the first/last pattern |
| `^Home,^End` | Move to the end/top of previous/next pattern |
| `Space` | Advance to next row |
| `^PgUp,^PgDn` | Transpose note or block halftone up/down |
| `Backspace` | Remove note or clear attributes |
| `Insert` | Insert new line (within track only) |
| `Delete` | Delete line (within track only) |
| `[Shift] Insert` | Insert new line |
| `[Shift] Delete` | Delete line |
| `[Shift] Enter` | Toggle fixed and regular note |
| `^K` | Insert Key-Off |
| `^C` | Copy object at cursor to clipboard |
| `^V` | Paste object from clipboard |
| `[Alt][Shift] P` | Paste object from clipboard to more patterns |
| `^Z` | Undo last operation (if possible) |
| `{Ctrl} "[","]"` | Change current instrument |
| `[Alt] F2` | Save current pattern to file |
| `^F2` | Save module in tiny format |
| `[Shift] F3` | Quick load recent pattern data |
| `Enter` | Switch to Pattern Order |

NOTE SYSTEM: C,C#,D,D#,E,F,F#,G,G#,A,A#,B(H)  
VALiD NOTE ENTRiES: C,C-,C#,C1,C-1,C#1...

<a id="block_operations"></a>

### BLOCK OPERATiONS iN PATTERN EDiTOR

Starting to mark a block: `[Shift] ↑,↓,←,→`  
When at least one row in one track is marked, you can continue  
marking also with `PgUp,PgDn,Home,End` (`Shift` is still held down!)

Quick mark: `[Alt] Q` (1x-2x-3x) track ─► pattern ─► discard  
Toggle last marked block: `[Alt] B`

| Key | Function |
|---|---|
| `^B` | Blank block (Insert blank block to pattern) |
| `^C` | Copy block (Copy block to clipboard) |
| `^D` | Delete block (Remove block from pattern) |
| `^N` | Nuke block (Clear block contents) |
| `^V` | Paste block (Paste block from clipboard to pattern) `(*)` |
| `^X` | Cut block (Combine both Copy and Delete operation) |

**(\*) PASTE BLOCK OPERATiON VARiANTS**

"Paste block" operation has three other functional variants  
with different key shortcuts for activation:

1. `[Alt] V` toggles "Mix block" operation, when block data
   from clipboard is applied without overwriting existing data;
2. `[Shift] ^V` toggles "Selective paste block" operation,
   when only block data from clipboard corresponding to current
   cursor position is being applied (i.e. note, instrument,
   1st effect or 2nd effect);
3. `[Alt][Shift] V` toggles "Flipped paste block" operation,
   when block data from clipboard is applied vertically flipped.

**MANiPULATiON WiTH FX VOLUME iNFORMATiON**

When there is block marked, which contains some effect  
commands carrying volume information, you can increase/decrease  
their values with `+`/`-` keys.  
Effect commands are processed with following priority:  


1. Set instrument volume (`Cxx`), Force instrument volume (`=xx`)
2. Set modulator volume (`9xx`)
3. Set carrier volume (`Ixx`)
4. Set global volume (`%xx`)

If effect command with higher priority has been processed,  
all remaining effect commands with lower priority are skipped.

<a id="pattern_list"></a>

## PATTERN LiST WiNDOW KEY REFERENCE

| Key | Function |
|---|---|
| `↑,↓` | Cursor navigation |
| `PgUp,PgDn` | Move up/down 20 patterns |
| `Home,End` | Move to the top/end of pattern list |
| `Space` | Mark/Unmark pattern |
| `^Space` | Unmark all marked patterns |
| `[Shift] ^Space` | Reverse marks on all patterns |
| `[Alt] C (^C)` | Copy pattern to clipboard |
| `[Alt] P (^V)` | Paste pattern from clipboard |
| `[Shift] ^V` | Paste pattern data from clipboard |
| `[Alt] V` | Paste pattern name(s) from clipboard |
| `^N` | Nuke current pattern |
| `[Shift] ^N` | Nuke all marked patterns |
| `^W` | Swap marked patterns |
| `[Shift] ^W` | Swap marked patterns w/o names |
| `[Shift] Insert` | Insert new pattern |
| `[Shift] Delete` | Delete current pattern |
| `Enter` | Rename pattern / Multiple paste |
| `[Shift] F3` | Quick load recent pattern data |
| `Esc` | Return to Pattern Editor or Pattern Order |

<a id="instrument_control"></a>

## iNSTRUMENT CONTROL PANEL KEY REFERENCE

| Key | Function |
|---|---|
| `↑,↓` | Cursor navigation |
| `PgUp,PgDn` | Move up/down 16 instruments |
| `Home,End` | Move to the top/end of instrument list |
| `Space` | Mark/Unmark instrument |
| `Enter` | Rename instrument |
| `^C` | Copy instrument to clipboard |
| `[Shift] ^C` | Copy instrument also with macro-definitions |
| `^V` | Paste instrument(s) from clipboard |
| `[Shift] ^V` | Paste instrument data from clipboard |
| `[Alt] V` | Paste instrument name(s) from clipboard |
| `^W` | Swap marked instruments |
| `[Shift] ^W` | Swap marked instruments w/o names |
| `Tab` | Toggle Instrument Editor window |
| `[Shift] Tab` | Toggle Macro Editor window |
| `[Shift] O` | Toggle operator mode 2OP / 4OP |
| `[Shift] M,B,S,T,C,H` | Toggle **m**elodic and percussion (**B**D,**S**D,**T**T,T**C**,**H**H) |
| `[Shift] F2` | Save instrument w/ fm-register macro to file |
| `[Alt] F2` | Save instrument bank to file |
| `^F2` | Save instrument bank w/ all macros to file |
| `[Shift] F3` | Quick load recent instrument data |
| `MBoard keys <hold down>` | Preview instrument |
| `Esc` | Return to Pattern Editor or Pattern Order |

<a id="instrument_editor"></a>

## iNSTRUMENT EDiTOR WiNDOW KEY REFERENCE

| Key | Function |
|---|---|
| `↑,↓,←,→,Home,End` | Cursor navigation |
| `[Alt] <section hotkey>` | Jump to section |
| `Tab` | Jump to next setting |
| `[Shift] Tab` | Jump to previous setting |
| `PgUp,PgDn (+,-)` | Adjust value |
| `Space` | Select item |
| `^Space (opt.)` | Toggle ADSR preview ON/OFF |
| `[Ctrl] "[","]"` | Change current instrument |
| `[Ctrl][Shift] "[","]"` | Change macro speed |
| `[Alt]{Shift} 1..4,0` | Set operators for instrument preview `(*)` |
| `Enter` | Toggle carrier/modulator/4OP slot settings |
| `[Shift] O` | Toggle operator mode 2OP / 4OP |
| `[Shift] M,B,S,T,C,H` | Toggle **m**elodic and percussion (**B**D,**S**D,**T**T,T**C**,**H**H) |
| `[Shift] F2` | Save instrument w/ fm-register macro to file |
| `[Shift] Enter` | Copy values from carrier/modulator slot |
| `MBoard keys <hold down>` | Preview instrument |
| `Esc` | Return to Instrument Control panel |

(*) Set operators for instrument preview:

| Key | Function |
|---|---|
| `[Alt] 1..4` | Set solo operator |
| `[Alt][Shift] 1..4` | Toggle operator ON/OFF |
| `[Alt] 0` | Reset |

<a id="macro_editor"></a>

## iNSTRUMENT MACRO EDiTOR WiNDOW KEY REFERENCE

| Key | Function |
|---|---|
| `↑,↓,←,→,Home,End` | Cursor navigation |
| `PgUp,PgDown` | Move up/down 16 lines |
| `Tab (Enter)` | Jump to next field in order |
| `[Shift] Tab` | Jump to previous field in order |
| `[Shift] ↑,↓` | Synchronous navigation within tables |
| `[Shift] Home,End` | Move to the start/end of current line in table |
| `[Ctrl] ←,→` | Switch between macro tables |
| `[Ctrl][Shift] ←,→` | Navigate to start/end of macro table |
| `^PgUp,^PgDown` | Change current arpeggio/vibrato table |
| `[Ctrl] "[","]"` | Change current instrument |
| `[Ctrl][Shift] "[","]"` | Change macro speed |
| `[Alt]{Shift} 1..4,0` | Set operators for instrument preview `(*)` |
| `[Alt] ^C` | Copy values from carrier column |
| `[Alt] ^M` | Copy values from modulator column |
| `^C` | Copy line in table (whole table respectively) |
| `[Shift] ^C` | Copy column in table |
| `^V` | Paste object from clipboard |
| `^Enter` | Paste data from instrument registers |
| `[Shift] Enter` | Paste data to instrument registers |
| `[Shift] ^Enter` | Paste data from instrument registers w/ selection |
| `Backspace` | Clear current item in table |
| `[Shift] Backspace` | Clear line in table |
| `+,-` | Adjust value at cursor / current item in table |
| `^Home,^End` | Quick-adjust table length |
| `[Shift] ^Home,^End` | Quick-adjust loop begin position |
| `[Shift] ^PgUp,^PgDown` | Quick-adjust loop length |
| `Insert` | Insert new line in table |
| `Delete` | Delete line in table |
| `^E` | Toggle envelope restart ON/OFF (FM-register table) |
| `^N` | Toggle note retrigger ON/OFF (FM-register table) |
| `^Z` | Toggle ZERO frequency ON/OFF (FM-register table) |
| `[Alt] ^E,^N,^Z` | Reset all alike flags in table (FM-register table) |
| `^Backspace` | Toggle corresponding column ON/OFF (FM-register table) |
| `[Alt] S` | Set all OFF except current column (FM-register table) |
| `[Alt] R` | Reset flags on all columns (FM-register table) |
| `*` | Reverse ON/OFF on all columns (FM-register table) |
| `\` | Toggle current item (switch types only) |
| `Space` | Toggle macro-preview mode |
| `^Space` | Toggle Key-Off loop within macro-preview mode |
| `^F2` | Save instrument bank w/ all macros to file |
| `Esc` | Leave Instrument Macro Editor window |

(*) Set operators for instrument preview:

| Key | Function |
|---|---|
| `[Alt] 1..4` | Set solo operator |
| `[Alt][Shift] 1..4` | Toggle operator ON/OFF |
| `[Alt] 0` | Reset |

<a id="macro_editor_(av)"></a>

## APREGGiO/ViBRATO MACRO EDiTOR WiNDOW KEY REFERENCE

| Key | Function |
|---|---|
| `↑,↓,←,→,Home,End` | Cursor navigation |
| `PgUp,PgDown` | Move up/down 16 lines |
| `Tab (Enter)` | Jump to next field in order |
| `[Shift] Tab` | Jump to previous field in order |
| `[Shift] ↑,↓` | Synchronous navigation within tables |
| `[Ctrl] ←,→` | Switch between macro tables |
| `[Ctrl][Shift] ←,→` | Navigate to start/end of macro table |
| `^PgUp,^PgDown` | Change current arpeggio/vibrato table |
| `[Ctrl] "[","]"` | Change current instrument |
| `[Ctrl][Shift] "[","]"` | Change macro speed |
| `[Alt]{Shift} 1..4,0` | Set operators for instrument preview `(*)` |
| `^C` | Copy line in table (whole table respectively) |
| `[Shift] ^C` | Copy column in table |
| `^V` | Paste object from clipboard |
| `Backspace` | Clear current item in table |
| `[Shift] Backspace` | Clear line in table |
| `+,-` | Adjust value at cursor / current item in table |
| `^Home,^End` | Quick-adjust table length |
| `[Shift] ^Home,^End` | Quick-adjust loop begin position |
| `[Shift] ^PgUp,^PgDown` | Quick-adjust loop length |
| `Space` | Toggle macro-preview mode |
| `^Space` | Toggle Key-Off loop within macro-preview mode |
| `[Shift] Esc` | Apply table indexes to current instrument |
| `Esc` | Leave Arpeggio/Vibrato Macro Editor window |

(*) Set operators for instrument preview:

| Key | Function |
|---|---|
| `[Alt] 1..4` | Set solo operator |
| `[Alt][Shift] 1..4` | Toggle operator ON/OFF |
| `[Alt] 0` | Reset |

<a id="macro_browser"></a>

## iNSTRUMENT MACRO BROWSER KEY REFERENCE

| Key | Function |
|---|---|
| `↑,↓,←,→,Home,End` | Cursor navigation |
| `[Shift] ↑,↓` | Move up/down in macro table |
| `[Shift] ←,→` | Move left/right in macro table |
| `[Shift] PgUp,PgDown` | Move page up/down in macro table |
| `[Shift] Home,End` | Move to the start/end of macro table |
| `[Ctrl] Home,End` | Move to the start/end of line in macro table |
| `Enter` | Load selected macro data |
| `^Enter (opt.)` | Load all macro data from bank |
| `[Ctrl][Shift] "[","]"` | Change macro speed |
| `MBoard keys <hold down>` | Preview instrument with selected macro data |
| `Tab (opt.)` | Switch to Arpeggio/Vibrato Macro Browser window |
| `Esc` | Leave Instrument Macro Browser window |

<a id="macro_browser_av"></a>

## ARPEGGiO/ViBRATO MACRO BROWSER KEY REFERENCE

| Key | Function |
|---|---|
| `↑,↓,PgUp,PgDown,Home,End` | Cursor navigation |
| `[Shift] ←,→` | Move left/right in arpeggio table (refer to `(*)`) |
| `[Shift] PgUp,PgDown` | Move page left/right in arpeggio table (refer to `(*)`) |
| `[Ctrl] ←,→` | Move left/right in vibrato table (refer to `(*)`) |
| `^PgUp,^PgDown` | Move page left/right in vibrato table (refer to `(*)`) |
| `[Shift]{Alt} Space` | Toggle arpeggio table selection `(**)` (refer to `(*)`) |
| `[Ctrl] {Alt} Space` | Toggle vibrato table selection `(**)` (refer to `(*)`) |
| `[Shift] Home,End` | Navigate to start/end of arpeggio table |
| `^Home,^End` | Navigate to start/end of vibrato table |
| `[Ctrl] "[","]"` | Change current instrument |
| `[Ctrl][Shift] "[","]"` | Change macro speed |
| `MBoard keys <hold down>` | Preview instrument with selected macro data |
| `Enter` | Load selected macro data |
| `^Enter (opt.)` | Load all macro data from bank |
| `Esc` | Leave Arpeggio/Vibrato Macro Browser window |

`(*)` Key combination with `Ctrl+Shift` applies action to both tables  
`(**)` `Alt` key invokes no arpeggio resp. vibrato table (index value reset)

<a id="debug_info"></a>

## DEBUG iNFO WiNDOW KEY REFERENCE

| Key | Function |
|---|---|
| `↑,↓,PgUp,PgDown,Home,End` | Change current track |
| `Tab` | Toggle details |
| `Backspace` | Toggle pattern repeat |
| `Space` | Enter Debug mode / Proceed step |
| `^Space` | Exit Debug mode |
| `[Ctrl] Home,End` | Skip to previous/next pattern |
| `+,-` | Same as above; play pattern from start |
| `^Enter` | Play next pattern according to order |
| `[Ctrl] ←,→` | Rewind/Fast-Forward current pattern |
| `[Alt] 1..9,0` | Toggle track channel ON/OFF (`Shift` toggles 1**X**) |
| `[Alt] S` | Set all OFF except current track (solo) |
| `[Alt] R` | Reset flags on all tracks |
| `*` | Reverse ON/OFF on all tracks |
| `Esc` | Return to Pattern Editor or Pattern Order |

<a id="remap_dialog"></a>

## REMAP iNSTRUMENT WiNDOW KEY REFERENCE

| Key | Function |
|---|---|
| `↑,↓,←,→,Home,End` | Cursor navigation |
| `PgUp,PgDown` | Move up/down 16 instruments |
| `Tab` | Jump to next selection |
| `[Shift] Tab` | Jump to previous selection |
| `MBoard keys <hold down>` | Preview instrument |
| `Enter` | Remap |
| `Esc` | Return to Pattern Editor or Pattern Order |

<a id="rearrange_dialog"></a>

## REARRANGE TRACKS WiNDOW KEY REFERENCE

| Key | Function |
|---|---|
| `↑,↓,←,→,Home,End` | Cursor navigation |
| `Tab` | Jump to next selection |
| `[Shift] Tab` | Jump to previous selection |
| `^PgUp,^PgDown` | Shift track at cursor up/down in the track list |
| `[Shift] ^PgUp,^PgDown` | Rotate track list from cursor upside/downside |
| `Enter` | Rearrange |
| `Esc` | Return to Pattern Editor or Pattern Order |

<a id="replace_dialog"></a>

## REPLACE WiNDOW KEY REFERENCE

| Key | Function |
|---|---|
| `↑,↓,←,→,Home,End` | Cursor navigation |
| `Tab` | Jump to next selection |
| `[Shift] Tab` | Jump to previous selection |
| `^K` | Insert Key-Off in note column |
| `^N` | Mark "new" field to clear found item |
| `^W` | Swap "to find" and "replace" mask content |
| `Delete,Backspace` | Delete current/previous character |
| `^Backspace` | Delete "to find" or "replace" mask content |
| `[Shift] ^Backspace` | Delete content of both masks |
| `Enter` | Replace |
| `Esc` | Return to Pattern Editor or Pattern Order |

<a id="song_variables"></a>

## SONG VARiABLES WiNDOW KEY REFERENCE

| Key | Function |
|---|---|
| `↑,↓,←,→` | Cursor navigation |
| `[Alt] <section hotkey>` | Jump to section |
| `Tab (Enter)` | Jump to next variable field |
| `[Shift] Tab` | Jump to previous variable field |
| `Space` | Select item |
| `^Enter` | Setup rows per beat for BPM calculation |
| `Esc` | Return to Pattern Editor or Pattern Order |

<a id="file_browser"></a>

## FiLE BROWSER KEY REFERENCE

| Key | Function |
|---|---|
| `↑,↓,←,→,PgUp,PgDown,Home,End` | Cursor navigation |
| `\` | Navigate to drive root |
| `Backspace` | Navigate to parent directory |
| `[Shift] Backspace` | Navigate to program home directory |
| `MBoard keys <hold down>` | Preview instrument (instrument files only) |
| `Enter` | Choose file under cursor / read instrument bank |
| `Esc` | Leave without choosing file |

<a id="message_board"></a>

## MESSAGE BOARD WiNDOW KEY REFERENCE

| Key | Function |
|---|---|
| `↑,↓,←,→,^PgUp,^PgDown,Home,End,^Home,^End` | Cursor navigation |
| `PgUp,PgDown` | Move backwards/forwards over text |
| `[Ctrl] ←,→` | Move word left/right |
| `Backspace,Delete` | Delete character left/right |
| `^Backspace,^T` | Delete word left/right |
| `^K` | Delete characters to end |
| `^Y` | Delete current line |
| `Tab` | Indent current line |
| `^Space` | Insert row for text at cursor |
| `[Shift] ^Backspace` | Delete row for text at cursor |
| `Insert` | Toggle input and overwrite mode |
| `Enter` | Wrap line of text |
| `Esc` | Return to Pattern Editor or Pattern Order |

<a id="input_field"></a>

## iNPUT FiELD KEY REFERENCE

| Key | Function |
|---|---|
| `←,→,Home,End` | Cursor navigation |
| `[Ctrl] ←,→` | Move word left/right |
| `Backspace,Delete` | Delete character left/right |
| `^Backspace,^T` | Delete word left/right |
| `^K` | Delete characters to end |
| `^Y` | Delete string |
| `Insert` | Toggle input and overwrite mode |
| `+,-` | Increment/decrement decimal or hexadecimal value |

<a id="midiboard"></a>

## MiDiBOARD KEY REFERENCE

```
     C#  D#      F#  G#  A#      C#  D#      F#  G#  A#      C#  D#

  ▐██  ▐█  ▐██▐██  ▐█  ▐█  ▐██▐██  ▐█  ▐██▐██  ▐█  ▐█  ▐██▐██  ▐█  ▐██
  ▐██  ▐█  ▐██▐██  ▐█  ▐█  ▐██▐██  ▐█  ▐██▐██  ▐█  ▐█  ▐██▐██  ▐█  ▐██
  ▐██ S▐█ D▐██▐██ G▐█ H▐█ J▐██▐██ 2▐█ 3▐██▐██ 5▐█ 6▐█ 7▐██▐██ 9▐█ 0▐██
  ▐██  ▐█  ▐██▐██  ▐█  ▐█  ▐██▐██  ▐█  ▐██▐██  ▐█  ▐█  ▐██▐██  ▐█  ▐██
  ▐██  ▐█  ▐██▐██  ▐█  ▐█  ▐██▐██  ▐█  ▐██▐██  ▐█  ▐█  ▐██▐██  ▐█  ▐██
  ▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███
  ▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███
  ▐▌Z▐▐▌X▐▐▌C▐▐▌V▐▐▌B▐▐▌N▐▐▌M▐▐▌Q▐▐▌W▐▐▌E▐▐▌R▐▐▌T▐▐▌Y▐▐▌U▐▐▌I▐▐▌O▐▐▌P▐
  ▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███
  ▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███▐███

    C   D   E   F   G   A   B   C   D   E   F   G   A   B   C   D   E
```

### WHiLE TRACKER iS iN MiDiBOARD MODE

`MBoard key` copies note in note field, plays it, and advances song  
to next row. If used with `Left-Shift` key and line marking toggled ON,  
it advances song to next highlighted row.  
If used with `Right-Shift` key, it makes a fixed note.  
`Space` plays row and advances song by one row.  
`` ` `` inserts Key-Off, releases playing note and advances to next row.

<a id="instrument_registers"></a>

## iNSTRUMENT REGiSTERS

### Attack rate

Indicates how fast the sound volume goes to maximum.  
1=slow, 15=fast. 0 means no attack phase.

### Decay rate

Indicates how fast the sound goes from maximum level to sustain level.  
1=slow, 15=fast. 0 means no decay phase.

### Sustain level

Indicates the sustain level.  
1=loudest, 15=softest. 0 means no sustain phase.

### Release rate

Indicates how fast the sound goes from sustain level to zero level.  
1=slow, 15=fast. 0 means no release phase.

### Output level

Ranges from 0 to 63, indicates the attenuation according to the  
envelope generator output. In Additive synthesis, varying  
the output level of any operator varies the volume of its corresponding  
channel. In FM synthesis, varying the output level of carrier varies  
the volume of its corresponding channel, but varying the output of  
the modulator will change the frequency spectrum produced by the carrier.

### Waveform select

Specifies the output waveform type.  
The first is closest to pure sine wave, the last is most distorted.

**[0] SiNE**

```
     ▲
     │
     │     __                      __
     │   /    \                  /    \
     │ /        \              /        \
    ─┼───────────┬───────────┬───────────┬───────────┬───►
     │             \        /              \        /
     │               \    /                  \    /
     │                 ‾‾                      ‾‾
     │          1/2          1          3/2          2  pi
```

**[1] HALF-SiNE**

```
     ▲
     │
     │     __                      __
     │   /    \                  /    \
     │ /        \              /        \
    ─┼───────────┬───────────┬───────────┬───────────┬───►
     │
     │          1/2          1          3/2          2  pi
     │
     │
```

**[2] ABS-SiNE**

```
     ▲
     │
     │     __         __           __         __
     │   /    \     /    \       /    \     /    \
     │ /        \ /        \   /        \ /        \
    ─┼───────────┬───────────┬───────────┬───────────┬───►
     │
     │          1/2          1          3/2          2  pi
     │
     │
```

**[3] PULSE-SiNE**

```
     ▲
     │
     │    _           _           _           _
     │  /  |        /  |        /  |        /  |
     │/    |      /    |      /    |      /    |
    ─┼─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬───►
     │
     │    1/4    1/2   3/4   1    5/4   3/2   7/4    2  pi
     │
     │
```

**[4] SiNE, EVEN PERiODS ONLY (EPO)**

```
     ▲
     │
     │
     │ /‾\                     /‾\
     │/   \                   /   \
    ─┼─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬───►
     │      \   /                   \   /
     │       \_/                     \_/
     │
     │    1/4   1/2   3/4    1    5/4   3/2   7/4    2  pi
```

**[5] ABS-SiNE, EVEN PERiODS ONLY (EPO)**

```
     ▲
     │
     │
     │ /‾\   /‾\               /‾\   /‾\
     │/   \ /   \             /   \ /   \
    ─┼─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬───►
     │
     │    1/4   1/2   3/4    1    5/4   3/2   7/4    2  pi
     │
     │
```

**[6] SQUARE**

```
     ▲
     │
     │
     ├-----------┐           ┌-----------┐
     │           │           │           │
    ─┼───────────┼───────────┼───────────┼───────────┬───►
     │           │           │           │           │
     │           └-----------┘           └-----------┘
     │
     │          1/2          1          3/2          2  pi
```

**[7] DERiVED SQUARE**

```
     ▲
     │
     │
     │\__                    │\__
     │   ‾‾‾───__            │   ‾‾‾‾──__
    ─┼───────────┼───────────┼───────────┼───────────┬───►
     │            ‾‾───___   │            ‾‾──____   │
     │                    ‾‾\│                    ‾‾\│
     │
     │          1/2          1          3/2          2  pi
```

### Key scaling level (KSL)

When set, makes the sound softer at higher frequencies.  
With musical instruments, volume decreases as pitch increases.  
Level key scaling values are used to simulate this effect.  
If any (not zero), the diminishing factor can be 1.5 dB/octave,  
3.0 dB/octave, or 6.0 dB/octave.

### Panning

Gives you ability of controlling output, going to left or right channel,  
standing in the middle respectively.  
The parameter corresponds either with carrier and modulator, therefore  
it is listed only once (within the carrier slot).

### Fine-tune

This is not a hardware parameter.  
Ranges from -127 to 127, it indicates the number of frequency units  
shifted up or down for any note playing with the corresponding instrument.  
The parameter corresponds either with carrier and modulator, therefore  
it is listed only once (within the carrier slot).

### Feedback strength

Ranges from 0 to 7, it indicates the modulation depth  
for the modulator slot FM feedback.

| **FEEDBACK** | **[0]** | **[1]** | **[2]** | **[3]** | **[4]** | **[5]** | **[6]** | **[7]** |
|---|---|---|---|---|---|---|---|---|
| **MODULATiON** | 0 | 1/16 | 1/8 | 1/4 | 1/2 | 1 | 2 | 4 |

The parameter corresponds either with carrier and modulator, therefore  
it is listed only once (within the carrier slot).

### Connection type

Frequency modulation means that the modulator slot modulates the carrier.  
Additive synthesis means that both slots produce sound on their own.

**[FM] FREQUENCY MODULATiON**

```
        ┌────────────┐
        │            │
        ▼    ╔════╗  │         ╔════╗
 P1 ──►(+)──►║ MO ╟──┴──►(+)──►║ CA ╟──► OUT
             ╚════╝       ▲    ╚════╝
                          │

                          P2
```

**[ADDiTiVE SYNTHESiS] AM**

```
        ┌────────────┐
        │            │
        ▼    ╔════╗  │
 P1 ──►(+)──►║ MO ╟──┴────┐
             ╚════╝       │
                          ▼
                         (+)──► OUT
                          ▲
             ╔════╗       │
 P2 ────────►║ CA ╟───────┘
             ╚════╝
```

The parameter corresponds either with carrier and modulator, therefore  
it is listed only once (within the carrier slot).  
This parameter is also very important when making 4-op instruments,  
because the combination of two instrument connections specifies  
the connection of the 4-op instrument as shown below:

| **SLOT** | M1 | C1 | M2 | C2 |
|---|---|---|---|---|
| **OPERATOR** | 1 | 2 | 3 | 4 |

**[FM/FM]**

```
        ┌────────────┐
        │            │
        ▼    ╔════╗  │         ╔════╗         ╔════╗         ╔════╗
 P1 ──►(+)──►║ M1 ╟──┴──►(+)──►║ C1 ╟──►(+)──►║ M2 ╟──►(+)──►║ C2 ╟──► OUT
             ╚════╝       ▲    ╚════╝    ▲    ╚════╝    ▲    ╚════╝
                          │              │              │

                          P2             P3             P4
```

**[FM/AM]** `(*)`

```
        ┌────────────┐
        │            │
        ▼    ╔════╗  │         ╔════╗
 P1 ──►(+)──►║ M1 ╟──┴──►(+)──►║ C1 ╟────┐
             ╚════╝       ▲    ╚════╝    │
                          │              │
                                         ▼
                          P2            (+)──► OUT
                                         ▲
                                         │
             ╔════╗            ╔════╗    │
 P3 ────────►║ M2 ╟─────►(+)──►║ C2 ╟────┘
             ╚════╝       ▲    ╚════╝
                          │

                          P4
```

**[AM/FM]** `(*)`

```
        ┌────────────┐
        │            │
        ▼    ╔════╗  │
 P1 ──►(+)──►║ M1 ╟──┴──────────────────────────────────┐
             ╚════╝                                     │
                                                        │
                                                        │
                                                        │
             ╔════╗            ╔════╗         ╔════╗    ▼
 P2 ────────►║ C1 ╟─────►(+)──►║ M2 ╟──►(+)──►║ C2 ╟──►(+)──► OUT
             ╚════╝       ▲    ╚════╝    ▲    ╚════╝
                          │              │

                          P3             P4
```

**[AM/AM]**

```
        ┌────────────┐
        │            │
        ▼    ╔════╗  │
 P1 ──►(+)──►║ M1 ╟──┴───────────────────┐
             ╚════╝                      │
                                         │
                                         │
                                         │
             ╔════╗            ╔════╗    ▼
 P2 ─────────║ C1 ╟─────►(+)──►║ M2 ╟──►(+)──► OUT
             ╚════╝       ▲    ╚════╝    ▲
                          │              │
                                         │
                          P3             │
             ╔════╗                      │
 P4 ────────►║ C2 ╟──────────────────────┘
             ╚════╝
```

### (*) REMARK ABOUT 4OP CONNECTiONS FM/AM AND AM/FM

Please note, that since order of 4OP channels is (hardware-wise) ²⁄ and ¹⁄,  
these two instrument connections are swapped.  
The preview diagrams in the Instrument Editor window show actual order,  
but here this information is kept in conformity with the official  
Yamaha YMF262 data specification to prevent further confusion.

### Tremolo (Amplitude modulation)

When set, turns tremolo (volume vibrato) ON for the corresponding slot.  
The repetition rate is 3.7Hz, the depth is optional (1dB/4.8dB).

### Vibrato

When set, turns frequency vibrato ON for the corresponding slot.  
The repetition rate is 6.1Hz, the depth is optional (7%/14%).

### Key scale rate (KSR)

When set, makes the sound shorter at higher frequencies.  
With normal musical instruments, the attack and decay rate becomes faster  
as the pitch increases. The key scale rate controls simulation of  
this effect. An offset (rof) is added to the individual attack, decay,  
and release rates depending on the following formula:

actual\_rate = (rate × 4) + rof

The "rof" values for corresponding "rate" value and KSR state are shown  
in the following table:

| **%rate%** | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | A | B | C | D | E | F |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| **[OFF]** | 0 | 0 | 0 | 0 | 1 | 1 | 1 | 1 | 2 | 2 | 2 | 2 | 3 | 3 | 3 | 3 |
| **[ON]** | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | A | B | C | D | E | F |

### Sustain (Envelope generator type)

When set, the sustain level of the voice is maintained until released.  
When clear, the sound begins to decay immediately after hitting  
the sustain phase.

```
[OFF]                /\  DR
                   /    \
                 /     ...\...SL
           AR  /            \
             /                \  RR
           /                    \
      _ _/                        \_ _ _
         .
         :
         ┌────────────────────────────────────────
      ───┘     KEY ON

[ON]                 /\  DR
                   /    \      SL
                 /        \ _ _ _ _ _
           AR  /                      \
             /                       :  \  RR
           /                         :    \
      _ _/                           :      \_ _ _
         .                           :
         :                           :
         ┌───────────────────────────┐  KEY OFF
      ───┘     KEY ON                └────────────
```

### Frequency data multiplier

Sets the multiplier for the frequency data specified by block and  
F-number. This multiplier is applied to the FM carrier or modulation  
frequencies. The multiplication factor and corresonding harmonic types are  
shown in the following table:

| **MULT.** | **×** | **HARMONiC** |
|---|---|---|
| **[0]** | 0.5 | 1 octave below |
| **[1]** | 1 | at the voice's specified frequency |
| **[2]** | 2 | 1 octave above |
| **[3]** | 3 | 1 octave and a 5th above |
| **[4]** | 4 | 2 octaves above |
| **[5]** | 5 | 2 octaves and a Major 3rd above |
| **[6]** | 6 | 2 octaves and a 5th above |
| **[7]** | 7 | 2 octaves and a Minor 7th above |
| **[8]** | 8 | 3 octaves above |
| **[9]** | 9 | 3 octaves and a Major 2nd above |
| **[A]** | 10 | 3 octaves and a Major 3rd above |
| **[B]** | 10 | ... |
| **[C]** | 12 | 3 octaves and a 5th above |
| **[D]** | 12 | ... |
| **[E]** | 15 | 3 octaves and a Major 7th above |
| **[F]** | 15 | ... |

<a id="effects_page1"></a>

## SUPPORTED EFFECTS

| Cmd | Effect | Parameter | Range | |
|---|---|---|---|---|
| `0xy` | ARPEGGiO | xy=1st\_tone\|2nd\_tone | [1-F] |  |
| `1xx` | FREQUENCY SLiDE UP | xx=speed\_of\_slide | [1-FF] |  |
| `2xx` | FREQUENCY SLiDE DOWN | xx=speed\_of\_slide | [1-FF] |  |
| `3xx` | TONE PORTAMENTO | xx=speed\_of\_slide | [1-FF] | `C` |
| `4xy` | ViBRATO | xy=speed\|depth | [1-F] | `C` |
| `5xy` | `3xx` & VOLUME SLiDE | xy=up\_speed\|down\_speed | [1-F] | `C` |
| `6xy` | `4xy` & VOLUME SLiDE | xy=up\_speed\|down\_speed | [1-F] | `C` |
| `7xx` | FiNE FREQUENCY SLiDE UP | xx=speed\_of\_slide | [1-FF] |  |
| `8xx` | FiNE FREQUENCY SLiDE DOWN | xx=speed\_of\_slide | [1-FF] |  |
| `9xx` | SET MODULATOR VOLUME | xx=volume\_level | [0-3F] |  |
| `Axy` | VOLUME SLiDE | xy=up\_speed\|down\_speed | [1-F] |  |
| `Bxx` | POSiTiON JUMP | xx=position\_in\_order | [0-7F] |  |
| `Cxx` | SET iNSTRUMENT VOLUME | xx=volume\_level | [0-3F] |  |
| `Dxx` | PATTERN BREAK | xx=line\_in\_next\_pattern | [0-FF] |  |
| `Exx` | SET TEMPO | xx=bpm\_in\_Hz | [1-FF] |  |
| `Fxx` | SET SPEED | xx=frames\_per\_row | [1-FF] |  |
| `Gxy` | `3xx` & FiNE VOLUME SLiDE | xy=up\_speed\|down\_speed | [1-F] | `C` |
| `Hxy` | `4xy` & FiNE VOLUME SLiDE | xy=up\_speed\|down\_speed | [1-F] | `C` |
| `Ixx` | SET CARRiER VOLUME | xx=volume\_level | [0-3F] |  |
| `Jxy` | SET WAVEFORM | xy=carrier\|modulator | [0-7,F=NiL] |  |

<a id="effects_page2"></a>

| Cmd | Effect | Parameter | Range | |
|---|---|---|---|---|
| `Kxy` | FiNE VOLUME SLiDE | xy=up\_speed\|down\_speed | [1-F] |  |
| `Lxx` | RETRiG NOTE | xx=interval | [1-FF] |  |
| `Mxy` | TREMOLO | xy=speed\|depth | [1-F] | `C` |
| `Nxy` | TREMOR | xy=on\_time\|off\_time | [1-F] |  |
| `Oxy` | `0xy` & VOLUME SLiDE | xy=up\_speed\|down\_speed | [1-F] | `C` |
| `Pxy` | `0xy` & FiNE VOLUME SLiDE | xy=up\_speed\|down\_speed | [1-F] | `C` |
| `Qxy` | MULTi RETRiG NOTE | xy=interval\|vol\_change | [1-F] |  |

`Qxy` の y (vol\_change):

| y | Volume change | y | Volume change |
|---|---|---|---|
| `0` | None | `8` | Unused |
| `1` | -1 | `9` | +1 |
| `2` | -2 | `A` | +2 |
| `3` | -4 | `B` | +4 |
| `4` | -8 | `C` | +8 |
| `5` | -16 | `D` | +16 |
| `6` | ×2/3 | `E` | ×3/2 |
| `7` | ×1/2 | `F` | ×2 |

| Cmd | Effect | Parameter | Range | |
|---|---|---|---|---|
| `Rxy` | `1xx` & VOLUME SLiDE | xy=up\_speed\|down\_speed | [1-F] | `C` |
| `Sxy` | `2xx` & VOLUME SLiDE | xy=up\_speed\|down\_speed | [1-F] | `C` |
| `Txy` | `7xx` & VOLUME SLiDE | xy=up\_speed\|down\_speed | [1-F] | `C` |
| `Uxy` | `8xx` & VOLUME SLiDE | xy=up\_speed\|down\_speed | [1-F] | `C` |
| `Vxy` | `1xx` & FiNE VOLUME SLiDE | xy=up\_speed\|down\_speed | [1-F] | `C` |
| `Wxy` | `2xx` & FiNE VOLUME SLiDE | xy=up\_speed\|down\_speed | [1-F] | `C` |
| `Xxy` | `7xx` & FiNE VOLUME SLiDE | xy=up\_speed\|down\_speed | [1-F] | `C` |
| `Yxy` | `8xx` & FiNE VOLUME SLiDE | xy=up\_speed\|down\_speed | [1-F] | `C` |

<a id="effects_page3"></a>

| Cmd | Effect | Parameter | Range | |
|---|---|---|---|---|
| `Z0x` | SET TREMOLO DEPTH | x=1dB/4.8dB | [0-1] | `I` |
| `Z1x` | SET ViBRATO DEPTH | x=7%/14% | [0-1] | `I` |
| `Z2x` | SET ATTACK RATE (MOD.) | x=attack\_rate | [0-F] | `I` |
| `Z3x` | SET DECAY RATE (MOD.) | x=decay\_rate | [0-F] | `I` |
| `Z4x` | SET SUSTAiN LEVEL (MOD.) | x=sustain\_level | [0-F] | `I` |
| `Z5x` | SET RELEASE RATE (MOD.) | x=release\_rate | [0-F] | `I` |
| `Z6x` | SET ATTACK RATE (CAR.) | x=attack\_rate | [0-F] | `I` |
| `Z7x` | SET DECAY RATE (CAR.) | x=decay\_rate | [0-F] | `I` |
| `Z8x` | SET SUSTAiN LEVEL (CAR.) | x=sustain\_level | [0-F] | `I` |
| `Z9x` | SET RELEASE RATE (CAR.) | x=release\_rate | [0-F] | `I` |
| `ZAx` | SET FEEDBACK STRENGTH | x=feedback\_strength | [0-7] | `I` |
| `ZBx` | SET PANNiNG POSiTiON | x=panning\_position | [0-2] | `I` |
| `ZCx` | PATTERN LOOP | x=parameter | [0-F] | |
| `ZDx` | RECURSiVE PATTERN LOOP | x=parameter | [0-F] | |
| `ZEx` | EXTENDED COMMANDS (1) | | | |
| `ZFx` | EXTENDED COMMANDS (2) | | | |

**`ZB?`**

| ? | Meaning |
|---|---|
| `0` | Center |
| `1` | Left |
| `2` | Right |

**`ZC?` / `ZD?`**

| ? | Meaning |
|---|---|
| `0` | Set loopback point |
| x | Loop `x` times |

<a id="effects_page4"></a>

**`ZE?`**

| ? | Meaning |
|---|---|
| `0`/`1` | DiSABLE/ENABLE MACRO KEY-OFF LOOP |
| `2`/`3` | DiSABLE/ENABLE RESTART ENVELOPE WiTH TONE PORTAMENTO |
| `4` | PERFORM RESTART ENVELOPE |
| `5`/`6` | DiSABLE/ENABLE 4OP TRACK VOLUME LOCK |
| `7` | PERFORM `BPM SLiDE` USiNG `>xx` AND `<xx` COMMANDS |

**`ZF?`**

| ? | Meaning |
|---|---|
| `0` | RELEASE SUSTAiNiNG SOUND |
| `1` | RESET iNSTRUMENT VOLUME |
| `2`/`3` | LOCK/UNLOCK TRACK VOLUME |
| `4`/`5` | LOCK/UNLOCK VOLUME PEAK |
| `6` | TOGGLE MODULATOR VOLUME SLiDES |
| `7` | TOGGLE CARRiER VOLUME SLiDES |
| `8` | TOGGLE DEFAULT VOLUME SLiDES |
| `9`/`A` | LOCK/UNLOCK TRACK PANNiNG |
| `B` | ViBRATO OFF |
| `C` | TREMOLO OFF |
| `D` | FORCE FiNE ViBRATO<br>FORCE FiNE GLOBAL FREQ. SLiDE |
| `E` | FORCE FiNE TREMOLO<br>FORCE EXTRA FiNE GLOBAL FREQ. SLiDE |
| `F` | FORCE NO RESTART FOR MACRO TABLES |

<a id="effects_page5"></a>

| Cmd | Effect | Parameter | Range | |
|---|---|---|---|---|
| `#0x` | SET CONNECTiON TYPE | x=FM/AM | [0-1] | `I` |
| `#1x` | SET MULTiPLiER (MOD.) | x=multiplier | [0-F] | `I` |
| `#2x` | SET KSL (MOD.) | x=scaling\_level | [0-3] | `I` |
| `#3x` | SET TREMOLO (MOD.) | x=off/on | [0-1] | `I` |
| `#4x` | SET ViBRATO (MOD.) | x=off/on | [0-1] | `I` |
| `#5x` | SET KSR (MOD.) | x=off/on | [0-1] | `I` |
| `#6x` | SET SUSTAiN (MOD.) | x=off/on | [0-1] | `I` |
| `#7x` | SET MULTiPLiER (CAR.) | x=multiplier | [0-F] | `I` |
| `#8x` | SET KSL (CAR.) | x=scaling\_level | [0-3] | `I` |
| `#9x` | SET TREMOLO (CAR.) | x=off/on | [0-1] | `I` |
| `#Ax` | SET ViBRATO (CAR.) | x=off/on | [0-1] | `I` |
| `#Bx` | SET KSR (CAR.) | x=off/on | [0-1] | `I` |
| `#Cx` | SET SUSTAiN (CAR.) | x=off/on | [0-1] | `I` |

<a id="effects_page6"></a>

| Cmd | Effect | Parameter | Range | |
|---|---|---|---|---|
| `&0x` | PATTERN DELAY (FRAMES) | x=interval | [1-F] | |
| `&1x` | PATTERN DELAY (ROWS) | x=interval | [1-F] | |
| `&2x` | NOTE DELAY | x=interval | [1-F] | |
| `&3x` | NOTE CUT | x=interval | [1-F] | |
| `&4x` | FiNE-TUNE UP | x=freq\_shift | [1-F] | |
| `&5x` | FiNE-TUNE DOWN | x=freq\_shift | [1-F] | |
| `&6x` | GLOBAL VOLUME SLiDE UP | x=speed\_of\_slide | [1-F] | |
| `&7x` | GLOBAL VOLUME SLiDE DOWN | x=speed\_of\_slide | [1-F] | |
| `&8x` | FiNE `&6x` | x=speed\_of\_slide | [1-F] | |
| `&9x` | FiNE `&7x` | x=speed\_of\_slide | [1-F] | |
| `&Ax` | EXTRA FiNE `&6x` | x=speed\_of\_slide | [1-F] | |
| `&Bx` | EXTRA FiNE `&7x` | x=speed\_of\_slide | [1-F] | |
| `&Cx` | EXTRA FiNE VSLiDE UP | x=speed\_of\_slide | [1-F] | |
| `&Dx` | EXTRA FiNE VSLiDE DOWN | x=speed\_of\_slide | [1-F] | |
| `&Ex` | EXTRA FiNE FSLiDE UP | x=speed\_of\_slide | [1-F] | |
| `&Fx` | EXTRA FiNE FSLiDE DOWN | x=speed\_of\_slide | [1-F] | |

<a id="effects_page7"></a>

| Cmd | Effect | Parameter | Range | |
|---|---|---|---|---|
| `$xy` | EXTRA FiNE ARPEGGiO | xy=1st\_tone\|2nd\_tone | [1-F] |  |
| `~xy` | EXTRA FiNE ViBRATO | xy=speed\|depth | [1-F] | `C` |
| `^xy` | EXTRA FiNE TREMOLO | xy=speed\|depth | [1-F] | `C` |
| `!xx` | SWAP ARPEGGiO TABLE | xx=table\_number | [0-FF] |  |
| `@xx` | SWAP ViBRATO TABLE | xx=table\_number | [0-FF] |  |
| `=xx` | FORCE iNSTRUMENT VOLUME | xx=volume\_level | [0-3F] |  |
| `%xx` | SET GLOBAL VOLUME | xx=volume\_level | [0-3F] |  |
| `>xx` | GLOBAL FREQ. SLiDE UP | xx=speed\_of\_slide | [1-FF] |  |
| `<xx` | GLOBAL FREQ. SLiDE DOWN | xx=speed\_of\_slide | [1-FF] |  |
| `` `xx `` | SET CUSTOM SPEED TABLE | xx=parameter | [0-FF] |  |

**`` `?? ``**

| ?? | Meaning |
|---|---|
| `00` | Reset default speed table |
| `01`-`FF` | Calculate custom speed table with parameters **table size**, **maximum value** and **processing speed factor** |

| PARAMETER | **SiZE** | **MAX.** | **FACTOR** |
|---|---|---|---|
| **[01]**..**[EF]** | 32 | `01`..`EF` | ×1 |
| **[F0]**..**[F3]** | `32` | FF | `×1`..`×4` |
| **[F4]**..**[F7]** | `64` | FF | `×1`..`×4` |
| **[F8]**..**[FB]** | `128` | FF | `×1`..`×4` |
| **[FC]**..**[FF]** | `256` | FF | `×1`..`×4` |

**FURTHER NOTES:**

1. Effects marked as `C` in the above list of commands can be continued
   in subsequent lines by setting the parameter value to 0.

2. Effects marked as `I` in the above list of commands do apply
   to currently played instrument only.

For detailed information on effect commands, see the `adtrack2.doc` file.

```
                                ─┬─       ──
                           /┤DLiB│R/┤CK3R ││ SDL
                            │       │     ──
                                      2.3.58
```

**Get the latest version from:**

http://www.adlibtracker.net

**Credits:**

Florian Klaempfl and others [Free Pascal Compiler 2.6.4]  
Simple DirectMedia Layer [SDL 1.2.15]  
Daniel F. Moisset [SDL4Freepascal-1.2.0.0]  
Alexey Khokholov [NukedOPL3 1.6]  
Haruhiko Okomura & Haruyasu Yoshizaki [LZH algorithm]  
Markus Oberhumer, Laszlo Molnar & John Reiser [UPX 3.91w]

**Honest 'thank you' to the following people:**

Slawomir Bubel (Malfunction/Altair), Daniel Illgen (insane/Altair),  
Mikkel Hastrup (encore), Florian Jung (Windfisch), Dmitry Smagin,  
David Cohen (Diode Milliampere), Nick Balega,  
Cecill Etheredge (ijsf), Sven Renner (NeuralNET),  
Tyler Montbriand (Corona688), Janwillem Jagersma, PissMasterPlus,  
and Mr. Maan M. Hamze :-)

**Greetz fly to the following people:**

Dragan Espenschied (drx/Bodenstandig 2000), Carl Peczynski (OxygenStar),  
Hubert Lamontagne (Madbrain), Jason Karl Warren (Televicious), kvee,  
Vojta Nedved (nula), and all members of AT2 user group on Facebook
