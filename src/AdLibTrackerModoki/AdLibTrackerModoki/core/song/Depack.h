#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// A2M/A2T で使われる展開アルゴリズム (depackio.pas / adt2pack.pas の移植)
//
// オリジナルは入力の終端を超えて読むことがあるため、範囲外は 0 として扱う。
// 出力は dest に格納し、オリジナルと同じ「出力サイズ」を戻り値で返す。

// Lempel-Ziv-Storer-Szymanski
size_t LZSS_decompress(const uint8_t* source, size_t sourceSize, uint16_t size, std::vector<uint8_t>& dest);

// Lempel-Ziv-Welch
size_t LZW_decompress(const uint8_t* source, size_t sourceSize, std::vector<uint8_t>& dest);

// SixPack (Philip G. Gage)
size_t SIXPACK_decompress(const uint8_t* source, size_t sourceSize, uint16_t size, std::vector<uint8_t>& dest);

// aPack (Joergen Ibsen)
size_t APACK_decompress(const uint8_t* source, size_t sourceSize, std::vector<uint8_t>& dest);

// LZH (AT2 独自の LHA 系圧縮)
size_t LZH_decompress(const uint8_t* source, size_t sourceSize, uint32_t size, std::vector<uint8_t>& dest);

// CRC32 (parserio.pas の Update32)
uint32_t Update32(const void* buf, size_t size, uint32_t crc);

// RDC (Ed Ross の Run-length/Dictionary Compression。MPU-401 TRAKKER の MTK で使われる)
// 戻り値はオリジナルと同じく Word に切り詰めた出力サイズ
size_t RDC_decompress(const uint8_t* source, size_t sourceSize, uint16_t size, std::vector<uint8_t>& dest);

// CRC16 (parserio.pas の Update16)
uint16_t Update16(const void* buf, size_t size, uint16_t crc);
