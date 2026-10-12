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

// ---------------------------------------------------------------------------
// 楽器エディタ (instedit.inc の INSTRUMENT_CONTROL_edit とその補助処理)
// ---------------------------------------------------------------------------

constexpr uint8_t instrument_title = 0x0b;
constexpr uint8_t instrument_border = 0x0f;
constexpr uint8_t instrument_text = 0x07;
constexpr uint8_t instrument_glob = 0x0d;
constexpr uint8_t instrument_hi_glob = 0x0f;
constexpr uint8_t instrument_hid = 0x06;
constexpr uint8_t instrument_mod = 0x02;
constexpr uint8_t instrument_car = 0x03;
constexpr uint8_t instrument_hi_mod = 0x0a;
constexpr uint8_t instrument_hi_car = 0x0b;
constexpr uint8_t instrument_context = 0x0e;
constexpr uint8_t instrument_con_dis = 0x05;
constexpr uint8_t instrument_adsr = 0x60;

const std::string frame_solid_type1 = "        ";

// 楽器ごとに、エディタのカーソル位置 (キャリア / モジュレータ) を覚えておく (adtrack2.ini の remember_ins_pos)
bool remember_ins_pos = true;

// adt2unit.pas の bank_position_list (SDL 版は 500 件まで)
struct BankPosition
{
    std::string bank_name;
    int32_t bank_size;
    int32_t bank_position;
};
std::vector<BankPosition> bank_position_list;
constexpr size_t MAX_NUM_BANK_POSITIONS = 500;

std::string bank_position_key(const std::string& bank_name)
{
    std::string result = bank_name;
    for (char& c : result) c = UpCase(c);
    return result;
}

int32_t get_bank_position(const std::string& bank_name, int32_t bank_size)
{
    const std::string name = bank_position_key(bank_name);
    for (const BankPosition& bp : bank_position_list) {
        if (bp.bank_name == name && (bp.bank_size == bank_size || bank_size == -1)) return bp.bank_position;
    }
    return 0;
}

void add_bank_position(const std::string& bank_name, int32_t bank_size, int32_t bank_position)
{
    const std::string name = bank_position_key(bank_name);
    for (BankPosition& bp : bank_position_list) {
        if (bp.bank_name == name && (bp.bank_size == bank_size || bank_size == -1)) {
            bp.bank_position = bank_position;
            return;
        }
    }
    if (bank_position_list.size() >= MAX_NUM_BANK_POSITIONS) bank_position_list.erase(bank_position_list.begin());
    bank_position_list.push_back({ name, bank_size, bank_position });
}

namespace instedit
{
    // typcons2.inc
    const char* const ins_pan_str1[3] = {
        "~\xFA~\xFA\xFA\xFA\xFA\xFA\x0B\x0B~\x0B~\x0B\x0B\xFA\xFA\xFA\xFA\xFA~\xFA~",
        "~\x0B~\x0B\x0B\x0B\x0B\xFA\xFA\xFA~\xFA~\xFA\xFA\xFA\xFA\xFA\xFA\xFA~\xFA~",
        "~\xFA~\xFA\xFA\xFA\xFA\xFA\xFA\xFA~\xFA~\xFA\xFA\xFA\x0B\x0B\x0B\x0B~\x0B~",
    };

    // 9〜17 行目は _page_build で inst_text_ext_* に置き換わる。
    // オリジナルは [1..23] の配列を 24 行目まで表示する (24 行目は直後の inst_text_ext_2op[1] が出るが、下の枠の表示で隠れる)
    std::string inst_text[25] = {
        "",
        "~ATTACK RATE~   \xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1                     ~WAVEFORM TYPE~",
        "~DECAY RATE~    \xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1                     ( ) Sine",
        "                                                  ( ) Half-Sine",
        "~SUSTAiN LEVEL~ \xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1                     ( ) Abs-Sine",
        "~RELEASE RATE~  \xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1                     ( ) Pulse-Sine",
        "                                                  ( ) Sine (EPO)",
        "~OUTPUT LEVEL~  \xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1     ( ) Abs-Sine (EPO)",
        "                                                  ( ) Square",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "~ENVELOPE TYPE              FREQUENCY DATA MULTiPLiER~",
        "[ ] Tremolo (AM)",
        "[ ] Vibrato                ( )\x9F" "0\x06 ( )\x9F" "1  ( )\x9F" "2  ( )\x9F" "3  ( )\x9F" "4  ( )\x9F" "5",
        "[ ] Key scale rate (KSR)   ( )\x9F" "6  ( )\x9F" "7  ( )\x9F" "8  ( )\x9F" "9  ( )\x9F" "10 ( )\x9F" "10",
        "[ ] Sustain                ( )\x9F" "12 ( )\x9F" "12 ( )\x9F" "15 ( )\x9F" "15",
        "",
    };

    const char* const inst_text_ext_2op[9] = {
        "~KEY SCALiNG LEVEL~   `GLOBAL iNSTRUMENT SETTiNGS`    ( ) Derived square",
        "( ) Default         `\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A`",
        "( ) 1.5 dB/octave   `CONNECTiON`                    `FEEDBACK`",
        "( ) 3.0 dB/octave   ( ) FM                        \xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1",
        "( ) 6.0 dB/octave   ( ) AM                        MiN           MAX",
        "                    ^( ) FM/FM^",
        "~PANNiNG~             ^( ) FM/AM^                     `F-TUNE`",
        "\xAA       c       \xAB   ^( ) AM/FM^                     \xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1",
        "\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA   ^( ) AM/AM^                     -       \xF9       +",
    };

    const char* const inst_text_ext_perc[9] = {
        "~KEY SCALiNG LEVEL~   ^GLOBAL iNSTRUMENT SETTiNGS^    ( ) Derived square",
        "( ) Default         ^\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A^",
        "( ) 1.5 dB/octave   ^CONNECTiON                    FEEDBACK        0^",
        "( ) 3.0 dB/octave   ^( ) FM                        \xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1^",
        "( ) 6.0 dB/octave   ^( ) AM                        MiN           MAX^",
        "                    ^( ) FM/FM^",
        "~PANNiNG~             ^( ) FM/AM^                     `F-TUNE`",
        "\xAA       c       \xAB   ^( ) AM/FM^                     \xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1",
        "\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA   ^( ) AM/AM^                     -       \xF9       +",
    };

    const char* const inst_text_ext_4op[9] = {
        "~KEY SCALiNG LEVEL~   `GLOBAL iNSTRUMENT SETTiNGS`    ( ) Derived square",
        "( ) Default         `\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A\x9A`",
        "( ) 1.5 dB/octave   `CONNECTiON`                    `FEEDBACK`",
        "( ) 3.0 dB/octave   ^( ) FM^                        \xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1",
        "( ) 6.0 dB/octave   ^( ) AM^                        MiN           MAX",
        "                    ( ) FM/FM",
        "~PANNiNG~             ( ) FM/AM                     `F-TUNE`",
        "\xAA       c       \xAB   ( ) AM/FM                     \xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1\xB1",
        "\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA\xFA   ( ) AM/AM                     -       \xF9       +",
    };

    // [conn][1..6] は [conn][0..5]。オリジナルは String[17] の 17 文字目まで読む (4 の 5 行目は 16 文字なので、17 文字目は #0)
    const char* const inst_con_scheme[7][6] = {
        { "                 ",
          "                 ",
          "   \xDA\xC4\xC4\xC4\xBF         ",
          "   \xC0\xDB" "1\xDB\xC1\xDB" "2\xDB\x1A     ",
          "                 ",
          "                 " },
        { "                 ",
          "   \xDA\xC4\xC4\xC4\xBF         ",
          "   \xC0\xDB" "1\xDB\xC1\xBF        ",
          "        \xB3        ",
          "    \xDB" "2\xDB\xC4\xC1\x1A       ",
          "                 " },
        { "                 ",
          "                 ",
          "\xDA\xC4\xC4\xC4\xBF            ",
          "\xC0\xDB" "1\xDB\xC1\xDB" "2\xDB\xC4\xDB" "3\xDB\xC4\xDB" "4\xDB\x1A",
          "                 ",
          "                 " },
        { "                 ",
          " \xDA\xC4\xC4\xC4\xBF           ",
          " \xC0\xDB" "1\xDB\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xBF  ",
          "              \xB3  ",
          "  \xDB" "2\xDB\xC4\xDB" "3\xDB\xC4\xDB" "4\xDB\xC4\xC1\x1A ",
          "                 " },
        { "                 ",
          "  \xDA\xC4\xC4\xC4\xBF          ",
          "  \xC0\xDB" "1\xDB\xC1\xDB" "2\xDB\xC4\xBF     ",
          "           \xC3\x1A    ",
          "   \xDB" "3\xDB\xC4\xDB" "4\xDB\xC4\xD9    ",
          "                 " },
        { " \xDA\xC4\xC4\xC4\xBF           ",
          " \xC0\xDB" "1\xDB\xC1\xC4\xC4\xC4\xC4\xBF      ",
          "          \xB3      ",
          "  \xDB" "2\xDB\xC4\xDB" "3\xDB\xC4\xC5\x1A     ",
          "          \xB3      ",
          "  \xDB" "4\xDB\xC4\xC4\xC4\xC4\xC4\xD9      " },
        { "                 ",
          "                 ",
          "                 ",
          "                 ",
          "                 ",
          "                 " },
    };

    const char* const inst_mult_hint[16] = {
        "1 octave below",
        "at the voice's specified frequency",
        "1 octave above",
        "1 octave and a 5th above",
        "2 octaves above",
        "2 octaves and a Major 3rd above",
        "2 octaves and a 5th above",
        "2 octaves and a Minor 7th above",
        "3 octaves above",
        "3 octaves and a Major 2nd above",
        "3 octaves and a Major 3rd above",
        "3 octaves and a Major 3rd above",
        "3 octaves and a 5th above",
        "3 octaves and a 5th above",
        "3 octaves and a Major 7th above",
        "3 octaves and a Major 7th above",
    };

    const char* const _4op_connection_str[4] = { "FM/FM", "FM/AM", "AM/FM", "AM/AM" };
    const uint8_t _carrier_flag[2] = { 2, 1 };   // [FALSE, TRUE]

    // typcons1.inc。[1..inst_vpos_max][1..inst_hpos_max] をそのままの添字で使う。12〜17 行目は _page_build で
    // inst_hpos_ext_* に置き換わる。オリジナルの kUP は範囲外の 22 行目も読む (直後の inst_hpos_ext_2op[1] を入れておく)
    constexpr int inst_vpos_max = 21;
    constexpr int inst_hpos_max = 7;
    uint8_t inst_hpos[inst_vpos_max + 2][inst_hpos_max + 1] = {
        {},
        { 0, 15, 0, 0, 0, 0, 0, 0 },
        { 0, 15, 52, 0, 0, 0, 0, 0 },
        { 0, 52, 0, 0, 0, 0, 0, 0 },
        { 0, 15, 52, 0, 0, 0, 0, 0 },
        { 0, 15, 52, 0, 0, 0, 0, 0 },
        { 0, 52, 0, 0, 0, 0, 0, 0 },
        { 0, 15, 52, 0, 0, 0, 0, 0 },
        { 0, 52, 0, 0, 0, 0, 0, 0 },
        { 0, 52, 0, 0, 0, 0, 0, 0 },
        { 0, 2, 0, 0, 0, 0, 0, 0 },
        { 0, 2, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 2, 0, 0, 0, 0, 0, 0 },
        { 0, 2, 29, 36, 43, 50, 57, 64 },
        { 0, 2, 29, 36, 43, 50, 57, 64 },
        { 0, 2, 29, 36, 43, 50, 0, 0 },
        { 0, 2, 22, 51, 0, 0, 0, 0 },
    };

    const uint8_t inst_hpos_ext_2op[6][inst_hpos_max + 1] = {
        { 0, 2, 22, 51, 0, 0, 0, 0 },
        { 0, 2, 22, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 51, 0, 0, 0, 0, 0 },
        { 0, 1, 9, 17, 0, 0, 0, 0 },
    };

    const uint8_t inst_hpos_ext_perc[6][inst_hpos_max + 1] = {
        { 0, 2, 0, 0, 0, 0, 0, 0 },
        { 0, 2, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 51, 0, 0, 0, 0, 0 },
        { 0, 1, 9, 17, 0, 0, 0, 0 },
    };

    const uint8_t inst_hpos_ext_4op[6][inst_hpos_max + 1] = {
        { 0, 2, 0, 51, 0, 0, 0, 0 },
        { 0, 2, 0, 0, 0, 0, 0, 0 },
        { 0, 22, 0, 0, 0, 0, 0, 0 },
        { 0, 22, 0, 0, 0, 0, 0, 0 },
        { 0, 22, 51, 0, 0, 0, 0, 0 },
        { 0, 1, 9, 17, 22, 0, 0, 0 },
    };

    const uint8_t inst_vpos[inst_vpos_max + 1] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 20, 21, 22, 23 };

    // instedit.inc の、INSTRUMENT_CONTROL_edit が使う大域変数
    uint8_t xstart = 0, ystart = 0;
    uint8_t hpos = 1, vpos = 1;
    bool carrier = true;
    Instrument* inst = nullptr;
    std::string _curr_connection_str;
    uint8_t _status_shift_pos = 0;
    std::string _4op_conn_str;
    std::string _4op_slot_str;
    std::string _4op_slot_chr;   // オリジナルは Set of Char
    uint8_t _col_mod = 0, _col_hi_mod = 0;
    uint8_t adsr_bckg = 0;
    bool _ADSR_preview_flag = true;

    bool in_set(int value, std::initializer_list<int> set)
    {
        return std::find(set.begin(), set.end(), value) != set.end();
    }

    bool is_perc_2_5(const Instrument* ins) { return ins->perc_voice >= 2 && ins->perc_voice <= 5; }

    // Pascal の min(value, minimum) / max(value, maximum) (AT2 の min は下限、max は上限)
    int min_(int value, int minimum) { return value < minimum ? minimum : value; }
    int max_(int value, int maximum) { return value > maximum ? maximum : value; }

    std::string flipstr(const std::string& str) { return std::string(str.rbegin(), str.rend()); }

    // _show_adsr の、高さ value の縦棒 (上から下へ)
    std::string _gfx_bar_str(uint8_t value)
    {
        std::string result;
        do {
            if (value > 15) {
                result += '\xDB';
                value = B(value - 15);
            }
            if (value <= 15 && value != 0) result += static_cast<char>(127 + value);
        } while (!(value <= 15));
        return flipstr(result);
    }

    uint8_t _conv(uint8_t value, uint8_t variant) { return value != 0 ? variant : 1; }

    void _show_adsr(tSCREEN_MEM_PTR dest, uint8_t xs, uint8_t ys, uint8_t attack, uint8_t decay, uint8_t sustain,
                    uint8_t release, uint8_t attenuation, uint8_t eg_type, uint8_t attr, uint8_t attr_hi, bool reset)
    {
        fr_setting.update_area = false;
        fr_setting.shadow_enabled = false;

        if (!is_default_screen_mode()) {
            Frame(dest, xs, B(ys + 1), B(xs + 61), B(ys + 11), instrument_bckg, "", 0, frame_solid_type1);
        } else if (reset) {
            Frame(dest, xs, ys, B(xs + 63), B(ys + 12), instrument_bckg, "", 0, frame_solid_type1);
        } else {
            Frame(dest, xs, ys, B(xs + 63), B(ys + 12), instrument_adsr + instrument_text, " ADSR PREViEW ",
                  instrument_adsr + instrument_text, frame_double);
        }

        fr_setting.update_area = true;
        fr_setting.shadow_enabled = true;

        const double k = 15 / std::log(15.0);
        if (attack != 0) attack = B(min_(int(pas_round(k * std::log(double(attack)))), 1));
        if (decay != 0) decay = B(min_(int(pas_round(k * std::log(double(decay)))), 1));
        else sustain = 0;
        if (sustain != 0) sustain = B(min_(int(pas_round(k * std::log(double(sustain)))), 1));
        if (release != 0) release = B(min_(int(pas_round(k * std::log(double(release)))), 1));

        if (reset) return;
        int pos = 1;
        double temp = 0;

        const auto bar = [&](uint8_t a) {
            ShowVStr(dest, B(xs + 1 + pos), B(ys + 1),
                     ExpStrL(_gfx_bar_str(B(pas_round(temp / 63 * (63 - attenuation)))), 9, ' '), a);
        };

        for (int index = 0; index <= 15 - attack; ++index) {
            temp = temp + 8 * 16 / double(16 - attack);
            if (attack != 0) {
                bar(attr_hi);
                ++pos;
            }
        }

        if (attack == 0) {
            ++pos;
            temp = 8 * 16;
        }

        if (decay == 0 && (release != 0 || (sustain != 0 && eg_type == 1))) decay = 15;

        if (decay != 0) {
            for (int index = 0; index <= 15 - decay; ++index) {
                temp = temp - 8 * 16 / 16.0 * sustain / (16 - decay);
                bar(attr_hi);
                ++pos;
            }
        }

        if (decay == 0) ++pos;
        if (eg_type != 1 && release != 0) sustain = 0;
        if (sustain == 0 && release != 0) sustain = 1;

        for (int index = 1; index <= sustain; ++index) {
            bar(attr_hi);
            ++pos;
        }

        if (sustain == 0) ++pos;

        const double temp2 = temp;
        for (int index = 1; index <= 15 - release; ++index) {
            temp = temp - temp2 / (16 - release);
            if (release != 0 && temp > 0) {
                bar(eg_type != 1 ? attr_hi : attr);
                ++pos;
            }
        }

        const uint8_t ca = _conv(attack, B(16 - attack));
        const uint8_t cd = _conv(decay, B(16 - decay));
        const uint8_t cs = _conv(sustain, sustain);
        const uint8_t cr = _conv(release, B(15 - release));
        const uint8_t rest = B(cr + 15 - cr + 15 - cs + 15 - cd + 15 - ca);

        if (release > 0) {
            ShowStr(dest, B(xs + 2), B(ys + 10),
                    Copy(Copy(ExpStrR("\xF6", ca, ' ') + ExpStrR("\xF6", cd, ' ') + ExpStrR("\xF6", cs, ' ') +
                                  ExpStrR("\xF6", cr, ' '),
                              1, 60 - 1) +
                             "\xF6" + ExpStrL("", 60, ' '),
                         1, 60),
                    attr_hi);
        } else {
            ShowStr(dest, B(xs + 2), B(ys + 10),
                    ExpStrR("\xF6", ca, ' ') + ExpStrR("\xF6", cd, ' ') + ExpStrR("\xF6", cs, ' ') + ExpStrR("\xF6", rest, ' '),
                    attr_hi);
        }

        if (release > 0) {
            ShowStr(dest, B(xs + 2), B(ys + 11),
                    Copy(Copy(ExpStrR("A", ca, '\xFA') + ExpStrR("D", cd, '\xFA') + ExpStrR("S", cs, '\xFA') +
                                  ExpStrR("R", cr, '\xFA'),
                              1, 60 - 1) +
                             "\xAF" + ExpStrL("", 60, '\xFA'),
                         1, 60),
                    attr_hi);
        } else {
            ShowStr(dest, B(xs + 2), B(ys + 11),
                    ExpStrR("A", ca, '\xFA') + ExpStrR("D", cd, '\xFA') + ExpStrR("S", cs, '\xFA') + ExpStrR("R", rest, '\xFA'),
                    attr_hi);
        }
    }

    // FilterStr1(str, '~') と _4op_connection_str から、接続図の番号 (2〜5) を求める
    uint8_t _4op_conn_to_idx(const std::string& str)
    {
        std::string temps;
        for (char c : str) {
            if (c != '~') temps += c;
        }
        uint8_t result = 2;
        for (int idx = 0; idx <= 3; ++idx) {
            if (temps == _4op_connection_str[idx]) {
                result = B(result + idx);
                break;
            }
        }
        return result;
    }

    // 接続図。hi_op に含まれるオペレータを強調する
    void _show_conn_scheme(uint8_t xs, uint8_t ys, uint8_t conn, const std::string& hi_op)
    {
        const auto chr = [conn](int row, int idx) -> char {
            const char* s = inst_con_scheme[conn][row - 1];
            return idx <= static_cast<int>(std::strlen(s)) ? s[idx - 1] : '\0';
        };

        std::string result[7];
        for (int idx2 = 1; idx2 <= 6; ++idx2) {
            int idx = 1;
            while (idx <= 17) {
                const char c = chr(idx2, idx);
                const char d = chr(idx2, idx + 1);
                if (c == '\xDB' && d >= '1' && d <= '4') {
                    if (hi_op.find(d) != std::string::npos) result[idx2] += std::string("`\xDB`^") + d + "^`\xDB`";
                    else result[idx2] += std::string("\xDB~") + d + "~\xDB";
                    idx += 3;
                } else {
                    result[idx2] += c;
                    ++idx;
                }
            }
        }
        for (int idx = 1; idx <= 6; ++idx) {
            ShowC4Str(temp_screen, xs, B(ys + idx - 1), result[idx], instrument_bckg + instrument_glob,
                      B(instrument_glob << 4), instrument_bckg + instrument_hi_glob, B(instrument_hi_glob << 4));
        }
    }

    Instrument& instr(uint8_t ins) { return at1(songdata().instr_data, ins); }

    uint8_t _get_feedback_val()
    {
        const uint16_t t = get_4op_to_test();
        return B((instr(t == 0 ? current_inst : LO(t)).fm_data.FEEDBACK_FM >> 1) & 7);
    }

    void _inc_feedback_val()
    {
        const uint16_t t = get_4op_to_test();
        uint8_t& feedb = instr(t == 0 ? current_inst : LO(t)).fm_data.FEEDBACK_FM;
        if (((feedb >> 1) & 7) < 7) feedb = B((feedb & 1) + ((((feedb >> 1) & 7) + 1) << 1));
    }

    void _dec_feedback_val()
    {
        const uint16_t t = get_4op_to_test();
        uint8_t& feedb = instr(t == 0 ? current_inst : LO(t)).fm_data.FEEDBACK_FM;
        if (((feedb >> 1) & 7) > 0) feedb = B((feedb & 1) + ((((feedb >> 1) & 7) - 1) << 1));
    }

    int8_t _get_finetune_val()
    {
        const uint16_t t = get_4op_to_test();
        return instr(t == 0 ? current_inst : LO(t)).fine_tune;
    }

    void _inc_finetune_val()
    {
        const uint16_t t = get_4op_to_test();
        int8_t& ft = instr(t == 0 ? current_inst : LO(t)).fine_tune;
        ft = static_cast<int8_t>(max_(ft + 1, 127));
    }

    void _dec_finetune_val()
    {
        const uint16_t t = get_4op_to_test();
        int8_t& ft = instr(t == 0 ? current_inst : LO(t)).fine_tune;
        ft = static_cast<int8_t>(min_(ft - 1, -127));
    }

    // 接続 (FEEDBACK_FM の 0 ビット目)
    const char* conn_of(uint8_t ins) { return connection_str[instr(ins).fm_data.FEEDBACK_FM & 1]; }

    // 項目名などの、カーソル位置で変わらない部分の表示
    void _page_build()
    {
        const uint16_t t4 = get_4op_to_test();
        const bool perc = is_perc_2_5(inst);

        ShowStr(temp_screen, B(xstart + 45), ystart, byte2hex(instrum_page), instrument_bckg + instrument_title);

        if (!is_default_screen_mode()) {
            ShowStr(temp_screen, B(xstart - 1), B(ystart + 24), "\xCC" + ExpStrL("", 70, '\xCD') + "\xB9",
                    instrument_bckg + instrument_border);
        }

        if (!perc && t4 != 0) {
            if (LO(t4) == HI(t4)) {
                _4op_conn_str = std::string(conn_of(HI(t4))) + "/" + conn_of(HI(t4));
            } else if (current_inst == HI(t4)) {
                _4op_conn_str = std::string("~") + conn_of(HI(t4)) + "~/" + conn_of(LO(t4));
            } else {
                _4op_conn_str = std::string(conn_of(HI(t4))) + "/~" + conn_of(LO(t4)) + "~";
            }
            const bool same = HI(t4) == LO(t4);
            if (current_inst == LO(t4)) {
                if (!carrier) {
                    if (!same) {
                        _4op_slot_str = "~1~`\x1B`2`\x1B`3`\x1B`4";
                        _4op_slot_chr = "1";
                    } else {
                        _4op_slot_str = "~1~`\x1B`2`\x1B`~3~`\x1B`4";
                        _4op_slot_chr = "13";
                    }
                } else if (!same) {
                    _4op_slot_str = "1`\x1B`~2~`\x1B`3`\x1B`4";
                    _4op_slot_chr = "2";
                } else {
                    _4op_slot_str = "1`\x1B`~2~`\x1B`3`\x1B`~4~";
                    _4op_slot_chr = "24";
                }
            } else if (!carrier) {
                if (!same) {
                    _4op_slot_str = "1`\x1B`2`\x1B`~3~`\x1B`4";
                    _4op_slot_chr = "3";
                } else {
                    _4op_slot_str = "~1~`\x1B`2`\x1B`~3~`\x1B`4";
                    _4op_slot_chr = "13";
                }
            } else if (!same) {
                _4op_slot_str = "1`\x1B`2`\x1B`3`\x1B`~4~";
                _4op_slot_chr = "4";
            } else {
                _4op_slot_str = "1`\x1B`~2~`\x1B`3`\x1B`~4~";
                _4op_slot_chr = "24";
            }
        }

        const char* const* ext_text = perc ? inst_text_ext_perc : (t4 == 0 ? inst_text_ext_2op : inst_text_ext_4op);
        const auto& ext_hpos = perc ? inst_hpos_ext_perc : (t4 == 0 ? inst_hpos_ext_2op : inst_hpos_ext_4op);
        for (int i = 0; i < 9; ++i) inst_text[9 + i] = ext_text[i];
        std::memcpy(inst_hpos[12], ext_hpos, sizeof(ext_hpos));

        const bool slot_car = _4op_slot_chr.find('2') != std::string::npos || _4op_slot_chr.find('4') != std::string::npos;
        const uint8_t text_col = (!perc && ((t4 == 0 && carrier) || (t4 != 0 && slot_car))) ? instrument_car : instrument_mod;
        for (int temp = 1; temp <= 24; ++temp) {
            ShowC4Str(temp_screen, B(xstart + 1), B(ystart + temp), inst_text[temp], instrument_bckg + instrument_text,
                      instrument_bckg + text_col, instrument_bckg + instrument_glob, instrument_bckg + instrument_hid);
        }

        if (perc) _status_shift_pos = 5;
        else if (t4 == 0) _status_shift_pos = 9;
        else _status_shift_pos = 19;

        const uint8_t ctx_inv = B(instrument_context << 4);
        if (!perc && t4 != 0) {
            if (LO(t4) == HI(t4)) {
                ShowC3Str(temp_screen, xstart, B(ystart + 24),
                          " `[`\xF4" + byte2hex(HI(t4)) + ",\xF5" + byte2hex(HI(t4)) + " " + _4op_conn_str + "`]` ",
                          instrument_bckg + instrument_context, ctx_inv, instrument_bckg + instrument_border);
            } else if (current_inst == HI(t4)) {
                ShowC3Str(temp_screen, xstart, B(ystart + 24),
                          " `[`~\xF4" + byte2hex(HI(t4)) + "~,\xF5" + byte2hex(LO(t4)) + " " + _4op_conn_str + "`]` ",
                          instrument_bckg + instrument_context, ctx_inv, instrument_bckg + instrument_border);
            } else {
                ShowC3Str(temp_screen, xstart, B(ystart + 24),
                          " `[`\xF4" + byte2hex(HI(t4)) + ",~\xF5" + byte2hex(LO(t4)) + "~ " + _4op_conn_str + "`]` ",
                          instrument_bckg + instrument_context, ctx_inv, instrument_bckg + instrument_border);
            }
            _show_conn_scheme(B(xstart + 32), B(ystart + 11), _4op_conn_to_idx(_4op_conn_str), _4op_slot_chr);
            ShowC3Str(temp_screen, B(xstart + 17), B(ystart + 24), "`[`\x04\x03:" + _4op_slot_str + "`]` ",
                      instrument_bckg + instrument_context, ctx_inv, instrument_bckg + instrument_border);
        } else {
            if (!perc) {
                _show_conn_scheme(B(xstart + 32), B(ystart + 11), B(inst->fm_data.FEEDBACK_FM & 1),
                                  std::string(1, static_cast<char>('1' + int(carrier))));
            } else {
                _show_conn_scheme(B(xstart + 32), B(ystart + 11), 6, "1");
            }
            if (inst->perc_voice <= 5) {
                ShowCStr(temp_screen, xstart, B(ystart + 24), std::string(" [~") + perc_voice_str[inst->perc_voice] + "~]",
                         instrument_bckg + instrument_border, instrument_bckg + instrument_context);
            }
            if (perc) {
                ShowCStr(temp_screen, B(xstart + 10), B(ystart + 24), " [~\x01\x03~] ", instrument_bckg + instrument_border,
                         instrument_bckg + instrument_context);
            } else if (!carrier) {
                ShowC3Str(temp_screen, B(xstart + 10), B(ystart + 24), " `[`\x02\x03:~1~`\x1B`2`]`",
                          instrument_bckg + instrument_context, ctx_inv, instrument_bckg + instrument_border);
            } else {
                ShowC3Str(temp_screen, B(xstart + 10), B(ystart + 24), " `[`\x02\x03:1`\x1B`~2~`]`",
                          instrument_bckg + instrument_context, ctx_inv, instrument_bckg + instrument_border);
            }
        }

        const FixedSongData& sd = songdata();
        const RegisterTable& macros = at1(sd.instr_macros, instrum_page);
        std::string temp_str = macros.length != 0 ? " [~MACRO:FM" : " ";
        if (macros.arpeggio_table != 0) temp_str += temp_str != " " ? "+ARP" : "[~MACRO:ARP";
        if (macros.vibrato_table != 0) temp_str += temp_str != " " ? "+ViB" : "[~MACRO:ViB";
        if (temp_str != " ") temp_str += "~] ";

        ShowCStr(temp_screen, B(xstart + 10 + _status_shift_pos), B(ystart + 24), ExpStrR(temp_str, 39, '\xCD'),
                 instrument_bckg + instrument_border, instrument_bckg + instrument_context);

        uint8_t nm_slots;
        if (perc) {
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
        ShowC3Str(temp_screen, B(xstart + 48), B(ystart + 24), ExpStrL(temp_str, 28, '\xCD'),
                  instrument_bckg + instrument_border, instrument_bckg + instrument_context,
                  instrument_bckg + instrument_con_dis);

        if (nm_slots > 1) {
            for (int temp = 1; temp <= nm_slots; ++temp) {
                if (!_operator_enabled[temp]) {
                    ShowStr(temp_screen, B(xstart + 72 - C3StrLen(temp_str) - 1 + temp), B(ystart + 24), "\xFA",
                            instrument_bckg + instrument_border);
                }
            }
        }

        if (inst_hpos[vpos][hpos] == 0) {
            vpos = 1;
            hpos = 1;
        }

        std::string _pan_4op, _feedb_4op, _ftune_4op;
        if (t4 == 0) {
            _pan_4op = "     ";
            _feedb_4op = "    ";
            _ftune_4op = "    ";
        } else {
            _pan_4op = current_inst == HI(t4) ? "(\xF4\x02\x03)" : "(\xF5\x02\x03)";
            _feedb_4op = "[\x04\x03]";
            _ftune_4op = "[\x04\x03]";
        }

        if (!perc) {
            _col_mod = instrument_mod;
            _col_hi_mod = instrument_hi_mod;
        } else {
            _col_mod = instrument_glob;
            _col_hi_mod = instrument_hi_glob;
        }

        const uint8_t col = carrier ? instrument_car : _col_mod;
        const uint8_t col_hi = carrier ? instrument_hi_car : _col_hi_mod;
        const auto label = [&](uint8_t x, uint8_t y, const std::string& s) {
            ShowCStr(temp_screen, B(xstart + x), B(ystart + y), s, instrument_bckg + col, instrument_bckg + col_hi);
        };
        const auto label_glob = [&](uint8_t x, uint8_t y, const std::string& s) {
            ShowCStr(temp_screen, B(xstart + x), B(ystart + y), s, instrument_bckg + instrument_glob,
                     instrument_bckg + instrument_hi_glob);
        };

        if (!(vpos == 1 && hpos == 1)) label(1, 1, "~A~TTACK RATE");
        if (!(vpos == 2 && hpos == 1)) label(1, 2, "~D~ECAY RATE");
        if (!(in_set(vpos, { 3, 6, 8, 9 }) && hpos == 1)) label(51, 1, "~W~AVEFORM TYPE");
        if (!(vpos == 4 && hpos == 1)) label(1, 4, "~S~USTAiN LEVEL");
        if (!(vpos == 5 && hpos == 1)) label(1, 5, "~R~ELEASE RATE");
        if (!(vpos == 7 && hpos == 1)) label(1, 7, "~O~UTPUT LEVEL");
        if (!(vpos >= 10 && vpos <= 13 && hpos == 1)) label(1, 9, "~K~EY SCALiNG LEVEL");

        if (!perc) {
            if (!((in_set(vpos, { 12, 13 }) && hpos == 2) || (in_set(vpos, { 14, 15 }) && hpos == 1) ||
                  (vpos == 17 && hpos == 4))) {
                label_glob(21, 11, "~C~ONNECTiON");
            }
            if (!(vpos == 12 && hpos == 3)) label_glob(51, 11, "~F~EEDBACK " + _feedb_4op);
        }

        if (!(vpos == 16 && hpos == 2)) label_glob(51, 15, "F-~T~UNE " + _ftune_4op);
        if (!(vpos == 17 && hpos >= 1 && hpos <= 3)) label(1, 15, "~P~ANNiNG " + _pan_4op);
        if (!(vpos >= 18 && vpos <= 21 && hpos == 1)) label(1, 19, "~E~NVELOPE TYPE");
        if (!(vpos >= 18 && vpos <= 20 && hpos > 1)) label(28, 19, "FREQUENCY DATA ~M~ULTiPLiER");

        const auto hi = [&](uint8_t x, uint8_t y, const std::string& s) {
            ShowStr(temp_screen, B(xstart + x), B(ystart + y), s, instrument_bckg + col_hi);
        };
        const auto hi_glob = [&](uint8_t x, uint8_t y, const std::string& s) {
            ShowStr(temp_screen, B(xstart + x), B(ystart + y), s, instrument_bckg + instrument_hi_glob);
        };

        if (hpos == 1) {
            switch (vpos) {
            case 1: hi(1, 1, "ATTACK RATE"); break;
            case 2: hi(1, 2, "DECAY RATE"); break;
            case 3: case 6: case 8: case 9: hi(51, 1, "WAVEFORM TYPE"); break;
            case 4: hi(1, 4, "SUSTAiN LEVEL"); break;
            case 5: hi(1, 5, "RELEASE RATE"); break;
            case 7: hi(1, 7, "OUTPUT LEVEL"); break;
            case 10: case 11: case 12: case 13: case 14: case 15: case 16: case 17:
                if (vpos >= 10 && vpos <= 13 && hpos == 1) hi(1, 9, "KEY SCALiNG LEVEL");
                else if (vpos != 17) hi_glob(21, 11, "CONNECTiON");
                else hi(1, 15, "PANNiNG " + _pan_4op);
                break;
            case 18: case 19: case 20: case 21: hi(1, 19, "ENVELOPE TYPE"); break;
            }
        } else if (hpos >= 2 && hpos <= 7) {
            switch (vpos) {
            case 2: case 4: case 5: case 7: hi(51, 1, "WAVEFORM TYPE"); break;
            case 12: case 13: case 14: case 15: case 16: case 17:
                if (vpos == 12 && hpos == 3) hi_glob(51, 11, "FEEDBACK " + _feedb_4op);
                else if (vpos == 16 && hpos == 2) hi_glob(51, 15, "F-TUNE " + _ftune_4op);
                else if (vpos == 17 && hpos >= 1 && hpos <= 3) hi(1, 15, "PANNiNG " + _pan_4op);
                else hi_glob(21, 11, "CONNECTiON");
                break;
            case 18: case 19: case 20: case 21: hi(28, 19, "FREQUENCY DATA MULTiPLiER"); break;
            }
        }
    }

    // 値の表示と ADSR のプレビュー
    void _page_refresh()
    {
        const uint8_t x0 = xstart;
        const uint8_t y0 = ystart;
        const uint16_t t4 = get_4op_to_test();
        const bool perc = is_perc_2_5(inst);
        const bool adsr_pos = in_set(vpos, { 1, 2, 4, 5, 7, 21 }) && hpos == 1;

        uint8_t adsr_vpos;
        if (is_default_screen_mode()) {
            adsr_vpos = B(y0 + 9);
            adsr_bckg = instrument_adsr;
        } else {
            adsr_vpos = B(y0 + 24);
            adsr_bckg = instrument_bckg;
        }

        if (!adsr_pos || !_ADSR_preview_flag) {
            _show_adsr(temp_screen, B(x0 + 3), adsr_vpos, BYTE_NULL, BYTE_NULL, BYTE_NULL, BYTE_NULL, BYTE_NULL, BYTE_NULL,
                       adsr_bckg, adsr_bckg, true);
            _page_build();
        }

        const auto X = [&](int v, int h, int d = 0) { return B(x0 + inst_hpos[v][h] + d); };
        const auto Y = [&](int v, int d = 0) { return B(y0 + inst_vpos[v] + d); };
        const auto slider = [&](int v, int value, int width) {
            ShowCStr(temp_screen, X(v, 1), Y(v), "~" + ExpStrL("", B(value), '\x0E') + "~" + ExpStrL("", B(width - value), '\x0E'),
                     instrument_bckg + instrument_text, instrument_bckg + instrument_hi_text);
        };
        const auto number = [&](int v, int dx, const std::string& s) {
            ShowStr(temp_screen, X(v, 1, dx), Y(v), s, instrument_bckg + instrument_text);
        };

        uint8_t temp = carrier ? inst->fm_data.ATTCK_DEC_carrier : inst->fm_data.ATTCK_DEC_modulator;
        slider(1, temp >> 4, 15);
        slider(2, temp & 0x0f, 15);
        number(1, 16, ExpStrR(Num2str(temp >> 4, 16), 1, ' '));
        number(2, 16, ExpStrR(Num2str(temp & 0x0f, 16), 1, ' '));

        temp = carrier ? inst->fm_data.SUSTN_REL_carrier : inst->fm_data.SUSTN_REL_modulator;
        slider(4, temp >> 4, 15);
        slider(5, temp & 0x0f, 15);
        number(4, 16, ExpStrR(Num2str(temp >> 4, 16), 1, ' '));
        number(5, 16, ExpStrR(Num2str(temp & 0x0f, 16), 1, ' '));

        temp = carrier ? inst->fm_data.WAVEFORM_carrier : inst->fm_data.WAVEFORM_modulator;
        ShowVStr(temp_screen, X(2, 2), Y(2), ExpStrL("", B(temp & 7), ' ') + "\x0B" + ExpStrL("", B(7 - (temp & 7)), ' '),
                 instrument_bckg + instrument_hi_text);

        temp = carrier ? inst->fm_data.KSL_VOLUM_carrier : inst->fm_data.KSL_VOLUM_modulator;
        slider(7, (63 - (temp & 0x3f)) / 2, 31);
        number(7, 32, ExpStrR(Num2str(63 - (temp & 0x3f), 16), 2, ' '));

        temp = B(temp >> 6);
        temp = B((temp >> 1) + ((temp & 1) << 1));
        ShowVStr(temp_screen, X(10, 1), Y(10), ExpStrL("", temp, ' ') + "\x0B" + ExpStrL("", B(3 - temp), ' '),
                 instrument_bckg + instrument_hi_text);

        // オリジナルは panning が 3 以上でも ins_pan_str1 を読む。ここでは表示しない
        if (inst->panning <= 2) {
            ShowCStr(temp_screen, X(17, 1), Y(17), ins_pan_str1[inst->panning], instrument_bckg + instrument_text,
                     instrument_bckg + instrument_hi_text);
        }

        if (!perc) {
            temp = inst->fm_data.FEEDBACK_FM;
            const int fb = int(pas_round(_get_feedback_val() * (17 / 7.0)));
            ShowCStr(temp_screen, X(12, 3), Y(12), "~" + ExpStrL("", B(fb), '\x0E') + "~" + ExpStrL("", B(17 - fb), '\x0E'),
                     instrument_bckg + instrument_text, instrument_bckg + instrument_hi_text);
            ShowStr(temp_screen, X(12, 3, 16), Y(12, -1), ExpStrR(Num2str(_get_feedback_val(), 16), 2, ' '),
                    instrument_bckg + instrument_text);

            _curr_connection_str = ExpStrR("", 6, ' ');
            if (t4 == 0) {
                if ((temp & 1) == 0) _curr_connection_str[0] = '\x0B';
                else _curr_connection_str[1] = '\x0B';
            } else {
                _curr_connection_str[_4op_conn_to_idx(_4op_conn_str)] = '\x0B';
            }

            if (t4 == 0) {
                ShowVStr(temp_screen, X(12, 2), Y(12), _curr_connection_str, instrument_bckg + instrument_hi_text);
            } else {
                ShowVStr(temp_screen, X(14, 1), Y(12), _curr_connection_str, instrument_bckg + instrument_hi_text);
            }
        }

        const int8_t ftune = _get_finetune_val();
        temp = ftune >= 0 ? B(ftune) : B(0 - ftune);
        std::string temps1 = ExpStrL("", 17, '\x0E');
        std::string temps2 = (vpos == 16 && hpos == 2) ? "\x0F~\x0F~\x0F" : "\x0F`\x0F`\x0F";

        const int knob = ftune > 0 ? 1 + 7 + temp / 18 : 1 + 7 - temp / 18;
        temps1.erase(knob - 1, 3);
        temps1.insert(knob - 1, temps2);

        if (ftune > 0) temps2 = "+";
        else if (ftune < 0) temps2 = "-";
        else temps2 = "";

        ShowStr(temp_screen, X(16, 2, 14), Y(16, -1), ExpStrL(temps2 + ExpStrL(Num2str(temp, 16), 2, '0'), 3, ' '),
                instrument_bckg + instrument_text);
        ShowC3Str(temp_screen, X(16, 2), Y(16), temps1, instrument_bckg + instrument_text, instrument_bckg + instrument_hi_text,
                  instrument_bckg + instrument_glob);

        temp = carrier ? inst->fm_data.AM_VIB_EG_carrier : inst->fm_data.AM_VIB_EG_modulator;
        temps1 = (temp >> 7) == 0 ? " " : "\xFB";
        temps1 += ((temp >> 6) & 1) == 0 ? " " : "\xFB";
        temps1 += ((temp >> 4) & 1) == 0 ? " " : "\xFB";
        temps1 += ((temp >> 5) & 1) == 0 ? " " : "\xFB";
        ShowVStr(temp_screen, X(18, 1), Y(18), temps1, instrument_bckg + instrument_hi_text);

        temps1 = ExpStrL("", 16, ' ');
        temps1[temp & 0x0f] = '.';
        if (vpos >= 19 && vpos <= 21 && hpos >= 2 && hpos <= 7) {
            ShowStr(temp_screen, X(21, 2, -1), Y(19, -1), ExpStrR(inst_mult_hint[(vpos - 19) * 6 + hpos - 2], 34, ' '),
                    instrument_bckg + instrument_hi_text);
        } else {
            ShowStr(temp_screen, X(21, 2, -1), Y(19, -1), ExpStrR(inst_mult_hint[temp & 0x0f], 34, ' '),
                    instrument_bckg + instrument_hi_text);
        }

        const auto radio = [&](int v, int count, int first) {
            for (int i = 1; i <= count; ++i) {
                ShowStr(temp_screen, X(v, 2 + i - 1), Y(v), temps1[first + i - 1] == ' ' ? " " : "\x0B",
                        instrument_bckg + instrument_hi_text);
            }
        };
        radio(19, 6, 0);
        radio(20, 6, 6);
        radio(21, 4, 12);

        if (!is_default_screen_mode() || (adsr_pos && _ADSR_preview_flag)) {
            const FmInstData& fm = inst->fm_data;
            if (!carrier || perc) {
                _show_adsr(temp_screen, B(x0 + 3), adsr_vpos, B(fm.ATTCK_DEC_modulator >> 4), B(fm.ATTCK_DEC_modulator & 0x0f),
                           B(fm.SUSTN_REL_modulator >> 4), B(fm.SUSTN_REL_modulator & 0x0f), B(fm.KSL_VOLUM_modulator & 0x3f),
                           B((fm.AM_VIB_EG_modulator >> 5) & 1), B(adsr_bckg + _col_mod), B(adsr_bckg + _col_hi_mod), false);
            } else {
                _show_adsr(temp_screen, B(x0 + 3), adsr_vpos, B(fm.ATTCK_DEC_carrier >> 4), B(fm.ATTCK_DEC_carrier & 0x0f),
                           B(fm.SUSTN_REL_carrier >> 4), B(fm.SUSTN_REL_carrier & 0x0f), B(fm.KSL_VOLUM_carrier & 0x3f),
                           B((fm.AM_VIB_EG_carrier >> 5) & 1), B(adsr_bckg + instrument_car), B(adsr_bckg + instrument_hi_car),
                           false);
            }
        }

        move2screen_alt();
    }

    // パンの位置へ
    void goto_panning()
    {
        switch (inst->panning) {
        case 1: vpos = 17; hpos = 1; break;
        case 0: vpos = 17; hpos = 2; break;
        case 2: vpos = 17; hpos = 3; break;
        }
    }

    // 接続の位置へ (last_hpos: 6 番目 (AM/AM) の hpos。オリジナルは場所によって 3 と 4 がある)
    void goto_connection(uint8_t last_hpos)
    {
        switch (_curr_connection_str.find('\x0B') + 1) {
        case 1: vpos = 12; hpos = 2; break;
        case 2: vpos = 13; hpos = 2; break;
        case 3: vpos = 14; hpos = 1; break;
        case 4: vpos = 15; hpos = 1; break;
        case 5: vpos = 16; hpos = 1; break;
        case 6: vpos = 17; hpos = last_hpos; break;
        }
    }

    uint8_t waveform_of(const Instrument& ins) { return carrier ? ins.fm_data.WAVEFORM_carrier : ins.fm_data.WAVEFORM_modulator; }

    // KSL の位置 (0〜3)
    uint8_t ksl_index(const Instrument& ins)
    {
        uint8_t temp = carrier ? ins.fm_data.KSL_VOLUM_carrier : ins.fm_data.KSL_VOLUM_modulator;
        temp = B(temp >> 6);
        return B((temp >> 1) + ((temp & 1) << 1));
    }

    uint8_t mult_of(const Instrument& ins)
    {
        return B((carrier ? ins.fm_data.AM_VIB_EG_carrier : ins.fm_data.AM_VIB_EG_modulator) & 0x0f);
    }

    // カーソルの位置を、選んでいる値に合わせる
    void _sync_radio_buttons()
    {
        const Instrument& cur = instr(current_inst);
        if ((in_set(vpos, { 1, 2, 4, 5, 7 }) && hpos == 2) || in_set(vpos, { 3, 6, 8, 9 })) {   // sync WAVEFORM
            vpos = B(2 + waveform_of(cur));
            if (in_set(vpos, { 1, 2, 4, 5, 7 })) hpos = 2;
            else if (in_set(vpos, { 3, 6, 8, 9 })) hpos = 1;
        }

        if (vpos >= 10 && vpos <= 13 && hpos == 1) {   // sync SCALING LEVEL
            vpos = B(10 + ksl_index(cur));
            hpos = 1;
        }

        if (vpos == 17 && hpos >= 1 && hpos <= 3) goto_panning();   // sync PANNING

        if ((in_set(vpos, { 12, 13 }) && hpos == 2) || (in_set(vpos, { 14, 15, 16 }) && hpos == 1) ||
            (vpos == 17 && hpos == 4)) {
            goto_connection(3);   // sync CONNECTION
        }

        if (vpos >= 19 && vpos <= 21 && hpos > 1) {   // sync FREQ. DATA MULT.
            const uint8_t temp = mult_of(*inst);
            hpos = B(2 + temp % 6);
            vpos = B(19 + temp / 6);
        }
    }

    int min0(int number) { return number > 0 ? number : 0; }

    int _ftune(int ftune)
    {
        const int temp = (ftune / 18 == 0 && ftune != 0) ? 0 : std::abs(ftune) / 18;
        return (ftune / min_(std::abs(ftune), 1)) * temp;
    }

    uint8_t _check_hpos_idx_l(uint8_t v)
    {
        for (int idx = 1; idx <= inst_hpos_max; ++idx) {
            if (inst_hpos[v][idx] != 0) return B(idx);
        }
        return 0;
    }

    uint8_t _check_hpos_idx_r(uint8_t v)
    {
        for (int idx = inst_hpos_max; idx >= 1; --idx) {
            if (inst_hpos[v][idx] != 0) return B(idx);
        }
        return 0;
    }

    // Alt+1〜4 (Shift で切り替え): 試聴で鳴らすオペレータ
    void _set_operator_flag(uint8_t op, bool toggle)
    {
        const uint16_t t4 = get_4op_to_test();
        if (is_perc_2_5(inst) || (t4 == 0 && !(op >= 1 && op <= 2)) || !(op >= 1 && op <= 4)) return;

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

    std::string bank_key(const char* suffix) { return "?instrument_editor?" + byte2hex(instrum_page) + suffix; }

    // 4OP の組のもう一方へ移る
    void switch_4op_slot()
    {
        const uint16_t t4 = get_4op_to_test();
        current_inst = current_inst == LO(t4) ? HI(t4) : LO(t4);
        instrum_page = current_inst;
        STATUS_LINE_refresh();
        inst = &instr(instrum_page);
        _page_build();
        _page_refresh();
    }

    // Ctrl+[ / Ctrl+]: 前後の楽器へ
    void select_instrument(uint8_t ins)
    {
        current_inst = ins;
        reset_marked_instruments();
        instrum_page = current_inst;
        STATUS_LINE_refresh();
        inst = &instr(instrum_page);
        carrier = !is_perc_2_5(inst);
        std::fill(std::begin(_operator_enabled), std::end(_operator_enabled), true);
        _page_build();
        _page_refresh();
        _sync_radio_buttons();
    }
}

// TAB (楽器窓から): 楽器エディタ
void INSTRUMENT_CONTROL_edit()
{
    using namespace instedit;
    static const uint8_t KSL_volume[4] = { 0, 0x80, 0x40, 0xc0 };
    static const uint8_t AM_Vibrato_EG[4] = { 0x80, 0x40, 0x10, 0x20 };
    static const uint8_t panning_pos[3] = { 1, 0, 2 };

    {
        PlayerLock lock;
        inst = &instr(instrum_page);
        std::fill(std::begin(_operator_enabled), std::end(_operator_enabled), true);
        if (!is_perc_2_5(inst)) carrier = remember_ins_pos ? B(get_bank_position(bank_key("?carrier"), -1)) != 2 : true;
        else carrier = false;

        if (!remember_ins_pos) {
            hpos = 1;
            vpos = 1;
        } else if (carrier) {
            hpos = B(min_(get_bank_position(bank_key("?carrier?hpos"), -1), 1));
            vpos = B(min_(get_bank_position(bank_key("?carrier?vpos"), -1), 1));
        } else {
            hpos = B(min_(get_bank_position(bank_key("?modulator?hpos"), -1), 1));
            vpos = B(min_(get_bank_position(bank_key("?modulator?vpos"), -1), 1));
        }
    }

    // 画面を元に戻す (F キーなどで他の画面へ移るときと、閉じるとき)
    const auto restore_screen = []() {
        --xstart;
        HideCursor();
        move_to_screen_data = screen_backup;
        move_to_screen_area[1] = xstart;
        move_to_screen_area[2] = ystart;
        move_to_screen_area[3] = B(xstart + 71 + 2);
        move_to_screen_area[4] = B(ystart + 24 + INSEDIT_yshift + 1);
        move2screen();
    };

    for (;;) {   // _jmp1
        if (_force_program_quit) return;

        ScreenMemCopy(screen_ptr, screen_backup);
        HideCursor();

        ScreenMemCopy(screen_ptr, temp_screen);
        centered_frame_vdest = temp_screen;
        centered_frame(xstart, ystart, 71, B(24 + INSEDIT_yshift), " iNSTRUMENT EDiTOR (iNS_  ) ",
                       instrument_bckg + instrument_border, instrument_bckg + instrument_title, frame_double);
        centered_frame_vdest = screen_ptr;

        move_to_screen_data = temp_screen;
        move_to_screen_area[1] = xstart;
        move_to_screen_area[2] = ystart;
        move_to_screen_area[3] = B(xstart + 71 + 2);
        move_to_screen_area[4] = B(ystart + 24 + INSEDIT_yshift + 1);
        move2screen_alt();

        move_to_screen_area[1] = B(xstart + 1);
        move_to_screen_area[2] = ystart;
        move_to_screen_area[3] = B(xstart + 70);
        move_to_screen_area[4] = B(ystart + 24 + INSEDIT_yshift);
        ++xstart;

        {
            PlayerLock lock;
            _page_build();
            _sync_radio_buttons();
        }

        uint16_t leave_key = 0;   // 他の画面へ移るキー (0: ESC で閉じる)
        bool done = false;
        while (!done && !_force_program_quit) {
            keyboard_poll_input();
            {
                PlayerLock lock;
                _page_build();
                _page_refresh();
                if ((in_set(vpos, { 1, 2, 4, 5, 7 }) && hpos == 1) || (vpos == 12 && hpos == 3) || (vpos == 16 && hpos == 2)) {
                    const FmInstData& fm = instr(current_inst).fm_data;
                    const uint8_t x = B(xstart + inst_hpos[vpos][hpos]);
                    const uint8_t y = B(ystart + inst_vpos[vpos]);
                    switch (vpos) {
                    case 1: GotoXY(B(x + min0((carrier ? fm.ATTCK_DEC_carrier : fm.ATTCK_DEC_modulator) / 16 - 1)), y); break;
                    case 2: GotoXY(B(x + min0((carrier ? fm.ATTCK_DEC_carrier : fm.ATTCK_DEC_modulator) % 16 - 1)), y); break;
                    case 4: GotoXY(B(x + min0((carrier ? fm.SUSTN_REL_carrier : fm.SUSTN_REL_modulator) / 16 - 1)), y); break;
                    case 5: GotoXY(B(x + min0((carrier ? fm.SUSTN_REL_carrier : fm.SUSTN_REL_modulator) % 16 - 1)), y); break;
                    case 7:
                        GotoXY(B(x + min0((63 - ((carrier ? fm.KSL_VOLUM_carrier : fm.KSL_VOLUM_modulator) & 0x3f)) / 2 - 1)), y);
                        break;
                    case 12: GotoXY(B(x + min0(int(pas_round(_get_feedback_val() * (17 / 7.0))) - 1)), y); break;
                    case 16: {
                        const int ft = _get_finetune_val();
                        if (ft > 0) GotoXY(B(x + 7 + _ftune(ft) + 1), y);
                        else if (ft < 0) GotoXY(B(x + 1 + 7 + _ftune(ft) - 1), y);
                        else GotoXY(B(x + 1 + 7 + _ftune(ft)), y);
                        break;
                    }
                    }
                    SetCursorShape(0x010c);
                } else {
                    if (use_large_cursor) WideCursor();
                    else ThinCursor();
                    GotoXY(B(xstart + inst_hpos[vpos][hpos]), B(ystart + inst_vpos[vpos]));
                }
            }

            if (!keypressed()) {
                draw_screen();
                continue;
            }
            const uint16_t fkey = getkey();
            if (_force_program_quit) break;

            PlayerLock lock;
            FixedSongData& sd = songdata();
            Instrument& in = *inst;
            FmInstData& fm = in.fm_data;
            const uint16_t t4 = get_4op_to_test();
            bool nope = false;

            if (!shift_pressed() && !alt_pressed() && !ctrl_pressed()) {
                if (t4 != 0) INSTRUMENT_test(LO(t4), HI(t4), count_channel(pattern_hpos), fkey, true);
                else INSTRUMENT_test(instrum_page, 0, count_channel(pattern_hpos), fkey, true);
            }

            switch (fkey) {
            case kAlt0: std::fill(std::begin(_operator_enabled), std::end(_operator_enabled), true); break;
            case kAlt1: _set_operator_flag(1, shift_pressed()); break;
            case kAlt2: _set_operator_flag(2, shift_pressed()); break;
            case kAlt3: _set_operator_flag(3, shift_pressed()); break;
            case kAlt4: _set_operator_flag(4, shift_pressed()); break;

            case kAltA: hpos = 1; vpos = 1; break;
            case kAltD: hpos = 1; vpos = 2; break;
            case kAltS: hpos = 1; vpos = 4; break;
            case kAltR: hpos = 1; vpos = 5; break;
            case kAltO: hpos = 1; vpos = 7; break;

            case kAltW:
                vpos = B(2 + waveform_of(in));
                hpos = in_set(vpos, { 2, 4, 5, 7 }) ? 2 : 1;
                break;

            case kAltK:
                vpos = B(10 + ksl_index(in));
                hpos = 1;
                break;

            case kAltP: goto_panning(); break;

            case kAltC:
                if (!is_perc_2_5(inst)) goto_connection(4);
                break;

            case kAltF:
                if (!is_perc_2_5(inst)) {
                    vpos = 12;
                    hpos = 3;
                }
                break;

            case kAltT: vpos = 16; hpos = 2; break;
            case kAltE: vpos = 18; hpos = 1; break;

            case kAltM: {
                const uint8_t temp = mult_of(in);
                hpos = B(2 + temp % 6);
                vpos = B(19 + temp / 6);
                break;
            }

            case kLEFT:
                if (hpos - 1 >= 1 && inst_hpos[vpos][hpos - 1] != 0) {
                    --hpos;
                } else if (hpos - 2 >= 1 && inst_hpos[vpos][hpos - 2] != 0) {
                    hpos = B(hpos - 2);
                } else if (vpos > 1) {
                    do --vpos; while (_check_hpos_idx_r(vpos) == 0);
                    hpos = _check_hpos_idx_r(vpos);
                } else {
                    vpos = inst_vpos_max;
                    hpos = inst_hpos_max;
                    while (hpos > 1 && inst_hpos[vpos][hpos] == 0) --hpos;
                }
                break;

            case kRIGHT:
                if (hpos + 1 <= inst_hpos_max && inst_hpos[vpos][hpos + 1] != 0) {
                    ++hpos;
                } else if (hpos + 2 <= inst_hpos_max && inst_hpos[vpos][hpos + 2] != 0) {
                    hpos = B(hpos + 2);
                } else if (vpos < inst_vpos_max) {
                    do ++vpos; while (_check_hpos_idx_l(vpos) == 0);
                    hpos = _check_hpos_idx_l(vpos);
                } else {
                    vpos = 1;
                    hpos = 1;
                }
                break;

            case kUP: {
                if (vpos > 1) {
                    do --vpos; while (inst_hpos[vpos][1] == 0);
                } else {
                    vpos = inst_vpos_max;
                }
                const uint8_t temp = hpos;
                hpos = inst_hpos_max;
                while (hpos > 1 && (inst_hpos[vpos][hpos] == 0 || inst_hpos[vpos][hpos] > inst_hpos[vpos + 1][temp])) --hpos;
                break;
            }

            case kDOWN: {
                if (vpos < inst_vpos_max) {
                    do ++vpos; while (inst_hpos[vpos][1] == 0);
                } else {
                    vpos = 1;
                }
                const uint8_t temp = hpos;
                hpos = inst_hpos_max;
                while (hpos > 1 && (inst_hpos[vpos][hpos] == 0 || inst_hpos[vpos][hpos] > inst_hpos[vpos - 1][temp])) --hpos;
                break;
            }

            case kHOME: vpos = 1; hpos = 1; break;

            case kEND:
                vpos = inst_vpos_max;
                hpos = inst_hpos_max;
                while (hpos > 1 && inst_hpos[vpos][hpos] == 0) --hpos;
                break;

            case kTAB:
                switch (vpos) {
                case 1: vpos = 2; hpos = 1; break;   // ATTACK RATE -> DECAY RATE
                case 2:
                    if (hpos == 1) { vpos = 4; hpos = 1; }   // DECAY RATE -> SUSTAIN LEVEL
                    else { vpos = 7; hpos = 1; }   // WAVEFORM TYPE -> OUTPUT LEVEL
                    break;
                case 3: vpos = 7; hpos = 1; break;   // WAVEFORM TYPE -> OUTPUT LEVEL
                case 4:
                    if (hpos == 1) { vpos = 5; hpos = 1; }   // SUSTAIN LEVEL -> RELEASE RATE
                    else { vpos = 7; hpos = 1; }   // WAVEFORM TYPE -> OUTPUT LEVEL
                    break;
                case 5:
                    if (hpos == 1) {   // RELEASE RATE -> WAVEFORM TYPE
                        vpos = B(2 + waveform_of(in));
                        hpos = in_set(vpos, { 2, 4, 5, 7 }) ? 2 : 1;
                    } else {
                        vpos = 7; hpos = 1;   // WAVEFORM TYPE -> OUTPUT LEVEL
                    }
                    break;
                case 6: vpos = 7; hpos = 1; break;   // WAVEFORM TYPE -> OUTPUT LEVEL
                case 7:
                    if (hpos == 1) {   // OUTPUT LEVEL -> KEY SCALiNG LEVEL
                        vpos = B(10 + ksl_index(in));
                        hpos = 1;
                    } else {
                        vpos = 7; hpos = 1;   // WAVEFORM TYPE -> OUTPUT LEVEL
                    }
                    break;
                case 8: case 9: vpos = 7; hpos = 1; break;   // WAVEFORM TYPE -> OUTPUT LEVEL
                case 10: case 11: case 12: case 13:
                    if (hpos == 1) goto_panning();   // KEY SCALING LEVEL -> PANNING
                    else if (in_set(vpos, { 12, 13 }) && hpos == 2) { vpos = 12; hpos = 3; }   // CONNECTION -> FEEDBACK
                    else { vpos = 16; hpos = 2; }   // FEEDBACK -> F-TUNE
                    break;
                case 14: case 15: case 16:
                    if (hpos == 1) { vpos = 12; hpos = 3; }   // CONNECTION -> FEEDBACK
                    else { vpos = 18; hpos = 1; }   // F-TUNE -> AM/VIB/EG/KSR
                    break;
                case 17:
                    if (hpos >= 1 && hpos <= 3) {
                        if (!is_perc_2_5(inst)) goto_connection(3);   // PANNING -> CONNECTION
                        else { vpos = 16; hpos = 2; }   // PERC:SD/TT/TC/HH == PANNING -> F-TUNE
                    } else {
                        vpos = 12; hpos = 3;   // CONNECTION -> FEEDBACK
                    }
                    break;
                case 18: case 19: case 20: case 21:
                    if (vpos == 18 || hpos == 1) {   // AM/VIB/EG/KSR -> FREQ. DATA MULT.
                        const uint8_t temp = mult_of(in);
                        hpos = B(2 + temp % 6);
                        vpos = B(19 + temp / 6);
                    } else {   // *NEXT SLOT*
                        vpos = 1;
                        hpos = 1;
                        if (!is_perc_2_5(inst)) {
                            carrier = !carrier;
                            if (carrier && t4 != 0) switch_4op_slot();
                        }
                    }
                    break;
                }
                break;

            case kShTAB:
                switch (vpos) {
                case 1: {   // *PREVIOUS SLOT*
                    if (!is_perc_2_5(inst)) {
                        carrier = !carrier;
                        if (!carrier && t4 != 0) switch_4op_slot();
                    }
                    const uint8_t temp = mult_of(*inst);
                    hpos = B(2 + temp % 6);
                    vpos = B(19 + temp / 6);
                    break;
                }
                case 2:
                    if (hpos == 1) { vpos = 1; hpos = 1; }   // DECAY RATE -> ATTACK RATE
                    else { vpos = 5; hpos = 1; }   // WAVEFORM TYPE -> RELEASE RATE
                    break;
                case 3: vpos = 5; hpos = 1; break;   // WAVEFORM TYPE -> RELEASE RATE
                case 4:
                    if (hpos == 1) { vpos = 2; hpos = 1; }
                    else { vpos = 5; hpos = 1; }   // WAVEFORM TYPE -> RELEASE RATE
                    break;
                case 5:
                    if (hpos == 1) { vpos = 4; hpos = 1; }
                    else { vpos = 5; hpos = 1; }   // WAVEFORM TYPE -> RELEASE RATE
                    break;
                case 6: vpos = 5; hpos = 1; break;   // WAVEFORM TYPE -> RELEASE RATE
                case 7:
                    if (hpos == 1) {   // OUTPUT LEVEL -> WAVEFORM TYPE
                        vpos = B(2 + waveform_of(in));
                        hpos = in_set(vpos, { 2, 4, 5, 7 }) ? 2 : 1;
                    } else {
                        vpos = 5; hpos = 1;   // WAVEFORM TYPE -> RELEASE RATE
                    }
                    break;
                case 8: case 9: vpos = 5; hpos = 1; break;   // WAVEFORM TYPE -> RELEASE RATE
                case 10: case 11: case 12: case 13:
                    if (hpos == 1) { vpos = 7; hpos = 1; }   // KEY SCALING RATE -> OUTPUT LEVEL
                    else if (hpos == 2) goto_panning();   // CONNECTION -> PANNING
                    else goto_connection(3);   // FEEDBACK -> CONNECTION
                    break;
                case 14: case 15: case 16:
                    if (hpos == 1) goto_panning();   // CONNECTION -> PANNING
                    else if (!is_perc_2_5(inst)) { vpos = 12; hpos = 3; }   // F-TUNE -> FEEDBACK
                    else goto_panning();   // PERC:SD/TT/TC/HH == F-TUNE -> PANNING
                    break;
                case 17:
                    if (hpos >= 1 && hpos <= 3) {   // PANNING -> KEY SCALING LEVEL
                        vpos = B(10 + ksl_index(in));
                        hpos = 1;
                    } else {
                        goto_panning();   // CONNECTION -> PANNING
                    }
                    break;
                case 18: case 19: case 20: case 21:
                    if (vpos == 18 || hpos == 1) { vpos = 16; hpos = 2; }   // AM/VIB/EG/KSR -> F-TUNE
                    else { vpos = 18; hpos = 1; }   // FREQ. DATA MULT. -> AM/VIB/EG/KSR
                    break;
                }
                break;

            case kSPACE:
                if (ctrl_pressed()) {
                    _ADSR_preview_flag = !_ADSR_preview_flag;
                } else {
                    uint8_t& wave = carrier ? fm.WAVEFORM_carrier : fm.WAVEFORM_modulator;
                    switch (vpos) {
                    case 2: if (hpos == 2) wave = 0; break;
                    case 3: wave = 1; break;
                    case 4: if (hpos == 2) wave = 2; break;
                    case 5: if (hpos == 2) wave = 3; break;
                    case 6: wave = 4; break;
                    case 7: if (hpos == 2) wave = 5; break;
                    case 8: wave = 6; break;
                    case 9: wave = 7; break;
                    case 10: case 11: case 12: case 13: case 14: case 15: case 16: case 17:
                        if (vpos >= 10 && vpos <= 13 && hpos == 1) {
                            uint8_t& ksl = carrier ? fm.KSL_VOLUM_carrier : fm.KSL_VOLUM_modulator;
                            ksl = B((ksl & 0x3f) + KSL_volume[vpos - 10]);
                        } else if ((in_set(vpos, { 12, 13 }) && hpos == 2) || (in_set(vpos, { 14, 15, 16 }) && hpos == 1) ||
                                   (vpos == 17 && hpos == 4)) {
                            // 4OP: 14..17 = FM/FM, FM/AM, AM/FM, AM/AM (1 つ目 / 2 つ目)
                            const auto set_conn = [&](uint8_t hi_conn, uint8_t lo_conn) {
                                uint8_t& fb_hi = instr(HI(t4)).fm_data.FEEDBACK_FM;
                                fb_hi = B(hi_conn + (fb_hi & 0x0e));
                                uint8_t& fb_lo = instr(LO(t4)).fm_data.FEEDBACK_FM;
                                fb_lo = B(lo_conn + (fb_lo & 0x0e));
                            };
                            switch (vpos) {
                            case 12: case 13:
                                if (t4 == 0) fm.FEEDBACK_FM = B(vpos - 12 + (fm.FEEDBACK_FM & 0x0e));
                                break;
                            case 14: if (t4 != 0) set_conn(0, 0); break;
                            case 15: if (t4 != 0) set_conn(0, 1); break;
                            case 16: if (t4 != 0) set_conn(1, 0); break;
                            case 17: if (t4 != 0) set_conn(1, 1); break;
                            }
                        } else if (vpos == 17 && hpos >= 1 && hpos <= 3) {
                            in.panning = panning_pos[hpos - 1];
                        }
                        break;
                    case 18: case 19: case 20: case 21: {
                        uint8_t& am = carrier ? fm.AM_VIB_EG_carrier : fm.AM_VIB_EG_modulator;
                        if (vpos == 18 || hpos == 1) am = B(am ^ AM_Vibrato_EG[vpos - 18]);
                        else am = B((am & 0xf0) + (vpos - 19) * 6 + hpos - 2);
                        break;
                    }
                    }
                    reset_instrument_preview();
                }
                break;

            case kPgUP:
            case kCHplus:
            case kNPplus: {
                uint8_t& ad = carrier ? fm.ATTCK_DEC_carrier : fm.ATTCK_DEC_modulator;
                uint8_t& sr = carrier ? fm.SUSTN_REL_carrier : fm.SUSTN_REL_modulator;
                uint8_t& kv = carrier ? fm.KSL_VOLUM_carrier : fm.KSL_VOLUM_modulator;
                if (vpos == 1 && (ad >> 4) < 0x0f) ad = B((ad & 0x0f) + (((ad >> 4) + 1) << 4));
                if (vpos == 2 && hpos == 1 && (ad & 0x0f) < 0x0f) ++ad;
                if (vpos == 4 && hpos == 1 && (sr >> 4) < 0x0f) sr = B((sr & 0x0f) + (((sr >> 4) + 1) << 4));
                if (vpos == 5 && hpos == 1 && (sr & 0x0f) < 0x0f) ++sr;
                if (vpos == 7 && hpos == 1 && (kv & 0x3f) > 0) --kv;
                if (vpos == 12 && hpos == 3) _inc_feedback_val();
                if (vpos == 16 && hpos == 2) _inc_finetune_val();
                reset_instrument_preview();
                break;
            }

            case kPgDOWN:
            case kCHmins:
            case kNPmins: {
                uint8_t& ad = carrier ? fm.ATTCK_DEC_carrier : fm.ATTCK_DEC_modulator;
                uint8_t& sr = carrier ? fm.SUSTN_REL_carrier : fm.SUSTN_REL_modulator;
                uint8_t& kv = carrier ? fm.KSL_VOLUM_carrier : fm.KSL_VOLUM_modulator;
                if (vpos == 1 && (ad >> 4) > 0) ad = B((ad & 0x0f) + (((ad >> 4) - 1) << 4));
                if (vpos == 2 && hpos == 1 && (ad & 0x0f) > 0) --ad;
                if (vpos == 4 && hpos == 1 && (sr >> 4) > 0) sr = B((sr & 0x0f) + (((sr >> 4) - 1) << 4));
                if (vpos == 5 && hpos == 1 && (sr & 0x0f) > 0) --sr;
                if (vpos == 7 && hpos == 1 && (kv & 0x3f) < 0x3f) ++kv;
                if (vpos == 12 && hpos == 3) _dec_feedback_val();
                if (vpos == 16 && hpos == 2) _dec_finetune_val();
                reset_instrument_preview();
                break;
            }

            case kCtLbr:
                if (shift_pressed()) {
                    if (sd.macro_speedup > 1) --sd.macro_speedup;
                    P().macro_speedup = sd.macro_speedup;
                    keyboard_reset_buffer();
                } else if (current_inst > 1) {
                    select_instrument(B(current_inst - 1));
                }
                break;

            case kCtRbr:
                if (shift_pressed()) {
                    ++sd.macro_speedup;
                    if (calc_max_speedup(sd.tempo) < sd.macro_speedup) sd.macro_speedup = calc_max_speedup(sd.tempo);
                    P().macro_speedup = sd.macro_speedup;
                    keyboard_reset_buffer();
                } else if (current_inst < 255) {
                    select_instrument(B(current_inst + 1));
                }
                break;

            case kF1:
            case kF2:
            case kCtrlS:
            case kShF2:
            case kF3:
            case kCtrlL:
            case kCtrlO:
            case kCtrlQ:
            case kCtrlE:
                leave_key = fkey;
                break;

            case kF7:
                for (int temp = 1; temp <= 20; ++temp) P().reset_chan_data(B(temp));
                break;

            case kENTER:
                if (!is_perc_2_5(inst)) {
                    if (!shift_pressed()) {
                        if (remember_ins_pos) {
                            if (carrier) {
                                add_bank_position(bank_key("?carrier?hpos"), -1, hpos);
                                add_bank_position(bank_key("?carrier?vpos"), -1, vpos);
                            } else {
                                add_bank_position(bank_key("?modulator?hpos"), -1, hpos);
                                add_bank_position(bank_key("?modulator?vpos"), -1, vpos);
                            }
                        }
                        carrier = !carrier;
                        if (carrier && t4 != 0) switch_4op_slot();
                        _sync_radio_buttons();
                    } else {
                        // Shift+Enter: もう一方のオペレータの内容を写す
                        FmInstData& f = instr(instrum_page).fm_data;
                        if (carrier) {
                            f.ATTCK_DEC_carrier = f.ATTCK_DEC_modulator;
                            f.AM_VIB_EG_carrier = f.AM_VIB_EG_modulator;
                            f.KSL_VOLUM_carrier = f.KSL_VOLUM_modulator;
                            f.SUSTN_REL_carrier = f.SUSTN_REL_modulator;
                            f.WAVEFORM_carrier = f.WAVEFORM_modulator;
                        } else {
                            f.AM_VIB_EG_modulator = f.AM_VIB_EG_carrier;
                            f.KSL_VOLUM_modulator = f.KSL_VOLUM_carrier;
                            f.ATTCK_DEC_modulator = f.ATTCK_DEC_carrier;
                            f.SUSTN_REL_modulator = f.SUSTN_REL_carrier;
                            f.WAVEFORM_modulator = f.WAVEFORM_carrier;
                        }
                    }
                }
                break;

            default:
                nope = true;
                break;
            }

            if (leave_key != 0) break;

            // Shift+M / B / S / T / C / H: パーカッションの種類、Shift+O: 4OP の組の切り替え
            const char chr = UpCase(static_cast<char>(LO(fkey)));
            if (shift_pressed() && chr != '\0' && std::strchr("MOBSTCH", chr) != nullptr) {
                if (chr == 'O') {
                    instr(instrum_page).perc_voice = 0;
                    if (instrum_page != 255) instr(B(instrum_page + 1)).perc_voice = 0;
                    else instr(B(instrum_page - 1)).perc_voice = 0;
                    if (instrum_page != 255) {
                        if (check_4op_flag(instrum_page)) a2ins::reset_4op_flag(sd, instrum_page);
                        else a2ins::set_4op_flag(sd, instrum_page);
                    } else if (check_4op_flag(B(instrum_page - 1))) {
                        a2ins::reset_4op_flag(sd, B(instrum_page - 1));
                    } else {
                        a2ins::set_4op_flag(sd, B(instrum_page - 1));
                    }
                    update_4op_flag_marks();
                } else if (!check_4op_flag(instrum_page)) {
                    switch (chr) {
                    case 'M': inst->perc_voice = 0; break;
                    case 'B': inst->perc_voice = 1; break;
                    case 'S': inst->perc_voice = 2; break;
                    case 'T': inst->perc_voice = 3; break;
                    case 'C': inst->perc_voice = 4; break;
                    case 'H': inst->perc_voice = 5; break;
                    }
                }
                update_4op_flag_marks();
            }

            if (nope && fkey == kESC) done = true;
        }

        if (leave_key == 0) break;

        // 他の画面へ移って、戻ったら開き直す
        restore_screen();
        switch (leave_key) {
        case kF2:
        case kCtrlS:
            FILE_save(L"a2i", false);
            break;
        case kShF2:
            FILE_save(L"a2f", false);
            break;
        case kF3:
        case kCtrlL: {
            {
                PlayerLock lock;
                std::fill(std::begin(_operator_enabled), std::end(_operator_enabled), true);
            }
            FILE_open_instrument();
            PlayerLock lock;
            reset_instrument_preview();
            break;
        }
        default:
            // F1 (ヘルプ)・Ctrl+O (オクターブ設定)・Ctrl+Q / Ctrl+E (マクロエディタ) は未実装
            break;
        }
    }

    {
        PlayerLock lock;
        std::fill(std::begin(_operator_enabled), std::end(_operator_enabled), true);
        if (remember_ins_pos) {
            add_bank_position(bank_key("?carrier"), -1, _carrier_flag[carrier ? 1 : 0]);
            if (carrier) {
                add_bank_position(bank_key("?carrier?hpos"), -1, hpos);
                add_bank_position(bank_key("?carrier?vpos"), -1, vpos);
            } else {
                add_bank_position(bank_key("?modulator?hpos"), -1, hpos);
                add_bank_position(bank_key("?modulator?vpos"), -1, vpos);
            }
        }
    }
    restore_screen();
}

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
        if (key == kTAB) {
            {
                PlayerLock lock;
                const uint16_t t4 = get_4op_to_test();
                if (t4 != 0) {
                    at1(songdata().instr_data, HI(t4)).perc_voice = 0;
                    at1(songdata().instr_data, LO(t4)).perc_voice = 0;
                }
            }
            INSTRUMENT_CONTROL_edit();
            continue;
        }
        if (key == kCtrlQ || key == kShTAB || key == kCtrlE || key == kCtrlM) {
            // マクロエディタ (6e-3)・マクロブラウザ (6e-4) は未実装
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
