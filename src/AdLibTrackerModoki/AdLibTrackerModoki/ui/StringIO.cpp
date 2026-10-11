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

tCHARSET CharRange(uint8_t first, uint8_t last)
{
    tCHARSET set;
    for (int c = first; c <= last; ++c) set.set(static_cast<size_t>(c));
    return set;
}

tCHARSET CharSet(const std::string& chars)
{
    tCHARSET set;
    for (const char c : chars) set.set(static_cast<uint8_t>(c));
    return set;
}

const tCHARSET DEC_NUM_CHARSET = CharRange('0', '9');
const tCHARSET HEX_NUM_CHARSET = CharRange('0', '9') | CharRange('a', 'f') | CharRange('A', 'F');

int32_t Str2num(const std::string& str, uint8_t base)
{
    static const char digits[] = "0123456789ABCDEF";
    int32_t value = 0;
    if (base == 2 || base == 10 || base == 16) {
        const size_t len = str.size();
        for (size_t index = 1; index <= len; ++index) {
            char digit = str[index - 1];
            if (digit >= 'a' && digit <= 'z') digit = static_cast<char>(digit - 'a' + 'A');
            uint8_t d = 15;   // Digit2index (見つからなければ 0)
            while (d > 0 && digit != digits[d]) --d;
            int32_t position_value = 1;
            for (size_t i = 2; i <= len - index + 1; ++i) position_value *= base;
            value += d * position_value;
        }
    }
    return value;
}

std::string FilterStr2(std::string str, const tCHARSET& chr0, char chr1)
{
    static const char _treat_char[] = "CueaaaaceeeiiiAAE_AooouuyOU_____aiounN";   // [$80..$a5]
    for (char& c : str) {
        if (!InSet(chr0, c)) {
            const uint8_t b = static_cast<uint8_t>(c);
            if (b >= 0x80 && b <= 0xa5) c = _treat_char[b - 0x80];
            else if (b == 0) c = ' ';
            else c = chr1;
        }
    }
    return str;
}

std::string CutStrR(std::string str, uint8_t margin)
{
    const int len = static_cast<int>(str.size());
    if (margin > len) margin = static_cast<uint8_t>(len);
    // str[0] は Pascal では長さのバイト
    const auto at = [&](int i) { return i == 0 ? static_cast<char>(len) : str[static_cast<size_t>(i - 1)]; };
    int idx = 0;
    while (len - idx >= 0 && at(len - idx) == ' ' && len - idx >= margin) ++idx;
    str.resize(static_cast<size_t>(std::max(0, len - idx)));
    return str;
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

std::string DietStr(std::string str, uint8_t size)
{
    if (str.size() <= size) return str;
    // オリジナルは size < 3 のとき終わらないため、ここでは size 文字に切るだけにする
    if (size < 3) return str.substr(0, size);

    const size_t pos = size / 2;   // Delete / Insert の位置 (1 始まり)
    do {
        str.erase(pos - 1, 1);
    } while (str.size() + 3 != size);
    str.insert(pos - 1, "...");
    return str;
}

} // namespace at2
