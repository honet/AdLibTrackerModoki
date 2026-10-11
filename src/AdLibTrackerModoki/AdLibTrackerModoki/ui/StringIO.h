#pragma once

#include <cstdint>
#include <string>

// 文字列補助関数 (stringio.pas の SDL 版の移植)。文字列は CP437 のバイト列。
namespace at2
{

std::string byte2hex(uint8_t value);
std::string byte2dec(uint8_t value);
std::string Num2str(int32_t num, uint8_t base);
std::string ExpStrL(std::string str, uint8_t size, char chr);
std::string ExpStrR(std::string str, uint8_t size, char chr);
std::string CenterStr(std::string str, uint8_t size);
std::string FlipStr(const std::string& str);
std::string iCASE(const std::string& str);
std::string Bpm2str(double bpm);

// 長い文字列の中ほどを "..." に置き換えて size 文字に縮める
std::string DietStr(std::string str, uint8_t size);

// Pascal の Copy(str, index, count) (index は 1 始まり)
std::string Copy(const std::string& str, int index, int count);

} // namespace at2
