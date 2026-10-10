#include "StringIO.h"

#include <algorithm>
#include <cmath>

namespace at2
{

std::string byte2hex(uint8_t value)
{
    static const char data[] = "0123456789ABCDEF";
    return { data[(value & 0xf0) >> 4], data[value & 0x0f] };
}

std::string byte2dec(uint8_t value)
{
    static const char data[] = "0123456789";
    if (value < 100) return { data[value / 10], data[value % 10] };
    return { data[value / 100], data[value % 100 / 10], data[value % 100 % 10] };
}

std::string Num2str(int32_t num, uint8_t base)
{
    static const char hexa[] = "0123456789ABCDEF";
    std::string result;
    if (base >= 2 && base <= 16) {
        while (num > 0) {
            result.insert(result.begin(), hexa[num % base]);
            num /= base;
        }
    }
    return result.empty() ? "0" : result;
}

std::string ExpStrL(std::string str, uint8_t size, char chr)
{
    while (str.size() < size) str.insert(str.begin(), chr);
    return str;
}

std::string ExpStrR(std::string str, uint8_t size, char chr)
{
    while (str.size() < size) str.push_back(chr);
    return str;
}

std::string CenterStr(std::string str, uint8_t size)
{
    bool flag = false;
    while (str.size() < size) {
        if (flag) str.insert(str.begin(), ' ');
        else str.push_back(' ');
        flag = !flag;
    }
    return str;
}

std::string FlipStr(const std::string& str)
{
    return std::string(str.rbegin(), str.rend());
}

std::string iCASE(const std::string& str)
{
    std::string result = str;
    for (char& c : result) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        if (c == 'I') c = 'i';
    }
    return result;
}

std::string Bpm2str(double bpm)
{
    if (bpm < 1000) {
        const double t = std::trunc(bpm);
        return Num2str(static_cast<int32_t>(t), 10) + "." + Num2str(static_cast<int32_t>(std::trunc((bpm - t) * 10)), 10);
    }
    return Num2str(static_cast<int32_t>(std::nearbyint(bpm)), 10);
}

std::string Copy(const std::string& str, int index, int count)
{
    if (index < 1) index = 1;
    if (index > static_cast<int>(str.size()) || count <= 0) return std::string();
    return str.substr(index - 1, count);
}

} // namespace at2
