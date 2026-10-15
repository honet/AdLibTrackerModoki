// パターンを編集する道具 (adt2extn.pas の TRANSPOSE / REMAP / REARRANGE / REPLACE / LINE_MARKING_SETUP / OCTAVE_CONTROL の移植)
//
// MainScreen.cpp から #include する (MainScreen.cpp の無名名前空間の中で使う)。
// どれもロックを取らない状態で呼び、曲データを書き換えるときにだけロックを取る。

// count_patterns: 空でない最後のパターン + 1 (ロックを取った状態で呼ぶ)
uint8_t count_patterns()
{
    const PatternData& pd = P().song()->patterns();
    uint8_t patterns = 0;
    for (int temp1 = 0; temp1 < 128; ++temp1) {
        if (!is_empty_data(&pd.block[temp1 / 8][temp1 % 8], sizeof(pd.block[0][0]))) patterns = B(temp1 + 1);
    }
    return patterns;
}

// adt2unit.pas の calc_bpm_speed (sdl_timer_slowdown = 2)
double calc_bpm_speed(uint8_t tempo, uint8_t speed, uint8_t rows_per_beat)
{
    const A2Player& p = P();
    if (speed == 0 || rows_per_beat == 0 || p.IRQ_freq == 0) return 0;
    return tempo * 60.0 / speed / rows_per_beat * (1 + static_cast<double>(p.IRQ_freq_shift) / p.IRQ_freq - 2 / 100.0);
}

std::string ExpC2StrL(std::string str, uint8_t size, char chr)
{
    while (CStr2Len(str) < size) str = chr + str;
    return str;
}

// --- 行の印の設定 (Alt+L) ---

// realtime_gfx_poll_proc から呼ぶ (ロックを取った状態)
void _show_bpm_callback_LMS()
{
    const FixedSongData& sd = songdata();
    ShowC3Str(screen_ptr, B(dl_environment.xpos + 2), B(dl_environment.ypos + dl_environment.ysize),
              ExpC2StrL(" ~ " + Bpm2str(calc_bpm_speed(sd.tempo, sd.speed, dl_environment.cur_item)) + " ~`BPM `",
                        B(dl_environment.xsize - 2), '\xCD') + " ",
              dialog_background + dialog_border, dialog_def_bckg + dialog_input, dialog_def_bckg + dialog_input);
}

void LINE_MARKING_SETUP()
{
    void (*old_bpm_proc)();
    uint8_t mark_line;
    {
        PlayerLock lock;
        old_bpm_proc = _show_bpm_realtime_proc;
        _show_bpm_realtime_proc = _show_bpm_callback_LMS;
        mark_line = P().mark_line;
    }
    dl_setting.all_enabled = true;
    mark_line = Dialog("USE CURSOR KEYS OR DiRECTLY PRESS ~HOTKEY~ TO SETUP COUNTER$",
                       "~1~$~2~$~3~$~4~$~5~$~6~$~7~$~8~$~9~$10$11$12$13$14$15$16$",
                       " LiNE MARKiNG SETUP (ROWS PER BEAT) ", mark_line);
    dl_setting.all_enabled = false;
    PlayerLock lock;
    P().mark_line = mark_line;
    _IRQFREQ_update_event = false;
    _show_bpm_realtime_proc = old_bpm_proc;
}

// --- オクターブの設定 (Ctrl+O) ---

void OCTAVE_CONTROL()
{
    current_octave = Dialog("USE CURSOR KEYS OR DiRECTLY PRESS HOTKEY TO CHANGE OCTAVE$",
                            "~1~$~2~$~3~$~4~$~5~$~6~$~7~$~8~$",
                            " OCTAVE CONTROL ", current_octave);
}

// --- 移調 (Ctrl+T) ---

// typcons2.inc の transp_menu_str1 (表示の切り替えで書き換える)
std::vector<std::string> transp_menu_str1 = {
    " [~1~]   1 UP   \xBF",
    " [~2~]  12 UP   \xF6 CURRENT",
    " [~3~]   1 DOWN \xF8 iNSTRUMENT         \xDA\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xBF",
    "",
    " [~5~]   1 UP   \xBF                    \xC0\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xD9",
    " [~6~]  12 UP   \xF6 ALL",
    " [~7~]   1 DOWN \xF8 iNSTRUMENTS",
    " [~8~]  12 DOWN \xD9",
    std::string(48, '\x14'),
    " [~A~]   1 UP   \xBF",
    " [~B~]  12 UP   \xF6 CURRENT",
    " [~C~]   1 DOWN \xF8 iNSTRUMENT         \xDA\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xBF",
    "",
    " [~E~]   1 UP   \xBF                    \xC0\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xD9",
    " [~F~]  12 UP   \xF6 ALL",
    " [~G~]   1 DOWN \xF8 iNSTRUMENTS",
    " [~H~]  12 DOWN \xD9",
};

const char* const transp_menu_str2[8] = {
    " [~A~]   1 UP   \xBF",
    " [~B~]  12 UP   \xF6 CURRENT",
    " [~C~]   1 DOWN \xF8 iNSTRUMENT         \xDA\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xBF",
    "",
    " [~E~]   1 UP   \xBF                    \xC0\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xD9",
    " [~F~]  12 UP   \xF6 ALL",
    " [~G~]   1 DOWN \xF8 iNSTRUMENTS",
    " [~H~]  12 DOWN \xD9",
};

const char* const transp_menu_str3[8] = {
    " [A]   1 UP   \xBF",
    " [B]  12 UP   \xF6 CURRENT",
    " [C]   1 DOWN \xF8 iNSTRUMENT         \xDA\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xBF",
    " [D]  12 DOWN \xD9                    \xB3  BLOCK  \xB3",
    " [E]   1 UP   \xBF                    \xC0\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xD9",
    " [F]  12 UP   \xF6 ALL",
    " [G]   1 DOWN \xF8 iNSTRUMENTS",
    " [H]  12 DOWN \xD9",
};

const char* const transp_menu_str4[4] = {
    " [~4~]  12 DOWN \xD9                    \xB3 PATTERN \xB3",
    " [~D~]  12 DOWN \xD9                    \xB3  SONG   \xB3",
    " [~4~]  12 DOWN \xD9                    \xB3  TRACK  \xB3",
    " [~D~]  12 DOWN \xD9                    \xB3  BLOCK  \xB3",
};

const char* const transp_mnu_str5[19] = {
    "~CURRENT~",
    "~iNSTRUMENT         \xC9\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBB~",
    "ALL",
    "iNSTRUMENTS",
    "CURRENT",
    "iNSTRUMENT         ~\xC9\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBB~",
    "~ALL~",
    "~iNSTRUMENTS~",
    "iNSTRUMENT         \xDA\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xBF",
    "~\xBA PATTERN \xBA~",
    "~\xBA  TRACK  \xBA~",
    "\xB3 PATTERN \xB3",
    "\xB3  TRACK  \xB3",
    "~\xBA  SONG   \xBA~",
    "~\xBA  BLOCK  \xBA~",
    "\xB3  SONG   \xB3",
    "\xB3  BLOCK  \xB3",
    "~\xC8\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBC~",
    "\xC0\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xD9",
};

uint8_t transpos = 1;
bool transp_menu2 = false;
uint8_t transp_pos1 = 1;
uint8_t transp_pos2 = 1;

// 選んでいる項目の組を強調する (TRANSPOSE の Menu の ext_proc)
void transpose__control_proc()
{
    // transp_menu_str1[item] の先頭 len 文字に transp_mnu_str5[idx] を続ける (String[50])
    const auto set = [](int item, int len, int idx) {
        std::string& s = transp_menu_str1[item - 1];
        s = Copy(Copy(s, 1, len) + transp_mnu_str5[idx - 1], 1, 50);
    };
    const uint16_t pos = mn_environment.curr_pos;

    if (pos >= 1 && pos <= 8) {
        if (pos <= 4) {
            set(2, 18, 1);
            set(3, 18, 2);
            set(5, 37, 18);
            set(6, 18, 3);
            set(7, 18, 4);
        } else {
            set(2, 18, 5);
            set(3, 18, 6);
            set(5, 37, 18);
            set(6, 18, 7);
            set(7, 18, 8);
        }
        set(4, 37, !transp_menu2 ? 10 : 11);
    } else {
        set(2, 18, 5);
        set(3, 18, 9);
        set(5, 37, 19);
        set(6, 18, 3);
        set(7, 18, 4);
        set(4, 37, !transp_menu2 ? 12 : 13);
    }

    if (pos >= 10 && pos <= 17) {
        if (pos <= 13) {
            set(11, 18, 1);
            set(12, 18, 2);
            set(14, 37, 18);
            set(15, 18, 3);
            set(16, 18, 4);
        } else {
            set(11, 18, 5);
            set(12, 18, 6);
            set(14, 37, 18);
            set(15, 18, 7);
            set(16, 18, 8);
        }
        set(13, 37, !transp_menu2 ? 14 : 15);
    } else if (!(transp_menu2 && !marking)) {
        set(11, 18, 5);
        set(12, 18, 9);
        set(14, 37, 19);
        set(15, 18, 3);
        set(16, 18, 4);
        set(13, 37, !transp_menu2 ? 16 : 17);
    }

    mn_environment.do_refresh = true;
    mn_environment.refresh();
}

void TRANSPOSE()
{
    static const uint8_t factor[17] = { 1, 12, 1, 12, 1, 12, 1, 12, BYTE_NULL, 1, 12, 1, 12, 1, 12, 1, 12 };

    const uint8_t old_text_attr = mn_setting.text_attr;
    mn_setting.text_attr = dialog_background + dialog_text;
    mn_setting.cycle_moves = true;
    mn_setting.fixed_len = 14;
    mn_setting.terminate_keys[2] = kTAB;
    mn_environment.ext_proc = transpose__control_proc;
    uint8_t patterns;
    {
        PlayerLock lock;
        patterns = count_patterns();
    }

    const auto copy_items = [](const char* const (&src)[8]) {
        for (int i = 0; i < 8; ++i) transp_menu_str1[9 + i] = src[i];
    };

    do {
        if (transp_menu2) {
            mn_environment.context = " TAB \xC4\x10 PATTERN/SONG ";
            transp_menu_str1[3] = transp_menu_str4[2];
            if (marking) {
                copy_items(transp_menu_str2);
                transp_menu_str1[12] = transp_menu_str4[3];
            } else {
                copy_items(transp_menu_str3);
            }
        } else {
            mn_environment.context = " TAB \xC4\x10 TRACK/BLOCK ";
            copy_items(transp_menu_str2);
            transp_menu_str1[3] = transp_menu_str4[0];
            transp_menu_str1[12] = transp_menu_str4[1];
        }

        transpos = B(Menu(transp_menu_str1, 1, 1, transpos, 50, 17, 17, " TRANSPOSE "));
        if (transp_menu2) transp_pos1 = transpos;
        else transp_pos2 = transpos;

        if (mn_environment.keystroke == kTAB) {
            transp_menu2 = !transp_menu2;
            transpos = transp_menu2 ? transp_pos1 : transp_pos2;
            keyboard_reset_buffer();
        }

        if (mn_environment.keystroke != kESC && mn_environment.keystroke != kTAB) {
            PlayerLock lock;
            const FixedSongData& sd = songdata();
            uint8_t patt0, patt1, track0, track1, line0, line1;
            if (transp_menu2 || transpos < 9) {
                patt0 = pattern_patt;
                patt1 = patt0;
            } else {
                // オリジナルは最後のパターンの次 (count_patterns の値) まで移調する。
                // 128 個すべて使っているときは範囲の外になるため、パターン 127 までにする
                patt0 = 0;
                patt1 = std::min<uint8_t>(patterns, 127);
            }

            if (!transp_menu2) {
                line0 = 0;
                line1 = B(sd.patt_len - 1);
                track0 = 1;
                track1 = sd.nm_tracks;
            } else if (transpos < 9) {
                line0 = 0;
                line1 = B(sd.patt_len - 1);
                track0 = count_channel(pattern_hpos);
                track1 = track0;
            } else {
                if (tracing) {
                    patt0 = tracing_block_pattern;
                    patt1 = patt0;
                }
                line0 = block_y0;
                line1 = block_y1;
                track0 = block_x0;
                track1 = block_x1;
            }

            const bool up = transpos == 1 || transpos == 2 || transpos == 5 || transpos == 6 ||
                            transpos == 10 || transpos == 11 || transpos == 14 || transpos == 15;
            const bool down = transpos == 3 || transpos == 4 || transpos == 7 || transpos == 8 ||
                              transpos == 12 || transpos == 13 || transpos == 16 || transpos == 17;
            const bool current_ins = (transpos >= 1 && transpos <= 4) || (transpos >= 10 && transpos <= 13);
            if (up || down) {
                transpose_custom_area(up ? TransposeType::Up : TransposeType::Down,
                                      current_ins ? TransposeType::CurrentIns : TransposeType::AllIns,
                                      patt0, patt1, track0, track1, line0, line1, factor[transpos - 1]);
            }
        }
    } while (mn_environment.keystroke == kTAB);

    {
        PlayerLock lock;
        PATTERN_ORDER_page_refresh(pattord_page);
        PATTERN_page_refresh(pattern_page);
    }
    mn_setting.fixed_len = 0;
    mn_setting.terminate_keys[2] = 0;
    mn_environment.context.clear();
    mn_environment.ext_proc = nullptr;
    mn_setting.text_attr = old_text_attr;
}

// --- 楽器の置き換え (Ctrl+R) ---

uint8_t _remap_pos = 1;
uint8_t _remap_inst_page_len = 0;
uint8_t _remap_xstart = 0;
uint8_t _remap_ystart = 0;
std::vector<std::string> temp_instr_names(255);

void _remap_refresh_proc()
{
    const uint8_t x = _remap_xstart, y = _remap_ystart;
    const uint8_t yb = B(y + _remap_inst_page_len + 4);

    ShowStr(centered_frame_vdest, B(x + 8), B(y + 1), "CURRENT iNSTRUMENT (" + byte2hex(B(MenuLib1.mn_environment.curr_pos)) + ")",
            _remap_pos == 1 ? dialog_background + dialog_hi_text : dialog_background + dialog_text);
    ShowStr(centered_frame_vdest, B(x + 44), B(y + 1), "NEW iNSTRUMENT (" + byte2hex(B(MenuLib2.mn_environment.curr_pos)) + ")",
            _remap_pos == 2 ? dialog_background + dialog_hi_text : dialog_background + dialog_text);

    // 範囲の選択肢 (選んでいる位置 / 選ばれている範囲 / それ以外)
    const auto show_item = [&](uint8_t xofs, const char* str, uint8_t pos, uint8_t selection, uint8_t other_attr) {
        uint8_t attr;
        if (_remap_pos == pos) attr = B(dialog_hi_text << 4);
        else if (remap_selection == selection) attr = dialog_sel_itm_bck + dialog_sel_itm;
        else attr = other_attr;
        ShowStr(centered_frame_vdest, B(x + xofs), yb, str, attr);
    };
    show_item(18, " PATTERN ", 3, 1, dialog_background + dialog_text);
    show_item(29, " SONG ", 4, 2, dialog_background + dialog_text);
    show_item(37, " TRACK ", 5, 3, dialog_background + dialog_text);
    show_item(46, " BLOCK ", 6, 4, marking ? dialog_background + dialog_text : dialog_background + dialog_item_dis);
}

// 2 つの一覧の Menu の ext_proc。選んでいる楽器を試聴する
void REMAP_instr_control_proc()
{
    _remap_refresh_proc();
    PlayerLock lock;
    if (remap_mtype == 1)
        INSTRUMENT_test(B(MenuLib1.mn_environment.curr_pos), BYTE_NULL, count_channel(pattern_hpos),
                        MenuLib1.mn_environment.keystroke, true);
    else
        INSTRUMENT_test(B(MenuLib2.mn_environment.curr_pos), BYTE_NULL, count_channel(pattern_hpos),
                        MenuLib2.mn_environment.keystroke, true);
}

// 一覧の枠を描き直す (右の縦線はスクロールバーなので、その列の色だけ変える)
void override_frame(tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, const std::string& frame, uint8_t attr)
{
    ShowStr(dest, x, y, std::string(1, frame[0]) + ExpStrL("", 32, frame[1]) + frame[2], attr);
    ShowVStr(dest, x, B(y + 1), ExpStrL("", MAX_PATTERN_ROWS, frame[3]), attr);
    ShowStr(dest, x, B(y + MAX_PATTERN_ROWS + 1), std::string(1, frame[5]) + ExpStrL("", 32, frame[6]) + frame[7], attr);

    // override_vscrollbar (x + 33 の列の y + 1 行目から MAX_PATTERN_ROWS + 1 行)
    const int col = x + 33 - 1;
    for (int row = y; row <= y + MAX_PATTERN_ROWS; ++row) dest[(row * MaxCol + col) * 2 + 1] = attr;
}

void REMAP()
{
    if (remap_selection == 4 && !marking) remap_selection = 1;
    _remap_pos = 1;
    _remap_inst_page_len = MAX_PATTERN_ROWS;
    bool qflag = false;

    MenuLib1.mn_setting.menu_attr = dialog_background + dialog_text;
    MenuLib2.mn_setting.menu_attr = dialog_background + dialog_text;

    const auto restore_area = []() {
        move_to_screen_data = screen_backup;
        move_to_screen_area[1] = _remap_xstart;
        move_to_screen_area[2] = _remap_ystart;
        move_to_screen_area[3] = B(_remap_xstart + 71 + 2);
        move_to_screen_area[4] = B(_remap_ystart + _remap_inst_page_len + 5 + 1);
    };
    const auto reset_screen = [&]() {
        MenuLib1.mn_environment.ext_proc = nullptr;
        MenuLib2.mn_environment.ext_proc = nullptr;
        restore_area();
        move2screen();
    };

    for (;;) {   // _jmp1
        if (_force_program_quit) return;

        {
            PlayerLock lock;
            for (int temp = 1; temp <= 255; ++temp)
                temp_instr_names[temp - 1] = " " + Copy(cstr2str(at1(songdata().instr_names, temp).str()), 2, 31);
        }

        ScreenMemCopy(screen_ptr, screen_backup);
        HideCursor();

        ScreenMemCopy(screen_ptr, temp_screen);
        centered_frame_vdest = temp_screen;
        centered_frame(_remap_xstart, _remap_ystart, 71, B(_remap_inst_page_len + 5), " REMAP iNSTRUMENT ",
                       dialog_background + dialog_border, dialog_background + dialog_title, frame_double);

        MenuLib1.mn_environment.curr_pos = remap_ins1;
        MenuLib2.mn_environment.curr_pos = remap_ins2;

        MenuLib1.mn_environment.v_dest = temp_screen;
        MenuLib2.mn_environment.v_dest = temp_screen;
        MenuLib1.mn_environment.ext_proc = REMAP_instr_control_proc;
        MenuLib2.mn_environment.ext_proc = REMAP_instr_control_proc;

        MenuLib1.mn_environment.unpolite = false;
        MenuLib1.mn_environment.preview = true;
        remap_ins1 = B(MenuLib1.Menu(temp_instr_names, B(_remap_xstart + 2), B(_remap_ystart + 2), remap_ins1, 32,
                                     _remap_inst_page_len, 255, ""));

        MenuLib2.mn_environment.unpolite = false;
        MenuLib2.mn_environment.preview = true;
        remap_ins2 = B(MenuLib2.Menu(temp_instr_names, B(_remap_xstart + 36), B(_remap_ystart + 2), remap_ins2, 32,
                                     _remap_inst_page_len, 255, ""));

        // 選んでいる一覧は二重線の枠
        const auto draw_frames = [](tSCREEN_MEM_PTR dest) {
            if (_remap_pos == 1)
                override_frame(dest, B(_remap_xstart + 2), B(_remap_ystart + 2), frame_double, dialog_background + dialog_hi_text);
            else
                override_frame(dest, B(_remap_xstart + 2), B(_remap_ystart + 2), frame_single, dialog_background + dialog_text);
            if (_remap_pos == 2)
                override_frame(dest, B(_remap_xstart + 36), B(_remap_ystart + 2), frame_double, dialog_background + dialog_hi_text);
            else
                override_frame(dest, B(_remap_xstart + 36), B(_remap_ystart + 2), frame_single, dialog_background + dialog_text);
        };
        draw_frames(centered_frame_vdest);

        _remap_refresh_proc();
        move_to_screen_data = temp_screen;
        move_to_screen_area[1] = _remap_xstart;
        move_to_screen_area[2] = _remap_ystart;
        move_to_screen_area[3] = B(_remap_xstart + 71 + 2);
        move_to_screen_area[4] = B(_remap_ystart + _remap_inst_page_len + 5 + 1);
        move2screen_alt();

        centered_frame_vdest = screen_ptr;
        MenuLib1.mn_environment.v_dest = screen_ptr;
        MenuLib2.mn_environment.v_dest = screen_ptr;

        // Ctrl+O・F1 は画面を戻してから開き直す (ヘルプは未実装)
        bool reopen = false;
        const auto other_keys = [&](uint16_t key) {
            if (key == kCtrlO) {
                reset_screen();
                OCTAVE_CONTROL();
                reopen = true;
            } else if (key == kF1) {
                reset_screen();
                reopen = true;
            }
        };

        if (!_force_program_quit) {
            do {
                draw_frames(screen_ptr);
                MenuLib1.mn_setting.menu_attr = _remap_pos == 1 ? dialog_background + dialog_hi_text : dialog_background + dialog_text;
                MenuLib2.mn_setting.menu_attr = _remap_pos == 2 ? dialog_background + dialog_hi_text : dialog_background + dialog_text;

                switch (_remap_pos) {
                case 1: {
                    remap_ins1 = B(MenuLib1.Menu(temp_instr_names, B(_remap_xstart + 2), B(_remap_ystart + 2), remap_ins1, 32,
                                                 _remap_inst_page_len, 255, ""));
                    const uint16_t key = MenuLib1.mn_environment.keystroke;
                    if (key == kShTAB || key == kLEFT) _remap_pos = B(2 + remap_selection);
                    else if (key == kRIGHT || key == kTAB) _remap_pos = 2;
                    else if (key == kESC) qflag = true;
                    else if (key == kENTER) { _remap_pos = B(2 + remap_selection); qflag = true; }
                    else other_keys(key);
                    break;
                }

                case 2: {
                    remap_ins2 = B(MenuLib2.Menu(temp_instr_names, B(_remap_xstart + 36), B(_remap_ystart + 2), remap_ins2, 32,
                                                 _remap_inst_page_len, 255, ""));
                    const uint16_t key = MenuLib2.mn_environment.keystroke;
                    if (key == kLEFT || key == kShTAB) _remap_pos = 1;
                    else if (key == kTAB || key == kRIGHT) _remap_pos = B(2 + remap_selection);
                    else if (key == kESC) qflag = true;
                    else if (key == kENTER) { _remap_pos = B(2 + remap_selection); qflag = true; }
                    else other_keys(key);
                    break;
                }

                default: {
                    const uint16_t fkey = getkey();
                    const uint8_t last_pos = marking ? 6 : 5;
                    switch (fkey) {
                    case kHOME: _remap_pos = 3; break;
                    case kEND: _remap_pos = _remap_pos == 6 ? 6 : last_pos; break;
                    case kLEFT:
                        if (_remap_pos == 3) _remap_pos = last_pos;
                        else --_remap_pos;
                        break;
                    case kRIGHT:
                        if (_remap_pos == 3) _remap_pos = 4;
                        else if (_remap_pos == 4) _remap_pos = 5;
                        else if (_remap_pos == 5) _remap_pos = marking ? 6 : 3;
                        else _remap_pos = 3;
                        break;
                    case kTAB: case kDOWN: _remap_pos = 1; break;
                    case kShTAB: case kUP: _remap_pos = 2; break;
                    case kENTER: qflag = true; break;
                    case kESC: _remap_pos = 1; qflag = true; break;
                    default: other_keys(fkey); break;
                    }
                    break;
                }
                }
                if (reopen) break;

                if (_remap_pos < 3) remap_mtype = _remap_pos;
                else remap_selection = B(_remap_pos - 2);
                _remap_refresh_proc();
            } while (!qflag);
        }
        if (!reopen) break;
    }

    MenuLib1.mn_environment.ext_proc = nullptr;
    MenuLib2.mn_environment.ext_proc = nullptr;
    restore_area();
    move2screen();

    PlayerLock lock;
    if (qflag && _remap_pos > 2) {
        A2Player& p = P();
        const FixedSongData& sd = songdata();
        const bool backup_replay_forbidden = p.replay_forbidden;
        const PlayStatus backup_play_status = p.play_status;
        p.replay_forbidden = true;
        if (p.play_status != PlayStatus::Stopped) p.play_status = PlayStatus::Paused;

        uint8_t patt0 = pattern_patt, patt1 = pattern_patt;
        uint8_t track0 = 1, track1 = sd.nm_tracks;
        uint8_t line0 = 0, line1 = B(sd.patt_len - 1);
        switch (_remap_pos - 2) {
        case 2:
            // オリジナルは最後のパターンの次 (count_patterns の値) まで置き換える。
            // 128 個すべて使っているときは範囲の外になるため、パターン 127 までにする
            patt0 = 0;
            patt1 = std::min<uint8_t>(count_patterns(), 127);
            break;
        case 3:
            track0 = count_channel(pattern_hpos);
            track1 = track0;
            break;
        case 4:
            track0 = block_x0;
            track1 = block_x1;
            line0 = block_y0;
            line1 = block_y1;
            break;
        }

        Chunk chunk;
        for (int temp3 = patt0; temp3 <= patt1; ++temp3) {
            for (int temp2 = track0; temp2 <= track1; ++temp2) {
                for (int temp1 = line0; temp1 <= line1; ++temp1) {
                    get_chunk(B(temp3), B(temp1), B(temp2), chunk);
                    if (chunk.instr_def == remap_ins1) {
                        chunk.instr_def = remap_ins2;
                        put_chunk(B(temp3), B(temp1), B(temp2), chunk);
                    }
                }
            }
        }

        p.replay_forbidden = backup_replay_forbidden;
        p.play_status = backup_play_status;
    }

    PATTERN_ORDER_page_refresh(pattord_page);
    PATTERN_page_refresh(pattern_page);
}

// --- トラックの並べ替え (Ctrl+X) ---

uint8_t _rearrange_track_pos = 1;
uint8_t _rearrange_pos = 1;
uint8_t _rearrange_tracklist_idx[20] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20 };   // [1..20]
uint8_t _rearrange_xstart = 0;
uint8_t _rearrange_ystart = 0;
uint8_t _rearrange_nm_tracks = 0;
std::string _rearrange_tracklist[19];   // [1..18] (オリジナルどおり [0] は使わない)

void _rearrange_refresh_proc()
{
    const uint8_t x = _rearrange_xstart, y = _rearrange_ystart;
    const uint8_t attr = _rearrange_pos != 1 ? dialog_sel_itm_bck + dialog_sel_itm : B(dialog_hi_text << 4);

    for (int idx = 1; idx <= _rearrange_nm_tracks; ++idx) {
        if (idx == _rearrange_track_pos)
            ShowCStr(centered_frame_vdest, B(x + 1), B(y + idx),
                     "~      " + ExpStrL(Num2str(idx, 10), 2, ' ') + " ~\x10" + _rearrange_tracklist[idx] + "\x11", attr,
                     dialog_background + dialog_contxt_dis2);
        else
            ShowCStr(centered_frame_vdest, B(x + 1), B(y + idx),
                     "~      " + ExpStrL(Num2str(idx, 10), 2, ' ') + "  ~" + _rearrange_tracklist[idx] + " ", mn_setting.text_attr,
                     dialog_background + dialog_contxt_dis2);
    }

    const uint8_t yb = B(y + _rearrange_nm_tracks + 2);
    const auto show_item = [&](uint8_t xofs, const char* str, uint8_t pos, uint8_t selection) {
        uint8_t item_attr;
        if (_rearrange_pos == pos) item_attr = B(dialog_hi_text << 4);
        else if (rearrange_selection == selection) item_attr = dialog_sel_itm_bck + dialog_sel_itm;
        else item_attr = dialog_background + dialog_text;
        ShowStr(centered_frame_vdest, B(x + xofs), yb, str, item_attr);
    };
    show_item(2, " PATTERN ", 2, 1);
    show_item(13, " SONG ", 3, 2);
}

void REARRANGE()
{
    {
        PlayerLock lock;
        const FixedSongData& sd = songdata();
        if (_rearrange_track_pos > sd.nm_tracks) _rearrange_track_pos = 1;
        if (rearrange_selection == 4 && !marking) rearrange_selection = 1;
        _rearrange_pos = 1;

        // オリジナルの max は小さい方を返す (パーカッションモードでは 15 トラックまで)
        if (P().percussion_mode) _rearrange_nm_tracks = std::min<uint8_t>(sd.nm_tracks, 15);
        else _rearrange_nm_tracks = sd.nm_tracks;
    }
    bool qflag = false;

    bool reset_flag = false;
    for (int temp = 1; temp <= _rearrange_nm_tracks; ++temp) {
        if (_rearrange_tracklist_idx[temp - 1] > _rearrange_nm_tracks) {
            reset_flag = true;
            break;
        }
    }
    if (reset_flag) {
        for (int temp = 1; temp <= _rearrange_nm_tracks; ++temp) _rearrange_tracklist_idx[temp - 1] = B(temp);
        _rearrange_track_pos = 1;
    }

    for (int temp = 1; temp <= _rearrange_nm_tracks; ++temp)
        _rearrange_tracklist[temp] = " " + ExpStrL(Num2str(_rearrange_tracklist_idx[temp - 1], 10), 2, ' ') + " ";

    const auto reset_screen = []() {
        move_to_screen_data = screen_backup;
        move_to_screen_area[1] = _rearrange_xstart;
        move_to_screen_area[2] = _rearrange_ystart;
        move_to_screen_area[3] = B(_rearrange_xstart + 20 + 2);
        move_to_screen_area[4] = B(_rearrange_ystart + _rearrange_nm_tracks + 3 + 1);
        move2screen();
    };

    for (;;) {   // _jmp1
        if (_force_program_quit) return;

        ScreenMemCopy(screen_ptr, screen_backup);
        HideCursor();

        ScreenMemCopy(screen_ptr, temp_screen);
        centered_frame_vdest = temp_screen;
        centered_frame(_rearrange_xstart, _rearrange_ystart, 20, B(_rearrange_nm_tracks + 3), " REARRANGE TRACKS ",
                       dialog_background + dialog_border, dialog_background + dialog_title, frame_double);

        ShowStr(centered_frame_vdest, B(_rearrange_xstart + 1), B(_rearrange_ystart + _rearrange_nm_tracks + 1),
                ExpStrL("", 19, '\xC4'), dialog_background + dialog_contxt_dis2);

        _rearrange_refresh_proc();
        move_to_screen_data = temp_screen;
        move_to_screen_area[1] = _rearrange_xstart;
        move_to_screen_area[2] = _rearrange_ystart;
        move_to_screen_area[3] = B(_rearrange_xstart + 20 + 2);
        move_to_screen_area[4] = B(_rearrange_ystart + _rearrange_nm_tracks + 3 + 1);
        move2screen_alt();
        centered_frame_vdest = screen_ptr;

        // F1 は画面を戻してから開き直す (ヘルプは未実装)
        bool reopen = false;
        if (!_force_program_quit) {
            do {
                uint16_t fkey = 0;
                switch (_rearrange_pos) {
                case 1:
                    fkey = getkey();
                    switch (fkey) {
                    case kUP:
                        if (_rearrange_track_pos > 1) --_rearrange_track_pos;
                        else _rearrange_track_pos = _rearrange_nm_tracks;
                        break;
                    case kDOWN:
                        if (_rearrange_track_pos < _rearrange_nm_tracks) ++_rearrange_track_pos;
                        else _rearrange_track_pos = 1;
                        break;
                    case kPgUP: case kHOME: _rearrange_track_pos = 1; break;
                    case kPgDOWN: case kEND: _rearrange_track_pos = _rearrange_nm_tracks; break;
                    case kCtPgUP:
                        if (_rearrange_nm_tracks > 1) {
                            if (shift_pressed()) {
                                // 先頭のトラックを今の位置へ移す (オリジナルは temp + 1 = 19 の要素も読む)
                                const std::string temps = _rearrange_tracklist[1];
                                for (int temp = 1; temp < _rearrange_track_pos; ++temp)
                                    _rearrange_tracklist[temp] = _rearrange_tracklist[temp + 1];
                                _rearrange_tracklist[_rearrange_track_pos] = temps;
                            } else if (_rearrange_track_pos > 1) {
                                std::swap(_rearrange_tracklist[_rearrange_track_pos - 1], _rearrange_tracklist[_rearrange_track_pos]);
                                --_rearrange_track_pos;
                            }
                        }
                        break;
                    case kCtPgDN:
                        if (_rearrange_track_pos < _rearrange_nm_tracks) {
                            if (shift_pressed()) {
                                // 最後のトラックを今の位置へ移す
                                const std::string temps = _rearrange_tracklist[_rearrange_nm_tracks];
                                for (int temp = _rearrange_nm_tracks; temp >= _rearrange_track_pos + 1; --temp)
                                    _rearrange_tracklist[temp] = _rearrange_tracklist[temp - 1];
                                _rearrange_tracklist[_rearrange_track_pos] = temps;
                            } else {
                                std::swap(_rearrange_tracklist[_rearrange_track_pos + 1], _rearrange_tracklist[_rearrange_track_pos]);
                                ++_rearrange_track_pos;
                            }
                        }
                        break;
                    case kLEFT: case kRIGHT: case kTAB: case kShTAB: _rearrange_pos = B(1 + rearrange_selection); break;
                    case kESC: qflag = true; break;
                    case kENTER: _rearrange_pos = B(1 + rearrange_selection); qflag = true; break;
                    case kF1: reset_screen(); reopen = true; break;
                    }
                    break;

                case 2:
                case 3:
                    fkey = getkey();
                    switch (fkey) {
                    case kHOME: _rearrange_pos = 2; break;
                    case kEND: _rearrange_pos = 3; break;
                    case kLEFT: case kRIGHT: _rearrange_pos = _rearrange_pos == 2 ? 3 : 2; break;
                    case kUP: case kDOWN: case kTAB: case kShTAB: _rearrange_pos = 1; break;
                    case kENTER: qflag = true; break;
                    case kESC: _rearrange_pos = 1; qflag = true; break;
                    case kF1: reset_screen(); reopen = true; break;
                    }
                    break;
                }
                if (reopen) break;

                if (_rearrange_pos > 1) rearrange_selection = B(_rearrange_pos - 1);
                if (fkey != kESC) _rearrange_refresh_proc();
            } while (!qflag);
        }
        if (!reopen) break;
    }

    reset_screen();
    for (int temp = 1; temp <= _rearrange_nm_tracks; ++temp)
        _rearrange_tracklist_idx[temp - 1] = B(Str2num(CutStr(_rearrange_tracklist[temp]), 10));

    PlayerLock lock;
    if (qflag && _rearrange_pos > 1) {
        A2Player& p = P();
        const FixedSongData& sd = songdata();
        const bool backup_replay_forbidden = p.replay_forbidden;
        const PlayStatus backup_play_status = p.play_status;
        p.replay_forbidden = true;
        if (p.play_status != PlayStatus::Stopped) p.play_status = PlayStatus::Paused;
        _rearrange_track_pos = 1;

        uint8_t patt0 = pattern_patt, patt1 = pattern_patt;
        if (rearrange_selection == 2) {
            // オリジナルは最後のパターンの次 (count_patterns の値) まで並べ替える。
            // 128 個すべて使っているときは範囲の外になるため、パターン 127 までにする
            patt0 = 0;
            patt1 = std::min<uint8_t>(count_patterns(), 127);
        }

        static Chunk temp_pattern[20][256];   // [1..20][0..255]
        for (int temp3 = patt0; temp3 <= patt1; ++temp3) {
            for (int temp2 = 0; temp2 <= sd.patt_len - 1; ++temp2)
                for (int temp1 = 1; temp1 <= _rearrange_nm_tracks; ++temp1)
                    get_chunk(B(temp3), B(temp2), _rearrange_tracklist_idx[temp1 - 1], temp_pattern[temp1 - 1][temp2]);
            for (int temp2 = 0; temp2 <= sd.patt_len - 1; ++temp2)
                for (int temp1 = 1; temp1 <= _rearrange_nm_tracks; ++temp1)
                    put_chunk(B(temp3), B(temp2), B(temp1), temp_pattern[temp1 - 1][temp2]);
        }

        for (int temp = 1; temp <= _rearrange_nm_tracks; ++temp) _rearrange_tracklist_idx[temp - 1] = B(temp);
        p.replay_forbidden = backup_replay_forbidden;
        p.play_status = backup_play_status;
    }

    PATTERN_ORDER_page_refresh(pattord_page);
    PATTERN_page_refresh(pattern_page);
}

// --- 置換 (Ctrl+H) ---

uint8_t _replace_xstart = 0;
uint8_t _replace_ystart = 0;

// typcons2.inc の note_keyoff_str (pattern_layout ごとのキーオフの表記)
const char* replace_keyoff_str()
{
    static const char* const note_keyoff_str[3] = { "\xFE\xFE\xFE", "\xCD\xCD\xCD", "^^\xFA" };
    return note_keyoff_str[pattern_layout <= 2 ? pattern_layout : 0];
}

std::string FilterStr(std::string str, char chr0, char chr1)
{
    for (char& c : str) {
        if (c == chr0) c = chr1;
    }
    return str;
}

// StrUtils の IsWild ('*' は任意の文字列、'?' は任意の 1 文字。大文字と小文字は区別する)
bool IsWild(const char* str, const char* wilds)
{
    for (; *wilds != '\0'; ++wilds) {
        if (*wilds == '*') {
            for (const char* s = str;; ++s) {
                if (IsWild(s, wilds + 1)) return true;
                if (*s == '\0') return false;
            }
        }
        if (*str == '\0' || (*wilds != '?' && *wilds != *str)) return false;
        ++str;
    }
    return *str == '\0';
}

// stringio.pas の SameName
bool SameName(std::string mask, const std::string& str)
{
    while (mask.size() > str.size() && !mask.empty() && (mask.back() == '*' || mask.back() == '?')) mask.pop_back();
    return IsWild(str.c_str(), mask.c_str());
}

void _replace_refresh(uint8_t pos)
{
    const uint8_t x = _replace_xstart, y = _replace_ystart;
    ShowStr(centered_frame_vdest, B(x + 2), B(y + 1), "NOTE,iNSTRUMENT,FX N\xF9" "1/N\xF9" "2 TO FiND",
            pos >= 1 && pos <= 11 ? dialog_background + dialog_hi_text : dialog_background + dialog_text);
    ShowStr(centered_frame_vdest, B(x + 2), B(y + 4), "NEW NOTE,iNSTRUMENT,FX N\xF9" "1/N\xF9" "2",
            pos >= 12 && pos <= 22 ? dialog_background + dialog_hi_text : dialog_background + dialog_text);

    const auto event_str = [](const ReplaceEvent& ev) {
        return "\"" + FilterStr(ev.note, '?', '\xFA') + "\" \"" + FilterStr(ev.inst, '?', '\xFA') + "\" \"" +
               FilterStr(ev.fx_1, '?', '\xFA') + "\" \"" + FilterStr(ev.fx_2, '?', '\xFA') + "\"";
    };
    ShowCStr2(centered_frame_vdest, B(x + 2), B(y + 2), event_str(replace_data.event_to_find), dialog_background + dialog_text,
              dialog_input_bckg + dialog_input);
    ShowCStr2(centered_frame_vdest, B(x + 2), B(y + 5), event_str(replace_data.new_event), dialog_background + dialog_text,
              dialog_input_bckg + dialog_input);

    const char on_off = replace_prompt ? '\xFB' : ' ';
    if (pos == 27)
        ShowC3Str(centered_frame_vdest, B(x + 2), B(y + 7), std::string("~[~`") + on_off + "`~]~ PROMPT ON REPLACE",
                  dialog_background + dialog_hi_text, dialog_background + dialog_text, dialog_background + dialog_item);
    else
        ShowCStr(centered_frame_vdest, B(x + 2), B(y + 7), std::string("[~") + on_off + "~] PROMPT ON REPLACE",
                 dialog_background + dialog_text, dialog_background + dialog_item);

    const auto show_item = [&](uint8_t xofs, const char* str, uint8_t item_pos, uint8_t selection, uint8_t other_attr) {
        uint8_t attr;
        if (pos == item_pos) attr = B(dialog_hi_text << 4);
        else if (replace_selection == selection) attr = dialog_sel_itm_bck + dialog_sel_itm;
        else attr = other_attr;
        ShowStr(centered_frame_vdest, B(x + xofs), B(y + 9), str, attr);
    };
    show_item(2, " PATTERN ", 23, 1, dialog_background + dialog_text);
    show_item(13, " SONG ", 24, 2, dialog_background + dialog_text);
    show_item(21, " TRACK ", 25, 3, dialog_background + dialog_text);
    show_item(30, " BLOCK ", 26, 4, marking ? dialog_background + dialog_text : dialog_background + dialog_item_dis);
}

// layout の音名のノート番号 (キーオフなら BYTE_NULL、見つからなければ old_note)
uint8_t _find_note(const std::string& layout, uint8_t old_note)
{
    if (layout == replace_keyoff_str()) return BYTE_NULL;
    for (int temp = 0; temp <= 12 * 8 + 1; ++temp) {
        if (SameName(note_layout[temp], layout)) return B(temp);
    }
    return old_note;
}

uint8_t _find_fx(char fx_str)
{
    const char* p = std::strchr(fx_digits, fx_str);
    if (fx_str != '\0' && p != nullptr) return B(static_cast<int>(p - fx_digits));
    return 48 - 1;
}

// wildcard の '?' を str の同じ位置の文字にする
std::string _wildcard_str(std::string wildcard, const std::string& str)
{
    for (size_t temp = 0; temp < wildcard.size(); ++temp) {
        if (wildcard[temp] == '?') wildcard[temp] = temp < str.size() ? str[temp] : ' ';
    }
    return wildcard;
}

void REPLACE()
{
    // 入力欄の各桁に打てる文字 (_charset[1..11])
    static const tCHARSET fx_chars = CharRange('0', '9') | CharRange('A', 'Z') | CharSet("&%!@=#$~^`><");
    static const tCHARSET hex_chars = CharRange('0', '9') | CharRange('A', 'F');
    const tCHARSET _charset[11] = {
        CharSet("A") | CharSet(std::string(1, UpCase(b_note))) | CharRange('C', 'G'),
        CharSet("#-"),
        CharRange('1', '9'),
        hex_chars, hex_chars,
        fx_chars, hex_chars, hex_chars,
        fx_chars, hex_chars, hex_chars,
    };
    static const uint8_t pos6[11] = { 1, 2, 3, 5, 6, 8, 9, 10, 12, 13, 14 };
    const std::string null3 = "\xFA\xFA\xFA";

    if (replace_selection == 4 && !marking) replace_selection = 1;
    // オリジナルの min は大きい方を返す
    uint8_t pos = B(std::max<int32_t>(get_bank_position("?replace_window?pos", -1), 1));
    bool qflag = false;

    const auto reset_screen = []() {
        HideCursor();
        move_to_screen_data = screen_backup;
        move_to_screen_area[1] = _replace_xstart;
        move_to_screen_area[2] = _replace_ystart;
        move_to_screen_area[3] = B(_replace_xstart + 38 + 2);
        move_to_screen_area[4] = B(_replace_ystart + 10 + 1);
        move2screen();
    };

    // 入力欄に文字を打つ (row_ofs: 上の欄 0 / 下の欄 11)
    const auto input_char = [&](uint16_t fkey, ReplaceEvent& ev, uint8_t row_ofs) {
        if (!(InSet(_charset[pos - row_ofs - 1], UpCase(static_cast<char>(LO(fkey)))) || fkey == kBkSPC || fkey == kDELETE))
            return;

        char chr;
        if (fkey == kDELETE) chr = '\xFA';
        else if (fkey == kBkSPC) {
            chr = '\xFA';
            if (row_ofs == 0) {
                if (pos > 1) --pos;
            } else {
                --pos;   // 下の欄の左端では上の欄の右端へ移る (その桁は書き換えない)
            }
        } else chr = UpCase(static_cast<char>(LO(fkey)));

        const int col = pos - row_ofs;
        if (row_ofs == 0) {
            if (ev.note == replace_keyoff_str() && col >= 1 && col <= 3) ev.note = null3;
        } else {
            if ((ev.note == replace_keyoff_str() || ev.note == "\x07\x07\x07") && col >= 1 && col <= 3) ev.note = null3;
        }

        std::string& note = ev.note;
        const auto is_e_or_b = [&](char c) { return c == 'E' || c == UpCase(b_note); };
        switch (col) {
        case 1:
            note[0] = chr;
            if (is_e_or_b(note[0]) && note[1] == '#') note[1] = '-';
            if (note[0] != 'C' && note[2] == '9') note[2] = '8';
            break;
        case 2:
            if (!(is_e_or_b(note[0]) && chr == '#')) note[1] = chr;
            else note[1] = '-';
            break;
        case 3:
            if (!(note[0] != 'C' && chr == '9')) note[2] = chr;
            break;
        case 4: ev.inst[0] = chr; break;
        case 5: ev.inst[1] = chr; break;
        case 6: ev.fx_1[0] = chr; break;
        case 7: ev.fx_1[1] = chr; break;
        case 8: ev.fx_1[2] = chr; break;
        case 9: ev.fx_2[0] = chr; break;
        case 10: ev.fx_2[1] = chr; break;
        case 11: ev.fx_2[2] = chr; break;
        }

        if (fkey != kDELETE && fkey != kBkSPC && pos < 22) ++pos;
    };

    uint8_t patterns = 0;
    for (;;) {   // _jmp1
        if (_force_program_quit) return;

        ScreenMemCopy(screen_ptr, screen_backup);
        HideCursor();
        {
            PlayerLock lock;
            patterns = count_patterns();
        }

        ScreenMemCopy(screen_ptr, temp_screen);
        centered_frame_vdest = temp_screen;
        centered_frame(_replace_xstart, _replace_ystart, 38, 10, " REPLACE ", dialog_background + dialog_border,
                       dialog_background + dialog_title, frame_double);

        move_to_screen_data = temp_screen;
        move_to_screen_area[1] = _replace_xstart;
        move_to_screen_area[2] = _replace_ystart;
        move_to_screen_area[3] = B(_replace_xstart + 38 + 2);
        move_to_screen_area[4] = B(_replace_ystart + 10 + 1);

        _replace_refresh(pos);
        ShowStr(centered_frame_vdest, B(_replace_xstart + 1), B(_replace_ystart + 8), ExpStrL("", 38 - 1, '\x14'),
                dialog_background + dialog_context_dis);

        move2screen_alt();
        centered_frame_vdest = screen_ptr;

        // F1 は画面を戻してから開き直す (ヘルプは未実装)
        bool reopen = false;
        if (!_force_program_quit) {
            do {
                if ((pos >= 1 && pos <= 22) || pos == 27) {
                    if (use_large_cursor) WideCursor();
                    else ThinCursor();
                } else HideCursor();

                const uint8_t xstart = _replace_xstart, ystart = _replace_ystart;
                uint16_t fkey = 0;
                if (pos >= 1 && pos <= 11) {
                    GotoXY(B(xstart + 1 + pos6[pos - 1]), B(ystart + 2));
                    fkey = getkey();
                    switch (fkey) {
                    case kTAB: case kDOWN:
                        add_bank_position("?replace_window?posfx", -1, pos);
                        pos += 11;
                        break;
                    case kShTAB: case kUP:
                        add_bank_position("?replace_window?posfx", -1, pos);
                        pos = B(22 + replace_selection);
                        break;
                    case kLEFT:
                        if (pos > 1) --pos;
                        else {
                            add_bank_position("?replace_window?posfx", -1, pos);
                            pos = B(22 + replace_selection);
                        }
                        break;
                    case kRIGHT: ++pos; break;
                    case kHOME: pos = 1; break;
                    case kEND: pos = 11; break;
                    case kESC: qflag = true; break;
                    case kENTER: pos = B(22 + replace_selection); qflag = true; break;
                    case kF1: reset_screen(); reopen = true; break;
                    case kCtrlK:
                        if (pos <= 3) {
                            replace_data.event_to_find.note = replace_keyoff_str();
                            pos = 4;
                        }
                        break;
                    default: input_char(fkey, replace_data.event_to_find, 0); break;
                    }
                } else if (pos >= 12 && pos <= 22) {
                    GotoXY(B(xstart + 1 + pos6[pos - 11 - 1]), B(ystart + 5));
                    fkey = getkey();
                    switch (fkey) {
                    case kTAB: case kDOWN:
                        add_bank_position("?replace_window?posfx", -1, pos - 11);
                        pos = 27;
                        break;
                    case kShTAB: case kUP:
                        add_bank_position("?replace_window?posfx", -1, pos - 11);
                        pos -= 11;
                        break;
                    case kLEFT: --pos; break;
                    case kRIGHT: pos = pos < 22 ? B(pos + 1) : 27; break;
                    case kHOME: pos = 12; break;
                    case kEND: pos = 22; break;
                    case kESC: qflag = true; break;
                    case kENTER: pos = B(22 + replace_selection); qflag = true; break;
                    case kF1: reset_screen(); reopen = true; break;
                    case kCtrlN:
                        switch (pos - 11) {
                        case 1: case 2: case 3: replace_data.new_event.note = "\x07\x07\x07"; pos = 11 + 4; break;
                        case 4: case 5: replace_data.new_event.inst = "00"; pos = 11 + 6; break;
                        case 6: case 7: case 8: replace_data.new_event.fx_1 = "000"; pos = 11 + 9; break;
                        case 9: case 10: case 11: replace_data.new_event.fx_2 = "000"; pos = 27; break;
                        }
                        break;
                    case kCtrlK:
                        if (pos - 11 <= 3) {
                            replace_data.new_event.note = replace_keyoff_str();
                            pos = 11 + 4;
                        }
                        break;
                    default: input_char(fkey, replace_data.new_event, 11); break;
                    }
                } else if (pos == 27) {
                    GotoXY(B(xstart + 3), B(ystart + 7));
                    fkey = getkey();
                    switch (fkey) {
                    case kUP: case kShTAB: pos = B(get_bank_position("?replace_window?posfx", -1) + 11); break;
                    case kLEFT: pos = 22; break;
                    case kTAB: case kRIGHT: case kDOWN: pos = B(22 + replace_selection); break;
                    case kENTER: qflag = true; break;
                    case kSPACE: replace_prompt = !replace_prompt; break;
                    case kESC: pos = 1; qflag = true; break;
                    case kF1: reset_screen(); reopen = true; break;
                    }
                } else {   // 23..26 (範囲の選択肢)
                    fkey = getkey();
                    const uint8_t last_pos = marking ? 26 : 25;
                    switch (fkey) {
                    case kTAB: case kDOWN:
                        // オリジナルは入力欄の位置をまだ記録していない (0) と pos が 0 になり、どの分岐にも入らず止まる。
                        // 0 のときは 1 にする
                        pos = B(std::max<int32_t>(get_bank_position("?replace_window?posfx", -1), 1));
                        break;
                    case kShTAB: case kUP: pos = 27; break;
                    case kHOME: pos = 23; break;
                    case kEND: pos = pos == 26 ? 26 : last_pos; break;
                    case kLEFT:
                        if (pos == 23) pos = last_pos;
                        else --pos;
                        break;
                    case kRIGHT:
                        if (pos == 23) pos = 24;
                        else if (pos == 24) pos = 25;
                        else if (pos == 25) pos = marking ? 26 : 23;
                        else pos = 23;
                        break;
                    case kENTER: qflag = true; break;
                    case kESC: pos = 1; qflag = true; break;
                    case kF1: reset_screen(); reopen = true; break;
                    }
                }
                if (reopen) break;

                if (fkey == kCtrlW) {
                    std::swap(replace_data.event_to_find, replace_data.new_event);
                } else if (fkey == kCtBkSp) {
                    if (pos < 12 || shift_pressed()) replace_data.event_to_find = { null3, "\xFA\xFA", null3, null3 };
                    if (pos >= 12 || shift_pressed()) replace_data.new_event = { null3, "\xFA\xFA", null3, null3 };
                }

                if (pos >= 23 && pos <= 26) replace_selection = B(pos - 22);
                _replace_refresh(pos);
                if (!qflag) add_bank_position("?replace_window?pos", -1, pos);
            } while (!qflag);
        }
        if (!reopen) break;
    }

    HideCursor();
    move_to_screen_data = screen_backup;
    move_to_screen_area[1] = _replace_xstart;
    move_to_screen_area[2] = _replace_ystart;
    move_to_screen_area[3] = B(_replace_xstart + 38 + 2);
    move_to_screen_area[4] = B(_replace_ystart + 10 + 1);
    move2screen();

    PlayerLock lock;
    if (qflag && pos > 22) {
        A2Player& p = P();
        const FixedSongData& sd = songdata();
        const bool backup_replay_forbidden = p.replay_forbidden;
        const PlayStatus backup_play_status = p.play_status;
        p.replay_forbidden = true;
        if (p.play_status != PlayStatus::Stopped) p.play_status = PlayStatus::Paused;
        PATTERN_position_preview(BYTE_NULL, BYTE_NULL, BYTE_NULL, 0);

        const auto to_wild = [](const ReplaceEvent& ev) {
            return ReplaceEvent{ FilterStr(ev.note, '\xFA', '?'), FilterStr(ev.inst, '\xFA', '?'), FilterStr(ev.fx_1, '\xFA', '?'),
                                 FilterStr(ev.fx_2, '\xFA', '?') };
        };
        const ReplaceEvent event_to_find = to_wild(replace_data.event_to_find);
        const ReplaceEvent new_event = to_wild(replace_data.new_event);

        uint8_t patt0 = pattern_patt, patt1 = pattern_patt;
        uint8_t track0 = 1, track1 = sd.nm_tracks;
        uint8_t line0 = 0, line1 = B(sd.patt_len - 1);
        switch (replace_selection) {
        case 2:
            // オリジナルは最後のパターンの次 (count_patterns の値) まで置換する。
            // 128 個すべて使っているときは範囲の外になるため、パターン 127 までにする
            patt0 = 0;
            patt1 = std::min<uint8_t>(patterns, 127);
            break;
        case 3:
            track0 = count_channel(pattern_hpos);
            track1 = track0;
            break;
        case 4:
            track0 = block_x0;
            track1 = block_x1;
            line0 = block_y0;
            line1 = block_y1;
            break;
        }

        bool _replace_all = false;
        bool _cancel = false;
        bool _1st_choice = true;

        Chunk chunk, old_chunk;
        for (int temp3 = patt0; temp3 <= patt1; ++temp3) {
            for (int temp2 = track0; temp2 <= track1; ++temp2) {
                for (int temp1 = line0; temp1 <= line1; ++temp1) {
                    if (_cancel) continue;
                    get_chunk(B(temp3), B(temp1), B(temp2), chunk);
                    old_chunk = chunk;

                    if (SameName(event_to_find.inst, byte2hex(old_chunk.instr_def)) &&
                        SameName(event_to_find.fx_1, fx_digit(old_chunk.effect_def) + byte2hex(old_chunk.effect)) &&
                        SameName(event_to_find.fx_2, fx_digit(old_chunk.effect_def2) + byte2hex(old_chunk.effect2))) {
                        bool _valid_note = false;
                        uint8_t temp_note = 0;
                        const uint8_t note = old_chunk.note;
                        if (note <= 12 * 8 + 1) {
                            if (SameName(event_to_find.note, note_layout[note])) {
                                temp_note = _find_note(_wildcard_str(new_event.note, note_layout[note]), note);
                                _valid_note = true;
                            }
                        } else if (note >= fixed_note_flag + 1 && note <= fixed_note_flag + 12 * 8 + 1) {
                            if (SameName(event_to_find.note, note_layout[note - fixed_note_flag])) {
                                if (FilterStr(replace_data.new_event.note, '?', '\xFA') != replace_keyoff_str())
                                    temp_note = B(fixed_note_flag +
                                                  _find_note(_wildcard_str(new_event.note, note_layout[note - fixed_note_flag]), note));
                                else temp_note = _find_note(new_event.note, note);
                                _valid_note = true;
                            }
                        } else if (note == BYTE_NULL) {
                            if (replace_data.event_to_find.note == replace_keyoff_str()) {
                                temp_note = _find_note(new_event.note, note);
                                _valid_note = true;
                            }
                        }

                        if (_valid_note && new_event.note != "???") {
                            if (new_event.note != "\x07\x07\x07") chunk.note = temp_note;
                            else chunk.note = 0;
                        }
                        if (_valid_note && new_event.inst != "??")
                            chunk.instr_def = B(Str2num(_wildcard_str(new_event.inst, byte2hex(old_chunk.instr_def)), 16));
                        if (_valid_note && new_event.fx_1 != "???") {
                            chunk.effect_def = _find_fx(_wildcard_str(new_event.fx_1.substr(0, 1),
                                                                      std::string(1, fx_digit(old_chunk.effect_def)))[0]);
                            chunk.effect = B(Str2num(_wildcard_str(new_event.fx_1.substr(1, 2), byte2hex(old_chunk.effect)), 16));
                        }
                        if (_valid_note && new_event.fx_2 != "???") {
                            chunk.effect_def2 = _find_fx(_wildcard_str(new_event.fx_2.substr(0, 1),
                                                                       std::string(1, fx_digit(old_chunk.effect_def2)))[0]);
                            chunk.effect2 = B(Str2num(_wildcard_str(new_event.fx_2.substr(1, 2), byte2hex(old_chunk.effect2)), 16));
                        }
                    }

                    if (std::memcmp(&chunk, &old_chunk, sizeof(chunk)) == 0) continue;

                    if (replace_prompt && !_replace_all) {
                        PATTERN_position_preview(B(temp3), B(temp1), B(temp2), 1);
                        keyboard_reset_buffer();
                        const std::string text = "PATTERN #" + byte2hex(B(temp3)) + ", LiNE #" + byte2hex(B(temp1)) +
                                                 ", TRACK #" + Num2str(temp2, 10) + "$FOUND OCCURENCE$";
                        uint8_t temp;
                        {
                            PlayerUnlock unlock;
                            if (_1st_choice)
                                temp = Dialog(text, "~R~EPLACE$REPLACE ~A~LL$~S~KiP$S~K~iP ALL$~C~ANCEL$", " REPLACE ", 1);
                            else
                                temp = Dialog(text, "~R~EPLACE$REPLACE ~A~LL$~S~KiP$S~K~iP ALL$CANCEL$", " REPLACE ", 1);
                        }

                        if (dl_environment.keystroke != kESC) {
                            _1st_choice = false;
                            if (temp == 2) _replace_all = true;
                            else if (temp == 3) continue;
                            else if (temp == 4 || temp == 5) {
                                _cancel = true;
                                break;
                            }
                        } else if (!_1st_choice) {
                            continue;
                        } else {
                            _cancel = true;
                            break;
                        }
                    }

                    put_chunk(B(temp3), B(temp1), B(temp2), chunk);
                }
            }
        }

        PATTERN_position_preview(BYTE_NULL, BYTE_NULL, BYTE_NULL, BYTE_NULL);
        p.replay_forbidden = backup_replay_forbidden;
        p.play_status = backup_play_status;
    }

    PATTERN_ORDER_page_refresh(pattord_page);
    PATTERN_page_refresh(pattern_page);
}
