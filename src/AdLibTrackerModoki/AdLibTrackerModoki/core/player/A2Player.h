#pragma once

#include <cstdint>
#include <memory>

#include "../song/Song.h"

class IOplDevice;

// AdLib Tracker II の再生エンジン (adt2unit.pas の再生部分の移植)
//
// 内部の関数名・変数名は、オリジナルとの対応を追えるよう Pascal 版の名前をそのまま使っている。
// エフェクト列 1/2 で重複していた処理は slot (0/1) を引数に取る形にまとめている。
//
// スレッド安全ではない。呼び出し側で排他すること。
class A2Player
{
public:
    enum class PlayStatus { Playing, Paused, Stopped };

    explicit A2Player(IOplDevice& opl);

    // 曲を差し替える (再生中なら停止する)。ロード直後の設定反映 (load_flag = 1 の処理) も行う。
    void setSong(std::shared_ptr<Song> song);
    const std::shared_ptr<Song>& song() const { return song_; }

    void startPlaying();
    void stopPlaying();

    // タイマー 1 回分の処理 (SDL 版 play_callback_proc のフレーム境界で行う処理)
    void timerTick();

    // 現在のタイマー周波数 (TimerSetup に渡された値)
    int32_t timerHz() const { return timer_hz_; }

    PlayStatus playStatus() const { return play_status; }
    uint8_t currentOrder() const { return current_order; }
    uint8_t currentPattern() const { return current_pattern; }
    uint8_t currentLine() const { return current_line; }
    uint8_t currentSpeed() const { return speed; }
    uint8_t currentTempo() const { return tempo; }
    double timePlaying() const { return time_playing; }

    // 指定位置まで早送りしてから再生する (adt2unit.pas の calibrate_player から画面処理を除いたもの)
    void calibrate_player(uint8_t order, uint8_t line, bool line_dependent);

    // 音量バー表示用の値をリセットする (adt2extn.pas の nul_volume_bars)
    void nul_volume_bars();

    // 以下の状態と手続きは Pascal 版ではユニットのグローバル変数・手続きであり、
    // 画面処理 (ステータス表示・パターン表示など) から直接参照されるため公開している。
    // オーディオスレッドと排他して使うこと。
    struct VibTremState
    {
        uint8_t pos;
        uint8_t dir;
        uint8_t speed;
        uint8_t depth;
        bool fine;
    };

    struct AdsrwState
    {
        uint8_t attck, dec, sustn, rel, wform;
    };

    struct FmParameterTable   // tFM_PARAMETER_TABLE
    {
        AdsrwState adsrw_car;
        AdsrwState adsrw_mod;
        uint8_t connect;
        uint8_t feedb;
        uint8_t multipM, kslM, tremM, vibrM, ksrM, sustM;
        uint8_t multipC, kslC, tremC, vibrC, ksrC, sustC;
    };

    struct PortaState
    {
        uint16_t freq;
        uint8_t speed;
    };

    struct ArpggState
    {
        uint8_t state, note, add1, add2;
    };

    struct TremorState
    {
        int16_t pos;
        uint16_t volume;
    };

    struct MacroState
    {
        uint16_t fmreg_pos, arpg_pos, vib_pos;
        uint8_t fmreg_count, fmreg_duration, arpg_count, vib_count, vib_delay;
        bool vib_paused;
        uint8_t fmreg_table, arpg_table, vib_table;
        uint8_t arpg_note;
        uint16_t vib_freq;
    };

    struct DecayBar
    {
        int16_t dir1, dir2;
        double lvl1, lvl2;
    };

    struct VolumBar
    {
        int16_t dir;
        double lvl;
    };

    struct FourOpData   // _4op_data_flag の各フィールド
    {
        bool mode;
        uint8_t conn;
        uint8_t ch1, ch2;
        uint8_t ins1, ins2;
    };

    static constexpr int NCH = 21;   // チャンネルは 1..20

    // --- 出力 ---
    void opl3out(uint16_t reg, uint16_t data);

    // --- 曲データ参照 ---
    FixedSongData& songdata() { return song_->data(); }
    // pattern_order[index] (オリジナル同様 128 以上は後続フィールドを読む)
    uint8_t order_at(uint8_t index) { return reinterpret_cast<const uint8_t*>(songdata().pattern_order)[index]; }
    uint8_t ins_parameter(uint8_t ins, uint8_t param);
    bool is_chan_adsr_data_empty(uint8_t chan);
    bool is_ins_adsr_data_empty(uint8_t ins);
    bool is_4op_chan(uint8_t chan);

    // --- 周波数 ---
    static uint16_t nFreq(uint8_t note);
    static uint16_t calc_freq_shift_up(uint16_t freq, uint16_t shift);
    static uint16_t calc_freq_shift_down(uint16_t freq, uint16_t shift);
    uint16_t calc_vibtrem_shift(VibTremState& table);
    void change_freq(uint8_t chan, uint16_t freq);
    void change_frequency(uint8_t chan, uint16_t freq);

    // --- タイマー ---
    void TimerSetup(int32_t hz) { timer_hz_ = hz; }
    void update_timer(uint16_t hz);
    void update_playback_speed(int32_t speed_shift);
    uint16_t _macro_speedup() const;
    void synchronize_song_timer();

    // --- 音量・音色 ---
    void key_on(uint8_t chan);
    void key_off(uint8_t chan);
    void release_sustaining_sound(uint8_t chan);
    static uint8_t scale_volume(uint8_t volume, uint8_t scale_factor);
    FourOpData _4op_data_flag(uint8_t chan);
    bool _4op_vol_valid_chan(uint8_t chan);
    void set_ins_volume(uint8_t modulator, uint8_t carrier, uint8_t chan);
    void set_ins_volume_4op(uint8_t volume, uint8_t chan);
    void reset_ins_volume(uint8_t chan);
    void set_ins_data(uint8_t ins, uint8_t chan);
    // 楽器データ (tFM_INST_DATA の先頭 12 バイト) を直接チャンネルに設定する (エディタのプレビュー用)
    void load_instrument(const uint8_t* data, uint8_t chan);
    void update_modulator_adsrw(uint8_t chan);
    void update_carrier_adsrw(uint8_t chan);
    void update_fmpar(uint8_t chan);
    void reset_chan_data(uint8_t chan);
    void init_macro_table(uint8_t chan, uint8_t note, uint8_t ins, uint16_t freq);
    void output_note(uint8_t note, uint8_t ins, uint8_t chan, bool restart_macro, bool restart_adsr);
    void generate_custom_vibrato(uint8_t value);
    void set_global_volume();

    // --- 行の処理 ---
    bool no_loop(uint8_t current_chan, uint8_t line);
    void play_line();
    void play_line_global_fslide(int slot, uint8_t chan, const Chunk& ev);
    void play_line_effect(int slot, uint8_t chan, const Chunk& ev);
    void play_line_swap_macro(int slot, uint8_t chan, const Chunk& ev);

    // --- エフェクトの更新 ---
    void portamento_up(uint8_t chan, uint16_t slide, uint16_t limit);
    void portamento_down(uint8_t chan, uint16_t slide, uint16_t limit);
    void macro_vibrato__porta_up(uint8_t chan, uint8_t depth);
    void macro_vibrato__porta_down(uint8_t chan, uint8_t depth);
    void tone_portamento(int slot, uint8_t chan);
    void slide_carrier_volume_up(uint8_t chan, uint8_t slide, uint8_t limit);
    void slide_modulator_volume_up(uint8_t chan, uint8_t slide, uint8_t limit);
    void slide_volume_up(uint8_t chan, uint8_t slide);
    void slide_carrier_volume_down(uint8_t chan, uint8_t slide);
    void slide_modulator_volume_down(uint8_t chan, uint8_t slide);
    void slide_volume_down(uint8_t chan, uint8_t slide);
    void volume_slide(uint8_t chan, uint8_t up_speed, uint8_t down_speed);
    void global_volume_slide(uint8_t up_speed, uint8_t down_speed);
    void arpeggio(int slot, uint8_t chan);
    void vibrato(int slot, uint8_t chan);
    void tremolo(int slot, uint8_t chan);
    uint8_t chanvol(uint8_t chan);
    void update_effects();
    void update_effects_slot(int slot, uint8_t chan);
    void update_fine_effects(uint8_t chan);
    void update_fine_effects_slot(int slot, uint8_t chan);
    void update_extra_fine_effects();
    void update_extra_fine_effects_slot(int slot, uint8_t chan);

    // --- 演奏位置 ---
    int calc_following_order(uint8_t order);
    int calc_order_jump();
    uint8_t calc_pattern_pos(uint8_t pattern);
    void update_song_position();
    void poll_proc();
    void macro_poll_proc();

    // --- 初期化 ---
    void init_buffers();
    void init_player();
    void reset_player();
    void apply_song_flags();
    // percussion_mode に合わせて _chan_n / _chan_m / _chan_c を _chmm_* か _chpm_* にする
    void select_chan_tables();

    IOplDevice& opl_;
    std::shared_ptr<Song> song_;
    int32_t timer_hz_ = 50;

    // 設定 (adtrack2.ini の既定値)
    bool timer_fix = true;
    uint8_t fforward_factor = 1;
    uint8_t rewind_factor = 1;
    uint8_t mark_line = 4;

    // adt2unit.pas のグローバル変数
    uint16_t IRQ_freq = 50;
    int16_t IRQ_freq_shift = 0;
    uint8_t tempo = 50;
    uint8_t speed = 6;
    uint16_t macro_speedup = 1;
    bool repeat_pattern = false;
    bool fast_forward = false;
    bool rewind = false;
    bool pattern_break = false;
    bool pattern_delay = false;
    uint8_t next_line = 0;
    uint8_t start_order = 0xff;
    uint8_t start_pattern = 0xff;
    uint8_t start_line = 0xff;
    bool replay_forbidden = true;
    bool single_play = false;
    bool debugging = false;
    bool space_pressed = false;
    bool no_step_debugging = false;
    bool play_single_patt = false;
    bool skip_macro_flag = false;
    bool force_macro_keyon = false;
    bool no_sync_playing = false;
    bool calibrating = false;

    uint8_t vibtrem_speed_factor = 1;
    uint8_t vibtrem_table_size = 32;
    uint8_t vibtrem_table[256] = {};

    VibTremState vibr_table[2][NCH] = {};
    VibTremState trem_table[2][NCH] = {};

    FmParameterTable fmpar_table[NCH] = {};
    bool volume_lock[NCH] = {};
    bool vol4op_lock[NCH] = {};
    uint16_t volume_table[NCH] = {};
    uint16_t vscale_table[NCH] = {};
    bool peak_lock[NCH] = {};
    bool pan_lock[NCH] = {};
    uint8_t modulator_vol[NCH] = {};
    uint8_t carrier_vol[NCH] = {};
    DecayBar decay_bar[NCH] = {};
    VolumBar volum_bar[NCH] = {};
    bool channel_flag[NCH] = {};
    Chunk event_table[NCH] = {};
    uint8_t voice_table[NCH] = {};
    uint16_t freq_table[NCH] = {};
    uint16_t zero_fq_table[NCH] = {};
    uint16_t effect_table[2][NCH] = {};
    uint8_t fslide_table[2][NCH] = {};
    uint16_t glfsld_table[2][NCH] = {};
    PortaState porta_table[2][NCH] = {};
    bool portaFK_table[NCH] = {};
    ArpggState arpgg_table[2][NCH] = {};
    uint8_t retrig_table[2][NCH] = {};
    TremorState tremor_table[2][NCH] = {};
    uint8_t panning_table[NCH] = {};
    uint16_t last_effect[2][NCH] = {};
    uint8_t volslide_type[NCH] = {};
    bool event_new[NCH] = {};
    uint16_t freqtable2[NCH] = {};
    uint8_t notedel_table[NCH] = {};
    uint8_t notecut_table[NCH] = {};
    int8_t ftune_table[NCH] = {};
    bool keyoff_loop[NCH] = {};
    MacroState macro_table[NCH] = {};
    uint8_t loopbck_table[NCH] = {};
    uint8_t loop_table[NCH][256] = {};
    uint8_t misc_register = 0;
    uint8_t ai_table[256] = {};

    uint8_t overall_volume = 63;
    uint8_t global_volume = 63;
    uint8_t fade_out_volume = 63;
    int32_t playback_speed_shift = 0;
    PlayStatus play_status = PlayStatus::Stopped;
    uint16_t play_pos_buf[10] = {};

    uint8_t current_order = 0;
    uint8_t current_pattern = 0;
    uint8_t current_line = 0;
    uint8_t current_tremolo_depth = 0;
    uint8_t current_vibrato_depth = 0;

    bool reset_chan[NCH] = {};
    bool reset_adsrw[NCH] = {};
    bool ignore_note_once[NCH] = {};
    bool seek_pattern_break = false;

    bool speed_update = false;
    bool lockvol = false;
    bool panlock = false;
    bool lockVP = false;
    uint8_t tremolo_depth = 0;
    uint8_t vibrato_depth = 0;
    bool volume_scaling = false;
    bool percussion_mode = false;
    uint8_t last_order = 0;

    int32_t song_timer = 0;
    int32_t timer_temp = 0;
    int32_t song_timer_tenths = 0;
    int32_t ticks = 0;
    int32_t tick0 = 0;
    int32_t tickD = 0;
    int32_t tickXF = 0;
    double time_playing = 0;
    double time_playing0 = 0;

    int32_t ticklooper = 0;
    int32_t macro_ticklooper = 0;

    uint16_t _chan_n[NCH] = {};
    uint16_t _chan_m[NCH] = {};
    uint16_t _chan_c[NCH] = {};

    // play_line のローカル変数 (eLo/eHi)
    uint8_t eLo_[2][NCH] = {};
    uint8_t eHi_[2][NCH] = {};
};
