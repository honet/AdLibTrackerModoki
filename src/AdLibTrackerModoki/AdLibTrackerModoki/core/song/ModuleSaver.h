#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "Pack.h"
#include "Song.h"

// 曲ファイルの保存 (adt2extn.pas の _a2m_saver / _a2t_saver / _a2p_saver のうちデータの組み立てと書き込み)
//
// ファイル名の決定・上書きの確認・進捗表示は画面側で行う。
// オリジナル同様、保存の際に曲データ (名前の未使用部分の 0 埋め、common_flag、bpm_data) を書き換える。

// 再生エンジンが持っていて、保存時に曲データへ書き戻す設定
struct SongSaveState
{
    uint8_t common_flag = 0;     // speed_update / lockvol / lockVP / tremolo_depth / vibrato_depth / panlock / percussion_mode / volume_scaling
    uint8_t mark_line = 4;       // bpm_data.rows_per_beat
    int16_t tempo_finetune = 0;  // IRQ_freq_shift
};

enum class SaveResult
{
    Ok,
    WriteError,   // ERROR WRiTiNG DATA - DiSK ERROR? (ファイルを作れない)
    DiskFull,     // ERROR WRiTiNG DATA - DiSK FULL? (書き込みに失敗)
};

// 進捗表示 (A2M / A2T)。オリジナルと同じ時点で呼ぶ。
struct SaveProgress
{
    std::function<void(uint8_t num_steps)> begin;   // ヘッダーを書いた後 (進捗ウィンドウを出す)
    std::function<void()> step;                     // ブロックを 1 つ書き終えた (Inc(progress_step))
    LzhProgress value;                              // 圧縮中のブロックの進み具合
};

SaveResult SaveA2m(const std::wstring& path, Song& song, const SongSaveState& state, const SaveProgress* progress = nullptr);
SaveResult SaveA2t(const std::wstring& path, Song& song, const SongSaveState& state, const SaveProgress* progress = nullptr);

// パターン pattern を A2P として保存する
SaveResult SaveA2p(const std::wstring& path, Song& song, uint8_t pattern);
