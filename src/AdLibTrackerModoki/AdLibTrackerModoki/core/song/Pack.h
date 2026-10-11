#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

// LZH 圧縮 (adt2pack.pas の LZH_compress / LZH_compress_ultra の移植)
//
// 出力の先頭 5 バイトはオリジナルと同じく [圧縮モード (0: 通常, 1: ultra)][元のサイズ (4 バイト)]。
// オリジナルは Turbo Pascal モード (Integer = 16 ビット) でコンパイルされており、変数への代入で
// 16 ビットに切り詰められる。本家と同じ出力を得るため、同じ型・同じ計算順で処理する。

// 進捗の通知 (読み込んだバイト数, 全体のバイト数)。オリジナルの ReadDataBlock から呼ばれる show_progress に相当。
using LzhProgress = std::function<void(uint32_t done, uint32_t total)>;

// 通常の圧縮 (A2M / A2P の保存で使う)
size_t LZH_compress(const uint8_t* source, uint32_t size, std::vector<uint8_t>& dest, const LzhProgress& progress = {});

// ultra 圧縮 (A2T の保存で使う)
size_t LZH_compress_ultra(const uint8_t* source, uint32_t size, std::vector<uint8_t>& dest, const LzhProgress& progress = {});
