// ---------------------------------------------------------------------------
// 楽器窓 (instedit.inc の INSTRUMENT_CONTROL / instr_control_proc とその補助処理)
// ---------------------------------------------------------------------------
//
// MainScreen.cpp の無名名前空間の中で #include する (オリジナルの {$i instedit.inc} に相当)。
//
// 楽器名 (String[42]) の 1 文字目はマーク (#16) または 4OP の組の印 (#172 / #173)、2〜9 文字目は ' iNS_xx'#247' ' の番号部分、
// 10 文字目からが名前。オリジナルは Menu に songdata.instr_names をそのまま渡すが、本クローン版の Menu は文字列の配列を
// 受け取るため、instr_list_items を介して受け渡す (instr_control_proc の始めと終わりで同期する)。

constexpr uint8_t dialog_contxt_dis2 = 0x09;
constexpr uint8_t dialog_mod_text = 0x0a;
constexpr uint8_t dialog_car_text = 0x0b;
constexpr uint8_t instrument_bckg = 0x40;
constexpr uint8_t instrument_hi_text = 0x0f;
constexpr uint8_t instrument_ai_off = 0x06;
constexpr uint8_t instrument_ai_on = 0x0d;
constexpr uint8_t instrument_ai_trig = 0x0e;

struct InstItem
{
    const char* str;
    uint8_t colr;
};

// typcons2.inc の inst_itm1 (FM 接続) / inst_itm2 (AM 接続)。[1..18] は [0..17]
const InstItem inst_itm1[18] = {
    { "~ENV.TYPE/MULT~ \xC4\xC4\xC4\xC4\xC4\xBF  ", 1 },
    { "~Carrier~            \xC3\xC4\x10", 3 },
    { "~Modulator~          \xC0\x1B\xC4", 2 },
    { "~KSL/VOLUME~ \xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xBF  ", 1 },
    { "~Carrier~            \xC3\xC4\x10", 3 },
    { "~Modulator~          \xC0\x1B\xC4", 2 },
    { "~ATTACK/DECAY~ \xC4\xC4\xC4\xC4\xC4\xC4\xBF  ", 1 },
    { "~Carrier~            \xC3\xC4\x10", 3 },
    { "~Modulator~          \xC0\x1B\xC4", 2 },
    { "~SUSTAiN/RELEASE~ \xC4\xC4\xC4\xBF  ", 1 },
    { "~Carrier~            \xC3\xC4\x10", 3 },
    { "~Modulator~          \xC0\x1B\xC4", 2 },
    { "~WAVEFORM~ \xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xBF  ", 1 },
    { "~Carrier~            \xC3\xC4\x10", 3 },
    { "~Modulator~          \xC0\x1B\xC4", 2 },
    { "~CONNECTiON/FB~ \x02\x03 \xC4\xC4\x10", 1 },
    { "~PANNiNG~ \x02\x03 \xC4\xC4\x10", 1 },
    { "~F-TUNE~  \x02\x03 \xC4\xC4\x10", 1 },
};

const InstItem inst_itm2[18] = {
    { "~ENV.TYPE/MULT~ \xC4\xC4\xC4\xC4\xC4\xBF  ", 1 },
    { "~Carrier~            \xC3\xC4\x10", 3 },
    { "~Modulator~          \xC0\xC4\x10", 2 },
    { "~KSL/VOLUME~ \xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xBF  ", 1 },
    { "~Carrier~            \xC3\xC4\x10", 3 },
    { "~Modulator~          \xC0\xC4\x10", 2 },
    { "~ATTACK/DECAY~ \xC4\xC4\xC4\xC4\xC4\xC4\xBF  ", 1 },
    { "~Carrier~            \xC3\xC4\x10", 3 },
    { "~Modulator~          \xC0\xC4\x10", 2 },
    { "~SUSTAiN/RELEASE~ \xC4\xC4\xC4\xBF  ", 1 },
    { "~Carrier~            \xC3\xC4\x10", 3 },
    { "~Modulator~          \xC0\xC4\x10", 2 },
    { "~WAVEFORM~ \xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xBF  ", 1 },
    { "~Carrier~            \xC3\xC4\x10", 3 },
    { "~Modulator~          \xC0\xC4\x10", 2 },
    { "~CONNECTiON/FB~ \x02\x03 \xC4\xC4\x10", 1 },
    { "~PANNiNG~ \x02\x03 \xC4\xC4\x10", 1 },
    { "~F-TUNE~  \x02\x03 \xC4\xC4\x10", 1 },
};

// typcons1.inc。[1..13] は [0..12]
const uint8_t pos2[13] = { 0, 1, 3, 4, 6, 7, 9, 10, 12, 13, 14, 15, 16 };
const uint8_t _instr_data_ofs[13] = { 1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 10, 11, 12 };
const char* const connection_str[2] = { "FM", "AM" };

// typcons2.inc
const char* const perc_voice_str[6] = { "MELODiC", "PERC:BD", "PERC:SD", "PERC:TT", "PERC:TC", "PERC:HH" };

const char* const copy_menu_str1[15] = {
    " COPY ORDER DEF.",
    " COPY ORDER TABLE",
    " COPY ~i~NSTRUMENT",
    " COPY iNSTRUMENT ~B~ANK",
    " COPY NOTE",
    " COPY iNSTRUMENT DEF.",
    " COPY 1ST EFFECT",
    " COPY 2ND EFFECT",
    " COPY LiNE",
    " COPY TRACK",
    " COPY PATTERN",
    " COPY MARKED BLOCK",
    " COPY MACRO-TABLE LiNE",
    " COPY MACRO-TABLE COLUMN",
    " COPY MACRO-TABLE",
};

uint8_t copypos1 = 1;

// 試聴で鳴らすオペレータ (楽器エディタで切り替える。[1..4])
bool _operator_enabled[5] = { false, true, true, true, true };

uint8_t color_table[4];   // [1..3]
std::vector<std::string> instr_list_items(255);

const uint8_t _4op_main_chan[6] = { 2, 4, 6, 11, 13, 15 };
const uint8_t _perc_sim_chan[2] = { 18, 17 };   // [19..20]

// songdata.instr_data[ins] の ofs バイト目 (pBYTE(@Addr(songdata.instr_data[ins])^)[ofs])
uint8_t ins_byte(uint8_t ins, int ofs)
{
    return instr_data_bytes(ins)[ofs];
}

void instr_names_to_list()
{
    const FixedSongData& sd = songdata();
    for (int temp = 1; temp <= 255; ++temp) instr_list_items[temp - 1] = at1(sd.instr_names, temp).str();
}

void list_to_instr_names()
{
    FixedSongData& sd = songdata();
    for (int temp = 1; temp <= 255; ++temp) {
        auto& name = at1(sd.instr_names, temp);
        if (instr_list_items[temp - 1] != name.str()) name.assign(instr_list_items[temp - 1]);
    }
}

// 4OP の組 (上位バイト: 1 つ目、下位バイト: 2 つ目)。get_4op_to_test と違い songdata.flag_4op を見ない
uint16_t check_4op_to_test()
{
    return a2ins::check_4op_instrument(songdata(), current_inst);
}

// adt2unit.pas の update_instr_data: 楽器 ins を鳴らしているチャンネルの音色を設定し直す
void update_instr_data(uint8_t ins)
{
    A2Player& p = P();
    for (int temp = 1; temp <= 20; ++temp) {
        if (p.voice_table[temp] == ins) {
            p.reset_chan[temp] = true;
            p.set_ins_data(ins, B(temp));
            p.change_frequency(B(temp), static_cast<uint16_t>(A2Player::nFreq(B((p.event_table[temp].note & 0x7f) - 1)) +
                                                              static_cast<int8_t>(p.ins_parameter(ins, 12))));
        }
    }
}

void reset_instrument_preview()
{
    A2Player& p = P();
    if (p.play_status != PlayStatus::Playing) p.reset_player();
    const uint16_t temp = check_4op_to_test();
    if (!(temp != 0)) {
        update_instr_data(instrum_page);
    } else {
        update_instr_data(LO(temp));
        update_instr_data(HI(temp));
    }
}

// adt2unit.pas の get_chanpos (SDL 版)。内側のループは 1 回目で抜けるため、data[chan] だけを調べる。
// その結果、4OP でない試聴は 1 チャンネルだけで鳴る (オリジナルと同じ)。
uint8_t get_chanpos(const uint8_t* data, uint8_t channels, uint8_t scancode)
{
    uint8_t result = 0;
    for (int chan = 0; chan <= channels; ++chan) {
        uint8_t chan2 = 0;
        if (chan <= channels - 1 && data[chan] == scancode) ++chan2;
        if (chan2 == 0) {
            result = 0;
            break;
        }
        if (P().is_4op_chan(chan2)) {
            ++chan;
            continue;
        }
        result = chan2;
        break;
    }
    return result;
}

uint8_t get_chanpos2(const uint8_t* data, uint8_t channels, uint8_t scancode)
{
    for (int chan = 0; chan <= channels - 1; ++chan) {
        if (data[chan] == scancode) return B(chan + 1);
    }
    return 0;
}

bool is_empty_data(const void* data, size_t size)
{
    const auto* p = static_cast<const uint8_t*>(data);
    return std::all_of(p, p + size, [](uint8_t b) { return b == 0; });
}

// 楽器の試聴。MIDI ボードの鍵盤を押している間、楽器 instr (4OP なら instr2 と組で) を鳴らす。
// ロックを取った状態で呼ぶ (鍵盤を離すのを待つ間は外す)。
void INSTRUMENT_test(uint8_t instr, uint8_t instr2, uint8_t /*chan*/, uint16_t fkey, bool process_macros)
{
    if (ctrl_pressed() || alt_pressed() || shift_pressed()) return;

    A2Player& p = P();
    FixedSongData& sd = songdata();

    uint8_t nm_slots = 0;
    for (int temp = 1; temp <= 4; ++temp) {
        if (_operator_enabled[temp]) ++nm_slots;
    }
    const bool _1op_preview = nm_slots == 1;
    const bool _4op_mode = !_1op_preview && sd.flag_4op != 0 && instr2 != 0;

    bool valid_key = false;
    int temp = 1;
    for (; temp <= 29; ++temp) {
        if (!shift_pressed() && board_scancodes[temp - 1] == HI(fkey)) {
            valid_key = true;
            break;
        }
    }
    {
        const int note = temp + 12 * (current_octave - 1) - 1;
        if (!valid_key || !(note >= 0 && note <= 12 * 8 + 1)) return;
    }

    if ((is_empty_data(&at1(sd.instr_data, instr), sizeof(Instrument)) &&
         is_empty_data(&at1(sd.instr_macros, instr), sizeof(RegisterTable))) ||
        (_4op_mode && is_empty_data(&at1(sd.instr_data, instr2), sizeof(Instrument)) &&
         is_empty_data(&at1(sd.instr_macros, instr2), sizeof(RegisterTable)))) {
        return;
    }

    uint8_t chan_handle[21] = {};   // [1..20]
    uint8_t channels;

    // 鍵盤 board_pos の音を chan (chan_handle の位置) で鳴らす
    const auto output_note = [&](uint8_t chan, uint8_t board_pos) {
        const int note = board_pos + 12 * (current_octave - 1);
        if (!(note >= 0 && note <= 12 * 8 + 1)) return false;

        chan_handle[chan] = board_scancodes[board_pos - 1];
        // オリジナルは _4op_main_chan[1..6] の範囲外 (パーカッションの 16..20) も読むが、ここでは変えない
        if (_4op_mode && chan <= 6) chan = _4op_main_chan[chan - 1];

        if (_1op_preview) {
            Instrument ins = (_operator_enabled[1] || _operator_enabled[2]) ? at1(sd.instr_data, instr) : at1(sd.instr_data, instr2);
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
            p.load_instrument(instr_data_bytes(instr), chan);
            const uint16_t vol = p.volume_table[chan];
            p.set_ins_volume(B(0x3f - int(_operator_enabled[1]) * (0x3f - LO(vol))),
                             B(0x3f - int(_operator_enabled[2]) * (0x3f - HI(vol))), chan);
            const uint8_t perc_voice = at1(sd.instr_data, instr).perc_voice;
            if (p.percussion_mode && (perc_voice == 4 || perc_voice == 5) && (chan == 19 || chan == 20))
                p.load_instrument(instr_data_bytes(instr), _perc_sim_chan[chan - 19]);
            if (_4op_mode) {
                p.load_instrument(instr_data_bytes(instr2), B(chan - 1));
                const uint16_t vol2 = p.volume_table[chan - 1];
                p.set_ins_volume(B(0x3f - int(_operator_enabled[3]) * (0x3f - LO(vol2))),
                                 B(0x3f - int(_operator_enabled[4]) * (0x3f - HI(vol2))), B(chan - 1));
            }
        }

        p.macro_speedup = sd.macro_speedup;
        if (p.play_status == PlayStatus::Stopped) p.update_timer(sd.tempo);

        const uint16_t freq = static_cast<uint16_t>(A2Player::nFreq(B(note - 1)) + 0x2000 +
                                                    static_cast<int8_t>(ins_byte(instr, 12)));
        p.event_table[chan].note = B(note);
        p.freq_table[chan] = freq;
        p.freqtable2[chan] = freq;
        p.key_on(chan);
        p.change_freq(chan, freq);

        const auto clear_macro = [&](uint8_t ch) {
            p.macro_table[ch].fmreg_table = 0;
            p.macro_table[ch].arpg_table = 0;
            p.macro_table[ch].vib_table = 0;
        };
        if (process_macros) {
            if (!(_1op_preview && (_operator_enabled[3] || _operator_enabled[4]))) p.init_macro_table(chan, B(note), instr, freq);
            else p.init_macro_table(chan, B(note), instr2, freq);
        } else {
            clear_macro(chan);
        }

        if (_4op_mode && !_1op_preview) {
            if (process_macros) p.init_macro_table(B(chan - 1), B(note), instr2, freq);
            else clear_macro(B(chan - 1));
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

    uint8_t temp2 = B(temp);
    const bool status_backup_replay_forbidden = p.replay_forbidden;
    const PlayStatus status_backup_play_status = p.play_status;
    p.replay_forbidden = true;
    if (p.play_status != PlayStatus::Stopped) p.play_status = PlayStatus::Paused;
    p.nul_volume_bars();
    really_no_status_refresh = true;

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
    } else if (_4op_mode) {
        sd.flag_4op = 0x3f;
        channels = 6;
    } else {
        sd.flag_4op = 0;
        channels = !p.percussion_mode ? 18 : 15;
    }

    p.reset_player();
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

    const uint8_t perc_voice = at1(sd.instr_data, instr).perc_voice;
    if (p.percussion_mode && perc_voice >= 1 && perc_voice <= 5) {
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

    for (int temp = 1; temp <= 20; ++temp) p.key_off(B(temp));
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

// 楽器窓の左側 (選んだ楽器のパラメータ) と下の枠の表示
void INSTRUMENT_CONTROL_page_refresh(uint8_t page)
{
    const FixedSongData& sd = songdata();
    const tSCREEN_MEM_PTR dest = mn_environment.v_dest;
    const uint8_t xs = INSCTRL_xshift;
    const int ys = INSCTRL_yshift;
    const auto Y = [ys](int y) { return B(ys + y); };

    update_4op_flag_marks();
    const InstItem* items = (P().ins_parameter(page, 10) & 1) != 1 ? inst_itm1 : inst_itm2;
    for (int temp = 1; temp <= 18; ++temp) {
        ShowCStr(dest, B(xs + 10), Y(6 + temp), items[temp - 1].str, dialog_background + dialog_contxt_dis2,
                 color_table[items[temp - 1].colr]);
    }

    ShowStr(dest, B(xs + 56), Y(5), byte2hex(page), dialog_background + dialog_title);

    for (int temp = 1; temp <= 10; ++temp) {
        ShowStr(dest, B(xs + 32), Y(8 + pos2[temp - 1]), byte2hex(ins_byte(page, _instr_data_ofs[temp - 1])),
                dialog_background + dialog_item);
    }

    // オリジナルはループを抜けた後の temp (= 10) の色を使う
    const uint8_t conn = ins_byte(page, _instr_data_ofs[10]);
    ShowCStr(dest, B(xs + 30), Y(8 + pos2[10]),
             std::string(connection_str[conn & 1]) + "~/~" + static_cast<char>('0' + ((conn >> 1) & 7)),
             dialog_background + dialog_item, color_table[inst_itm1[9].colr]);

    const Instrument& ins = at1(sd.instr_data, page);
    switch (ins.panning) {
    case 0: ShowStr(dest, B(xs + 24), Y(8 + pos2[11]), "CENTER", dialog_background + dialog_item); break;
    case 1: ShowStr(dest, B(xs + 24), Y(8 + pos2[11]), "LEFT  ", dialog_background + dialog_item); break;
    case 2: ShowStr(dest, B(xs + 24), Y(8 + pos2[11]), "RiGHT ", dialog_background + dialog_item); break;
    }

    ShowStr(dest, B(xs + 24), Y(8 + pos2[12]),
            (ins.fine_tune < 0 ? "-" : "+") + ExpStrR(Num2str(std::abs(ins.fine_tune), 16), 2, ' '),
            dialog_background + dialog_item);

    uint8_t _status_shift_pos = 0;
    if (check_4op_flag(page) || (page > 1 && check_4op_flag(B(page - 1)))) {
        _status_shift_pos = 5;
        ShowCStr(dest, B(xs + 9), Y(26), " [~\x04\x03~] \xCD\xCD\xCD", dialog_background + dialog_border,
                 dialog_background + dialog_context);
    } else {
        ShowCStr(dest, B(xs + 9), Y(26), std::string(" [~") + perc_voice_str[std::min<int>(ins.perc_voice, 5)] + "~] ",
                 dialog_background + dialog_border, dialog_background + dialog_context);
    }

    const RegisterTable& macros = at1(sd.instr_macros, page);
    std::string temp_str = macros.length != 0 ? " [~MACRO:FM" : " ";
    if (macros.arpeggio_table != 0) temp_str += temp_str != " " ? "+ARP" : "[~MACRO:ARP";
    if (macros.vibrato_table != 0) temp_str += temp_str != " " ? "+ViB" : "[~MACRO:ViB";
    if (temp_str != " ") temp_str += "~] ";

    ShowCStr(dest, B(xs + 19 - _status_shift_pos), Y(26), ExpStrR(temp_str, 27, '\xCD'), dialog_background + dialog_border,
             dialog_background + dialog_context);

    STATUS_LINE_refresh();
}

// instedit.inc の copy_object (Ctrl+C / Shift+Ctrl+C / Alt+C)
void instr_copy_object()
{
    const FixedSongData& sd = songdata();
    auto& ci = clipboard.instrument;
    switch (clipboard.object_type) {
    case objInstrument:
        ci.names[1].assign(Copy(at1(sd.instr_names, instrum_page).str(), 10, 32));
        ci.data[1] = at1(sd.instr_data, instrum_page);
        break;

    case objInstrumentWithMacros:
        ci.names[1].assign(Copy(at1(sd.instr_names, instrum_page).str(), 10, 32));
        ci.data[1] = at1(sd.instr_data, instrum_page);
        ci.macros[1] = at1(sd.instr_macros, instrum_page);
        std::memcpy(ci.dis_fmreg_col[1], at1(sd.dis_fmreg_col, instrum_page), sizeof(DisFmregCol));
        break;

    case objInstrumentBank:
        for (int temp = 1; temp <= 255; ++temp) {
            ci.names[temp].assign(Copy(at1(sd.instr_names, temp).str(), 10, 32));
            ci.data[temp] = at1(sd.instr_data, temp);
            ci.macros[temp] = at1(sd.instr_macros, temp);
            ci.arpvib[temp] = at1(sd.macro_table, temp);
            std::memcpy(ci.dis_fmreg_col[temp], at1(sd.dis_fmreg_col, temp), sizeof(DisFmregCol));
        }
        break;

    default:
        break;
    }
}

// instedit.inc の paste_object (mode 0: 名前と内容、1: 内容だけ、2: 名前だけ)
void instr_paste_object(uint8_t mode)
{
    FixedSongData& sd = songdata();
    const auto& ci = clipboard.instrument;
    const auto paste_name = [&](int ins, int src) {
        auto& name = at1(sd.instr_names, ins);
        name.assign(Copy(name.str(), 1, 9) + ci.names[src].str());
    };

    switch (clipboard.object_type) {
    case objInstrument:
        switch (mode) {
        case 0:
            paste_name(instrum_page, 1);
            at1(sd.instr_data, instrum_page) = ci.data[1];
            reset_instrument_preview();
            break;
        case 1:
            at1(sd.instr_data, instrum_page) = ci.data[1];
            reset_instrument_preview();
            break;
        case 2:
            paste_name(instrum_page, 1);
            break;
        }
        break;

    case objInstrumentWithMacros:
        switch (mode) {
        case 0:
            paste_name(instrum_page, 1);
            [[fallthrough]];
        case 1:
            at1(sd.instr_data, instrum_page) = ci.data[1];
            at1(sd.instr_macros, instrum_page) = ci.macros[1];
            std::memcpy(at1(sd.dis_fmreg_col, instrum_page), ci.dis_fmreg_col[1], sizeof(DisFmregCol));
            reset_instrument_preview();
            break;
        case 2:
            paste_name(instrum_page, 1);
            break;
        }
        break;

    case objInstrumentBank:
        for (int temp = 1; temp <= 255; ++temp) {
            switch (mode) {
            case 0:
                paste_name(temp, temp);
                [[fallthrough]];
            case 1:
                at1(sd.instr_data, temp) = ci.data[temp];
                at1(sd.instr_macros, temp) = ci.macros[temp];
                at1(sd.macro_table, temp) = ci.arpvib[temp];
                std::memcpy(at1(sd.dis_fmreg_col, temp), ci.dis_fmreg_col[temp], sizeof(DisFmregCol));
                update_instr_data(B(temp));
                break;
            case 2:
                paste_name(temp, temp);
                break;
            }
        }
        break;

    default:
        break;
    }
}

// 一覧の左の、各楽器が鳴っているかの表示 (instr_control_ai。realtime_gfx_poll_proc から呼ばれる)
void instr_control_ai()
{
    A2Player& p = P();
    const FixedSongData& sd = songdata();
    const int curr_page = mn_environment.curr_page;
    std::string istr;
    // オリジナルは iflag[1..18] の範囲外 (チャンネル番号で引く 19, 20) も読む。ここでは 0 として扱う
    uint8_t iflag[21] = {};
    for (int temp = 1; temp <= 18; ++temp) iflag[temp] = p.ai_table[B(curr_page + temp - 1)];

    for (int temp2 = 1; temp2 <= 18; ++temp2) {
        for (int temp = 1; temp <= sd.nm_tracks; ++temp) {
            if (p.event_table[temp].instr_def == curr_page + temp2 - 1 && p.channel_flag[temp]) {
                if (p.event_new[temp] && iflag[temp] < 2) iflag[temp2] = 2;
                else if (iflag[temp2] < 1) iflag[temp2] = 1;
            }
        }
        switch (iflag[temp2]) {
        case 0: istr += "\x1C"; break;
        case 1: istr += "~\x1C~"; break;
        case 2: istr += "`\x0B`"; break;
        }
    }

    for (int temp = 1; temp <= 18; ++temp) {
        if (iflag[temp] != 0) p.ai_table[B(curr_page + temp - 1)] = 1;
    }

    ShowVC3Str(mn_environment.v_dest, B(INSCTRL_xshift + 37), B(INSCTRL_yshift + 7), istr,
               dialog_background + instrument_ai_off, dialog_background + instrument_ai_on,
               dialog_background + instrument_ai_trig);
}

// 楽器窓の Menu の ext_proc
void instr_control_proc()
{
    PlayerLock lock;
    A2Player& p = P();
    FixedSongData& sd = songdata();
    list_to_instr_names();

    const auto refresh_list = []() {
        instr_names_to_list();
        mn_environment.do_refresh = true;
        mn_environment.refresh();
    };

    const uint16_t key = mn_environment.keystroke;
    switch (key) {
    case kCtLEFT:
        if (!p.debugging && p.play_status == PlayStatus::Playing) p.rewind = true;
        break;

    case kCtRGHT:
        if (!p.debugging && p.play_status == PlayStatus::Playing) p.fast_forward = true;
        break;

    case kCHmins:
    case kNPmins:
    case kCtHOME:
        // 前のオーダーへ (Ctrl+Home は同じ行へ)
        if (p.play_status != PlayStatus::Stopped && !mn_environment.is_editing && !p.play_single_patt) {
            uint8_t temp = p.current_order;
            const uint8_t temp2 = p.current_line;
            while (temp > 0 && !(sd.pattern_order[temp - 1] < 0x80)) --temp;
            if (temp > 0) {
                --temp;
                if (sd.pattern_order[temp] < 0x80) {
                    fade_out_playback(false);
                    calibrate_player(temp, key == kCtHOME ? temp2 : 0, true);
                }
            }
        }
        break;

    case kCHplus:
    case kNPplus:
    case kCtEND:
        // 次のオーダーへ (Ctrl+End は同じ行へ)
        if (p.play_status != PlayStatus::Stopped && !mn_environment.is_editing && !p.play_single_patt) {
            uint8_t temp = p.current_order;
            const uint8_t temp2 = p.current_line;
            while (temp < 0x7f && p.order_at(B(temp + 1)) > 0x80) ++temp;
            if (temp < 0x7f) {
                ++temp;
                if (sd.pattern_order[temp] < 0x80) {
                    fade_out_playback(false);
                    calibrate_player(temp, key == kCtEND ? temp2 : 0, true);
                }
            }
        }
        break;

    case kF5:
        if (p.play_single_patt && p.play_status == PlayStatus::Paused) {
            p.replay_forbidden = false;
            p.play_status = PlayStatus::Playing;
        } else {
            switch (p.play_status) {
            case PlayStatus::Playing:
                p.debugging = false;
                p.no_sync_playing = true;
                break;
            case PlayStatus::Paused:
                p.debugging = false;
                p.replay_forbidden = false;
                p.play_status = PlayStatus::Playing;
                p.no_sync_playing = true;
                break;
            case PlayStatus::Stopped:
                p.no_sync_playing = true;
                start_playing();
                break;
            }
        }
        break;

    case kF6:
        switch (p.play_status) {
        case PlayStatus::Playing:
            if (!p.debugging) {
                p.replay_forbidden = true;
                p.play_status = PlayStatus::Paused;
            }
            p.debugging = false;
            break;
        case PlayStatus::Paused:
            p.debugging = false;
            p.replay_forbidden = false;
            p.play_status = PlayStatus::Playing;
            break;
        default:
            break;
        }
        break;

    case kF7:
        if (p.play_status != PlayStatus::Stopped) {
            fade_out_playback(false);
            stop_playing();
            PATTERN_ORDER_page_refresh(pattord_page);
            PATTERN_page_refresh(pattern_page);
        } else {
            for (int temp = 1; temp <= 20; ++temp) p.reset_chan_data(B(temp));
        }
        break;

    case kF9:
        if (p.play_single_patt && p.play_status == PlayStatus::Paused) {
            p.replay_forbidden = false;
            p.play_status = PlayStatus::Playing;
        } else {
            switch (p.play_status) {
            case PlayStatus::Playing:
                p.debugging = false;
                p.repeat_pattern = true;
                p.no_sync_playing = true;
                break;
            case PlayStatus::Paused:
                p.debugging = false;
                p.repeat_pattern = true;
                p.replay_forbidden = false;
                p.play_status = PlayStatus::Playing;
                p.no_sync_playing = true;
                break;
            case PlayStatus::Stopped:
                start_playing();
                p.debugging = false;
                p.repeat_pattern = true;
                p.replay_forbidden = false;
                p.play_status = PlayStatus::Playing;
                p.no_sync_playing = true;
                break;
            }
        }
        break;

    case kCtrlC:
        clipboard.object_type = !shift_pressed() ? objInstrument : objInstrumentWithMacros;
        instr_copy_object();
        break;

    case kAltP:
        instr_paste_object(0);
        refresh_list();
        break;

    case kCtrlV:
        instr_paste_object(!shift_pressed() ? 0 : 1);
        refresh_list();
        break;

    case kAltV:
        if (!shift_pressed()) instr_paste_object(2);
        refresh_list();
        break;

    case kCtrlW:
        // マークした 2 つの楽器を入れ替える (Shift を押していなければ名前も)。どちらも 4OP の組なら組ごと入れ替える
        if (marked_instruments() == 2) {
            uint8_t ins1 = 1;
            while (instr_name_1st_char(ins1) != _ins_mark_chr) ++ins1;
            uint8_t ins2 = B(ins1 + 1);
            while (instr_name_1st_char(ins2) != _ins_mark_chr) ++ins2;

            const auto swap_instruments = [&](uint8_t a, uint8_t b) {
                const std::string temp_name = at1(sd.instr_names, a).str();
                const Instrument temp_data = at1(sd.instr_data, a);
                std::swap(at1(sd.instr_macros, a), at1(sd.instr_macros, b));
                at1(sd.instr_data, a) = at1(sd.instr_data, b);
                update_instr_data(a);
                at1(sd.instr_data, b) = temp_data;
                update_instr_data(b);
                if (!shift_pressed()) {
                    auto& name1 = at1(sd.instr_names, a);
                    auto& name2 = at1(sd.instr_names, b);
                    name1.assign(Copy(name1.str(), 1, 9) + Copy(name2.str(), 10, 32));
                    name2.assign(Copy(name2.str(), 1, 9) + Copy(temp_name, 10, 32));
                }
            };

            const uint16_t ins_4op = a2ins::check_4op_instrument(sd, ins1);
            const uint16_t ins_4op_2 = a2ins::check_4op_instrument(sd, ins2);
            if (!(ins_4op != 0 && ins_4op_2 != 0)) {
                swap_instruments(ins1, ins2);
            } else {
                swap_instruments(HI(ins_4op), HI(ins_4op_2));   // 4OP 1/2
                swap_instruments(LO(ins_4op), LO(ins_4op_2));   // 4OP 2/2
            }
            refresh_list();
        }
        break;

    case kTAB:
        reset_marked_instruments();
        break;

    case kSPACE:
        if (instr_name_1st_char(instrum_page) != _ins_mark_chr) {
            if (marked_instruments() < 2) {
                instr_name_1st_char(instrum_page) = _ins_mark_chr;
            } else {
                reset_marked_instruments();
                instr_name_1st_char(instrum_page) = _ins_mark_chr;
            }
        } else {
            reset_marked_instruments();
        }
        refresh_list();
        break;

    default:
        break;
    }

    instrum_page = B(mn_environment.curr_pos);
    current_inst = instrum_page;

    // Shift+M / B / S / T / C / H: パーカッションの種類、Shift+O: 4OP の組の切り替え
    const char chr = UpCase(static_cast<char>(LO(key)));
    if (shift_pressed() && std::strchr("MOBSTCH", chr) != nullptr && chr != '\0') {
        if (chr == 'O') {
            at1(sd.instr_data, current_inst).perc_voice = 0;
            if (current_inst != 255) at1(sd.instr_data, current_inst + 1).perc_voice = 0;
            else at1(sd.instr_data, current_inst - 1).perc_voice = 0;
            if (current_inst != 255) {
                if (check_4op_flag(current_inst)) a2ins::reset_4op_flag(sd, current_inst);
                else a2ins::set_4op_flag(sd, current_inst);
            } else {
                if (check_4op_flag(B(current_inst - 1))) a2ins::reset_4op_flag(sd, B(current_inst - 1));
                else a2ins::set_4op_flag(sd, B(current_inst - 1));
            }
            update_4op_flag_marks();
            refresh_list();
        } else if (!check_4op_flag(current_inst)) {
            uint8_t& perc_voice = at1(sd.instr_data, current_inst).perc_voice;
            switch (chr) {
            case 'M': perc_voice = 0; break;
            case 'B': perc_voice = 1; break;
            case 'S': perc_voice = 2; break;
            case 'T': perc_voice = 3; break;
            case 'C': perc_voice = 4; break;
            case 'H': perc_voice = 5; break;
            }
        }
    }

    INSTRUMENT_CONTROL_page_refresh(instrum_page);
    const uint16_t ins_4op = get_4op_to_test();
    if (ins_4op != 0 && sd.flag_4op != 0) {
        const std::string temps = std::string(connection_str[ins_byte(HI(ins_4op), _instr_data_ofs[10]) & 1]) + "/" +
                                  connection_str[ins_byte(LO(ins_4op), _instr_data_ofs[10]) & 1];
        ShowCStr(mn_environment.v_dest, B(INSCTRL_xshift + 39), B(INSCTRL_yshift + 26),
                 " ~[~\xF4" + byte2hex(HI(ins_4op)) + ",\xF5" + byte2hex(LO(ins_4op)) + " " + temps + "~]~ ",
                 dialog_background + dialog_context, dialog_background + dialog_border);
    } else if (check_4op_to_test() != 0) {
        ShowCStr(mn_environment.v_dest, B(INSCTRL_xshift + 39), B(INSCTRL_yshift + 26),
                 ExpStrR(" ~[~\x02\x03 MODE~]~ ", 17, '\xCD'), dialog_background + dialog_context,
                 dialog_background + dialog_border);
    } else {
        ShowStr(mn_environment.v_dest, B(INSCTRL_xshift + 39), B(INSCTRL_yshift + 26), ExpStrL("", 17, '\xCD'),
                dialog_background + dialog_border);
    }

    instr_names_to_list();

    if (!shift_pressed() && !alt_pressed() && !ctrl_pressed()) {
        const uint16_t t = get_4op_to_test();
        if (t != 0) INSTRUMENT_test(LO(t), HI(t), count_channel(pattern_hpos), key, true);
        else INSTRUMENT_test(instrum_page, 0, count_channel(pattern_hpos), key, true);
    }
}

void FILE_save(const wchar_t* ext, bool quick);
void FILE_open_instrument();

// Ctrl+I: 楽器窓。Enter で閉じたら選んだ楽器、それ以外は 0 を返す
uint8_t INSTRUMENT_CONTROL()
{
    static const uint16_t new_keys[20] = { kESC, kENTER, kTAB, kShTAB, kCtrlO, kCtrlM, kAltC, kCtrlA, kCtrlQ, kCtrlE,
                                           kF1, kF2, kF4, kAltF2, kCtrlF2, kCtrlS, kShF2, kF3, kCtrlL, kShF3 };
    const uint8_t xs = INSCTRL_xshift;
    const int ys = INSCTRL_yshift;
    const auto Y = [ys](int y) { return B(ys + y); };

    {
        PlayerLock lock;
        update_4op_flag_marks();
        songdata_crc = Update32(&songdata(), sizeof(FixedSongData), 0);
    }

    for (;;) {   // _jmp1
        if (_force_program_quit) return 0;

        keyboard_reset_buffer();
        ScreenMemCopy(screen_ptr, screen_backup);
        mn_environment.v_dest = temp_screen;
        ScreenMemCopy(screen_ptr, mn_environment.v_dest);

        mn_setting.center_box = false;
        mn_setting.cycle_moves = false;
        mn_environment.edit_pos = 9;
        mn_environment.ext_proc = instr_control_proc;
        mn_environment.ext_proc_rt = instr_control_ai;
        mn_environment.context = " TAB \xC4\x10 EDiTOR ";

        uint16_t old_keys[20];
        std::memcpy(old_keys, mn_setting.terminate_keys, sizeof(old_keys));
        std::memcpy(mn_setting.terminate_keys, new_keys, sizeof(new_keys));

        Frame(temp_screen, B(xs + 7), Y(5), B(xs + 83), Y(26), dialog_background + dialog_border,
              " iNSTRUMENT CONTROL (iNS_  ) ", dialog_background + dialog_title, frame_double);

        fr_setting.update_area = false;
        fr_setting.shadow_enabled = false;
        Frame(temp_screen, B(xs + 9), Y(6), B(xs + 34), Y(25), dialog_background + dialog_border, "",
              dialog_background + dialog_title, frame_single);
        Frame(temp_screen, B(xs + 36), Y(6), B(xs + 81), Y(25), dialog_background + dialog_border, "",
              dialog_background + dialog_title, frame_single);

        ShowStr(temp_screen, B(xs + 66), Y(25), " TAB \xC4\x10 EDITOR ", dialog_background + dialog_context);
        ShowStr(temp_screen, B(xs + 58), Y(26), " [SHiFT] TAB \xC4\x10 MACROS ", dialog_background + dialog_context);

        ShowVStr(mn_environment.v_dest, B(xs + 37), Y(7), ExpStrL("", 18, '\x1C'), dialog_background + instrument_ai_off);
        ShowVStr(mn_environment.v_dest, B(xs + 38), Y(6), "\xC2" + ExpStrL("", 18, '\xB3') + "\xC1",
                 dialog_background + dialog_border);

        fr_setting.shadow_enabled = true;

        color_table[1] = dialog_background + dialog_context_dis;
        color_table[2] = dialog_background + dialog_mod_text;
        color_table[3] = dialog_background + dialog_car_text;

        {
            PlayerLock lock;
            const InstItem* items = (P().ins_parameter(instrum_page, 10) & 1) != 1 ? inst_itm1 : inst_itm2;
            for (int temp = 1; temp <= 18; ++temp) {
                ShowCStr(temp_screen, B(xs + 10), Y(6 + temp), items[temp - 1].str, dialog_background + dialog_contxt_dis2,
                         color_table[items[temp - 1].colr]);
            }
            instr_names_to_list();
        }

        mn_setting.edit_contents = true;
        mn_setting.shadow_enabled = false;
        mn_setting.frame_enabled = false;
        mn_environment.unpolite = false;
        mn_environment.preview = true;
        mn_environment.hlight_chrs = 1;

        if (!_force_program_quit) Menu(instr_list_items, B(xs + 38), Y(6), instrum_page, 42, 18, 255, "");

        {
            PlayerLock lock;
            INSTRUMENT_CONTROL_page_refresh(instrum_page);
        }
        move_to_screen_data = temp_screen;
        move_to_screen_area[1] = B(xs + 7);
        move_to_screen_area[2] = Y(5);
        move_to_screen_area[3] = B(xs + 83 + 2);
        move_to_screen_area[4] = Y(26 + 1);
        move2screen_alt();

        mn_environment.v_dest = screen_ptr;
        if (!_force_program_quit) Menu(instr_list_items, B(xs + 38), Y(6), instrum_page, 42, 18, 255, "");

        {
            PlayerLock lock;
            list_to_instr_names();
            reset_marked_instruments();
        }
        mn_environment.unpolite = false;
        mn_environment.preview = false;
        mn_setting.frame_enabled = true;
        mn_setting.shadow_enabled = true;
        fr_setting.update_area = true;
        mn_setting.center_box = true;
        mn_environment.context = "";

        std::memcpy(mn_setting.terminate_keys, old_keys, sizeof(old_keys));
        mn_setting.edit_contents = false;
        mn_environment.ext_proc = nullptr;
        mn_environment.ext_proc_rt = nullptr;
        mn_environment.hlight_chrs = 0;

        move_to_screen_data = screen_backup;
        move_to_screen_area[1] = B(xs + 7);
        move_to_screen_area[2] = Y(5);
        move_to_screen_area[3] = B(xs + 83 + 2);
        move_to_screen_area[4] = Y(26 + 1);
        move2screen();

        const uint16_t key = mn_environment.keystroke;
        if (key == kAltC) {
            mn_setting.cycle_moves = true;
            std::vector<std::string> items(std::begin(copy_menu_str1), std::end(copy_menu_str1));
            const uint16_t temp = Menu(items, 1, 1, copypos1, 30, 15, 15, " COPY OBJECT ");
            if (mn_environment.keystroke != kESC) {
                copypos1 = B(temp);
                PlayerLock lock;
                clipboard.object_type = static_cast<tCOPY_OBJECT>(temp);
                instr_copy_object();
            }
            continue;
        }
        if (key == kTAB || key == kCtrlQ || key == kShTAB || key == kCtrlE || key == kCtrlM) {
            // 楽器エディタ (6e-2)・マクロエディタ (6e-3)・マクロブラウザ (6e-4) は未実装
            continue;
        }
        if (key == kCtrlO || key == kF1) {
            // オクターブ設定 (OCTAVE_CONTROL) とヘルプ (HELP) は未実装
            continue;
        }
        if (key == kF2 || key == kCtrlS) {
            FILE_save(L"a2i", false);
            continue;
        }
        if (key == kShF2) {
            FILE_save(L"a2f", false);
            continue;
        }
        if (key == kAltF2 || key == kCtrlF2) {
            // A2B / A2W の保存 (6e-4) は未実装
            continue;
        }
        if (key == kF3 || key == kShF3 || key == kCtrlL) {
            // Shift+F3 (前回のファイルをそのまま読み込む) も、本クローン版ではファイルを選ばせる
            FILE_open_instrument();
            PlayerLock lock;
            reset_instrument_preview();
            update_4op_flag_marks();
            continue;
        }
        if (key == kF4 || key == kCtrlA) {
            NUKE(clearpos);
            continue;
        }
        break;
    }

    PlayerLock lock;
    if (Update32(&songdata(), sizeof(FixedSongData), 0) != songdata_crc) module_archived = false;
    return mn_environment.keystroke == kENTER ? instrum_page : 0;
}
