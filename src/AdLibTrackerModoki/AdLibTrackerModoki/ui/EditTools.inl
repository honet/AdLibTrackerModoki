// パターンを編集する道具 (adt2extn.pas の TRANSPOSE / REMAP / LINE_MARKING_SETUP / OCTAVE_CONTROL の移植)
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
