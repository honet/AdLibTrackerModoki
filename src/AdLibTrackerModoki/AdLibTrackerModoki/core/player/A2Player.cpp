#include "A2Player.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "../opl/IOplDevice.h"
#include "../song/EffectDefs.h"

namespace
{
    constexpr uint16_t MIN_IRQ_FREQ = 50;
    constexpr uint16_t MAX_IRQ_FREQ = 1000;
    constexpr uint8_t BYTE_NULL = 0xff;

    constexpr uint8_t keyoff_flag = 0x80;
    constexpr uint8_t fixed_note_flag = 0x90;
    constexpr uint8_t pattern_loop_flag = 0xe0;
    constexpr uint8_t pattern_break_flag = 0xf0;

    constexpr uint8_t MACRO_NOTE_RETRIG_FLAG = 0x80;
    constexpr uint8_t MACRO_ENVELOPE_RESTART_FLAG = 0x40;
    constexpr uint8_t MACRO_ZERO_FREQ_FLAG = 0x20;

    constexpr uint16_t FreqStart = 0x156;
    constexpr uint16_t FreqEnd = 0x2ae;
    constexpr uint16_t FreqRange = FreqEnd - FreqStart;

    constexpr uint8_t ef_NoteDelay_fx = ef_Extended2 + ef_fix2 + ef_ex2_NoteDelay;
    constexpr uint8_t ef_NoteCut_fx = ef_Extended2 + ef_fix2 + ef_ex2_NoteCut;

    constexpr uint8_t def_vibtrem_speed_factor = 1;
    constexpr uint8_t def_vibtrem_table_size = 32;
    const uint8_t def_vibtrem_table[256] = {
        0,24,49,74,97,120,141,161,180,197,212,224,235,244,250,253,255,
        253,250,244,235,224,212,197,180,161,141,120,97,74,49,24,
        0,24,49,74,97,120,141,161,180,197,212,224,235,244,250,253,255,
        253,250,244,235,224,212,197,180,161,141,120,97,74,49,24,
        0,24,49,74,97,120,141,161,180,197,212,224,235,244,250,253,255,
        253,250,244,235,224,212,197,180,161,141,120,97,74,49,24,
        0,24,49,74,97,120,141,161,180,197,212,224,235,244,250,253,255,
        253,250,244,235,224,212,197,180,161,141,120,97,74,49,24,
        0,24,49,74,97,120,141,161,180,197,212,224,235,244,250,253,255,
        253,250,244,235,224,212,197,180,161,141,120,97,74,49,24,
        0,24,49,74,97,120,141,161,180,197,212,224,235,244,250,253,255,
        253,250,244,235,224,212,197,180,161,141,120,97,74,49,24,
        0,24,49,74,97,120,141,161,180,197,212,224,235,244,250,253,255,
        253,250,244,235,224,212,197,180,161,141,120,97,74,49,24,
        0,24,49,74,97,120,141,161,180,197,212,224,235,244,250,253,255,
        253,250,244,235,224,212,197,180,161,141,120,97,74,49,24 };

    // _panning (array[0..2]) は範囲外の添字で参照されることがあるため、
    // オリジナルでメモリ上の直後に続く _instr_data_ofs の値を含めて 16 要素分を用意している。
    const uint8_t _panning[16] = { 0x30, 0x10, 0x20, 1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 10, 11, 12 };

    const uint16_t _instr[12] = { 0x20, 0x20, 0x40, 0x40, 0x60, 0x60, 0x80, 0x80, 0xe0, 0xe0, 0xc0, 0xbd };

    //                              01     02     03     04     05     06     07     08     09     10     11     12     13     14     15     16     17     18     19     20
    const uint16_t _chmm_n[21] = { 0, 0x003, 0x000, 0x004, 0x001, 0x005, 0x002, 0x006, 0x007, 0x008, 0x103, 0x100, 0x104, 0x101, 0x105, 0x102, 0x106, 0x107, 0x108, 0x0ff, 0x0ff };
    const uint16_t _chmm_m[21] = { 0, 0x008, 0x000, 0x009, 0x001, 0x00a, 0x002, 0x010, 0x011, 0x012, 0x108, 0x100, 0x109, 0x101, 0x10a, 0x102, 0x110, 0x111, 0x112, 0x0ff, 0x0ff };
    const uint16_t _chmm_c[21] = { 0, 0x00b, 0x003, 0x00c, 0x004, 0x00d, 0x005, 0x013, 0x014, 0x015, 0x10b, 0x103, 0x10c, 0x104, 0x10d, 0x105, 0x113, 0x114, 0x115, 0x0ff, 0x0ff };
    //                                                                                                                                       BD     SD     TT     TC     HH
    const uint16_t _chpm_n[21] = { 0, 0x003, 0x000, 0x004, 0x001, 0x005, 0x002, 0x106, 0x107, 0x108, 0x103, 0x100, 0x104, 0x101, 0x105, 0x102, 0x006, 0x007, 0x008, 0x008, 0x007 };
    const uint16_t _chpm_m[21] = { 0, 0x008, 0x000, 0x009, 0x001, 0x00a, 0x002, 0x110, 0x111, 0x112, 0x108, 0x100, 0x109, 0x101, 0x10a, 0x102, 0x010, 0x014, 0x012, 0x015, 0x011 };
    const uint16_t _chpm_c[21] = { 0, 0x00b, 0x003, 0x00c, 0x004, 0x00d, 0x005, 0x113, 0x114, 0x115, 0x10b, 0x103, 0x10c, 0x104, 0x10d, 0x105, 0x013, 0x0ff, 0x0ff, 0x0ff, 0x0ff };

    const uint8_t _4op_main_chan[6] = { 2, 4, 6, 11, 13, 15 };

    inline uint8_t LO(uint16_t w) { return static_cast<uint8_t>(w & 0xff); }
    inline uint8_t HI(uint16_t w) { return static_cast<uint8_t>(w >> 8); }
    inline uint16_t concw(uint8_t lo, uint8_t hi) { return static_cast<uint16_t>(lo + (hi << 8)); }

    // Pascal の Round (偶数丸め)
    inline int32_t pround(double v) { return static_cast<int32_t>(std::nearbyint(v)); }

    inline bool is_note(uint8_t note) { return note >= 1 && note <= 12 * 8 + 1; }
    inline bool in_4op_tracks_hi(uint8_t chan) { return chan == 1 || chan == 3 || chan == 5 || chan == 10 || chan == 12 || chan == 14; }
    inline bool in_4op_tracks_lo(uint8_t chan) { return chan == 2 || chan == 4 || chan == 6 || chan == 11 || chan == 13 || chan == 15; }
    inline bool is_perc_chan(uint8_t chan) { return chan >= 17 && chan <= 20; }

    inline uint8_t fx_def(const Chunk& c, int slot) { return slot == 0 ? c.effect_def : c.effect_def2; }
    inline uint8_t fx_val(const Chunk& c, int slot) { return slot == 0 ? c.effect : c.effect2; }

    template <class... T>
    inline bool one_of(uint8_t v, T... list) { return ((v == list) || ...); }
}

A2Player::A2Player(IOplDevice& opl)
    : opl_(opl)
    , song_(std::make_shared<Song>())
{
    std::memcpy(_chan_n, _chmm_n, sizeof(_chan_n));
    std::memcpy(_chan_m, _chmm_m, sizeof(_chan_m));
    std::memcpy(_chan_c, _chmm_c, sizeof(_chan_c));
    std::memcpy(vibtrem_table, def_vibtrem_table, sizeof(vibtrem_table));
    std::fill(std::begin(channel_flag), std::end(channel_flag), true);
    init_buffers();
    TimerSetup(50);   // init_timer_proc
}

// ---------------------------------------------------------------------------
// 出力・曲データ参照
// ---------------------------------------------------------------------------

void A2Player::opl3out(uint16_t reg, uint16_t data)
{
    opl_.writeReg(reg, static_cast<uint8_t>(data));
}

uint8_t A2Player::ins_parameter(uint8_t ins, uint8_t param)
{
    return reinterpret_cast<const uint8_t*>(&at1(songdata().instr_data, ins))[param];
}

bool A2Player::is_chan_adsr_data_empty(uint8_t chan)
{
    const FmParameterTable& f = fmpar_table[chan];
    return f.adsrw_car.attck == 0 && f.adsrw_mod.attck == 0 &&
           f.adsrw_car.dec == 0 && f.adsrw_mod.dec == 0 &&
           f.adsrw_car.sustn == 0 && f.adsrw_mod.sustn == 0 &&
           f.adsrw_car.rel == 0 && f.adsrw_mod.rel == 0;
}

bool A2Player::is_ins_adsr_data_empty(uint8_t ins)
{
    return (ins_parameter(ins, 5) >> 4) == 0 && (ins_parameter(ins, 4) >> 4) == 0 &&
           (ins_parameter(ins, 5) & 0x0f) == 0 && (ins_parameter(ins, 4) & 0x0f) == 0 &&
           (ins_parameter(ins, 7) >> 4) == 0 && (ins_parameter(ins, 6) >> 4) == 0 &&
           (ins_parameter(ins, 7) & 0x0f) == 0 && (ins_parameter(ins, 6) & 0x0f) == 0;
}

bool A2Player::is_4op_chan(uint8_t chan)
{
    const uint8_t f = songdata().flag_4op;
    return ((f & 0x01) && (chan == 1 || chan == 2)) ||
           ((f & 0x02) && (chan == 3 || chan == 4)) ||
           ((f & 0x04) && (chan == 5 || chan == 6)) ||
           ((f & 0x08) && (chan == 10 || chan == 11)) ||
           ((f & 0x10) && (chan == 12 || chan == 13)) ||
           ((f & 0x20) && (chan == 14 || chan == 15));
}

// ---------------------------------------------------------------------------
// 周波数
// ---------------------------------------------------------------------------

uint16_t A2Player::nFreq(uint8_t note)
{
    static const uint16_t Fnum[12] = { 0x157, 0x16b, 0x181, 0x198, 0x1b0, 0x1ca, 0x1e5, 0x202, 0x220, 0x241, 0x263, 0x287 };
    if (note >= 12 * 8) return (7 << 10) + FreqEnd;
    return static_cast<uint16_t>(((note / 12) << 10) + Fnum[note % 12]);
}

uint16_t A2Player::calc_freq_shift_up(uint16_t freq, uint16_t shift)
{
    uint16_t fshift = static_cast<uint16_t>((freq & 0x03ff) + shift);
    uint16_t temp = (freq & 0x1c00) >> 10;

    if (!(fshift < FreqEnd)) {
        if (temp != 7) {
            fshift = static_cast<uint16_t>(fshift - FreqRange);
            ++temp;
        } else {
            fshift = FreqEnd;
        }
    }

    uint16_t result = freq & 0xe000;
    result = static_cast<uint16_t>(result + static_cast<uint16_t>(temp << 10));
    result = static_cast<uint16_t>(result + fshift);
    return result;
}

uint16_t A2Player::calc_freq_shift_down(uint16_t freq, uint16_t shift)
{
    uint16_t fshift = static_cast<uint16_t>((freq & 0x03ff) - shift);
    uint16_t temp = (freq & 0x1c00) >> 10;

    if (!(fshift > FreqStart)) {
        if (temp != 0) {
            fshift = static_cast<uint16_t>(fshift + FreqRange);
            --temp;
        } else {
            fshift = FreqStart;
        }
    }

    uint16_t result = freq & 0xe000;
    result = static_cast<uint16_t>(result + static_cast<uint16_t>(temp << 10));
    result = static_cast<uint16_t>(result + fshift);
    return result;
}

uint16_t A2Player::calc_vibtrem_shift(VibTremState& table)
{
    const uint8_t pos = table.pos;
    const uint8_t depth = table.depth;

    uint16_t result = static_cast<uint16_t>(vibtrem_table[pos & static_cast<uint8_t>(vibtrem_table_size - 1)] * depth);
    result = static_cast<uint16_t>((result << 1) | (result >> 15));
    result = static_cast<uint16_t>((result << 8) | (result >> 8));
    result = static_cast<uint16_t>((result & 0x00ff) | (((result >> 8) & 1) << 8));

    table.dir = (pos & vibtrem_table_size) != 0 ? 1 : 0;
    return result;
}

void A2Player::change_freq(uint8_t chan, uint16_t freq)
{
    if (is_4op_chan(chan) && in_4op_tracks_hi(chan)) {
        freq_table[chan + 1] = freq_table[chan];
        freqtable2[chan + 1] = freqtable2[chan];
        ++chan;
    }

    freq_table[chan] = static_cast<uint16_t>((freq & 0x1fff) + (freq_table[chan] & ~0x1fff));
    freqtable2[chan] = freq_table[chan];
    if (channel_flag[chan]) {
        opl3out(0x0b0 + _chan_n[chan], HI(freq_table[chan]));
        opl3out(0x0a0 + _chan_n[chan], LO(freq_table[chan]));
    }

    if (is_4op_chan(chan)) {
        freq_table[chan - 1] = freq_table[chan];
        freqtable2[chan - 1] = freqtable2[chan];
    }
}

void A2Player::change_frequency(uint8_t chan, uint16_t freq)
{
    macro_table[chan].vib_paused = true;
    change_freq(chan, freq);

    if (is_4op_chan(chan)) {
        const uint8_t pair = in_4op_tracks_hi(chan) ? chan + 1 : chan - 1;
        macro_table[pair].vib_count = 1;
        macro_table[pair].vib_pos = 0;
        macro_table[pair].vib_freq = freq;
        macro_table[pair].vib_paused = false;
    }

    macro_table[chan].vib_count = 1;
    macro_table[chan].vib_pos = 0;
    macro_table[chan].vib_freq = freq;
    macro_table[chan].vib_paused = false;
}

// ---------------------------------------------------------------------------
// タイマー
// ---------------------------------------------------------------------------

uint16_t A2Player::_macro_speedup() const
{
    return macro_speedup > 0 ? macro_speedup : macro_speedup + 1;
}

void A2Player::synchronize_song_timer()
{
    song_timer = static_cast<int32_t>(time_playing);
    song_timer_tenths = static_cast<int32_t>(time_playing * 100) % 100;
    timer_temp = song_timer_tenths;
}

void A2Player::update_timer(uint16_t hz)
{
    if (hz == 0) {
        TimerSetup(18);
        return;
    }
    tempo = static_cast<uint8_t>(hz);
    if (tempo == 18 && timer_fix) IRQ_freq = static_cast<uint16_t>((tempo + 0.2) * 20);
    else IRQ_freq = 250;
    while (IRQ_freq % (tempo * _macro_speedup()) != 0) ++IRQ_freq;
    if (IRQ_freq > MAX_IRQ_FREQ) IRQ_freq = MAX_IRQ_FREQ;
    while (IRQ_freq + IRQ_freq_shift + playback_speed_shift > MAX_IRQ_FREQ && playback_speed_shift > 0) {
        --playback_speed_shift;
    }
    while (IRQ_freq + IRQ_freq_shift + playback_speed_shift > MAX_IRQ_FREQ && IRQ_freq_shift > 0) {
        --IRQ_freq_shift;
    }
    TimerSetup(std::min<int32_t>(IRQ_freq + IRQ_freq_shift + playback_speed_shift, MAX_IRQ_FREQ));
}

void A2Player::update_playback_speed(int32_t speed_shift)
{
    if (speed_shift == 0) return;
    if (speed_shift > 0 && IRQ_freq + playback_speed_shift + speed_shift > MAX_IRQ_FREQ) {
        while (IRQ_freq + IRQ_freq_shift + playback_speed_shift + speed_shift > MAX_IRQ_FREQ) --speed_shift;
    } else if (speed_shift < 0 && IRQ_freq + IRQ_freq_shift + playback_speed_shift + speed_shift < MIN_IRQ_FREQ) {
        while (IRQ_freq + IRQ_freq_shift + playback_speed_shift + speed_shift < MIN_IRQ_FREQ) ++speed_shift;
    }
    playback_speed_shift = playback_speed_shift + speed_shift;
    update_timer(tempo);
}

// ---------------------------------------------------------------------------
// 音量・音色
// ---------------------------------------------------------------------------

void A2Player::key_on(uint8_t chan)
{
    if (!(is_4op_chan(chan) && in_4op_tracks_hi(chan))) opl3out(0x0b0 + _chan_n[chan], 0);
    else opl3out(0x0b0 + _chan_n[chan + 1], 0);
}

void A2Player::key_off(uint8_t chan)
{
    freq_table[chan] = static_cast<uint16_t>(LO(freq_table[chan]) + ((HI(freq_table[chan]) & ~0x20) << 8));
    change_freq(chan, freq_table[chan]);
    event_table[chan].note = event_table[chan].note | keyoff_flag;
}

void A2Player::release_sustaining_sound(uint8_t chan)
{
    opl3out(_instr[2] + _chan_m[chan], 63);
    opl3out(_instr[3] + _chan_c[chan], 63);

    std::memset(&fmpar_table[chan].adsrw_car, 0, sizeof(AdsrwState));
    std::memset(&fmpar_table[chan].adsrw_mod, 0, sizeof(AdsrwState));

    key_on(chan);
    opl3out(_instr[4] + _chan_m[chan], BYTE_NULL);
    opl3out(_instr[5] + _chan_c[chan], BYTE_NULL);
    opl3out(_instr[6] + _chan_m[chan], BYTE_NULL);
    opl3out(_instr[7] + _chan_c[chan], BYTE_NULL);

    key_off(chan);
    event_table[chan].instr_def = 0;
    reset_chan[chan] = true;
}

uint8_t A2Player::scale_volume(uint8_t volume, uint8_t scale_factor)
{
    return static_cast<uint8_t>(63 - pround((63 - volume) / 63.0 * (63 - scale_factor)));
}

A2Player::FourOpData A2Player::_4op_data_flag(uint8_t chan)
{
    FourOpData d{};
    if (is_4op_chan(chan)) {
        d.mode = true;
        if (in_4op_tracks_hi(chan)) {
            d.ch1 = chan;
            d.ch2 = chan + 1;
        } else {
            d.ch1 = chan - 1;
            d.ch2 = chan;
        }
        d.ins1 = event_table[d.ch1].instr_def;
        if (d.ins1 == 0) d.ins1 = voice_table[d.ch1];
        d.ins2 = event_table[d.ch2].instr_def;
        if (d.ins2 == 0) d.ins2 = voice_table[d.ch2];
        if (d.ins1 != 0 && d.ins2 != 0) {
            d.conn = static_cast<uint8_t>(((at1(songdata().instr_data, d.ins1).fm_data.FEEDBACK_FM & 1) << 1) +
                                          (at1(songdata().instr_data, d.ins2).fm_data.FEEDBACK_FM & 1));
        }
    }
    d.ch1 &= 15;
    d.ch2 &= 15;
    d.conn &= 3;
    return d;
}

bool A2Player::_4op_vol_valid_chan(uint8_t chan)
{
    const FourOpData d = _4op_data_flag(chan);
    return d.mode && vol4op_lock[chan] && d.ins1 != 0 && d.ins2 != 0;
}

void A2Player::set_ins_volume(uint8_t modulator, uint8_t carrier, uint8_t chan)
{
    // OPL3 エミュレーション向けの回避策 (SDL 版のみ):
    // チャンネルの ADSR データが空で FM レジスタマクロも無い場合は強制的に消音する
    if (is_chan_adsr_data_empty(chan) && !(at1(songdata().instr_macros, voice_table[chan]).length != 0)) {
        modulator = 63;
        carrier = 63;
    }

    const bool am_or_perc = (ins_parameter(voice_table[chan], 10) & 1) == 1 || (percussion_mode && is_perc_chan(chan));

    if (modulator != BYTE_NULL) {
        const uint8_t temp = modulator;
        if (volume_scaling && am_or_perc) {
            modulator = scale_volume(ins_parameter(voice_table[chan], 2) & 0x3f, modulator);
        }
        if (am_or_perc) {
            opl3out(_instr[2] + _chan_m[chan],
                    scale_volume(scale_volume(modulator, scale_volume(63 - global_volume, 63 - fade_out_volume)), 63 - overall_volume) + LO(vscale_table[chan]));
        } else {
            opl3out(_instr[2] + _chan_m[chan], temp + LO(vscale_table[chan]));
        }
        volume_table[chan] = concw(temp, HI(volume_table[chan]));
        if (am_or_perc) {
            modulator_vol[chan] = static_cast<uint8_t>(63 - scale_volume(modulator, scale_volume(63 - global_volume, 63 - fade_out_volume)));
        } else {
            modulator_vol[chan] = static_cast<uint8_t>(63 - modulator);
        }
    }

    if (carrier != BYTE_NULL) {
        const uint8_t temp = carrier;
        if (volume_scaling) {
            carrier = scale_volume(ins_parameter(voice_table[chan], 3) & 0x3f, carrier);
        }
        opl3out(_instr[3] + _chan_c[chan],
                scale_volume(scale_volume(carrier, scale_volume(63 - global_volume, 63 - fade_out_volume)), 63 - overall_volume) + HI(vscale_table[chan]));
        volume_table[chan] = concw(LO(volume_table[chan]), temp);
        carrier_vol[chan] = static_cast<uint8_t>(63 - scale_volume(carrier, scale_volume(63 - global_volume, 63 - fade_out_volume)));
    }
}

void A2Player::set_ins_volume_4op(uint8_t volume, uint8_t chan)
{
    auto set_volume = [this](uint8_t modulator, uint8_t carrier, uint8_t chan) {
        if (is_chan_adsr_data_empty(chan) && !(at1(songdata().instr_macros, voice_table[chan]).length != 0)) {
            modulator = 63;
            carrier = 63;
        }

        if (modulator != BYTE_NULL) {
            const uint8_t temp = modulator;
            modulator = scale_volume(ins_parameter(voice_table[chan], 2) & 0x3f, modulator);
            opl3out(_instr[2] + _chan_m[chan],
                    scale_volume(scale_volume(modulator, scale_volume(63 - global_volume, 63 - fade_out_volume)), 63 - overall_volume) + LO(vscale_table[chan]));
            volume_table[chan] = concw(temp, HI(volume_table[chan]));
            modulator_vol[chan] = static_cast<uint8_t>(63 - scale_volume(modulator, scale_volume(63 - global_volume, 63 - fade_out_volume)));
        }

        if (carrier != BYTE_NULL) {
            const uint8_t temp = carrier;
            carrier = scale_volume(ins_parameter(voice_table[chan], 3) & 0x3f, carrier);
            opl3out(_instr[3] + _chan_c[chan],
                    scale_volume(scale_volume(carrier, scale_volume(63 - global_volume, 63 - fade_out_volume)), 63 - overall_volume) + HI(vscale_table[chan]));
            volume_table[chan] = concw(LO(volume_table[chan]), temp);
            carrier_vol[chan] = static_cast<uint8_t>(63 - scale_volume(carrier, scale_volume(63 - global_volume, 63 - fade_out_volume)));
        }
    };

    const FourOpData d = _4op_data_flag(chan);
    if (!_4op_vol_valid_chan(chan)) return;

    switch (d.conn) {
    case 0:   // FM/FM
        if (volume == BYTE_NULL) set_volume(BYTE_NULL, HI(volume_table[d.ch1]), d.ch1);
        else set_volume(BYTE_NULL, volume, d.ch1);
        break;
    case 1:   // FM/AM
        if (volume == BYTE_NULL) {
            set_volume(BYTE_NULL, HI(volume_table[d.ch1]), d.ch1);
            set_volume(LO(volume_table[d.ch2]), BYTE_NULL, d.ch2);
        } else {
            set_volume(BYTE_NULL, volume, d.ch1);
            set_volume(volume, BYTE_NULL, d.ch2);
        }
        break;
    case 2:   // AM/FM
        if (volume == BYTE_NULL) {
            set_volume(BYTE_NULL, HI(volume_table[d.ch1]), d.ch1);
            set_volume(BYTE_NULL, HI(volume_table[d.ch2]), d.ch2);
        } else {
            set_volume(BYTE_NULL, volume, d.ch1);
            set_volume(BYTE_NULL, volume, d.ch2);
        }
        break;
    case 3:   // AM/AM
        if (volume == BYTE_NULL) {
            set_volume(LO(volume_table[d.ch1]), HI(volume_table[d.ch1]), d.ch1);
            set_volume(LO(volume_table[d.ch2]), BYTE_NULL, d.ch2);
        } else {
            set_volume(volume, volume, d.ch1);
            set_volume(volume, BYTE_NULL, d.ch2);
        }
        break;
    }
}

void A2Player::reset_ins_volume(uint8_t chan)
{
    if (!volume_scaling) {
        set_ins_volume(ins_parameter(voice_table[chan], 2) & 0x3f, ins_parameter(voice_table[chan], 3) & 0x3f, chan);
    } else if ((ins_parameter(voice_table[chan], 10) & 1) == 0) {
        set_ins_volume(ins_parameter(voice_table[chan], 2) & 0x3f, 0, chan);
    } else {
        set_ins_volume(0, 0, chan);
    }
}

void A2Player::set_ins_data(uint8_t ins, uint8_t chan)
{
    if (ins != event_table[chan].instr_def || reset_chan[chan]) {
        opl3out(_instr[2] + _chan_m[chan], 63);
        opl3out(_instr[3] + _chan_c[chan], 63);

        if (!pan_lock[chan]) panning_table[chan] = ins_parameter(ins, 11);
        else panning_table[chan] = songdata().lock_flags[chan - 1] & 3;

        opl3out(_instr[0] + _chan_m[chan], ins_parameter(ins, 0));
        opl3out(_instr[1] + _chan_c[chan], ins_parameter(ins, 1));
        opl3out(_instr[4] + _chan_m[chan], ins_parameter(ins, 4));
        opl3out(_instr[5] + _chan_c[chan], ins_parameter(ins, 5));
        opl3out(_instr[6] + _chan_m[chan], ins_parameter(ins, 6));
        opl3out(_instr[7] + _chan_c[chan], ins_parameter(ins, 7));
        opl3out(_instr[8] + _chan_m[chan], ins_parameter(ins, 8));
        opl3out(_instr[9] + _chan_c[chan], ins_parameter(ins, 9));
        opl3out(_instr[10] + _chan_n[chan], ins_parameter(ins, 10) | _panning[panning_table[chan] & 15]);

        FmParameterTable& f = fmpar_table[chan];
        f.connect = ins_parameter(ins, 10) & 1;
        f.feedb = (ins_parameter(ins, 10) >> 1) & 7;
        f.multipM = ins_parameter(ins, 0) & 0x0f;
        f.kslM = ins_parameter(ins, 2) >> 6;
        f.tremM = ins_parameter(ins, 0) >> 7;
        f.vibrM = (ins_parameter(ins, 0) >> 6) & 1;
        f.ksrM = (ins_parameter(ins, 0) >> 4) & 1;
        f.sustM = (ins_parameter(ins, 0) >> 5) & 1;
        f.multipC = ins_parameter(ins, 1) & 0x0f;
        f.kslC = ins_parameter(ins, 3) >> 6;
        f.tremC = ins_parameter(ins, 1) >> 7;
        f.vibrC = (ins_parameter(ins, 1) >> 6) & 1;
        f.ksrC = (ins_parameter(ins, 1) >> 4) & 1;
        f.sustC = (ins_parameter(ins, 1) >> 5) & 1;

        f.adsrw_car.attck = ins_parameter(ins, 5) >> 4;
        f.adsrw_mod.attck = ins_parameter(ins, 4) >> 4;
        f.adsrw_car.dec = ins_parameter(ins, 5) & 0x0f;
        f.adsrw_mod.dec = ins_parameter(ins, 4) & 0x0f;
        f.adsrw_car.sustn = ins_parameter(ins, 7) >> 4;
        f.adsrw_mod.sustn = ins_parameter(ins, 6) >> 4;
        f.adsrw_car.rel = ins_parameter(ins, 7) & 0x0f;
        f.adsrw_mod.rel = ins_parameter(ins, 6) & 0x0f;
        f.adsrw_car.wform = ins_parameter(ins, 9) & 0x07;
        f.adsrw_mod.wform = ins_parameter(ins, 8) & 0x07;

        if (!reset_chan[chan]) keyoff_loop[chan] = false;

        if (reset_chan[chan]) {
            voice_table[chan] = ins;
            reset_ins_volume(chan);
            reset_chan[chan] = false;
        }

        if (is_note(event_table[chan].note & 0x7f)) init_macro_table(chan, event_table[chan].note & 0x7f, ins, freq_table[chan]);
        else init_macro_table(chan, 0, ins, freq_table[chan]);
    }

    vscale_table[chan] = concw(static_cast<uint8_t>(fmpar_table[chan].kslM << 6), static_cast<uint8_t>(fmpar_table[chan].kslC << 6));
    voice_table[chan] = ins;
    const uint8_t old_ins = event_table[chan].instr_def;
    event_table[chan].instr_def = ins;

    if (!volume_lock[chan] || ins != old_ins) reset_ins_volume(chan);
    ai_table[ins] = 1;
}

void A2Player::load_instrument(const uint8_t* data, uint8_t chan)
{
    FmParameterTable& f = fmpar_table[chan];
    f.connect = data[10] & 1;
    f.feedb = (data[10] >> 1) & 7;
    f.multipM = data[0] & 0x0f;
    f.kslM = data[2] >> 6;
    f.tremM = data[0] >> 7;
    f.vibrM = (data[0] >> 6) & 1;
    f.ksrM = (data[0] >> 4) & 1;
    f.sustM = (data[0] >> 5) & 1;
    f.multipC = data[1] & 0x0f;
    f.kslC = data[3] >> 6;
    f.tremC = data[1] >> 7;
    f.vibrC = (data[1] >> 6) & 1;
    f.ksrC = (data[1] >> 4) & 1;
    f.sustC = (data[1] >> 5) & 1;

    f.adsrw_car.attck = data[5] >> 4;
    f.adsrw_mod.attck = data[4] >> 4;
    f.adsrw_car.dec = data[5] & 0x0f;
    f.adsrw_mod.dec = data[4] & 0x0f;
    f.adsrw_car.sustn = data[7] >> 4;
    f.adsrw_mod.sustn = data[6] >> 4;
    f.adsrw_car.rel = data[7] & 0x0f;
    f.adsrw_mod.rel = data[6] & 0x0f;
    f.adsrw_car.wform = data[9] & 0x07;
    f.adsrw_mod.wform = data[8] & 0x07;

    panning_table[chan] = data[11] & 3;
    volume_table[chan] = concw(data[2] & 0x3f, data[3] & 0x3f);

    update_modulator_adsrw(chan);
    update_carrier_adsrw(chan);
    update_fmpar(chan);
}

void A2Player::update_modulator_adsrw(uint8_t chan)
{
    const AdsrwState& m = fmpar_table[chan].adsrw_mod;
    opl3out(_instr[4] + _chan_m[chan], (m.attck << 4) + m.dec);
    opl3out(_instr[6] + _chan_m[chan], (m.sustn << 4) + m.rel);
    opl3out(_instr[8] + _chan_m[chan], m.wform);
}

void A2Player::update_carrier_adsrw(uint8_t chan)
{
    const AdsrwState& c = fmpar_table[chan].adsrw_car;
    opl3out(_instr[5] + _chan_c[chan], (c.attck << 4) + c.dec);
    opl3out(_instr[7] + _chan_c[chan], (c.sustn << 4) + c.rel);
    opl3out(_instr[9] + _chan_c[chan], c.wform);
}

void A2Player::update_fmpar(uint8_t chan)
{
    const FmParameterTable& f = fmpar_table[chan];
    opl3out(_instr[0] + _chan_m[chan], f.multipM + (f.ksrM << 4) + (f.sustM << 5) + (f.vibrM << 6) + (f.tremM << 7));
    opl3out(_instr[1] + _chan_c[chan], f.multipC + (f.ksrC << 4) + (f.sustC << 5) + (f.vibrC << 6) + (f.tremC << 7));
    opl3out(_instr[10] + _chan_n[chan], (f.connect + (f.feedb << 1)) | _panning[panning_table[chan] & 15]);

    vscale_table[chan] = concw(static_cast<uint8_t>(f.kslM << 6), static_cast<uint8_t>(f.kslC << 6));
    set_ins_volume(LO(volume_table[chan]), HI(volume_table[chan]), chan);
}

void A2Player::reset_chan_data(uint8_t chan)
{
    if (percussion_mode && is_perc_chan(chan)) {
        if (channel_flag[chan]) reset_ins_volume(chan);
        else set_ins_volume(0, 0, chan);
    } else {
        opl3out(_instr[2] + _chan_m[chan], 63);
        opl3out(_instr[3] + _chan_c[chan], 63);
        key_on(chan);

        opl3out(_instr[4] + _chan_m[chan], BYTE_NULL);
        opl3out(_instr[5] + _chan_c[chan], BYTE_NULL);
        opl3out(_instr[6] + _chan_m[chan], BYTE_NULL);
        opl3out(_instr[7] + _chan_c[chan], BYTE_NULL);
        key_off(chan);
        update_fmpar(chan);
    }

    reset_adsrw[chan] = true;
    if (play_status != PlayStatus::Stopped) {
        if (is_note(event_table[chan].note & 0x7f)) init_macro_table(chan, event_table[chan].note & 0x7f, voice_table[chan], freq_table[chan]);
        else init_macro_table(chan, 0, voice_table[chan], freq_table[chan]);
    }
}

void A2Player::init_macro_table(uint8_t chan, uint8_t note, uint8_t ins, uint16_t freq)
{
    MacroState& m = macro_table[chan];
    m.fmreg_count = 1;
    m.fmreg_pos = 0;
    m.fmreg_duration = 0;
    m.fmreg_table = ins;
    m.arpg_count = 1;
    m.arpg_pos = 0;
    m.arpg_table = at1(songdata().instr_macros, ins).arpeggio_table;
    m.arpg_note = note;
    m.vib_count = 1;
    m.vib_paused = false;
    m.vib_pos = 0;
    m.vib_table = at1(songdata().instr_macros, ins).vibrato_table;
    m.vib_freq = freq;
    m.vib_delay = at1(songdata().macro_table, m.vib_table).vibrato.delay;
    zero_fq_table[chan] = 0;
}

void A2Player::output_note(uint8_t note, uint8_t ins, uint8_t chan, bool restart_macro, bool restart_adsr)
{
    if (note == 0 && ftune_table[chan] == 0) return;

    uint16_t freq;
    if (!is_note(note)) {
        freq = freq_table[chan];
    } else {
        freq = static_cast<uint16_t>(nFreq(note - 1) + static_cast<int8_t>(ins_parameter(ins, 12)));
        if (restart_adsr) key_on(chan);

        freq_table[chan] = concw(LO(freq_table[chan]), HI(freq_table[chan]) | 0x20);

        // ボリュームバー表示用
        auto rise_volum_bar = [this](uint8_t ch) {
            if (!(percussion_mode && is_perc_chan(ch))) {
                if ((ins_parameter(voice_table[ch], 10) & 1) == 1) {
                    if (volum_bar[ch].lvl < (carrier_vol[ch] + modulator_vol[ch]) / 2) volum_bar[ch].dir = 1;
                } else if (volum_bar[ch].lvl < carrier_vol[ch]) {
                    volum_bar[ch].dir = 1;
                }
            } else if (volum_bar[ch].lvl < modulator_vol[ch]) {
                volum_bar[ch].dir = 1;
            }
        };
        auto rise_decay_bar = [this](uint8_t ch) {
            if (decay_bar[ch].lvl1 < carrier_vol[ch]) decay_bar[ch].dir1 = 1;
            if (decay_bar[ch].lvl2 < modulator_vol[ch]) decay_bar[ch].dir2 = 1;
        };

        if (channel_flag[chan]) {
            if (is_4op_chan(chan)) {
                rise_volum_bar(chan);
                rise_volum_bar(chan - 1);
                rise_decay_bar(chan);
                rise_decay_bar(chan - 1);
                if (play_status != PlayStatus::Playing) {
                    volum_bar[chan].dir = -1;
                    decay_bar[chan].dir1 = -1;
                    decay_bar[chan].dir2 = -1;
                    decay_bar[chan - 1].dir1 = -1;
                    decay_bar[chan - 1].dir2 = -1;
                }
            } else {
                rise_volum_bar(chan);
                rise_decay_bar(chan);
                if (play_status != PlayStatus::Playing) {
                    volum_bar[chan].dir = -1;
                    decay_bar[chan].dir1 = -1;
                    decay_bar[chan].dir2 = -1;
                }
            }
        }
    }

    if (ftune_table[chan] == -127) ftune_table[chan] = 0;
    freq = static_cast<uint16_t>(freq + ftune_table[chan]);
    change_frequency(chan, freq);

    if (note != 0) {
        event_table[chan].note = note;
        if (is_4op_chan(chan)) event_table[chan - 1].note = note;
        if (restart_macro) {
            const Chunk& e = event_table[chan];
            const bool no_restart =
                (e.effect_def == ef_Extended && e.effect / 16 == ef_ex_ExtendedCmd2 && e.effect % 16 == ef_ex_cmd2_NoRestart) ||
                (e.effect_def2 == ef_Extended && e.effect2 / 16 == ef_ex_ExtendedCmd2 && e.effect2 % 16 == ef_ex_cmd2_NoRestart);
            if (!no_restart) init_macro_table(chan, note, ins, freq);
            else macro_table[chan].arpg_note = note;
        }
    }
}

void A2Player::generate_custom_vibrato(uint8_t value)
{
    static const uint8_t vibtab_size[16] = { 16, 16, 16, 16, 32, 32, 32, 32, 64, 64, 64, 64, 128, 128, 128, 128 };
    auto min0 = [](int32_t v) { return v >= 0 ? v : 0; };

    if (value == 0) {
        // 既定のテーブル
        vibtrem_table_size = def_vibtrem_table_size;
        std::memcpy(vibtrem_table, def_vibtrem_table, sizeof(vibtrem_table));
    } else if (value <= 239) {
        // カスタムテーブル (サイズ 32 固定)
        vibtrem_table_size = def_vibtrem_table_size;
        const double mul_r = value / 16.0;
        for (int idx2 = 0; idx2 <= 7; ++idx2) {
            vibtrem_table[idx2 * 32] = 0;
            for (int idx = 1; idx <= 16; ++idx) vibtrem_table[idx2 * 32 + idx] = static_cast<uint8_t>(pround(idx * mul_r));
            for (int idx = 17; idx <= 31; ++idx) vibtrem_table[idx2 * 32 + idx] = static_cast<uint8_t>(pround((32 - idx) * mul_r));
        }
    } else {
        // カスタムテーブル (速度係数 1-4)
        const int vs = vibtab_size[value - 240];
        vibtrem_speed_factor = static_cast<uint8_t>((value - 240) % 4 + 1);
        vibtrem_table_size = static_cast<uint8_t>(2 * vs);
        const int mul_b = 256 / vs;
        for (int idx2 = 0; idx2 <= 128 / vs - 1; ++idx2) {
            vibtrem_table[(2 * vs * idx2) & 0xff] = 0;
            for (int idx = 1; idx <= vs; ++idx) {
                vibtrem_table[(2 * vs * idx2 + idx) & 0xff] = static_cast<uint8_t>(min0(idx * mul_b - 1));
            }
            for (int idx = vs + 1; idx <= 2 * vs - 1; ++idx) {
                vibtrem_table[(2 * vs * idx2 + idx) & 0xff] = static_cast<uint8_t>(min0((2 * vs - idx) * mul_b - 1));
            }
        }
    }
}

void A2Player::set_global_volume()
{
    for (uint8_t chan = 1; chan <= songdata().nm_tracks; ++chan) {
        if (_4op_vol_valid_chan(chan)) {
            set_ins_volume_4op(BYTE_NULL, chan);
        } else if (!(carrier_vol[chan] == 0 && modulator_vol[chan] == 0)) {
            if ((ins_parameter(voice_table[chan], 10) & 1) == 0) set_ins_volume(BYTE_NULL, HI(volume_table[chan]), chan);
            else set_ins_volume(LO(volume_table[chan]), HI(volume_table[chan]), chan);
        }
    }
}

// ---------------------------------------------------------------------------
// 行の処理
// ---------------------------------------------------------------------------

bool A2Player::no_loop(uint8_t current_chan, uint8_t line)
{
    for (uint8_t chan = 1; chan < current_chan; ++chan) {
        if (loop_table[chan][line] != 0 && loop_table[chan][line] != BYTE_NULL) return false;
    }
    return true;
}

void A2Player::play_line()
{
    FixedSongData& sd = songdata();
    const uint8_t nm_tracks = std::min<uint8_t>(sd.nm_tracks, 20);
    Chunk event[NCH] = {};

    if (!(pattern_break && (next_line & 0xf0) == pattern_loop_flag) && current_order != last_order) {
        std::memset(loopbck_table, BYTE_NULL, sizeof(loopbck_table));
        std::memset(loop_table, BYTE_NULL, sizeof(loop_table));
        last_order = current_order;
    }

    for (uint8_t chan = 1; chan <= nm_tracks; ++chan) {
        if (channel_flag[chan] && reset_adsrw[chan]) {
            update_modulator_adsrw(chan);
            update_carrier_adsrw(chan);
            reset_adsrw[chan] = false;
        }
    }

    for (uint8_t chan = 1; chan <= nm_tracks; ++chan) {
        Chunk& ev = event[chan];
        ev = song_->chunk(current_pattern & 0x7f, chan, current_line);

        for (int s = 0; s < 2; ++s) {
            if (effect_table[s][chan] != 0) last_effect[s][chan] = effect_table[s][chan];
            if (glfsld_table[s][chan] != 0) effect_table[s][chan] = glfsld_table[s][chan];
            else effect_table[s][chan] = effect_table[s][chan] & 0xff00;
        }
        ftune_table[chan] = 0;

        if (ev.note == BYTE_NULL) {
            ev.note = event_table[chan].note | keyoff_flag;
        } else if (ev.note >= fixed_note_flag + 1 && ev.note <= fixed_note_flag + 12 * 8 + 1) {
            ev.note = static_cast<uint8_t>(ev.note - fixed_note_flag);
        }

        event_new[chan] = ev.note != 0 || ev.effect_def != 0 || ev.effect_def2 != 0 ||
                          (ev.effect_def == 0 && ev.effect != 0) || (ev.effect_def2 == 0 && ev.effect2 != 0);

        if (ev.note != 0 || ev.instr_def != 0 || ev.effect_def + ev.effect != 0 || ev.effect_def2 + ev.effect2 != 0) {
            event_table[chan].effect_def = ev.effect_def;
            event_table[chan].effect = ev.effect;
            event_table[chan].effect_def2 = ev.effect_def2;
            event_table[chan].effect2 = ev.effect2;
        }

        if (ev.instr_def != 0) {
            static const Instrument empty{};
            if (std::memcmp(&at1(sd.instr_data, ev.instr_def), &empty, INSTRUMENT_SIZE) != 0) {
                set_ins_data(ev.instr_def, chan);
            } else {
                release_sustaining_sound(chan);
                set_ins_data(ev.instr_def, chan);
            }
        }

        for (int s = 0; s < 2; ++s) {
            const uint8_t d = fx_def(ev, s);
            if (!one_of(d, ef_Vibrato, ef_ExtraFineVibrato, ef_VibratoVolSlide, ef_VibratoVSlideFine)) vibr_table[s][chan] = {};
        }
        for (int s = 0; s < 2; ++s) {
            const uint8_t d = fx_def(ev, s);
            if (!one_of(d, ef_RetrigNote, ef_MultiRetrigNote)) retrig_table[s][chan] = 0;
        }
        for (int s = 0; s < 2; ++s) {
            const uint8_t d = fx_def(ev, s);
            if (!one_of(d, ef_Tremolo, ef_ExtraFineTremolo)) trem_table[s][chan] = {};
        }

        for (int s = 0; s < 2; ++s) {
            const uint8_t d = fx_def(ev, s);
            const uint8_t v = fx_val(ev, s);
            ArpggState& a = arpgg_table[s][chan];
            if (!((d == ef_Arpeggio && v != 0) || d == ef_ExtraFineArpeggio) && a.note != 0 && a.state != 1) {
                a.state = 1;
                change_frequency(chan, static_cast<uint16_t>(nFreq(a.note - 1) + static_cast<int8_t>(ins_parameter(event_table[chan].instr_def, 12))));
                break;   // オリジナルは else-if (エフェクト列 1 を処理したら 2 は見ない)
            }
        }

        play_line_global_fslide(0, chan, ev);
        play_line_global_fslide(1, chan, ev);
    }

    for (uint8_t chan = 1; chan <= nm_tracks; ++chan) {
        if (event_new[chan] && is_4op_chan(chan)) {
            if (in_4op_tracks_hi(chan)) event_new[chan + 1] = true;
            else event_new[chan - 1] = true;
        }

        for (int s = 0; s < 2; ++s) {
            if (tremor_table[s][chan].pos != 0 && fx_def(event[chan], s) != ef_Tremor) {
                tremor_table[s][chan].pos = 0;
                set_ins_volume(LO(tremor_table[s][chan].volume), HI(tremor_table[s][chan].volume), chan);
            }
        }

        for (int s = 0; s < 2; ++s) {
            eLo_[s][chan] = LO(last_effect[s][chan]);
            eHi_[s][chan] = HI(last_effect[s][chan]);
        }
    }

    for (int s = 0; s < 2; ++s) {
        for (uint8_t chan = 1; chan <= nm_tracks; ++chan) {
            play_line_effect(s, chan, event[chan]);
        }
    }

    for (uint8_t chan = 1; chan <= nm_tracks; ++chan) {
        const Chunk& ev = event[chan];
        const bool tporta_flag = one_of(ev.effect_def, ef_TonePortamento, ef_TPortamVolSlide, ef_TPortamVSlideFine) ||
                                 one_of(ev.effect_def2, ef_TonePortamento, ef_TPortamVolSlide, ef_TPortamVSlideFine);

        if (ev.effect_def + ev.effect == 0) {
            if (glfsld_table[0][chan] == 0) effect_table[0][chan] = 0;
        } else {
            event_table[chan].effect_def = ev.effect_def;
            event_table[chan].effect = ev.effect;
        }

        if (ev.effect_def2 + ev.effect2 == 0) {
            if (glfsld_table[1][chan] == 0) effect_table[1][chan] = 0;
        } else {
            event_table[chan].effect_def2 = ev.effect_def2;
            event_table[chan].effect2 = ev.effect2;
        }

        // トーンポルタメント系エフェクトとキーオフ・スペースキーでの単独再生の扱い
        if (ev.note & keyoff_flag) {
            key_off(chan);
        } else if (!one_of(LO(effect_table[0][chan]), ef_TonePortamento, ef_TPortamVolSlide, ef_TPortamVSlideFine, ef_NoteDelay_fx) &&
                   !one_of(LO(effect_table[1][chan]), ef_TonePortamento, ef_TPortamVolSlide, ef_TPortamVSlideFine, ef_NoteDelay_fx)) {
            const bool swap_no_restart1 = (ev.effect_def2 == ef_SwapArpeggio || ev.effect_def2 == ef_SwapVibrato) &&
                                          ev.effect_def == ef_Extended && ev.effect / 16 == ef_ex_ExtendedCmd2 &&
                                          ev.effect % 16 == ef_ex_cmd2_NoRestart;
            const bool swap_no_restart2 = (ev.effect_def == ef_SwapArpeggio || ev.effect_def == ef_SwapVibrato) &&
                                          ev.effect_def2 == ef_Extended && ev.effect2 / 16 == ef_ex_ExtendedCmd2 &&
                                          ev.effect2 % 16 == ef_ex_cmd2_NoRestart;
            if (!swap_no_restart1 && !swap_no_restart2) {
                if (!ignore_note_once[chan]) output_note(ev.note, voice_table[chan], chan, true, true);
            } else {
                output_note(ev.note, voice_table[chan], chan, true, false);
            }
        } else if (!single_play && ev.note != 0 && tporta_flag) {
            if (event_table[chan].note & keyoff_flag) {
                if (!ignore_note_once[chan]) output_note(event_table[chan].note & ~keyoff_flag, voice_table[chan], chan, false, true);
            } else if (portaFK_table[chan]) {
                output_note(event_table[chan].note & ~keyoff_flag, voice_table[chan], chan, false, true);
            } else if (ev.note != 0) {
                event_table[chan].note = ev.note;
            }
        } else if (single_play && !(ev.note & keyoff_flag) && !(event_table[chan].note != 0) &&
                   ev.instr_def != 0 && tporta_flag) {
            output_note(ev.note, ev.instr_def, chan, false, true);
        } else if (ev.note != 0) {
            if (portaFK_table[chan] && tporta_flag) output_note(ev.note, ev.instr_def, chan, false, true);
            else event_table[chan].note = ev.note;
        }

        play_line_swap_macro(0, chan, ev);
        play_line_swap_macro(1, chan, ev);

        update_fine_effects(chan);
    }

    if (tempo != 0) {
        if (pattern_delay) time_playing = time_playing + 1.0 / tempo * tickD;
        else time_playing = time_playing + 1.0 / tempo * speed;
        if (time_playing > 3600 - 1) time_playing = 0;
    }
}

void A2Player::play_line_global_fslide(int slot, uint8_t chan, const Chunk& ev)
{
    const int other = 1 - slot;
    const uint8_t d = fx_def(ev, slot);
    const uint8_t v = fx_val(ev, slot);
    const uint8_t od = fx_def(ev, other);
    const uint8_t ov = fx_val(ev, other);

    if (d != ef_GlobalFSlideUp && d != ef_GlobalFSlideDown) return;

    if (od == ef_Extended && ov == ef_ex_ExtendedCmd * 16 + ef_ex_cmd_ForceBpmSld) {
        if (d == ef_GlobalFSlideUp) update_playback_speed(v);
        else update_playback_speed(-static_cast<int32_t>(v));
        return;
    }

    uint16_t& eff = effect_table[slot][chan];
    const bool xfgfs = od == ef_Extended && ov == ef_ex_ExtendedCmd2 * 16 + ef_ex_cmd2_FTrm_XFGFS;
    const bool fgfs = od == ef_Extended && ov == ef_ex_ExtendedCmd2 * 16 + ef_ex_cmd2_FVib_FGFS;
    if (d == ef_GlobalFSlideUp) {
        if (xfgfs) eff = concw(ef_Extended2 + ef_fix2 + ef_ex2_FreqSlideUpXF, v);
        else if (fgfs) eff = concw(ef_FSlideUpFine, v);
        else eff = concw(ef_FSlideUp, v);
    } else {
        if (xfgfs) eff = concw(ef_Extended2 + ef_fix2 + ef_ex2_FreqSlideDnXF, v);
        else if (fgfs) eff = concw(ef_FSlideDownFine, v);
        else eff = concw(ef_FSlideDown, v);
    }
    for (uint8_t idx = chan; idx <= songdata().nm_tracks && idx <= 20; ++idx) {
        fslide_table[slot][idx] = v;
        glfsld_table[slot][idx] = eff;
    }
}

void A2Player::play_line_effect(int slot, uint8_t chan, const Chunk& ev)
{
    const int other = 1 - slot;
    const uint8_t d = fx_def(ev, slot);
    const uint8_t v = fx_val(ev, slot);
    const uint8_t od = fx_def(ev, other);
    const uint8_t ov = fx_val(ev, other);
    const uint8_t eLo = eLo_[slot][chan];
    const uint8_t eHi = eHi_[slot][chan];
    uint16_t& eff = effect_table[slot][chan];

    switch (d) {
    case ef_Arpeggio:
    case ef_ExtraFineArpeggio:
    case ef_ArpggVSlide:
    case ef_ArpggVSlideFine:
        if (d != ef_Arpeggio || v != 0) {
            switch (d) {
            case ef_Arpeggio:
                eff = concw(ef_Arpeggio + ef_fix1, v);
                break;
            case ef_ExtraFineArpeggio:
                eff = concw(ef_ExtraFineArpeggio, v);
                break;
            default:
                if (v != 0) eff = concw(d, v);
                else if (one_of(eLo, ef_ArpggVSlide, ef_ArpggVSlideFine) && eHi != 0) eff = concw(d, eHi);
                else eff = eff & 0xff00;
                break;
            }

            ArpggState& a = arpgg_table[slot][chan];
            if (is_note(ev.note & 0x7f)) {
                a.state = 0;
                a.note = ev.note & 0x7f;
                if (d == ef_Arpeggio || d == ef_ExtraFineArpeggio) {
                    a.add1 = v / 16;
                    a.add2 = v % 16;
                }
            } else if (ev.note == 0 && is_note(event_table[chan].note & 0x7f)) {
                if (!one_of(eLo, ef_Arpeggio + ef_fix1, ef_ExtraFineArpeggio, ef_ArpggVSlide, ef_ArpggVSlideFine)) a.state = 0;
                a.note = event_table[chan].note & 0x7f;
                if (d == ef_Arpeggio || d == ef_ExtraFineArpeggio) {
                    a.add1 = v / 16;
                    a.add2 = v % 16;
                }
            } else {
                eff = 0;
            }
        }
        break;

    case ef_FSlideUp:
    case ef_FSlideDown:
    case ef_FSlideUpFine:
    case ef_FSlideDownFine:
        eff = concw(d, v);
        fslide_table[slot][chan] = v;
        break;

    case ef_FSlideUpVSlide:
    case ef_FSlUpVSlF:
    case ef_FSlideDownVSlide:
    case ef_FSlDownVSlF:
    case ef_FSlUpFineVSlide:
    case ef_FSlUpFineVSlF:
    case ef_FSlDownFineVSlide:
    case ef_FSlDownFineVSlF:
        if (v != 0) eff = concw(d, v);
        else if (one_of(eLo, ef_FSlideUpVSlide, ef_FSlUpVSlF, ef_FSlideDownVSlide, ef_FSlDownVSlF,
                        ef_FSlUpFineVSlide, ef_FSlUpFineVSlF, ef_FSlDownFineVSlide, ef_FSlDownFineVSlF) && eHi != 0) {
            eff = concw(d, eHi);
        } else {
            eff = eff & 0xff00;
        }
        break;

    case ef_TonePortamento:
        if (is_note(ev.note)) {
            if (v != 0) eff = concw(ef_TonePortamento, v);
            else if (eLo == ef_TonePortamento && eHi != 0) eff = concw(ef_TonePortamento, eHi);
            else eff = ef_TonePortamento;

            porta_table[slot][chan].speed = HI(eff);
            porta_table[slot][chan].freq = static_cast<uint16_t>(nFreq(ev.note - 1) + static_cast<int8_t>(ins_parameter(event_table[chan].instr_def, 12)));
        } else if (eLo == ef_TonePortamento) {
            if (v != 0) eff = concw(ef_TonePortamento, v);
            else if (eLo == ef_TonePortamento && eHi != 0) eff = concw(ef_TonePortamento, eHi);
            else eff = ef_TonePortamento;
            porta_table[slot][chan].speed = HI(eff);
        }
        break;

    case ef_TPortamVolSlide:
    case ef_TPortamVSlideFine:
        if (v != 0) eff = concw(d, v);
        else if (one_of(eLo, ef_TPortamVolSlide, ef_TPortamVSlideFine) && eHi != 0) eff = concw(d, eHi);
        else eff = eff & 0xff00;
        break;

    case ef_Vibrato:
    case ef_ExtraFineVibrato:
        if (v != 0) eff = concw(d, v);
        else if (one_of(eLo, ef_Vibrato, ef_ExtraFineVibrato) && eHi != 0) eff = concw(d, eHi);
        else eff = d;

        if (od == ef_Extended && ov == ef_ex_ExtendedCmd2 * 16 + ef_ex_cmd2_FVib_FGFS) vibr_table[slot][chan].fine = true;

        vibr_table[slot][chan].speed = HI(eff) / 16;
        vibr_table[slot][chan].depth = HI(eff) % 16;
        break;

    case ef_Tremolo:
    case ef_ExtraFineTremolo:
        if (v != 0) eff = concw(d, v);
        else if (one_of(eLo, ef_Tremolo, ef_ExtraFineTremolo) && eHi != 0) eff = concw(d, eHi);
        else eff = d;

        if (od == ef_Extended && ov == ef_ex_ExtendedCmd2 * 16 + ef_ex_cmd2_FTrm_XFGFS) trem_table[slot][chan].fine = true;

        trem_table[slot][chan].speed = HI(eff) / 16;
        trem_table[slot][chan].depth = HI(eff) % 16;
        break;

    case ef_VibratoVolSlide:
    case ef_VibratoVSlideFine:
        if (v != 0) eff = concw(d, v);
        else if (one_of(eLo, ef_VibratoVolSlide, ef_VibratoVSlideFine) && HI(eff) != 0) eff = concw(d, HI(eff));
        else eff = eff & 0xff00;

        if (od == ef_Extended && ov == ef_ex_ExtendedCmd2 * 16 + ef_ex_cmd2_FVib_FGFS) vibr_table[slot][chan].fine = true;
        break;

    case ef_SetCarrierVol:
        set_ins_volume(BYTE_NULL, 63 - v, chan);
        break;

    case ef_SetModulatorVol:
        set_ins_volume(63 - v, BYTE_NULL, chan);
        break;

    case ef_SetInsVolume:
        if (_4op_vol_valid_chan(chan)) set_ins_volume_4op(63 - v, chan);
        else if (percussion_mode && is_perc_chan(chan)) set_ins_volume(63 - v, BYTE_NULL, chan);
        else if ((ins_parameter(voice_table[chan], 10) & 1) == 0) set_ins_volume(BYTE_NULL, 63 - v, chan);
        else set_ins_volume(63 - v, 63 - v, chan);
        break;

    case ef_ForceInsVolume:
        if (percussion_mode && is_perc_chan(chan)) set_ins_volume(63 - v, BYTE_NULL, chan);
        else if ((ins_parameter(voice_table[chan], 10) & 1) == 0)
            set_ins_volume(scale_volume(ins_parameter(voice_table[chan], 2) & 0x3f, 63 - v), 63 - v, chan);
        else set_ins_volume(63 - v, 63 - v, chan);
        break;

    case ef_PositionJump:
        if (no_loop(chan, current_line)) {
            pattern_break = true;
            next_line = pattern_break_flag + chan;
        }
        break;

    case ef_PatternBreak:
        if (no_loop(chan, current_line)) {
            pattern_break = true;
            seek_pattern_break = true;
            next_line = static_cast<uint8_t>(std::min<int>(v, songdata().patt_len - 1));
        }
        break;

    case ef_SetSpeed:
        speed = v;
        break;

    case ef_SetTempo:
        update_timer(v);
        break;

    case ef_SetWaveform:
        if (v / 16 <= 7) {
            fmpar_table[chan].adsrw_car.wform = v / 16;
            update_carrier_adsrw(chan);
        }
        if (v % 16 <= 7) {
            fmpar_table[chan].adsrw_mod.wform = v % 16;
            update_modulator_adsrw(chan);
        }
        break;

    case ef_VolSlide:
        eff = concw(ef_VolSlide, v);
        break;

    case ef_VolSlideFine:
        eff = concw(ef_VolSlideFine, v);
        break;

    case ef_RetrigNote:
        if (v != 0) {
            if (!one_of(eLo, ef_RetrigNote, ef_MultiRetrigNote)) retrig_table[slot][chan] = 1;
            eff = concw(ef_RetrigNote, v);
        }
        break;

    case ef_SetGlobalVolume:
        global_volume = v;
        set_global_volume();
        break;

    case ef_MultiRetrigNote:
        if (v / 16 != 0) {
            if (!one_of(eLo, ef_RetrigNote, ef_MultiRetrigNote)) retrig_table[slot][chan] = 1;
            eff = concw(ef_MultiRetrigNote, v);
        }
        break;

    case ef_Tremor:
        if (v / 16 != 0 && v % 16 != 0) {
            if (eLo != ef_Tremor) {
                tremor_table[slot][chan].pos = 0;
                tremor_table[slot][chan].volume = volume_table[chan];
            }
            eff = concw(ef_Tremor, v);
        }
        break;

    case ef_Extended:
        switch (v / 16) {
        case ef_ex_SetTremDepth:
            switch (v % 16) {
            case 0:
                opl3out(_instr[11], misc_register & 0x7f);
                current_tremolo_depth = 0;
                break;
            case 1:
                opl3out(_instr[11], misc_register | 0x80);
                current_tremolo_depth = 1;
                break;
            }
            break;

        case ef_ex_SetVibDepth:
            switch (v % 16) {
            case 0:
                opl3out(_instr[11], misc_register & 0xbf);
                current_vibrato_depth = 0;
                break;
            case 1:
                opl3out(_instr[11], misc_register | 0x40);
                current_vibrato_depth = 1;
                break;
            }
            break;

        case ef_ex_SetAttckRateM:
            fmpar_table[chan].adsrw_mod.attck = v % 16;
            update_modulator_adsrw(chan);
            break;
        case ef_ex_SetDecayRateM:
            fmpar_table[chan].adsrw_mod.dec = v % 16;
            update_modulator_adsrw(chan);
            break;
        case ef_ex_SetSustnLevelM:
            fmpar_table[chan].adsrw_mod.sustn = v % 16;
            update_modulator_adsrw(chan);
            break;
        case ef_ex_SetRelRateM:
            fmpar_table[chan].adsrw_mod.rel = v % 16;
            update_modulator_adsrw(chan);
            break;
        case ef_ex_SetAttckRateC:
            fmpar_table[chan].adsrw_car.attck = v % 16;
            update_carrier_adsrw(chan);
            break;
        case ef_ex_SetDecayRateC:
            fmpar_table[chan].adsrw_car.dec = v % 16;
            update_carrier_adsrw(chan);
            break;
        case ef_ex_SetSustnLevelC:
            fmpar_table[chan].adsrw_car.sustn = v % 16;
            update_carrier_adsrw(chan);
            break;
        case ef_ex_SetRelRateC:
            fmpar_table[chan].adsrw_car.rel = v % 16;
            update_carrier_adsrw(chan);
            break;
        case ef_ex_SetFeedback:
            fmpar_table[chan].feedb = v % 16;
            update_fmpar(chan);
            break;
        case ef_ex_SetPanningPos:
            panning_table[chan] = v % 16;
            update_fmpar(chan);
            break;

        case ef_ex_PatternLoop:
        case ef_ex_PatternLoopRec:
            if (v % 16 == 0) {
                loopbck_table[chan] = current_line;
            } else if (loopbck_table[chan] != BYTE_NULL) {
                if (loop_table[chan][current_line] == BYTE_NULL) loop_table[chan][current_line] = v % 16;
                if (loop_table[chan][current_line] != 0) {
                    pattern_break = true;
                    next_line = pattern_loop_flag + chan;
                } else if (v / 16 == ef_ex_PatternLoopRec) {
                    loop_table[chan][current_line] = BYTE_NULL;
                }
            }
            break;

        case ef_ex_ExtendedCmd:
            switch (v % 16) {
            case ef_ex_cmd_MKOffLoopDi: keyoff_loop[chan] = false; break;
            case ef_ex_cmd_MKOffLoopEn: keyoff_loop[chan] = true; break;
            case ef_ex_cmd_TPortaFKdis: portaFK_table[chan] = false; break;
            case ef_ex_cmd_TPortaFKenb: portaFK_table[chan] = true; break;
            case ef_ex_cmd_RestartEnv:
                key_on(chan);
                change_freq(chan, freq_table[chan]);
                break;
            case ef_ex_cmd_4opVlockOff:
            case ef_ex_cmd_4opVlockOn:
                if (is_4op_chan(chan)) {
                    const bool lock = (v % 16 == ef_ex_cmd_4opVlockOn);
                    vol4op_lock[chan] = lock;
                    if (in_4op_tracks_hi(chan)) vol4op_lock[chan + 1] = lock;
                    else vol4op_lock[chan - 1] = lock;
                }
                break;
            }
            break;

        case ef_ex_ExtendedCmd2:
            switch (v % 16) {
            case ef_ex_cmd2_RSS: release_sustaining_sound(chan); break;
            case ef_ex_cmd2_ResetVol: reset_ins_volume(chan); break;
            case ef_ex_cmd2_LockVol: volume_lock[chan] = true; break;
            case ef_ex_cmd2_UnlockVol: volume_lock[chan] = false; break;
            case ef_ex_cmd2_LockVP: peak_lock[chan] = true; break;
            case ef_ex_cmd2_UnlockVP: peak_lock[chan] = false; break;
            case ef_ex_cmd2_VSlide_def: volslide_type[chan] = 0; break;
            case ef_ex_cmd2_LockPan: pan_lock[chan] = true; break;
            case ef_ex_cmd2_UnlockPan: pan_lock[chan] = false; break;
            case ef_ex_cmd2_VibrOff: change_frequency(chan, freq_table[chan]); break;

            case ef_ex_cmd2_TremOff:
                // オリジナルはエフェクト列 1 では is_4op_chan、列 2 では _4op_vol_valid_chan で判定している
                if (slot == 0 ? is_4op_chan(chan) : _4op_vol_valid_chan(chan)) {
                    set_ins_volume_4op(BYTE_NULL, chan);
                } else {
                    set_ins_volume(LO(volume_table[chan]), HI(volume_table[chan]), chan);
                }
                break;

            case ef_ex_cmd2_VSlide_car:
            {
                const bool both = od == ef_Extended && ov == ef_ex_ExtendedCmd2 * 16 + ef_ex_cmd2_VSlide_mod;
                if (slot == 0) volslide_type[chan] = both ? 3 : 1;
                else if (!both) volslide_type[chan] = 1;
                break;
            }

            case ef_ex_cmd2_VSlide_mod:
            {
                const bool both = od == ef_Extended && ov == ef_ex_ExtendedCmd2 * 16 + ef_ex_cmd2_VSlide_car;
                if (slot == 0) volslide_type[chan] = both ? 3 : 2;
                else if (!both) volslide_type[chan] = 2;
                break;
            }
            }
            break;
        }
        break;

    case ef_Extended2:
        switch (v / 16) {
        case ef_ex2_PatDelayFrame:
        case ef_ex2_PatDelayRow:
            pattern_delay = true;
            if (v / 16 == ef_ex2_PatDelayFrame) tickD = v % 16;
            else tickD = speed * (v % 16);
            break;

        case ef_ex2_NoteDelay:
            eff = concw(ef_NoteDelay_fx, 0);
            notedel_table[chan] = v % 16;
            break;

        case ef_ex2_NoteCut:
            eff = concw(ef_NoteCut_fx, 0);
            notecut_table[chan] = v % 16;
            break;

        case ef_ex2_FineTuneUp:
            ftune_table[chan] = static_cast<int8_t>(ftune_table[chan] + v % 16);
            break;

        case ef_ex2_FineTuneDown:
            ftune_table[chan] = static_cast<int8_t>(ftune_table[chan] - v % 16);
            break;

        case ef_ex2_GlVolSlideUp:
        case ef_ex2_GlVolSlideDn:
        case ef_ex2_GlVolSlideUpF:
        case ef_ex2_GlVolSlideDnF:
        case ef_ex2_GlVolSldUpXF:
        case ef_ex2_GlVolSldDnXF:
        case ef_ex2_VolSlideUpXF:
        case ef_ex2_VolSlideDnXF:
        case ef_ex2_FreqSlideUpXF:
        case ef_ex2_FreqSlideDnXF:
            eff = concw(static_cast<uint8_t>(ef_Extended2 + ef_fix2 + v / 16), v % 16);
            break;
        }
        break;

    case ef_Extended3:
    {
        FmParameterTable& f = fmpar_table[chan];
        const uint8_t value = v % 16;
        bool update = true;
        switch (v / 16) {
        case ef_ex3_SetConnection: f.connect = value; break;
        case ef_ex3_SetMultipM: f.multipM = value; break;
        case ef_ex3_SetKslM: f.kslM = value; break;
        case ef_ex3_SetTremoloM: f.tremM = value; break;
        case ef_ex3_SetVibratoM: f.vibrM = value; break;
        case ef_ex3_SetKsrM: f.ksrM = value; break;
        case ef_ex3_SetSustainM: f.sustM = value; break;
        case ef_ex3_SetMultipC: f.multipC = value; break;
        case ef_ex3_SetKslC: f.kslC = value; break;
        case ef_ex3_SetTremoloC: f.tremC = value; break;
        case ef_ex3_SetVibratoC: f.vibrC = value; break;
        case ef_ex3_SetKsrC: f.ksrC = value; break;
        case ef_ex3_SetSustainC: f.sustC = value; break;
        default: update = false; break;
        }
        if (update) update_fmpar(chan);
        break;
    }
    }
}

void A2Player::play_line_swap_macro(int slot, uint8_t chan, const Chunk& ev)
{
    const int other = 1 - slot;
    const uint8_t d = fx_def(ev, slot);
    const uint8_t v = fx_val(ev, slot);
    const uint8_t od = fx_def(ev, other);
    const uint8_t ov = fx_val(ev, other);
    const bool no_restart = od == ef_Extended && ov / 16 == ef_ex_ExtendedCmd2 && ov % 16 == ef_ex_cmd2_NoRestart;
    MacroState& m = macro_table[chan];

    switch (d) {
    case ef_SwapArpeggio:
        if (no_restart) {
            const uint8_t length = at1(songdata().macro_table, v).arpeggio.length;
            if (m.arpg_pos > length) m.arpg_pos = length;
            m.arpg_table = v;
        } else {
            m.arpg_count = 1;
            m.arpg_pos = 0;
            m.arpg_table = v;
            m.arpg_note = event_table[chan].note;
        }
        break;

    case ef_SwapVibrato:
        if (no_restart) {
            // オリジナルは vib_pos ではなく vib_table と長さを比較している
            const uint8_t length = at1(songdata().macro_table, v).vibrato.length;
            if (m.vib_table > length) m.vib_pos = length;
            m.vib_table = v;
        } else {
            m.vib_count = 1;
            m.vib_pos = 0;
            m.vib_table = v;
            m.vib_delay = at1(songdata().macro_table, m.vib_table).vibrato.delay;
        }
        break;

    case ef_SetCustomSpeedTab:
        generate_custom_vibrato(v);
        break;
    }
}

// ---------------------------------------------------------------------------
// エフェクトの更新
// ---------------------------------------------------------------------------

void A2Player::portamento_up(uint8_t chan, uint16_t slide, uint16_t limit)
{
    if ((freq_table[chan] & 0x1fff) == 0) return;
    const uint16_t freq = calc_freq_shift_up(freq_table[chan] & 0x1fff, slide);
    if (freq <= limit) change_frequency(chan, freq);
    else change_frequency(chan, limit);
}

void A2Player::portamento_down(uint8_t chan, uint16_t slide, uint16_t limit)
{
    if ((freq_table[chan] & 0x1fff) == 0) return;
    const uint16_t freq = calc_freq_shift_down(freq_table[chan] & 0x1fff, slide);
    if (freq >= limit) change_frequency(chan, freq);
    else change_frequency(chan, limit);
}

void A2Player::macro_vibrato__porta_up(uint8_t chan, uint8_t depth)
{
    const uint16_t freq = calc_freq_shift_up(macro_table[chan].vib_freq & 0x1fff, depth);
    if (freq <= nFreq(12 * 8 + 1)) change_freq(chan, freq);
    else change_freq(chan, nFreq(12 * 8 + 1));
}

void A2Player::macro_vibrato__porta_down(uint8_t chan, uint8_t depth)
{
    const uint16_t freq = calc_freq_shift_down(macro_table[chan].vib_freq & 0x1fff, depth);
    if (freq >= nFreq(0)) change_freq(chan, freq);
    else change_freq(chan, nFreq(0));
}

void A2Player::tone_portamento(int slot, uint8_t chan)
{
    const PortaState& p = porta_table[slot][chan];
    if ((freq_table[chan] & 0x1fff) > p.freq) portamento_down(chan, p.speed, p.freq);
    else if ((freq_table[chan] & 0x1fff) < p.freq) portamento_up(chan, p.speed, p.freq);
}

void A2Player::slide_carrier_volume_up(uint8_t chan, uint8_t slide, uint8_t limit)
{
    const uint8_t vLo = LO(volume_table[chan]);
    const uint8_t vHi = HI(volume_table[chan]);
    uint16_t vol;
    if (vHi - slide >= limit) vol = concw(vLo, static_cast<uint8_t>(vHi - slide));
    else vol = concw(vLo, limit);
    set_ins_volume(BYTE_NULL, HI(vol), chan);
    volume_table[chan] = vol;
}

void A2Player::slide_modulator_volume_up(uint8_t chan, uint8_t slide, uint8_t limit)
{
    const uint8_t vLo = LO(volume_table[chan]);
    const uint8_t vHi = HI(volume_table[chan]);
    uint16_t vol;
    if (vLo - slide >= limit) vol = concw(static_cast<uint8_t>(vLo - slide), vHi);
    else vol = concw(limit, vHi);
    set_ins_volume(LO(vol), BYTE_NULL, chan);
    volume_table[chan] = vol;
}

void A2Player::slide_volume_up(uint8_t chan, uint8_t slide)
{
    const FourOpData d = _4op_data_flag(chan);
    uint8_t limit1 = 0, limit2 = 0;
    uint16_t limit1_4op = 0, limit2_4op = 0;

    if (_4op_vol_valid_chan(chan)) {
        // オリジナルは Word 型に (car SHL 16) + mod を代入しているため、上位 (キャリア側) は常に 0 になる
        if (peak_lock[d.ch1]) limit1_4op = static_cast<uint16_t>(((ins_parameter(d.ins1, 3) & 0x3f) << 16) + (ins_parameter(d.ins1, 2) & 0x3f));
        if (peak_lock[d.ch2]) limit2_4op = static_cast<uint16_t>(((ins_parameter(d.ins2, 3) & 0x3f) << 16) + (ins_parameter(d.ins2, 2) & 0x3f));
    } else {
        if (peak_lock[chan]) limit1 = ins_parameter(event_table[chan].instr_def, 3) & 0x3f;
        if (peak_lock[chan]) limit2 = ins_parameter(event_table[chan].instr_def, 2) & 0x3f;
    }

    switch (volslide_type[chan]) {
    case 0:
        if (!_4op_vol_valid_chan(chan)) {
            slide_carrier_volume_up(chan, slide, limit1);
            if ((ins_parameter(voice_table[chan], 10) & 1) == 1 || (percussion_mode && is_perc_chan(chan))) {
                slide_modulator_volume_up(chan, slide, limit2);
            }
        } else {
            switch (d.conn) {
            case 0:   // FM/FM
                slide_carrier_volume_up(d.ch1, slide, HI(limit1_4op));
                break;
            case 1:   // FM/AM
                slide_carrier_volume_up(d.ch1, slide, HI(limit1_4op));
                slide_modulator_volume_up(d.ch2, slide, LO(limit2_4op));
                break;
            case 2:   // AM/FM
                slide_carrier_volume_up(d.ch1, slide, HI(limit1_4op));
                slide_carrier_volume_up(d.ch2, slide, HI(limit2_4op));
                break;
            case 3:   // AM/AM
                slide_carrier_volume_up(d.ch1, slide, HI(limit1_4op));
                slide_modulator_volume_up(d.ch1, slide, LO(limit1_4op));
                slide_modulator_volume_up(d.ch2, slide, LO(limit2_4op));
                break;
            }
        }
        break;
    case 1:
        slide_carrier_volume_up(chan, slide, limit1);
        break;
    case 2:
        slide_modulator_volume_up(chan, slide, limit2);
        break;
    case 3:
        slide_carrier_volume_up(chan, slide, limit1);
        slide_modulator_volume_up(chan, slide, limit2);
        break;
    }
}

void A2Player::slide_carrier_volume_down(uint8_t chan, uint8_t slide)
{
    const uint8_t vLo = LO(volume_table[chan]);
    const uint8_t vHi = HI(volume_table[chan]);
    uint16_t vol;
    if (vHi + slide <= 63) vol = concw(vLo, static_cast<uint8_t>(vHi + slide));
    else vol = concw(vLo, 63);
    set_ins_volume(BYTE_NULL, HI(vol), chan);
    volume_table[chan] = vol;
}

void A2Player::slide_modulator_volume_down(uint8_t chan, uint8_t slide)
{
    const uint8_t vLo = LO(volume_table[chan]);
    const uint8_t vHi = HI(volume_table[chan]);
    uint16_t vol;
    if (vLo + slide <= 63) vol = concw(static_cast<uint8_t>(vLo + slide), vHi);
    else vol = concw(63, vHi);
    set_ins_volume(LO(vol), BYTE_NULL, chan);
    volume_table[chan] = vol;
}

void A2Player::slide_volume_down(uint8_t chan, uint8_t slide)
{
    const FourOpData d = _4op_data_flag(chan);

    switch (volslide_type[chan]) {
    case 0:
        if (!_4op_vol_valid_chan(chan)) {
            slide_carrier_volume_down(chan, slide);
            if ((ins_parameter(voice_table[chan], 10) & 1) == 1 || (percussion_mode && is_perc_chan(chan))) {
                slide_modulator_volume_down(chan, slide);
            }
        } else {
            switch (d.conn) {
            case 0:   // FM/FM
                slide_carrier_volume_down(d.ch1, slide);
                break;
            case 1:   // FM/AM
                slide_carrier_volume_down(d.ch1, slide);
                slide_modulator_volume_down(d.ch2, slide);
                break;
            case 2:   // AM/FM
                slide_carrier_volume_down(d.ch1, slide);
                slide_carrier_volume_down(d.ch2, slide);
                break;
            case 3:   // AM/AM
                slide_carrier_volume_down(d.ch1, slide);
                slide_modulator_volume_down(d.ch1, slide);
                slide_modulator_volume_down(d.ch2, slide);
                break;
            }
        }
        break;
    case 1:
        slide_carrier_volume_down(chan, slide);
        break;
    case 2:
        slide_modulator_volume_down(chan, slide);
        break;
    case 3:
        slide_carrier_volume_down(chan, slide);
        slide_modulator_volume_down(chan, slide);
        break;
    }
}

void A2Player::volume_slide(uint8_t chan, uint8_t up_speed, uint8_t down_speed)
{
    if (up_speed != 0) slide_volume_up(chan, up_speed);
    else if (down_speed != 0) slide_volume_down(chan, down_speed);
}

void A2Player::global_volume_slide(uint8_t up_speed, uint8_t down_speed)
{
    if (up_speed != BYTE_NULL) global_volume = static_cast<uint8_t>(std::min(global_volume + up_speed, 63));
    if (down_speed != BYTE_NULL) {
        if (global_volume >= down_speed) global_volume = static_cast<uint8_t>(global_volume - down_speed);
        else global_volume = 0;
    }
    set_global_volume();
}

void A2Player::arpeggio(int slot, uint8_t chan)
{
    static const uint8_t arpgg_state[3] = { 1, 2, 0 };
    ArpggState& a = arpgg_table[slot][chan];

    uint16_t freq = 0;
    switch (a.state) {
    case 0: freq = nFreq(static_cast<uint8_t>(a.note - 1)); break;
    case 1: freq = nFreq(static_cast<uint8_t>(a.note - 1 + a.add1)); break;
    case 2: freq = nFreq(static_cast<uint8_t>(a.note - 1 + a.add2)); break;
    }

    a.state = a.state <= 2 ? arpgg_state[a.state] : 0;
    change_frequency(chan, static_cast<uint16_t>(freq + static_cast<int8_t>(ins_parameter(event_table[chan].instr_def, 12))));
}

void A2Player::vibrato(int slot, uint8_t chan)
{
    VibTremState& v = vibr_table[slot][chan];
    v.pos = static_cast<uint8_t>(v.pos + v.speed * vibtrem_speed_factor);
    const uint16_t freq = calc_vibtrem_shift(v);
    const uint16_t old_freq = freq_table[chan];
    if (v.dir == 0) portamento_down(chan, freq, nFreq(0));
    else portamento_up(chan, freq, nFreq(12 * 8 + 1));
    freq_table[chan] = old_freq;
}

void A2Player::tremolo(int slot, uint8_t chan)
{
    VibTremState& t = trem_table[slot][chan];
    t.pos = static_cast<uint8_t>(t.pos + t.speed * vibtrem_speed_factor);
    const uint16_t vol = calc_vibtrem_shift(t);
    const uint16_t old_vol = volume_table[chan];
    // オリジナルの tremolo2 は dir ではなく pos = 0 で向きを判定している
    const bool down = (slot == 0) ? (t.dir == 0) : (t.pos == 0);
    if (down) slide_volume_down(chan, static_cast<uint8_t>(vol));
    else slide_volume_up(chan, static_cast<uint8_t>(vol));
    volume_table[chan] = old_vol;
}

uint8_t A2Player::chanvol(uint8_t chan)
{
    if ((ins_parameter(voice_table[chan], 10) & 1) == 0) return static_cast<uint8_t>(63 - HI(volume_table[chan]));
    return static_cast<uint8_t>(63 - pround((LO(volume_table[chan]) + HI(volume_table[chan])) / 2.0));
}

void A2Player::update_effects()
{
    for (uint8_t chan = 1; chan <= songdata().nm_tracks && chan <= 20; ++chan) {
        update_effects_slot(0, chan);
        update_effects_slot(1, chan);
    }
}

void A2Player::update_effects_slot(int slot, uint8_t chan)
{
    const uint8_t eLo = LO(effect_table[slot][chan]);
    const uint8_t eHi = HI(effect_table[slot][chan]);

    switch (eLo) {
    case ef_Arpeggio + ef_fix1:
        arpeggio(slot, chan);
        break;

    case ef_ArpggVSlide:
        volume_slide(chan, eHi / 16, eHi % 16);
        arpeggio(slot, chan);
        break;

    case ef_ArpggVSlideFine:
        arpeggio(slot, chan);
        break;

    case ef_FSlideUp:
        portamento_up(chan, eHi, nFreq(12 * 8 + 1));
        break;

    case ef_FSlideDown:
        portamento_down(chan, eHi, nFreq(0));
        break;

    case ef_FSlideUpVSlide:
        portamento_up(chan, fslide_table[slot][chan], nFreq(12 * 8 + 1));
        volume_slide(chan, eHi / 16, eHi % 16);
        break;

    case ef_FSlUpVSlF:
        portamento_up(chan, fslide_table[slot][chan], nFreq(12 * 8 + 1));
        break;

    case ef_FSlideDownVSlide:
        portamento_down(chan, fslide_table[slot][chan], nFreq(0));
        volume_slide(chan, eHi / 16, eHi % 16);
        break;

    case ef_FSlDownVSlF:
        portamento_down(chan, fslide_table[slot][chan], nFreq(0));
        break;

    case ef_FSlUpFineVSlide:
    case ef_FSlDownFineVSlide:
        volume_slide(chan, eHi / 16, eHi % 16);
        break;

    case ef_TonePortamento:
        tone_portamento(slot, chan);
        break;

    case ef_TPortamVolSlide:
        volume_slide(chan, eHi / 16, eHi % 16);
        tone_portamento(slot, chan);
        break;

    case ef_TPortamVSlideFine:
        tone_portamento(slot, chan);
        break;

    case ef_Vibrato:
        if (!vibr_table[slot][chan].fine) vibrato(slot, chan);
        break;

    case ef_Tremolo:
        if (!trem_table[slot][chan].fine) tremolo(slot, chan);
        break;

    case ef_VibratoVolSlide:
        volume_slide(chan, eHi / 16, eHi % 16);
        if (!vibr_table[slot][chan].fine) vibrato(slot, chan);
        break;

    case ef_VibratoVSlideFine:
        if (!vibr_table[slot][chan].fine) vibrato(slot, chan);
        break;

    case ef_VolSlide:
        volume_slide(chan, eHi / 16, eHi % 16);
        break;

    case ef_RetrigNote:
        if (retrig_table[slot][chan] >= eHi) {
            retrig_table[slot][chan] = 0;
            output_note(event_table[chan].note, event_table[chan].instr_def, chan, true, true);
        } else {
            ++retrig_table[slot][chan];
        }
        break;

    case ef_MultiRetrigNote:
        if (retrig_table[slot][chan] >= eHi / 16) {
            switch (eHi % 16) {
            case 0:
            case 8:
                break;
            case 1: slide_volume_down(chan, 1); break;
            case 2: slide_volume_down(chan, 2); break;
            case 3: slide_volume_down(chan, 4); break;
            case 4: slide_volume_down(chan, 8); break;
            case 5: slide_volume_down(chan, 16); break;
            case 9: slide_volume_up(chan, 1); break;
            case 10: slide_volume_up(chan, 2); break;
            case 11: slide_volume_up(chan, 4); break;
            case 12: slide_volume_up(chan, 8); break;
            case 13: slide_volume_up(chan, 16); break;
            case 6: slide_volume_down(chan, static_cast<uint8_t>(chanvol(chan) - pround(chanvol(chan) * 2 / 3.0))); break;
            case 7: slide_volume_down(chan, static_cast<uint8_t>(chanvol(chan) - pround(chanvol(chan) * 1 / 2.0))); break;
            case 14: slide_volume_up(chan, static_cast<uint8_t>(std::min(pround(chanvol(chan) * 3 / 2.0) - chanvol(chan), 63))); break;
            case 15: slide_volume_up(chan, static_cast<uint8_t>(std::min(pround(chanvol(chan) * 2.0) - chanvol(chan), 63))); break;
            }

            retrig_table[slot][chan] = 0;
            output_note(event_table[chan].note, event_table[chan].instr_def, chan, true, true);
        } else {
            ++retrig_table[slot][chan];
        }
        break;

    case ef_Tremor:
    {
        TremorState& t = tremor_table[slot][chan];
        if (t.pos >= 0) {
            if (t.pos + 1 <= eHi / 16) {
                ++t.pos;
            } else {
                slide_volume_down(chan, 63);
                t.pos = -1;
            }
        } else if (t.pos - 1 >= -(eHi % 16)) {
            --t.pos;
        } else {
            set_ins_volume(LO(t.volume), HI(t.volume), chan);
            t.pos = 1;
        }
        break;
    }

    case ef_NoteDelay_fx:
        if (notedel_table[chan] == 0) {
            notedel_table[chan] = BYTE_NULL;
            output_note(event_table[chan].note, event_table[chan].instr_def, chan, true, true);
        } else {
            --notedel_table[chan];
        }
        break;

    case ef_NoteCut_fx:
        if (notecut_table[chan] == 0) {
            notecut_table[chan] = BYTE_NULL;
            key_off(chan);
        } else {
            --notecut_table[chan];
        }
        break;

    case ef_Extended2 + ef_fix2 + ef_ex2_GlVolSlideUp:
        global_volume_slide(eHi, BYTE_NULL);
        break;

    case ef_Extended2 + ef_fix2 + ef_ex2_GlVolSlideDn:
        global_volume_slide(BYTE_NULL, eHi);
        break;
    }
}

void A2Player::update_fine_effects(uint8_t chan)
{
    update_fine_effects_slot(0, chan);
    update_fine_effects_slot(1, chan);
}

void A2Player::update_fine_effects_slot(int slot, uint8_t chan)
{
    const uint8_t eLo = LO(effect_table[slot][chan]);
    const uint8_t eHi = HI(effect_table[slot][chan]);

    switch (eLo) {
    case ef_ArpggVSlideFine:
        volume_slide(chan, eHi / 16, eHi % 16);
        break;

    case ef_FSlideUpFine:
        portamento_up(chan, eHi, nFreq(12 * 8 + 1));
        break;

    case ef_FSlideDownFine:
        portamento_down(chan, eHi, nFreq(0));
        break;

    case ef_FSlUpVSlF:
    case ef_FSlDownVSlF:
        volume_slide(chan, eHi / 16, eHi % 16);
        break;

    case ef_FSlUpFineVSlide:
        portamento_up(chan, fslide_table[slot][chan], nFreq(12 * 8 + 1));
        break;

    case ef_FSlUpFineVSlF:
        portamento_up(chan, fslide_table[slot][chan], nFreq(12 * 8 + 1));
        volume_slide(chan, eHi / 16, eHi % 16);
        break;

    case ef_FSlDownFineVSlide:
        portamento_down(chan, fslide_table[slot][chan], nFreq(0));
        break;

    case ef_FSlDownFineVSlF:
        portamento_down(chan, fslide_table[slot][chan], nFreq(0));
        volume_slide(chan, eHi / 16, eHi % 16);
        break;

    case ef_TPortamVSlideFine:
        volume_slide(chan, eHi / 16, eHi % 16);
        break;

    case ef_Vibrato:
        if (vibr_table[slot][chan].fine) vibrato(slot, chan);
        break;

    case ef_Tremolo:
        if (trem_table[slot][chan].fine) tremolo(slot, chan);
        break;

    case ef_VibratoVolSlide:
        if (vibr_table[slot][chan].fine) vibrato(slot, chan);
        break;

    case ef_VibratoVSlideFine:
        volume_slide(chan, eHi / 16, eHi % 16);
        if (vibr_table[slot][chan].fine) vibrato(slot, chan);
        break;

    case ef_VolSlideFine:
        volume_slide(chan, eHi / 16, eHi % 16);
        break;

    case ef_Extended2 + ef_fix2 + ef_ex2_GlVolSlideUpF:
        global_volume_slide(eHi, BYTE_NULL);
        break;

    case ef_Extended2 + ef_fix2 + ef_ex2_GlVolSlideDnF:
        global_volume_slide(BYTE_NULL, eHi);
        break;
    }
}

void A2Player::update_extra_fine_effects()
{
    for (uint8_t chan = 1; chan <= songdata().nm_tracks && chan <= 20; ++chan) {
        update_extra_fine_effects_slot(0, chan);
        update_extra_fine_effects_slot(1, chan);
    }
}

void A2Player::update_extra_fine_effects_slot(int slot, uint8_t chan)
{
    const uint8_t eLo = LO(effect_table[slot][chan]);
    const uint8_t eHi = HI(effect_table[slot][chan]);

    switch (eLo) {
    case ef_Extended2 + ef_fix2 + ef_ex2_GlVolSldUpXF:
        global_volume_slide(eHi, BYTE_NULL);
        break;

    case ef_Extended2 + ef_fix2 + ef_ex2_GlVolSldDnXF:
        global_volume_slide(BYTE_NULL, eHi);
        break;

    case ef_Extended2 + ef_fix2 + ef_ex2_VolSlideUpXF:
        volume_slide(chan, eHi, 0);
        break;

    case ef_Extended2 + ef_fix2 + ef_ex2_VolSlideDnXF:
        volume_slide(chan, 0, eHi);
        break;

    case ef_Extended2 + ef_fix2 + ef_ex2_FreqSlideUpXF:
        portamento_up(chan, eHi, nFreq(12 * 8 + 1));
        break;

    case ef_Extended2 + ef_fix2 + ef_ex2_FreqSlideDnXF:
        portamento_down(chan, eHi, nFreq(0));
        break;

    case ef_ExtraFineArpeggio:
        arpeggio(slot, chan);
        break;

    case ef_ExtraFineVibrato:
        if (!vibr_table[slot][chan].fine) vibrato(slot, chan);
        break;

    case ef_ExtraFineTremolo:
        if (!trem_table[slot][chan].fine) tremolo(slot, chan);
        break;
    }
}

// ---------------------------------------------------------------------------
// 演奏位置
// ---------------------------------------------------------------------------

int A2Player::calc_following_order(uint8_t order)
{
    int result = -1;
    uint8_t index = order;
    int jump_count = 0;

    do {
        if (order_at(index) < 0x80) {
            result = index;
        } else {
            index = static_cast<uint8_t>(order_at(index) - 0x80);
            ++jump_count;
        }
    } while (!(jump_count > 0x7f || result != -1));

    return result;
}

int A2Player::calc_order_jump()
{
    int result = 0;
    int temp = 0;

    do {
        if (order_at(current_order) > 0x7f) {
            current_order = static_cast<uint8_t>(order_at(current_order) - 0x80);
        }
        ++temp;
    } while (!(temp > 0x7f || order_at(current_order) < 0x80));

    if (temp > 0x7f) {
        stopPlaying();
        result = -1;
    }
    return result;
}

uint8_t A2Player::calc_pattern_pos(uint8_t pattern)
{
    uint8_t pattern_pos = BYTE_NULL;
    int jump_count = 0;
    int index = calc_following_order(0);
    while (index != -1 && jump_count < 0x7f) {
        if (order_at(static_cast<uint8_t>(index)) != pattern) {
            if (!(index < 0x7f)) break;
            ++index;
            index = calc_following_order(static_cast<uint8_t>(index));
            ++jump_count;
        } else {
            pattern_pos = static_cast<uint8_t>(index);
            break;
        }
    }
    return pattern_pos;
}

void A2Player::update_song_position()
{
    FixedSongData& sd = songdata();

    for (int temp = 9; temp >= 2; --temp) play_pos_buf[temp] = play_pos_buf[temp - 1];
    play_pos_buf[1] = static_cast<uint16_t>((current_pattern << 8) + current_line);

    if (!rewind) {
        if (current_line < sd.patt_len - 1 && !pattern_break) {
            ++current_line;
        } else {
            if (!(pattern_break && (next_line & 0xf0) == pattern_loop_flag) && repeat_pattern) {
                std::memset(loopbck_table, BYTE_NULL, sizeof(loopbck_table));
                std::memset(loop_table, BYTE_NULL, sizeof(loop_table));
                current_line = 0;
                pattern_break = false;
            } else {
                if (!(pattern_break && (next_line & 0xf0) == pattern_loop_flag) && current_order < 0x7f) {
                    std::memset(loopbck_table, BYTE_NULL, sizeof(loopbck_table));
                    std::memset(loop_table, BYTE_NULL, sizeof(loop_table));
                    ++current_order;
                }

                if (pattern_break && (next_line & 0xf0) == pattern_loop_flag) {
                    const uint8_t temp = next_line - pattern_loop_flag;
                    next_line = loopbck_table[temp];
                    if (loop_table[temp][current_line] != 0) --loop_table[temp][current_line];
                } else if (pattern_break && (next_line & 0xf0) == pattern_break_flag) {
                    const uint8_t ch = next_line - pattern_break_flag;
                    if (event_table[ch].effect_def2 == ef_PositionJump) current_order = event_table[ch].effect2;
                    else current_order = event_table[ch].effect;
                    pattern_break = false;
                } else if (current_order > 0x7f) {
                    current_order = 0;
                }

                if (!play_single_patt) {
                    if (order_at(current_order) > 0x7f) {
                        if (calc_order_jump() == -1) return;
                    }
                }

                if (!play_single_patt) current_pattern = order_at(current_order);

                if (!pattern_break) {
                    current_line = 0;
                } else {
                    pattern_break = false;
                    current_line = next_line;
                }
            }
        }
    } else if (current_line > 0) {
        --current_line;
    }

    for (uint8_t temp = 1; temp <= sd.nm_tracks && temp <= 20; ++temp) {
        ignore_note_once[temp] = false;
        glfsld_table[0][temp] = 0;
        glfsld_table[1][temp] = 0;
    }

    if (!play_single_patt) {
        if (current_line == 0 && current_order == calc_following_order(0) && speed_update) {
            tempo = sd.tempo;
            speed = sd.speed;
            update_timer(tempo);
        }
    }
}

void A2Player::poll_proc()
{
    FixedSongData& sd = songdata();

    if ((!pattern_delay && ticks - tick0 + 1 >= speed) || fast_forward || rewind || single_play) {
        if (debugging && !single_play && !pattern_break && (!space_pressed || no_step_debugging)) return;

        if (!single_play && !play_single_patt) {
            if (order_at(current_order) > 0x7f) {
                if (calc_order_jump() == -1) return;
            }
            current_pattern = order_at(current_order);
        }

        play_line();
        if (!single_play && !(fast_forward || rewind)) {
            update_effects();
        } else {
            for (int temp = 1; temp <= speed; ++temp) {
                update_effects();
                if (temp % 4 == temp) update_extra_fine_effects();
                ++ticks;
            }
        }

        tick0 = ticks;
        if (!single_play && (fast_forward || rewind || !pattern_delay)) update_song_position();

        if (fast_forward || rewind) {
            if (!pattern_delay) synchronize_song_timer();
        }

        if ((fast_forward || rewind) && pattern_delay) {
            tickD = 0;
            pattern_delay = false;
        }

        if (fast_forward || rewind) {
            const double factor = fast_forward ? 1.0 / (5 - fforward_factor + 1) : 1.0 / (5 - rewind_factor + 1);
            if (speed != 0 && std::abs(time_playing - time_playing0) > factor * (1 + (1.0 / 255 * tempo)) / speed) {
                fast_forward = false;
                rewind = false;
                time_playing0 = time_playing;
                synchronize_song_timer();
            }
        }
    } else {
        update_effects();
        ++ticks;

        if (!(debugging && !single_play && (!space_pressed || no_step_debugging))) {
            if (pattern_delay && tickD > 1) {
                --tickD;
            } else {
                if (pattern_delay && !single_play) {
                    tick0 = ticks;
                    update_song_position();
                }
                pattern_delay = false;
            }
        }
    }

    ++tickXF;
    if (tickXF % 4 == 0) {
        update_extra_fine_effects();
        tickXF -= 4;
    }
}

void A2Player::macro_poll_proc()
{
    constexpr uint16_t IDLE = 0x0fff;
    constexpr uint16_t FINISHED = 0xffff;
    FixedSongData& sd = songdata();

    for (uint8_t chan = 1; chan <= sd.nm_tracks && chan <= 20; ++chan) {
        const uint16_t finished_flag = !keyoff_loop[chan] ? FINISHED : IDLE;
        MacroState& m = macro_table[chan];
        const bool key_is_on = (freq_table[chan] | 0x2000) == freq_table[chan];

        // FM レジスタマクロ (instr_macros にはテーブル固有の速度が無いため、曲の speed で判定している)
        if (m.fmreg_table != 0 && speed != 0) {
            const RegisterTable& rt = at1(sd.instr_macros, m.fmreg_table);
            if (m.fmreg_duration > 1) {
                --m.fmreg_duration;
            } else {
                m.fmreg_count = 1;
                if (m.fmreg_pos <= rt.length) {
                    if (rt.loop_begin != 0 && rt.loop_length != 0) {
                        if (m.fmreg_pos == rt.loop_begin + (rt.loop_length - 1)) m.fmreg_pos = rt.loop_begin;
                        else if (m.fmreg_pos < rt.length) ++m.fmreg_pos;
                        else m.fmreg_pos = finished_flag;
                    } else if (m.fmreg_pos < rt.length) {
                        ++m.fmreg_pos;
                    } else {
                        m.fmreg_pos = finished_flag;
                    }
                } else {
                    m.fmreg_pos = finished_flag;
                }

                if (key_is_on && rt.keyoff_pos != 0 && m.fmreg_pos >= rt.keyoff_pos) {
                    m.fmreg_pos = IDLE;
                } else if (!key_is_on && m.fmreg_pos != 0 && rt.keyoff_pos != 0 &&
                           (m.fmreg_pos < rt.keyoff_pos || m.fmreg_pos == IDLE)) {
                    m.fmreg_pos = rt.keyoff_pos;
                }

                if (m.fmreg_pos != 0 && m.fmreg_pos != IDLE && m.fmreg_pos != finished_flag) {
                    const RegisterTableDef& def = at1(rt.data, m.fmreg_pos);
                    m.fmreg_duration = def.duration;
                    if (m.fmreg_duration != 0) {
                        const FmInstData& fm = def.fm_data;
                        const DisFmregCol& dis = at1(sd.dis_fmreg_col, m.fmreg_table);
                        FmParameterTable& f = fmpar_table[chan];

                        // ADSR データの無い音色ではキーオンを強制する
                        force_macro_keyon = false;
                        if (m.fmreg_pos == 1) {
                            if (is_ins_adsr_data_empty(voice_table[chan]) &&
                                !(dis[0] && dis[1] && dis[2] && dis[3] && dis[12] && dis[13] && dis[14] && dis[15])) {
                                force_macro_keyon = true;
                            }
                        }

                        if (!dis[0]) f.adsrw_mod.attck = fm.ATTCK_DEC_modulator >> 4;
                        if (!dis[1]) f.adsrw_mod.dec = fm.ATTCK_DEC_modulator & 0x0f;
                        if (!dis[2]) f.adsrw_mod.sustn = fm.SUSTN_REL_modulator >> 4;
                        if (!dis[3]) f.adsrw_mod.rel = fm.SUSTN_REL_modulator & 0x0f;
                        if (!dis[4]) f.adsrw_mod.wform = fm.WAVEFORM_modulator & 0x07;
                        if (!dis[6]) f.kslM = fm.KSL_VOLUM_modulator >> 6;
                        if (!dis[7]) f.multipM = fm.AM_VIB_EG_modulator & 0x0f;
                        if (!dis[8]) f.tremM = fm.AM_VIB_EG_modulator >> 7;
                        if (!dis[9]) f.vibrM = (fm.AM_VIB_EG_modulator >> 6) & 1;
                        if (!dis[10]) f.ksrM = (fm.AM_VIB_EG_modulator >> 4) & 1;
                        if (!dis[11]) f.sustM = (fm.AM_VIB_EG_modulator >> 5) & 1;
                        if (!dis[12]) f.adsrw_car.attck = fm.ATTCK_DEC_carrier >> 4;
                        if (!dis[13]) f.adsrw_car.dec = fm.ATTCK_DEC_carrier & 0x0f;
                        if (!dis[14]) f.adsrw_car.sustn = fm.SUSTN_REL_carrier >> 4;
                        if (!dis[15]) f.adsrw_car.rel = fm.SUSTN_REL_carrier & 0x0f;
                        if (!dis[16]) f.adsrw_car.wform = fm.WAVEFORM_carrier & 0x07;
                        if (!dis[18]) f.kslC = fm.KSL_VOLUM_carrier >> 6;
                        if (!dis[19]) f.multipC = fm.AM_VIB_EG_carrier & 0x0f;
                        if (!dis[20]) f.tremC = fm.AM_VIB_EG_carrier >> 7;
                        if (!dis[21]) f.vibrC = (fm.AM_VIB_EG_carrier >> 6) & 1;
                        if (!dis[22]) f.ksrC = (fm.AM_VIB_EG_carrier >> 4) & 1;
                        if (!dis[23]) f.sustC = (fm.AM_VIB_EG_carrier >> 5) & 1;
                        if (!dis[24]) f.connect = fm.FEEDBACK_FM & 1;
                        if (!dis[25]) f.feedb = (fm.FEEDBACK_FM >> 1) & 7;
                        if (!dis[27]) {
                            if (!pan_lock[chan]) panning_table[chan] = def.panning;
                        }

                        if (!dis[5]) set_ins_volume(63 - (fm.KSL_VOLUM_modulator & 0x3f), BYTE_NULL, chan);
                        if (!dis[17]) set_ins_volume(BYTE_NULL, 63 - (fm.KSL_VOLUM_carrier & 0x3f), chan);

                        update_modulator_adsrw(chan);
                        update_carrier_adsrw(chan);
                        update_fmpar(chan);

                        if (force_macro_keyon || (fm.FEEDBACK_FM & MACRO_NOTE_RETRIG_FLAG)) {
                            if (!(is_4op_chan(chan) && in_4op_tracks_hi(chan))) {
                                output_note(event_table[chan].note, event_table[chan].instr_def, chan, false, true);
                                if (is_4op_chan(chan) && in_4op_tracks_lo(chan)) {
                                    init_macro_table(chan - 1, 0, voice_table[chan - 1], 0);
                                }
                            }
                        } else if (fm.FEEDBACK_FM & MACRO_ENVELOPE_RESTART_FLAG) {
                            key_on(chan);
                            change_freq(chan, freq_table[chan]);
                        } else if (fm.FEEDBACK_FM & MACRO_ZERO_FREQ_FLAG) {
                            if (freq_table[chan] != 0) {
                                zero_fq_table[chan] = freq_table[chan];
                                freq_table[chan] = freq_table[chan] & ~0x1fff;
                                change_freq(chan, freq_table[chan]);
                            }
                        } else if (zero_fq_table[chan] != 0) {
                            freq_table[chan] = zero_fq_table[chan];
                            zero_fq_table[chan] = 0;
                            change_freq(chan, freq_table[chan]);
                        }

                        if (!dis[26]) {
                            if (def.freq_slide > 0) portamento_up(chan, static_cast<uint16_t>(def.freq_slide), nFreq(12 * 8 + 1));
                            else if (def.freq_slide < 0) portamento_down(chan, static_cast<uint16_t>(std::abs(def.freq_slide)), nFreq(0));
                        }
                    }
                }
            }
        }

        // アルペジオマクロ
        if (m.arpg_table != 0) {
            const ArpeggioTable& at = at1(sd.macro_table, m.arpg_table).arpeggio;
            if (at.speed != 0) {
                if (m.arpg_count == at.speed) {
                    m.arpg_count = 1;
                    if (m.arpg_pos <= at.length) {
                        if (at.loop_begin != 0 && at.loop_length != 0) {
                            if (m.arpg_pos == at.loop_begin + (at.loop_length - 1)) m.arpg_pos = at.loop_begin;
                            else if (m.arpg_pos < at.length) ++m.arpg_pos;
                            else m.arpg_pos = finished_flag;
                        } else if (m.arpg_pos < at.length) {
                            ++m.arpg_pos;
                        } else {
                            m.arpg_pos = finished_flag;
                        }
                    } else {
                        m.arpg_pos = finished_flag;
                    }

                    const bool on = (freq_table[chan] | 0x2000) == freq_table[chan];
                    if (on && at.keyoff_pos != 0 && m.arpg_pos >= at.keyoff_pos) {
                        m.arpg_pos = IDLE;
                    } else if (!on && at.keyoff_pos != 0 && (m.arpg_pos < at.keyoff_pos || m.arpg_pos == IDLE)) {
                        m.arpg_pos = at.keyoff_pos;
                    }

                    if (m.arpg_pos != 0 && m.arpg_pos != IDLE && m.arpg_pos != finished_flag) {
                        const uint8_t value = at1(at.data, m.arpg_pos);
                        const int8_t ftune = static_cast<int8_t>(ins_parameter(event_table[chan].instr_def, 12));
                        if (value == 0) {
                            change_frequency(chan, static_cast<uint16_t>(nFreq(static_cast<uint8_t>(m.arpg_note - 1)) + ftune));
                        } else if (value <= 96) {
                            change_frequency(chan, static_cast<uint16_t>(nFreq(static_cast<uint8_t>(std::min(m.arpg_note + value, 97) - 1)) + ftune));
                        } else if (value >= 0x80 && value <= 0x80 + 12 * 8 + 1) {
                            change_frequency(chan, static_cast<uint16_t>(nFreq(static_cast<uint8_t>(value - 0x80 - 1)) + ftune));
                        }
                    }
                } else {
                    ++m.arpg_count;
                }
            }
        }

        // ビブラートマクロ
        if (!m.vib_paused && m.vib_table != 0) {
            const VibratoTable& vt = at1(sd.macro_table, m.vib_table).vibrato;
            if (vt.speed != 0) {
                if (m.vib_count == vt.speed) {
                    if (m.vib_delay != 0) {
                        --m.vib_delay;
                    } else {
                        m.vib_count = 1;
                        if (m.vib_pos <= vt.length) {
                            if (vt.loop_begin != 0 && vt.loop_length != 0) {
                                if (m.vib_pos == vt.loop_begin + (vt.loop_length - 1)) m.vib_pos = vt.loop_begin;
                                else if (m.vib_pos < vt.length) ++m.vib_pos;
                                else m.vib_pos = finished_flag;
                            } else if (m.vib_pos < vt.length) {
                                ++m.vib_pos;
                            } else {
                                m.vib_pos = finished_flag;
                            }
                        } else {
                            m.vib_pos = finished_flag;
                        }

                        const bool on = (freq_table[chan] | 0x2000) == freq_table[chan];
                        if (on && vt.keyoff_pos != 0 && m.vib_pos >= vt.keyoff_pos) {
                            m.vib_pos = IDLE;
                        } else if (!on && m.vib_pos != 0 && vt.keyoff_pos != 0 &&
                                   (m.vib_pos < vt.keyoff_pos || m.vib_pos == IDLE)) {
                            m.vib_pos = vt.keyoff_pos;
                        }

                        if (m.vib_pos != 0 && m.vib_pos != IDLE && m.vib_pos != finished_flag) {
                            const int8_t value = at1(vt.data, m.vib_pos);
                            if (value > 0) macro_vibrato__porta_up(chan, static_cast<uint8_t>(value));
                            else if (value < 0) macro_vibrato__porta_down(chan, static_cast<uint8_t>(std::abs(value)));
                            else change_freq(chan, m.vib_freq);
                        }
                    }
                } else {
                    ++m.vib_count;
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 初期化・制御
// ---------------------------------------------------------------------------

void A2Player::init_buffers()
{
    FixedSongData& sd = songdata();

    std::memset(fmpar_table, 0, sizeof(fmpar_table));
    std::fill(std::begin(pan_lock), std::end(pan_lock), panlock);
    std::fill(std::begin(volume_table), std::end(volume_table), static_cast<uint16_t>(0x3f3f));   // FillChar(..., 63)
    std::memset(vscale_table, 0, sizeof(vscale_table));
    std::memset(modulator_vol, 0, sizeof(modulator_vol));
    std::memset(carrier_vol, 0, sizeof(carrier_vol));
    std::fill(std::begin(decay_bar), std::end(decay_bar), DecayBar{});
    std::fill(std::begin(volum_bar), std::end(volum_bar), VolumBar{});
    std::memset(event_table, 0, sizeof(event_table));
    std::memset(freq_table, 0, sizeof(freq_table));
    std::memset(zero_fq_table, 0, sizeof(zero_fq_table));
    std::memset(effect_table, 0, sizeof(effect_table));
    std::memset(fslide_table, 0, sizeof(fslide_table));
    std::memset(glfsld_table, 0, sizeof(glfsld_table));
    std::memset(porta_table, 0, sizeof(porta_table));
    std::fill(std::begin(portaFK_table), std::end(portaFK_table), false);
    std::memset(arpgg_table, 0, sizeof(arpgg_table));
    std::memset(vibr_table, 0, sizeof(vibr_table));
    std::memset(trem_table, 0, sizeof(trem_table));
    std::memset(retrig_table, 0, sizeof(retrig_table));
    std::memset(tremor_table, 0, sizeof(tremor_table));
    std::memset(last_effect, 0, sizeof(last_effect));
    std::memset(voice_table, 0, sizeof(voice_table));
    std::fill(std::begin(event_new), std::end(event_new), false);
    std::memset(freqtable2, 0, sizeof(freqtable2));
    std::memset(notedel_table, BYTE_NULL, sizeof(notedel_table));
    std::memset(notecut_table, BYTE_NULL, sizeof(notecut_table));
    std::memset(ftune_table, 0, sizeof(ftune_table));
    std::memset(loopbck_table, BYTE_NULL, sizeof(loopbck_table));
    std::memset(loop_table, BYTE_NULL, sizeof(loop_table));
    std::fill(std::begin(reset_chan), std::end(reset_chan), false);
    std::fill(std::begin(reset_adsrw), std::end(reset_adsrw), false);
    std::fill(std::begin(keyoff_loop), std::end(keyoff_loop), false);
    std::memset(macro_table, 0, sizeof(macro_table));
    std::fill(std::begin(ignore_note_once), std::end(ignore_note_once), false);

    for (int temp = 1; temp <= 20; ++temp) {
        volume_lock[temp] = lockvol ? ((sd.lock_flags[temp - 1] >> 4) & 1) != 0 : false;
        panning_table[temp] = panlock ? (sd.lock_flags[temp - 1] & 3) : 0;
        peak_lock[temp] = lockVP ? ((sd.lock_flags[temp - 1] >> 5) & 1) != 0 : false;
    }

    std::fill(std::begin(vol4op_lock), std::end(vol4op_lock), false);
    for (uint8_t main_chan : _4op_main_chan) {
        vol4op_lock[main_chan] = (sd.lock_flags[main_chan - 1] | 0x40) == sd.lock_flags[main_chan - 1];
        vol4op_lock[main_chan - 1] = (sd.lock_flags[main_chan - 2] | 0x40) == sd.lock_flags[main_chan - 2];
    }

    for (int temp = 1; temp <= 20; ++temp) {
        volslide_type[temp] = (sd.lock_flags[temp - 1] >> 2) & 3;
    }
}

void A2Player::init_player()
{
    opl_.reset();   // opl3_init

    std::memset(ai_table, 0, sizeof(ai_table));
    opl3out(0x01, 0);

    for (int temp = 1; temp <= 18; ++temp) opl3out(0x0b0 + _chan_n[temp], 0);
    for (int temp = 0x80; temp <= 0x8d; ++temp) opl3out(static_cast<uint16_t>(temp), BYTE_NULL);
    for (int temp = 0x90; temp <= 0x95; ++temp) opl3out(static_cast<uint16_t>(temp), BYTE_NULL);

    misc_register = static_cast<uint8_t>((tremolo_depth << 7) + (vibrato_depth << 6) + (static_cast<uint8_t>(percussion_mode) << 5));

    opl3out(0x01, 0x20);
    opl3out(0x08, 0x40);
    opl3out(0x105, 0x01);                  // opl3exp($0105)
    opl3out(0x104, songdata().flag_4op);   // opl3exp($04+flag_4op SHL 8)

    key_off(17);
    key_off(18);
    opl3out(_instr[11], misc_register);
    init_buffers();

    current_tremolo_depth = tremolo_depth;
    current_vibrato_depth = vibrato_depth;
    global_volume = 63;
    macro_ticklooper = 0;
    vibtrem_speed_factor = def_vibtrem_speed_factor;
    vibtrem_table_size = def_vibtrem_table_size;
    std::memcpy(vibtrem_table, def_vibtrem_table, sizeof(vibtrem_table));

    for (int temp = 1; temp <= 20; ++temp) {
        arpgg_table[0][temp].state = 1;
        arpgg_table[1][temp].state = 1;
        voice_table[temp] = static_cast<uint8_t>(temp);
    }
}

void A2Player::reset_player()
{
    opl3out(0x01, 0);

    for (int temp = 1; temp <= 18; ++temp) opl3out(0x0b0 + _chan_n[temp], 0);
    for (int temp = 0x80; temp <= 0x8d; ++temp) opl3out(static_cast<uint16_t>(temp), BYTE_NULL);
    for (int temp = 0x90; temp <= 0x95; ++temp) opl3out(static_cast<uint16_t>(temp), BYTE_NULL);

    misc_register = static_cast<uint8_t>((tremolo_depth << 7) + (vibrato_depth << 6) + (static_cast<uint8_t>(percussion_mode) << 5));

    opl3out(0x01, 0x20);
    opl3out(0x08, 0x40);
    opl3out(0x105, 0x01);
    opl3out(0x104, songdata().flag_4op);

    key_off(17);
    key_off(18);
    opl3out(_instr[11], misc_register);
    for (int temp = 1; temp <= 20; ++temp) reset_chan_data(static_cast<uint8_t>(temp));
}

void A2Player::apply_song_flags()
{
    FixedSongData& sd = songdata();

    speed_update = (sd.common_flag & 1) != 0;
    lockvol = ((sd.common_flag >> 1) & 1) != 0;
    lockVP = ((sd.common_flag >> 2) & 1) != 0;
    tremolo_depth = (sd.common_flag >> 3) & 1;
    vibrato_depth = (sd.common_flag >> 4) & 1;
    panlock = ((sd.common_flag >> 5) & 1) != 0;
    percussion_mode = ((sd.common_flag >> 6) & 1) != 0;
    volume_scaling = ((sd.common_flag >> 7) & 1) != 0;

    mark_line = sd.bpm_data.rows_per_beat;
    IRQ_freq_shift = sd.bpm_data.tempo_finetune;
    playback_speed_shift = 0;

    current_tremolo_depth = tremolo_depth;
    current_vibrato_depth = vibrato_depth;

    if (!percussion_mode) {
        std::memcpy(_chan_n, _chmm_n, sizeof(_chan_n));
        std::memcpy(_chan_m, _chmm_m, sizeof(_chan_m));
        std::memcpy(_chan_c, _chmm_c, sizeof(_chan_c));
    } else {
        std::memcpy(_chan_n, _chpm_n, sizeof(_chan_n));
        std::memcpy(_chan_m, _chpm_m, sizeof(_chan_m));
        std::memcpy(_chan_c, _chpm_c, sizeof(_chan_c));
    }

    init_buffers();
    std::fill(std::begin(channel_flag), std::end(channel_flag), true);
}

void A2Player::setSong(std::shared_ptr<Song> song)
{
    if (play_status != PlayStatus::Stopped) stopPlaying();

    song_ = std::move(song);
    speed = songdata().speed;
    tempo = songdata().tempo;
    apply_song_flags();
}

void A2Player::startPlaying()
{
    FixedSongData& sd = songdata();

    init_player();
    if (start_pattern == BYTE_NULL) {
        current_order = 0;
    } else if (start_order == BYTE_NULL) {
        if (calc_pattern_pos(start_pattern) != BYTE_NULL) {
            current_order = calc_pattern_pos(start_pattern);
        } else if (!play_single_patt) {
            start_pattern = BYTE_NULL;
            current_order = 0;
            return;
        }
    } else {
        current_order = start_order;
        current_pattern = start_pattern;
    }

    if (!play_single_patt) {
        if (order_at(current_order) > 0x7f) {
            if (calc_order_jump() == -1) return;
        }
    }

    if (!play_single_patt) current_pattern = order_at(current_order);
    else current_pattern = start_pattern;

    current_line = (start_line == BYTE_NULL) ? 0 : start_line;
    pattern_break = false;
    pattern_delay = false;
    tickXF = 0;
    last_order = 0;
    next_line = 0;
    song_timer = 0;
    timer_temp = 0;
    song_timer_tenths = 0;
    time_playing = 0;
    ticklooper = 0;
    macro_ticklooper = 0;
    debugging = false;
    ticks = 0;
    tick0 = 0;
    fade_out_volume = 63;
    playback_speed_shift = 0;
    replay_forbidden = false;
    play_status = PlayStatus::Playing;
    speed = sd.speed;
    macro_speedup = sd.macro_speedup;
    update_timer(sd.tempo);
    std::memset(play_pos_buf, 0, sizeof(play_pos_buf));
}

void A2Player::stopPlaying()
{
    replay_forbidden = true;
    play_status = PlayStatus::Stopped;
    fade_out_volume = 63;
    repeat_pattern = false;
    global_volume = 63;
    no_sync_playing = false;
    play_single_patt = false;
    current_tremolo_depth = tremolo_depth;
    current_vibrato_depth = vibrato_depth;
    pattern_break = false;
    current_order = 0;
    current_pattern = 0;
    current_line = 0;
    start_order = BYTE_NULL;
    start_pattern = BYTE_NULL;
    start_line = BYTE_NULL;
    song_timer = 0;
    timer_temp = 0;
    song_timer_tenths = 0;
    time_playing = 0;
    playback_speed_shift = 0;

    skip_macro_flag = true;
    for (int temp = 1; temp <= 20; ++temp) reset_chan_data(static_cast<uint8_t>(temp));
    opl3out(_instr[11], 0);
    init_buffers();
    skip_macro_flag = false;

    speed = songdata().speed;
    update_timer(songdata().tempo);
}

void A2Player::nul_volume_bars()
{
    for (int chan = 1; chan <= 20; ++chan) {
        decay_bar[chan] = {};
        volum_bar[chan] = {};
    }
}

void A2Player::calibrate_player(uint8_t order, uint8_t line, bool line_dependent)
{
    if (calc_following_order(0) == -1) return;
    calibrating = true;
    const bool backup_replay_forbidden = replay_forbidden;
    const PlayStatus backup_play_status = play_status;

    nul_volume_bars();
    bool temp_channel_flag[NCH];
    std::copy(std::begin(channel_flag), std::end(channel_flag), std::begin(temp_channel_flag));
    std::fill(std::begin(channel_flag), std::end(channel_flag), false);

    const bool old_debugging = debugging;
    const bool old_repeat_pattern = repeat_pattern;
    debugging = false;
    repeat_pattern = false;

    if (play_status == PlayStatus::Stopped || order < current_order || order == calc_following_order(0)) {
        if (!no_sync_playing) {
            stopPlaying();
        } else {
            stopPlaying();
            no_sync_playing = true;
        }

        init_player();
        speed = songdata().speed;
        macro_speedup = songdata().macro_speedup;
        update_timer(songdata().tempo);
        current_order = static_cast<uint8_t>(calc_following_order(0));
        current_pattern = order_at(current_order);
        current_line = 0;
        pattern_break = false;
        pattern_delay = false;
        last_order = 0;
        next_line = 0;
        song_timer = 0;
        timer_temp = 0;
        song_timer_tenths = 0;
        time_playing = 0;
        ticklooper = 0;
        macro_ticklooper = 0;
        ticks = 0;
        tick0 = 0;
    }

    uint8_t previous_order = current_order;
    uint8_t previous_line = current_line;
    int jump_count = 0;
    int loop_count = 0;
    replay_forbidden = true;
    seek_pattern_break = false;

    if (!no_sync_playing) {
        while (current_line != line || current_order != order) {
            if (!(previous_order == current_order && previous_line >= current_line &&
                  !(pattern_break && (next_line & 0xf0) == pattern_loop_flag))) {
                loop_count = 0;
            } else if (++loop_count > 15) {
                break;
            }

            if (current_order == order && current_line >= line && !line_dependent) break;

            previous_order = current_order;
            previous_line = current_line;
            fast_forward = true;

            poll_proc();
            if (macro_ticklooper == 0) macro_poll_proc();

            fast_forward = false;
            ++ticklooper;
            if (ticklooper >= IRQ_freq / std::max<int32_t>(tempo, 1)) ticklooper = 0;

            ++macro_ticklooper;
            if (macro_ticklooper >= IRQ_freq / std::max<int32_t>(tempo * macro_speedup, 1)) macro_ticklooper = 0;

            if (previous_order != current_order) {
                if (++jump_count > 0x7f) break;
            }

            if (seek_pattern_break) {
                if (current_order > order || (current_order == order && (current_line != 0 || line == 0))) break;
                seek_pattern_break = false;
            }
        }
    } else {
        startPlaying();
        current_order = order;
        current_pattern = order_at(order);
        current_line = line;
    }

    fade_out_volume = 63;
    std::copy(std::begin(temp_channel_flag), std::end(temp_channel_flag), std::begin(channel_flag));

    for (int temp = 1; temp <= 20; ++temp) reset_chan_data(static_cast<uint8_t>(temp));
    if (backup_play_status != PlayStatus::Stopped) {
        replay_forbidden = backup_replay_forbidden;
        play_status = backup_play_status;
    } else {
        replay_forbidden = false;
        play_status = PlayStatus::Playing;
    }

    debugging = old_debugging;
    repeat_pattern = old_repeat_pattern;
    synchronize_song_timer();
    calibrating = false;
}

void A2Player::timerTick()
{
    const int32_t IRQ_freq_val = !rewind ? IRQ_freq : IRQ_freq * 20;

    if (ticklooper > 0) {
        if ((fast_forward || rewind) && !replay_forbidden) poll_proc();
    } else if (!replay_forbidden) {
        poll_proc();
    }

    if (macro_ticklooper == 0) macro_poll_proc();

    // tempo や macro_speedup が 0 の場合オリジナルは 0 除算で停止するため、1 として扱う
    const int32_t tempo_div = std::max<int32_t>(tempo, 1);
    const int32_t macro_div = std::max<int32_t>(tempo * macro_speedup, 1);

    ++ticklooper;
    if (ticklooper >= IRQ_freq_val / tempo_div) ticklooper = 0;

    ++macro_ticklooper;
    if (macro_ticklooper >= IRQ_freq_val / macro_div) macro_ticklooper = 0;
}
