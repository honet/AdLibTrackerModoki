// ---------------------------------------------------------------------------
// 他のトラッカーの楽器バンクの読み込み (adt2ext5.pas の bnk / fib / ibk_file_loader と *_file_loader_alt、
// import_instrument_from_data_record、import_sbi_instrument_alt、*_lister_external_proc、
// iloadins.inc の import_standard_instrument_alt、test_instrument_alt2)
// ---------------------------------------------------------------------------
//
// MainScreen.cpp の無名名前空間の中で、BankLoad.inl の後に #include する。
//
// 一覧の項目は、使っている楽器なら '~ ~~' + 名前 + '~'、使っていない楽器なら ' ' + 名前 + '~' で始まる。
// オリジナルは Enter で取り込むとき、どちらも同じ位置 (Copy(…, 5, 8) など) から名前を取るため、
// 使っていない楽器では名前がずれる。本クローン版も同じにしている。

#pragma pack(push, 1)
struct BnkHeader   // tBNK_HEADER
{
    uint8_t fver_major;
    uint8_t fver_minor;
    char signature[6];
    uint16_t entries_used;
    uint16_t total_entries;
    int32_t name_offset;
    int32_t data_offset;
    uint8_t filler[8];
};

struct BnkNameRecord   // tBNK_NAME_RECORD
{
    uint16_t data_index;
    uint8_t usage_flag;
    char ins_name[9];
};

struct BnkOperator   // tBNK_OPERATOR
{
    uint8_t ksl, freq_mult, feedback, attack, sust_level, sustain, decay, release, output, am, vib, ksr, fm;
};

struct BnkDataRecord   // tBNK_DATA_RECORD
{
    uint8_t mode;
    uint8_t voice_num;
    BnkOperator modulator;
    BnkOperator carrier;
    uint8_t wform_mod;
    uint8_t wform_car;
};

struct FibHeader   // tFIB_HEADER
{
    char ident[4];
    uint16_t nmins;
};

struct FibInstrument   // tFIN_DATA
{
    char dname[12];
    char iname[27];
    uint8_t idata[11];
};

struct IbkInstrument   // ibk_file_loader の instrument_data
{
    uint8_t idata[11];
    uint8_t dummy[5];
};
#pragma pack(pop)

static_assert(sizeof(BnkHeader) == 28);
static_assert(sizeof(BnkNameRecord) == 12);
static_assert(sizeof(BnkDataRecord) == 30);
static_assert(sizeof(FibHeader) == 6);
static_assert(sizeof(FibInstrument) == 50);
static_assert(sizeof(IbkInstrument) == 16);

constexpr char bnk_id[] = "ADLIB-";
constexpr char fib_id[] = "FIB\xF4";
constexpr char ibk_id[] = "IBK\x1A";
constexpr int MAX_TIMBRES = 4096;   // オリジナルの max(…, MAX_TIMBRES) は小さい方を返す

// typcons2.inc (bnk_queue / fib の一覧は String[72]、ibk_queue は String[45])
const std::string bnk_header_str[3] = {
    " iNSTRUMENT                                  MELODiC/                   ",
    " NAME         \xDA" "20\xBF \xDA" "40\xBF \xDA" "60\xBF \xDA" "80\xBF \xDA" "E0\xBF C0    PERCUSSiON (VOiCE)         ",
    std::string(72, '\xCD'),
};

const std::string fib_header_str[3] = {
    " DOS       iNSTRUMENT                                                   ",
    " NAME      NAME                          \xDA" "20\xBF \xDA" "40\xBF \xDA" "60\xBF \xDA" "80\xBF \xDA" "E0\xBF C0    ",
    std::string(72, '\xCD'),
};

const std::string ibk_header_str[3] = {
    " iNSTRUMENT                                 ",
    " NAME         \xDA" "20\xBF \xDA" "40\xBF \xDA" "60\xBF \xDA" "80\xBF \xDA" "E0\xBF C0   ",
    std::string(44, '\xCD'),
};

// 一覧の先頭で飛ばした使っていない楽器の数 (オリジナルと同じく Byte)
uint8_t bnk_skip = 0;
uint8_t ibk_skip = 0;

BnkNameRecord name_record{};
BnkDataRecord data_record{};

// SeekF + BlockReadF。読めなければ (ファイルの外を含む) false
bool read_at(const std::vector<uint8_t>& file, int64_t pos, void* dst, size_t size)
{
    if (pos < 0 || static_cast<uint64_t>(pos) + size > file.size()) return false;
    std::memcpy(dst, file.data() + pos, size);
    return true;
}

// String[N] への代入 (長さを超えた部分を捨てる)
std::string short_str(const std::string& str, size_t len)
{
    return str.size() > len ? str.substr(0, len) : str;
}

template <size_t N>
std::string char_array_str(const char (&arr)[N])
{
    return std::string(arr, N);
}

std::string asciiz_str(const std::string& str)
{
    const size_t pos = str.find('\0');
    return pos != std::string::npos ? str.substr(0, pos) : std::string();
}

// FM データの 16 進表示 (20 / 40 / 60 / 80 / E0 / C0 の列)
std::string fm_data_hex(const FmInstData& fm)
{
    return byte2hex(fm.AM_VIB_EG_carrier) + byte2hex(fm.AM_VIB_EG_modulator) + " " + byte2hex(fm.KSL_VOLUM_carrier) +
           byte2hex(fm.KSL_VOLUM_modulator) + " " + byte2hex(fm.ATTCK_DEC_carrier) + byte2hex(fm.ATTCK_DEC_modulator) + " " +
           byte2hex(fm.SUSTN_REL_carrier) + byte2hex(fm.SUSTN_REL_modulator) + " " + byte2hex(fm.WAVEFORM_carrier) +
           byte2hex(fm.WAVEFORM_modulator) + " " + byte2hex(fm.FEEDBACK_FM);
}

void import_instrument_from_data_record()
{
    temp_instrument = Instrument{};
    FmInstData& fm = temp_instrument.fm_data;
    const BnkOperator& m = data_record.modulator;
    const BnkOperator& c = data_record.carrier;
    fm.AM_VIB_EG_modulator = B((m.freq_mult & 0x0f) + ((m.ksr & 1) << 4) + ((m.sustain & 1) << 5) + ((m.vib & 1) << 6) +
                               ((m.am & 1) << 7));
    fm.AM_VIB_EG_carrier = B((c.freq_mult & 0x0f) + ((c.ksr & 1) << 4) + ((c.sustain & 1) << 5) + ((c.vib & 1) << 6) +
                             ((c.am & 1) << 7));
    fm.KSL_VOLUM_modulator = B((m.output & 0x3f) + ((m.ksl & 3) << 6));
    fm.KSL_VOLUM_carrier = B((c.output & 0x3f) + ((c.ksl & 3) << 6));
    fm.ATTCK_DEC_modulator = B((m.decay & 0x0f) + ((m.attack & 0x0f) << 4));
    fm.ATTCK_DEC_carrier = B((c.decay & 0x0f) + ((c.attack & 0x0f) << 4));
    fm.SUSTN_REL_modulator = B((m.release & 0x0f) + ((m.sust_level & 0x0f) << 4));
    fm.SUSTN_REL_carrier = B((c.release & 0x0f) + ((c.sust_level & 0x0f) << 4));
    fm.WAVEFORM_modulator = B(data_record.wform_mod & 7);
    fm.WAVEFORM_carrier = B(data_record.wform_car & 7);
    // オリジナルの "fm XOR 1 AND 1 + ..." は XOR が + と同じ優先順位のため (fm XOR 1) + ... になる
    fm.FEEDBACK_FM = B((m.fm ^ 1) + ((m.feedback & 7) << 1));

    if (data_record.voice_num >= 6 && data_record.voice_num <= 10) temp_instrument.perc_voice = B(data_record.voice_num - 5);
}

// import_standard_instrument_alt (FM データ・panning・fine_tune だけを書き換え、perc_voice は前回の値のまま)
void import_standard_instrument_alt(const uint8_t* data)
{
    FmInstData& fm = temp_instrument.fm_data;
    fm.AM_VIB_EG_modulator = data[0];
    fm.AM_VIB_EG_carrier = data[1];
    fm.KSL_VOLUM_modulator = data[2];
    fm.KSL_VOLUM_carrier = data[3];
    fm.ATTCK_DEC_modulator = data[4];
    fm.ATTCK_DEC_carrier = data[5];
    fm.SUSTN_REL_modulator = data[6];
    fm.SUSTN_REL_carrier = data[7];
    fm.WAVEFORM_modulator = B(data[8] & 3);
    fm.WAVEFORM_carrier = B(data[9] & 3);
    fm.FEEDBACK_FM = B(data[10] & 0x0f);
    temp_instrument.panning = 0;
    temp_instrument.fine_tune = 0;
}

void import_sbi_instrument_alt(const uint8_t* data)
{
    temp_instrument = Instrument{};
    import_standard_instrument_alt(data);
}

// 楽器バンクの instr 番目の楽器を temp_instrument へ読む (bnk / fib / ibk_file_loader_alt)。読めたら true
bool bnk_file_loader_alt(uint16_t instr)
{
    std::vector<uint8_t> file;
    if (!a2ins::ReadFileBytes(instdata_source, file)) return false;

    BnkHeader header;
    if (!read_at(file, 0, &header, sizeof(header))) return false;
    if (!(header.fver_major == 1 && header.fver_minor == 0)) return false;
    if (instr > std::min<int>(header.total_entries, MAX_TIMBRES) || std::memcmp(header.signature, bnk_id, 6) != 0 ||
        header.total_entries < header.entries_used) {
        return false;
    }

    if (!read_at(file, header.name_offset + int64_t(instr - 1) * int64_t(sizeof(name_record)), &name_record,
                 sizeof(name_record))) {
        return false;
    }
    if (!read_at(file, header.data_offset + int64_t(name_record.data_index) * int64_t(sizeof(data_record)), &data_record,
                 sizeof(data_record))) {
        return false;
    }

    import_instrument_from_data_record();
    return true;
}

bool fib_file_loader_alt(uint16_t instr)
{
    std::vector<uint8_t> file;
    if (!a2ins::ReadFileBytes(instdata_source, file)) return false;

    FibHeader header;
    if (!read_at(file, 0, &header, sizeof(header)) || std::memcmp(header.ident, fib_id, 4) != 0) return false;

    char ident[4];
    if (!read_at(file, int64_t(sizeof(header)) + int64_t(header.nmins) * int64_t(sizeof(FibInstrument)), ident, sizeof(ident)) ||
        std::memcmp(ident, fib_id, 4) != 0) {
        return false;
    }

    FibInstrument instrument_data;
    if (!read_at(file, int64_t(sizeof(header)) + int64_t(instr - 1) * int64_t(sizeof(instrument_data)), &instrument_data,
                 sizeof(instrument_data))) {
        return false;
    }

    import_standard_instrument_alt(instrument_data.idata);
    return true;
}

bool ibk_file_loader_alt(uint16_t instr)
{
    std::vector<uint8_t> file;
    if (!a2ins::ReadFileBytes(instdata_source, file)) return false;

    char header[4];
    if (!read_at(file, 0, header, sizeof(header)) || std::memcmp(header, ibk_id, 4) != 0) return false;

    IbkInstrument instrument_data;
    if (!read_at(file, 0x004 + int64_t(instr - 1) * int64_t(sizeof(instrument_data)), &instrument_data, sizeof(instrument_data))) {
        return false;
    }

    import_sbi_instrument_alt(instrument_data.idata);
    return true;
}

// 楽器バンクの一覧で鍵盤を押したときの試聴 (test_instrument_alt2)。ロックを取った状態で呼ぶ。
// オリジナルと同じく、前回 temp_instrument に読んだ楽器が空なら鳴らさない。
void test_instrument_alt2(uint8_t /*chan*/, uint16_t fkey)
{
    A2Player& p = P();
    FixedSongData& sd = songdata();

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
    uint8_t temp2 = B(temp);
    const std::string temps = mn_environment.curr_item;

    if (is_empty_data(&temp_instrument, sizeof(temp_instrument))) return;

    const std::string ext = ExtOnly(temps);
    const uint16_t instr = static_cast<uint16_t>(std::strtol(BaseNameOnly(temps).c_str(), nullptr, 10));
    bool loaded = false;
    if (ext == "bnk") loaded = bnk_file_loader_alt(instr);
    else if (ext == "fib") loaded = fib_file_loader_alt(instr);
    else if (ext == "ibk") loaded = ibk_file_loader_alt(instr);
    if (!loaded) return;

    const bool status_backup_replay_forbidden = p.replay_forbidden;
    const PlayStatus status_backup_play_status = p.play_status;
    p.replay_forbidden = true;
    if (p.play_status != PlayStatus::Stopped) p.play_status = PlayStatus::Paused;
    p.nul_volume_bars();
    really_no_status_refresh = true;
    p.reset_player();

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
    const uint8_t flag_4op_backup = sd.flag_4op;
    if (!p.percussion_mode && !(sd.flag_4op != 0)) {
        channels = 18;
    } else if (!(sd.flag_4op != 0)) {
        channels = 15;
    } else {
        sd.flag_4op = 0;
        channels = !p.percussion_mode ? 18 : 15;
    }

    p.reset_player();
    uint8_t chan_handle[21] = {};   // [1..20]
    std::memcpy(pan_lock_backup, p.pan_lock, sizeof(p.pan_lock));
    std::memcpy(volume_lock_backup, p.volume_lock, sizeof(p.volume_lock));
    std::memcpy(peak_lock_backup, p.peak_lock, sizeof(p.peak_lock));
    std::memcpy(panning_table_backup, p.panning_table, sizeof(p.panning_table));
    std::fill(std::begin(p.pan_lock), std::end(p.pan_lock), false);
    std::fill(std::begin(p.volume_lock), std::end(p.volume_lock), false);
    std::fill(std::begin(p.peak_lock), std::end(p.peak_lock), false);
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

    // 鍵盤 board_pos の音を chan で鳴らす (マクロは current_inst のものを使う)
    const auto output_note = [&](uint8_t chan, uint8_t board_pos) {
        const int note = board_pos + 12 * (current_octave - 1);
        if (!(note >= 0 && note <= 12 * 8 + 1)) return false;

        chan_handle[chan] = board_scancodes[board_pos - 1];
        p.load_instrument(reinterpret_cast<const uint8_t*>(&temp_instrument), chan);
        if (p.percussion_mode && (temp_instrument.perc_voice == 4 || temp_instrument.perc_voice == 5) &&
            (chan == 19 || chan == 20)) {
            p.load_instrument(reinterpret_cast<const uint8_t*>(&temp_instrument), _perc_sim_chan[chan - 19]);
        }

        const uint16_t freq = static_cast<uint16_t>(A2Player::nFreq(B(note - 1)) + 0x2000 + temp_instrument.fine_tune);
        p.event_table[chan].note = B(note);
        p.freq_table[chan] = freq;
        p.freqtable2[chan] = freq;
        p.key_on(chan);
        p.change_freq(chan, freq);

        p.macro_speedup = sd.macro_speedup;
        if (p.play_status == PlayStatus::Stopped) p.update_timer(sd.tempo);

        p.freq_table[chan] = freq;
        p.freqtable2[chan] = freq;
        p.init_macro_table(chan, B(note), current_inst, freq);
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
                const uint8_t temp3 = get_chanpos(chan_handle + 1, channels, temp2);
                const bool temp4 = scankey(temp2);
                const uint8_t temp5 = get_chanpos(chan_handle + 1, channels, 0);

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

// 一覧の Menu の ext_proc (bnk / fib / ibk_lister_external_proc)
void bank_lister_external_proc(const char* ext, uint8_t skip)
{
    mn_environment.curr_item = Num2str(mn_environment.curr_pos + skip, 10) + ext;
    PlayerLock lock;
    if (!shift_pressed() && !alt_pressed() && !ctrl_pressed()) test_instrument_alt2(count_channel(pattern_hpos), mn_environment.keystroke);
}

void bnk_lister_external_proc() { bank_lister_external_proc(".bnk", bnk_skip); }
void fib_lister_external_proc() { bank_lister_external_proc(".fib", bnk_skip); }
void ibk_lister_external_proc() { bank_lister_external_proc(".ibk", ibk_skip); }

// 一覧を作る間の進捗の窓 (LOADiNG DATA FROM BANK FiLE...)
struct BankProgressWindow
{
    uint8_t xstart = 0, ystart = 0;

    void open(uint32_t value)
    {
        ScreenMemCopy(screen_ptr, screen_backup);
        centered_frame_vdest = screen_ptr;
        const std::string context = " ESC \xC4\x10 STOP ";
        centered_frame(xstart, ystart, 43, 3, " " + iCASE(bank_file_name()) + " ", dialog_background + dialog_border,
                       dialog_background + dialog_title, frame_double);
        ShowStr(screen_ptr, B(xstart + 43 - static_cast<int>(context.size())), B(ystart + 3), context, dialog_background + dialog_border);

        progress_num_steps = 1;
        progress_step = 1;
        progress_value = value;
        progress_old_value = BYTE_NULL;
        progress_xstart = B(xstart + 2);
        progress_ystart = B(ystart + 2);

        ShowCStr(screen_ptr, B(xstart + 2), B(ystart + 1), "LOADiNG DATA FROM BANK FiLE...", dialog_background + dialog_text,
                 dialog_background + dialog_hi_text);
        show_progress(0);
    }

    // _restore
    void restore() const
    {
        move_to_screen_data = screen_backup;
        move_to_screen_area[1] = xstart;
        move_to_screen_area[2] = ystart;
        move_to_screen_area[3] = B(xstart + 43 + 2 + 1);
        move_to_screen_area[4] = B(ystart + 3 + 1);
        move2screen();
    }

    static std::string bank_file_name() { return to_cp437(std::filesystem::path(instdata_source).filename().wstring()); }
};

// ESC で一覧の作成を止めるか (2 件目から、ticklooper が 0 のときだけ見る)
bool bank_loading_stopped(int index)
{
    if (P().ticklooper == 0 && keypressed() && index > 1) {
        if (getkey() == kESC) return true;
    }
    return false;
}

// 一覧を出して選ばせる。Enter なら選んだ番号 (1 始まり) を、それ以外は 0 を返す
uint16_t select_from_bank(std::vector<std::string>& queue, uint16_t nm_valid, uint8_t len, void (*ext_proc)())
{
    void (*const old_external_proc)() = mn_environment.ext_proc;
    const uint8_t old_topic_len = mn_setting.topic_len;
    const bool old_cycle_moves = mn_setting.cycle_moves;
    mn_environment.ext_proc = ext_proc;
    mn_setting.topic_len = 3;
    mn_setting.cycle_moves = false;

    const uint8_t ysize = is_default_screen_mode() ? 20 : 30;

    keyboard_reset_buffer();
    const std::string bank_name = to_cp437(instdata_source);
    const std::string title = " " + iCASE(BankProgressWindow::bank_file_name()) + " ";
    uint16_t index = 0;
    if (!_force_program_quit) {
        const uint16_t spos = static_cast<uint16_t>(std::max<int32_t>(get_bank_position(bank_name, nm_valid), 1));
        index = Menu(queue, 1, 1, spos, len, ysize, static_cast<uint16_t>(nm_valid + 3), title);
    }

    add_bank_position(bank_name, nm_valid, index + 3);
    mn_environment.ext_proc = old_external_proc;
    mn_setting.topic_len = old_topic_len;
    mn_setting.cycle_moves = old_cycle_moves;
    return mn_environment.keystroke == kENTER ? index : 0;
}

// 取り込んだ楽器を current_inst へ入れる
void put_bank_instrument(const std::string& name)
{
    PlayerLock lock;
    FixedSongData& sd = songdata();
    at1(sd.instr_data, current_inst) = temp_instrument;
    auto& instr_name = at1(sd.instr_names, current_inst);
    instr_name.assign(Copy(instr_name.str(), 1, 9) + name);
}

// BNK (AdLib Visual Composer の楽器バンク) の読み込み。取り込んだら (Enter を押したら) true
bool bnk_file_loader()
{
    static const char* const _perc_voice[5] = { "BD", "SD", "TT", "TC", "HH" };
    const std::string title = " BNK LOADER ";

    std::vector<uint8_t> file;
    if (!a2ins::ReadFileBytes(instdata_source, file)) {
        Dialog("ERROR READiNG DATA - DiSK ERROR?$LOADiNG STOPPED$", "~O~KAY$", title, 1);
        return false;
    }

    BnkHeader header;
    if (!read_at(file, 0, &header, sizeof(header))) {
        Dialog("ERROR READiNG DATA - FiLE CORRUPTED$LOADiNG STOPPED$", "~O~KAY$", title, 1);
        return false;
    }
    if (!(header.fver_major == 1 && header.fver_minor == 0)) {
        Dialog("UNKNOWN FiLE FORMAT VERSiON$LOADiNG STOPPED$", "~O~KAY$", title, 1);
        return false;
    }
    if (std::memcmp(header.signature, bnk_id, 6) != 0 || header.total_entries < header.entries_used) {
        Dialog("ERROR READiNG DATA - FiLE CORRUPTED$LOADiNG STOPPED$", "~O~KAY$", title, 1);
        return false;
    }

    const int count = std::min<int>(header.total_entries, MAX_TIMBRES);
    BankProgressWindow window;
    window.open(static_cast<uint32_t>(count));

    std::vector<std::string> bnk_queue(3 + std::max(count, 0));
    for (int idx = 0; idx < 3; ++idx) bnk_queue[idx] = bnk_header_str[idx];

    uint16_t nm_valid = 0;
    bnk_skip = 0;
    int index = 1;
    for (; index <= count; ++index) {
        if (bank_loading_stopped(index)) {
            --index;
            break;
        }
        if (P().ticklooper == 0) show_progress(static_cast<uint32_t>(index), 3);

        if (!read_at(file, header.name_offset + int64_t(index - 1) * int64_t(sizeof(name_record)), &name_record,
                     sizeof(name_record)) ||
            !read_at(file, header.data_offset + int64_t(name_record.data_index) * int64_t(sizeof(data_record)), &data_record,
                     sizeof(data_record))) {
            window.restore();
            Dialog("ERROR READiNG DATA - FiLE CORRUPTED$LOADiNG STOPPED$", "~O~KAY$", title, 1);
            return false;
        }

        import_instrument_from_data_record();
        std::string& q = bnk_queue[2 + index - bnk_skip];
        if (name_record.usage_flag != 0) {
            q = "~ ~~";
            ++nm_valid;
        } else {
            if (nm_valid == 0) {
                ++bnk_skip;
                continue;
            }
            q = " ";
        }

        q += ExpStrR(CutStr(asciiz_str(char_array_str(name_record.ins_name))), 11, ' ') + "~  ";
        q += fm_data_hex(temp_instrument.fm_data) + "    ";
        switch (data_record.mode) {
        case 0: q += "MELODiC"; break;
        case 1:
            if (data_record.voice_num >= 6 && data_record.voice_num <= 10)
                q += std::string("PERCUSSiON (") + _perc_voice[data_record.voice_num - 6] + ")";
            else
                q += "PERCUSSiON (??)";
            break;
        default: q += "???"; break;
        }
        q = short_str(q, 72);
    }
    if (index > count) index = std::max(count, 1);   // For ループを最後まで回ったときの値

    show_progress(static_cast<uint32_t>(index));
    // delay for awhile to show progress bar
    Sleep(200);
    window.restore();
    if (nm_valid == 0) return false;

    if (index == header.total_entries && bnk_skip == 0)
        mn_environment.context = "~[~" + Num2str(nm_valid, 10) + "~/" + Num2str(index, 10) + "]~";
    else
        mn_environment.context = "[" + Num2str(nm_valid, 10) + "]~[" + Num2str(index, 10) + "/" +
                                 Num2str(header.total_entries, 10) + "]~";

    const uint16_t selected = select_from_bank(bnk_queue, nm_valid, 72, bnk_lister_external_proc);
    if (selected == 0) return false;

    // load_flag := 1 (取り込めなくても曲は変わったものとして扱う)
    if (bnk_file_loader_alt(static_cast<uint16_t>(bnk_skip + selected))) put_bank_instrument(Copy(bnk_queue[2 + selected], 5, 8));
    return true;
}

// FIB の読み込み
bool fib_file_loader()
{
    const std::string title = " FiB LOADER ";

    std::vector<uint8_t> file;
    if (!a2ins::ReadFileBytes(instdata_source, file)) {
        Dialog("ERROR READiNG DATA - DiSK ERROR?$LOADiNG STOPPED$", "~O~KAY$", title, 1);
        return false;
    }

    FibHeader header;
    if (!read_at(file, 0, &header, sizeof(header)) || std::memcmp(header.ident, fib_id, 4) != 0) {
        Dialog("ERROR READiNG DATA - FiLE CORRUPTED$LOADiNG STOPPED$", "~O~KAY$", title, 1);
        return false;
    }

    const int count = std::min<int>(header.nmins, MAX_TIMBRES);
    BankProgressWindow window;
    window.open(static_cast<uint32_t>(count));

    std::vector<std::string> bnk_queue(3 + std::max(count, 0));
    for (int idx = 0; idx < 3; ++idx) bnk_queue[idx] = fib_header_str[idx];

    uint16_t nm_valid = 0;
    bnk_skip = 0;
    int64_t pos = sizeof(header);
    int index = 1;
    for (; index <= count; ++index) {
        if (bank_loading_stopped(index)) {
            --index;
            break;
        }
        if (P().ticklooper == 0) show_progress(static_cast<uint32_t>(index), 3);

        FibInstrument instrument_data;
        if (!read_at(file, pos, &instrument_data, sizeof(instrument_data))) {
            window.restore();
            Dialog("ERROR READiNG DATA - FiLE CORRUPTED$LOADiNG STOPPED$", "~O~KAY$", title, 1);
            return false;
        }
        pos += sizeof(instrument_data);

        import_standard_instrument_alt(instrument_data.idata);
        std::string& q = bnk_queue[2 + index - bnk_skip];
        if (!is_empty_data(instrument_data.idata, sizeof(instrument_data.idata))) {
            q = "~ ~~";
            ++nm_valid;
        } else {
            if (nm_valid == 0) {
                ++bnk_skip;
                continue;
            }
            q = " ";
        }

        q += ExpStrR(Upper(CutStr(BaseNameOnly(char_array_str(instrument_data.dname)))), 8, ' ') + "~  ";
        q += ExpStrR(CutStr(char_array_str(instrument_data.iname)), 27, ' ') + "   ";
        q += fm_data_hex(temp_instrument.fm_data) + "  ";
        q = short_str(q, 72);
    }
    if (index > count) index = std::max(count, 1);

    char ident[4];
    if (!read_at(file, int64_t(sizeof(header)) + int64_t(header.nmins) * int64_t(sizeof(FibInstrument)), ident, sizeof(ident)) ||
        std::memcmp(ident, fib_id, 4) != 0) {
        window.restore();
        Dialog("ERROR READiNG DATA - FiLE CORRUPTED$LOADiNG STOPPED$", "~O~KAY$", title, 1);
        return false;
    }

    show_progress(static_cast<uint32_t>(index), 3);
    // delay for awhile to show progress bar
    Sleep(200);
    window.restore();

    if (nm_valid == 0) return false;
    // オリジナルは全体の数を header.nmins ではなく 128 と表示する
    if (index == header.nmins && bnk_skip == 0)
        mn_environment.context = "~[~" + Num2str(nm_valid, 10) + "~/" + Num2str(index, 10) + "]~";
    else
        mn_environment.context = "[" + Num2str(nm_valid, 10) + "]~[" + Num2str(index, 10) + "/" + Num2str(128, 10) + "]~";

    const uint16_t selected = select_from_bank(bnk_queue, nm_valid, 72, fib_lister_external_proc);
    if (selected == 0) return false;

    if (fib_file_loader_alt(static_cast<uint16_t>(selected + bnk_skip))) {
        const std::string& q = bnk_queue[2 + selected];
        if (!CutStr(Copy(q, 16, 27)).empty()) put_bank_instrument(Copy(q, 16, 27));
        else put_bank_instrument(Copy(q, 5, 8));
    }
    return true;
}

// IBK の読み込み
bool ibk_file_loader()
{
    const std::string title = " iBK LOADER ";

    std::vector<uint8_t> file;
    if (!a2ins::ReadFileBytes(instdata_source, file)) {
        Dialog("ERROR READiNG DATA - DiSK ERROR?$LOADiNG STOPPED$", "~O~KAY$", title, 1);
        return false;
    }

    char header[4];
    if (!read_at(file, 0, header, sizeof(header)) || std::memcmp(header, ibk_id, 4) != 0) {
        Dialog("ERROR READiNG DATA - FiLE CORRUPTED$LOADiNG STOPPED$", "~O~KAY$", title, 1);
        return false;
    }

    constexpr int count = 128;
    BankProgressWindow window;
    window.open(count);

    std::vector<std::string> ibk_queue(3 + count);
    for (int idx = 0; idx < 3; ++idx) ibk_queue[idx] = ibk_header_str[idx];

    uint16_t nm_valid = 0;
    ibk_skip = 0;
    int index = 1;
    for (; index <= count; ++index) {
        if (bank_loading_stopped(index)) {
            --index;
            break;
        }
        if (P().ticklooper == 0) show_progress(static_cast<uint32_t>(index), 3);

        IbkInstrument instrument_data;
        char instrument_name[9];
        if (!read_at(file, 0x004 + int64_t(index - 1) * int64_t(sizeof(instrument_data)), &instrument_data,
                     sizeof(instrument_data)) ||
            !read_at(file, 0x804 + int64_t(index - 1) * int64_t(sizeof(instrument_name)), instrument_name,
                     sizeof(instrument_name))) {
            window.restore();
            Dialog("ERROR READiNG DATA - FiLE CORRUPTED$LOADiNG STOPPED$", "~O~KAY$", title, 1);
            return false;
        }

        import_sbi_instrument_alt(instrument_data.idata);
        std::string& q = ibk_queue[2 + index - ibk_skip];
        if (!is_empty_data(&instrument_data, sizeof(instrument_data))) {
            q = "~ ~~";
            ++nm_valid;
        } else {
            if (nm_valid == 0) {
                ++ibk_skip;
                continue;
            }
            q = " ";
        }

        q += ExpStrR(CutStr(asciiz_str(char_array_str(instrument_name))), 11, ' ') + "~  ";
        q += fm_data_hex(temp_instrument.fm_data) + "  ";
        q = short_str(q, 45);
    }
    if (index > count) index = count;

    show_progress(static_cast<uint32_t>(index), 3);
    // delay for awhile to show progress bar
    Sleep(200);
    window.restore();

    if (nm_valid == 0) return false;
    if (index == 128 && ibk_skip == 0)
        mn_environment.context = "~[~" + Num2str(nm_valid, 10) + "~/" + Num2str(index, 10) + "]~";
    else
        mn_environment.context = "[" + Num2str(nm_valid, 10) + "]~[" + Num2str(index, 10) + "/" + Num2str(128, 10) + "]~";

    const uint16_t selected = select_from_bank(ibk_queue, nm_valid, 45, ibk_lister_external_proc);
    if (selected == 0) return false;

    if (ibk_file_loader_alt(static_cast<uint16_t>(selected + ibk_skip))) put_bank_instrument(Copy(ibk_queue[2 + selected], 5, 8));
    return true;
}
