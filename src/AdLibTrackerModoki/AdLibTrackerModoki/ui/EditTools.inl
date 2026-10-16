// パターンを編集する道具 (adt2extn.pas の TRANSPOSE / REMAP / REARRANGE / REPLACE / SONG_VARIABLES / MESSAGE_BOARD / LINE_MARKING_SETUP / OCTAVE_CONTROL の移植)
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

        // Ctrl+O・F1 は画面を戻してから開き直す
        bool reopen = false;
        const auto other_keys = [&](uint16_t key) {
            if (key == kCtrlO) {
                reset_screen();
                OCTAVE_CONTROL();
                reopen = true;
            } else if (key == kF1) {
                reset_screen();
                HELP("remap_dialog");
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

        // F1 は画面を戻してヘルプを出してから開き直す
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
                    case kF1: reset_screen(); HELP("rearrange_dialog"); reopen = true; break;
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
                    case kF1: reset_screen(); HELP("rearrange_dialog"); reopen = true; break;
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

        // F1 は画面を戻してヘルプを出してから開き直す
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
                    case kF1: reset_screen(); HELP("replace_dialog"); reopen = true; break;
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
                    case kF1: reset_screen(); HELP("replace_dialog"); reopen = true; break;
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
                    case kF1: reset_screen(); HELP("replace_dialog"); reopen = true; break;
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
                    case kF1: reset_screen(); HELP("replace_dialog"); reopen = true; break;
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

// --- 曲の設定 (Ctrl+F) ---

constexpr uint8_t dialog_misc_indic = 0x08;
constexpr uint8_t dialog_hid = 0x06;

uint8_t _bpm_xstart = 0;
uint8_t _bpm_ystart = 0;
uint8_t _song_variables_pos = 1;
std::string bpm_inc_str, bpm_dec_str;

// realtime_gfx_poll_proc から呼ぶ (ロックを取った状態)
void _show_bpm_callback_SV()
{
    const FixedSongData& sd = songdata();
    const uint8_t mark_line = P().mark_line;
    double bpm = calc_bpm_speed(sd.tempo, sd.speed, mark_line);
    if (_song_variables_pos == 3) {
        const uint8_t is_num = B(Str2num(is_environment.cur_str, 10));
        if (is_num >= 1) bpm = calc_bpm_speed(is_num, sd.speed, mark_line);
    } else if (_song_variables_pos == 4) {
        const uint8_t is_num = B(Str2num(is_environment.cur_str, 16));
        if (is_num >= 1) bpm = calc_bpm_speed(sd.tempo, is_num, mark_line);
    }
    ShowStr(screen_ptr, _bpm_xstart, _bpm_ystart, ExpStrL(Bpm2str(bpm) + " BPM", 9, ' '), dialog_background + dialog_misc_indic);
}

// テンポ・スピードの入力欄の ext_proc。+/- で変えたときの BPM を出す
void _show_current_bpm_with_hints()
{
    PlayerLock lock;
    const FixedSongData& sd = songdata();
    const uint8_t mark_line = P().mark_line;
    const auto hint = [](const char* sign, double bpm) {
        return std::string("`(`~") + sign + "~`)` " + ExpStrL(Bpm2str(bpm) + " `BPM`", 11, ' ');
    };
    if (_song_variables_pos == 3 || _song_variables_pos == 4) {
        const bool is_tempo = _song_variables_pos == 3;
        const uint8_t is_num = B(Str2num(is_environment.cur_str, is_tempo ? 10 : 16));
        const auto bpm_of = [&](int value) {
            return is_tempo ? calc_bpm_speed(B(value), sd.speed, mark_line) : calc_bpm_speed(sd.tempo, B(value), mark_line);
        };
        bpm_inc_str = is_num >= 1 && is_num <= 254 ? hint("+", bpm_of(is_num + 1)) : ExpStrL("", 13, ' ');
        bpm_dec_str = is_num >= 2 ? hint("-", bpm_of(is_num - 1)) : ExpStrL("", 13, ' ');
    }

    const auto show = [](uint8_t dy, const std::string& str) {
        ShowC3Str(screen_ptr, B(_bpm_xstart - 4), B(_bpm_ystart + dy), str, dialog_background + dialog_context_dis,
                  dialog_background + dialog_title, dialog_background + dialog_contxt_dis2);
    };
    show(1, bpm_inc_str);
    if (CutStr(bpm_inc_str) != "") show(2, bpm_dec_str);
    else {
        show(1, bpm_dec_str);
        show(2, bpm_inc_str);
    }
}

// 空でない最後の楽器 (adt2unit.pas の count_instruments)
uint8_t count_instruments()
{
    const FixedSongData& sd = songdata();
    uint8_t instruments = 255;
    while (instruments > 0 && is_empty_data(&at1(sd.instr_data, instruments), INSTRUMENT_SIZE)) --instruments;
    return instruments;
}

std::string truncate_string(std::string str)
{
    while (!str.empty() && (str.back() == '\0' || str.back() == ' ' || str.back() == '\xFF')) str.pop_back();
    return str;
}

void SONG_VARIABLES()
{
    static const uint16_t new_keys[29] = { kF1,   kESC,  kENTER, kTAB,  kShTAB, kUP,   kDOWN, kCtENTR, kAltN, kAltE,
                                           kAltT, kAltS, kAltR,  kAltD, kAltO,  kAltI, kAltA, kAltL,   kAltB, kAltH,
                                           kAltF, kAltX, kAltU,  kAltK, kAltG,  kAltM, kAltC, kAltV,   kAltP };
    constexpr uint8_t RANGE_PAN_LO = 18, RANGE_PAN_HI = 77;
    constexpr uint8_t RANGE_4OP_LO = 78, RANGE_4OP_HI = 83;
    constexpr uint8_t RANGE_LCK_LO = 84, RANGE_LCK_HI = 163;
    const auto in_pan = [](uint8_t p) { return p >= RANGE_PAN_LO && p <= RANGE_PAN_HI; };
    const auto in_4op = [](uint8_t p) { return p >= RANGE_4OP_LO && p <= RANGE_4OP_HI; };
    const auto in_lck = [](uint8_t p) { return p >= RANGE_LCK_LO && p <= RANGE_LCK_HI; };

    static const char _on_off[2] = { '\xFA', '\xFB' };
    static const char* const _4op_str[6] = { "1 \xF1" "2   ", "3 \xF1" "4   ", "5 \xF1" "6   ",
                                             "10\xF1" "11  ", "12\xF1" "13  ", "14\xF1" "15  " };
    static const char* const voice_pan_str[4] = { "~\xFA~\xFA\x0B~\x0B~\x0B\xFA~\xFA~", "~\x0B~\x0B\x0B~\xFA~\xFA\xFA~\xFA~",
                                                  "~\xFA~\xFA\xFA~\xFA~\x0B\x0B~\x0B~", "\xFA\xFA\xFA\xFA\xFA\xFA\xFA" };
    static const uint8_t _4op_main_chan[6] = { 2, 4, 6, 11, 13, 15 };
    static const uint8_t _pan_pos[3] = { 1, 0, 2 };
    static const uint8_t _left_pos_pan[60] = {
        2,   18, 19, 87,  21, 22, 91,  24, 25, 3,   27, 28, 5,   30, 31, 17,  33, 34, 107, 36,
        37,  78, 39, 40,  79, 42, 43,  80, 45, 46,  81, 48, 49,  82, 51, 52,  83, 54, 55,  12,
        57,  58, 139, 60, 61, 13, 63,  64, 14, 66,  67, 151, 69, 70, 15, 72,  73, 16, 75,  76 };
    static const uint8_t _left_pos_lck[80] = {
        20, 84,  85,  86,  23, 88,  89,  90,  26, 92,  93,  94,  29, 96,  97,  98,  32, 100, 101, 102,
        35, 104, 105, 106, 38, 108, 109, 110, 41, 112, 113, 114, 44, 116, 117, 118, 47, 120, 121, 122,
        50, 124, 125, 126, 53, 128, 129, 130, 56, 132, 133, 134, 59, 136, 137, 138, 62, 140, 141, 142,
        65, 144, 145, 146, 68, 148, 149, 150, 71, 152, 153, 154, 74, 156, 157, 158, 77, 160, 161, 162 };
    static const uint8_t _right_pos_pan[60] = {
        19, 20, 84,  22, 23, 88,  25, 26, 92,  28, 29, 96,  31, 32, 100, 34, 35, 104, 37, 38,
        108, 40, 41, 112, 43, 44, 116, 46, 47, 120, 49, 50, 124, 52, 53, 128, 55, 56, 132, 58,
        59, 136, 61, 62, 140, 64, 65, 144, 67, 68, 148, 70, 71, 152, 73, 74, 156, 76, 77, 160 };
    // 右端の列の行き先はトラック数で変わる ((temp - 1) * 4 + 4 の要素を書き換える)
    static uint8_t _right_pos_lck[80] = {
        85,  86,  87,  0, 89,  90,  91,  0, 93,  94,  95,  0, 97,  98,  99,  0, 101, 102, 103, 0,
        105, 106, 107, 0, 109, 110, 111, 0, 113, 114, 115, 0, 117, 118, 119, 0, 121, 122, 123, 0,
        125, 126, 127, 0, 129, 130, 131, 0, 133, 134, 135, 0, 137, 138, 139, 0, 141, 142, 143, 0,
        145, 146, 147, 0, 149, 150, 151, 0, 153, 154, 155, 0, 157, 158, 159, 0, 161, 162, 163, 1 };
    static const uint8_t _right_pos_lck_def[19][2] = {
        { 21, 3 }, { 24, 3 }, { 3, 3 },   { 4, 4 },   { 17, 17 }, { 36, 6 },  { 6, 6 },   { 7, 7 },   { 8, 8 }, { 81, 81 },
        { 82, 82 }, { 9, 9 }, { 10, 10 }, { 60, 13 }, { 13, 13 }, { 14, 14 }, { 69, 15 }, { 15, 15 }, { 16, 16 } };
    static uint8_t _left_pos_4op[6] = { 6, 7, 8, 0, 0, 11 };
    static uint8_t _right_pos_4op[6] = { 0, 0, 0, 0, 0, 0 };
    static uint8_t _up_pos_4op[6] = { 0, 78, 79, 80, 81, 82 };
    static const uint8_t _down_pos_4op[6] = { 79, 80, 81, 82, 83, 10 };
    static const uint8_t _right_pos_4op_def[6][2] = { { 39, 7 }, { 42, 8 }, { 45, 81 }, { 48, 82 }, { 51, 9 }, { 54, 10 } };
    static uint8_t _old_pos_pan = RANGE_PAN_LO + 1;
    static uint8_t _old_pos_4op = RANGE_4OP_LO;
    static uint8_t _old_pos_lck = RANGE_LCK_LO;

    uint8_t pos, pos_4op;
    uint8_t order_entries, patterns, instruments;
    uint8_t xstart = 0, ystart = 0;
    {
        PlayerLock lock;
        A2Player& p = P();
        FixedSongData& sd = songdata();
        songdata_crc = Update32(&sd, sizeof(FixedSongData), 0);
        count_order(order_entries);
        patterns = count_patterns();
        instruments = count_instruments();
        // オリジナルの min は大きい方を返す
        pos = B(std::max<int32_t>(get_bank_position("?song_variables_window?pos", -1), 1));
        pos_4op = B(std::max<int32_t>(get_bank_position("?song_variables_window?pos_4op", -1), 0));
        if (calc_max_speedup(sd.tempo) < sd.macro_speedup) {
            sd.macro_speedup = calc_max_speedup(sd.tempo);
            if (p.play_status == PlayStatus::Stopped) p.macro_speedup = sd.macro_speedup;
        }
    }

    const auto _check_key_shortcuts = [&]() {
        if (in_pan(pos)) {
            _old_pos_pan = B(RANGE_PAN_LO + 1 + (pos - RANGE_PAN_LO) / 3 * 3);
            _old_pos_lck = B(RANGE_LCK_LO + (pos - RANGE_PAN_LO) / 3 * 4);
        } else if (in_4op(pos)) {
            _old_pos_4op = pos;
        } else if (in_lck(pos)) {
            _old_pos_pan = B(RANGE_PAN_LO + 1 + (pos - RANGE_LCK_LO) / 4 * 3);
            _old_pos_lck = B(RANGE_LCK_LO + pos % 4 + (pos - RANGE_LCK_LO) / 4 * 4);
        }
        const A2Player& p = P();
        const auto lck_pos = [&](uint8_t col) {
            if (in_lck(_old_pos_lck)) return B(RANGE_LCK_LO + col + (_old_pos_lck - RANGE_LCK_LO) / 4 * 4);
            return B(RANGE_LCK_LO + col);
        };
        switch (is_environment.keystroke) {
        case kAltN: pos = 1; break;
        case kAltE: pos = 2; break;
        case kAltT: pos = 3; break;
        case kAltS: pos = 4; break;
        case kAltR: pos = 5; break;
        case kAltD: pos = 17; break;
        case kAltO: pos = 6; break;
        case kAltI: pos = 7; break;
        case kAltA: pos = 8; break;
        case kAltL: pos = p.tremolo_depth == 0 ? 9 : 10; break;
        case kAltB: pos = p.vibrato_depth == 0 ? 11 : 12; break;
        case kAltH: pos = 13; break;
        case kAltF: pos = 14; break;
        case kAltX: pos = 15; break;
        case kAltU: pos = 16; break;
        case kAltK:
            pos = in_4op(_old_pos_4op) ? _old_pos_4op : RANGE_4OP_LO;
            pos_4op = 0;
            break;
        case kAltG: pos = in_pan(_old_pos_pan) ? _old_pos_pan : B(RANGE_PAN_LO + 1); break;
        case kAltM: pos = lck_pos(0); break;
        case kAltC: pos = lck_pos(1); break;
        case kAltV: pos = lck_pos(2); break;
        case kAltP: pos = lck_pos(3); break;
        }
    };

    const auto set_cursor = []() {
        if (use_large_cursor) WideCursor();
        else ThinCursor();
    };

    const auto refresh_pages = []() {
        PlayerLock lock;
        force_scrollbars = true;
        PATTERN_ORDER_page_refresh(pattord_page);
        PATTERN_page_refresh(pattern_page);
        force_scrollbars = false;
    };

    for (;;) {   // _jmp1
        if (_force_program_quit) return;

        ScreenMemCopy(screen_ptr, screen_backup);
        HideCursor();

        ScreenMemCopy(screen_ptr, temp_screen);
        centered_frame_vdest = temp_screen;
        centered_frame(xstart, ystart, 79, 26, " SONG VARiABLES ", dialog_background + dialog_border,
                       dialog_background + dialog_title, frame_double);
        centered_frame_vdest = screen_ptr;

        move_to_screen_data = temp_screen;
        move_to_screen_area[1] = xstart;
        move_to_screen_area[2] = ystart;
        move_to_screen_area[3] = B(xstart + 79 + 2);
        move_to_screen_area[4] = B(ystart + 26 + 1);
        move2screen_alt();

        move_to_screen_area[1] = B(xstart + 1);
        move_to_screen_area[2] = B(ystart + 1);
        move_to_screen_area[3] = B(xstart + 78);
        move_to_screen_area[4] = B(ystart + 25);

        _bpm_xstart = B(xstart + 39);
        _bpm_ystart = B(ystart + 7);

        void (*old_bpm_proc)();
        {
            PlayerLock lock;
            old_bpm_proc = _show_bpm_realtime_proc;
            _show_bpm_realtime_proc = _show_bpm_callback_SV;
        }

        ShowCStr(temp_screen, B(xstart + 2), B(ystart + 6), "iNSTRUMENTS: ~" + Num2str(instruments, 10) + "/255~  ",
                 dialog_background + dialog_text, dialog_background + dialog_context_dis);
        ShowCStr(temp_screen, B(xstart + 25), B(ystart + 6), "PATTERNS: ~" + Num2str(patterns, 10) + "/" + Num2str(128, 10) + "~  ",
                 dialog_background + dialog_text, dialog_background + dialog_context_dis);
        ShowCStr(temp_screen, B(xstart + 2), B(ystart + 7), "ORDER LiST ENTRiES: ~" + Num2str(order_entries, 10) + "/128~  ",
                 dialog_background + dialog_text, dialog_background + dialog_context_dis);
        ShowStr(temp_screen, B(xstart + 51), B(ystart + 2), "iNiTiAL LOCK SETTiNGS", dialog_background + dialog_context_dis);
        ShowStr(temp_screen, B(xstart + 51), B(ystart + 3), ExpStrL("", 27, '\x9A'), dialog_background + dialog_context_dis);

        // オリジナルは 7 個だけ保存して 29 個書き込むため、閉じた後も 8 番目以降は Alt+N などのままになる
        uint16_t old_keys[7];
        std::memcpy(old_keys, is_setting.terminate_keys, sizeof(old_keys));
        std::memcpy(is_setting.terminate_keys, new_keys, sizeof(new_keys));

        {
            PlayerLock lock;
            FixedSongData& sd = songdata();
            sd.songname.assign(truncate_string(sd.songname.str()));
            sd.composer.assign(truncate_string(sd.composer.str()));
        }

        if (!_force_program_quit) {
            do {
                {
                    PlayerLock lock;
                    const A2Player& p = P();
                    const FixedSongData& sd = songdata();

                    _left_pos_4op[3] = sd.nm_tracks < 11 ? 80 : 123;
                    _left_pos_4op[4] = sd.nm_tracks < 12 ? 81 : 127;
                    for (int temp = 1; temp <= 6; ++temp)
                        _right_pos_4op[temp - 1] = _right_pos_4op_def[temp - 1][sd.nm_tracks < 8 + temp - 1 ? 1 : 0];
                    _up_pos_4op[0] = sd.nm_tracks < 7 ? 17 : 36;
                    for (int temp = 1; temp <= 19; ++temp)
                        _right_pos_lck[(temp - 1) * 4 + 3] = _right_pos_lck_def[temp - 1][sd.nm_tracks < temp + 1 ? 1 : 0];

                    // attr[n]: 下位が文字の色、上位がホットキーの色
                    uint16_t attr[164] = {};
                    constexpr uint16_t hi = dialog_hi_text + (dialog_hi_text << 8);
                    constexpr uint16_t lo = dialog_text + (dialog_title << 8);
                    for (int temp2 = 1; temp2 <= 18; ++temp2) attr[temp2] = pos == temp2 ? hi : lo;
                    if (pos == 4) attr[5] = 0;
                    else if (pos == 5) attr[4] = 0;
                    else attr[5] = 0;
                    if (pos == 9) attr[10] = 0;
                    else if (pos == 10) attr[9] = 0;
                    else attr[10] = 0;
                    if (pos == 11) attr[12] = 0;
                    else if (pos == 12) attr[11] = 0;
                    else attr[11] = 0;
                    attr[RANGE_PAN_LO] = in_pan(pos) ? hi : lo;
                    attr[RANGE_4OP_LO] = in_4op(pos) ? hi : lo;
                    for (int col = 0; col <= 3; ++col) attr[RANGE_LCK_LO + col] = in_lck(pos) && pos % 4 == col ? hi : lo;

                    const auto A = [](uint16_t a) { return B(dialog_background + LO(a)); };
                    const auto H = [](uint16_t a) { return B(dialog_background + HI(a)); };
                    const auto A2 = [](uint16_t a, uint16_t b) { return B(dialog_background + LO(a) + LO(b)); };
                    const auto H2 = [](uint16_t a, uint16_t b) { return B(dialog_background + HI(a) + HI(b)); };

                    ShowCStr(temp_screen, B(xstart + 34), B(ystart + 12), "\x04\x03 TRAC~K~S  \x04\x03+", A(attr[RANGE_4OP_LO]),
                             H(attr[RANGE_4OP_LO]));

                    for (int temp = 1; temp <= 6; ++temp) {
                        if ((sd.flag_4op | (1 << (temp - 1))) == sd.flag_4op)
                            ShowC3Str(temp_screen, B(xstart + 34), B(ystart + 13 + temp - 1),
                                      std::string("[~\xFB~] ") + _4op_str[temp - 1] + "`[ ]`", dialog_background + dialog_text,
                                      dialog_background + dialog_item, dialog_background + dialog_text);
                        else
                            ShowC3Str(temp_screen, B(xstart + 34), B(ystart + 13 + temp - 1),
                                      std::string("[~ ~] ") + _4op_str[temp - 1] + "`[ ]`", dialog_background + dialog_text,
                                      dialog_background + dialog_item, dialog_background + dialog_item_dis);
                    }

                    ShowCStr(temp_screen, B(xstart + 51), B(ystart + 4), "PANNiN~G~", A(attr[RANGE_PAN_LO]), H(attr[RANGE_PAN_LO]));
                    ShowStr(temp_screen, B(xstart + 51), B(ystart + 5), "\xAA  c  \xAB", A(attr[18]));

                    static const char* const lck_titles[4] = { "~M~\x1F", "~C~\x1F", "~V~\x1F", "~P~\x1F" };
                    static const char* const lck_marks[4] = { "\x0A", "\x0A", "+", "+" };
                    for (int col = 0; col <= 3; ++col) {
                        ShowVCStr(temp_screen, B(xstart + 64 + col * 4), B(ystart + 4), lck_titles[col], A(attr[RANGE_LCK_LO + col]),
                                  H(attr[RANGE_LCK_LO + col]));
                        ShowVStr(temp_screen, B(xstart + 65 + col * 4), B(ystart + 4), lck_marks[col], A(attr[RANGE_LCK_LO + col]));
                    }

                    std::string temps;
                    for (int temp = 1; temp <= 6; ++temp) {
                        const uint8_t main_chan = _4op_main_chan[temp - 1];
                        if ((sd.lock_flags[main_chan - 1] | 0x40) == sd.lock_flags[main_chan - 1] ||
                            (sd.lock_flags[main_chan - 2] | 0x40) == sd.lock_flags[main_chan - 2])
                            temps += '\xFB';
                        else temps += ' ';
                    }
                    ShowVStr(temp_screen, B(xstart + 46), B(ystart + 13), temps, dialog_background + dialog_item);

                    for (int temp = 1; temp <= 20; ++temp) {
                        if (temp <= sd.nm_tracks) {
                            const uint8_t flags = sd.lock_flags[temp - 1];
                            ShowCStr(temp_screen, B(xstart + 51), B(ystart + 6 + temp - 1), voice_pan_str[flags & 3],
                                     dialog_background + dialog_text, dialog_background + dialog_item);
                            ShowCStr(temp_screen, B(xstart + 60), B(ystart + 6 + temp - 1),
                                     "~" + ExpStrL(Num2str(temp, 10), 2, ' ') + "~  " + _on_off[flags >> 3 & 1] + " ~\xF6~ " +
                                         _on_off[flags >> 2 & 1] + " ~\xF6~ " + _on_off[flags >> 4 & 1] + " ~\xF6~ " +
                                         _on_off[flags >> 5 & 1],
                                     dialog_background + dialog_item, dialog_background + dialog_context_dis);
                        } else {
                            ShowStr(temp_screen, B(xstart + 51), B(ystart + 6 + temp - 1),
                                    std::string(voice_pan_str[3]) + "  " + ExpStrL(Num2str(temp, 10), 2, ' ') +
                                        "  \xFA \xF6 \xFA \xF6 \xFA \xF6 \xFA",
                                    dialog_background + dialog_hid);
                        }
                    }

                    temps.clear();
                    for (int temp = 1; temp <= sd.nm_tracks; ++temp) {
                        if (p.percussion_mode && temp >= 16 && temp <= 20) {
                            temps += _perc_char[temp - 16];
                            continue;
                        }
                        // 4OP の組の 1 つ目は #172、2 つ目は #173
                        static const int pair_bit[21] = { 0, 1, 1, 2, 2, 4, 4, 0, 0, 0, 8, 8, 0x10, 0x10, 0x20, 0x20, 0, 0, 0, 0, 0 };
                        static const char pair_chr[21] = { 0,      '\xAC', '\xAD', '\xAC', '\xAD', '\xAC', '\xAD', 0, 0, 0, '\xAC',
                                                           '\xAD', '\xAC', '\xAD', '\xAC', '\xAD', 0,      0,      0, 0, 0 };
                        const int bit = temp <= 20 ? pair_bit[temp] : 0;
                        temps += bit != 0 && (sd.flag_4op | bit) == sd.flag_4op ? pair_chr[temp] : ' ';
                    }
                    ShowVStr(temp_screen, B(xstart + 50), B(ystart + 6), ExpStrR(temps, 20, ' '), dialog_background + dialog_misc_indic);

                    ShowCStr(temp_screen, B(xstart + 2), B(ystart + 1), "SONG~N~AME", A(attr[1]), H(attr[1]));
                    ShowCStr(temp_screen, B(xstart + 2), B(ystart + 3), "COMPOS~E~R", A(attr[2]), H(attr[2]));
                    ShowCStr(temp_screen, B(xstart + 2), B(ystart + 9), "SONG ~T~EMPO", A(attr[3]), H(attr[3]));
                    ShowCStr(temp_screen, B(xstart + 2), B(ystart + 10), "SONG ~S~PEED", A2(attr[4], attr[5]), H2(attr[4], attr[5]));
                    ShowC3Str(temp_screen, B(xstart + 26), B(ystart + 10), "[ ] ~`R`ESET~", dialog_background + dialog_text,
                              A2(attr[4], attr[5]), H2(attr[4], attr[5]));
                    ShowC3Str(temp_screen, B(xstart + 2), B(ystart + 11), "~MACRO`D`EF.~ \x07", dialog_background + dialog_text,
                              A(attr[17]), H(attr[17]));

                    ShowCStr(temp_screen, B(xstart + 31), B(ystart + 7),
                             "RHYTHM: ~" + ExpStrL(Bpm2str(calc_bpm_speed(sd.tempo, sd.speed, p.mark_line)) + " BPM", 9, ' '),
                             dialog_background + dialog_text, dialog_background + dialog_misc_indic);

                    const auto check = [&](uint8_t x, uint8_t y, bool on) {
                        ShowStr(temp_screen, x, y, on ? "\xFB" : " ", dialog_background + dialog_item);
                    };
                    check(B(xstart + 27), B(ystart + 10), p.speed_update);

                    ShowC3Str(temp_screen, B(xstart + 2), B(ystart + 13), "[ ] ~TRACK VOLUME L`O`CK~", dialog_background + dialog_text,
                              A(attr[6]), H(attr[6]));
                    ShowC3Str(temp_screen, B(xstart + 2), B(ystart + 14), "[ ] ~TRACK PANN`i`NG LOCK~", dialog_background + dialog_text,
                              A(attr[7]), H(attr[7]));
                    ShowC3Str(temp_screen, B(xstart + 2), B(ystart + 15), "[ ] ~VOLUME PE`A`K LOCK~", dialog_background + dialog_text,
                              A(attr[8]), H(attr[8]));
                    check(B(xstart + 3), B(ystart + 13), p.lockvol);
                    check(B(xstart + 3), B(ystart + 14), p.panlock);
                    check(B(xstart + 3), B(ystart + 15), p.lockVP);

                    ShowCStr(temp_screen, B(xstart + 2), B(ystart + 17), "TREMO~L~O DEPTH", A2(attr[9], attr[10]), H2(attr[9], attr[10]));
                    ShowStr(temp_screen, B(xstart + 2), B(ystart + 18), "( ) 1 dB", dialog_background + dialog_text);
                    ShowStr(temp_screen, B(xstart + 2), B(ystart + 19), "( ) 4.8 dB", dialog_background + dialog_text);
                    ShowVStr(temp_screen, B(xstart + 3), B(ystart + 18), p.tremolo_depth == 0 ? "\x0B " : " \x0B",
                             dialog_background + dialog_item);

                    ShowCStr(temp_screen, B(xstart + 18), B(ystart + 17), "Vi~B~RATO DEPTH", A2(attr[11], attr[12]),
                             H2(attr[11], attr[12]));
                    ShowStr(temp_screen, B(xstart + 18), B(ystart + 18), "( ) 7%", dialog_background + dialog_text);
                    ShowStr(temp_screen, B(xstart + 18), B(ystart + 19), "( ) 14%", dialog_background + dialog_text);
                    ShowVStr(temp_screen, B(xstart + 19), B(ystart + 18), p.vibrato_depth == 0 ? "\x0B " : " \x0B",
                             dialog_background + dialog_item);

                    ShowCStr(temp_screen, B(xstart + 2), B(ystart + 21), "PATTERN LENGT~H~", A(attr[13]), H(attr[13]));
                    ShowCStr(temp_screen, B(xstart + 2), B(ystart + 22), "NUMBER O~F~ TRACKS", A(attr[14]), H(attr[14]));

                    ShowC3Str(temp_screen, B(xstart + 2), B(ystart + 24),
                              "[ ] ~PERCUSSiON TRACK E`X`TENSiON (\xA0,\xA1,\xA2,\xA3,\xA4)~", dialog_background + dialog_text,
                              A(attr[15]), H(attr[15]));
                    check(B(xstart + 3), B(ystart + 24), p.percussion_mode);

                    ShowC3Str(temp_screen, B(xstart + 2), B(ystart + 25), "[ ] ~VOL`U`ME SCALiNG~", dialog_background + dialog_text,
                              A(attr[16]), H(attr[16]));
                    check(B(xstart + 3), B(ystart + 25), p.volume_scaling);

                    is_setting.append_enabled = true;
                    is_environment.locate_pos = 1;

                    ShowStr(temp_screen, B(xstart + 2), B(ystart + 2), ExpStrR(sd.songname.str(), 46, ' '),
                            dialog_input_bckg + dialog_input);
                    ShowStr(temp_screen, B(xstart + 2), B(ystart + 4), ExpStrR(sd.composer.str(), 46, ' '),
                            dialog_input_bckg + dialog_input);
                    ShowCStr(temp_screen, B(xstart + 13), B(ystart + 9), ExpStrR(Num2str(sd.tempo, 10), 4, ' ') + "~ {1..255}~",
                             dialog_input_bckg + dialog_input, dialog_background + dialog_text);
                    ShowCStr(temp_screen, B(xstart + 13), B(ystart + 10), ExpStrR(Num2str(sd.speed, 16), 4, ' ') + "~ {1..FF}~",
                             dialog_input_bckg + dialog_input, dialog_background + dialog_text);
                    ShowCStr(temp_screen, B(xstart + 13), B(ystart + 11),
                             "~" + ExpStrR(Num2str(sd.macro_speedup, 10), 4, ' ') + "~ {1.." +
                                 Num2str(calc_max_speedup(sd.tempo), 10) + "}   ",
                             dialog_background + dialog_text, dialog_input_bckg + dialog_input);
                    ShowCStr(temp_screen, B(xstart + 19), B(ystart + 21), ExpStrR(Num2str(sd.patt_len, 10), 3, ' ') + "~ {1..256}~",
                             dialog_input_bckg + dialog_input, dialog_background + dialog_text);
                    ShowCStr(temp_screen, B(xstart + 19), B(ystart + 22), ExpStrR(Num2str(sd.nm_tracks, 10), 3, ' ') + "~ {1..20}~",
                             dialog_input_bckg + dialog_input, dialog_background + dialog_text);
                }

                move2screen_alt();
                _song_variables_pos = pos;

                const FixedSongData& sd = songdata();
                std::string temps;
                const uint8_t input_attr = dialog_input_bckg + dialog_input;
                const uint8_t def_attr = dialog_def_bckg + dialog_def;
                const auto key = [&]() { return is_environment.keystroke; };
                // チェックボックスなど: カーソルを置いてキーを待つ
                const auto wait_key = [&](uint8_t x, uint8_t y) {
                    GotoXY(x, y);
                    set_cursor();
                    is_environment.keystroke = getkey();
                    _check_key_shortcuts();
                };

                if (pos == 1 || pos == 2) {
                    is_setting.character_set = CharRange(32, 255);
                    temps = InputStr(pos == 1 ? sd.songname.str() : sd.composer.str(), B(xstart + 2), B(ystart + (pos == 1 ? 2 : 4)),
                                     42, 42, input_attr, def_attr);
                    {
                        PlayerLock lock;
                        if (pos == 1) songdata().songname.assign(truncate_string(temps));
                        else songdata().composer.assign(truncate_string(temps));
                    }
                    _check_key_shortcuts();
                    if (pos == 1) {
                        if (key() == kENTER || key() == kTAB || key() == kDOWN) pos = 2;
                        else if (key() == kUP) pos = 16;
                        else if (key() == kShTAB) pos = in_lck(_old_pos_lck) ? B(RANGE_LCK_LO + 3 + (_old_pos_lck - RANGE_LCK_LO) / 4 * 4)
                                                                             : B(RANGE_LCK_LO + 3);
                    } else if (pos == 2) {
                        if (key() == kENTER || key() == kTAB || key() == kDOWN) pos = 3;
                        else if (key() == kUP || key() == kShTAB) pos = 1;
                    }
                } else if (pos == 3) {
                    is_setting.character_set = DEC_NUM_CHARSET;
                    is_environment.ext_proc = _show_current_bpm_with_hints;
                    is_environment.min_num = 1;
                    is_environment.max_num = 255;
                    do {
                        temps = InputStr(Num2str(sd.tempo, 10), B(xstart + 13), B(ystart + 9), 3, 3, input_attr, def_attr);
                    } while (!(key() == kESC || (Str2num(temps, 10) > 0 && Str2num(temps, 10) < 256)));
                    is_environment.ext_proc = nullptr;
                    {
                        PlayerLock lock;
                        A2Player& p = P();
                        FixedSongData& wsd = songdata();
                        if (Str2num(temps, 10) > 0 && Str2num(temps, 10) < 256) {
                            wsd.tempo = B(Str2num(temps, 10));
                            if (p.play_status == PlayStatus::Stopped) p.tempo = wsd.tempo;
                        }
                        if (calc_max_speedup(wsd.tempo) < wsd.macro_speedup) wsd.macro_speedup = calc_max_speedup(wsd.tempo);
                    }
                    _check_key_shortcuts();
                    if (key() == kENTER || key() == kTAB || key() == kDOWN) pos = 4;
                    else if (key() == kUP || key() == kShTAB) pos = 2;
                } else if (pos == 4) {
                    is_setting.character_set = HEX_NUM_CHARSET;
                    is_environment.ext_proc = _show_current_bpm_with_hints;
                    is_environment.min_num = 1;
                    is_environment.max_num = 255;
                    const auto valid = [&]() { return Str2num(temps, 16) >= 1 && Str2num(temps, 16) <= 255; };
                    do {
                        temps = InputStr(Num2str(sd.speed, 16), B(xstart + 13), B(ystart + 10), 2, 2, input_attr, def_attr);
                    } while (!(key() == kESC || valid()));
                    is_environment.ext_proc = nullptr;
                    if (valid()) {
                        PlayerLock lock;
                        A2Player& p = P();
                        songdata().speed = B(Str2num(temps, 16));
                        if (p.play_status == PlayStatus::Stopped) p.speed = songdata().speed;
                    }
                    _check_key_shortcuts();
                    if (key() == kENTER || key() == kTAB) pos = 5;
                    else if (key() == kDOWN) pos = 17;
                    else if (key() == kUP || key() == kShTAB) pos = 3;
                } else if (pos == 5) {
                    wait_key(B(xstart + 27), B(ystart + 10));
                    switch (key()) {
                    case kUP: pos = 3; break;
                    case kLEFT: case kShTAB: pos = 4; break;
                    case kDOWN: case kTAB: case kENTER: pos = 17; break;
                    case kRIGHT: pos = sd.nm_tracks < 5 ? 17 : 30; break;
                    case kSPACE: {
                        PlayerLock lock;
                        P().speed_update = !P().speed_update;
                        break;
                    }
                    }
                } else if (pos == 17) {
                    is_setting.character_set = DEC_NUM_CHARSET;
                    is_environment.min_num = 1;
                    is_environment.max_num = calc_max_speedup(sd.tempo);
                    const auto valid = [&]() { return Str2num(temps, 10) >= 1 && Str2num(temps, 10) <= calc_max_speedup(sd.tempo); };
                    do {
                        temps = InputStr(Num2str(sd.macro_speedup, 10), B(xstart + 13), B(ystart + 11), 4, 4, input_attr, def_attr);
                    } while (!(key() == kESC || valid()));
                    if (valid()) {
                        PlayerLock lock;
                        songdata().macro_speedup = static_cast<uint16_t>(Str2num(temps, 10));
                    }
                    _check_key_shortcuts();
                    if (key() == kENTER || key() == kTAB || key() == kDOWN) pos = 6;
                    else if (key() == kUP) pos = 4;
                    else if (key() == kShTAB) pos = 5;
                } else if (pos == 6) {
                    wait_key(B(xstart + 3), B(ystart + 13));
                    switch (key()) {
                    case kLEFT: pos = sd.nm_tracks < 7 ? 17 : 111; break;
                    case kUP: case kShTAB: pos = 17; break;
                    case kDOWN: case kTAB: case kENTER: pos = 7; break;
                    case kRIGHT: pos = _old_pos_4op; break;
                    case kSPACE: {
                        PlayerLock lock;
                        P().lockvol = !P().lockvol;
                        break;
                    }
                    }
                } else if (pos == 7) {
                    wait_key(B(xstart + 3), B(ystart + 14));
                    switch (key()) {
                    case kUP: case kShTAB: pos = 6; break;
                    case kLEFT: pos = sd.nm_tracks < 8 ? 78 : 115; break;
                    case kTAB: case kENTER: case kDOWN: pos = 8; break;
                    case kRIGHT: pos = 79; break;
                    case kSPACE: {
                        PlayerLock lock;
                        P().panlock = !P().panlock;
                        break;
                    }
                    }
                } else if (pos == 8) {
                    wait_key(B(xstart + 3), B(ystart + 15));
                    switch (key()) {
                    case kUP: case kShTAB: pos = 7; break;
                    case kLEFT: pos = sd.nm_tracks < 9 ? 79 : 119; break;
                    case kDOWN: pos = 9; break;
                    case kTAB: case kENTER: pos = P().tremolo_depth == 0 ? 9 : 10; break;
                    case kRIGHT: pos = 80; break;
                    case kSPACE: {
                        PlayerLock lock;
                        P().lockVP = !P().lockVP;
                        break;
                    }
                    }
                } else if (pos == 9 || pos == 10) {
                    wait_key(B(xstart + 3), B(ystart + (pos == 9 ? 18 : 19)));
                    const uint8_t vib_pos = P().vibrato_depth == 0 ? 11 : 12;
                    if (pos == 9) {
                        switch (key()) {
                        case kUP: case kShTAB: pos = 8; break;
                        case kLEFT: pos = sd.nm_tracks < 12 ? 82 : 131; break;
                        case kRIGHT: case kTAB: case kENTER: pos = vib_pos; break;
                        case kDOWN: pos = 10; break;
                        case kSPACE: {
                            PlayerLock lock;
                            P().tremolo_depth = 0;
                            break;
                        }
                        }
                    } else {
                        switch (key()) {
                        case kUP: pos = 9; break;
                        case kShTAB: pos = 8; break;
                        case kDOWN: pos = 13; break;
                        case kTAB: case kENTER: pos = vib_pos; break;
                        case kLEFT: pos = sd.nm_tracks < 13 ? 83 : 135; break;
                        case kRIGHT: pos = 12; break;
                        case kSPACE: {
                            PlayerLock lock;
                            P().tremolo_depth = 1;
                            break;
                        }
                        }
                    }
                } else if (pos == 11 || pos == 12) {
                    wait_key(B(xstart + 19), B(ystart + (pos == 11 ? 18 : 19)));
                    if (pos == 11) {
                        switch (key()) {
                        case kUP: pos = 8; break;
                        case kShTAB: pos = P().tremolo_depth == 0 ? 9 : 10; break;
                        case kDOWN: pos = 12; break;
                        case kTAB: case kENTER: pos = 13; break;
                        case kLEFT: pos = 9; break;
                        case kRIGHT: pos = 83; break;
                        case kSPACE: {
                            PlayerLock lock;
                            P().vibrato_depth = 0;
                            break;
                        }
                        }
                    } else {
                        switch (key()) {
                        case kUP: pos = 11; break;
                        case kShTAB: pos = 9; break;
                        case kDOWN: case kTAB: case kENTER: pos = 13; break;
                        case kLEFT: pos = 10; break;
                        case kRIGHT: pos = sd.nm_tracks < 14 ? 13 : 57; break;
                        case kSPACE: {
                            PlayerLock lock;
                            P().vibrato_depth = 1;
                            break;
                        }
                        }
                    }
                } else if (pos == 13) {
                    is_setting.character_set = DEC_NUM_CHARSET;
                    is_environment.min_num = 1;
                    is_environment.max_num = 256;
                    const auto valid = [&]() { return Str2num(temps, 10) >= 1 && Str2num(temps, 10) <= 256; };
                    do {
                        temps = InputStr(Num2str(sd.patt_len, 10), B(xstart + 19), B(ystart + 21), 3, 3, input_attr, def_attr);
                    } while (!(key() == kESC || valid()));
                    if (valid()) {
                        {
                            PlayerLock lock;
                            A2Player& p = P();
                            const uint16_t new_len = static_cast<uint16_t>(Str2num(temps, 10));
                            if (p.current_line <= new_len) {
                                songdata().patt_len = new_len;
                            } else {
                                const bool backup_replay_forbidden = p.replay_forbidden;
                                const PlayStatus backup_play_status = p.play_status;
                                p.replay_forbidden = true;
                                if (p.play_status != PlayStatus::Stopped) p.play_status = PlayStatus::Paused;
                                p.pattern_break = true;
                                p.next_line = 0;
                                p.ticks = p.tick0;
                                p.update_song_position();
                                songdata().patt_len = new_len;
                                p.replay_forbidden = backup_replay_forbidden;
                                p.play_status = backup_play_status;
                            }
                        }
                        refresh_pages();
                    }
                    _check_key_shortcuts();
                    if (key() == kENTER || key() == kTAB || key() == kDOWN) pos = 14;
                    else if (key() == kUP) pos = 12;
                    else if (key() == kShTAB) pos = P().vibrato_depth == 0 ? 11 : 12;
                } else if (pos == 14) {
                    is_setting.character_set = DEC_NUM_CHARSET;
                    is_environment.min_num = 1;
                    is_environment.max_num = 20;
                    const auto valid = [&]() { return Str2num(temps, 10) >= 1 && Str2num(temps, 10) <= 20; };
                    do {
                        temps = InputStr(Num2str(sd.nm_tracks, 10), B(xstart + 19), B(ystart + 22), 2, 2, input_attr, def_attr);
                    } while (!(key() == kESC || valid()));
                    if (valid()) {
                        {
                            PlayerLock lock;
                            A2Player& p = P();
                            FixedSongData& wsd = songdata();
                            wsd.nm_tracks = B(Str2num(temps, 10));
                            if (wsd.nm_tracks > 18 && !p.percussion_mode) {
                                p.reset_player();
                                p.percussion_mode = true;
                                p.select_chan_tables();
                                p.reset_player();
                            }
                            // 4OP の組に使うトラックがなくなったら、その組を外す
                            static const struct { uint8_t min_tracks, bit; } pairs[6] = {
                                { 15, 0x20 }, { 13, 0x10 }, { 11, 8 }, { 6, 4 }, { 4, 2 }, { 2, 1 } };
                            for (const auto& pair : pairs) {
                                if (wsd.nm_tracks < pair.min_tracks && (wsd.flag_4op | pair.bit) == wsd.flag_4op) {
                                    p.reset_player();
                                    wsd.flag_4op = B(wsd.flag_4op & ~pair.bit);
                                    p.reset_player();
                                }
                            }
                        }
                        refresh_pages();
                    }
                    _check_key_shortcuts();
                    if (key() == kENTER || key() == kTAB || key() == kDOWN) pos = 15;
                    else if (key() == kUP || key() == kShTAB) pos = 13;
                } else if (pos == 15) {
                    wait_key(B(xstart + 3), B(ystart + 24));
                    switch (key()) {
                    case kLEFT: pos = sd.nm_tracks < 18 ? 14 : 155; break;
                    case kRIGHT: pos = sd.nm_tracks < 19 ? 16 : 72; break;
                    case kUP: case kShTAB: pos = 14; break;
                    case kDOWN: case kTAB: case kENTER: pos = 16; break;
                    }
                    if (key() == kSPACE) {
                        PlayerLock lock;
                        A2Player& p = P();
                        FixedSongData& wsd = songdata();
                        p.reset_player();
                        if (!p.percussion_mode) {
                            wsd.nm_tracks = 20;
                            p.percussion_mode = true;
                        } else {
                            if (wsd.nm_tracks > 18) wsd.nm_tracks = 18;
                            p.percussion_mode = false;
                        }
                        p.select_chan_tables();
                        p.reset_player();
                        if (p.play_status == PlayStatus::Stopped) p.init_buffers();
                    }
                    refresh_pages();
                } else if (pos == 16) {
                    wait_key(B(xstart + 3), B(ystart + 25));
                    switch (key()) {
                    case kUP: case kShTAB: pos = 15; break;
                    case kLEFT: pos = sd.nm_tracks < 19 ? 15 : 159; break;
                    case kRIGHT: pos = sd.nm_tracks < 20 ? 1 : 75; break;
                    case kDOWN: pos = 1; break;
                    case kTAB: case kENTER:
                        pos = _old_pos_4op;
                        pos_4op = 0;
                        break;
                    case kSPACE: {
                        PlayerLock lock;
                        P().volume_scaling = !P().volume_scaling;
                        break;
                    }
                    }
                } else if (in_pan(pos)) {
                    const int rel = pos - RANGE_PAN_LO;
                    GotoXY(B(xstart + 51 + rel % 3 * 3), B(ystart + 6 + rel / 3));
                    set_cursor();
                    is_environment.keystroke = getkey();
                    _old_pos_pan = pos;
                    _check_key_shortcuts();
                    switch (key()) {
                    case kLEFT: pos = _left_pos_pan[rel]; break;
                    case kRIGHT: pos = _right_pos_pan[rel]; break;
                    case kUP:
                        if (rel / 3 > 0) pos -= 3;
                        else pos = B(pos + (sd.nm_tracks - 1) * 3);
                        break;
                    case kDOWN:
                        if (rel / 3 < sd.nm_tracks - 1) pos += 3;
                        else pos = B(18 + rel % 3);
                        break;
                    case kShTAB:
                        pos = _old_pos_4op;
                        pos_4op = 0;
                        break;
                    case kTAB: case kENTER: pos = B(RANGE_LCK_LO + rel / 3 * 4); break;
                    case kSPACE: {
                        PlayerLock lock;
                        uint8_t& flags = songdata().lock_flags[rel / 3];
                        flags = B((flags & ~3) + _pan_pos[rel % 3]);
                        P().panlock = true;
                        break;
                    }
                    }
                } else if (in_4op(pos)) {
                    const int idx = pos - RANGE_4OP_LO;   // 0..5
                    const auto has_pair = [&]() { return (sd.flag_4op | (1 << idx)) == sd.flag_4op; };
                    if (pos_4op != 0 && !has_pair()) pos_4op = 0;
                    GotoXY(B(xstart + 35 + pos_4op * 11), B(ystart + 13 + idx));
                    set_cursor();
                    is_environment.keystroke = getkey();
                    _old_pos_4op = pos;
                    _check_key_shortcuts();
                    switch (key()) {
                    case kLEFT:
                        if (pos_4op != 0) pos_4op = 0;
                        else pos = _left_pos_4op[idx];
                        break;
                    case kRIGHT:
                        if (pos_4op != 1 && has_pair()) pos_4op = 1;
                        else pos = _right_pos_4op[idx];
                        break;
                    case kUP: pos = pos > 78 ? _up_pos_4op[idx] : 17; break;
                    case kDOWN: pos = _down_pos_4op[idx]; break;
                    case kShTAB: pos = 16; break;
                    case kTAB: case kENTER: pos = in_pan(_old_pos_pan) ? _old_pos_pan : 19; break;
                    }

                    // オリジナルどおり、キーで pos が変わった後の位置で切り替える
                    if (key() == kSPACE) {
                        PlayerLock lock;
                        A2Player& p = P();
                        FixedSongData& wsd = songdata();
                        const int n = pos - RANGE_4OP_LO + 1;
                        const uint8_t bit = B(1 << (n - 1));
                        if (pos_4op == 0) {
                            if ((wsd.flag_4op | bit) != wsd.flag_4op) {
                                static const uint8_t min_tracks[6] = { 2, 4, 6, 11, 13, 15 };
                                p.reset_player();
                                // オリジナルの min は大きい方を返す
                                wsd.nm_tracks = std::max(wsd.nm_tracks, min_tracks[n - 1]);
                                wsd.flag_4op = B(wsd.flag_4op | bit);
                                p.reset_player();
                                if (p.play_status == PlayStatus::Stopped) p.init_buffers();
                            } else {
                                p.reset_player();
                                wsd.flag_4op = B(wsd.flag_4op & ~bit);
                                p.reset_player();
                                if (p.play_status == PlayStatus::Stopped) p.init_buffers();
                            }
                        } else if (pos_4op == 1) {
                            const uint8_t main_chan = _4op_main_chan[n - 1];
                            wsd.lock_flags[main_chan - 1] ^= 0x40;
                            wsd.lock_flags[main_chan - 2] ^= 0x40;
                        }
                    }
                    refresh_pages();
                } else if (in_lck(pos)) {
                    const int rel = pos - RANGE_LCK_LO;
                    GotoXY(B(xstart + 64 + rel % 4 * 4), B(ystart + 6 + rel / 4));
                    set_cursor();
                    is_environment.keystroke = getkey();
                    _old_pos_lck = pos;
                    _check_key_shortcuts();
                    switch (key()) {
                    case kLEFT: pos = _left_pos_lck[rel]; break;
                    case kRIGHT: pos = _right_pos_lck[rel]; break;
                    case kUP:
                        if (rel / 4 > 0) pos -= 4;
                        else pos = B(RANGE_LCK_LO + (sd.nm_tracks - 1) * 4 + rel % 4);
                        break;
                    case kDOWN:
                        if (rel / 4 < sd.nm_tracks - 1) pos += 4;
                        else pos = B(RANGE_LCK_LO + rel % 4);
                        break;
                    case kShTAB:
                        if (rel % 4 == 0) pos = B(RANGE_PAN_LO + 1 + rel / 4 * 3);
                        else --pos;
                        break;
                    case kTAB: case kENTER:
                        if (rel % 4 == 3) pos = 1;
                        else ++pos;
                        break;
                    case kSPACE: {
                        PlayerLock lock;
                        uint8_t& flags = songdata().lock_flags[rel / 4];
                        static const uint8_t bits[4] = { 8, 4, 0x10, 0x20 };
                        flags ^= bits[rel % 4];
                        if (rel % 4 == 2) P().lockvol = true;
                        if (rel % 4 == 3) P().lockVP = true;
                        break;
                    }
                    }
                }

                draw_screen();
            } while (!(is_environment.keystroke == kESC || is_environment.keystroke == kF1 || is_environment.keystroke == kCtENTR));
        }

        {
            PlayerLock lock;
            A2Player& p = P();
            FixedSongData& sd = songdata();
            if (nm_track_chan > sd.nm_tracks) nm_track_chan = sd.nm_tracks;
            sd.common_flag = B(static_cast<uint8_t>(p.speed_update) + (static_cast<uint8_t>(p.lockvol) << 1) +
                               (static_cast<uint8_t>(p.lockVP) << 2) + (p.tremolo_depth << 3) + (p.vibrato_depth << 4) +
                               (static_cast<uint8_t>(p.panlock) << 5) + (static_cast<uint8_t>(p.percussion_mode) << 6) +
                               (static_cast<uint8_t>(p.volume_scaling) << 7));
            if (Update32(&sd, sizeof(FixedSongData), 0) != songdata_crc) module_archived = false;
            if (p.play_status == PlayStatus::Stopped) {
                p.current_tremolo_depth = p.tremolo_depth;
                p.current_vibrato_depth = p.vibrato_depth;
            }
        }

        add_bank_position("?song_variables_window?pos", -1, pos);
        add_bank_position("?song_variables_window?pos_4op", -1, pos_4op);
        HideCursor();
        std::memcpy(is_setting.terminate_keys, old_keys, sizeof(old_keys));
        move_to_screen_data = screen_backup;
        move_to_screen_area[1] = xstart;
        move_to_screen_area[2] = ystart;
        move_to_screen_area[3] = B(xstart + 79 + 2);
        move_to_screen_area[4] = B(ystart + 26 + 1);
        move2screen();

        {
            PlayerLock lock;
            _IRQFREQ_update_event = false;
            _show_bpm_realtime_proc = old_bpm_proc;
        }
        // オリジナルどおり SizeOf(DWORD) (= 4) に戻す
        is_environment.min_num = 1;
        is_environment.max_num = 4;

        // F1 はヘルプを、Ctrl+Enter は行の印の設定を開いてから開き直す
        if (is_environment.keystroke == kF1) {
            HELP("song_variables");
            continue;
        }
        if (is_environment.keystroke == kCtENTR) {
            LINE_MARKING_SETUP();
            continue;
        }
        break;
    }
}

// --- メッセージボード (Ctrl+B) ---

constexpr int MB_HSIZE = 50;   // 50x20 文字
constexpr int MB_VSIZE = 20;

// songdata.reserved_data の中の String[MB_HSIZE] (tMESSAGE_BOARD_DATA.data の 1 行)。
// 長さより後ろのバイトも曲データとして保存されるため、Pascal (FPC) の文字列操作と同じようにバイトを動かす
class MbLine
{
public:
    MbLine(uint8_t* base, int offset) : base_(base), ofs_(offset) {}

    int len() const { return raw(0); }
    std::string str() const
    {
        std::string s;
        for (int i = 1; i <= len(); ++i) s += static_cast<char>(raw(i));
        return s;
    }
    // s[i] (0 は長さ。範囲の外は reserved_data の続き。reserved_data の外は読み書きしない)
    uint8_t raw(int i) const { return ofs_ + i < 1024 ? base_[ofs_ + i] : 0; }
    void set_raw(int i, uint8_t v)
    {
        if (ofs_ + i < 1024) base_[ofs_ + i] = v;
    }
    char at(int i) const { return static_cast<char>(raw(i)); }
    void set_len(int n) { set_raw(0, B(n)); }

    // 代入 (MB_HSIZE 文字まで。後ろのバイトはそのまま)
    void assign(const std::string& s)
    {
        const int n = std::min<int>(static_cast<int>(s.size()), MB_HSIZE);
        for (int i = 0; i < n; ++i) set_raw(i + 1, static_cast<uint8_t>(s[i]));
        set_len(n);
    }
    // fpc_shortstr_delete
    void del(int index, int count)
    {
        if (index <= 0) return;
        const int length = len();
        if (index <= length && count > 0) {
            if (count > length - index) count = length - index + 1;
            set_len(length - count);
            const int n = len() - index + 1;
            for (int i = 0; i < n; ++i) set_raw(index + i, raw(index + count + i));
        }
    }

    // fpc_shortstr_insert (high = MB_HSIZE、sizeof = MB_HSIZE + 1)
    void ins(const std::string& source, int index)
    {
        const int length = len();
        if (index < 1) index = 1;
        if (index > length) {
            index = length + 1;
            if (index > MB_HSIZE) return;
        }
        int indexlen = length - index + 1;
        int srclen = static_cast<int>(source.size());
        if (srclen + length >= MB_HSIZE + 1) {
            const int cut = srclen + length - (MB_HSIZE + 1) + 1;
            if (cut > indexlen) {
                srclen -= cut - indexlen;
                indexlen = 0;
            } else {
                indexlen -= cut;
            }
        }
        for (int i = indexlen - 1; i >= 0; --i) set_raw(index + srclen + i, raw(index + i));
        for (int i = 0; i < srclen; ++i) set_raw(index + i, static_cast<uint8_t>(source[i]));
        set_len(index + srclen + indexlen - 1);
    }

private:
    uint8_t* base_;
    int ofs_;
};

void MESSAGE_BOARD()
{
    static const uint16_t new_keys[21] = { kF1,   kESC,   kENTER,  kUP,     kDOWN, kLEFT,   kRIGHT, kHOME,  kEND,   kPgUP, kPgDOWN,
                                           kCtHOME, kCtEND, kCtPgUP, kCtPgDN, kTAB,  kCtBkSp, kCtrlY, kDELETE, kBkSPC, kSPACE };
    static const char MB_SIGNATURE[4] = { 'M', 'B', '_', '1' };   // Message Board v1.0

    // メッセージボードは reserved_data に置く (再生側は読まないため、ロックは取らずに書き換える)
    uint8_t* reserved = songdata().reserved_data;
    if (is_empty_data(reserved, sizeof(songdata().reserved_data))) std::memcpy(reserved, MB_SIGNATURE, 4);
    else if (std::memcmp(reserved, MB_SIGNATURE, 4) != 0) return;
    is_environment.insert_mode = true;

    const auto data = [reserved](int idx) { return MbLine(reserved, 4 + (idx - 1) * (MB_HSIZE + 1)); };   // [1..MB_VSIZE]

    for (;;) {   // _jmp1
        if (_force_program_quit) return;

        ScreenMemCopy(screen_ptr, screen_backup);
        HideCursor();

        uint8_t xstart, ystart;
        ScreenMemCopy(screen_ptr, temp_screen);
        centered_frame_vdest = temp_screen;
        centered_frame(xstart, ystart, MB_HSIZE + 3, MB_VSIZE + 1, " MESSAGE BOARD ", dialog_background + dialog_border,
                       dialog_background + dialog_title, frame_double);

        move_to_screen_data = temp_screen;
        move_to_screen_area[1] = xstart;
        move_to_screen_area[2] = ystart;
        move_to_screen_area[3] = B(xstart + MB_HSIZE + 3 + 2);
        move_to_screen_area[4] = B(ystart + MB_VSIZE + 1 + 1);

        const auto show_lines = [&]() {
            for (int idx = 1; idx <= MB_VSIZE; ++idx)
                ShowStr(centered_frame_vdest, B(xstart + 2), B(ystart + 1 + idx - 1), ExpStrR(data(idx).str(), MB_HSIZE, ' '),
                        dl_setting.text_attr);
        };
        show_lines();

        move2screen_alt();
        centered_frame_vdest = screen_ptr;

        const bool old_append_enabled = is_setting.append_enabled;
        is_setting.append_enabled = false;
        uint16_t old_keys[21];
        std::memcpy(old_keys, is_setting.terminate_keys, sizeof(old_keys));
        std::memcpy(is_setting.terminate_keys, new_keys, sizeof(new_keys));

        // オリジナルの min は大きい方を返す
        int hpos = B(std::max<int32_t>(get_bank_position("?message_board?hpos", -1), 1));
        int vpos = B(std::max<int32_t>(get_bank_position("?message_board?vpos", -1), 1));
        uint16_t fkey = 0;

        if (!_force_program_quit) {
            do {
                if (hpos > data(vpos).len() + 1) hpos = data(vpos).len() + 1;

                is_setting.insert_mode = is_environment.insert_mode;
                is_environment.locate_pos = B(hpos);
                is_setting.character_set = CharRange(32, 255);

                data(vpos).assign(InputStr(data(vpos).str(), B(xstart + 2), B(ystart + 1 + vpos - 1), MB_HSIZE, MB_HSIZE,
                                           dl_setting.text_attr, dl_setting.text_attr));

                hpos = is_environment.locate_pos;
                fkey = is_environment.keystroke;
                const int ref_vpos = vpos;
                MbLine line = data(vpos);

                switch (fkey) {
                case kUP:
                    if (vpos > 1) --vpos;
                    break;
                case kDOWN:
                    if (vpos < MB_VSIZE) ++vpos;
                    break;

                case kLEFT:
                    if (hpos > 1) --hpos;
                    else if (vpos > 1) {
                        --vpos;
                        if (data(vpos).len() != 0)
                            hpos = std::min(data(vpos).len() + 1, MB_HSIZE);   // オリジナルの max は小さい方を返す
                        else {
                            hpos = MB_HSIZE;
                            data(vpos).assign(ExpStrR("", MB_HSIZE, ' '));
                        }
                    }
                    break;

                case kRIGHT:
                    if (hpos < MB_HSIZE) ++hpos;
                    else if (vpos < MB_VSIZE) {
                        ++vpos;
                        hpos = 1;
                    }
                    break;

                case kCtPgUP: vpos = 1; break;
                case kCtPgDN: vpos = MB_VSIZE; break;

                case kPgUP: {
                    const int old_vpos = vpos;
                    if (vpos > 1) {
                        do {
                            --vpos;
                        } while (!(vpos == 1 || CutStr(data(vpos).str()) != ""));
                    }
                    if (vpos == old_vpos) vpos = 1;
                    break;
                }

                case kPgDOWN: {
                    const int old_vpos = vpos;
                    if (vpos < MB_VSIZE) {
                        do {
                            ++vpos;
                        } while (!(vpos == MB_VSIZE || CutStr(data(vpos).str()) != ""));
                    }
                    if (vpos == old_vpos) vpos = MB_VSIZE;
                    break;
                }

                case kHOME:
                    if (hpos > 1) hpos = 1;
                    else vpos = 1;
                    break;

                case kCtHOME:
                    vpos = 1;
                    hpos = 1;
                    break;

                case kEND: {
                    const int end_pos = static_cast<int>(CutStrR(line.str(), 0).size()) + 1;
                    if (hpos < end_pos) hpos = end_pos;
                    else if (hpos < MB_HSIZE) hpos = MB_HSIZE;
                    else vpos = MB_VSIZE;
                    break;
                }

                case kCtEND:
                    vpos = MB_VSIZE;
                    hpos = data(MB_VSIZE).len() + 1;
                    break;

                case kENTER:
                    if (vpos < MB_VSIZE) {
                        if (CutStr(data(MB_VSIZE).str()) == "") {
                            std::string temps;
                            if (hpos < MB_HSIZE) {
                                temps = Copy(line.str(), hpos, line.len());
                                if (temps != "") line.del(hpos, line.len());
                            }
                            for (int idx = MB_VSIZE; idx >= vpos + 1; --idx) data(idx).assign(data(idx - 1).str());
                            data(vpos + 1).assign(temps);
                        }
                        ++vpos;
                        hpos = 1;
                    }
                    break;

                case kDELETE: {
                    const std::string s = line.str();
                    if ((hpos == line.len() && line.at(line.len()) != ' ') ||
                        !(CutStr(s) == "" || hpos >= line.len() || CutStrR(s, B(hpos)) != CutStrR(s, 0))) {
                        line.del(hpos, 1);
                    } else if (vpos < MB_VSIZE) {
                        const std::string next = CutStrR(data(vpos + 1).str(), 0);
                        if (static_cast<int>(CutStrR(s, B(hpos)).size() + next.size()) <= MB_HSIZE) {
                            line.assign(CutStrR(s, B(hpos)) + next);
                            if (line.len() > MB_HSIZE) line.del(MB_HSIZE + 1, line.len());
                            for (int idx = vpos + 1; idx <= MB_VSIZE - 1; ++idx) data(idx).assign(data(idx + 1).str());
                            data(MB_VSIZE).assign("");
                        }
                    }
                    break;
                }

                case kTAB:
                    if (vpos > 1) {
                        int vpos2 = vpos;
                        std::string temps;
                        do {
                            --vpos2;
                            const std::string s2 = data(vpos2).str();
                            temps = CutStr(Copy(s2, hpos + 1, static_cast<int>(s2.size()) - hpos));
                        } while (!(vpos2 == 1 || temps != ""));
                        MbLine line2 = data(vpos2);
                        if (hpos <= line2.len()) {
                            int idx2 = hpos + 1;
                            while (idx2 < line2.len() && line2.at(idx2) != ' ') ++idx2;
                            while (idx2 < line2.len() && line2.at(idx2) == ' ') ++idx2;
                            if (idx2 - hpos > 1) {
                                line.ins(ExpStrL("", B(idx2 - hpos), ' '), hpos);
                                hpos += idx2 - hpos;
                            }
                        }
                    }
                    if (line.len() > MB_HSIZE) line.del(MB_HSIZE + 1, line.len());
                    break;

                case kSPACE: {
                    const int idx2 = !ctrl_pressed() ? vpos : MB_VSIZE;
                    bool flag = false;
                    int idx = vpos;
                    for (; idx <= idx2; ++idx) {
                        if (static_cast<int>(CutStrR(data(idx).str(), 0).size()) == MB_HSIZE) {
                            flag = true;
                            break;
                        }
                    }
                    if (idx > idx2) idx = idx2;   // ループを抜けた後の制御変数 (最後の値)
                    if (!flag && is_environment.insert_mode) {
                        for (int i = vpos; i <= idx2; ++i) {
                            MbLine l = data(i);
                            l.ins(" ", hpos);
                            if (l.len() > MB_HSIZE) l.del(MB_HSIZE + 1, l.len());
                        }
                    } else if (!is_environment.insert_mode) {
                        data(idx).set_raw(hpos, ' ');
                    }
                    if ((!flag || !is_environment.insert_mode) && hpos < MB_HSIZE) ++hpos;
                    break;
                }

                case kCtBkSp:
                    if (!shift_pressed()) {
                        if (hpos == MB_HSIZE && line.len() > 0) line.set_len(line.len() - 1);
                        while (InSet(is_setting.word_characters, line.at(hpos - 1)) && hpos > 1) {
                            --hpos;
                            line.del(hpos, 1);
                        }
                        while (!InSet(is_setting.word_characters, line.at(hpos - 1)) && hpos > 1) {
                            --hpos;
                            line.del(hpos, 1);
                        }
                    } else if (hpos < MB_HSIZE) {
                        --hpos;
                        if (hpos != 0) {
                            for (int idx = vpos; idx <= MB_VSIZE; ++idx) data(idx).del(hpos, 1);
                        } else hpos = 1;
                    } else {
                        for (int idx = vpos; idx <= MB_VSIZE; ++idx) {
                            MbLine l = data(idx);
                            if (l.len() > 0) l.set_len(l.len() - 1);
                        }
                    }
                    break;

                case kBkSPC:
                    if (hpos > 1) {
                        if (hpos < MB_HSIZE || line.at(hpos) == ' ') {
                            --hpos;
                            line.del(hpos, 1);
                        } else if (line.len() > 0) {
                            line.set_len(line.len() - 1);
                        }
                    } else if (vpos > 1) {
                        const std::string prev = CutStrR(data(vpos - 1).str(), 0);
                        const std::string cur = CutStrR(line.str(), 0);
                        if (static_cast<int>(prev.size() + cur.size()) <= MB_HSIZE) {
                            --vpos;
                            MbLine l = data(vpos);
                            hpos = l.len() + 1;
                            l.assign(prev + cur);
                            if (l.len() > MB_HSIZE) l.del(MB_HSIZE + 1, l.len());
                            for (int idx = vpos + 1; idx <= MB_VSIZE - 1; ++idx) data(idx).assign(data(idx + 1).str());
                            data(MB_VSIZE).assign("");
                        }
                    }
                    break;

                case kCtrlY:
                    if (!shift_pressed()) {
                        for (int idx = vpos; idx <= MB_VSIZE - 1; ++idx) data(idx).assign(data(idx + 1).str());
                        data(MB_VSIZE).assign("");
                    }
                    break;
                }

                if (data(vpos).len() < hpos) data(vpos).assign(ExpStrR(data(vpos).str(), B(hpos), ' '));
                if (ref_vpos != vpos) data(ref_vpos).assign(CutStrR(data(ref_vpos).str(), 0));

                show_lines();
            } while (!(fkey == kESC || fkey == kF1 || _force_program_quit));
        }

        is_setting.append_enabled = old_append_enabled;
        std::memcpy(is_setting.terminate_keys, old_keys, sizeof(old_keys));
        add_bank_position("?message_board?hpos", -1, hpos);
        add_bank_position("?message_board?vpos", -1, vpos);

        HideCursor();
        move_to_screen_data = screen_backup;
        move_to_screen_area[1] = xstart;
        move_to_screen_area[2] = ystart;
        move_to_screen_area[3] = B(xstart + MB_HSIZE + 3 + 2);
        move_to_screen_area[4] = B(ystart + MB_VSIZE + 1 + 1);
        move2screen();

        if (fkey == kF1) {
            HELP("message_board");
            continue;
        }
        break;
    }
}
