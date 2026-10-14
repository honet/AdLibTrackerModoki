// ---------------------------------------------------------------------------
// 楽器ファイル・楽器バンクの読み込み (adtrack2.pas の FILE_open のうち楽器の部分、adt2ext5.pas の a2b_file_loader、
// iloadins.inc の test_instrument_alt)
// ---------------------------------------------------------------------------
//
// MainScreen.cpp の無名名前空間の中で、InstEdit.inl の後に #include する。
//
// バンクの一覧 (オリジナルの Menu に渡す a2b_queue) の項目は、先頭が '~' + 楽器名の先頭 9 文字 + '~' のため、
// Menu のショートカットキーは楽器名の 1 文字目 (印) になる (オリジナルと同じ)。

// バンクから読み込んだデータ (オリジナルの temp_songdata)
FixedSongData& temp_songdata()
{
    static const auto data = std::make_unique<FixedSongData>();
    return *data;
}

// typcons2.inc
const char* const ins_pan_str2[3] = {
    "\xFA\xFA\x0B\x0B\x0B\xFA\xFA",
    "\x0B\x0B\x0B\xFA\xFA\xFA\xFA",
    "\xFA\xFA\xFA\xFA\x0B\x0B\x0B",
};

const std::string a2b_header_str[3] = {
    " iNSTRUMENT                                 PANNiNG            iNSTRUMENT ",
    " NAME    DESCRiPTiON                        \xAA  c  \xAB   F.TUNE   VOiCE      ",
    std::string(74, '\xCD'),
};

const std::string a2b_header_hires_str[3] = {
    " iNSTRUMENT                                                               PANNiNG            iNSTRUMENT ",
    " NAME    DESCRiPTiON                        \xDA" "20\xBF \xDA" "40\xBF \xDA" "60\xBF \xDA" "80\xBF \xDA"
    "E0\xBF C0   \xAA  c  \xAB   F.TUNE   VOiCE      ",
    std::string(104, '\xCD'),
};

// バンクの 4OP の組の印 (_4op_flag_column[1..255])
char _4op_flag_column[256];
uint8_t _a2b_lister_count = 0;

bool check_4op_flag_temp(uint8_t ins)
{
    return a2ins::check_4op_flag(temp_songdata(), ins);
}

// init 4OP flags
void init_4op_flag_column()
{
    const auto is_flag_chr = [](char c) { return c == a2ins::_4op_flag_chr_beg || c == a2ins::_4op_flag_chr_end; };
    std::fill(std::begin(_4op_flag_column), std::end(_4op_flag_column), '\0');
    for (int temp = 1; temp <= 254; ++temp) {
        if (check_4op_flag_temp(B(temp))) {
            if (!is_flag_chr(_4op_flag_column[temp])) _4op_flag_column[temp] = a2ins::_4op_flag_chr_beg;
            if (!is_flag_chr(_4op_flag_column[temp + 1])) _4op_flag_column[temp + 1] = a2ins::_4op_flag_chr_end;
        }
    }
}

// 一覧で選んでいる 4OP の組 (上位バイト: 1 つ目、下位バイト: 2 つ目)。組でなければ 0
uint16_t get_4op_to_test_temp()
{
    uint16_t result = 0;
    const uint16_t curr_inst = mn_environment.curr_pos;
    if (curr_inst >= 1 && curr_inst <= 255 && songdata().flag_4op != 0) {
        if (_4op_flag_column[curr_inst] == a2ins::_4op_flag_chr_beg) {
            result = static_cast<uint16_t>(curr_inst + 1 + (curr_inst << 8));
        } else if (curr_inst > 1 && _4op_flag_column[curr_inst] == a2ins::_4op_flag_chr_end) {
            result = static_cast<uint16_t>(curr_inst + ((curr_inst - 1) << 8));
        }
    }
    return result;
}

// 空でない最後の楽器 (名前もなければ空とみなす)
uint8_t count_instruments_temp()
{
    const FixedSongData& ts = temp_songdata();
    uint8_t result = 255;
    while (result > 0 && is_empty_data(&at1(ts.instr_data, result), INSTRUMENT_SIZE) &&
           CutStr(Copy(at1(ts.instr_names, result).str(), 10, 32)).empty()) {
        --result;
    }
    return result;
}

// バンクの楽器の試聴 (iloadins.inc の test_instrument_alt)。ロックを取った状態で呼ぶ (鍵盤を離すのを待つ間は外す)。
//   bankSelector: バンク (temp_songdata) の楽器を鳴らす。loadMacros なら FM マクロとアルペジオ / ビブラートの表も使う
//   loadArpVib: 曲の楽器を、選んでいるアルペジオ / ビブラートの表 (バンクのもの) で鳴らす
// オリジナルにはファイル選択画面で選んだ楽器ファイルを鳴らす場合 (bankSelector も loadArpVib も FALSE) もあるが、
// 本クローン版はコモンダイアログで楽器ファイルを選ぶため使わない。
void test_instrument_alt(uint8_t /*chan*/, uint16_t fkey, bool loadMacros, bool bankSelector, bool loadArpVib,
                         uint8_t test_ins1, uint8_t test_ins2)
{
    if (!bankSelector && !loadArpVib) return;

    A2Player& p = P();
    FixedSongData& sd = songdata();
    const FixedSongData& ts = temp_songdata();

    bool valid_key = false;
    int temp = 1;
    for (; temp <= 29; ++temp) {
        if (!shift_pressed() && board_scancodes[temp - 1] == HI(fkey)) {
            valid_key = true;
            break;
        }
    }

    if (scankey(SC_F7)) {
        for (int chan = 1; chan <= 20; ++chan) p.reset_chan_data(B(chan));
    }

    {
        const int note = temp + 12 * (current_octave - 1) - 1;
        if (!valid_key || !(note >= 0 && note <= 12 * 8 + 1)) return;
    }

    uint8_t channels = !p.percussion_mode ? 18 : 15;

    Instrument temp_instrument{}, temp_instrument2{};
    RegisterTable temp_instrument_macro{}, temp_instrument_macro2{};
    DisFmregCol temp_instrument_dis_fmreg_col{}, temp_instrument_dis_fmreg_col2{};
    uint8_t temp2 = B(temp);

    // bankSelector ならバンクから、loadArpVib なら曲から
    const FixedSongData& src = !loadArpVib ? ts : sd;
    temp_instrument = at1(src.instr_data, test_ins1);
    temp_instrument_macro = at1(src.instr_macros, test_ins1);
    std::memcpy(temp_instrument_dis_fmreg_col, at1(src.dis_fmreg_col, test_ins1), sizeof(DisFmregCol));
    if (test_ins2 != 0) {
        temp_instrument2 = at1(src.instr_data, test_ins2);
        temp_instrument_macro2 = at1(src.instr_macros, test_ins2);
        std::memcpy(temp_instrument_dis_fmreg_col2, at1(src.dis_fmreg_col, test_ins2), sizeof(DisFmregCol));
    }

    if (test_ins2 != 0 && is_empty_data(&temp_instrument2, sizeof(temp_instrument2)) &&
        is_empty_data(&temp_instrument_macro2, sizeof(temp_instrument_macro2))) {
        test_ins2 = 0;
    }

    const auto arp_vib_macro_table_backup = std::make_unique<MacroTable[]>(255);
    std::memcpy(arp_vib_macro_table_backup.get(), sd.macro_table, sizeof(sd.macro_table));

    // オリジナルはアルペジオ / ビブラートの表を macro_table の 1 件分 (アルペジオ + ビブラート) の大きさで写すため、
    // アルペジオなら同じ番号のビブラート、ビブラートなら次の番号のアルペジオ (255 番なら macro_table の後ろ) まで写す。
    // temp_songdata は曲データと同じ並びなので、同じ位置の内容を写して再現する。
    const auto move_entry = [&](const void* from) {
        const size_t ofs = static_cast<const uint8_t*>(from) - reinterpret_cast<const uint8_t*>(&ts);
        std::memcpy(reinterpret_cast<uint8_t*>(&sd) + ofs, from, sizeof(MacroTable));
    };
    if (loadArpVib) {
        if (arp_tab_selected) {
            temp_instrument_macro.arpeggio_table = arpvib_arpeggio_table;
            move_entry(&at1(ts.macro_table, arpvib_arpeggio_table).arpeggio);
        } else {
            temp_instrument_macro.arpeggio_table = 0;
        }
        if (vib_tab_selected) {
            temp_instrument_macro.vibrato_table = arpvib_vibrato_table;
            move_entry(&at1(ts.macro_table, arpvib_vibrato_table).vibrato);
        } else {
            temp_instrument_macro.vibrato_table = 0;
        }
        at1(sd.instr_macros, test_ins1) = temp_instrument_macro;
        if (test_ins2 != 0) at1(sd.instr_macros, test_ins2) = temp_instrument_macro2;
    } else if (bankSelector && loadMacros) {
        if (temp_instrument_macro.arpeggio_table != 0)
            move_entry(&at1(ts.macro_table, temp_instrument_macro.arpeggio_table).arpeggio);
        if (temp_instrument_macro.vibrato_table != 0)
            move_entry(&at1(ts.macro_table, temp_instrument_macro.vibrato_table).vibrato);
    }

    // オリジナルはここで抜けるとき、上で書き換えた曲データを戻さない
    if ((is_empty_data(&temp_instrument, sizeof(temp_instrument)) &&
         is_empty_data(&temp_instrument_macro, sizeof(temp_instrument_macro))) ||
        (test_ins2 != 0 && is_empty_data(&temp_instrument2, sizeof(temp_instrument2)) &&
         is_empty_data(&temp_instrument_macro2, sizeof(temp_instrument_macro2)))) {
        return;
    }

    const bool status_backup_replay_forbidden = p.replay_forbidden;
    const PlayStatus status_backup_play_status = p.play_status;
    p.replay_forbidden = true;
    if (p.play_status != PlayStatus::Stopped) p.play_status = PlayStatus::Paused;
    p.nul_volume_bars();
    really_no_status_refresh = true;
    p.reset_player();

    RegisterTable ins__fmreg_table_backup{}, ins__fmreg_table_backup2{};
    DisFmregCol ins__dis_fmreg_col_backup{}, ins__dis_fmreg_col_backup2{};
    const auto put_macro = [&](uint8_t ins, RegisterTable& backup, DisFmregCol& dis_backup, const RegisterTable* macro,
                               const DisFmregCol* dis) {
        backup = at1(sd.instr_macros, ins);
        std::memcpy(dis_backup, at1(sd.dis_fmreg_col, ins), sizeof(DisFmregCol));
        if (macro != nullptr) {
            at1(sd.instr_macros, ins) = *macro;
            std::memcpy(at1(sd.dis_fmreg_col, ins), *dis, sizeof(DisFmregCol));
        } else {
            at1(sd.instr_macros, ins) = RegisterTable{};
            std::memset(at1(sd.dis_fmreg_col, ins), 0, sizeof(DisFmregCol));
        }
    };
    if (loadMacros || loadArpVib) {
        put_macro(test_ins1, ins__fmreg_table_backup, ins__dis_fmreg_col_backup, &temp_instrument_macro,
                  &temp_instrument_dis_fmreg_col);
        if (test_ins2 != 0)
            put_macro(test_ins2, ins__fmreg_table_backup2, ins__dis_fmreg_col_backup2, &temp_instrument_macro2,
                      &temp_instrument_dis_fmreg_col2);
    } else {
        put_macro(test_ins1, ins__fmreg_table_backup, ins__dis_fmreg_col_backup, nullptr, nullptr);
        if (test_ins2 != 0) put_macro(test_ins2, ins__fmreg_table_backup2, ins__dis_fmreg_col_backup2, nullptr, nullptr);
    }

    decltype(p.channel_flag) channel_flag_backup;
    decltype(p.event_table) event_table_backup;
    decltype(p.pan_lock) pan_lock_backup;
    decltype(p.volume_lock) volume_lock_backup;
    decltype(p.peak_lock) peak_lock_backup;
    decltype(p.panning_table) panning_table_backup;
    decltype(p.fmpar_table) fmpar_table_backup;
    decltype(p.volume_table) volume_table_backup;
    decltype(p.freq_table) freq_table_backup;
    decltype(p.freqtable2) freqtable2_backup;
    decltype(p.keyoff_loop) keyoff_loop_backup;

    std::memcpy(channel_flag_backup, p.channel_flag, sizeof(p.channel_flag));
    std::memcpy(event_table_backup, p.event_table, sizeof(p.event_table));
    const uint8_t common_flag_backup = sd.common_flag;
    const bool volume_scaling_backup = p.volume_scaling;
    sd.common_flag = B(sd.common_flag & ~0x80);
    p.volume_scaling = false;
    std::fill(std::begin(p.channel_flag), std::end(p.channel_flag), true);
    std::memcpy(pan_lock_backup, p.pan_lock, sizeof(p.pan_lock));
    std::memcpy(volume_lock_backup, p.volume_lock, sizeof(p.volume_lock));
    std::memcpy(peak_lock_backup, p.peak_lock, sizeof(p.peak_lock));
    std::memcpy(panning_table_backup, p.panning_table, sizeof(p.panning_table));
    std::fill(std::begin(p.pan_lock), std::end(p.pan_lock), false);
    std::fill(std::begin(p.volume_lock), std::end(p.volume_lock), false);
    std::fill(std::begin(p.peak_lock), std::end(p.peak_lock), false);
    const uint8_t flag_4op_backup = sd.flag_4op;
    if (!p.percussion_mode && !(sd.flag_4op != 0)) {
        channels = 18;
    } else if (!(sd.flag_4op != 0)) {
        channels = 15;
    } else if (test_ins2 != 0) {
        sd.flag_4op = 0x3f;
        channels = 6;
    } else {
        sd.flag_4op = 0;
        channels = !p.percussion_mode ? 18 : 15;
    }

    p.reset_player();
    uint8_t chan_handle[21] = {};   // [1..20]
    std::memcpy(fmpar_table_backup, p.fmpar_table, sizeof(p.fmpar_table));
    std::memcpy(volume_table_backup, p.volume_table, sizeof(p.volume_table));
    std::memcpy(freq_table_backup, p.freq_table, sizeof(p.freq_table));
    std::memcpy(freqtable2_backup, p.freqtable2, sizeof(p.freqtable2));
    std::memcpy(keyoff_loop_backup, p.keyoff_loop, sizeof(p.keyoff_loop));
    std::fill(std::begin(p.keyoff_loop), std::end(p.keyoff_loop), false);

    p.misc_register = B((p.current_tremolo_depth << 7) + (p.current_vibrato_depth << 6) + (int(p.percussion_mode) << 5));
    p.key_off(17);
    p.key_off(18);
    p.opl3out(0xbd, p.misc_register);

    // 鍵盤 board_pos の音を chan (chan_handle の位置) で鳴らす
    const auto output_note = [&](uint8_t chan, uint8_t board_pos) {
        const int note = board_pos + 12 * (current_octave - 1);
        if (!(note >= 0 && note <= 12 * 8 + 1)) return false;

        chan_handle[chan] = board_scancodes[board_pos - 1];
        // オリジナルは _4op_main_chan[1..6] の範囲外 (パーカッションの 16..20) も読むが、ここでは変えない
        if (test_ins2 != 0 && chan <= 6) chan = _4op_main_chan[chan - 1];
        p.load_instrument(reinterpret_cast<const uint8_t*>(&temp_instrument), chan);
        if (p.percussion_mode && (temp_instrument.perc_voice == 4 || temp_instrument.perc_voice == 5) &&
            (chan == 19 || chan == 20)) {
            p.load_instrument(reinterpret_cast<const uint8_t*>(&temp_instrument), _perc_sim_chan[chan - 19]);
        }
        if (test_ins2 != 0) p.load_instrument(reinterpret_cast<const uint8_t*>(&temp_instrument2), B(chan - 1));

        p.macro_speedup = sd.macro_speedup;
        if (p.play_status == PlayStatus::Stopped) p.update_timer(sd.tempo);

        const uint16_t freq = static_cast<uint16_t>(A2Player::nFreq(B(note - 1)) + 0x2000 + temp_instrument.fine_tune);
        p.event_table[chan].note = B(note);
        p.freq_table[chan] = freq;
        p.freqtable2[chan] = freq;
        p.key_on(chan);
        p.change_freq(chan, freq);

        p.init_macro_table(chan, B(note), test_ins1, freq);
        if (test_ins2 != 0) {
            p.freq_table[chan - 1] = freq;
            p.freqtable2[chan - 1] = freq;
            p.init_macro_table(B(chan - 1), B(note), test_ins2, freq);
        }
        return true;
    };

    // 鍵盤を離すまで画面を更新し続ける
    const auto poll_screen = []() {
        PlayerUnlock unlock;
        keyboard_poll_input();
        keyboard_reset_buffer();
        draw_screen();
    };

    if (p.percussion_mode && temp_instrument.perc_voice >= 1 && temp_instrument.perc_voice <= 5) {
        output_note(B(temp_instrument.perc_voice + 15), temp2);
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
                if (!(sd.flag_4op != 0 && test_ins2 != 0)) {
                    temp3 = get_chanpos(chan_handle + 1, channels, temp2);
                    temp5 = get_chanpos(chan_handle + 1, channels, 0);
                } else {
                    temp3 = get_chanpos2(chan_handle + 1, channels, temp2);
                    temp5 = get_chanpos2(chan_handle + 1, channels, 0);
                }

                if (temp4) valid_key = true;
                if (temp4 && temp3 == 0 && temp5 != 0) output_note(temp5, B(temp));

                if (!temp4 && temp3 != 0) {
                    chan_handle[temp3] = 0;
                    p.key_off(temp3);
                    if (alt_pressed()) p.keyoff_loop[temp3] = true;
                }
            }
            poll_screen();
        } while (valid_key && !_force_program_quit);
    }

    while (ctrl_pressed() && !_force_program_quit) poll_screen();

    for (int chan = 1; chan <= 20; ++chan) p.key_off(B(chan));

    at1(sd.instr_macros, test_ins1) = ins__fmreg_table_backup;
    std::memcpy(at1(sd.dis_fmreg_col, test_ins1), ins__dis_fmreg_col_backup, sizeof(DisFmregCol));
    if (test_ins2 != 0) {
        at1(sd.instr_macros, test_ins2) = ins__fmreg_table_backup2;
        std::memcpy(at1(sd.dis_fmreg_col, test_ins2), ins__dis_fmreg_col_backup2, sizeof(DisFmregCol));
    }
    std::memcpy(sd.macro_table, arp_vib_macro_table_backup.get(), sizeof(sd.macro_table));

    sd.flag_4op = flag_4op_backup;
    std::memcpy(p.fmpar_table, fmpar_table_backup, sizeof(p.fmpar_table));
    std::memcpy(p.volume_table, volume_table_backup, sizeof(p.volume_table));
    std::memcpy(p.panning_table, panning_table_backup, sizeof(p.panning_table));
    sd.common_flag = common_flag_backup;
    p.volume_scaling = volume_scaling_backup;
    if (status_backup_play_status == PlayStatus::Playing) p.reset_player();

    std::memcpy(p.channel_flag, channel_flag_backup, sizeof(p.channel_flag));
    std::memcpy(p.event_table, event_table_backup, sizeof(p.event_table));
    std::memcpy(p.pan_lock, pan_lock_backup, sizeof(p.pan_lock));
    std::memcpy(p.volume_lock, volume_lock_backup, sizeof(p.volume_lock));
    std::memcpy(p.peak_lock, peak_lock_backup, sizeof(p.peak_lock));

    really_no_status_refresh = false;
    std::memcpy(p.freq_table, freq_table_backup, sizeof(p.freq_table));
    std::memcpy(p.freqtable2, freqtable2_backup, sizeof(p.freqtable2));
    std::memcpy(p.keyoff_loop, keyoff_loop_backup, sizeof(p.keyoff_loop));
    std::memset(p.macro_table, 0, sizeof(p.macro_table));
    p.replay_forbidden = status_backup_replay_forbidden;
    p.play_status = status_backup_play_status;
    keyboard_reset_buffer();
}

// バンクの一覧の Menu の ext_proc (a2b_lister_external_proc と a2b_lister_external_proc_callback)
void a2b_lister_external_proc()
{
    for (int temp = 1; temp <= _a2b_lister_count; ++temp) {
        const uint8_t attr = mn_environment.curr_pos == mn_environment.curr_page + temp - 1 ? mn_setting.text2_attr
                                                                                            : mn_setting.text_attr;
        const int idx = mn_environment.curr_page + temp - 1;
        ShowStr(mn_environment.v_dest, B(mn_environment.xpos + 1), B(mn_environment.ypos + 3 + temp),
                std::string(1, idx <= 255 ? _4op_flag_column[idx] : '\0'), attr);
    }

    PlayerLock lock;
    uint8_t test_ins1, test_ins2;
    const uint16_t t = get_4op_to_test_temp();
    if (t != 0) {
        test_ins1 = LO(t);
        test_ins2 = HI(t);
    } else {
        const uint16_t curr_inst = mn_environment.curr_pos;
        if (!(curr_inst >= 1 && curr_inst <= 255)) return;
        test_ins1 = B(curr_inst);
        test_ins2 = 0;
    }
    if (!shift_pressed() && !alt_pressed() && !ctrl_pressed()) {
        // test instrument from bank, without any macros
        test_instrument_alt(count_channel(pattern_hpos), mn_environment.keystroke, false, true, false, test_ins1, test_ins2);
    }
}

// 4OP の組を取り込む位置を決める (Enter: 組なら必ず組で、Shift+Enter: 取り込む先も組のときだけ)
uint8_t _4op_idx11, _4op_idx12, _4op_idx21, _4op_idx22;

bool select_4op_destination(uint8_t index, bool alternate)
{
    const FixedSongData& sd = songdata();
    const char flag = _4op_flag_column[index];
    const bool beg = flag == a2ins::_4op_flag_chr_beg;
    const bool end = flag == a2ins::_4op_flag_chr_end;
    if (!beg && !end) return false;

    if (alternate) {
        // put 4op instrument (alternate)
        if (a2ins::check_4op_flag(sd, current_inst)) {
            _4op_idx11 = current_inst;
            _4op_idx12 = B(current_inst + 1);
        } else if (a2ins::check_4op_flag(sd, B(current_inst - 1))) {
            _4op_idx11 = B(std::max<int>(current_inst, 2) - 1);
            _4op_idx12 = B(std::max<int>(current_inst, 2));
        } else {
            return false;
        }
    } else if (beg) {
        // put 4op instrument (force)
        _4op_idx11 = current_inst;
        _4op_idx12 = B(current_inst + 1);
    } else {
        _4op_idx11 = B(std::max<int>(current_inst, 2) - 1);
        _4op_idx12 = B(std::max<int>(current_inst, 2));
    }
    _4op_idx21 = beg ? index : B(index - 1);
    _4op_idx22 = beg ? B(index + 1) : index;
    return true;
}

bool a2b_file_loader(bool bankSelector, bool loadBankPossible);
uint8_t a2w_file_loader(bool loadFromFile, bool loadMacros, bool bankSelector, bool loadBankPossible, bool updateCurInstr);

// 楽器窓・楽器エディタ・マクロエディタの F3: 楽器ファイル・楽器バンクを読み込む (FILE_open の mpos = 2 の場合)。
// 本クローン版はコモンダイアログで選ぶ。quick (楽器窓の Shift+F3) なら前回の楽器ファイルをそのまま読み込む。
// a2w_only (マクロエディタのアルペジオ / ビブラートの位置) なら A2W だけを選ぶ。
// オリジナルはファイル選択画面で Shift+Enter を押すと、一覧を出さずにバンク全体を読み込む (確認のダイアログ付き)。
// コモンダイアログでは区別できないため、本クローン版は常に一覧を出す (load_bank_possible なら Ctrl+Enter で全体を読み込める)。
void FILE_open_instrument(bool load_bank_possible, bool quick, bool a2w_only)
{
    if (!quick) {
        WCHAR buf[MAX_PATH] = L"";
        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = g_hwnd;
        if (!a2w_only) {
            ofn.lpstrFilter = L"AdLib Tracker II 楽器・楽器バンク (*.a2i;*.a2f;*.a2b;*.a2w)\0*.a2i;*.a2f;*.a2b;*.a2w\0"
                              L"AdLib Tracker II 楽器 (*.a2i;*.a2f)\0*.a2i;*.a2f\0"
                              L"AdLib Tracker II 楽器バンク (*.a2b;*.a2w)\0*.a2b;*.a2w\0";
        } else {
            ofn.lpstrFilter = L"AdLib Tracker II マクロ付き楽器バンク (*.a2w)\0*.a2w\0";
        }
        ofn.lpstrFile = buf;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        const bool ok = GetOpenFileNameW(&ofn) != FALSE;
        // ダイアログ表示中に離されたキーが押されたままにならないようにする
        keyboard_init();
        if (!ok) return;
        instdata_source = buf;
    }

    const std::string ext = ExtOnly(to_cp437(instdata_source));
    HideCursor();
    {
        PlayerLock lock;
        P().nul_volume_bars();
    }
    no_status_refresh = true;

    bool loaded = false;
    if (ext == "a2i" || ext == "a2f") {
        std::wstring error;
        {
            PlayerLock lock;
            loaded = a2ins::LoadInstrumentFile(instdata_source, songdata(), current_inst, error);
        }
        if (!loaded) {
            MessageBoxW(g_hwnd, error.c_str(), L"AdLibTrackerModoki", MB_OK | MB_ICONWARNING);
            keyboard_init();
        }
    } else if (ext == "a2b") {
        loaded = a2b_file_loader(true, load_bank_possible);   // w/ bank selector
        if (loaded) {
            PlayerLock lock;
            filter_loaded_names(songdata());
        }
    } else if (ext == "a2w") {
        loaded = a2w_file_loader(true, _arp_vib_loader, true, load_bank_possible, false) != BYTE_NULL;   // w/ bank selector
        _arp_vib_loader = false;
        if (loaded) {
            PlayerLock lock;
            filter_loaded_names(songdata());
        }
    }

    PlayerLock lock;
    if (loaded) module_archived = false;
    force_scrollbars = true;
    PATTERN_ORDER_page_refresh(pattord_page);
    PATTERN_page_refresh(pattern_page);
    force_scrollbars = false;
    std::fill(std::begin(P().ai_table), std::end(P().ai_table), uint8_t(0));
    no_status_refresh = false;
}

// A2B の読み込み (a2b_file_loader)。楽器を取り込んだら true を返す (オリジナルの load_flag = 1)。
// bankSelector が FALSE なら一覧を出さずにバンク全体を読み込む。loadBankPossible なら一覧で Ctrl+Enter を押すと
// バンク全体を読み込む。
bool a2b_file_loader(bool bankSelector, bool loadBankPossible)
{
    FixedSongData& ts = temp_songdata();
    std::wstring error;
    bool ok;
    {
        PlayerLock lock;
        ts = songdata();
        ok = a2ins::LoadA2bBank(instdata_source, ts, error);
    }
    if (!ok) {
        MessageBoxW(g_hwnd, error.c_str(), L"AdLibTrackerModoki", MB_OK | MB_ICONWARNING);
        keyboard_init();
        return false;
    }

    if (!bankSelector) {
        PlayerLock lock;
        std::memcpy(songdata().instr_names, ts.instr_names, sizeof(ts.instr_names));
        std::memcpy(songdata().instr_data, ts.instr_data, sizeof(ts.instr_data));
        return true;
    }

    init_4op_flag_column();

    uint8_t nm_valid = count_instruments_temp();
    if (nm_valid == 0) nm_valid = 1;

    std::vector<std::string> a2b_queue(3 + nm_valid);
    std::vector<std::string> a2b_queue_more(3 + nm_valid);
    for (int idx = 0; idx < 3; ++idx) {
        a2b_queue[idx] = a2b_header_str[idx];
        a2b_queue_more[idx] = a2b_header_hires_str[idx];
    }

    for (int idx = 1; idx <= nm_valid; ++idx) {
        const std::string name = at1(ts.instr_names, idx).str();
        const Instrument& ins = at1(ts.instr_data, idx);
        std::string& q = a2b_queue[2 + idx];
        std::string& qm = a2b_queue_more[2 + idx];

        q = "~" + ExpStrR(Copy(name, 1, 9) + "~" + Copy(name, 10, 32), 45, ' ');
        const FmInstData& fm = ins.fm_data;
        qm = q + byte2hex(fm.AM_VIB_EG_carrier) + byte2hex(fm.AM_VIB_EG_modulator) + " " + byte2hex(fm.KSL_VOLUM_carrier) +
             byte2hex(fm.KSL_VOLUM_modulator) + " " + byte2hex(fm.ATTCK_DEC_carrier) + byte2hex(fm.ATTCK_DEC_modulator) + " " +
             byte2hex(fm.SUSTN_REL_carrier) + byte2hex(fm.SUSTN_REL_modulator) + " " + byte2hex(fm.WAVEFORM_carrier) +
             byte2hex(fm.WAVEFORM_modulator) + " " + byte2hex(fm.FEEDBACK_FM) + "   ";

        std::string temp_str = ins.panning <= 2 ? std::string(ins_pan_str2[ins.panning]) : ExpStrL("", 7, '\xFA');
        q += temp_str + "   ";
        qm += temp_str + "   ";

        if (ins.fine_tune > 0) temp_str = "+" + ExpStrR(Num2str(ins.fine_tune, 16), 5, ' ');
        else if (ins.fine_tune < 0) temp_str = "-" + ExpStrR(Num2str(0 - ins.fine_tune, 16), 5, ' ');
        else temp_str = ExpStrR("", 6, ' ');
        q += temp_str + "   ";
        qm += temp_str + "   ";

        temp_str = ins.perc_voice <= 5 ? std::string(perc_voice_str[ins.perc_voice]) : ExpStrL("", 7, ' ');
        q += temp_str;
        qm += temp_str;
    }

    uint16_t old_keys[3];
    std::memcpy(old_keys, mn_setting.terminate_keys, sizeof(old_keys));
    void (*const old_external_proc)() = mn_environment.ext_proc;
    const uint8_t old_topic_len = mn_setting.topic_len;
    const bool old_cycle_moves = mn_setting.cycle_moves;

    static const uint16_t new_keys[3] = { kESC, kENTER, kCtENTR };
    std::memcpy(mn_setting.terminate_keys, new_keys, sizeof(new_keys));
    mn_environment.ext_proc = a2b_lister_external_proc;
    mn_setting.topic_len = 3;
    mn_setting.cycle_moves = false;

    if (loadBankPossible)
        mn_environment.context = " ~[~" + Num2str(nm_valid, 10) + "~/255]~ ^ENTER \xC4\x10 LOAD COMPLETE BANK ";
    else
        mn_environment.context = "~[~" + Num2str(nm_valid, 10) + "~/255]~";

    keyboard_reset_buffer();
    const uint8_t ysize = is_default_screen_mode() ? 20 : 30;
    _a2b_lister_count = B(ysize - 3);

    const std::string bank_name = to_cp437(instdata_source);
    const std::string title = " " + iCASE(to_cp437(std::filesystem::path(instdata_source).filename().wstring())) + " ";
    const uint16_t spos = static_cast<uint16_t>(std::max<int32_t>(get_bank_position(bank_name, nm_valid), 1));
    uint16_t index = 0;
    if (!_force_program_quit) {
        if (program_screen_mode == 0 || program_screen_mode == 3 || program_screen_mode == 4 || program_screen_mode == 5)
            index = Menu(a2b_queue, 1, 1, spos, 74, ysize, static_cast<uint16_t>(nm_valid + 3), title);
        else
            index = Menu(a2b_queue_more, 1, 1, spos, 104, 30, static_cast<uint16_t>(nm_valid + 3), title);
    }

    add_bank_position(bank_name, nm_valid, index + 3);
    std::memcpy(mn_setting.terminate_keys, old_keys, sizeof(old_keys));
    mn_environment.ext_proc = old_external_proc;
    mn_setting.topic_len = old_topic_len;
    mn_setting.cycle_moves = old_cycle_moves;

    bool loaded = false;
    const uint16_t key = mn_environment.keystroke;
    if (key == kENTER || (loadBankPossible && key == kCtENTR)) {
        PlayerLock lock;
        FixedSongData& sd = songdata();
        const auto put = [&](uint8_t dst, uint8_t src) {
            at1(sd.instr_data, dst) = at1(ts.instr_data, src);
            auto& name = at1(sd.instr_names, dst);
            name.assign(Copy(name.str(), 1, 9) + Copy(at1(ts.instr_names, src).str(), 10, 32));
        };

        if (key == kENTER) {
            const bool alternate = shift_pressed();
            const bool _4op_ins_flag = select_4op_destination(B(index), alternate);
            if (_4op_ins_flag) {
                put(_4op_idx11, _4op_idx21);
                put(_4op_idx12, _4op_idx22);
                if (!alternate) a2ins::set_4op_flag(sd, _4op_idx11);
            } else {
                // put 2op instrument
                put(current_inst, B(index));
            }
        } else {
            std::memcpy(sd.instr_data, ts.instr_data, sizeof(ts.instr_data));
            sd.ins_4op_flags = ts.ins_4op_flags;
            for (int idx = 1; idx <= 255; ++idx) {
                auto& name = at1(sd.instr_names, idx);
                name.assign(Copy(name.str(), 1, 9) + Copy(at1(ts.instr_names, idx).str(), 10, 32));
            }
        }
        loaded = true;
    }
    keyboard_reset_buffer();
    return loaded;
}
