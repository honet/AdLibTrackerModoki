// ---------------------------------------------------------------------------
// マクロエディタ (adt2ext4.pas の MACRO_EDITOR とその補助処理)
// ---------------------------------------------------------------------------
//
// MainScreen.cpp の無名名前空間の中で、InstEdit.inl の後に #include する。
// Ctrl+Q (FM レジスタのマクロ) と Ctrl+G (アルペジオ / ビブラートのマクロ) で開く。

// typcons1.inc (adtrack2.ini の macro editor window)
constexpr uint8_t macro_background = 0x40;
constexpr uint8_t macro_topic = 0x0c;
constexpr uint8_t macro_text = 0x0d;
constexpr uint8_t macro_hi_text = 0x0e;
constexpr uint8_t macro_text_dis = 0x06;
constexpr uint8_t macro_text_loop = 0x0c;
constexpr uint8_t macro_text_keyoff = 0x03;
constexpr uint8_t macro_current_bckg = 0x50;
constexpr uint8_t macro_current = 0x0d;
constexpr uint8_t macro_current_dis = 0x00;
constexpr uint8_t macro_current_loop = 0x0c;
constexpr uint8_t macro_current_koff = 0x0b;
constexpr uint8_t macro_input_bckg = 0x70;
constexpr uint8_t macro_input = 0x00;
constexpr uint8_t macro_def_bckg = 0xc0;
constexpr uint8_t macro_def = 0x00;
constexpr uint8_t macro_scrbar_bckg = 0x70;
constexpr uint8_t macro_scrbar_text = 0x00;
constexpr uint8_t macro_scrbar_mark = 0x0f;
constexpr uint8_t macro_hint = 0x07;
constexpr uint8_t macro_context = 0x0e;
constexpr uint8_t macro_context_dis = 0x07;
constexpr uint8_t macro_border = 0x0f;

// adt2unit.pas
constexpr uint8_t MACRO_NOTE_RETRIG_FLAG = 0x80;
constexpr uint8_t MACRO_ENVELOPE_RESTART_FLAG = 0x40;
constexpr uint8_t MACRO_ZERO_FREQ_FLAG = 0x20;

// typcons1.inc: FM レジスタ表の各列の、表示文字列 (_fmreg_str) での位置。[1..35] は [0..34]
const uint8_t pos5[35] = { 1, 3, 5, 7, 9, 11, 12, 14, 16, 18, 19, 20, 21, 23, 25, 27, 29, 31, 33, 34,
                           36, 38, 40, 41, 42, 43, 45, 47, 49, 50, 51, 52, 54, 56, 57 };

// typcons2.inc。[1..20+35] は [0..54]
const char* const macro_table_hint_str[55] = {
    "Length of FM-register definition macro-table {1-FF} (0 means no macros)",
    "Loop cycle starting position {1-FF} (0 means no loop)",
    "Length of loop cycle {1-FF} (0 means no loop)",
    "Key-Off jump position {1-FF} (0 means no jump)",
    "Arpeggio table number {1-FF} (0 means no arpeggio)",
    "Vibrato table number {1-FF} (0 means no vibrato)",
    "",
    "Length of arpeggio macro-table {1-FF} (0 means no macros)",
    "Speed of arpeggio in macro-table {1-FF} (0 means no arpeggio)",
    "Loop cycle starting position {1-FF} (0 means no loop)",
    "Length of loop cycle {1-FF} (0 means no loop)",
    "Key-Off jump position {1-FF} (0 means no jump)",
    "Number of half-tones to add [1-96] or fixed-note [C,C-,C#,C1,C-1,C#1,...]",
    "Length of vibrato macro-table {1-FF} (0 means no macros)",
    "Speed of vibrato in macro-table {1-FF} (0 means no vibrato)",
    "Delay before starting vibrato in macro-table {1-FF} (0 means no delay)",
    "Loop cycle starting position {1-FF} (0 means no loop)",
    "Length of loop cycle {1-FF} (0 means no loop)",
    "Key-Off jump position {1-FF} (0 means no jump)",
    "Frequency to add {1..7F} or subtract {-7F..-1}",

    "Attack rate [modulator] {0-F}",
    "Decay rate [modulator] {0-F}",
    "Sustain level [modulator] {0-F}",
    "Release rate [modulator] {0-F}",
    "Waveform type [modulator] {0-7}",
    "Output level [modulator] {0-3F}",
    "Output level [modulator] {0-3F}",
    "Key scaling level [modulator] {0-3}",
    "Multiplier [modulator] {0-F}",
    "Amplitude modulation (tremolo) [modulator] {on/off}",
    "Vibrato [modulator] {on/off}",
    "Key scale rate [modulator] {on/off}",
    "Sustain [modulator] {on/off}",
    "Attack rate [carrier] {0-F}",
    "Decay rate [carrier] {0-F}",
    "Sustain level [carrier] {0-F}",
    "Release rate [carrier] {0-F}",
    "Waveform type [carrier] {0-7}",
    "Output level [carrier] {0-3F}",
    "Output level [carrier] {0-3F}",
    "Key scaling level [carrier] {0-3}",
    "Multiplier [carrier] {0-F}",
    "Amplitude modulation (tremolo) [carrier] {on/off}",
    "Vibrato [carrier] {on/off}",
    "Key scale rate [carrier] {on/off}",
    "Sustain [carrier] {on/off}",
    "Connection type {0-1} (0=FM,1=AM)",
    "Feedback {0-7}",
    "Frequency slide {-FFF..+FFF}",
    "Frequency slide {-FFF..+FFF}",
    "Frequency slide {-FFF..+FFF}",
    "Frequency slide {-FFF..+FFF}",
    "Panning {Left/Center/Right}",
    "Duration {1-FF} (0 means skip)",
    "Duration {1-FF} (0 means skip)",
};

// Alt+C のメニュー (typcons2.inc の copy_menu_str4。13, 14 番目はカーソル位置で入れ替える)
const char* const copy_menu_str4[15] = {
    " COPY ORDER DEF.",
    " COPY ORDER TABLE",
    " COPY iNSTRUMENT",
    " COPY iNSTRUMENT BANK",
    " COPY NOTE",
    " COPY iNSTRUMENT DEF.",
    " COPY 1ST EFFECT",
    " COPY 2ND EFFECT",
    " COPY LiNE",
    " COPY TRACK",
    " COPY PATTERN",
    " COPY MARKED BLOCK",
    " COPY M~A~CRO-TABLE LiNE",
    " COPY MA~C~RO-TABLE COLUMN",
    " COPY ~M~ACRO-TABLE",
};

const char* const copy_macro_str[4] = {
    " COPY MACRO-TABLE LiNE",
    " COPY M~A~CRO-TABLE LiNE",
    " COPY MACRO-TABLE COLUMN",
    " COPY MA~C~RO-TABLE COLUMN",
};

uint8_t copypos4 = 1;

// 編集しているアルペジオ / ビブラートの表の番号 (adt2ext5.pas)
uint8_t ptr_arpeggio_table = 0;
uint8_t ptr_vibrato_table = 0;

// Ctrl+G のマクロエディタで編集する表の番号 (adt2ext4.pas)
uint8_t arpvib_arpeggio_table = 1;
uint8_t arpvib_vibrato_table = 1;
bool _arp_vib_mode = false;

// マクロエディタの F3 で、A2W のアルペジオ / ビブラートのマクロを読み込むか (adt2ext4.pas)。
// オリジナルは A2W を読み込んだときにだけ FALSE に戻す
bool _arp_vib_loader = false;

// アルペジオ / ビブラートのマクロの窓で、表を選んでいるか (adt2ext5.pas)
bool arp_tab_selected = false;
bool vib_tab_selected = false;

namespace macroedit
{
    constexpr uint16_t EMPTY_FIELD = 0xffff;
    constexpr uint16_t COMMON_FLAG = 0x8000;

    constexpr uint8_t flag_FMREG = 1;
    constexpr uint8_t flag_ARPEGGIO = 2;
    constexpr uint8_t flag_VIBRATO = 4;

    // 閉じたときのカーソル位置 ([FALSE]: Ctrl+Q、[TRUE]: Ctrl+G)
    uint8_t _macro_editor__pos[2] = { 1, 8 };
    uint8_t _macro_editor__fmreg_hpos[2] = { 1, 1 };
    uint8_t _macro_editor__fmreg_page[2] = { 1, 1 };
    uint8_t _macro_editor__fmreg_left_margin[2] = { 1, 1 };
    uint8_t _macro_editor__fmreg_cursor_pos[2] = { 1, 1 };
    uint8_t _macro_editor__arpeggio_page[2] = { 1, 1 };
    uint8_t _macro_editor__vibrato_hpos[2] = { 1, 1 };
    uint8_t _macro_editor__vibrato_page[2] = { 1, 1 };

    // 試聴の表示 (PREViEW / LOOP)
    uint8_t _pip_xloc = 1;
    uint8_t _pip_yloc = 1;
    tSCREEN_MEM_PTR _pip_dest = nullptr;
    bool _pip_loop = false;

    // 試聴で鳴らすオペレータ ([1..4]。楽器エディタとは別)
    bool _operator_enabled[5] = { false, true, true, true, true };

    // MACRO_EDITOR のローカル変数
    uint8_t window_area_inc_x = 0;
    uint8_t window_area_inc_y = 0;
    bool arp_vib_mode = false;
    uint8_t instr = 1;
    uint8_t xstart = 0, ystart = 0;
    uint8_t fmreg_cursor_pos = 1, fmreg_left_margin = 1, fmreg_hpos = 1;
    uint8_t pos = 1, vibrato_hpos = 1;
    uint8_t fmreg_page = 1, arpeggio_page = 1, vibrato_page = 1;
    uint8_t attr[21];                // [1..20]
    uint8_t attr2[6];                // [1..5]
    std::string frame_type[4];       // [1..3]
    std::string fmreg_str;           // 最後に表示した行の _fmreg_str (カーソルの横移動で区切りの位置を見る)
    bool call_pickup_proc = false, call_pickup_proc2 = false;
    uint8_t _source_ins = 1, _source_ins2 = 1;

    // songdata.macro_table[idx]。オリジナルは表の番号 0 も範囲外のまま読む (直前の instr_macros[255] の末尾)
    MacroTable& mt(uint8_t idx)
    {
        uint8_t* base = reinterpret_cast<uint8_t*>(&songdata().macro_table[0]);
        return *reinterpret_cast<MacroTable*>(base + (static_cast<ptrdiff_t>(idx) - 1) * static_cast<ptrdiff_t>(sizeof(MacroTable)));
    }
    RegisterTable& im() { return at1(songdata().instr_macros, instr); }
    ArpeggioTable& arp() { return mt(ptr_arpeggio_table).arpeggio; }
    VibratoTable& vib() { return mt(ptr_vibrato_table).vibrato; }
    uint8_t& dis_col(int idx) { return at1(songdata().dis_fmreg_col, instr)[idx]; }

    bool in_range(int value, int lo, int hi) { return value >= lo && value <= hi; }
    int min0(int number, int flag) { return number > 0 ? number : flag; }

    const char _panning[3] = { '\xF1', '<', '>' };
    const char _hex[] = "0123456789ABCDEF";

    void _preview_indic_proc(uint8_t state)
    {
        switch (state) {
        case 0: ShowStr(_pip_dest, _pip_xloc, _pip_yloc, "\x10 PREViEW \x11", macro_background + macro_text_dis); break;
        case 1: ShowStr(_pip_dest, _pip_xloc, _pip_yloc, "\x10 PREViEW \x11", macro_background + macro_text); break;
        case 2: ShowStr(_pip_dest, _pip_xloc, _pip_yloc, "\x10 PREViEW \x11", B(~(macro_background + macro_text))); break;
        }

        if (_pip_loop && state != 0) ShowStr(_pip_dest, _pip_xloc, B(_pip_yloc - 1), "\x0C LOOP", macro_background + macro_text);
        else ShowStr(_pip_dest, _pip_xloc, B(_pip_yloc - 1), "\x0C LOOP", macro_background + macro_text_dis);
    }

    // 表の行の色 (下位: 通常、上位: カーソル行)
    template <class T>
    uint16_t def_attr(const T& t, uint8_t page)
    {
        uint8_t a, a2;
        if (page <= t.length) {
            if (page >= t.loop_begin && page <= t.loop_begin + (t.loop_length - 1) && t.loop_begin > 0 && t.loop_length > 0) {
                a = macro_background + macro_text_loop;
                a2 = macro_current_bckg + macro_current_loop;
            } else if (page >= t.keyoff_pos && t.keyoff_pos > 0) {
                a = macro_background + macro_text_keyoff;
                a2 = macro_current_bckg + macro_current_koff;
            } else {
                a = macro_background + macro_text;
                a2 = macro_current_bckg + macro_current;
            }
        } else {
            a = macro_background + macro_text_dis;
            a2 = macro_current_bckg + macro_current_dis;
        }
        return static_cast<uint16_t>(a + (a2 << 8));
    }

    uint16_t fmreg_def_attr(uint8_t page) { return def_attr(im(), page); }
    uint16_t arpeggio_def_attr(uint8_t page) { return def_attr(arp(), page); }
    uint16_t vibrato_def_attr(uint8_t page) { return def_attr(vib(), page); }

    // FM レジスタ表の 1 行 d の表示文字列 (A2W の一覧でも使う)
    std::string fmreg_def_str(const RegisterTableDef& d)
    {
        const FmInstData& f = d.fm_data;
        std::string s;
        const auto hx = [](int v) { return std::string(1, _hex[v & 15]); };
        const auto flags = [&s](uint8_t v) {
            s += (v >> 7) == 0 ? '\xFA' : 'T';
            s += ((v >> 6) & 1) == 0 ? '\xFA' : 'V';
            s += ((v >> 4) & 1) == 0 ? '\xFA' : 'K';
            s += ((v >> 5) & 1) == 0 ? "\xFA " : "S ";
        };

        s = hx(f.ATTCK_DEC_modulator >> 4) + " " + hx(f.ATTCK_DEC_modulator) + " " + hx(f.SUSTN_REL_modulator >> 4) + " " +
            hx(f.SUSTN_REL_modulator) + " " + hx(f.WAVEFORM_modulator & 7) + " " + byte2hex(f.KSL_VOLUM_modulator & 0x3f) + " " +
            hx(f.KSL_VOLUM_modulator >> 6) + " " + hx(f.AM_VIB_EG_modulator) + " ";
        flags(f.AM_VIB_EG_modulator);
        s += hx(f.ATTCK_DEC_carrier >> 4) + " " + hx(f.ATTCK_DEC_carrier) + " " + hx(f.SUSTN_REL_carrier >> 4) + " " +
             hx(f.SUSTN_REL_carrier) + " " + hx(f.WAVEFORM_carrier & 7) + " " + byte2hex(f.KSL_VOLUM_carrier & 0x3f) + " " +
             hx(f.KSL_VOLUM_carrier >> 6) + " " + hx(f.AM_VIB_EG_carrier) + " ";
        flags(f.AM_VIB_EG_carrier);
        s += hx(f.FEEDBACK_FM & 1) + " ";
        s += hx((f.FEEDBACK_FM >> 1) & 7) + " ";

        s += (d.freq_slide < 0 ? "-" : "+") + ExpStrL(Num2str(std::abs(d.freq_slide), 16), 3, '0') + " ";
        s += std::string(1, _panning[d.panning <= 2 ? d.panning : 0]) + " " + byte2hex(d.duration);
        return s;
    }

    // FM レジスタ表の 1 行の表示文字列
    std::string _fmreg_str(uint8_t page) { return fmreg_def_str(im().data[page == 0 ? 0 : page - 1]); }

    // 列 fmreg_col (1..35) が無効 (Ctrl+BkSpc など) か
    bool _dis_fmreg_col(uint8_t fmreg_col)
    {
        bool result = false;
        if (in_range(fmreg_col, 1, 5)) result = dis_col(fmreg_col - 1) != 0;
        else if (in_range(fmreg_col, 6, 7)) result = dis_col(5) != 0;
        else if (in_range(fmreg_col, 8, 18)) result = dis_col(fmreg_col - 2) != 0;
        else if (in_range(fmreg_col, 19, 20)) result = dis_col(17) != 0;
        else if (in_range(fmreg_col, 21, 28)) result = dis_col(fmreg_col - 3) != 0;
        else if (in_range(fmreg_col, 29, 32)) result = dis_col(26) != 0;
        else if (fmreg_col == 33) result = dis_col(27) != 0;

        if (in_range(fmreg_col, 14, 28) && in_range(at1(songdata().instr_data, current_inst).perc_voice, 2, 5)) result = true;
        return result;
    }

    // FM レジスタ表の枠の上辺 (無効な列の印)
    std::string _str1(char def_chr)
    {
        const auto oo = [def_chr](int idx) { return dis_col(idx) != 0 ? '\xFE' : def_chr; };
        std::string s;
        for (int temp = 0; temp <= 4; ++temp) {
            s += oo(temp);
            s += def_chr;
        }
        s += oo(5); s += oo(5); s += def_chr;
        s += oo(6); s += def_chr; s += oo(7); s += def_chr;
        s += oo(8); s += oo(9); s += oo(10); s += oo(11); s += def_chr;
        for (int temp = 12; temp <= 16; ++temp) {
            s += oo(temp);
            s += def_chr;
        }
        s += oo(17); s += oo(17); s += def_chr;
        s += oo(18); s += def_chr; s += oo(19); s += def_chr;
        s += oo(20); s += oo(21); s += oo(22); s += oo(23); s += def_chr;
        for (int temp = 24; temp <= 25; ++temp) {
            s += oo(temp);
            s += def_chr;
        }
        s += oo(26); s += oo(26); s += oo(26); s += oo(26); s += def_chr;
        s += oo(27);
        return s;
    }

    // str の先頭から、'`' を数えずに len 文字分
    std::string _str2(const std::string& str, uint8_t len)
    {
        std::string result;
        size_t idx = 0;
        int len2 = 0;
        while (idx < str.size() && len > len2) {
            if (str[idx] != '`') ++len2;
            result += str[idx];
            ++idx;
        }
        return result;
    }

    void fmreg_page_refresh(uint8_t xpos, uint8_t ypos, uint16_t page)
    {
        const uint8_t a = LO(fmreg_def_attr(B(page & 0x0fff)));
        const uint8_t a2 = HI(fmreg_def_attr(B(page & 0x0fff)));
        fmreg_str = _fmreg_str(B(page & 0x0fff));
        std::string fmreg_str2 = fmreg_str;

        int index = 0, index2 = 0;
        for (int fmreg_col = 1; fmreg_col <= 35 - 1; ++fmreg_col) {
            if (_dis_fmreg_col(B(fmreg_col))) {
                fmreg_str2.insert(static_cast<size_t>(pos5[fmreg_col - 1] + index - 1), 1, '`');
                fmreg_str2.insert(static_cast<size_t>(pos5[fmreg_col - 1] + index + 2 - 1), 1, '`');
                if (pos5[fmreg_col - 1] < fmreg_left_margin) index2 += 2;
                index += 2;
            }
        }

        const std::string temps = Copy(fmreg_str2, fmreg_left_margin + index2,
                                       static_cast<int>(fmreg_str2.size()) - fmreg_left_margin - index2 + 1);
        const std::string dummy_str = macro_retrig_str[im().data[B(page & 0xff) == 0 ? 0 : B(page & 0xff) - 1].fm_data.FEEDBACK_FM >> 5];
        const std::string body = _str2(temps, B(31 + window_area_inc_x));
        if (!arp_vib_mode) {
            if (page != EMPTY_FIELD) {
                if ((page | COMMON_FLAG) != page) {
                    ShowC3Str(temp_screen, xpos, ypos, "~" + byte2hex(B(page)) + "~ \xB3~" + dummy_str + "~\xF6~" + body + "~",
                              macro_background + macro_text, a, macro_background + macro_text_dis);
                } else {
                    ShowC3Str(temp_screen, B(xpos - 1), ypos,
                              " ~" + byte2hex(B(page & ~COMMON_FLAG)) + "~ \xB3~" + dummy_str + "~\xF6~" + body + "~ ",
                              macro_current_bckg + macro_current, a2, macro_current_bckg + macro_current_dis);
                }
            } else {
                ShowStr(temp_screen, xpos, ypos, ExpStrL("", 36, ' '), a);
            }
        } else {
            const uint8_t dis = macro_background + macro_text_dis;
            if (page != EMPTY_FIELD) {
                if ((page | COMMON_FLAG) != page) {
                    ShowC3Str(temp_screen, xpos, ypos, byte2hex(B(page)) + " \xB3~" + dummy_str + "~\xF6~" + body + "~", dis, dis, dis);
                } else {
                    ShowC3Str(temp_screen, B(xpos - 1), ypos,
                              " " + byte2hex(B(page & ~COMMON_FLAG)) + " \xB3~" + dummy_str + "~\xF6~" + body + "~ ", dis, dis, dis);
                }
            } else {
                ShowStr(temp_screen, xpos, ypos, ExpStrL("", 36, ' '), dis);
            }
        }
    }

    // オリジナルは case に当てはまらない値のとき、前回の文字列のまま表示する
    std::string arpeggio_temps;

    void arpeggio_page_refresh(uint8_t xpos, uint8_t ypos, uint16_t page)
    {
        const uint8_t a = LO(arpeggio_def_attr(B(page & 0x0fff)));
        const uint8_t a2 = HI(arpeggio_def_attr(B(page & 0x0fff)));

        if (page != EMPTY_FIELD) {
            const uint8_t value = arp().data[(page & 0x0fff) - 1];
            if (value == 0) arpeggio_temps = "\xFA\xFA\xFA";
            else if (in_range(value, 1, 96)) arpeggio_temps = "+" + ExpStrR(Num2str(value, 10), 2, ' ');
            else if (in_range(value, 0x80, 0x80 + 12 * 8 + 1)) arpeggio_temps = note_layout[value - 0x80];

            if ((page | COMMON_FLAG) != page) {
                ShowCStr(temp_screen, xpos, ypos, "~" + byte2hex(B(page & ~COMMON_FLAG)) + "~ \xB3 ~" + arpeggio_temps + "~",
                         macro_background + macro_text, a);
            } else {
                ShowCStr(temp_screen, B(xpos - 1), ypos, " ~" + byte2hex(B(page & ~COMMON_FLAG)) + "~ \xB3 ~" + arpeggio_temps + "~ ",
                         macro_current_bckg + macro_current, a2);
            }
        } else {
            ShowStr(temp_screen, xpos, ypos, ExpStrL("", 9, ' '), a);
        }
    }

    void vibrato_page_refresh(uint8_t xpos, uint8_t ypos, uint16_t page)
    {
        const uint8_t a = LO(vibrato_def_attr(B(page & 0x0fff)));
        const uint8_t a2 = HI(vibrato_def_attr(B(page & 0x0fff)));

        if (page != EMPTY_FIELD) {
            const int8_t value = vib().data[(page & 0x0fff) - 1];
            std::string temps;
            if (value == 0) temps = "\xFA\xFA\xFA";
            else if (value < 0) temps = "-" + byte2hex(B(std::abs(value)));
            else temps = "+" + byte2hex(B(std::abs(value)));

            if ((page | COMMON_FLAG) != page) {
                ShowCStr(temp_screen, xpos, ypos, "~" + byte2hex(B(page & ~COMMON_FLAG)) + "~ \xB3 ~" + temps + "~",
                         macro_background + macro_text, a);
            } else {
                ShowCStr(temp_screen, B(xpos - 1), ypos, " ~" + byte2hex(B(page & ~COMMON_FLAG)) + "~ \xB3 ~" + temps + "~ ",
                         macro_current_bckg + macro_current, a2);
            }
        } else {
            ShowStr(temp_screen, xpos, ypos, ExpStrL("", 9, ' '), a);
        }
    }

    // 表の番号が 0 のときのアルペジオ / ビブラートの表 (arpeggio_page_refresh_alt / vibrato_page_refresh_alt)
    void arpvib_page_refresh_alt(uint8_t xpos, uint8_t ypos, uint16_t page)
    {
        const uint8_t dis = macro_background + macro_text_dis;
        if (page != EMPTY_FIELD) {
            if ((page | COMMON_FLAG) != page) {
                ShowCStr(temp_screen, xpos, ypos, byte2hex(B(page & ~COMMON_FLAG)) + " \xB3 ~\xFA\xFA\xFA~", dis, dis);
            } else {
                ShowCStr(temp_screen, B(xpos - 1), ypos, " " + byte2hex(B(page & ~COMMON_FLAG)) + " \xB3 ~\xFA\xFA\xFA~ ", dis, dis);
            }
        } else {
            ShowStr(temp_screen, xpos, ypos, ExpStrL("", 9, ' '), dis);
        }
    }

    void _show_proc(int show_proc_index, uint8_t xpos, uint8_t ypos, uint16_t page)
    {
        switch (show_proc_index) {
        case 1: fmreg_page_refresh(xpos, ypos, page); break;
        case 2: arpeggio_page_refresh(xpos, ypos, page); break;
        case 4: vibrato_page_refresh(xpos, ypos, page); break;
        case 3:
        case 5: arpvib_page_refresh_alt(xpos, ypos, page); break;
        }
    }

    // 表の、カーソル行 page を中心とした page_len 行
    void show_queue(uint8_t x, uint8_t y, uint8_t page_len, int page, int len, int show_proc_index)
    {
        const int half = page_len / 2;
        const int spos = (page - 1 < half) ? half - (page - 1) : 0;
        const int epos = (page - 1 > len - half - 1) ? (page - 1) - (len - half - 1) : 0;

        if (spos != 0) {
            for (int temp3 = 1; temp3 <= spos; ++temp3) _show_proc(show_proc_index, x, B(y + temp3), EMPTY_FIELD);
        }
        if (epos != 0) {
            for (int temp3 = page_len; temp3 >= page_len - epos + 1; --temp3) _show_proc(show_proc_index, x, B(y + temp3), EMPTY_FIELD);
        }

        for (int temp1 = 1 + spos; temp1 <= page_len - epos; ++temp1) {
            const uint16_t p = static_cast<uint16_t>(page - 1 + temp1 - half);
            if (temp1 != half + 1) _show_proc(show_proc_index, x, B(y + temp1), p);
            else _show_proc(show_proc_index, x, B(y + temp1), static_cast<uint16_t>(p + COMMON_FLAG));
        }
    }

    // グラフの縦棒 (下から上へ。neg なら上から下へ)
    std::string _gfx_bar_str(uint8_t value, bool neg)
    {
        std::string result;
        if (!neg) {
            do {
                if (value > 15) {
                    result += '\xDB';
                    value = B(value - 15);
                }
                if (value <= 15 && value != 0) result += static_cast<char>(127 + value);
            } while (!(value <= 15));
        } else {
            do {
                if (value > 15) {
                    result = '\xDB' + result;
                    value = B(value - 15);
                }
                if (value <= 15 && value != 0) result = static_cast<char>(158 - value) + result;
            } while (!(value <= 15));
        }
        return std::string(result.rbegin(), result.rend());
    }

    // FM レジスタ表の行の表示文字列 s から、列 hpos の値を読む (A2W の一覧でも使う)
    int fmreg_str_param(const std::string& s, uint8_t hpos)
    {
        const auto at = [&s](int i) { return Copy(s, i, 1); };
        int result = 0;
        switch (hpos) {
        case 1: case 2: case 3: case 4: case 5: case 8: case 9: case 14: case 15: case 16: case 17:
        case 18: case 21: case 22: case 27: case 28:
            result = Str2num(at(pos5[hpos - 1]), 16);
            break;
        case 6: case 7: result = Str2num(Copy(s, pos5[5], 2), 16); break;
        case 19: case 20: result = Str2num(Copy(s, pos5[18], 2), 16); break;
        case 34: case 35: result = Str2num(Copy(s, pos5[33], 2), 16); break;
        case 10: case 11: case 12: case 13: case 23: case 24: case 25: case 26:
            // オリジナルは #251 と比べるため、常に 0
            result = s[pos5[hpos - 1] - 1] == '\xFB' ? 1 : 0;
            break;
        case 29: case 30: case 31: case 32:
            result = Str2num(Copy(s, pos5[29], 3), 16);
            if (s[pos5[28] - 1] == '-') result = -result;
            break;
        case 33:
            switch (s[pos5[32] - 1]) {
            case '\xF1': result = 0; break;
            case '<': result = -1; break;
            case '>': result = 1; break;
            }
            break;
        }
        return result;
    }

    // FM レジスタ表の行 page、列 hpos の値
    int _fmreg_param(uint8_t page, uint8_t hpos) { return fmreg_str_param(_fmreg_str(page), hpos); }

    void refresh(uint8_t refresh_flag)
    {
        const FixedSongData& sd = songdata();
        const RegisterTable& m = im();
        const int ix = window_area_inc_x;
        const int iy = window_area_inc_y;
        const auto X = [](int x) { return B(xstart + x); };
        const auto Y = [](int y) { return B(ystart + y); };

        for (int temp = 1; temp <= 20; ++temp) {
            if (pos == temp) attr[temp] = macro_background + macro_hi_text;
            else if (in_range(temp, 1, 7) && arp_vib_mode) attr[temp] = macro_background + macro_text_dis;
            else if (in_range(temp, 8, 13) && ptr_arpeggio_table == 0) attr[temp] = macro_background + macro_text_dis;
            else if (in_range(temp, 14, 20) && ptr_vibrato_table == 0) attr[temp] = macro_background + macro_text_dis;
            else attr[temp] = macro_background + macro_text;
        }

        if (ptr_arpeggio_table != 0) {
            attr2[1] = macro_input_bckg + macro_input;
            attr2[3] = macro_background + macro_topic;
        } else {
            attr2[1] = macro_background + macro_text_dis;
            attr2[3] = macro_background + macro_text_dis;
        }
        if (ptr_vibrato_table != 0) {
            attr2[2] = macro_input_bckg + macro_input;
            attr2[4] = macro_background + macro_topic;
        } else {
            attr2[2] = macro_background + macro_text_dis;
            attr2[4] = macro_background + macro_text_dis;
        }
        attr2[5] = !arp_vib_mode ? macro_input_bckg + macro_input : macro_background + macro_text_dis;

        frame_type[1] = pos == 7 ? frame_double : frame_single;
        frame_type[2] = pos == 13 ? frame_double : frame_single;
        frame_type[3] = pos == 20 ? frame_double : frame_single;

        const uint8_t topic_attr = !arp_vib_mode ? macro_background + macro_topic : macro_background + macro_text_dis;
        ShowStr(temp_screen, X(2), Y(1), "FM-REGiSTER", topic_attr);
        ShowStr(temp_screen, X(2), Y(2), "DEFiNiTiON MACRO-TABLE", topic_attr);

        ShowStr(temp_screen, X(2), Y(3), ExpStrL("", B(78 + ix), '\xCD'), macro_background + macro_text);
        ShowStr(temp_screen, X(2), Y(10), ExpStrL("", B(78 + ix), '\xCD'), macro_background + macro_text);
        ShowStr(temp_screen, X(2), Y(22 + iy), ExpStrL("", B(78 + ix), '\xCD'), macro_background + macro_text);

        int _add_prev_size, _sub_prev_xpos_a, _sub_prev_xpos_v;
        if (!arp_vib_mode) {
            ShowCStr(temp_screen, X(2), Y(4), "LENGTH         ~" + byte2hex(m.length) + " ~", attr[1], attr2[5]);
            ShowCStr(temp_screen, X(2), Y(5), "LOOP BEGiN     ~" + byte2hex(m.loop_begin) + " ~", attr[2], attr2[5]);
            ShowCStr(temp_screen, X(2), Y(6), "LOOP LENGTH    ~" + byte2hex(m.loop_length) + " ~", attr[3], attr2[5]);
            ShowCStr(temp_screen, X(2), Y(7), "KEY-OFF        ~" + byte2hex(m.keyoff_pos) + " ~", attr[4], attr2[5]);
            ShowCStr(temp_screen, X(2), Y(8), "ARPEGGiO TABLE ~" + byte2hex(ptr_arpeggio_table) + " ~", attr[5], attr2[5]);
            ShowCStr(temp_screen, X(2), Y(9), "ViBRATO TABLE  ~" + byte2hex(ptr_vibrato_table) + " ~", attr[6], attr2[5]);
            _add_prev_size = ix / 2;
            _sub_prev_xpos_a = 0;
            _sub_prev_xpos_v = 0;
        } else {
            _add_prev_size = 0;
            _sub_prev_xpos_a = 20;
            _sub_prev_xpos_v = -3;
        }

        const ArpeggioTable& a = arp();
        const VibratoTable& v = vib();
        ShowStr(temp_screen, X(48 + ix), Y(2), "ARPEGGiO (" + byte2hex(ptr_arpeggio_table) + ")", attr2[3]);
        ShowCStr(temp_screen, X(48 + ix), Y(4), "LENGTH      ~" + byte2hex(a.length) + " ~", attr[8], attr2[1]);
        ShowCStr(temp_screen, X(48 + ix), Y(5), "SPEED       ~" + byte2hex(a.speed) + " ~", attr[9], attr2[1]);
        ShowCStr(temp_screen, X(48 + ix), Y(6), "LOOP BEGiN  ~" + byte2hex(a.loop_begin) + " ~", attr[10], attr2[1]);
        ShowCStr(temp_screen, X(48 + ix), Y(7), "LOOP LENGTH ~" + byte2hex(a.loop_length) + " ~", attr[11], attr2[1]);
        ShowCStr(temp_screen, X(48 + ix), Y(8), "KEY-OFF     ~" + byte2hex(a.keyoff_pos) + " ~", attr[12], attr2[1]);

        ShowStr(temp_screen, X(65 + ix), Y(2), "ViBRATO (" + byte2hex(ptr_vibrato_table) + ")", attr2[4]);
        ShowCStr(temp_screen, X(65 + ix), Y(4), "LENGTH      ~" + byte2hex(v.length) + " ~", attr[14], attr2[2]);
        ShowCStr(temp_screen, X(65 + ix), Y(5), "SPEED       ~" + byte2hex(v.speed) + " ~", attr[15], attr2[2]);
        ShowCStr(temp_screen, X(65 + ix), Y(6), "DELAY       ~" + byte2hex(v.delay) + " ~", attr[16], attr2[2]);
        ShowCStr(temp_screen, X(65 + ix), Y(7), "LOOP BEGiN  ~" + byte2hex(v.loop_begin) + " ~", attr[17], attr2[2]);
        ShowCStr(temp_screen, X(65 + ix), Y(8), "LOOP LENGTH ~" + byte2hex(v.loop_length) + " ~", attr[18], attr2[2]);
        ShowCStr(temp_screen, X(65 + ix), Y(9), "KEY-OFF     ~" + byte2hex(v.keyoff_pos) + " ~", attr[19], attr2[2]);

        fr_setting.update_area = false;
        fr_setting.shadow_enabled = false;
        if ((refresh_flag & flag_FMREG) == flag_FMREG) {
            Frame(temp_screen, X(2), Y(11), X(42 + _add_prev_size * 2), Y(21 + iy), attr[7], "", macro_background + macro_text,
                  frame_type[1]);
        }
        if ((refresh_flag & flag_ARPEGGIO) == flag_ARPEGGIO) {
            Frame(temp_screen, X(48 + ix), Y(11), X(59 + _add_prev_size * 2), Y(21 + iy), attr[13], "",
                  macro_background + macro_text, frame_type[2]);
        }
        if ((refresh_flag & flag_VIBRATO) == flag_VIBRATO) {
            Frame(temp_screen, X(65 + ix), Y(11), X(76 + _add_prev_size * 2), Y(21 + iy), attr[20], "",
                  macro_background + macro_text, frame_type[3]);
        }
        fr_setting.update_area = true;
        fr_setting.shadow_enabled = true;

        const uint8_t page_len = B(9 + iy);
        show_queue(X(4), Y(11), page_len, fmreg_page, 255, 1);
        if ((refresh_flag & flag_FMREG) == flag_FMREG) {
            if (!arp_vib_mode) {
                if (ix != 26) {
                    HScrollBar(temp_screen, X(29 + _add_prev_size * 2), Y(21 + iy), 13, 35, fmreg_hpos, WORD_NULL,
                               macro_scrbar_bckg + macro_scrbar_text, macro_scrbar_bckg + macro_scrbar_mark);
                }
                VScrollBar(temp_screen, X(43 + _add_prev_size * 2), Y(12), page_len, 255, fmreg_page, WORD_NULL,
                           macro_scrbar_bckg + macro_scrbar_text, macro_scrbar_bckg + macro_scrbar_mark);
            } else {
                HScrollBar(temp_screen, X(29 + _add_prev_size * 2), Y(21 + iy), 13, 35, fmreg_hpos, WORD_NULL,
                           macro_background + macro_text_dis, macro_background + macro_text_dis);
                VScrollBar(temp_screen, X(43 + _add_prev_size * 2), Y(12), page_len, 255, fmreg_page, WORD_NULL,
                           macro_background + macro_text_dis, macro_background + macro_text_dis);
            }
        }

        ShowStr(temp_screen, X(2 + 8), Y(11), Copy(_str1(pos == 7 ? '\xCD' : '\xC4'), fmreg_left_margin, 31), attr[7]);

        if ((refresh_flag & flag_ARPEGGIO) == flag_ARPEGGIO) {
            if (ptr_arpeggio_table != 0) {
                show_queue(X(50 + _add_prev_size * 2), Y(11), page_len, arpeggio_page, 255, 2);
                VScrollBar(temp_screen, X(60 + _add_prev_size * 2), Y(12), page_len, 255, arpeggio_page, WORD_NULL,
                           macro_scrbar_bckg + macro_scrbar_text, macro_scrbar_bckg + macro_scrbar_mark);
            } else {
                show_queue(X(50 + _add_prev_size * 2), Y(11), page_len, 1, 255, 3);
                VScrollBar(temp_screen, X(60 + _add_prev_size * 2), Y(12), page_len, 255, 1, WORD_NULL,
                           macro_background + macro_text_dis, macro_background + macro_text_dis);
            }
        }

        if ((refresh_flag & flag_VIBRATO) == flag_VIBRATO) {
            if (ptr_vibrato_table != 0) {
                show_queue(X(67 + _add_prev_size * 2), Y(11), page_len, vibrato_page, 255, 4);
                VScrollBar(temp_screen, X(77 + _add_prev_size * 2), Y(12), page_len, 255, vibrato_page, WORD_NULL,
                           macro_scrbar_bckg + macro_scrbar_text, macro_scrbar_bckg + macro_scrbar_mark);
            } else {
                show_queue(X(67 + _add_prev_size * 2), Y(11), page_len, 1, 255, 5);
                VScrollBar(temp_screen, X(77 + _add_prev_size * 2), Y(12), page_len, 255, 1, WORD_NULL,
                           macro_background + macro_text_dis, macro_background + macro_text_dis);
            }
        }

        if (pos != 7) {
            ShowStr(temp_screen, X(2), Y(23 + iy), ExpStrR(macro_table_hint_str[pos - 1], 77, ' '), macro_background + macro_hint);
        } else {
            ShowStr(temp_screen, X(2), Y(23 + iy), ExpStrR(macro_table_hint_str[20 + fmreg_hpos - 1], 77, ' '),
                    macro_background + macro_hint);
        }

        // --- グラフ ---
        const uint8_t topic = macro_background + macro_topic;
        if (in_range(pos, 1, 7)) {
            ShowStr(temp_screen, X(32 + _add_prev_size), Y(3), "\xFD", macro_background + macro_text);
            ShowStr(temp_screen, X(32 + _add_prev_size), Y(10), "\xFC", macro_background + macro_text);

            const bool signed_col = in_range(fmreg_hpos, 29, 33);
            const std::string axis = !signed_col ? "\xB3\xB3\xB3\xB3\xB3\x9E" : "\xB3\xB3\x9E\xB3\xB3\xB3";
            ShowVStr(temp_screen, X(22), Y(4), axis, macro_background + macro_text);
            ShowVStr(temp_screen, X(42 + ix), Y(4), axis, macro_background + macro_text);

            int max_value = 0;
            for (int temp = 1; temp <= 255; ++temp) {
                if (std::abs(_fmreg_param(B(temp), fmreg_hpos)) > max_value) max_value = std::abs(_fmreg_param(B(temp), fmreg_hpos));
            }

            const std::string max_str = ExpStrR(ExpStrL(Num2str(max_value, 16), 2, '0'), 3, ' ');
            ShowStr(temp_screen, X(42 + ix + 1), Y(4), max_str, topic);
            ShowStr(temp_screen, X(42 + ix + 1), Y(5), "+", topic);
            if (!signed_col) {
                ShowStr(temp_screen, X(42 + ix + 1), Y(8), " ", topic);
                ShowStr(temp_screen, X(42 + ix + 1), Y(9), ExpStrR("", 3, ' '), topic);
            } else {
                ShowStr(temp_screen, X(42 + ix + 1), Y(8), "-", topic);
                ShowStr(temp_screen, X(42 + ix + 1), Y(9), max_str, topic);
            }

            const double d_factor = (!signed_col ? 90.0 : 45.0) / std::max(max_value, 1);
            const bool dis = _dis_fmreg_col(fmreg_hpos);
            for (int temp = -9 - _add_prev_size; temp <= 9 + _add_prev_size; ++temp) {
                const uint8_t x = X(32 + temp + _add_prev_size);
                const int page = fmreg_page + temp;
                if (page >= 1 && page <= 255) {
                    const uint8_t col_attr = !dis ? LO(fmreg_def_attr(B(page))) : macro_background + macro_text_dis;
                    const int value = _fmreg_param(B(page), fmreg_hpos);
                    if (!signed_col) {
                        ShowVStr(temp_screen, x, Y(4), ExpStrL(_gfx_bar_str(B(pas_round(value * d_factor)), false), 6, ' '), col_attr);
                    } else if (pas_round(value * d_factor) >= 0) {
                        ShowVStr(temp_screen, x, Y(4),
                                 ExpStrR(ExpStrL(_gfx_bar_str(B(pas_round(value * d_factor)), false), 3, ' '), 6, ' '), col_attr);
                    } else {
                        ShowVStr(temp_screen, x, Y(4),
                                 ExpStrL(ExpStrR(_gfx_bar_str(B(pas_round(std::abs(value) * d_factor)), true), 3, ' '), 6, ' '),
                                 col_attr);
                    }
                } else {
                    ShowVStr(temp_screen, x, Y(4), ExpStrL("", 6, ' '), macro_background + macro_text);
                }
            }
        }

        if (in_range(pos, 8, 13) || arp_vib_mode) {
            uint8_t _axis_attr;
            if (in_range(pos, 8, 13)) {
                ShowStr(temp_screen, X(32 + _add_prev_size - _sub_prev_xpos_a), Y(3), "\xFD", macro_background + macro_text);
                ShowStr(temp_screen, X(32 + _add_prev_size - _sub_prev_xpos_a), Y(10), "\xFC", macro_background + macro_text);
            }
            if (in_range(pos, 8, 13)) _axis_attr = arp_vib_mode ? macro_background + macro_hi_text : macro_background + macro_text;
            else _axis_attr = macro_background + macro_text;

            ShowVStr(temp_screen, X(22 - _sub_prev_xpos_a), Y(4), "\xB3\xB3\xB3\xB3\xB3\x9E", _axis_attr);
            ShowVStr(temp_screen, X(42 + ix - _sub_prev_xpos_a), Y(4), "\xB3\xB3\xB3\xB3\xB3\x9E", _axis_attr);

            int max_value = 0;
            for (int temp = 1; temp <= 255; ++temp) {
                if (a.data[temp - 1] > max_value && a.data[temp - 1] < 0x80) max_value = a.data[temp - 1];
            }

            const uint8_t mx = X(42 + ix + 1 - _sub_prev_xpos_a);
            if (!arp_vib_mode) {
                ShowStr(temp_screen, mx, Y(4), ExpStrR(Num2str(max_value, 10), 3, ' '), topic);
                ShowStr(temp_screen, mx, Y(5), "+ ", topic);
                ShowStr(temp_screen, mx, Y(8), "  ", topic);
                ShowStr(temp_screen, mx, Y(9), ExpStrR("", 3, ' '), topic);
            } else if (in_range(pos, 8, 13)) {
                const uint8_t hi = macro_background + macro_hi_text;
                ShowStr(temp_screen, mx, Y(4), ExpStrR(Num2str(max_value, 10), 3, ' '), hi);
                ShowStr(temp_screen, mx, Y(5), "+ ", hi);
                ShowStr(temp_screen, mx, Y(8), "  ", hi);
                ShowStr(temp_screen, mx, Y(9), ExpStrR("", 3, ' '), hi);
            }

            const double d_factor = 90.0 / std::max(max_value, 1);
            for (int temp = -9 - _add_prev_size; temp <= 9 + _add_prev_size; ++temp) {
                const uint8_t x = X(32 + temp + _add_prev_size - _sub_prev_xpos_a);
                const int page = arpeggio_page + temp;
                if (page >= 1 && page <= 255) {
                    const uint8_t value = a.data[page - 1];
                    if (value < 0x80) {
                        ShowVStr(temp_screen, x, Y(4), ExpStrL(_gfx_bar_str(B(pas_round(value * d_factor)), false), 6, ' '),
                                 LO(arpeggio_def_attr(B(page))));
                    } else {
                        // オリジナルは範囲外の値でも note_layout を引く
                        const int idx = value - 0x80;
                        const std::string note = idx <= 12 * 8 + 1 ? std::string(note_layout[idx]) : std::string("   ");
                        std::string filtered = note;
                        for (char& c : filtered) {
                            if (c == '-') c = '\xF1';
                        }
                        ShowVStr(temp_screen, x, Y(4), ExpStrL(filtered, 6, ' '), LO(arpeggio_def_attr(B(page))));
                    }
                } else {
                    ShowVStr(temp_screen, x, Y(4), ExpStrL("", 6, ' '), macro_background + macro_text);
                }
            }
        }

        if (in_range(pos, 14, 20) || arp_vib_mode) {
            uint8_t _axis_attr;
            if (in_range(pos, 14, 20)) {
                ShowStr(temp_screen, X(32 + _add_prev_size - _sub_prev_xpos_v), Y(3), "\xFD", macro_background + macro_text);
                ShowStr(temp_screen, X(32 + _add_prev_size - _sub_prev_xpos_v), Y(10), "\xFC", macro_background + macro_text);
            }
            if (in_range(pos, 14, 20)) _axis_attr = arp_vib_mode ? macro_background + macro_hi_text : macro_background + macro_text;
            else _axis_attr = macro_background + macro_text;

            ShowVStr(temp_screen, X(22 - _sub_prev_xpos_v), Y(4), "\xB3\xB3\x9E\xB3\xB3\xB3", _axis_attr);
            ShowVStr(temp_screen, X(42 + ix - _sub_prev_xpos_v), Y(4), "\xB3\xB3\x9E\xB3\xB3\xB3", _axis_attr);

            int max_value = 0;
            for (int temp = 1; temp <= 255; ++temp) {
                if (std::abs(v.data[temp - 1]) > max_value) max_value = std::abs(v.data[temp - 1]);
            }

            const std::string max_str = ExpStrL(Num2str(max_value, 16), 2, '0');
            if (!arp_vib_mode) {
                const uint8_t mx = X(42 + ix + 1 - _sub_prev_xpos_v);
                ShowStr(temp_screen, mx, Y(4), ExpStrR(max_str, 3, ' '), topic);
                ShowStr(temp_screen, mx, Y(5), "+", topic);
                ShowStr(temp_screen, mx, Y(8), "-", topic);
                ShowStr(temp_screen, mx, Y(9), ExpStrR(max_str, 3, ' '), topic);
            } else if (in_range(pos, 14, 20)) {
                const uint8_t hi = macro_background + macro_hi_text;
                const uint8_t mx = X(20 - _sub_prev_xpos_v);
                ShowStr(temp_screen, mx, Y(4), ExpStrL(max_str, 2, ' '), hi);
                ShowStr(temp_screen, mx, Y(5), " +", hi);
                ShowStr(temp_screen, mx, Y(8), " -", hi);
                ShowStr(temp_screen, mx, Y(9), ExpStrL(max_str, 2, ' '), hi);
            }

            const double d_factor = 45.0 / std::max(max_value, 1);
            for (int temp = -9 - _add_prev_size; temp <= 9 + _add_prev_size; ++temp) {
                const uint8_t x = X(32 + temp + _add_prev_size - _sub_prev_xpos_v);
                const int page = vibrato_page + temp;
                if (page >= 1 && page <= 255) {
                    const int value = v.data[page - 1];
                    if (pas_round(value * d_factor) >= 0) {
                        ShowVStr(temp_screen, x, Y(4),
                                 ExpStrR(ExpStrL(_gfx_bar_str(B(pas_round(value * d_factor)), false), 3, ' '), 6, ' '),
                                 LO(vibrato_def_attr(B(page))));
                    } else {
                        ShowVStr(temp_screen, x, Y(4),
                                 ExpStrL(ExpStrR(_gfx_bar_str(B(pas_round(std::abs(value) * d_factor)), true), 3, ' '), 6, ' '),
                                 LO(vibrato_def_attr(B(page))));
                    }
                } else {
                    ShowVStr(temp_screen, x, Y(4), ExpStrR("", 6, ' '), macro_background + macro_text);
                }
            }
        }

        // --- 下の枠 ---
        const uint16_t t4 = get_4op_to_test();
        const uint8_t _4op_pos_shift = t4 == 0 ? 0 : 6;
        if (t4 != 0) {
            const uint8_t _4op_ins1 = HI(t4);
            const uint8_t _4op_ins2 = LO(t4);
            const std::string c1 = connection_str[ins_byte(_4op_ins1, 10) & 1];
            const std::string c2 = connection_str[ins_byte(_4op_ins2, 10) & 1];
            std::string s;
            if (_4op_ins1 == _4op_ins2) {
                s = " `[`\xF4" + byte2hex(_4op_ins1) + ",\xF5" + byte2hex(_4op_ins1) + " " + c1 + "/" + c2 + "`]` ";
            } else if (current_inst == _4op_ins1) {
                s = " `[`~\xF4" + byte2hex(_4op_ins1) + "~,\xF5" + byte2hex(_4op_ins2) + " ~" + c1 + "~/" + c2 + "`]` ";
            } else {
                s = " `[`\xF4" + byte2hex(_4op_ins1) + ",~\xF5" + byte2hex(_4op_ins2) + "~ " + c1 + "/~" + c2 + "~`]` ";
            }
            ShowC3Str(temp_screen, X(1), Y(24 + iy), s, macro_background + macro_hi_text, B(macro_hi_text << 4),
                      macro_background + macro_border);
        } else {
            ShowCStr(temp_screen, X(1), Y(24 + iy),
                     std::string(" [~") + perc_voice_str[std::min<int>(at1(sd.instr_data, current_inst).perc_voice, 5)] + "~] ",
                     macro_background + macro_border, macro_background + macro_hi_text);
        }

        std::string temp_str = at1(sd.instr_macros, current_inst).length != 0 ? " [~MACRO:FM" : " ";
        if (!arp_vib_mode) {
            if (ptr_arpeggio_table != 0) temp_str += temp_str != " " ? "+ARP" : "[~MACRO:ARP";
            if (ptr_vibrato_table != 0) temp_str += temp_str != " " ? "+ViB" : "[~MACRO:ViB";
        }
        if (temp_str != " ") temp_str += "~] ";
        ShowCStr(temp_screen, X(11 + _4op_pos_shift), Y(24 + iy), ExpStrR(temp_str, 21 + 2, '\xCD'),
                 macro_background + macro_border, macro_background + macro_hi_text);

        int nm_slots;
        if (in_range(at1(sd.instr_data, instr).perc_voice, 2, 5)) {
            temp_str = "```` ";
            nm_slots = 1;
        } else if (t4 == 0) {
            temp_str = " `[`12`]` ";
            nm_slots = 2;
        } else {
            temp_str = " `[`1234`]` ";
            nm_slots = 4;
        }
        temp_str += "[~SPEED:" + Num2str(sd.tempo * sd.macro_speedup, 10) + "\xAE~] ";
        ShowC3Str(temp_screen, X(ix + 59), Y(24 + iy), ExpStrL(temp_str, 28, '\xCD'), macro_background + macro_border,
                  macro_background + macro_context, macro_background + macro_context_dis);

        if (nm_slots > 1) {
            for (int temp = 1; temp <= nm_slots; ++temp) {
                if (!_operator_enabled[temp]) {
                    ShowStr(temp_screen, X(ix + 83 - C3StrLen(temp_str) - 1 + temp), Y(24 + iy), "\xFA",
                            instrument_bckg + instrument_border);
                }
            }
        }

        _preview_indic_proc(0);
        move2screen_alt();
    }

    // --- 編集 ---

    uint16_t key() { return is_environment.keystroke; }

    void next_page(uint8_t& page)
    {
        if (page < 255) ++page;
        else if (cycle_pattern) page = 1;
    }

    void prev_page(uint8_t& page)
    {
        if (page > 1) --page;
        else if (cycle_pattern) page = 255;
    }

    // 16 進数の文字の値 (オリジナルの hex)
    uint8_t hex(char chr)
    {
        const char c = UpCase(chr);
        for (int i = 0; i < 16; ++i) {
            if (_hex[i] == c) return B(i);
        }
        return BYTE_NULL;
    }

    void copy_object()
    {
        switch (clipboard.object_type) {
        case objMacroTableLine:
            switch (clipboard.mcrtab_type) {
            case mttFM_reg_table: clipboard.fmreg_table.data[0] = im().data[fmreg_page - 1]; break;
            case mttArpeggio_table: clipboard.macro_table.arpeggio.data[0] = arp().data[arpeggio_page - 1]; break;
            case mttVibrato_table: clipboard.macro_table.vibrato.data[0] = vib().data[vibrato_page - 1]; break;
            }
            break;

        case objMacroTableColumn:
            switch (clipboard.mcrtab_type) {
            case mttFM_reg_table:
                clipboard.fmtab_def_typ = fmreg_hpos;
                for (int temp = 1; temp <= 255; ++temp) clipboard.fmreg_table.data[temp - 1] = im().data[temp - 1];
                break;
            case mttArpeggio_table: clipboard.macro_table.arpeggio = arp(); break;
            case mttVibrato_table: clipboard.macro_table.vibrato = vib(); break;
            }
            break;

        case objMacroTable:
            switch (clipboard.mcrtab_type) {
            case mttFM_reg_table: clipboard.fmreg_table = im(); break;
            case mttArpeggio_table: clipboard.macro_table.arpeggio = arp(); break;
            case mttVibrato_table: clipboard.macro_table.vibrato = vib(); break;
            }
            break;

        default:
            break;
        }
    }

    void paste_object()
    {
        switch (clipboard.object_type) {
        case objMacroTableLine:
            switch (clipboard.mcrtab_type) {
            case mttFM_reg_table:
                if (pos == 7) {
                    RegisterTableDef& d = im().data[fmreg_page - 1];
                    const uint8_t temp = d.fm_data.FEEDBACK_FM;
                    d = clipboard.fmreg_table.data[0];
                    d.fm_data.FEEDBACK_FM = B((temp & 0xc0) + d.fm_data.FEEDBACK_FM);
                }
                break;
            case mttArpeggio_table:
                if (pos == 13) arp().data[arpeggio_page - 1] = clipboard.macro_table.arpeggio.data[0];
                break;
            case mttVibrato_table:
                if (pos == 20) vib().data[vibrato_page - 1] = clipboard.macro_table.vibrato.data[0];
                break;
            }
            break;

        case objMacroTableColumn:
            switch (clipboard.mcrtab_type) {
            case mttFM_reg_table:
                for (int temp = 1; temp <= 255; ++temp) {
                    RegisterTableDef& d = im().data[temp - 1];
                    FmInstData& f = d.fm_data;
                    const RegisterTableDef& cd = clipboard.fmreg_table.data[temp - 1];
                    const FmInstData& c = cd.fm_data;
                    switch (clipboard.fmtab_def_typ) {
                    case 1: f.ATTCK_DEC_modulator = B((c.ATTCK_DEC_modulator & 0xf0) + (f.ATTCK_DEC_modulator & 0x0f)); break;
                    case 2: f.ATTCK_DEC_modulator = B((f.ATTCK_DEC_modulator & 0xf0) + (c.ATTCK_DEC_modulator & 0x0f)); break;
                    case 3: f.SUSTN_REL_modulator = B((c.SUSTN_REL_modulator & 0xf0) + (f.SUSTN_REL_modulator & 0x0f)); break;
                    case 4: f.SUSTN_REL_modulator = B((f.SUSTN_REL_modulator & 0xf0) + (c.SUSTN_REL_modulator & 0x0f)); break;
                    case 5: f.WAVEFORM_modulator = c.WAVEFORM_modulator; break;
                    case 6:
                    case 7: f.KSL_VOLUM_modulator = B((f.KSL_VOLUM_modulator & 0xc0) + (c.KSL_VOLUM_modulator & 0x3f)); break;
                    case 8: f.KSL_VOLUM_modulator = B((f.KSL_VOLUM_modulator & 0x3f) + (c.KSL_VOLUM_modulator & 0xc0)); break;
                    case 9: f.AM_VIB_EG_modulator = B((f.AM_VIB_EG_modulator & 0xf0) + (c.AM_VIB_EG_modulator & 0x0f)); break;
                    case 10: f.AM_VIB_EG_modulator = B((f.AM_VIB_EG_modulator & 0x7f) + (c.AM_VIB_EG_modulator & 0x80)); break;
                    case 11: f.AM_VIB_EG_modulator = B((f.AM_VIB_EG_modulator & 0xbf) + (c.AM_VIB_EG_modulator & 0x40)); break;
                    case 12: f.AM_VIB_EG_modulator = B((f.AM_VIB_EG_modulator & 0xef) + (c.AM_VIB_EG_modulator & 0x10)); break;
                    case 13: f.AM_VIB_EG_modulator = B((f.AM_VIB_EG_modulator & 0xdf) + (c.AM_VIB_EG_modulator & 0x20)); break;
                    case 14: f.ATTCK_DEC_carrier = B((c.ATTCK_DEC_carrier & 0xf0) + (f.ATTCK_DEC_carrier & 0x0f)); break;
                    case 15: f.ATTCK_DEC_carrier = B((f.ATTCK_DEC_carrier & 0xf0) + (c.ATTCK_DEC_carrier & 0x0f)); break;
                    case 16: f.SUSTN_REL_carrier = B((c.SUSTN_REL_carrier & 0xf0) + (f.SUSTN_REL_carrier & 0x0f)); break;
                    case 17: f.SUSTN_REL_carrier = B((f.SUSTN_REL_carrier & 0xf0) + (c.SUSTN_REL_carrier & 0x0f)); break;
                    case 18: f.WAVEFORM_carrier = c.WAVEFORM_carrier; break;
                    case 19:
                    case 20: f.KSL_VOLUM_carrier = B((f.KSL_VOLUM_carrier & 0xc0) + (c.KSL_VOLUM_carrier & 0x3f)); break;
                    case 21: f.KSL_VOLUM_carrier = B((f.KSL_VOLUM_carrier & 0x3f) + (c.KSL_VOLUM_carrier & 0xc0)); break;
                    case 22: f.AM_VIB_EG_carrier = B((f.AM_VIB_EG_carrier & 0xf0) + (c.AM_VIB_EG_carrier & 0x0f)); break;
                    case 23: f.AM_VIB_EG_carrier = B((f.AM_VIB_EG_carrier & 0x7f) + (c.AM_VIB_EG_carrier & 0x80)); break;
                    case 24: f.AM_VIB_EG_carrier = B((f.AM_VIB_EG_carrier & 0xbf) + (c.AM_VIB_EG_carrier & 0x40)); break;
                    case 25: f.AM_VIB_EG_carrier = B((f.AM_VIB_EG_carrier & 0xef) + (c.AM_VIB_EG_carrier & 0x10)); break;
                    case 26: f.AM_VIB_EG_carrier = B((f.AM_VIB_EG_carrier & 0xdf) + (c.AM_VIB_EG_carrier & 0x20)); break;
                    case 27: f.FEEDBACK_FM = B((f.FEEDBACK_FM & 0xfe) + (c.FEEDBACK_FM & 1)); break;
                    case 28: f.FEEDBACK_FM = B((f.FEEDBACK_FM & 0xc1) + (c.FEEDBACK_FM & 0x3e)); break;
                    case 29: case 30: case 31: case 32: d.freq_slide = cd.freq_slide; break;
                    case 33: d.panning = cd.panning; break;
                    case 34: case 35: d.duration = cd.duration; break;
                    }
                }
                break;
            case mttArpeggio_table:
                if (in_range(pos, 8, 13)) std::memcpy(arp().data, clipboard.macro_table.arpeggio.data, sizeof(arp().data));
                break;
            case mttVibrato_table:
                if (in_range(pos, 14, 20)) std::memcpy(vib().data, clipboard.macro_table.vibrato.data, sizeof(vib().data));
                break;
            }
            break;

        case objMacroTable:
            switch (clipboard.mcrtab_type) {
            case mttFM_reg_table:
                if (in_range(pos, 1, 7)) im() = clipboard.fmreg_table;
                break;
            case mttArpeggio_table:
                if (in_range(pos, 8, 13)) arp() = clipboard.macro_table.arpeggio;
                break;
            case mttVibrato_table:
                if (in_range(pos, 14, 20)) vib() = clipboard.macro_table.vibrato;
                break;
            }
            break;

        default:
            break;
        }
    }

    // fmreg_str[i] (範囲外は 0)
    char fmreg_chr(int i) { return (i >= 1 && i <= static_cast<int>(fmreg_str.size())) ? fmreg_str[i - 1] : '\0'; }

    void _scroll_cur_left()
    {
        do {
            if (fmreg_cursor_pos > 1) --fmreg_cursor_pos;
            else --fmreg_left_margin;
        } while (!(fmreg_chr(fmreg_left_margin + fmreg_cursor_pos - 1 - 1) == ' ' || fmreg_left_margin + fmreg_cursor_pos - 1 == 1));
        fmreg_cursor_pos = B(pos5[fmreg_hpos - 1] - fmreg_left_margin + 1);
    }

    void _scroll_cur_right()
    {
        do {
            if (fmreg_cursor_pos < std::min(57, 31 + window_area_inc_x)) ++fmreg_cursor_pos;
            else ++fmreg_left_margin;
        } while (!(fmreg_chr(fmreg_left_margin + fmreg_cursor_pos - 1 + 1) == ' ' || fmreg_left_margin + fmreg_cursor_pos - 1 == 57));
        fmreg_cursor_pos = B(pos5[fmreg_hpos - 1] - fmreg_left_margin + 1);
    }

    // Shift+Home / Shift+End など: FM レジスタ表の最初 / 最後の列へ
    void fmreg_first_col()
    {
        fmreg_hpos = 1;
        fmreg_cursor_pos = 1;
        fmreg_left_margin = 1;
    }

    void fmreg_last_col()
    {
        fmreg_hpos = 35;
        fmreg_cursor_pos = B(std::min(57, 31 + window_area_inc_x));
        fmreg_left_margin = B(std::max(pos5[fmreg_hpos - 1] - (31 + window_area_inc_x) + 1, 1));
    }

    int _inc(int value, int limit) { return value < limit ? value + 1 : value; }
    int _dec(int value, int limit) { return value > limit ? value - 1 : value; }

    void _set_operator_flag(uint8_t op, bool toggle)
    {
        const uint16_t t4 = get_4op_to_test();
        if (in_range(at1(songdata().instr_data, instr).perc_voice, 2, 5) || (t4 == 0 && !in_range(op, 1, 2)) || !in_range(op, 1, 4)) return;

        if (!toggle) {
            std::fill(std::begin(_operator_enabled), std::end(_operator_enabled), false);
            _operator_enabled[op] = true;
            return;
        }

        bool temp_enabled[5];
        std::copy(std::begin(_operator_enabled), std::end(_operator_enabled), temp_enabled);
        temp_enabled[op] = !temp_enabled[op];
        if (t4 == 0) {
            if (!(!temp_enabled[1] && !temp_enabled[2])) std::copy(temp_enabled + 1, temp_enabled + 5, _operator_enabled + 1);
        } else if (!(!temp_enabled[1] && !temp_enabled[2] && !temp_enabled[3] && !temp_enabled[4])) {
            std::copy(temp_enabled + 1, temp_enabled + 5, _operator_enabled + 1);
        }
    }

    // Ctrl+[ / ] (前後の楽器)、Ctrl+Shift+[ / ] (マクロの速度)、Alt+0〜4 (試聴するオペレータ)。開き直すなら TRUE
    bool _check_macro_speed_change()
    {
        FixedSongData& sd = songdata();
        bool result = false;
        switch (key()) {
        case kCtLbr:
            if (shift_pressed()) {
                if (sd.macro_speedup > 1) --sd.macro_speedup;
                P().macro_speedup = sd.macro_speedup;
                keyboard_reset_buffer();
            } else if (current_inst > 1) {
                --current_inst;
                if (marked_instruments() != 2) reset_marked_instruments();
                instrum_page = current_inst;
                std::fill(std::begin(_operator_enabled), std::end(_operator_enabled), true);
                STATUS_LINE_refresh();
                keyboard_reset_buffer();
                result = true;
            }
            break;

        case kCtRbr:
            if (shift_pressed()) {
                ++sd.macro_speedup;
                if (calc_max_speedup(sd.tempo) < sd.macro_speedup) sd.macro_speedup = calc_max_speedup(sd.tempo);
                P().macro_speedup = sd.macro_speedup;
                keyboard_reset_buffer();
            } else if (current_inst < 255) {
                ++current_inst;
                if (marked_instruments() != 2) reset_marked_instruments();
                instrum_page = current_inst;
                std::fill(std::begin(_operator_enabled), std::end(_operator_enabled), true);
                STATUS_LINE_refresh();
                keyboard_reset_buffer();
                result = true;
            }
            break;
        }

        switch (key()) {
        case kAlt0: std::fill(std::begin(_operator_enabled), std::end(_operator_enabled), true); return true;
        case kAlt1: _set_operator_flag(1, shift_pressed()); return true;
        case kAlt2: _set_operator_flag(2, shift_pressed()); return true;
        case kAlt3: _set_operator_flag(3, shift_pressed()); return true;
        case kAlt4: _set_operator_flag(4, shift_pressed()); return true;
        }
        return result;
    }

    // キーオフの位置がループの中にならないように後ろへずらす
    template <class T>
    void fix_keyoff(T& t)
    {
        while (!(t.keyoff_pos > t.loop_begin + min0(t.loop_length - 1, 0) || t.loop_begin == 0 || t.loop_length == 0 ||
                 t.keyoff_pos == 0)) {
            ++t.keyoff_pos;
        }
    }

    // Ctrl+Home / End (長さ)、Ctrl+PgUp / PgDn (表の番号)。Shift 付きはループの開始位置 / 長さ
    // (_check_fmreg_general_keys / _check_arp_general_keys / _check_vib_general_keys)
    template <class T>
    void general_keys(T& t, uint8_t* ptr_table)
    {
        switch (key()) {
        case kCtHOME:
            if (!shift_pressed()) {
                if (t.length > 0) --t.length;
            } else if (t.loop_begin > 0) {
                --t.loop_begin;
                fix_keyoff(t);
            }
            break;
        case kCtEND:
            if (!shift_pressed()) {
                if (t.length < 255) ++t.length;
            } else if (t.loop_begin < 255) {
                ++t.loop_begin;
                fix_keyoff(t);
            }
            break;
        case kCtPgUP:
            if (!shift_pressed()) {
                if (ptr_table != nullptr && *ptr_table > 1) --*ptr_table;
            } else if (t.loop_length > 0) {
                --t.loop_length;
                fix_keyoff(t);
            }
            break;
        case kCtPgDN:
            if (!shift_pressed()) {
                if (ptr_table != nullptr && *ptr_table < 255) ++*ptr_table;
            } else if (t.loop_length < 255) {
                ++t.loop_length;
                fix_keyoff(t);
            }
            break;
        }
    }

    enum FieldKind { fkPlain, fkLoop, fkKeyOff };

    // 上段の項目 (2 桁の 16 進数) の入力と +/-。fkLoop は入力の後でキーオフの位置を直す
    template <class T>
    void input_field(T& t, uint8_t& value, uint8_t x, uint8_t y, FieldKind kind)
    {
        is_setting.character_set = HEX_NUM_CHARSET;
        std::string temps;
        for (;;) {
            const std::string def = byte2hex(value);
            {
                PlayerUnlock unlock;
                temps = InputStr(def, x, y, 2, 2, macro_input_bckg + macro_input, macro_def_bckg + macro_def);
            }
            const int n = Str2num(temps, 16);
            if (key() == kESC || _force_program_quit) break;
            if (kind != fkKeyOff) {
                if (n >= 0 && n <= 255) break;
            } else if ((n >= 0 && n <= 255 && n > t.loop_begin + min0(t.loop_length - 1, 0)) || t.loop_begin == 0 ||
                       t.loop_length == 0 || n == 0) {
                break;
            }
        }

        value = B(Str2num(temps, 16));
        const bool plus = key() == kCHplus || key() == kNPplus;
        const bool minus = key() == kCHmins || key() == kNPmins;
        if (kind != fkKeyOff) {
            if (plus && value < 255) ++value;
            if (minus && value > 0) --value;
        } else if (plus) {
            if (t.loop_begin == 0 || t.loop_length == 0 || t.keyoff_pos != 0) {
                if (t.keyoff_pos < 255) ++t.keyoff_pos;
            } else if (t.loop_begin + t.loop_length <= 255) {
                t.keyoff_pos = B(t.loop_begin + t.loop_length);
            }
        } else if (minus) {
            if (min0(t.keyoff_pos - 1, 0) > t.loop_begin + min0(t.loop_length - 1, 0) ||
                (t.keyoff_pos > 0 && (t.loop_begin == 0 || t.loop_length == 0))) {
                --t.keyoff_pos;
            }
        }
        if (kind == fkLoop) fix_keyoff(t);
    }

    // pos 1〜6: FM レジスタのマクロの上段 (長さ・ループ・キーオフ・表の番号)。GOTO _jmp2 なら TRUE
    bool edit_fmreg_header()
    {
        static const uint8_t vib_pos[7] = { 0, 14, 17, 18, 19, 14, 14 };
        static const uint8_t arp_pos[7] = { 0, 8, 10, 11, 12, 8, 8 };
        const uint8_t p = pos;
        RegisterTable& m = im();
        switch (p) {
        case 1: input_field(m, m.length, B(xstart + 17), B(ystart + 4), fkPlain); break;
        case 2: input_field(m, m.loop_begin, B(xstart + 17), B(ystart + 5), fkLoop); break;
        case 3: input_field(m, m.loop_length, B(xstart + 17), B(ystart + 6), fkLoop); break;
        case 4: input_field(m, m.keyoff_pos, B(xstart + 17), B(ystart + 7), fkKeyOff); break;
        case 5: input_field(m, ptr_arpeggio_table, B(xstart + 17), B(ystart + 8), fkPlain); break;
        case 6: input_field(m, ptr_vibrato_table, B(xstart + 17), B(ystart + 9), fkPlain); break;
        }

        const uint16_t k = key();
        if (k == kENTER || k == kTAB || k == kDOWN) {
            pos = B(p + 1);
        } else if (p == 1) {
            if (k == kUP) pos = 7;
            else if (k == kShTAB) pos = ptr_vibrato_table != 0 ? 20 : ptr_arpeggio_table != 0 ? 13 : 7;
        } else if (k == kUP || k == kShTAB) {
            pos = B(p - 1);
        }

        if (_check_macro_speed_change()) return true;
        general_keys(im(), nullptr);

        switch (key()) {
        case kCtLEFT:
            if (ptr_vibrato_table != 0) pos = vib_pos[p];
            else if (ptr_arpeggio_table != 0) pos = arp_pos[p];
            break;
        case kCtRGHT:
            if (ptr_arpeggio_table != 0) pos = arp_pos[p];
            else if (ptr_vibrato_table != 0) pos = vib_pos[p];
            break;
        case kCtrlC:
            if (p != 5) {
                clipboard.object_type = objMacroTable;
                clipboard.mcrtab_type = mttFM_reg_table;
                copy_object();
            }
            break;
        case kCtrlV:
        case kAltP:
            if (p != 5) paste_object();
            break;
        }
        return false;
    }

    // pos 8〜12: アルペジオの表の上段
    bool edit_arpeggio_header()
    {
        static const uint8_t fm_pos[5] = { 1, 1, 2, 3, 4 };
        static const uint8_t vib_pos[5] = { 14, 15, 17, 18, 19 };
        const uint8_t p = pos;
        ArpeggioTable& a = arp();
        const uint8_t x = B(xstart + 60 + window_area_inc_x);
        const uint8_t y = B(ystart + 4 + (p - 8));
        switch (p) {
        case 8: input_field(a, a.length, x, y, fkPlain); break;
        case 9: input_field(a, a.speed, x, y, fkPlain); break;
        case 10: input_field(a, a.loop_begin, x, y, fkLoop); break;
        case 11: input_field(a, a.loop_length, x, y, fkLoop); break;
        case 12: input_field(a, a.keyoff_pos, x, y, fkKeyOff); break;
        }

        const uint16_t k = key();
        if (k == kENTER || k == kTAB || k == kDOWN) {
            pos = B(p + 1);
        } else if (p == 8) {
            if (k == kUP) pos = 13;
            else if (k == kShTAB) pos = !arp_vib_mode ? 7 : 20;
        } else if (k == kUP || k == kShTAB) {
            pos = B(p - 1);
        }

        if (_check_macro_speed_change()) return true;
        general_keys(arp(), &ptr_arpeggio_table);

        switch (key()) {
        case kCtLEFT:
            pos = !arp_vib_mode ? fm_pos[p - 8] : vib_pos[p - 8];
            break;
        case kCtRGHT:
            if (arp_vib_mode) pos = vib_pos[p - 8];
            else pos = ptr_vibrato_table != 0 ? vib_pos[p - 8] : fm_pos[p - 8];
            break;
        case kCtrlC:
            clipboard.object_type = objMacroTable;
            clipboard.mcrtab_type = mttArpeggio_table;
            copy_object();
            break;
        case kCtrlV:
        case kAltP:
            paste_object();
            break;
        }
        return false;
    }

    // pos 14〜19: ビブラートの表の上段
    bool edit_vibrato_header()
    {
        static const uint8_t arp_pos[6] = { 8, 9, 8, 10, 11, 12 };
        static const uint8_t fm_pos[6] = { 1, 1, 1, 2, 3, 4 };
        const uint8_t p = pos;
        VibratoTable& v = vib();
        const uint8_t x = B(xstart + 77 + window_area_inc_x);
        const uint8_t y = B(ystart + 4 + (p - 14));
        switch (p) {
        case 14: input_field(v, v.length, x, y, fkPlain); break;
        case 15: input_field(v, v.speed, x, y, fkPlain); break;
        case 16: input_field(v, v.delay, x, y, fkPlain); break;
        case 17: input_field(v, v.loop_begin, x, y, fkLoop); break;
        case 18: input_field(v, v.loop_length, x, y, fkLoop); break;
        case 19: input_field(v, v.keyoff_pos, x, y, fkKeyOff); break;
        }

        const uint16_t k = key();
        if (k == kENTER || k == kTAB || k == kDOWN) {
            pos = B(p + 1);
        } else if (p == 14) {
            if (k == kUP) pos = 20;
            else if (k == kShTAB) pos = !arp_vib_mode ? (ptr_arpeggio_table != 0 ? 13 : 7) : 13;
        } else if (k == kUP || k == kShTAB) {
            pos = B(p - 1);
        }

        if (_check_macro_speed_change()) return true;
        general_keys(vib(), &ptr_vibrato_table);

        switch (key()) {
        case kCtLEFT:
            if (arp_vib_mode) pos = arp_pos[p - 14];
            else pos = ptr_arpeggio_table != 0 ? arp_pos[p - 14] : fm_pos[p - 14];
            break;
        case kCtRGHT:
            pos = arp_vib_mode ? arp_pos[p - 14] : fm_pos[p - 14];
            break;
        case kCtrlC:
            clipboard.object_type = objMacroTable;
            clipboard.mcrtab_type = mttVibrato_table;
            copy_object();
            break;
        case kCtrlV:
        case kAltP:
            paste_object();
            break;
        }
        return false;
    }

    // FEEDBACK_FM のフラグ (Ctrl+E / N / Z) を切り替えて次の行へ
    void toggle_macro_flag(uint8_t flag)
    {
        uint8_t& fb = im().data[fmreg_page - 1].fm_data.FEEDBACK_FM;
        if ((fb | flag) != fb) fb = B((fb & 0x1f) + flag);
        else fb = B(fb & 0x1f);
        next_page(fmreg_page);
    }

    // Ctrl+Alt+E / N / Z: 全行のフラグを消す
    void clear_macro_flag(uint8_t flag)
    {
        for (int temp = 1; temp <= 255; ++temp) {
            uint8_t& fb = im().data[temp - 1].fm_data.FEEDBACK_FM;
            if ((fb | flag) == fb) fb = B(fb & 0x1f);
        }
    }

    // 出力レベルを音量に読み替える (楽器 <-> マクロの行)
    void invert_volume(FmInstData& f)
    {
        f.KSL_VOLUM_modulator = B((f.KSL_VOLUM_modulator & 0xc0) + 63 - (f.KSL_VOLUM_modulator & 0x3f));
        f.KSL_VOLUM_carrier = B((f.KSL_VOLUM_carrier & 0xc0) + 63 - (f.KSL_VOLUM_carrier & 0x3f));
    }

    // Ctrl+BkSpc / Alt+S で使う、列 fmreg_hpos に対応する dis_fmreg_col の番号
    int dis_col_index(uint8_t hpos)
    {
        if (in_range(hpos, 1, 5)) return hpos - 1;
        if (in_range(hpos, 6, 7)) return 5;
        if (in_range(hpos, 8, 18)) return hpos - 2;
        if (in_range(hpos, 19, 20)) return 17;
        if (in_range(hpos, 21, 28)) return hpos - 3;
        if (in_range(hpos, 29, 32)) return 26;
        if (hpos == 33) return 27;
        return -1;
    }

    // pos 7: FM レジスタ表
    bool edit_fmreg_table()
    {
        GotoXY(B(xstart + 10 + fmreg_cursor_pos - 1), B(ystart + 16 + window_area_inc_y / 2));
        {
            PlayerUnlock unlock;
            is_environment.keystroke = getkey();
        }

        if (HI(key()) == HI(kSlashR)) {
            RegisterTableDef& d = im().data[fmreg_page - 1];
            FmInstData& f = d.fm_data;
            bool nope = true;
            switch (fmreg_hpos) {
            case 10: f.AM_VIB_EG_modulator ^= 0x80; break;
            case 11: f.AM_VIB_EG_modulator ^= 0x40; break;
            case 12: f.AM_VIB_EG_modulator ^= 0x10; break;
            case 13: f.AM_VIB_EG_modulator ^= 0x20; break;
            case 23: f.AM_VIB_EG_carrier ^= 0x80; break;
            case 24: f.AM_VIB_EG_carrier ^= 0x40; break;
            case 25: f.AM_VIB_EG_carrier ^= 0x10; break;
            case 26: f.AM_VIB_EG_carrier ^= 0x20; break;
            case 33:
                nope = false;
                switch (d.panning) {
                case 0: d.panning = 2; break;
                case 1: d.panning = 0; break;
                case 2: d.panning = 1; break;
                }
                break;
            default: nope = false; break;
            }
            if (nope) next_page(fmreg_page);
        }

        if (_check_macro_speed_change()) return true;
        RegisterTable& m = im();
        general_keys(m, nullptr);

        switch (key()) {
        case kCtLEFT:
            if (!shift_pressed()) {
                if (ptr_vibrato_table != 0) pos = 20;
                else if (ptr_arpeggio_table != 0) pos = 13;
            } else if (fmreg_page > m.length) {
                fmreg_page = B(std::max<int>(m.length, 1));
            } else {
                fmreg_page = 1;
            }
            break;

        case kCtRGHT:
            if (!shift_pressed()) {
                if (ptr_arpeggio_table != 0) pos = 13;
                else if (ptr_vibrato_table != 0) pos = 20;
            } else if (fmreg_page < m.length) {
                fmreg_page = B(std::max<int>(m.length, 1));
            } else {
                fmreg_page = 255;
            }
            break;

        case kUP: prev_page(fmreg_page); break;
        case kDOWN: next_page(fmreg_page); break;
        case kPgUP: fmreg_page = fmreg_page > 16 ? B(fmreg_page - 16) : 1; break;
        case kPgDOWN: fmreg_page = fmreg_page + 16 < 255 ? B(fmreg_page + 16) : 255; break;

        case kHOME:
            if (!shift_pressed()) fmreg_page = 1;
            else fmreg_first_col();
            break;
        case kEND:
            if (!shift_pressed()) fmreg_page = 255;
            else fmreg_last_col();
            break;
        case kNPHOME:
            if (shift_pressed()) fmreg_first_col();
            break;
        case kNPEND:
            if (shift_pressed()) fmreg_last_col();
            break;

        case kLEFT:
            if (fmreg_hpos > 1) {
                --fmreg_hpos;
                _scroll_cur_left();
            } else if (cycle_pattern) {
                fmreg_last_col();
            }
            break;
        case kRIGHT:
            if (fmreg_hpos < 35) {
                ++fmreg_hpos;
                _scroll_cur_right();
            } else if (cycle_pattern) {
                fmreg_first_col();
            }
            break;

        case kTAB: pos = ptr_arpeggio_table != 0 ? 8 : ptr_vibrato_table != 0 ? 14 : 1; break;
        case kShTAB: pos = 6; break;
        case kENTER:
            if (!shift_pressed()) pos = ptr_arpeggio_table != 0 ? 8 : ptr_vibrato_table != 0 ? 14 : 1;
            else call_pickup_proc2 = true;
            break;

        case kCtrlC:
            clipboard.object_type = !shift_pressed() ? objMacroTableLine : objMacroTableColumn;
            clipboard.mcrtab_type = mttFM_reg_table;
            copy_object();
            break;
        case kCtrlV:
        case kAltP:
            paste_object();
            if (clipboard.object_type == objMacroTableLine && clipboard.mcrtab_type == mttFM_reg_table) next_page(fmreg_page);
            break;

        case kAltC:
            // Ctrl+Alt+C: その列のモジュレータの値をキャリアから写す
            if (ctrl_pressed() && in_range(fmreg_hpos, 1, 13)) {
                for (int temp = 1; temp <= 255; ++temp) {
                    FmInstData& f = m.data[temp - 1].fm_data;
                    switch (fmreg_hpos) {
                    case 1: f.ATTCK_DEC_modulator = B(((f.ATTCK_DEC_carrier >> 4) << 4) + (f.ATTCK_DEC_modulator & 0x0f)); break;
                    case 2: f.ATTCK_DEC_modulator = B((f.ATTCK_DEC_modulator & 0xf0) + (f.ATTCK_DEC_carrier & 0x0f)); break;
                    case 3: f.SUSTN_REL_modulator = B(((f.SUSTN_REL_carrier >> 4) << 4) + (f.SUSTN_REL_modulator & 0x0f)); break;
                    case 4: f.SUSTN_REL_modulator = B((f.SUSTN_REL_modulator & 0xf0) + (f.SUSTN_REL_carrier & 0x0f)); break;
                    case 5: f.WAVEFORM_modulator = f.WAVEFORM_carrier; break;
                    case 6:
                    case 7: f.KSL_VOLUM_modulator = B((f.KSL_VOLUM_modulator & 0xc0) + (f.KSL_VOLUM_carrier & 0x3f)); break;
                    case 8: f.KSL_VOLUM_modulator = B((f.KSL_VOLUM_modulator & 0x3f) + ((f.KSL_VOLUM_carrier >> 6) << 6)); break;
                    case 9: f.AM_VIB_EG_modulator = B((f.AM_VIB_EG_modulator & 0xf0) + (f.AM_VIB_EG_carrier & 0x0f)); break;
                    case 10: f.AM_VIB_EG_modulator = B((f.AM_VIB_EG_modulator & 0x7f) + ((f.AM_VIB_EG_carrier >> 7) << 7)); break;
                    case 11: f.AM_VIB_EG_modulator = B((f.AM_VIB_EG_modulator & 0xbf) + (((f.AM_VIB_EG_carrier >> 6) & 1) << 6)); break;
                    case 12: f.AM_VIB_EG_modulator = B((f.AM_VIB_EG_modulator & 0xef) + (((f.AM_VIB_EG_carrier >> 4) & 1) << 4)); break;
                    case 13: f.AM_VIB_EG_modulator = B((f.AM_VIB_EG_modulator & 0xdf) + (((f.AM_VIB_EG_carrier >> 5) & 1) << 5)); break;
                    }
                }
            }
            break;

        case kAltM:
            // Ctrl+Alt+M: その列のキャリアの値をモジュレータから写す
            if (ctrl_pressed() && in_range(fmreg_hpos, 14, 26)) {
                for (int temp = 1; temp <= 255; ++temp) {
                    FmInstData& f = m.data[temp - 1].fm_data;
                    switch (fmreg_hpos) {
                    case 14: f.ATTCK_DEC_carrier = B(((f.ATTCK_DEC_modulator >> 4) << 4) + (f.ATTCK_DEC_carrier & 0x0f)); break;
                    case 15: f.ATTCK_DEC_carrier = B((f.ATTCK_DEC_carrier & 0xf0) + (f.ATTCK_DEC_modulator & 0x0f)); break;
                    case 16: f.SUSTN_REL_carrier = B(((f.SUSTN_REL_modulator >> 4) << 4) + (f.SUSTN_REL_carrier & 0x0f)); break;
                    case 17: f.SUSTN_REL_carrier = B((f.SUSTN_REL_carrier & 0xf0) + (f.SUSTN_REL_modulator & 0x0f)); break;
                    case 18: f.WAVEFORM_carrier = f.WAVEFORM_modulator; break;
                    case 19:
                    case 20: f.KSL_VOLUM_carrier = B((f.KSL_VOLUM_carrier & 0xc0) + (f.KSL_VOLUM_modulator & 0x3f)); break;
                    case 21: f.KSL_VOLUM_carrier = B((f.KSL_VOLUM_carrier & 0x3f) + ((f.KSL_VOLUM_modulator >> 6) << 6)); break;
                    case 22: f.AM_VIB_EG_carrier = B((f.AM_VIB_EG_carrier & 0xf0) + (f.AM_VIB_EG_modulator & 0x0f)); break;
                    case 23: f.AM_VIB_EG_carrier = B((f.AM_VIB_EG_carrier & 0x7f) + ((f.AM_VIB_EG_modulator >> 7) << 7)); break;
                    case 24: f.AM_VIB_EG_carrier = B((f.AM_VIB_EG_carrier & 0xbf) + (((f.AM_VIB_EG_modulator >> 6) & 1) << 6)); break;
                    case 25: f.AM_VIB_EG_carrier = B((f.AM_VIB_EG_carrier & 0xef) + (((f.AM_VIB_EG_modulator >> 4) & 1) << 4)); break;
                    case 26: f.AM_VIB_EG_carrier = B((f.AM_VIB_EG_carrier & 0xdf) + (((f.AM_VIB_EG_modulator >> 5) & 1) << 5)); break;
                    }
                }
            }
            break;

        case kCtrlE: toggle_macro_flag(MACRO_ENVELOPE_RESTART_FLAG); break;
        case kCtrlN: toggle_macro_flag(MACRO_NOTE_RETRIG_FLAG); break;
        case kCtrlZ: toggle_macro_flag(MACRO_ZERO_FREQ_FLAG); break;
        case kAltE:
            if (ctrl_pressed()) clear_macro_flag(MACRO_ENVELOPE_RESTART_FLAG);
            break;
        case kAltN:
            if (ctrl_pressed()) clear_macro_flag(MACRO_NOTE_RETRIG_FLAG);
            break;
        case kAltZ:
            if (ctrl_pressed()) clear_macro_flag(MACRO_ZERO_FREQ_FLAG);
            break;

        case kCtENTR:
            // Ctrl+Enter: 楽器の値をこの行へ写す (Ctrl+Shift+Enter は他の楽器から)
            if (!shift_pressed()) {
                RegisterTableDef& d = m.data[fmreg_page - 1];
                const Instrument& ins = at1(songdata().instr_data, instr);
                const uint8_t temp = d.fm_data.FEEDBACK_FM;
                d.fm_data = ins.fm_data;
                d.fm_data.FEEDBACK_FM = B((temp & 0xc0) + ins.fm_data.FEEDBACK_FM);
                d.panning = ins.panning;
                d.duration = B(std::max<int>(d.duration, 1));
                invert_volume(d.fm_data);
                next_page(fmreg_page);
            } else {
                call_pickup_proc = true;
            }
            break;

        case kINSERT:
            for (int temp = 255 - 1; temp >= fmreg_page; --temp) m.data[temp] = m.data[temp - 1];
            std::memset(&m.data[fmreg_page - 1], 0, sizeof(m.data[0]));
            break;
        case kDELETE:
            for (int temp = fmreg_page; temp <= 255 - 1; ++temp) m.data[temp - 1] = m.data[temp];
            std::memset(&m.data[254], 0, sizeof(m.data[0]));
            break;

        case kCHplus:
        case kNPplus:
        case kCHmins:
        case kNPmins: {
            const bool up = key() == kCHplus || key() == kNPplus;
            RegisterTableDef& d = m.data[fmreg_page - 1];
            FmInstData& f = d.fm_data;
            const auto step = [up](int value, int limit) { return up ? _inc(value, limit) : _dec(value, 0); };
            switch (fmreg_hpos) {
            case 1: f.ATTCK_DEC_modulator = B((step(f.ATTCK_DEC_modulator >> 4, 15) << 4) + (f.ATTCK_DEC_modulator & 0x0f)); break;
            case 2: f.ATTCK_DEC_modulator = B((f.ATTCK_DEC_modulator & 0xf0) + step(f.ATTCK_DEC_modulator & 0x0f, 15)); break;
            case 3: f.SUSTN_REL_modulator = B((step(f.SUSTN_REL_modulator >> 4, 15) << 4) + (f.SUSTN_REL_modulator & 0x0f)); break;
            case 4: f.SUSTN_REL_modulator = B((f.SUSTN_REL_modulator & 0xf0) + step(f.SUSTN_REL_modulator & 0x0f, 15)); break;
            case 5: f.WAVEFORM_modulator = B(step(f.WAVEFORM_modulator, 7)); break;
            case 6:
            case 7: f.KSL_VOLUM_modulator = B((f.KSL_VOLUM_modulator & 0xc0) + step(f.KSL_VOLUM_modulator & 0x3f, 63)); break;
            case 8: f.KSL_VOLUM_modulator = B((f.KSL_VOLUM_modulator & 0x3f) + (step(f.KSL_VOLUM_modulator >> 6, 3) << 6)); break;
            case 9: f.AM_VIB_EG_modulator = B((f.AM_VIB_EG_modulator & 0xf0) + step(f.AM_VIB_EG_modulator & 0x0f, 15)); break;
            case 10: f.AM_VIB_EG_modulator = B((f.AM_VIB_EG_modulator & 0x7f) + (step(f.AM_VIB_EG_modulator >> 7, 1) << 7)); break;
            case 11: f.AM_VIB_EG_modulator = B((f.AM_VIB_EG_modulator & 0xbf) + (step((f.AM_VIB_EG_modulator >> 6) & 1, 1) << 6)); break;
            case 12: f.AM_VIB_EG_modulator = B((f.AM_VIB_EG_modulator & 0xef) + (step((f.AM_VIB_EG_modulator >> 4) & 1, 1) << 4)); break;
            case 13: f.AM_VIB_EG_modulator = B((f.AM_VIB_EG_modulator & 0xdf) + (step((f.AM_VIB_EG_modulator >> 5) & 1, 1) << 5)); break;
            case 14: f.ATTCK_DEC_carrier = B((step(f.ATTCK_DEC_carrier >> 4, 15) << 4) + (f.ATTCK_DEC_carrier & 0x0f)); break;
            case 15: f.ATTCK_DEC_carrier = B((f.ATTCK_DEC_carrier & 0xf0) + step(f.ATTCK_DEC_carrier & 0x0f, 15)); break;
            case 16: f.SUSTN_REL_carrier = B((step(f.SUSTN_REL_carrier >> 4, 15) << 4) + (f.SUSTN_REL_carrier & 0x0f)); break;
            case 17: f.SUSTN_REL_carrier = B((f.SUSTN_REL_carrier & 0xf0) + step(f.SUSTN_REL_carrier & 0x0f, 15)); break;
            case 18: f.WAVEFORM_carrier = B(step(f.WAVEFORM_carrier, 7)); break;
            case 19:
            case 20: f.KSL_VOLUM_carrier = B((f.KSL_VOLUM_carrier & 0xc0) + step(f.KSL_VOLUM_carrier & 0x3f, 63)); break;
            case 21: f.KSL_VOLUM_carrier = B((f.KSL_VOLUM_carrier & 0x3f) + (step(f.KSL_VOLUM_carrier >> 6, 3) << 6)); break;
            case 22: f.AM_VIB_EG_carrier = B((f.AM_VIB_EG_carrier & 0xf0) + step(f.AM_VIB_EG_carrier & 0x0f, 15)); break;
            case 23: f.AM_VIB_EG_carrier = B((f.AM_VIB_EG_carrier & 0x7f) + (step(f.AM_VIB_EG_carrier >> 7, 1) << 7)); break;
            case 24: f.AM_VIB_EG_carrier = B((f.AM_VIB_EG_carrier & 0xbf) + (step((f.AM_VIB_EG_carrier >> 6) & 1, 1) << 6)); break;
            case 25: f.AM_VIB_EG_carrier = B((f.AM_VIB_EG_carrier & 0xef) + (step((f.AM_VIB_EG_carrier >> 4) & 1, 1) << 4)); break;
            case 26: f.AM_VIB_EG_carrier = B((f.AM_VIB_EG_carrier & 0xdf) + (step((f.AM_VIB_EG_carrier >> 5) & 1, 1) << 5)); break;
            case 27: f.FEEDBACK_FM = B((f.FEEDBACK_FM & 0xfe) + step(f.FEEDBACK_FM & 1, 1)); break;
            case 28: f.FEEDBACK_FM = B((f.FEEDBACK_FM & 1) + (step(f.FEEDBACK_FM >> 1, 7) << 1)); break;
            case 29: case 30: case 31: case 32:
                d.freq_slide = static_cast<int16_t>(up ? _inc(d.freq_slide, 1023) : _dec(d.freq_slide, -1023));
                break;
            case 33:
                if (up) {
                    if (d.panning == 0) d.panning = 2;
                    else if (d.panning == 1) d.panning = 0;
                } else {
                    if (d.panning == 0) d.panning = 1;
                    else if (d.panning == 2) d.panning = 0;
                }
                break;
            case 34: case 35: d.duration = B(step(d.duration, 255)); break;
            }
            break;
        }

        case kBkSPC: {
            RegisterTableDef& d = m.data[fmreg_page - 1];
            FmInstData& f = d.fm_data;
            if (!shift_pressed()) {
                switch (fmreg_hpos) {
                case 1: f.ATTCK_DEC_modulator &= 0x0f; break;
                case 2: f.ATTCK_DEC_modulator &= 0xf0; break;
                case 3: f.SUSTN_REL_modulator &= 0x0f; break;
                case 4: f.SUSTN_REL_modulator &= 0xf0; break;
                case 5: f.WAVEFORM_modulator = 0; break;
                case 6:
                case 7: f.KSL_VOLUM_modulator &= 0xc0; break;
                case 8: f.KSL_VOLUM_modulator &= 0x3f; break;
                case 9: f.AM_VIB_EG_modulator &= 0xf0; break;
                case 10: f.AM_VIB_EG_modulator &= 0x7f; break;
                case 11: f.AM_VIB_EG_modulator &= 0xbf; break;
                case 12: f.AM_VIB_EG_modulator &= 0xef; break;
                case 13: f.AM_VIB_EG_modulator &= 0xdf; break;
                case 14: f.ATTCK_DEC_carrier &= 0x0f; break;
                case 15: f.ATTCK_DEC_carrier &= 0xf0; break;
                case 16: f.SUSTN_REL_carrier &= 0x0f; break;
                case 17: f.SUSTN_REL_carrier &= 0xf0; break;
                case 18: f.WAVEFORM_carrier = 0; break;
                case 19:
                case 20: f.KSL_VOLUM_carrier &= 0xc0; break;
                case 21: f.KSL_VOLUM_carrier &= 0x3f; break;
                case 22: f.AM_VIB_EG_carrier &= 0xf0; break;
                case 23: f.AM_VIB_EG_carrier &= 0x7f; break;
                case 24: f.AM_VIB_EG_carrier &= 0xbf; break;
                case 25: f.AM_VIB_EG_carrier &= 0xef; break;
                case 26: f.AM_VIB_EG_carrier &= 0xdf; break;
                case 27: f.FEEDBACK_FM &= 0xfe; break;
                case 28: f.FEEDBACK_FM &= 0xc1; break;
                case 29: case 30: case 31: case 32: d.freq_slide = 0; break;
                case 33: d.panning = 0; break;
                case 34: case 35: d.duration = 0; break;
                }
            } else {
                std::memset(&f, 0, sizeof(f));
                d.freq_slide = 0;
                d.panning = 0;
                d.duration = 0;
            }
            next_page(fmreg_page);
            break;
        }

        case kCtBkSp: {
            // Ctrl+BkSpc: その列を無効にする / 戻す
            const int idx = dis_col_index(fmreg_hpos);
            if (idx >= 0) dis_col(idx) = dis_col(idx) != 0 ? 0 : 1;
            break;
        }

        case kAltS: {
            // Alt+S: その列だけを有効にする
            const int idx = dis_col_index(fmreg_hpos);
            if (idx >= 0) {
                for (int temp = 0; temp <= 27; ++temp) dis_col(temp) = temp != idx ? 1 : 0;
            }
            break;
        }

        case kAltR:
            for (int temp = 0; temp <= 27; ++temp) dis_col(temp) = 0;
            break;

        case kAstrsk:
        case kNPastr:
            for (int temp = 0; temp <= 27; ++temp) dis_col(temp) = dis_col(temp) != 0 ? 0 : 1;
            break;
        }

        // 16 進数の入力
        const char c = UpCase(static_cast<char>(LO(key())));
        if (((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')) && !shift_pressed()) {
            RegisterTableDef& d = m.data[fmreg_page - 1];
            FmInstData& f = d.fm_data;
            const int h = hex(c);
            const auto positive = [&]() {
                return d.freq_slide > 0 || (d.freq_slide == 0 && m.data[std::max(fmreg_page - 1, 1) - 1].freq_slide >= 0);
            };
            bool nope = true;
            switch (fmreg_hpos) {
            case 1: f.ATTCK_DEC_modulator = B((h << 4) + (f.ATTCK_DEC_modulator & 0x0f)); break;
            case 2: f.ATTCK_DEC_modulator = B((f.ATTCK_DEC_modulator & 0xf0) + h); break;
            case 3: f.SUSTN_REL_modulator = B((h << 4) + (f.SUSTN_REL_modulator & 0x0f)); break;
            case 4: f.SUSTN_REL_modulator = B((f.SUSTN_REL_modulator & 0xf0) + h); break;
            case 5:
                if (h <= 7) f.WAVEFORM_modulator = B(h);
                else nope = false;
                break;
            case 6:
                if (h <= 3) f.KSL_VOLUM_modulator = B((f.KSL_VOLUM_modulator & 0xcf) + (h << 4));
                else nope = false;
                break;
            case 7: f.KSL_VOLUM_modulator = B((f.KSL_VOLUM_modulator & 0xf0) + h); break;
            case 8:
                if (h <= 3) f.KSL_VOLUM_modulator = B((f.KSL_VOLUM_modulator & 0x3f) + (h << 6));
                else nope = false;
                break;
            case 9: f.AM_VIB_EG_modulator = B((f.AM_VIB_EG_modulator & 0xf0) + h); break;
            case 14: f.ATTCK_DEC_carrier = B((h << 4) + (f.ATTCK_DEC_carrier & 0x0f)); break;
            case 15: f.ATTCK_DEC_carrier = B((f.ATTCK_DEC_carrier & 0xf0) + h); break;
            case 16: f.SUSTN_REL_carrier = B((h << 4) + (f.SUSTN_REL_carrier & 0x0f)); break;
            case 17: f.SUSTN_REL_carrier = B((f.SUSTN_REL_carrier & 0xf0) + h); break;
            case 18:
                if (h <= 7) f.WAVEFORM_carrier = B(h);
                else nope = false;
                break;
            case 19:
                if (h <= 3) f.KSL_VOLUM_carrier = B((f.KSL_VOLUM_carrier & 0xcf) + (h << 4));
                else nope = false;
                break;
            case 20: f.KSL_VOLUM_carrier = B((f.KSL_VOLUM_carrier & 0xf0) + h); break;
            case 21:
                if (h <= 3) f.KSL_VOLUM_carrier = B((f.KSL_VOLUM_carrier & 0x3f) + (h << 6));
                else nope = false;
                break;
            case 22: f.AM_VIB_EG_carrier = B((f.AM_VIB_EG_carrier & 0xf0) + h); break;
            case 27:
                if (h <= 1) f.FEEDBACK_FM = B(h + (f.FEEDBACK_FM & 0xfe));
                else nope = false;
                break;
            case 28:
                if (h <= 7) f.FEEDBACK_FM = B((f.FEEDBACK_FM & 0xc1) + (h << 1));
                else nope = false;
                break;
            case 30:
                if (positive()) d.freq_slide = static_cast<int16_t>((d.freq_slide & 0x0ff) + (h << 8));
                else d.freq_slide = static_cast<int16_t>(-((std::abs(d.freq_slide) & 0x0ff) + (h << 8)));
                break;
            case 31:
                if (positive()) d.freq_slide = static_cast<int16_t>((d.freq_slide & 0x0f0f) + (h << 4));
                else d.freq_slide = static_cast<int16_t>(-((std::abs(d.freq_slide) & 0x0f0f) + (h << 4)));
                break;
            case 32:
                if (positive()) d.freq_slide = static_cast<int16_t>((d.freq_slide & 0x0ff0) + h);
                else d.freq_slide = static_cast<int16_t>(-((std::abs(d.freq_slide) & 0x0ff0) + h));
                break;
            case 34: d.duration = B((d.duration & 0x0f) + (h << 4)); break;
            case 35: d.duration = B((d.duration & 0xf0) + h); break;
            default: nope = false; break;
            }

            if (nope) {
                switch (fmreg_hpos) {
                case 6: case 19: case 30: case 31: case 34:
                    if (command_typing != 2) {
                        next_page(fmreg_page);
                    } else {
                        ++fmreg_hpos;
                        _scroll_cur_right();
                    }
                    break;
                case 7: case 20: case 32: case 35:
                    if (command_typing == 2) {
                        if (fmreg_hpos != 32) --fmreg_hpos;
                        else fmreg_hpos = B(fmreg_hpos - 2);
                        _scroll_cur_left();
                    }
                    next_page(fmreg_page);
                    break;
                default:
                    next_page(fmreg_page);
                    break;
                }
            }
        }

        // L / C / R: パン
        const char c2 = UpCase(static_cast<char>(LO(key())));
        if ((c2 == 'L' || c2 == 'C' || c2 == 'R') && !shift_pressed() && fmreg_hpos == 33) {
            RegisterTableDef& d = m.data[fmreg_page - 1];
            switch (c2) {
            case 'L': d.panning = 1; break;
            case 'C': d.panning = 0; break;
            case 'R': d.panning = 2; break;
            }
            next_page(fmreg_page);
        }

        // + / -: 周波数のスライドの符号
        if ((c2 == '+' || c2 == '-') && fmreg_hpos == 29) {
            RegisterTableDef& d = m.data[fmreg_page - 1];
            d.freq_slide = static_cast<int16_t>(c2 == '+' ? std::abs(d.freq_slide) : -std::abs(d.freq_slide));
            next_page(fmreg_page);
        }

        if (shift_pressed() && (key() == kUP || key() == kDOWN)) {
            if (ptr_arpeggio_table != 0) arpeggio_page = fmreg_page;
            if (ptr_vibrato_table != 0) vibrato_page = fmreg_page;
        }
        return false;
    }

    // アルペジオの表の値を、入力欄に出す文字列にする
    std::string arpeggio_value_str(uint8_t value, const std::string& def)
    {
        if (value == 0) return "+0";
        if (in_range(value, 1, 96)) return "+" + Num2str(value, 10);
        if (in_range(value, 0x80, 0x80 + 12 * 8 + 1)) return note_layout[value - 0x80];
        return def;
    }

    // pos 13: アルペジオの表
    bool edit_arpeggio_table()
    {
        const uint8_t in_x = B(xstart + 55 + window_area_inc_x);
        const uint8_t in_y = B(ystart + 16 + window_area_inc_y / 2);
        GotoXY(in_x, in_y);
        {
            PlayerUnlock unlock;
            is_environment.keystroke = getkey();
        }
        if (_check_macro_speed_change()) return true;
        general_keys(arp(), &ptr_arpeggio_table);

        const auto data = []() -> uint8_t& { return arp().data[arpeggio_page - 1]; };
        switch (key()) {
        case kCtLEFT:
            if (!shift_pressed()) pos = !arp_vib_mode ? 7 : 20;
            else if (arpeggio_page > arp().length) arpeggio_page = B(std::max<int>(arp().length, 1));
            else arpeggio_page = 1;
            break;
        case kCtRGHT:
            if (!shift_pressed()) pos = !arp_vib_mode ? (ptr_vibrato_table != 0 ? 20 : 7) : 20;
            else if (arpeggio_page < arp().length) arpeggio_page = B(std::max<int>(arp().length, 1));
            else arpeggio_page = 255;
            break;

        case kUP: prev_page(arpeggio_page); break;
        case kDOWN: next_page(arpeggio_page); break;
        case kPgUP: arpeggio_page = arpeggio_page > 16 ? B(arpeggio_page - 16) : 1; break;
        case kPgDOWN: arpeggio_page = arpeggio_page + 16 < 255 ? B(arpeggio_page + 16) : 255; break;
        case kHOME: arpeggio_page = 1; break;
        case kEND: arpeggio_page = 255; break;

        case kENTER:
        case kTAB:
            pos = !arp_vib_mode ? (ptr_vibrato_table != 0 ? 14 : 1) : 14;
            break;
        case kShTAB: pos = 12; break;

        case kCtrlC:
            clipboard.object_type = !shift_pressed() ? objMacroTableLine : objMacroTableColumn;
            clipboard.mcrtab_type = mttArpeggio_table;
            copy_object();
            break;
        case kCtrlV:
        case kAltP:
            paste_object();
            if (clipboard.object_type == objMacroTableLine && clipboard.mcrtab_type == mttArpeggio_table) next_page(arpeggio_page);
            break;

        case kCHplus:
        case kNPplus:
            if (data() < 0x80) {
                if (data() < 96) ++data();
            } else if (data() < 0x80 + 96 + 1) {
                ++data();
            }
            break;
        case kCHmins:
        case kNPmins:
            if (data() < 0x80) {
                if (data() > 1) --data();
            } else if (data() > 0x80 + 1) {
                --data();
            }
            break;

        case kBkSPC:
            data() = 0;
            next_page(arpeggio_page);
            break;

        case kINSERT: {
            ArpeggioTable& a = arp();
            for (int temp = 255 - 1; temp >= arpeggio_page; --temp) a.data[temp] = a.data[temp - 1];
            a.data[arpeggio_page - 1] = 0;
            break;
        }
        case kDELETE: {
            ArpeggioTable& a = arp();
            for (int temp = arpeggio_page; temp <= 255 - 1; ++temp) a.data[temp - 1] = a.data[temp];
            a.data[254] = 0;
            break;
        }
        }

        if (shift_pressed() && (key() == kUP || key() == kDOWN)) {
            fmreg_page = arpeggio_page;
            if (ptr_vibrato_table != 0) vibrato_page = arpeggio_page;
        }

        const auto is_end_key = [](uint16_t k) { return k == kENTER || k == kUP || k == kDOWN || k == kTAB || k == kShTAB; };
        const auto is_note_letter = [](char ch) { return ch == 'A' || ch == UpCase(b_note) || (ch >= 'C' && ch <= 'G'); };

        // 音名の入力 (固定のノート)
        if (is_note_letter(UpCase(static_cast<char>(LO(key())))) && !shift_pressed()) {
            bool nope = false;
            is_setting.append_enabled = false;
            is_setting.character_set = CharRange('1', '9') | CharSet("a") | CharSet(std::string(1, b_note)) |
                                       CharRange('c', 'g') | CharSet("A") | CharSet(std::string(1, UpCase(b_note))) |
                                       CharRange('C', 'F') | CharSet("#-");
            is_environment.locate_pos = 2;
            std::string tstr(1, static_cast<char>(LO(key())));
            char stale_1st = tstr[0];   // tstr が空になったときの tstr[1] (Pascal の文字列の残り)

            do {
                {
                    PlayerUnlock unlock;
                    tstr = InputStr(tstr, in_x, in_y, 3, 3, macro_input_bckg + macro_input, macro_def_bckg + macro_def);
                }
                is_setting.append_enabled = true;
                if (!tstr.empty()) stale_1st = tstr[0];

                const char first = UpCase(stale_1st);
                if ((first == '+' || (first >= '0' && first <= '9') || is_note_letter(first)) && is_end_key(key())) {
                    nope = false;
                    if (!tstr.empty() && tstr[0] == '+') tstr.erase(0, 1);
                    const char f1 = tstr.empty() ? stale_1st : tstr[0];
                    const int n = Str2num(tstr, 10);
                    if (f1 >= '0' && f1 <= '9' && n >= 0 && n <= 96) {
                        nope = true;
                        data() = B(n);
                        next_page(arpeggio_page);
                    } else {
                        if (tstr.size() == 2) {
                            if (tstr[1] >= '1' && tstr[1] <= '9') tstr.insert(1, "-");
                            else if (tstr[1] == '-' || tstr[1] == '#') tstr += Num2str(current_octave, 10);
                        }
                        if (tstr.size() == 1) tstr += "-" + Num2str(current_octave, 10);

                        for (int temp1 = 1; temp1 <= 12 * 8 + 1; ++temp1) {
                            if (Upper(tstr) == note_layout[temp1]) {
                                nope = true;
                                data() = B(0x80 + temp1);
                                break;
                            }
                        }
                        if (!nope && tstr.size() == 2) {
                            for (int temp1 = 1; temp1 <= 12 * 8 + 1; ++temp1) {
                                if (Copy(Upper(tstr), 1, 2) == Copy(note_layout[temp1], 1, 2)) {
                                    nope = true;
                                    data() = B(0x80 + temp1);
                                    break;
                                }
                            }
                        }

                        if (nope) {
                            if (key() == kUP) prev_page(arpeggio_page);
                            else if (key() == kDOWN || key() == kENTER) next_page(arpeggio_page);
                        }
                        if (!nope) tstr = arpeggio_value_str(data(), tstr);
                        if (!tstr.empty()) stale_1st = tstr[0];
                    }
                }
            } while (!((nope || key() == kESC) && (key() == kESC || is_end_key(key()))) && !_force_program_quit);

            switch (key()) {
            case kTAB: pos = ptr_vibrato_table != 0 ? 14 : 1; break;
            case kShTAB: pos = 12; break;
            case kESC: is_environment.keystroke = kENTER; break;
            }
        }

        // 半音の数の入力
        const char c = UpCase(static_cast<char>(LO(key())));
        if ((c == '+' || (c >= '0' && c <= '9')) && !shift_pressed()) {
            bool nope = false;
            is_setting.append_enabled = false;
            is_setting.character_set = CharSet("+") | CharRange('0', '9');
            is_environment.locate_pos = 2;
            std::string tstr(1, static_cast<char>(LO(key())));
            if (static_cast<char>(LO(key())) != '+') {
                tstr = "+" + tstr;
                ++is_environment.locate_pos;
            }
            char stale_1st = tstr[0];

            do {
                {
                    PlayerUnlock unlock;
                    tstr = InputStr(tstr, in_x, in_y, 3, 3, macro_input_bckg + macro_input, macro_def_bckg + macro_def);
                }
                is_setting.append_enabled = true;
                if (!tstr.empty()) stale_1st = tstr[0];

                const std::string temps = tstr;
                if (is_end_key(key())) {
                    nope = false;
                    if (!tstr.empty() && tstr[0] == '+') tstr.erase(0, 1);
                    const char f1 = tstr.empty() ? stale_1st : tstr[0];
                    const int n = Str2num(tstr, 10);
                    if (f1 >= '0' && f1 <= '9' && n >= 0 && n <= 96) {
                        nope = true;
                        data() = B(n);
                    } else {
                        tstr = temps;
                    }

                    if (nope) {
                        if (key() == kUP) prev_page(arpeggio_page);
                        else if (key() == kDOWN || key() == kENTER) next_page(arpeggio_page);
                    }
                    if (!nope) tstr = arpeggio_value_str(data(), tstr);
                    if (!tstr.empty()) stale_1st = tstr[0];
                }
            } while (!((nope || key() == kESC) && (key() == kESC || is_end_key(key()))) && !_force_program_quit);

            switch (key()) {
            case kTAB: pos = !arp_vib_mode ? (ptr_vibrato_table != 0 ? 14 : 1) : 14; break;
            case kShTAB: pos = 12; break;
            case kESC: is_environment.keystroke = kENTER; break;
            }
        }
        return false;
    }

    // pos 20: ビブラートの表
    bool edit_vibrato_table()
    {
        GotoXY(B(xstart + 72 + vibrato_hpos - 1 + window_area_inc_x), B(ystart + 16 + window_area_inc_y / 2));
        {
            PlayerUnlock unlock;
            is_environment.keystroke = getkey();
        }
        if (_check_macro_speed_change()) return true;
        general_keys(vib(), &ptr_vibrato_table);

        switch (key()) {
        case kCtLEFT:
            if (!shift_pressed()) pos = !arp_vib_mode ? (ptr_arpeggio_table != 0 ? 13 : 7) : 13;
            else if (vibrato_page > vib().length) vibrato_page = B(std::max<int>(vib().length, 1));
            else vibrato_page = 1;
            break;
        case kCtRGHT:
            if (!shift_pressed()) pos = !arp_vib_mode ? 7 : 13;
            else if (vibrato_page < vib().length) vibrato_page = B(std::max<int>(vib().length, 1));
            else vibrato_page = 255;
            break;

        case kUP: prev_page(vibrato_page); break;
        case kDOWN: next_page(vibrato_page); break;
        case kPgUP: vibrato_page = vibrato_page > 16 ? B(vibrato_page - 16) : 1; break;
        case kPgDOWN: vibrato_page = vibrato_page + 16 < 255 ? B(vibrato_page + 16) : 255; break;
        case kHOME: vibrato_page = 1; break;
        case kEND: vibrato_page = 255; break;
        case kLEFT: vibrato_hpos = vibrato_hpos > 1 ? B(vibrato_hpos - 1) : 3; break;
        case kRIGHT: vibrato_hpos = vibrato_hpos < 3 ? B(vibrato_hpos + 1) : 1; break;

        case kENTER:
        case kTAB:
            pos = !arp_vib_mode ? 1 : 8;
            break;
        case kShTAB: pos = 19; break;

        case kCtrlC:
            clipboard.object_type = !shift_pressed() ? objMacroTableLine : objMacroTableColumn;
            clipboard.mcrtab_type = mttVibrato_table;
            copy_object();
            break;
        case kCtrlV:
        case kAltP:
            paste_object();
            if (clipboard.object_type == objMacroTableLine && clipboard.mcrtab_type == mttVibrato_table) next_page(vibrato_page);
            break;

        case kBkSPC:
            vib().data[vibrato_page - 1] = 0;
            next_page(vibrato_page);
            break;

        case kINSERT: {
            VibratoTable& v = vib();
            for (int temp = 255 - 1; temp >= vibrato_page; --temp) v.data[temp] = v.data[temp - 1];
            v.data[vibrato_page - 1] = 0;
            break;
        }
        case kDELETE: {
            VibratoTable& v = vib();
            for (int temp = vibrato_page; temp <= 255 - 1; ++temp) v.data[temp - 1] = v.data[temp];
            v.data[254] = 0;
            break;
        }
        }

        if (shift_pressed() && (key() == kUP || key() == kDOWN)) {
            fmreg_page = vibrato_page;
            if (ptr_arpeggio_table != 0) arpeggio_page = vibrato_page;
        }

        const char c = UpCase(static_cast<char>(LO(key())));
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || c == '+' || c == '-') {
            VibratoTable& v = vib();
            int8_t& data = v.data[vibrato_page - 1];
            const int prev = v.data[std::max(vibrato_page - 1, 1) - 1];
            const bool positive = data > 0 || (data == 0 && prev >= 0);
            const int h = hex(c);
            bool nope = true;
            switch (vibrato_hpos) {
            case 1:
                if (c == '+') data = static_cast<int8_t>(std::abs(data));
                else if (c == '-') data = static_cast<int8_t>(-std::abs(data));
                else nope = false;
                break;

            case 2:
                if (c >= '0' && c <= '7') {
                    if (positive) data = static_cast<int8_t>((data & 0x0f) + (h << 4));
                    else data = static_cast<int8_t>(-((std::abs(data) & 0x0f) + (h << 4)));
                } else if (c == '+') {
                    if (data < 0x7f) ++data;
                    nope = false;
                } else if (c == '-') {
                    if (data > -0x7f) --data;
                    nope = false;
                } else {
                    data = static_cast<int8_t>(positive ? 0x7f : -0x7f);
                }
                break;

            case 3:
                if (c == '+') {
                    if (data < 0x7f) ++data;
                    nope = false;
                } else if (c == '-') {
                    if (data > -0x7f) --data;
                    nope = false;
                } else if (positive) {
                    data = static_cast<int8_t>((data & 0xf0) + h);
                } else {
                    data = static_cast<int8_t>(-((std::abs(data) & 0xf0) + h));
                }
                break;
            }

            if (nope) {
                switch (vibrato_hpos) {
                case 1:
                case 3:
                    if (command_typing == 2 && c != '-' && c != '+' && vibrato_hpos == 3) --vibrato_hpos;
                    next_page(vibrato_page);
                    break;
                case 2:
                    if (command_typing != 2 || c == '-' || c == '+') next_page(vibrato_page);
                    else ++vibrato_hpos;
                    break;
                }
            }
        }
        return false;
    }

    // --- 試聴 (Space / Ctrl+Space) ---

    // _macro_preview_init / _macro_preview_body の作業用
    uint8_t _m_chan_handle[21];   // [1..18]。オリジナルはパーカッションで [16..20] にも書くため広げてある
    uint8_t _m_channels = 0;
    uint8_t _m_flag_4op = 0;
    decltype(A2Player::event_table) _m_event_table_bak;
    decltype(A2Player::freq_table) _m_freq_table_bak, _m_freqtable2_bak;
    decltype(A2Player::keyoff_loop) _m_keyoff_loop_bak;
    decltype(A2Player::channel_flag) _m_channel_flag_backup;
    decltype(A2Player::fmpar_table) _m_fmpar_table_backup;
    decltype(A2Player::volume_table) _m_volume_table_backup;
    decltype(A2Player::pan_lock) _m_pan_lock_backup;
    decltype(A2Player::volume_lock) _m_volume_lock_backup;
    decltype(A2Player::peak_lock) _m_peak_lock_backup;
    decltype(A2Player::panning_table) _m_panning_table_backup;
    bool _m_status_backup_replay_forbidden = false;
    PlayStatus _m_status_backup_play_status = PlayStatus::Stopped;
    uint8_t _bak_arpeggio_table = 0, _bak_vibrato_table = 0;
    uint8_t _bak_common_flag = 0;
    bool _bak_volume_scaling = false;
    uint8_t _bak_current_inst = 1;
    bool _4op_mode = false;

    bool _1op_preview_active()
    {
        uint8_t nm_slots = 0;
        for (int temp = 1; temp <= 4; ++temp) {
            if (_operator_enabled[temp]) ++nm_slots;
        }
        return nm_slots == 1;
    }

    // state = 1: 試聴の準備 (プレイヤーの状態を退避して止める)、state = 0: 元に戻す
    void _macro_preview_init(uint8_t state, uint8_t instr2)
    {
        A2Player& p = P();
        FixedSongData& sd = songdata();
        _4op_mode = sd.flag_4op != 0 && instr2 != BYTE_NULL;

        switch (state) {
        case 0:
            std::memcpy(p.fmpar_table, _m_fmpar_table_backup, sizeof(p.fmpar_table));
            std::memcpy(p.volume_table, _m_volume_table_backup, sizeof(p.volume_table));
            std::memcpy(p.panning_table, _m_panning_table_backup, sizeof(p.panning_table));
            at1(sd.instr_macros, _bak_current_inst).arpeggio_table = _bak_arpeggio_table;
            at1(sd.instr_macros, _bak_current_inst).vibrato_table = _bak_vibrato_table;
            sd.common_flag = _bak_common_flag;
            p.volume_scaling = _bak_volume_scaling;
            p.reset_player();

            std::memcpy(p.channel_flag, _m_channel_flag_backup, sizeof(p.channel_flag));
            std::memcpy(p.event_table, _m_event_table_bak, sizeof(p.event_table));
            std::memcpy(p.pan_lock, _m_pan_lock_backup, sizeof(p.pan_lock));
            std::memcpy(p.volume_lock, _m_volume_lock_backup, sizeof(p.volume_lock));
            std::memcpy(p.peak_lock, _m_peak_lock_backup, sizeof(p.peak_lock));

            really_no_status_refresh = false;
            std::memcpy(p.freq_table, _m_freq_table_bak, sizeof(p.freq_table));
            std::memcpy(p.freqtable2, _m_freqtable2_bak, sizeof(p.freqtable2));
            std::memcpy(p.keyoff_loop, _m_keyoff_loop_bak, sizeof(p.keyoff_loop));
            std::memset(p.macro_table, 0, sizeof(p.macro_table));
            p.replay_forbidden = _m_status_backup_replay_forbidden;
            p.play_status = _m_status_backup_play_status;
            break;

        case 1:
            _m_status_backup_replay_forbidden = p.replay_forbidden;
            _m_status_backup_play_status = p.play_status;
            p.replay_forbidden = true;
            if (p.play_status != PlayStatus::Stopped) p.play_status = PlayStatus::Paused;
            p.nul_volume_bars();
            really_no_status_refresh = true;
            p.reset_player();

            std::memset(_m_chan_handle, 0, sizeof(_m_chan_handle));
            std::memcpy(_m_channel_flag_backup, p.channel_flag, sizeof(p.channel_flag));
            std::memcpy(_m_event_table_bak, p.event_table, sizeof(p.event_table));
            std::fill(std::begin(p.channel_flag), std::end(p.channel_flag), true);
            std::memcpy(_m_pan_lock_backup, p.pan_lock, sizeof(p.pan_lock));
            std::memcpy(_m_volume_lock_backup, p.volume_lock, sizeof(p.volume_lock));
            std::memcpy(_m_peak_lock_backup, p.peak_lock, sizeof(p.peak_lock));
            std::memcpy(_m_panning_table_backup, p.panning_table, sizeof(p.panning_table));
            std::fill(std::begin(p.pan_lock), std::end(p.pan_lock), false);
            std::fill(std::begin(p.volume_lock), std::end(p.volume_lock), false);
            std::fill(std::begin(p.peak_lock), std::end(p.peak_lock), false);
            _m_flag_4op = sd.flag_4op;
            if (!p.percussion_mode && !(sd.flag_4op != 0)) {
                _m_channels = 18;
            } else if (!(sd.flag_4op != 0)) {
                _m_channels = 15;
            } else if (_4op_mode && !_1op_preview_active()) {
                _m_flag_4op = 0x3f;
                _m_channels = 6;
            } else {
                _m_flag_4op = 0;
                _m_channels = !p.percussion_mode ? 18 : 15;
            }

            _bak_arpeggio_table = at1(sd.instr_macros, current_inst).arpeggio_table;
            _bak_vibrato_table = at1(sd.instr_macros, current_inst).vibrato_table;
            _bak_common_flag = sd.common_flag;
            _bak_volume_scaling = p.volume_scaling;
            _bak_current_inst = current_inst;
            at1(sd.instr_macros, current_inst).arpeggio_table = ptr_arpeggio_table;
            at1(sd.instr_macros, current_inst).vibrato_table = ptr_vibrato_table;
            sd.common_flag = B(sd.common_flag & ~0x80);
            p.volume_scaling = false;
            p.reset_player();

            std::memcpy(_m_fmpar_table_backup, p.fmpar_table, sizeof(p.fmpar_table));
            std::memcpy(_m_volume_table_backup, p.volume_table, sizeof(p.volume_table));
            std::memcpy(_m_freq_table_bak, p.freq_table, sizeof(p.freq_table));
            std::memcpy(_m_freqtable2_bak, p.freqtable2, sizeof(p.freqtable2));
            std::memcpy(_m_keyoff_loop_bak, p.keyoff_loop, sizeof(p.keyoff_loop));
            std::fill(std::begin(p.keyoff_loop), std::end(p.keyoff_loop), false);

            p.misc_register = B((p.current_tremolo_depth << 7) + (p.current_vibrato_depth << 6) + (int(p.percussion_mode) << 5));

            p.opl3out(0x01, 0x20);
            p.opl3out(0x08, 0x40);
            p.opl3out(0x105, 0x01);           // opl3exp($0105)
            p.opl3out(0x104, _m_flag_4op);    // opl3exp($04+_m_flag_4op SHL 8)

            p.key_off(17);
            p.key_off(18);
            p.opl3out(0xbd, p.misc_register);

            p.macro_speedup = sd.macro_speedup;
            if (p.play_status == PlayStatus::Stopped) p.update_timer(sd.tempo);
            break;
        }
    }

    // MIDI ボードの鍵盤を押している間、マクロを動かしながら楽器を鳴らす。ロックを取った状態で呼ぶ
    void _macro_preview_body(uint8_t instr_, uint8_t instr2, uint8_t /*chan*/, uint16_t fkey)
    {
        if (ctrl_pressed() || alt_pressed() || shift_pressed()) return;

        A2Player& p = P();
        FixedSongData& sd = songdata();
        const bool _1op = _1op_preview_active();

        // 鍵盤 board_pos の音を chan (_m_chan_handle の位置) で鳴らす
        const auto output_note = [&](uint8_t chan, uint8_t board_pos) {
            const int note = board_pos + 12 * (current_octave - 1);
            if (!(note >= 0 && note <= 12 * 8 + 1)) return false;

            _m_chan_handle[chan] = board_scancodes[board_pos - 1];
            // オリジナルは _4op_main_chan[1..6] の範囲外 (パーカッションの 16..20) も読むが、ここでは変えない
            if (_4op_mode && chan <= 6) chan = _4op_main_chan[chan - 1];

            if (_1op) {
                Instrument ins = (_operator_enabled[1] || _operator_enabled[2]) ? at1(sd.instr_data, instr_) : at1(sd.instr_data, instr2);
                ins.fm_data.FEEDBACK_FM |= 1;
                p.load_instrument(reinterpret_cast<const uint8_t*>(&ins), chan);
                const uint16_t vol = p.volume_table[chan];
                if (_operator_enabled[1] || _operator_enabled[2]) {
                    p.set_ins_volume(B(0x3f - int(_operator_enabled[1]) * (0x3f - LO(vol))),
                                     B(0x3f - int(_operator_enabled[2]) * (0x3f - HI(vol))), chan);
                } else {
                    p.set_ins_volume(B(0x3f - int(_operator_enabled[3]) * (0x3f - LO(vol))),
                                     B(0x3f - int(_operator_enabled[4]) * (0x3f - HI(vol))), chan);
                }
            } else {
                p.load_instrument(instr_data_bytes(instr_), chan);
                const uint16_t vol = p.volume_table[chan];
                p.set_ins_volume(B(0x3f - int(_operator_enabled[1]) * (0x3f - LO(vol))),
                                 B(0x3f - int(_operator_enabled[2]) * (0x3f - HI(vol))), chan);
                const uint8_t perc_voice = at1(sd.instr_data, instr_).perc_voice;
                if (p.percussion_mode && (perc_voice == 4 || perc_voice == 5) && (chan == 19 || chan == 20))
                    p.load_instrument(instr_data_bytes(instr_), _perc_sim_chan[chan - 19]);
                if (_4op_mode) {
                    p.load_instrument(instr_data_bytes(instr2), B(chan - 1));
                    const uint16_t vol2 = p.volume_table[chan - 1];
                    p.set_ins_volume(B(0x3f - int(_operator_enabled[3]) * (0x3f - LO(vol2))),
                                     B(0x3f - int(_operator_enabled[4]) * (0x3f - HI(vol2))), B(chan - 1));
                }
            }

            const uint16_t freq = static_cast<uint16_t>(A2Player::nFreq(B(note - 1)) + 0x2000 +
                                                        static_cast<int8_t>(ins_byte(instr_, 12)));
            p.event_table[chan].note = B(note);
            p.freq_table[chan] = freq;
            p.freqtable2[chan] = freq;
            p.key_on(chan);
            p.change_freq(chan, freq);

            if (!(_1op && (_operator_enabled[3] || _operator_enabled[4]))) p.init_macro_table(chan, B(note), instr_, freq);
            else p.init_macro_table(chan, B(note), instr2, freq);

            if (_4op_mode && !_1op) p.init_macro_table(B(chan - 1), B(note), instr2, freq);
            return true;
        };

        // 鍵盤を離すまで画面を更新し続ける
        const auto poll_screen = []() {
            PlayerUnlock unlock;
            keyboard_poll_input();
            keyboard_reset_buffer();
            draw_screen();
        };

        bool valid_key = false;
        int temp = 1;
        for (; temp <= 29; ++temp) {
            if (!shift_pressed() && board_scancodes[temp - 1] == HI(fkey)) {
                valid_key = true;
                break;
            }
        }
        if (!valid_key || !in_range(temp + 12 * (current_octave - 1) - 1, 0, 12 * 8 + 1)) return;

        uint8_t temp2 = B(temp);
        const uint8_t perc_voice = at1(sd.instr_data, instr_).perc_voice;
        if (p.percussion_mode && in_range(perc_voice, 1, 5)) {
            output_note(B(perc_voice + 15), temp2);
            while (scankey(board_scancodes[temp2 - 1]) && !_force_program_quit) poll_screen();
        } else {
            do {
                {
                    PlayerUnlock unlock;
                    keyboard_poll_input();
                }
                valid_key = false;
                for (temp = 1; temp <= 29; ++temp) {
                    temp2 = board_scancodes[temp - 1];
                    const bool temp4 = scankey(temp2);
                    uint8_t temp3, temp5;
                    if (!_4op_mode) {
                        temp3 = get_chanpos(_m_chan_handle + 1, _m_channels, temp2);
                        temp5 = get_chanpos(_m_chan_handle + 1, _m_channels, 0);
                    } else {
                        temp3 = get_chanpos2(_m_chan_handle + 1, _m_channels, temp2);
                        temp5 = get_chanpos2(_m_chan_handle + 1, _m_channels, 0);
                    }

                    if (temp4) valid_key = true;
                    if (temp4 && temp3 == 0 && temp5 != 0) output_note(temp5, B(temp));

                    if (!temp4 && temp3 != 0) {
                        p.key_off(temp3);
                        _m_chan_handle[temp3] = 0;
                    }
                }
                poll_screen();
            } while (valid_key && !_force_program_quit);
        }
    }

    void _macro_preview_init_cur(uint8_t state)
    {
        if (get_4op_to_test() != 0) _macro_preview_init(state, B(~BYTE_NULL));
        else _macro_preview_init(state, BYTE_NULL);
    }

    // Space / Ctrl+Space: 試聴の状態に入り、Space か ESC で抜ける (ESC はそのままエディタも閉じる)
    void macro_preview()
    {
        A2Player& p = P();
        FixedSongData& sd = songdata();

        refresh(0);
        HideCursor();
        _pip_dest = screen_ptr;
        _macro_preview_init_cur(1);

        if (ctrl_pressed() && key() == kSPACE) _pip_loop = true;
        std::fill(std::begin(p.keyoff_loop) + 1, std::begin(p.keyoff_loop) + 21, _pip_loop);

        macro_preview_indic_proc = _preview_indic_proc;
        is_environment.keystroke = WORD_NULL;

        if (!_force_program_quit) {
            do {
                // オクターブの表示 (オリジナルのまま 30 行目に書く)
                for (int temp = 1; temp <= 8; ++temp) {
                    show_str(B(30 + temp), 30, std::string(1, static_cast<char>(48 + temp)),
                             main_background + (temp != current_octave ? main_stat_line : main_hi_stat_line));
                }

                bool pressed;
                {
                    PlayerUnlock unlock;
                    pressed = keypressed();
                    if (pressed) is_environment.keystroke = getkey();
                }
                if (_force_program_quit) break;

                if (pressed) {
                    const uint16_t k = key();
                    switch (k) {
                    case kF7:
                        for (int temp = 1; temp <= 20; ++temp) p.reset_chan_data(B(temp));
                        break;

                    case kCtLbr:
                        if (shift_pressed()) {
                            if (sd.macro_speedup > 1) --sd.macro_speedup;
                            p.macro_speedup = sd.macro_speedup;
                            p.reset_player();
                        } else if (current_inst > 1) {
                            --current_inst;
                            if (marked_instruments() != 2) reset_marked_instruments();
                            instrum_page = current_inst;
                            std::fill(std::begin(_operator_enabled), std::end(_operator_enabled), true);
                            STATUS_LINE_refresh();
                            p.reset_player();
                        }
                        break;

                    case kCtRbr:
                        if (shift_pressed()) {
                            ++sd.macro_speedup;
                            if (calc_max_speedup(sd.tempo) < sd.macro_speedup) sd.macro_speedup = calc_max_speedup(sd.tempo);
                            p.macro_speedup = sd.macro_speedup;
                            p.reset_player();
                        } else if (current_inst < 255) {
                            ++current_inst;
                            if (marked_instruments() != 2) reset_marked_instruments();
                            instrum_page = current_inst;
                            std::fill(std::begin(_operator_enabled), std::end(_operator_enabled), true);
                            STATUS_LINE_refresh();
                            p.reset_player();
                        }
                        break;

                    case kAlt0: std::fill(std::begin(_operator_enabled), std::end(_operator_enabled), true); break;
                    case kAlt1: _set_operator_flag(1, shift_pressed()); break;
                    case kAlt2: _set_operator_flag(2, shift_pressed()); break;
                    case kAlt3: _set_operator_flag(3, shift_pressed()); break;
                    case kAlt4: _set_operator_flag(4, shift_pressed()); break;
                    }

                    if (k == kCtLbr || k == kCtRbr || k == kAlt0 || k == kAlt1 || k == kAlt2 || k == kAlt3 || k == kAlt4) {
                        keyboard_reset_buffer();
                        if (!arp_vib_mode) {
                            im().arpeggio_table = ptr_arpeggio_table;
                            im().vibrato_table = ptr_vibrato_table;
                        }

                        _macro_preview_init_cur(0);
                        instr = current_inst;

                        if (!arp_vib_mode) {
                            ptr_arpeggio_table = im().arpeggio_table;
                            ptr_vibrato_table = im().vibrato_table;
                        }

                        if (arp_vib_mode && pos < 8) pos = 8;
                        else if (!arp_vib_mode && ((ptr_arpeggio_table == 0 && in_range(pos, 8, 13)) ||
                                                   (ptr_vibrato_table == 0 && in_range(pos, 14, 20))))
                            pos = 1;

                        ShowStr(centered_frame_vdest, B(xstart + (!arp_vib_mode ? 54 : 57) + window_area_inc_x / 2), ystart,
                                byte2hex(instr), macro_background + dialog_title);

                        refresh(flag_FMREG + flag_ARPEGGIO + flag_VIBRATO);
                        _macro_preview_init_cur(1);
                    }

                    const uint16_t t4 = get_4op_to_test();
                    if (t4 != 0) _macro_preview_body(LO(t4), HI(t4), count_channel(pattern_hpos), key());
                    else _macro_preview_body(instrum_page, BYTE_NULL, count_channel(pattern_hpos), key());

                    if (ctrl_pressed() && !shift_pressed() && key() == kSPACE) {
                        _pip_loop = !_pip_loop;
                        std::fill(std::begin(p.keyoff_loop) + 1, std::begin(p.keyoff_loop) + 21, _pip_loop);
                        is_environment.keystroke = WORD_NULL;
                    }

                    if (shift_pressed() && key() == kSPACE) is_environment.keystroke = WORD_NULL;
                }

                {
                    PlayerUnlock unlock;
                    draw_screen();
                }
            } while (key() != kSPACE && key() != kESC && !_force_program_quit);
        }

        _macro_preview_init_cur(0);
        macro_preview_indic_proc = nullptr;
        _pip_dest = temp_screen;
        if (use_large_cursor) WideCursor();
        else ThinCursor();
    }
}   // namespace macroedit

void FILE_save(const wchar_t* ext, bool quick);
uint8_t INSTRUMENT_CONTROL_alt(uint8_t instr, const std::string& title);

// Ctrl+Q / Ctrl+G: マクロエディタ (arp_vib_mode = TRUE はアルペジオ / ビブラートのマクロだけを編集する)
void MACRO_EDITOR(uint8_t instr_, bool arp_vib_mode_)
{
    using namespace macroedit;
    static const uint16_t new_keys[38] = { kF1, kESC, kENTER, kSPACE, kTAB, kShTAB, kUP, kDOWN, kCtrlO, kF2, kCtrlF2,
                                           kF3, kCtrlL, kCtrlS, kCtrlM, kCtENTR, kAltC, kAltP, kCtrlC, kCtrlV, kCtPgUP,
                                           kCtPgDN, kSPACE, kCHplus, kNPplus, kCHmins, kNPmins, kCtLbr, kCtRbr, kAlt0,
                                           kAlt1, kAlt2, kAlt3, kAlt4, kCtHOME, kCtEND, kCtLEFT, kCtRGHT };

    PlayerLock lock;
    instr = instr_;
    arp_vib_mode = arp_vib_mode_;
    _arp_vib_mode = arp_vib_mode;
    const int av = arp_vib_mode ? 1 : 0;

    if (is_default_screen_mode()) {
        window_area_inc_x = 0;
        window_area_inc_y = 0;
    } else {
        window_area_inc_x = arp_vib_mode ? 0 : 26;
        window_area_inc_y = program_screen_mode == 1 ? 10 : 20;
    }
    const int ix = window_area_inc_x;
    const int iy = window_area_inc_y;

    call_pickup_proc = false;
    _source_ins = instrum_page;
    call_pickup_proc2 = false;
    _source_ins2 = instrum_page;

    // 楽器名の 1 文字目 (マーク) を除いた曲データの CRC
    const auto songdata_crc_without_marks = []() {
        char temp_marks[256];
        for (int temp = 1; temp <= 255; ++temp) {
            temp_marks[temp] = instr_name_1st_char(temp);
            instr_name_1st_char(temp) = ' ';
        }
        const uint32_t crc = Update32(&songdata(), sizeof(FixedSongData), 0);
        for (int temp = 1; temp <= 255; ++temp) instr_name_1st_char(temp) = temp_marks[temp];
        return crc;
    };

    // 上段の表の番号と、表がないときのカーソル位置
    const auto sync_tables = []() {
        if (!arp_vib_mode) {
            ptr_arpeggio_table = im().arpeggio_table;
            ptr_vibrato_table = im().vibrato_table;
        }
        if (arp_vib_mode && pos < 8) pos = 8;
        else if (!arp_vib_mode && ((ptr_arpeggio_table == 0 && in_range(pos, 8, 13)) || (ptr_vibrato_table == 0 && in_range(pos, 14, 20))))
            pos = 1;
    };

    const auto show_instr_number = []() {
        ShowStr(centered_frame_vdest, B(xstart + (!arp_vib_mode ? 54 : 57) + window_area_inc_x / 2), ystart, byte2hex(instr),
                macro_background + dialog_title);
    };

    for (;;) {   // _jmp1
        if (!arp_vib_mode) {
            ptr_arpeggio_table = im().arpeggio_table;
            ptr_vibrato_table = im().vibrato_table;
        } else {
            ptr_arpeggio_table = arpvib_arpeggio_table;
            ptr_vibrato_table = arpvib_vibrato_table;
        }
        pos = _macro_editor__pos[av];
        sync_tables();
        fmreg_hpos = _macro_editor__fmreg_hpos[av];
        fmreg_page = _macro_editor__fmreg_page[av];
        fmreg_left_margin = _macro_editor__fmreg_left_margin[av];
        fmreg_cursor_pos = _macro_editor__fmreg_cursor_pos[av];
        arpeggio_page = _macro_editor__arpeggio_page[av];
        vibrato_hpos = _macro_editor__vibrato_hpos[av];
        vibrato_page = _macro_editor__vibrato_page[av];
        if (_force_program_quit) return;

        ScreenMemCopy(screen_ptr, screen_backup);
        HideCursor();
        songdata_crc = songdata_crc_without_marks();

        ScreenMemCopy(screen_ptr, temp_screen);
        centered_frame_vdest = temp_screen;
        centered_frame(xstart, ystart, B(81 + ix), B(24 + iy),
                       !arp_vib_mode ? " iNSTRUMENT MACRO EDiTOR (iNS_  ) " : " ARPEGGiO/ViBRATO MACRO EDiTOR (iNS_  ) ",
                       macro_background + dialog_border, macro_background + dialog_title, frame_double);

        _pip_xloc = B(xstart + 30 + ix / 2);
        _pip_yloc = B(ystart + 2);
        _pip_dest = temp_screen;

        move_to_screen_data = temp_screen;
        move_to_screen_area[1] = xstart;
        move_to_screen_area[2] = ystart;
        move_to_screen_area[3] = B(xstart + 81 + 2 + ix);
        move_to_screen_area[4] = B(ystart + 24 + 1 + iy);
        refresh(flag_FMREG + flag_ARPEGGIO + flag_VIBRATO);

        move_to_screen_area[1] = B(xstart + 1);
        move_to_screen_area[2] = B(ystart + 1);
        move_to_screen_area[3] = B(xstart + 80 + ix);
        move_to_screen_area[4] = B(ystart + 24 + iy);

        if (pos == 1) GotoXY(B(xstart + 17), B(ystart + 4));
        if (use_large_cursor) WideCursor();
        else ThinCursor();
        centered_frame_vdest = screen_ptr;

        uint16_t old_keys[38];
        std::memcpy(old_keys, is_setting.terminate_keys, sizeof(old_keys));
        std::memcpy(is_setting.terminate_keys, new_keys, sizeof(new_keys));

        uint8_t old_instr = instr;
        uint8_t old_pos = pos;
        uint8_t old_arp_ptr = ptr_arpeggio_table;
        uint8_t old_vib_ptr = ptr_vibrato_table;
        uint8_t old_fmreg_page = fmreg_page;
        uint8_t old_arpeggio_page = arpeggio_page;
        uint8_t old_vibrato_page = vibrato_page;

        const auto terminated = []() {
            const uint16_t k = key();
            return k == kESC || k == kCtrlO || k == kF1 || k == kF2 || k == kCtrlF2 || k == kF3 || k == kCtrlL ||
                   k == kCtrlS || k == kCtrlM || (!ctrl_pressed() && k == kAltC) || call_pickup_proc || call_pickup_proc2;
        };

        bool jmp2;
        do {   // _jmp2
            jmp2 = false;
            if (!arp_vib_mode) {
                im().arpeggio_table = ptr_arpeggio_table;
                im().vibrato_table = ptr_vibrato_table;
            }
            if (instr != current_inst) instr = current_inst;
            sync_tables();
            show_instr_number();

            if (_force_program_quit) break;
            do {
                if (arp_vib_mode) {
                    refresh(flag_ARPEGGIO + flag_VIBRATO);
                } else {
                    uint8_t refresh_flag = 0;
                    if (in_range(pos, 1, 7)) {
                        refresh_flag = flag_FMREG;
                        if (in_range(old_pos, 8, 13) || old_arpeggio_page != arpeggio_page || old_arp_ptr != ptr_arpeggio_table ||
                            old_instr != instr)
                            refresh_flag += flag_ARPEGGIO;
                        if (in_range(old_pos, 14, 20) || old_vibrato_page != vibrato_page || old_vib_ptr != ptr_vibrato_table ||
                            old_instr != instr)
                            refresh_flag += flag_VIBRATO;
                    } else if (in_range(pos, 8, 13)) {
                        refresh_flag = flag_ARPEGGIO;
                        if (in_range(old_pos, 1, 7) || old_fmreg_page != fmreg_page || old_instr != instr) refresh_flag += flag_FMREG;
                        if (in_range(old_pos, 14, 20) || old_vibrato_page != vibrato_page || old_vib_ptr != ptr_vibrato_table ||
                            old_instr != instr)
                            refresh_flag += flag_VIBRATO;
                    } else if (in_range(pos, 14, 20)) {
                        refresh_flag = flag_VIBRATO;
                        if (in_range(old_pos, 1, 7) || old_fmreg_page != fmreg_page || old_instr != instr) refresh_flag += flag_FMREG;
                        if (in_range(old_pos, 8, 13) || old_arpeggio_page != arpeggio_page || old_arp_ptr != ptr_arpeggio_table ||
                            old_instr != instr)
                            refresh_flag += flag_ARPEGGIO;
                    }

                    old_instr = instr;
                    old_pos = pos;
                    old_arp_ptr = ptr_arpeggio_table;
                    old_vib_ptr = ptr_vibrato_table;
                    old_fmreg_page = fmreg_page;
                    old_arpeggio_page = arpeggio_page;
                    old_vibrato_page = vibrato_page;
                    refresh(refresh_flag);
                }

                is_setting.append_enabled = true;
                is_environment.locate_pos = 1;

                if (in_range(pos, 1, 6)) jmp2 = edit_fmreg_header();
                else if (pos == 7) jmp2 = edit_fmreg_table();
                else if (in_range(pos, 8, 12)) jmp2 = edit_arpeggio_header();
                else if (pos == 13) jmp2 = edit_arpeggio_table();
                else if (in_range(pos, 14, 19)) jmp2 = edit_vibrato_header();
                else if (pos == 20) jmp2 = edit_vibrato_table();
                if (jmp2) break;

                // Space / Ctrl+Space: マクロの試聴
                if (!shift_pressed() && key() == kSPACE) macro_preview();

                {
                    PlayerUnlock unlock;
                    draw_screen();
                }
            } while (!terminated() && !_force_program_quit);
        } while (jmp2 && !_force_program_quit);

        std::fill(std::begin(macroedit::_operator_enabled), std::end(macroedit::_operator_enabled), true);
        _macro_editor__pos[av] = pos;
        _macro_editor__fmreg_hpos[av] = fmreg_hpos;
        _macro_editor__fmreg_page[av] = fmreg_page;
        _macro_editor__fmreg_left_margin[av] = fmreg_left_margin;
        _macro_editor__fmreg_cursor_pos[av] = fmreg_cursor_pos;
        _macro_editor__arpeggio_page[av] = arpeggio_page;
        _macro_editor__vibrato_hpos[av] = vibrato_hpos;
        _macro_editor__vibrato_page[av] = vibrato_page;

        if (!arp_vib_mode) {
            im().arpeggio_table = ptr_arpeggio_table;
            im().vibrato_table = ptr_vibrato_table;
        } else {
            if (shift_pressed()) {
                at1(songdata().instr_macros, current_inst).arpeggio_table = ptr_arpeggio_table;
                at1(songdata().instr_macros, current_inst).vibrato_table = ptr_vibrato_table;
            }
            arpvib_arpeggio_table = ptr_arpeggio_table;
            arpvib_vibrato_table = ptr_vibrato_table;
        }

        if (songdata_crc_without_marks() != songdata_crc) module_archived = false;

        HideCursor();
        std::memcpy(is_setting.terminate_keys, old_keys, sizeof(old_keys));
        move_to_screen_data = screen_backup;
        move_to_screen_area[1] = xstart;
        move_to_screen_area[2] = ystart;
        move_to_screen_area[3] = B(xstart + 81 + 2 + ix);
        move_to_screen_area[4] = B(ystart + 24 + 1 + ix);   // オリジナルのまま (window_area_inc_y ではない)
        move2screen();

        if (_force_program_quit) break;

        const uint16_t k = key();
        if (k == kAltC && !ctrl_pressed()) {
            std::vector<std::string> items(std::begin(copy_menu_str4), std::end(copy_menu_str4));
            if (pos == 7 || pos == 13 || pos == 20) {
                items[12] = copy_macro_str[1];
                items[13] = copy_macro_str[3];
            } else {
                items[12] = copy_macro_str[0];
                items[13] = copy_macro_str[2];
            }
            mn_setting.cycle_moves = true;
            uint16_t temp;
            {
                PlayerUnlock unlock;
                temp = Menu(items, 1, 1, copypos4, 30, 15, 15, " COPY OBJECT ");
            }
            if (mn_environment.keystroke != kESC) {
                copypos4 = B(temp);
                clipboard.object_type = static_cast<tCOPY_OBJECT>(temp);
                if (in_range(pos, 1, 7)) clipboard.mcrtab_type = mttFM_reg_table;
                else if (in_range(pos, 8, 13)) clipboard.mcrtab_type = mttArpeggio_table;
                else if (in_range(pos, 14, 20)) clipboard.mcrtab_type = mttVibrato_table;
                copy_object();
            }
            continue;
        }

        if (k == kENTER && call_pickup_proc2) {
            // Shift+Enter: この行を他の楽器へ写す
            call_pickup_proc2 = false;
            uint8_t temp;
            {
                PlayerUnlock unlock;
                temp = INSTRUMENT_CONTROL_alt(_source_ins2, "FM-REGiSTER MACRO: PASTE DATA TO iNSTRUMENT");
            }
            if (temp != 0) {
                _source_ins2 = temp;
                Instrument& dst = at1(songdata().instr_data, _source_ins2);
                const RegisterTableDef& d = im().data[fmreg_page - 1];
                dst.fm_data = d.fm_data;
                dst.panning = d.panning;
                dst.fine_tune = at1(songdata().instr_data, instr).fine_tune;
                dst.perc_voice = at1(songdata().instr_data, instr).perc_voice;
                dst.fm_data.FEEDBACK_FM &= 0x3f;
                invert_volume(dst.fm_data);
            }
            continue;
        }

        if (k == kCtENTR && call_pickup_proc) {
            // Ctrl+Shift+Enter: 他の楽器の値をこの行へ写す
            call_pickup_proc = false;
            uint8_t temp;
            {
                PlayerUnlock unlock;
                temp = INSTRUMENT_CONTROL_alt(_source_ins, "FM-REGiSTER MACRO: PASTE DATA FROM iNSTRUMENT");
            }
            if (temp != 0) {
                _source_ins = temp;
                const Instrument& src = at1(songdata().instr_data, _source_ins);
                RegisterTableDef& d = im().data[fmreg_page - 1];
                const uint8_t fb = d.fm_data.FEEDBACK_FM;
                d.fm_data = src.fm_data;
                d.fm_data.FEEDBACK_FM = B((fb & 0xc0) + (src.fm_data.FEEDBACK_FM & 0x3f));
                d.panning = src.panning;
                d.duration = B(std::max<int>(d.duration, 1));
                invert_volume(d.fm_data);
                next_page(fmreg_page);
            }
            continue;
        }

        if (k == kF2 || k == kCtrlS) {
            PlayerUnlock unlock;
            FILE_save(!arp_vib_mode ? L"a2f" : L"a2w", false);
            continue;
        }

        if (k == kCtrlF2) {
            if (!arp_vib_mode) {
                PlayerUnlock unlock;
                FILE_save(L"a2w", false);
            }
            continue;
        }

        if (k == kF3 || k == kCtrlL) {
            // FM レジスタのマクロの位置なら楽器ファイル・楽器バンク、アルペジオ / ビブラートの位置なら A2W だけを選ぶ
            _arp_vib_loader = !(pos < 8);
            if (_arp_vib_loader) {
                arp_tab_selected = in_range(pos, 8, 13);
                vib_tab_selected = in_range(pos, 14, 20);
                if (!arp_vib_mode) {
                    arpvib_arpeggio_table = ptr_arpeggio_table;
                    arpvib_vibrato_table = ptr_vibrato_table;
                }
            } else {
                arp_tab_selected = ptr_arpeggio_table != 0;
                vib_tab_selected = ptr_vibrato_table != 0;
                if (!(arp_tab_selected || vib_tab_selected)) {
                    arp_tab_selected = true;
                    vib_tab_selected = true;
                }
            }
            {
                PlayerUnlock unlock;
                FILE_open_instrument(false, false, _arp_vib_loader);   // load bank is not possible
            }
            update_instr_data(instrum_page);
            continue;
        }

        if (k == kCtrlM) {
            PlayerUnlock unlock;
            MACRO_BROWSER(pos < 8, false);
            continue;
        }

        if (k == kCtrlO || k == kF1) {
            // オクターブ設定 (Ctrl+O)・ヘルプ (F1) は 6f で実装する。開き直す
            continue;
        }
        break;
    }
    _pip_loop = false;
}
