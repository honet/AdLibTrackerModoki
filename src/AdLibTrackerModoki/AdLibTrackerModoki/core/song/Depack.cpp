#include "Depack.h"

#include <array>
#include <cstring>
#include <memory>

namespace
{
    // 範囲外を 0 として読む入力
    struct Input
    {
        const uint8_t* data;
        size_t size;

        uint8_t at(size_t i) const { return i < size ? data[i] : 0; }
    };

    // 壊れたデータで無限に出力し続けないための上限
    constexpr size_t MaxOutputSize = 16 * 1024 * 1024;
}

// ---------------------------------------------------------------------------
// LZSS
// ---------------------------------------------------------------------------

size_t LZSS_decompress(const uint8_t* source, size_t sourceSize, uint16_t size, std::vector<uint8_t>& dest)
{
    constexpr uint32_t N_BITS = 12;
    constexpr uint32_t F_BITS = 4;
    constexpr uint32_t THRESHOLD = 2;
    constexpr uint32_t N = 1 << N_BITS;
    constexpr uint32_t F = (1 << F_BITS) + THRESHOLD;

    const Input in{ source, sourceSize };
    std::vector<uint8_t> work(64 * 1024, 0);
    dest.clear();

    uint16_t ibufCount = 0;
    const uint16_t ibufSize = size;
    uint8_t al = 0;
    uint8_t ch = 0;
    uint8_t cl = 0;
    uint16_t dx = 0;
    uint32_t ebx = 0;
    uint32_t edi = N - F;

    auto getChar = [&]() -> bool {
        if (ibufCount < ibufSize) {
            al = in.at(ibufCount);
            ++ibufCount;
            return true;
        }
        return false;
    };

    for (;;) {
        dx >>= 1;
        if ((dx >> 8) == 0) {
            if (!getChar()) break;
            dx = static_cast<uint16_t>(0xff00 | al);
        }
        if (dx & 1) {
            if (!getChar()) break;
            work[edi] = al;
            edi = (edi + 1) & (N - 1);
            dest.push_back(al);
            continue;
        }
        if (!getChar()) break;
        ch = al;
        if (!getChar()) break;
        ebx = (static_cast<uint32_t>(al) << 4) & 0xff00;
        ebx |= ch;
        cl = static_cast<uint8_t>((al & 0x0f) + THRESHOLD);
        ++cl;
        do {
            ebx &= N - 1;
            al = work[ebx];
            work[edi] = al;
            ++edi;
            edi &= N - 1;
            dest.push_back(al);
            ++ebx;
            --cl;
        } while (cl != 0);
    }
    return static_cast<uint16_t>(dest.size());
}

// ---------------------------------------------------------------------------
// LZW
// ---------------------------------------------------------------------------

size_t LZW_decompress(const uint8_t* source, size_t sourceSize, std::vector<uint8_t>& dest)
{
    // オリジナルの表は 5 要素 (le78 = 9..13) だが、le78 は 14 まで増えるため 14bit 分を補っている
    static const uint16_t le7a[6] = { 0x1ff, 0x3ff, 0x7ff, 0x0fff, 0x1fff, 0x3fff };

    const Input in{ source, sourceSize };
    auto work = std::make_unique<std::array<uint8_t, 65536>>();
    auto stack = std::make_unique<std::array<uint8_t, 65536>>();
    work->fill(0);
    dest.clear();

    uint8_t le76 = 0, le77 = 0;
    uint16_t le6a = 0, le6c = 0, le6e = 0, le70 = 0, le72 = 0, le74 = 0, le78 = 0;
    uint16_t le82a = 0, le82b = 0;
    uint16_t ax = 0, bx = 0, cx = 0, sp = 0;
    uint32_t td = 0;

    auto nextCode = [&]() {
        bx = le82a;
        ax = le82b;
        td = (static_cast<uint32_t>(ax) << 16) + bx;
        td = td + le78;
        le82a = static_cast<uint16_t>(td & 0xffff);
        le82b = static_cast<uint16_t>(td >> 16);
        cx = bx & 7;
        td = (static_cast<uint32_t>(ax) << 16) + bx;
        td >>= 3;
        bx = static_cast<uint16_t>(td);
        td = in.at(bx) + (static_cast<uint32_t>(in.at(static_cast<size_t>(bx) + 1)) << 8) +
             (static_cast<uint32_t>(in.at(static_cast<size_t>(bx) + 2)) << 16);
        td >>= cx;
        bx = le78 - 9;
        ax = static_cast<uint16_t>(td & le7a[bx]);
    };

    sp = 0xffff;
    le72 = 0;
    le78 = 9;
    le70 = 0x102;
    le74 = 0x200;

    for (;;) {
        // j1
        nextCode();
        if (ax == 0x101) break;   // j9

        if (ax == 0x100) {
            le78 = 9;
            le74 = 0x200;
            le70 = 0x102;
            nextCode();
            le6a = ax;
            le6c = ax;
            le77 = static_cast<uint8_t>(ax);
            le76 = static_cast<uint8_t>(ax);
            dest.push_back(static_cast<uint8_t>(ax));
            continue;
        }

        // j3
        le6a = ax;
        le6e = ax;
        if (!(ax < le70)) {
            ax = le6c;
            le6a = ax;
            ax = static_cast<uint16_t>((ax & 0xff00) + le76);
            --sp;
            (*stack)[sp] = static_cast<uint8_t>(ax);
            ++le72;
        }

        // j4
        while (le6a > 0xff) {
            bx = static_cast<uint16_t>(le6a * 3);
            ax = static_cast<uint16_t>((ax & 0xff00) + (*work)[static_cast<uint16_t>(bx + 2)]);
            --sp;
            (*stack)[sp] = static_cast<uint8_t>(ax);
            ++le72;
            ax = static_cast<uint16_t>((*work)[bx] + ((*work)[static_cast<uint16_t>(bx + 1)] << 8));
            le6a = ax;
        }

        // j5
        ax = le6a;
        le76 = static_cast<uint8_t>(ax);
        le77 = static_cast<uint8_t>(ax);
        --sp;
        (*stack)[sp] = static_cast<uint8_t>(ax);
        ++le72;
        cx = le72;
        while (cx != 0) {
            ax = (*stack)[sp];
            ++sp;
            dest.push_back(static_cast<uint8_t>(ax));
            --cx;
        }

        // j7
        le72 = 0;
        bx = static_cast<uint16_t>(le70 * 3);
        (*work)[static_cast<uint16_t>(bx + 2)] = le77;
        (*work)[static_cast<uint16_t>(bx + 1)] = static_cast<uint8_t>(le6c >> 8);
        (*work)[bx] = static_cast<uint8_t>(le6c);
        ++le70;
        ax = le6e;
        le6c = ax;
        bx = le70;
        if (bx >= le74 && le78 != 14) {
            ++le78;
            le74 = static_cast<uint16_t>(le74 << 1);
        }

        if (dest.size() > MaxOutputSize) break;
    }
    return static_cast<uint16_t>(dest.size());
}

// ---------------------------------------------------------------------------
// SixPack
// ---------------------------------------------------------------------------

namespace
{
    class SixPack
    {
    public:
        static constexpr uint16_t MAXFREQ = 2000;
        static constexpr uint16_t MINCOPY = 3;
        static constexpr uint16_t MAXCOPY = 255;
        static constexpr uint16_t COPYRANGES = 6;
        static constexpr uint16_t TERMINATE = 256;
        static constexpr uint16_t FIRSTCODE = 257;
        static constexpr uint16_t ROOT = 1;
        static constexpr uint16_t CODESPERRANGE = MAXCOPY - MINCOPY + 1;
        static constexpr uint16_t MAXCHAR = FIRSTCODE + COPYRANGES * CODESPERRANGE - 1;
        static constexpr uint16_t SUCCMAX = MAXCHAR + 1;
        static constexpr uint16_t TWICEMAX = 2 * MAXCHAR + 1;
        static constexpr uint16_t MAXBUF = 64 * 1024 - 1;
        static constexpr uint16_t MAXSIZE = 21389 + MAXCOPY;

        explicit SixPack(Input in) : in_(in) {}

        void decode(std::vector<uint8_t>& out)
        {
            static const uint16_t CopyBits[COPYRANGES] = { 4, 6, 8, 10, 12, 14 };
            static const uint16_t CopyMin[COPYRANGES] = { 0, 16, 80, 336, 1360, 5456 };

            std::vector<uint8_t> work(64 * 1024, 0);
            uint16_t count = 0;
            initTree();
            uint16_t c = uncompress();

            while (c != TERMINATE) {
                if (c < 256) {
                    out.push_back(static_cast<uint8_t>(c));
                    work[count] = static_cast<uint8_t>(c);
                    ++count;
                    if (count == MAXSIZE) count = 0;
                } else {
                    const uint16_t t = c - FIRSTCODE;
                    const uint16_t index = t / CODESPERRANGE;
                    const uint16_t len = static_cast<uint16_t>(t + MINCOPY - index * CODESPERRANGE);
                    const uint16_t dist = static_cast<uint16_t>(inputCode(CopyBits[index]) + len + CopyMin[index]);

                    uint16_t j = count;
                    uint16_t k = static_cast<uint16_t>(count - dist);
                    if (count < dist) k = static_cast<uint16_t>(k + MAXSIZE);

                    for (uint16_t i = 0; i < len; ++i) {
                        out.push_back(work[k]);
                        work[j] = work[k];
                        ++j;
                        ++k;
                        if (j == MAXSIZE) j = 0;
                        if (k == MAXSIZE) k = 0;
                    }

                    count = static_cast<uint16_t>(count + len);
                    if (count >= MAXSIZE) count = static_cast<uint16_t>(count - MAXSIZE);
                }

                if (out.size() > MaxOutputSize) break;
                c = uncompress();
            }
        }

    private:
        void initTree()
        {
            for (uint16_t index = 2; index <= TWICEMAX; ++index) {
                dad_[index] = index / 2;
                frq_[index] = 1;
            }
            for (uint16_t index = 1; index <= MAXCHAR; ++index) {
                leftC_[index] = 2 * index;
                rghtC_[index] = 2 * index + 1;
            }
        }

        void updateFreq(uint16_t a, uint16_t b)
        {
            do {
                frq_[dad_[a]] = static_cast<uint16_t>(frq_[a] + frq_[b]);
                a = dad_[a];
                if (a != ROOT) {
                    if (leftC_[dad_[a]] == a) b = rghtC_[dad_[a]];
                    else b = leftC_[dad_[a]];
                }
            } while (a != ROOT);

            if (frq_[ROOT] == MAXFREQ) {
                for (a = 1; a <= TWICEMAX; ++a) frq_[a] >>= 1;
            }
        }

        void updateModel(uint16_t code)
        {
            uint16_t a = code + SUCCMAX;
            ++frq_[a];

            if (dad_[a] != ROOT) {
                uint16_t code1 = dad_[a];
                if (leftC_[code1] == a) updateFreq(a, rghtC_[code1]);
                else updateFreq(a, leftC_[code1]);

                do {
                    const uint16_t code2 = dad_[code1];
                    uint16_t b;
                    if (leftC_[code2] == code1) b = rghtC_[code2];
                    else b = leftC_[code2];

                    if (frq_[a] > frq_[b]) {
                        if (leftC_[code2] == code1) rghtC_[code2] = a;
                        else leftC_[code2] = a;

                        uint16_t c;
                        if (leftC_[code1] == a) {
                            leftC_[code1] = b;
                            c = rghtC_[code1];
                        } else {
                            rghtC_[code1] = b;
                            c = leftC_[code1];
                        }

                        dad_[b] = code1;
                        dad_[a] = code2;
                        updateFreq(b, c);
                        a = b;
                    }

                    a = dad_[a];
                    code1 = dad_[a];
                } while (code1 != ROOT);
            }
        }

        void nextBit()
        {
            if (ibitCount_ == 0) {
                if (ibufCount_ == MAXBUF) ibufCount_ = 0;
                ibitBuffer_ = static_cast<uint16_t>(in_.at(static_cast<size_t>(ibufCount_) * 2) |
                                                    (in_.at(static_cast<size_t>(ibufCount_) * 2 + 1) << 8));
                ++ibufCount_;
                ibitCount_ = 15;
            } else {
                --ibitCount_;
            }
        }

        uint16_t inputCode(uint16_t bits)
        {
            uint16_t code = 0;
            for (uint16_t index = 1; index <= bits; ++index) {
                nextBit();
                if (ibitBuffer_ > 0x7fff) code |= static_cast<uint16_t>(1 << (index - 1));
                ibitBuffer_ = static_cast<uint16_t>(ibitBuffer_ << 1);
            }
            return code;
        }

        uint16_t uncompress()
        {
            uint16_t a = 1;
            do {
                nextBit();
                if (ibitBuffer_ > 0x7fff) a = rghtC_[a];
                else a = leftC_[a];
                ibitBuffer_ = static_cast<uint16_t>(ibitBuffer_ << 1);
            } while (a <= MAXCHAR);

            a = a - SUCCMAX;
            updateModel(a);
            return a;
        }

        Input in_;
        uint16_t leftC_[MAXCHAR + 1] = {};
        uint16_t rghtC_[MAXCHAR + 1] = {};
        uint16_t dad_[TWICEMAX + 1] = {};
        uint16_t frq_[TWICEMAX + 1] = {};
        uint16_t ibitCount_ = 0;
        uint16_t ibitBuffer_ = 0;
        uint16_t ibufCount_ = 0;
    };
}

size_t SIXPACK_decompress(const uint8_t* source, size_t sourceSize, uint16_t /*size*/, std::vector<uint8_t>& dest)
{
    dest.clear();
    auto sixpack = std::make_unique<SixPack>(Input{ source, sourceSize });
    sixpack->decode(dest);
    return static_cast<uint16_t>(dest.size());
}

// ---------------------------------------------------------------------------
// aPack
// ---------------------------------------------------------------------------

size_t APACK_decompress(const uint8_t* source, size_t sourceSize, std::vector<uint8_t>& dest)
{
    const Input in{ source, sourceSize };
    dest.clear();

    size_t esi = 0;
    uint32_t temp = 0, eax = 0, ecx = 0, swp = 0;
    uint8_t dl = 0x80, cf = 0, ncf = 0;

    // add dl,dl / jnz / mov dl,[esi] / inc esi / adc dl,dl
    auto getBit = [&]() {
        cf = dl >> 7;
        dl = static_cast<uint8_t>(dl << 1);
        if (dl == 0) {
            dl = in.at(esi);
            ++esi;
            ncf = dl >> 7;
            dl = static_cast<uint8_t>((dl << 1) + cf);
            cf = ncf;
        }
    };

j1:
    dest.push_back(in.at(esi));
    ++esi;
j2:
    if (dest.size() > MaxOutputSize) return dest.size();
    getBit();
    if (cf == 0) goto j1;
    ecx = 0;
    getBit();
    if (cf == 0) goto j8;
    eax = 0;
    getBit();
    if (cf == 0) goto j15;
    ++ecx;
    eax = 0x10;
j6:
    getBit();
    ncf = (eax >> 7) & 1;
    eax = (eax & 0xffffff00) + static_cast<uint8_t>((eax << 1) + cf);
    cf = ncf;
    if (cf == 0) goto j6;
    if (eax != 0) goto j24;
    dest.push_back(static_cast<uint8_t>(eax));
    goto j2;
j8:
    ++ecx;
j9:
    getBit();
    ecx = ecx + ecx + cf;
    getBit();
    if (cf == 1) goto j9;
    --ecx;
    --ecx;
    if (ecx != 0) goto j16;
    ecx = 1;
j12:
    getBit();
    ecx = ecx + ecx + cf;
    getBit();
    if (cf == 1) goto j12;
    goto j23;
j15:
    eax = in.at(esi);
    ++esi;
    cf = eax & 1;
    eax >>= 1;
    if (eax == 0) goto j25;
    ecx = ecx + ecx + cf;
    goto j20;
j16:
    swp = eax;
    eax = ecx;
    ecx = swp;
    --eax;
    eax = (eax << 8) + in.at(esi);
    ++esi;
    ecx = 1;
j17:
    getBit();
    ecx = ecx + ecx + cf;
    getBit();
    if (cf == 1) goto j17;
    if (eax >= 32000) goto j20;
    if (eax >= 1280) goto j21;
    if (eax > 127) goto j22;
j20:
    ++ecx;
j21:
    ++ecx;
j22:
    swp = temp;
    temp = eax;
    eax = swp;
j23:
    eax = temp;
j24:
    if (eax > dest.size()) return dest.size();   // 出力先頭より前を参照する壊れたデータ
    {
        size_t src = dest.size() - eax;
        while (ecx != 0) {
            dest.push_back(dest[src]);
            ++src;
            --ecx;
        }
    }
    goto j2;
j25:
    return dest.size();
}

// ---------------------------------------------------------------------------
// LZH
// ---------------------------------------------------------------------------

namespace
{
    class Lzh
    {
    public:
        static constexpr int WIN_SIZE_DEF = 1 << 12;
        static constexpr int DIC_SIZE_DEF = 1 << 13;
        static constexpr int WIN_SIZE_MAX = 1 << 15;
        static constexpr int DIC_SIZE_MAX = 1 << 14;
        static constexpr int MATCH_BIT = 8;
        static constexpr int MAX_MATCH = 1 << MATCH_BIT;
        static constexpr int THRESHOLD = 2;
        static constexpr int PBIT = 14;
        static constexpr int TBIT = 15;
        static constexpr int CBIT = 16;
        static constexpr int DIC_BIT = 14;
        static constexpr int CODE_BIT = 16;
        static constexpr int NC = 255 + MAX_MATCH + 2 - THRESHOLD;
        static constexpr int NP = DIC_BIT + 1;
        static constexpr int NT = CODE_BIT + 3;

        explicit Lzh(Input in) : in_(in) {}

        size_t decompress(uint32_t /*size*/, std::vector<uint8_t>& dest)
        {
            input_buffer_idx_ = 0;
            const bool ultra_compression_flag = in_.at(input_buffer_idx_) != 0;
            ++input_buffer_idx_;
            input_buffer_size_ = in_.size;

            uint32_t size_unpacked = 0;
            for (int i = 0; i < 4; ++i) {
                size_unpacked |= static_cast<uint32_t>(in_.at(input_buffer_idx_ + i)) << (8 * i);
            }
            input_buffer_idx_ += 4;
            uint32_t size = size_unpacked;
            if (size > MaxOutputSize) size = MaxOutputSize;

            dic_size_ = ultra_compression_flag ? DIC_SIZE_MAX : DIC_SIZE_DEF;

            std::vector<uint8_t> ptr(2 * (1 << DIC_BIT), 0);
            bit_buf_ = 0;
            sbit_buf_ = 0;
            bit_count_ = 0;
            fillBitBuffer(16);
            block_size_ = 0;
            dec_counter_ = 0;

            dest.clear();
            dest.reserve(size);
            while (size > 0) {
                const uint32_t size_temp = (size > static_cast<uint32_t>(dic_size_)) ? dic_size_ : size;
                decodeBuffer(static_cast<uint16_t>(size_temp), ptr.data());
                dest.insert(dest.end(), ptr.begin(), ptr.begin() + size_temp);
                size -= size_temp;
            }
            return size_unpacked;
        }

    private:
        void fillBitBuffer(int bits)
        {
            bit_buf_ = static_cast<uint16_t>(bit_buf_ << bits);
            while (bits > bit_count_) {
                bits -= bit_count_;
                bit_buf_ = static_cast<uint16_t>(bit_buf_ | (sbit_buf_ << bits));
                if (input_buffer_idx_ <= input_buffer_size_) {
                    sbit_buf_ = in_.at(input_buffer_idx_);
                    ++input_buffer_idx_;
                } else {
                    sbit_buf_ = 0;
                }
                bit_count_ = 8;
            }
            bit_count_ = static_cast<uint16_t>(bit_count_ - bits);
            bit_buf_ = static_cast<uint16_t>(bit_buf_ | (sbit_buf_ >> bit_count_));
        }

        uint16_t getBits(int bits)
        {
            const uint16_t result = static_cast<uint16_t>(bit_buf_ >> (16 - bits));
            fillBitBuffer(bits);
            return result;
        }

        void makeTable(int n_char, const uint8_t* bit_len, int bits, uint16_t* table)
        {
            uint16_t count[17] = {};   // [1..16] (添字 0 はオリジナルでは範囲外書き込み)
            uint16_t weight[17] = {};
            uint16_t start[18] = {};

            for (int idx = 0; idx < n_char; ++idx) ++count[bit_len[idx]];
            start[1] = 0;
            for (int idx = 1; idx <= 16; ++idx) {
                start[idx + 1] = static_cast<uint16_t>(start[idx] + (count[idx] << (16 - idx)));
            }
            const int j_bits = 16 - bits;
            for (int idx = 1; idx <= bits; ++idx) {
                start[idx] = static_cast<uint16_t>(start[idx] >> j_bits);
                weight[idx] = static_cast<uint16_t>(1 << (bits - idx));
            }
            for (int idx = bits + 1; idx <= 16; ++idx) {
                weight[idx] = static_cast<uint16_t>(1 << (16 - idx));
            }
            int idx = start[bits + 1] >> j_bits;
            if (idx != 0) {
                const int idx2 = 1 << bits;
                if (idx != idx2) {
                    for (int i = idx; i < idx2; ++i) table[i] = 0;
                }
            }
            int avail = n_char;
            const uint16_t mask = static_cast<uint16_t>(1 << (15 - bits));
            for (int chr = 0; chr < n_char; ++chr) {
                const int len = bit_len[chr];
                if (len == 0) continue;
                uint16_t idx2 = start[len];
                const uint16_t next_c = static_cast<uint16_t>(idx2 + weight[len]);
                if (len <= bits) {
                    for (int i = idx2; i < next_c; ++i) table[i] = static_cast<uint16_t>(chr);
                } else {
                    uint16_t* ptr = &table[idx2 >> j_bits];
                    int n = len - bits;
                    while (n != 0) {
                        if (*ptr == 0) {
                            r_tree_[avail] = 0;
                            l_tree_[avail] = 0;
                            *ptr = static_cast<uint16_t>(avail);
                            ++avail;
                        }
                        if (idx2 & mask) ptr = &r_tree_[*ptr];
                        else ptr = &l_tree_[*ptr];
                        idx2 = static_cast<uint16_t>(idx2 << 1);
                        --n;
                    }
                    *ptr = static_cast<uint16_t>(chr);
                }
                start[len] = next_c;
            }
        }

        void readPtrLen(int n_char, int n_bit, int i_bit)
        {
            const int bits = getBits(n_bit);
            if (bits == 0) {
                const uint16_t chr = getBits(n_bit);
                std::memset(p_len_, 0, sizeof(p_len_));
                for (auto& v : p_table_) v = chr;
            } else {
                int idx = 0;
                while (idx < bits) {
                    int chr = bit_buf_ >> (16 - 3);
                    if (chr == 7) {
                        uint16_t mask = 1 << (16 - 4);
                        while (mask & bit_buf_) {
                            mask >>= 1;
                            ++chr;
                        }
                    }
                    if (chr < 7) fillBitBuffer(3);
                    else fillBitBuffer(chr - 3);
                    p_len_[idx % NT] = static_cast<uint8_t>(chr);
                    ++idx;
                    if (idx == i_bit) {
                        chr = getBits(2) - 1;
                        while (chr >= 0) {
                            p_len_[idx % NT] = 0;
                            ++idx;
                            --chr;
                        }
                    }
                }
                while (idx < n_char) {
                    p_len_[idx] = 0;
                    ++idx;
                }
                makeTable(n_char, p_len_, 8, p_table_);
            }
        }

        void readCharLen()
        {
            const int bits = getBits(CBIT);
            if (bits == 0) {
                const uint16_t chr = getBits(CBIT);
                std::memset(c_len_, 0, sizeof(c_len_));
                for (auto& v : c_table_) v = chr;
            } else {
                int idx = 0;
                while (idx < bits) {
                    int chr = p_table_[bit_buf_ >> (16 - 8)];
                    if (chr >= NT) {
                        uint16_t mask = 1 << (16 - 9);
                        do {
                            if (bit_buf_ & mask) chr = r_tree_[chr];
                            else chr = l_tree_[chr];
                            mask >>= 1;
                        } while (chr >= NT);
                    }
                    fillBitBuffer(p_len_[chr]);
                    if (chr <= 2) {
                        if (chr == 1) chr = 2 + getBits(4);
                        else if (chr == 2) chr = 19 + getBits(CBIT);
                        while (chr >= 0) {
                            if (idx < NC) c_len_[idx] = 0;
                            ++idx;
                            --chr;
                        }
                    } else {
                        if (idx < NC) c_len_[idx] = static_cast<uint8_t>(chr - 2);
                        ++idx;
                    }
                }
                while (idx < NC) {
                    c_len_[idx] = 0;
                    ++idx;
                }
                makeTable(NC, c_len_, 12, c_table_);
            }
        }

        uint16_t decodeChar()
        {
            if (block_size_ == 0) {
                block_size_ = getBits(16);
                readPtrLen(NT, TBIT, 3);
                readCharLen();
                readPtrLen(NP, PBIT, -1);
            }
            --block_size_;
            uint16_t chr = c_table_[bit_buf_ >> (16 - 12)];
            if (chr >= NC) {
                uint16_t mask = 1 << (16 - 13);
                do {
                    if (bit_buf_ & mask) chr = r_tree_[chr];
                    else chr = l_tree_[chr];
                    mask >>= 1;
                } while (chr >= NC);
            }
            fillBitBuffer(c_len_[chr]);
            return chr;
        }

        uint16_t decodePtr()
        {
            uint16_t ptr = p_table_[bit_buf_ >> (16 - 8)];
            if (ptr >= NP) {
                uint16_t mask = 1 << (16 - 9);
                do {
                    if (bit_buf_ & mask) ptr = r_tree_[ptr];
                    else ptr = l_tree_[ptr];
                    mask >>= 1;
                } while (ptr >= NP);
            }
            fillBitBuffer(p_len_[ptr]);
            if (ptr != 0) {
                --ptr;
                ptr = static_cast<uint16_t>((1 << ptr) + getBits(ptr));
            }
            return ptr;
        }

        void decodeBuffer(uint16_t count, uint8_t* buffer)
        {
            const uint16_t dic_mask = static_cast<uint16_t>(dic_size_ - 1);
            uint16_t idx2 = 0;
            --dec_counter_;
            while (dec_counter_ >= 0) {
                buffer[idx2] = buffer[dec_ptr_];
                dec_ptr_ = static_cast<uint16_t>((dec_ptr_ + 1) & dic_mask);
                ++idx2;
                if (idx2 == count) return;
                --dec_counter_;
            }
            for (;;) {
                const uint16_t idx = decodeChar();
                if (idx <= 255) {
                    buffer[idx2] = static_cast<uint8_t>(idx);
                    ++idx2;
                    if (idx2 == count) return;
                } else {
                    dec_counter_ = static_cast<int16_t>(idx - (256 - THRESHOLD));
                    dec_ptr_ = static_cast<uint16_t>((idx2 - decodePtr() - 1) & dic_mask);
                    --dec_counter_;
                    while (dec_counter_ >= 0) {
                        buffer[idx2] = buffer[dec_ptr_];
                        dec_ptr_ = static_cast<uint16_t>((dec_ptr_ + 1) & dic_mask);
                        ++idx2;
                        if (idx2 == count) return;
                        --dec_counter_;
                    }
                }
            }
        }

        Input in_;
        size_t input_buffer_idx_ = 0;
        size_t input_buffer_size_ = 0;
        int dic_size_ = DIC_SIZE_DEF;

        uint16_t l_tree_[2 * (NC - 1) + 1] = {};
        uint16_t r_tree_[2 * (NC - 1) + 1] = {};
        uint16_t p_table_[256] = {};
        uint8_t p_len_[NT] = {};
        uint16_t c_table_[4096] = {};
        uint8_t c_len_[NC] = {};

        uint16_t bit_buf_ = 0;
        uint16_t sbit_buf_ = 0;
        uint16_t bit_count_ = 0;
        uint16_t block_size_ = 0;
        uint16_t dec_ptr_ = 0;
        int16_t dec_counter_ = 0;
    };
}

size_t LZH_decompress(const uint8_t* source, size_t sourceSize, uint32_t size, std::vector<uint8_t>& dest)
{
    auto lzh = std::make_unique<Lzh>(Input{ source, sourceSize });
    return lzh->decompress(size, dest);
}

// ---------------------------------------------------------------------------
// CRC32
// ---------------------------------------------------------------------------

uint32_t Update32(const void* buf, size_t size, uint32_t crc)
{
    static const auto table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t index = 0; index < 256; ++index) {
            uint32_t c = index;
            for (int n = 0; n < 8; ++n) {
                c = (c & 1) ? (c >> 1) ^ 0xedb88320u : (c >> 1);
            }
            t[index] = c;
        }
        return t;
    }();

    const auto* p = static_cast<const uint8_t*>(buf);
    for (size_t i = 0; i < size; ++i) {
        crc = (crc >> 8) ^ table[(crc ^ p[i]) & 0xff];
    }
    return crc;
}
