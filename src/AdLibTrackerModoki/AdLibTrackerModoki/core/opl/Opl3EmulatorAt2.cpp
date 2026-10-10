// AdLib Tracker II (AT2) の opl3emu.pas の移植
//   Based on NukedOPL3 1.6 by Nuke.YKT (Alexey Khokholov)
//   Pascal conversion by insane/Altair, AT2 by subz3ro
// 関数名・フィールド名はオリジナルとの対応を追えるよう Pascal 版の名前をそのまま使っている。

#include "Opl3EmulatorAt2.h"

#include <cstdlib>
#include <cstring>

namespace
{
    const uint16_t LOG_SIN_VAL[256] = {
        0x859,0x6c3,0x607,0x58b,0x52e,0x4e4,0x4a6,0x471,0x443,0x41a,0x3f5,0x3d3,0x3b5,0x398,0x37e,0x365,
        0x34e,0x339,0x324,0x311,0x2ff,0x2ed,0x2dc,0x2cd,0x2bd,0x2af,0x2a0,0x293,0x286,0x279,0x26d,0x261,
        0x256,0x24b,0x240,0x236,0x22c,0x222,0x218,0x20f,0x206,0x1fd,0x1f5,0x1ec,0x1e4,0x1dc,0x1d4,0x1cd,
        0x1c5,0x1be,0x1b7,0x1b0,0x1a9,0x1a2,0x19b,0x195,0x18f,0x188,0x182,0x17c,0x177,0x171,0x16b,0x166,
        0x160,0x15b,0x155,0x150,0x14b,0x146,0x141,0x13c,0x137,0x133,0x12e,0x129,0x125,0x121,0x11c,0x118,
        0x114,0x10f,0x10b,0x107,0x103,0x0ff,0x0fb,0x0f8,0x0f4,0x0f0,0x0ec,0x0e9,0x0e5,0x0e2,0x0de,0x0db,
        0x0d7,0x0d4,0x0d1,0x0cd,0x0ca,0x0c7,0x0c4,0x0c1,0x0be,0x0bb,0x0b8,0x0b5,0x0b2,0x0af,0x0ac,0x0a9,
        0x0a7,0x0a4,0x0a1,0x09f,0x09c,0x099,0x097,0x094,0x092,0x08f,0x08d,0x08a,0x088,0x086,0x083,0x081,
        0x07f,0x07d,0x07a,0x078,0x076,0x074,0x072,0x070,0x06e,0x06c,0x06a,0x068,0x066,0x064,0x062,0x060,
        0x05e,0x05c,0x05b,0x059,0x057,0x055,0x053,0x052,0x050,0x04e,0x04d,0x04b,0x04a,0x048,0x046,0x045,
        0x043,0x042,0x040,0x03f,0x03e,0x03c,0x03b,0x039,0x038,0x037,0x035,0x034,0x033,0x031,0x030,0x02f,
        0x02e,0x02d,0x02b,0x02a,0x029,0x028,0x027,0x026,0x025,0x024,0x023,0x022,0x021,0x020,0x01f,0x01e,
        0x01d,0x01c,0x01b,0x01a,0x019,0x018,0x017,0x017,0x016,0x015,0x014,0x014,0x013,0x012,0x011,0x011,
        0x010,0x00f,0x00f,0x00e,0x00d,0x00d,0x00c,0x00c,0x00b,0x00a,0x00a,0x009,0x009,0x008,0x008,0x007,
        0x007,0x007,0x006,0x006,0x005,0x005,0x005,0x004,0x004,0x004,0x003,0x003,0x003,0x002,0x002,0x002,
        0x002,0x001,0x001,0x001,0x001,0x001,0x001,0x001,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000 };

    const uint16_t EXP_VAL[256] = {
        0x000,0x003,0x006,0x008,0x00b,0x00e,0x011,0x014,0x016,0x019,0x01c,0x01f,0x022,0x025,0x028,0x02a,
        0x02d,0x030,0x033,0x036,0x039,0x03c,0x03f,0x042,0x045,0x048,0x04b,0x04e,0x051,0x054,0x057,0x05a,
        0x05d,0x060,0x063,0x066,0x069,0x06c,0x06f,0x072,0x075,0x078,0x07b,0x07e,0x082,0x085,0x088,0x08b,
        0x08e,0x091,0x094,0x098,0x09b,0x09e,0x0a1,0x0a4,0x0a8,0x0ab,0x0ae,0x0b1,0x0b5,0x0b8,0x0bb,0x0be,
        0x0c2,0x0c5,0x0c8,0x0cc,0x0cf,0x0d2,0x0d6,0x0d9,0x0dc,0x0e0,0x0e3,0x0e7,0x0ea,0x0ed,0x0f1,0x0f4,
        0x0f8,0x0fb,0x0ff,0x102,0x106,0x109,0x10c,0x110,0x114,0x117,0x11b,0x11e,0x122,0x125,0x129,0x12c,
        0x130,0x134,0x137,0x13b,0x13e,0x142,0x146,0x149,0x14d,0x151,0x154,0x158,0x15c,0x160,0x163,0x167,
        0x16b,0x16f,0x172,0x176,0x17a,0x17e,0x181,0x185,0x189,0x18d,0x191,0x195,0x199,0x19c,0x1a0,0x1a4,
        0x1a8,0x1ac,0x1b0,0x1b4,0x1b8,0x1bc,0x1c0,0x1c4,0x1c8,0x1cc,0x1d0,0x1d4,0x1d8,0x1dc,0x1e0,0x1e4,
        0x1e8,0x1ec,0x1f0,0x1f5,0x1f9,0x1fd,0x201,0x205,0x209,0x20e,0x212,0x216,0x21a,0x21e,0x223,0x227,
        0x22b,0x230,0x234,0x238,0x23c,0x241,0x245,0x249,0x24e,0x252,0x257,0x25b,0x25f,0x264,0x268,0x26d,
        0x271,0x276,0x27a,0x27f,0x283,0x288,0x28c,0x291,0x295,0x29a,0x29e,0x2a3,0x2a8,0x2ac,0x2b1,0x2b5,
        0x2ba,0x2bf,0x2c4,0x2c8,0x2cd,0x2d2,0x2d6,0x2db,0x2e0,0x2e5,0x2e9,0x2ee,0x2f3,0x2f8,0x2fd,0x302,
        0x306,0x30b,0x310,0x315,0x31a,0x31f,0x324,0x329,0x32e,0x333,0x338,0x33d,0x342,0x347,0x34c,0x351,
        0x356,0x35b,0x360,0x365,0x36a,0x370,0x375,0x37a,0x37f,0x384,0x38a,0x38f,0x394,0x399,0x39f,0x3a4,
        0x3a9,0x3ae,0x3b4,0x3b9,0x3bf,0x3c4,0x3c9,0x3cf,0x3d4,0x3da,0x3df,0x3e4,0x3ea,0x3ef,0x3f5,0x3fa };

    const uint8_t MULT_VAL[16] = { 1,2,4,6,8,10,12,14,16,18,20,20,24,24,30,30 };
    const uint8_t KSL_VAL[16] = { 0,32,40,45,48,51,53,55,56,58,59,60,61,62,63,64 };
    const uint8_t KSL_SHIFT[4] = { 8,1,2,0 };
    const uint8_t SL_VAL[16] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,31 };
    const uint8_t VIB_SHIFT[8] = { 3,1,0,1,3,1,0,1 };
    const int8_t VIB_S_SHIFT[8] = { 1,1,1,1,-1,-1,-1,-1 };
    const uint8_t EG_IDX[16] = { 0,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2 };
    const int8_t EG_SHIFT[16] = { 0,11,10,9,8,7,6,5,4,3,2,1,0,0,-1,-2 };
    const uint8_t EG_VAL[3][4][8] = {
        { {0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0} },
        { {0,1,0,1,0,1,0,1},{0,1,0,1,1,1,0,1},{0,1,1,1,0,1,1,1},{0,1,1,1,1,1,1,1} },
        { {1,1,1,1,1,1,1,1},{2,2,1,1,1,1,1,1},{2,2,1,1,2,2,1,1},{2,2,2,2,2,2,1,1} } };

    const uint8_t CH_5BIT_MASK[32] = { 1,2,3,4,5,6,0,0,7,8,9,10,11,12,0,0,13,14,15,16,17,18,0,0,0,0,0,0,0,0,0,0 };
    const uint8_t CH_SLOT_IDX[18] = { 0,1,2,6,7,8,12,13,14,18,19,20,24,25,26,30,31,32 };
    const uint8_t CH_4OP_MASK[18] = { 4,5,6,1,2,3,0,0,0,13,14,15,10,11,12,0,0,0 };
    const uint8_t CH_4OP_IDX[6] = { 0,1,2,9,10,11 };

    constexpr uint32_t NOISE_HASH_VAL = 0x306600;
    constexpr uint32_t NOISE_XOR = 0x800302;
    constexpr uint16_t WORD_NULL = 0xffff;

    int32_t limit_value(int32_t value, int32_t lo_bound, int32_t hi_bound)
    {
        if (value > hi_bound) return hi_bound;
        if (value < lo_bound) return lo_bound;
        return value;
    }
}

struct Opl3EmulatorAt2::Chip
{
    enum ChanType : uint8_t { CH_2OP, CH_4OP_1, CH_4OP_2, CH_RHYTHM };
    enum EgState : uint8_t { EG_OFF, EG_ATTACK, EG_DECAY, EG_SUSTAIN, EG_RELEASE };

    struct Chan;

    struct Slot
    {
        Chan* p_chan;
        Chip* p_chip;
        int16_t* p_mod;
        const uint8_t* p_trem;
        int16_t fb_out, prev_out, output;
        uint32_t pg_phase;
        EgState eg_state;
        int16_t eg_rout, eg_out;
        uint8_t eg_inc, eg_rate, eg_ksl;
        uint8_t reg_vib, reg_type, reg_ksr, reg_mult, reg_ksl, reg_tl, reg_ar, reg_dr, reg_sl, reg_rr, reg_wf, key;
    };

    struct Chan
    {
        Slot* p_slot[2];
        Chan* p_chan;
        Chip* p_chip;
        int16_t* p_out[4];
        ChanType ch_type;
        uint16_t fnum;
        uint8_t block, fb, con, alg, ksr;
        uint16_t out_l, out_r;
    };

    Chan chan[18];
    Slot slot[36];
    uint16_t timer;
    uint8_t nts_bit, dva_bit, dvb_bit;
    uint8_t rhy_flag;
    uint8_t trem_dir;
    uint8_t trem_pos, trem_val, vib_pos;
    uint32_t noise;
    int32_t output[2];
    int16_t out_l[18];
    int16_t out_r[18];
    int16_t out_null;

    const uint8_t* null_byte() const { return reinterpret_cast<const uint8_t*>(&out_null); }

    // -----------------------------------------------------------------------
    // エンベロープ
    // -----------------------------------------------------------------------

    static int16_t envelope_calc_sin(uint8_t wf, uint16_t phase, int16_t eg_out)
    {
        phase &= 0x3ff;
        uint16_t output = 0;
        uint16_t invert = 0;

        switch (wf) {
        case 0:   // Sine
            if (phase & 0x200) invert = ~invert;
            if (phase & 0x100) output = LOG_SIN_VAL[(phase & 0x0ff) ^ 0x0ff];
            else output = LOG_SIN_VAL[phase & 0x0ff];
            break;
        case 1:   // Half-Sine
            if (phase & 0x200) output = 0x1000;
            else if (phase & 0x100) output = LOG_SIN_VAL[(phase & 0x0ff) ^ 0x0ff];
            else output = LOG_SIN_VAL[phase & 0x0ff];
            break;
        case 2:   // Abs-Sine
            if (phase & 0x100) output = LOG_SIN_VAL[(phase & 0x0ff) ^ 0x0ff];
            else output = LOG_SIN_VAL[phase & 0x0ff];
            break;
        case 3:   // Pulse-Sine
            if (phase & 0x100) output = 0x1000;
            else output = LOG_SIN_VAL[phase & 0x0ff];
            break;
        case 4:   // Sine (EPO)
            if ((phase & 0x300) == 0x100) invert = ~invert;
            if (phase & 0x200) output = 0x1000;
            else if (phase & 0x80) output = LOG_SIN_VAL[((phase ^ 0x0ff) << 1) & 0x0ff];
            else output = LOG_SIN_VAL[(phase << 1) & 0x0ff];
            break;
        case 5:   // Abs-Sine (EPO)
            if (phase & 0x200) output = 0x1000;
            else if (phase & 0x80) output = LOG_SIN_VAL[((phase ^ 0x0ff) << 1) & 0x0ff];
            else output = LOG_SIN_VAL[(phase << 1) & 0x0ff];
            break;
        case 6:   // Square
            if (phase & 0x200) invert = WORD_NULL;
            output = 0;
            break;
        case 7:   // Derived Square
            if (phase & 0x200) {
                invert = ~invert;
                phase = (phase & 0x1ff) ^ 0x1ff;
            }
            output = static_cast<uint16_t>(phase << 3);
            break;
        }

        const int32_t level = limit_value(output + (eg_out * 8), 0, 0x1fff);
        const int16_t value = static_cast<int16_t>(((EXP_VAL[(level & 0x0ff) ^ 0x0ff] | 0x400) << 1) >> (level >> 8));
        return static_cast<int16_t>(static_cast<uint16_t>(value) ^ invert);
    }

    static uint8_t envelope_calc_rate(Slot* p_slot, uint8_t reg_rate)
    {
        if (reg_rate == 0) return 0;

        int32_t rate = reg_rate << 2;
        if (p_slot->reg_ksr != 0) rate += p_slot->p_chan->ksr;
        else rate += p_slot->p_chan->ksr >> 2;
        return static_cast<uint8_t>(limit_value(rate, 0, 60));
    }

    static void envelope_update_ksl(Slot* p_slot)
    {
        const int32_t ksl = (KSL_VAL[p_slot->p_chan->fnum >> 6] << 2) - ((8 - p_slot->p_chan->block) << 5);
        p_slot->eg_ksl = static_cast<uint8_t>(limit_value(static_cast<int16_t>(ksl), 0, 255));
    }

    static void envelope_update_rate(Slot* p_slot)
    {
        switch (p_slot->eg_state) {
        case EG_OFF: p_slot->eg_rate = 0; break;
        case EG_ATTACK: p_slot->eg_rate = envelope_calc_rate(p_slot, p_slot->reg_ar); break;
        case EG_DECAY: p_slot->eg_rate = envelope_calc_rate(p_slot, p_slot->reg_dr); break;
        case EG_SUSTAIN:
        case EG_RELEASE: p_slot->eg_rate = envelope_calc_rate(p_slot, p_slot->reg_rr); break;
        }
    }

    static void envelope_calc(Slot* p_slot)
    {
        const uint8_t rate_hi = p_slot->eg_rate >> 2;
        const uint8_t rate_lo = p_slot->eg_rate & 3;
        const uint16_t timer = p_slot->p_chip->timer;

        // 出力の増分を計算する
        if (EG_SHIFT[rate_hi] > 0) {
            if ((timer & ((1 << EG_SHIFT[rate_hi]) - 1)) == 0) {
                p_slot->eg_inc = EG_VAL[EG_IDX[rate_hi]][rate_lo][(timer >> EG_SHIFT[rate_hi]) & 7];
            } else {
                p_slot->eg_inc = 0;
            }
        } else {
            p_slot->eg_inc = static_cast<uint8_t>(EG_VAL[EG_IDX[rate_hi]][rate_lo][timer & 7] << std::abs(EG_SHIFT[rate_hi]));
        }

        p_slot->eg_out = static_cast<int16_t>(p_slot->eg_rout + (p_slot->reg_tl << 2) +
                                              (p_slot->eg_ksl >> KSL_SHIFT[p_slot->reg_ksl]) +
                                              *p_slot->p_trem);   // LFO トレモロを加える

        switch (p_slot->eg_state) {
        case EG_OFF:
            p_slot->eg_rout = 0x1ff;
            break;

        case EG_ATTACK:
            if (p_slot->eg_rout != 0) {
                // オリジナルは limit_value の結果を捨てているため、ここでも範囲制限はしない
                p_slot->eg_rout = static_cast<int16_t>(p_slot->eg_rout + ((~p_slot->eg_rout * p_slot->eg_inc) >> 3));
            } else {
                // 最大レベルに達したらディケイへ
                p_slot->eg_state = EG_DECAY;
                envelope_update_rate(p_slot);
            }
            break;

        case EG_DECAY:
            if (p_slot->eg_rout < static_cast<int16_t>(p_slot->reg_sl << 4)) {
                p_slot->eg_rout = static_cast<int16_t>(p_slot->eg_rout + p_slot->eg_inc);
            } else {
                // サステインレベルに達した
                p_slot->eg_state = EG_SUSTAIN;
                envelope_update_rate(p_slot);
            }
            break;

        case EG_SUSTAIN:
        case EG_RELEASE:
            if (p_slot->eg_state == EG_SUSTAIN && p_slot->reg_type != 0) {
                // サステイン中
            } else if (p_slot->eg_rout < 0x1ff) {
                p_slot->eg_rout = static_cast<int16_t>(p_slot->eg_rout + p_slot->eg_inc);
            } else {
                // 最小レベルに達したら停止
                p_slot->eg_state = EG_OFF;
                p_slot->eg_rout = 0x1ff;
                envelope_update_rate(p_slot);
            }
            break;
        }
    }

    static void eg_key_on_off(Slot* p_slot, bool key_on)
    {
        if (key_on) {
            if (p_slot->key == 0) {
                p_slot->eg_state = EG_ATTACK;
                envelope_update_rate(p_slot);
                if ((p_slot->eg_rate >> 2) == 0x0f) {
                    p_slot->eg_state = EG_DECAY;
                    envelope_update_rate(p_slot);
                    p_slot->eg_rout = 0;
                }
                p_slot->pg_phase = 0;
            }

            if (p_slot->p_chan->ch_type != CH_RHYTHM) p_slot->key |= 1;
            else p_slot->key |= 2;

            if (p_slot->reg_ar == 0) {
                // AR=0 のときはエンベロープを再開させないための擬似ディケイ
                p_slot->eg_state = EG_DECAY;
                ++p_slot->eg_rout;
            }
        } else if (p_slot->key != 0) {
            if (p_slot->p_chan->ch_type != CH_RHYTHM) p_slot->key &= 0xfe;
            else p_slot->key &= 0xfd;

            if (p_slot->key == 0) {
                p_slot->eg_state = EG_RELEASE;
                envelope_update_rate(p_slot);
            }
        }
    }

    // -----------------------------------------------------------------------
    // 位相・スロット
    // -----------------------------------------------------------------------

    static void pg_generate(Slot* p_slot)
    {
        uint16_t fnum = p_slot->p_chan->fnum;
        if (p_slot->reg_vib != 0) {
            // LFO ビブラートを加える
            const uint16_t timer = p_slot->p_chip->timer;
            const uint8_t fnum_hi = static_cast<uint8_t>(fnum >> (7 + VIB_SHIFT[(timer >> 10) & 7] + (1 - p_slot->p_chip->dvb_bit)));
            fnum = static_cast<uint16_t>(fnum + fnum_hi * VIB_S_SHIFT[(timer >> 10) & 7]);
        }
        p_slot->pg_phase += ((((static_cast<uint32_t>(fnum) << p_slot->p_chan->block) >> 1) * MULT_VAL[p_slot->reg_mult]) >> 1);
    }

    void update_lfo_eg_ksr_mult(Slot* p_slot, uint8_t data)
    {
        // LFO トレモロの割り当て
        if ((data >> 7) & 1) p_slot->p_trem = &trem_val;
        else p_slot->p_trem = null_byte();

        p_slot->reg_vib = (data >> 6) & 1;
        p_slot->reg_type = (data >> 5) & 1;
        p_slot->reg_ksr = (data >> 4) & 1;
        p_slot->reg_mult = data & 0x0f;
        envelope_update_rate(p_slot);
    }

    static void update_ksl_tl(Slot* p_slot, uint8_t data)
    {
        p_slot->reg_ksl = (data >> 6) & 3;
        p_slot->reg_tl = data & 0x3f;
        envelope_update_ksl(p_slot);
    }

    static void update_ar_dr(Slot* p_slot, uint8_t data)
    {
        p_slot->reg_ar = (data >> 4) & 0x0f;
        p_slot->reg_dr = data & 0x0f;
        envelope_update_rate(p_slot);
    }

    static void update_sl_rr(Slot* p_slot, uint8_t data)
    {
        p_slot->reg_sl = SL_VAL[(data >> 4) & 0x0f];
        p_slot->reg_rr = data & 0x0f;
        envelope_update_rate(p_slot);
    }

    static void slot_generate(Slot* p_slot, uint16_t phase)
    {
        if (phase == WORD_NULL) phase = static_cast<uint16_t>(static_cast<uint16_t>(p_slot->pg_phase >> 9) + *p_slot->p_mod);
        p_slot->output = envelope_calc_sin(p_slot->reg_wf, phase, p_slot->eg_out);
    }

    static void slot_calc_fb(Slot* p_slot)
    {
        if (p_slot->p_chan->fb != 0) {
            p_slot->fb_out = static_cast<int16_t>((p_slot->prev_out + p_slot->output) >> (9 - p_slot->p_chan->fb));
        } else {
            p_slot->fb_out = 0;
        }
        p_slot->prev_out = p_slot->output;
    }

    // -----------------------------------------------------------------------
    // チャンネル
    // -----------------------------------------------------------------------

    void chan_set_alg(Chan* p_chan)
    {
        switch (p_chan->ch_type) {
        case CH_2OP:
            if ((p_chan->alg & 1) == 0) {
                // FM
                p_chan->p_slot[0]->p_mod = &p_chan->p_slot[0]->fb_out;
                p_chan->p_slot[1]->p_mod = &p_chan->p_slot[0]->output;
                p_chan->p_out[0] = &p_chan->p_slot[1]->output;
                p_chan->p_out[1] = &out_null;
                p_chan->p_out[2] = &out_null;
                p_chan->p_out[3] = &out_null;
            } else {
                // AM
                p_chan->p_slot[0]->p_mod = &p_chan->p_slot[0]->fb_out;
                p_chan->p_slot[1]->p_mod = &out_null;
                p_chan->p_out[0] = &p_chan->p_slot[0]->output;
                p_chan->p_out[1] = &p_chan->p_slot[1]->output;
                p_chan->p_out[2] = &out_null;
                p_chan->p_out[3] = &out_null;
            }
            break;

        case CH_4OP_2:
        {
            Chan* pair = p_chan->p_chan;
            pair->p_out[0] = &out_null;
            pair->p_out[1] = &out_null;
            pair->p_out[2] = &out_null;
            pair->p_out[3] = &out_null;

            switch (p_chan->alg & 3) {
            case 0:   // FM-FM
                pair->p_slot[0]->p_mod = &pair->p_slot[0]->fb_out;
                pair->p_slot[1]->p_mod = &pair->p_slot[0]->output;
                p_chan->p_slot[0]->p_mod = &pair->p_slot[1]->output;
                p_chan->p_slot[1]->p_mod = &p_chan->p_slot[0]->output;
                p_chan->p_out[0] = &p_chan->p_slot[1]->output;
                p_chan->p_out[1] = &out_null;
                p_chan->p_out[2] = &out_null;
                p_chan->p_out[3] = &out_null;
                break;
            case 1:   // FM-AM
                pair->p_slot[0]->p_mod = &pair->p_slot[0]->fb_out;
                pair->p_slot[1]->p_mod = &pair->p_slot[0]->output;
                p_chan->p_slot[0]->p_mod = &out_null;
                p_chan->p_slot[1]->p_mod = &p_chan->p_slot[0]->output;
                p_chan->p_out[0] = &pair->p_slot[1]->output;
                p_chan->p_out[1] = &p_chan->p_slot[1]->output;
                p_chan->p_out[2] = &out_null;
                p_chan->p_out[3] = &out_null;
                break;
            case 2:   // AM-AM
                pair->p_slot[0]->p_mod = &pair->p_slot[0]->fb_out;
                pair->p_slot[1]->p_mod = &out_null;
                p_chan->p_slot[0]->p_mod = &pair->p_slot[1]->output;
                p_chan->p_slot[1]->p_mod = &p_chan->p_slot[0]->output;
                p_chan->p_out[0] = &pair->p_slot[0]->output;
                p_chan->p_out[1] = &p_chan->p_slot[1]->output;
                p_chan->p_out[2] = &out_null;
                p_chan->p_out[3] = &out_null;
                break;
            case 3:   // AM-FM
                pair->p_slot[0]->p_mod = &pair->p_slot[0]->fb_out;
                pair->p_slot[1]->p_mod = &out_null;
                p_chan->p_slot[0]->p_mod = &pair->p_slot[1]->output;
                p_chan->p_slot[1]->p_mod = &out_null;
                p_chan->p_out[0] = &pair->p_slot[0]->output;
                p_chan->p_out[1] = &p_chan->p_slot[0]->output;
                p_chan->p_out[2] = &p_chan->p_slot[1]->output;
                p_chan->p_out[3] = &out_null;
                break;
            }
            break;
        }

        case CH_RHYTHM:
            if ((p_chan->alg & 1) == 0) {
                // FM
                p_chan->p_slot[0]->p_mod = &p_chan->p_slot[0]->fb_out;
                p_chan->p_slot[1]->p_mod = &p_chan->p_slot[0]->output;
            } else {
                // AM
                p_chan->p_slot[0]->p_mod = &p_chan->p_slot[0]->fb_out;
                p_chan->p_slot[1]->p_mod = &out_null;
            }
            break;

        default:
            break;
        }
    }

    void chan_update_rhythm(uint8_t data)
    {
        rhy_flag = data & 0x3f;
        if (rhy_flag & 0x20) {
            // BD
            chan[6].ch_type = CH_RHYTHM;
            chan[6].p_out[0] = &chan[6].p_slot[1]->output;
            chan[6].p_out[1] = &chan[6].p_slot[1]->output;
            chan[6].p_out[2] = &out_null;
            chan[6].p_out[3] = &out_null;

            chan_set_alg(&chan[6]);
            eg_key_on_off(chan[6].p_slot[0], (rhy_flag & 0x10) != 0);
            eg_key_on_off(chan[6].p_slot[1], (rhy_flag & 0x10) != 0);

            // HH + SD
            chan[7].ch_type = CH_RHYTHM;
            chan[7].p_out[0] = &chan[7].p_slot[0]->output;
            chan[7].p_out[1] = &chan[7].p_slot[0]->output;
            chan[7].p_out[2] = &chan[7].p_slot[1]->output;
            chan[7].p_out[3] = &chan[7].p_slot[1]->output;

            eg_key_on_off(chan[7].p_slot[0], (rhy_flag & 1) != 0);
            eg_key_on_off(chan[7].p_slot[1], (rhy_flag & 8) != 0);

            // TT + TC
            chan[8].ch_type = CH_RHYTHM;
            chan[8].p_out[0] = &chan[8].p_slot[0]->output;
            chan[8].p_out[1] = &chan[8].p_slot[0]->output;
            chan[8].p_out[2] = &chan[8].p_slot[1]->output;
            chan[8].p_out[3] = &chan[8].p_slot[1]->output;

            eg_key_on_off(chan[8].p_slot[0], (rhy_flag & 4) != 0);
            eg_key_on_off(chan[8].p_slot[1], (rhy_flag & 2) != 0);
        } else {
            // チャンネル 6/7/8 を 2OP に戻す
            chan[6].ch_type = CH_2OP;
            chan_set_alg(&chan[6]);
            chan[7].ch_type = CH_2OP;
            chan_set_alg(&chan[7]);
            chan[8].ch_type = CH_2OP;
            chan_set_alg(&chan[8]);
        }
    }

    void update_fnum_block_ksr(Chan* p_chan, uint8_t data, bool msb_flag)
    {
        if (p_chan->ch_type == CH_4OP_2) return;

        if (msb_flag) {
            p_chan->fnum = static_cast<uint16_t>((p_chan->fnum & 0x300) | data);
        } else {
            p_chan->fnum = static_cast<uint16_t>((p_chan->fnum & 0x0ff) | ((data & 3) << 8));
            p_chan->block = (data >> 2) & 7;
        }
        p_chan->ksr = static_cast<uint8_t>((p_chan->block << 1) | ((p_chan->fnum >> (9 - nts_bit)) & 1));

        envelope_update_ksl(p_chan->p_slot[0]);
        envelope_update_ksl(p_chan->p_slot[1]);
        envelope_update_rate(p_chan->p_slot[0]);
        envelope_update_rate(p_chan->p_slot[1]);

        if (p_chan->ch_type == CH_4OP_1) {
            Chan* pair = p_chan->p_chan;
            pair->fnum = p_chan->fnum;
            pair->ksr = p_chan->ksr;
            // オリジナルは block を下位 (A0) 書き込み時にだけペアへコピーしている
            if (msb_flag) pair->block = p_chan->block;

            envelope_update_ksl(pair->p_slot[0]);
            envelope_update_ksl(pair->p_slot[1]);
            envelope_update_rate(pair->p_slot[0]);
            envelope_update_rate(pair->p_slot[1]);
        }
    }

    void update_fb_con(Chan* p_chan, uint8_t data)
    {
        p_chan->fb = (data & 0x0e) >> 1;
        p_chan->con = data & 1;
        p_chan->alg = p_chan->con;

        switch (p_chan->ch_type) {
        case CH_2OP:
        case CH_RHYTHM:
            chan_set_alg(p_chan);
            break;
        case CH_4OP_1:
            p_chan->p_chan->alg = static_cast<uint8_t>(4 | (p_chan->con << 1) | p_chan->p_chan->con);
            chan_set_alg(p_chan->p_chan);
            break;
        case CH_4OP_2:
            p_chan->alg = static_cast<uint8_t>(4 | (p_chan->p_chan->con << 1) | p_chan->con);
            chan_set_alg(p_chan);
            break;
        }

        p_chan->out_l = ((data >> 4) & 1) ? WORD_NULL : 0;
        p_chan->out_r = ((data >> 5) & 1) ? WORD_NULL : 0;
    }

    void update_key(Chan* p_chan, bool key_on)
    {
        switch (p_chan->ch_type) {
        case CH_2OP:
        case CH_4OP_1:
        case CH_RHYTHM:
            eg_key_on_off(p_chan->p_slot[0], key_on);
            eg_key_on_off(p_chan->p_slot[1], key_on);
            if (p_chan->ch_type == CH_4OP_1) {
                eg_key_on_off(p_chan->p_chan->p_slot[0], key_on);
                eg_key_on_off(p_chan->p_chan->p_slot[1], key_on);
            }
            break;
        default:
            break;
        }
    }

    void chan_update_4op(uint8_t data)
    {
        for (int bit_num = 0; bit_num <= 5; ++bit_num) {
            const ChanType t1 = ((data >> bit_num) & 1) ? CH_4OP_1 : CH_2OP;
            const ChanType t2 = ((data >> bit_num) & 1) ? CH_4OP_2 : CH_2OP;
            chan[CH_4OP_IDX[bit_num]].ch_type = t1;
            chan[CH_4OP_IDX[bit_num] + 3].ch_type = t2;
        }
    }

    void generate_rhythm_slots()
    {
        uint16_t phase_lo = 0, phase_hi = 0, phase_bit = 0;

        auto calc_phase_slot7_slot8 = [&]() {
            phase_lo = (chan[7].p_slot[0]->pg_phase >> 9) & 0x3ff;
            phase_hi = (chan[8].p_slot[1]->pg_phase >> 9) & 0x3ff;
            phase_bit = ((phase_lo & 8) | (((phase_lo >> 5) ^ phase_lo) & 4) | (((phase_hi >> 2) ^ phase_hi) & 8)) != 0 ? 1 : 0;
        };

        // BD
        Slot* p_slot = chan[6].p_slot[0];
        slot_generate(p_slot, static_cast<uint16_t>(static_cast<uint16_t>(p_slot->pg_phase >> 9) + *p_slot->p_mod));
        p_slot = chan[6].p_slot[1];
        slot_generate(p_slot, static_cast<uint16_t>(static_cast<uint16_t>(p_slot->pg_phase >> 9) + *p_slot->p_mod));

        // HH
        p_slot = chan[7].p_slot[0];
        calc_phase_slot7_slot8();
        uint16_t phase = static_cast<uint16_t>((phase_bit << 9) | static_cast<uint16_t>(0x34 << (phase_bit ^ ((noise & 1) << 1))));
        slot_generate(p_slot, phase);

        // TT
        p_slot = chan[8].p_slot[0];
        slot_generate(p_slot, static_cast<uint16_t>(p_slot->pg_phase >> 9));

        // SD
        p_slot = chan[7].p_slot[1];
        calc_phase_slot7_slot8();
        phase = static_cast<uint16_t>((0x100 << ((phase_lo >> 8) & 1)) ^ static_cast<uint16_t>((noise & 1) << 8));
        slot_generate(p_slot, phase);

        // TC
        p_slot = chan[8].p_slot[1];
        phase = static_cast<uint16_t>(0x100 | (phase_bit << 9));
        slot_generate(p_slot, phase);
    }

    // -----------------------------------------------------------------------
    // 生成・初期化・レジスタ書き込み
    // -----------------------------------------------------------------------

    // 戻り値: 下位 16bit = 左、上位 16bit = 右
    uint32_t chip_generate()
    {
        for (int slot_num = 0; slot_num <= 35; ++slot_num) {
            slot_calc_fb(&slot[slot_num]);
            pg_generate(&slot[slot_num]);
            envelope_calc(&slot[slot_num]);
            slot_generate(&slot[slot_num], WORD_NULL);
        }

        // リズムモード
        if (rhy_flag & 0x20) generate_rhythm_slots();

        // ミキサー (出力は 1 サンプル遅れる)
        const int16_t lr_mix1 = static_cast<int16_t>(limit_value(output[1], -0x7fff, 0x7fff));
        const int16_t lr_mix0 = static_cast<int16_t>(limit_value(output[0], -0x7fff, 0x7fff));
        output[0] = 0;
        output[1] = 0;

        for (int n = 0; n <= 17; ++n) {
            const int16_t accm = static_cast<int16_t>(*chan[n].p_out[0] + *chan[n].p_out[1] + *chan[n].p_out[2] + *chan[n].p_out[3]);
            const int16_t l = static_cast<int16_t>(static_cast<uint16_t>(accm) & chan[n].out_l);
            const int16_t r = static_cast<int16_t>(static_cast<uint16_t>(accm) & chan[n].out_r);
            output[0] += l;
            out_l[n] = l;
            output[1] += r;
            out_r[n] = r;
        }

        // LFO トレモロの更新
        if ((timer & 0x3f) == 0x3f) {
            if (trem_dir == 0) {
                if (trem_pos < 105) {
                    ++trem_pos;
                } else {
                    --trem_pos;
                    trem_dir = 1;
                }
            } else if (trem_pos > 0) {
                --trem_pos;
            } else {
                ++trem_pos;
                trem_dir = 0;
            }
            trem_val = static_cast<uint8_t>((trem_pos >> 2) >> ((1 - dva_bit) << 1));
        }

        // ノイズジェネレータの更新
        if (noise & 1) noise ^= NOISE_XOR;
        noise >>= 1;
        ++timer;

        return (static_cast<uint32_t>(static_cast<uint16_t>(lr_mix1)) << 16) | static_cast<uint16_t>(lr_mix0);
    }

    // OPL3EMU_init (レジスタ値・位相などは保持される)
    void init()
    {
        for (auto& s : slot) {
            s.p_chip = this;
            s.p_mod = &out_null;
            s.eg_rout = 0x1ff;
            s.eg_out = 0x1ff;
            s.eg_state = EG_OFF;
            s.p_trem = null_byte();
        }

        for (int chan_num = 0; chan_num <= 17; ++chan_num) {
            Chan& c = chan[chan_num];
            out_l[chan_num] = 0;
            out_r[chan_num] = 0;
            c.out_l = WORD_NULL;
            c.out_r = WORD_NULL;
            c.p_chip = this;
            c.ch_type = CH_2OP;
            c.p_out[0] = &out_null;
            c.p_out[1] = &out_null;
            c.p_out[2] = &out_null;
            c.p_out[3] = &out_null;
            c.p_slot[0] = &slot[CH_SLOT_IDX[chan_num]];
            c.p_slot[1] = &slot[CH_SLOT_IDX[chan_num] + 3];
            slot[CH_SLOT_IDX[chan_num]].p_chan = &c;
            slot[CH_SLOT_IDX[chan_num] + 3].p_chan = &c;

            if (CH_4OP_MASK[chan_num] != 0) c.p_chan = &chan[CH_4OP_MASK[chan_num] - 1];

            chan_set_alg(&c);
        }

        noise = NOISE_HASH_VAL;
        timer = 0;
        nts_bit = 0;
        dva_bit = 0;
        dvb_bit = 0;
        rhy_flag = 0;
        vib_pos = 0;
        trem_dir = 0;
        trem_pos = 0;
        trem_val = 0;
        output[0] = 0;
        output[1] = 0;
    }

    void write_reg(uint16_t reg, uint8_t data)
    {
        reg &= 0x1ff;
        const uint8_t reg_hi = (reg >> 8) & 1;
        const uint8_t reg_lo = reg & 0x0ff;
        const auto slot_of = [&](uint8_t r) { return &slot[18 * reg_hi + CH_5BIT_MASK[r & 0x1f] - 1]; };

        if (reg_lo >= 0x01 && reg_lo <= 0x08) {
            if (reg_hi != 0) {
                if ((reg_lo & 0x0f) == 4) chan_update_4op(data);   // 4OP 接続選択
            } else if ((reg_lo & 0x0f) == 8) {
                nts_bit = (data >> 6) & 1;   // NTS
            }
        } else if (reg_lo >= 0x20 && reg_lo <= 0x35) {
            if (CH_5BIT_MASK[reg_lo & 0x1f] != 0) update_lfo_eg_ksr_mult(slot_of(reg_lo), data);
        } else if (reg_lo >= 0x40 && reg_lo <= 0x55) {
            if (CH_5BIT_MASK[reg_lo & 0x1f] != 0) update_ksl_tl(slot_of(reg_lo), data);
        } else if (reg_lo >= 0x60 && reg_lo <= 0x75) {
            if (CH_5BIT_MASK[reg_lo & 0x1f] != 0) update_ar_dr(slot_of(reg_lo), data);
        } else if (reg_lo >= 0x80 && reg_lo <= 0x95) {
            if (CH_5BIT_MASK[reg_lo & 0x1f] != 0) update_sl_rr(slot_of(reg_lo), data);
        } else if (reg_lo >= 0xa0 && reg_lo <= 0xa8) {
            update_fnum_block_ksr(&chan[9 * reg_hi + (reg_lo & 0x0f)], data, true);
        } else if (reg_lo >= 0xb0 && reg_lo <= 0xb8) {
            Chan* c = &chan[9 * reg_hi + (reg_lo & 0x0f)];
            update_fnum_block_ksr(c, data, false);
            update_key(c, (data & 0x20) != 0);
        } else if (reg_lo == 0xbd) {
            if (reg_hi == 0) {
                dva_bit = data >> 7;           // DAM (LFO トレモロ)
                dvb_bit = (data >> 6) & 1;     // DVB (LFO ビブラート)
                chan_update_rhythm(data);
            }
        } else if (reg_lo >= 0xc0 && reg_lo <= 0xc8) {
            update_fb_con(&chan[9 * reg_hi + (reg_lo & 0x0f)], data);
        } else if (reg_lo >= 0xe0 && reg_lo <= 0xf5) {
            if (CH_5BIT_MASK[reg_lo & 0x1f] != 0) slot_of(reg_lo)->reg_wf = data & 7;
        }
    }
};

Opl3EmulatorAt2::Opl3EmulatorAt2()
    : chip_(std::make_unique<Chip>())
{
    std::memset(chip_.get(), 0, sizeof(Chip));
    chip_->init();
}

Opl3EmulatorAt2::~Opl3EmulatorAt2() = default;

void Opl3EmulatorAt2::reset()
{
    std::lock_guard lock(mutex_);
    chip_->init();
}

void Opl3EmulatorAt2::writeReg(uint16_t reg, uint8_t value)
{
    std::lock_guard lock(mutex_);
    chip_->write_reg(reg, value);
}

void Opl3EmulatorAt2::render(int16_t* buffer, uint32_t frames)
{
    std::lock_guard lock(mutex_);
    for (uint32_t i = 0; i < frames; ++i) {
        const uint32_t sample = chip_->chip_generate();
        buffer[i * 2] = static_cast<int16_t>(sample & 0xffff);
        buffer[i * 2 + 1] = static_cast<int16_t>(sample >> 16);
    }
}
