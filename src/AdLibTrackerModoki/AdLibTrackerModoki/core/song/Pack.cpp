#include "Pack.h"

#include <cstring>
#include <memory>

// adt2pack.pas の圧縮部分の移植。
// 変数名・手続き名はオリジナルのまま。Pascal の Integer (16 ビット符号付き) は int16_t、Word は uint16_t とし、
// 代入時の切り詰めもオリジナルと同じになるようにしている。

namespace
{
    constexpr int MATCH_BIT = 8;
    constexpr int MAX_MATCH = 1 << MATCH_BIT;
    constexpr int THRESHOLD = 2;
    constexpr int PBIT = 14;
    constexpr int TBIT = 15;
    constexpr int CBIT = 16;
    constexpr int DIC_BIT = 14;
    constexpr int CODE_BIT = 16;
    constexpr int NC = 255 + MAX_MATCH + 2 - THRESHOLD;
    constexpr int NP = DIC_BIT + 1;
    constexpr int NT = CODE_BIT + 3;
    constexpr int MAX_HASH_VAL = 3 * (1 << DIC_BIT) + (((1 << DIC_BIT) >> 9) + 1) * 255;
    static_assert(MAX_HASH_VAL == 57567);
    constexpr int PERC_FLAG = 32768;
    constexpr int LOOP_GUARD = 1 << 20;

    constexpr uint16_t WIN_SIZE_DEF = 1 << 12;
    constexpr uint16_t DIC_SIZE_DEF = 1 << 13;
    constexpr uint16_t WIN_SIZE_MAX = 1 << 15;
    constexpr uint16_t DIC_SIZE_MAX = 1 << 14;

    // GetMem で確保される領域。オリジナルの確保順 (stream, level, child_count, pos_ptr, parent, previous, next, buf)
    // に一続きに並べ、範囲外の添字はオリジナル同様に隣の領域を読み書きする (全体の外は無視する)。
    // ただし MakeChild で 16 ビットからあふれて負になった Integer の添字は、配列の添字として使われるときに
    // ゼロ拡張される (切り詰め前と同じ位置を指す) ため、next の負の添字には 65536 を足す。
    // 本家が保存した A2M / A2T との比較で、この扱いで一致することを確認している。
    class Arena
    {
    public:
        void init(uint16_t dic, uint16_t win)
        {
            stream = 0;
            level = stream + 2 * dic + MAX_MATCH;
            child_count = level + dic + 256;
            pos_ptr = child_count + dic + 256;
            parent = pos_ptr + (dic + 256) * 2;
            previous = parent + dic * 2 * 2;
            next = previous + dic * 2 * 2;
            buf = next + (MAX_HASH_VAL + 1) * 2;
            mem_.assign(buf + win, 0);
        }

        uint8_t r8(size_t base, int64_t idx) const
        {
            const int64_t off = static_cast<int64_t>(base) + idx;
            return (off >= 0 && off < static_cast<int64_t>(mem_.size())) ? mem_[static_cast<size_t>(off)] : 0;
        }
        void w8(size_t base, int64_t idx, int value)
        {
            const int64_t off = static_cast<int64_t>(base) + idx;
            if (off >= 0 && off < static_cast<int64_t>(mem_.size())) mem_[static_cast<size_t>(off)] = static_cast<uint8_t>(value);
        }
        int16_t r16(size_t base, int64_t idx) const
        {
            const int64_t off = static_cast<int64_t>(base) + idx * 2;
            if (off < 0 || off + 1 >= static_cast<int64_t>(mem_.size())) return 0;
            return static_cast<int16_t>(mem_[static_cast<size_t>(off)] | (mem_[static_cast<size_t>(off) + 1] << 8));
        }
        void w16(size_t base, int64_t idx, int value)
        {
            const int64_t off = static_cast<int64_t>(base) + idx * 2;
            if (off < 0 || off + 1 >= static_cast<int64_t>(mem_.size())) return;
            mem_[static_cast<size_t>(off)] = static_cast<uint8_t>(value);
            mem_[static_cast<size_t>(off) + 1] = static_cast<uint8_t>(value >> 8);
        }
        uint8_t* ptr(size_t base) { return mem_.data() + base; }

        size_t stream = 0, level = 0, child_count = 0, pos_ptr = 0, parent = 0, previous = 0, next = 0, buf = 0;

    private:
        std::vector<uint8_t> mem_;
    };

    class LzhPacker
    {
    public:
        size_t compress(const uint8_t* source, uint32_t size, std::vector<uint8_t>& dest, bool ultra, const LzhProgress& progress)
        {
            progress_ = &progress;
            input_buffer_ = source;
            input_buffer_idx_ = 0;
            input_buffer_size_ = size;
            out_ = &dest;
            dest.clear();
            dest.push_back(ultra ? 1 : 0);   // 圧縮モードのフラグ
            for (int i = 0; i < 4; ++i) dest.push_back(static_cast<uint8_t>(size >> (i * 8)));

            WIN_SIZE = ultra ? WIN_SIZE_MAX : WIN_SIZE_DEF;
            DIC_SIZE = ultra ? DIC_SIZE_MAX : DIC_SIZE_DEF;
            a_.init(DIC_SIZE, WIN_SIZE);

            depth = 0;
            InitSlide();
            buf(0, 0);
            std::memset(c_freq, 0, sizeof(c_freq));
            std::memset(p_freq, 0, sizeof(p_freq));
            out_pos = 0;
            out_mask = 0;
            bit_count = 8;
            sbit_buf = 0;
            remain = static_cast<int16_t>(ReadDataBlock(a_.stream + DIC_SIZE, static_cast<uint16_t>(DIC_SIZE + MAX_MATCH)));
            match_len = 0;
            pos = DIC_SIZE;
            InsertNode();
            if (match_len > remain) match_len = remain;

            while (remain > 0) {
                int16_t last_match_len = match_len;
                const int16_t last_match_pos = static_cast<int16_t>(match_pos);
                GetNextMatch();
                if (match_len > remain) match_len = remain;
                if (match_len > last_match_len || last_match_len < THRESHOLD) {
                    Output(stream(pos - 1), 0);
                } else {
                    Output(static_cast<uint16_t>(last_match_len + (256 - THRESHOLD)),
                           static_cast<uint16_t>((pos - last_match_pos - 2) & (DIC_SIZE - 1)));
                    --last_match_len;
                    while (last_match_len > 0) {
                        GetNextMatch();
                        --last_match_len;
                    }
                    if (match_len > remain) match_len = remain;
                }
            }

            SendBlock();
            PutBits(7, 0);
            return dest.size();
        }

    private:
        uint16_t WIN_SIZE = WIN_SIZE_DEF;
        uint16_t DIC_SIZE = DIC_SIZE_DEF;

        uint16_t l_tree[2 * (NC - 1) + 1] = {}, r_tree[2 * (NC - 1) + 1] = {};
        uint8_t p_len[NT] = {};
        uint8_t c_len[NC] = {};
        uint16_t heap[NC + 1] = {};
        uint16_t len_count[17] = {};
        uint16_t c_freq[2 * (NC - 1) + 1] = {};
        uint16_t p_freq[2 * (NP - 1) + 1] = {};
        uint16_t t_freq[2 * (NT - 1) + 1] = {};
        uint16_t c_code[NC] = {};
        uint16_t p_code[NT] = {};

        // freq / sort_ptr は pWORD (Integer の配列) として Word の配列を指す
        uint16_t* freq = nullptr;
        uint16_t* sort_ptr = nullptr;
        uint8_t* len = nullptr;

        int16_t bits = 0, heap_size = 0, remain = 0, match_len = 0;
        uint16_t sbit_buf = 0, bit_count = 0, depth = 0, c_pos = 0, pos = 0, out_pos = 0, match_pos = 0, out_mask = 0, avail = 0;

        const uint8_t* input_buffer_ = nullptr;
        uint32_t input_buffer_idx_ = 0;
        uint32_t input_buffer_size_ = 0;
        std::vector<uint8_t>* out_ = nullptr;
        const LzhProgress* progress_ = nullptr;
        Arena a_;

        // --- 領域への参照 ---
        uint8_t stream(int64_t i) const { return a_.r8(a_.stream, i); }
        uint8_t level(int64_t i) const { return a_.r8(a_.level, i); }
        void level(int64_t i, int v) { a_.w8(a_.level, i, v); }
        uint8_t child_count(int64_t i) const { return a_.r8(a_.child_count, i); }
        void child_count(int64_t i, int v) { a_.w8(a_.child_count, i, v); }
        int16_t pos_ptr(int64_t i) const { return a_.r16(a_.pos_ptr, i); }
        void pos_ptr(int64_t i, int v) { a_.w16(a_.pos_ptr, i, v); }
        int16_t parent(int64_t i) const { return a_.r16(a_.parent, i); }
        void parent(int64_t i, int v) { a_.w16(a_.parent, i, v); }
        int16_t previous(int64_t i) const { return a_.r16(a_.previous, i); }
        void previous(int64_t i, int v) { a_.w16(a_.previous, i, v); }
        int16_t next(int64_t i) const { return a_.r16(a_.next, i < 0 ? i + 65536 : i); }
        void next(int64_t i, int v) { a_.w16(a_.next, i < 0 ? i + 65536 : i, v); }
        uint8_t buf(int64_t i) const { return a_.r8(a_.buf, i); }
        void buf(int64_t i, int v) { a_.w8(a_.buf, i, v); }

        static int16_t fr(const uint16_t* f, int i) { return static_cast<int16_t>(f[i]); }

        uint16_t ReadDataBlock(size_t dest_off, uint16_t size)
        {
            const uint32_t result = (input_buffer_size_ - input_buffer_idx_ >= size) ? size : input_buffer_size_ - input_buffer_idx_;
            std::memcpy(a_.ptr(dest_off), input_buffer_ + input_buffer_idx_, result);
            input_buffer_idx_ += result;
            if (*progress_) (*progress_)(input_buffer_idx_, input_buffer_size_);
            return static_cast<uint16_t>(result);
        }

        void PutBits(int bits_, uint16_t xbits)
        {
            if (bits_ < bit_count) {
                bit_count = static_cast<uint16_t>(bit_count - bits_);
                sbit_buf = static_cast<uint16_t>(sbit_buf | (xbits << bit_count));
            } else {
                bits_ -= bit_count;
                out_->push_back(static_cast<uint8_t>(sbit_buf | (xbits >> bits_)));
                if (bits_ < 8) {
                    bit_count = static_cast<uint16_t>(8 - bits_);
                    sbit_buf = static_cast<uint16_t>(xbits << bit_count);
                } else {
                    out_->push_back(static_cast<uint8_t>(xbits >> (bits_ - 8)));
                    bit_count = static_cast<uint16_t>(16 - bits_);
                    sbit_buf = static_cast<uint16_t>(static_cast<uint32_t>(xbits) << bit_count);
                }
            }
        }

        void CountLen(int16_t idx)
        {
            if (idx < bits) {
                if (depth < 16) ++len_count[depth];
                else ++len_count[16];
            } else {
                ++depth;
                CountLen(static_cast<int16_t>(l_tree[idx]));
                CountLen(static_cast<int16_t>(r_tree[idx]));
                --depth;
            }
        }

        void MakeLen(int16_t root)
        {
            std::memset(len_count, 0, sizeof(len_count));
            CountLen(root);
            uint16_t sum = 0;
            for (int idx = 16; idx >= 1; --idx) sum = static_cast<uint16_t>(sum + (len_count[idx] << (16 - idx)));
            while (sum != 0) {
                --len_count[16];
                for (int idx = 15; idx >= 1; --idx) {
                    if (len_count[idx] != 0) {
                        --len_count[idx];
                        len_count[idx + 1] = static_cast<uint16_t>(len_count[idx + 1] + 2);
                        break;
                    }
                }
                --sum;
            }
            for (int idx = 16; idx >= 1; --idx) {
                int16_t idx2 = static_cast<int16_t>(len_count[idx] - 1);
                while (idx2 >= 0) {
                    --idx2;
                    len[static_cast<int16_t>(sort_ptr[0])] = static_cast<uint8_t>(idx);
                    ++sort_ptr;
                }
            }
        }

        void DownHeap(int16_t idx)
        {
            const uint16_t idx2 = heap[idx];
            int16_t idx3 = static_cast<int16_t>(idx << 1);
            while (idx3 <= heap_size) {
                if (idx3 < heap_size && fr(freq, heap[idx3]) > fr(freq, heap[idx3 + 1])) ++idx3;
                if (fr(freq, idx2) <= fr(freq, heap[idx3])) break;
                heap[idx] = heap[idx3];
                idx = idx3;
                idx3 = static_cast<int16_t>(idx << 1);
            }
            heap[idx] = idx2;
        }

        void MakeCode(int bits_, const uint8_t* len_, uint16_t* code)
        {
            uint16_t start[18] = {};
            start[1] = 0;
            for (int idx = 1; idx <= 16; ++idx) start[idx + 1] = static_cast<uint16_t>((start[idx] + len_count[idx]) << 1);
            for (int idx = 0; idx < bits_; ++idx) {
                const uint8_t idx2 = len_[idx];
                code[idx] = start[idx2];
                ++start[idx2];
            }
        }

        int16_t MakeTree(int n_par, uint16_t* f_par, uint8_t* l_par, uint16_t* c_par)
        {
            bits = static_cast<int16_t>(n_par);
            freq = f_par;
            len = l_par;
            int16_t avail_ = bits;
            heap_size = 0;
            heap[1] = 0;

            for (int idx = 0; idx < bits; ++idx) {
                len[idx] = 0;
                if (freq[idx] != 0) {
                    ++heap_size;
                    heap[heap_size] = static_cast<uint16_t>(idx);
                }
            }

            if (heap_size < 2) {
                c_par[heap[1]] = 0;
                return static_cast<int16_t>(heap[1]);
            }

            for (int idx = heap_size / 2; idx >= 1; --idx) DownHeap(static_cast<int16_t>(idx));
            sort_ptr = c_par;
            int16_t idx2 = 0;
            do {
                const int16_t idx = static_cast<int16_t>(heap[1]);
                if (idx < bits) *sort_ptr++ = static_cast<uint16_t>(idx);
                heap[1] = heap[heap_size];
                --heap_size;
                DownHeap(1);
                const int16_t idx3 = static_cast<int16_t>(heap[1]);
                if (idx3 < bits) *sort_ptr++ = static_cast<uint16_t>(idx3);
                idx2 = avail_;
                ++avail_;
                freq[idx2] = static_cast<uint16_t>(fr(freq, idx) + fr(freq, idx3));
                heap[1] = static_cast<uint16_t>(idx2);
                DownHeap(1);
                l_tree[idx2] = static_cast<uint16_t>(idx);
                r_tree[idx2] = static_cast<uint16_t>(idx3);
            } while (!(heap_size <= 1));

            sort_ptr = c_par;
            MakeLen(idx2);
            MakeCode(n_par, l_par, c_par);
            return idx2;
        }

        void CountFreq()
        {
            for (int idx = 0; idx < NT; ++idx) t_freq[idx] = 0;
            int bits_ = NC;
            while (bits_ > 0 && c_len[bits_ - 1] == 0) --bits_;
            int idx = 0;
            while (idx < bits_) {
                const int idx2 = c_len[idx];
                ++idx;
                if (idx2 == 0) {
                    int count = 1;
                    while (idx < bits_ && c_len[idx] == 0) {
                        ++idx;
                        ++count;
                    }
                    if (count <= 2) t_freq[0] = static_cast<uint16_t>(t_freq[0] + count);
                    else if (count <= 18) ++t_freq[1];
                    else if (count == 19) {
                        ++t_freq[0];
                        ++t_freq[1];
                    } else {
                        ++t_freq[2];
                    }
                } else {
                    ++t_freq[idx2 + 2];
                }
            }
        }

        void WritePtrLen(int bits_, int n_bit, int s_bit)
        {
            while (bits_ > 0 && p_len[bits_ - 1] == 0) --bits_;
            PutBits(n_bit, static_cast<uint16_t>(bits_));
            int idx = 0;
            while (idx < bits_) {
                int idx2 = p_len[idx];
                ++idx;
                if (idx2 <= 6) {
                    PutBits(3, static_cast<uint16_t>(idx2));
                } else {
                    idx2 -= 3;
                    PutBits(idx2, static_cast<uint16_t>((1 << idx2) - 2));
                }
                if (idx == s_bit) {
                    while (idx < 6 && p_len[idx] == 0) ++idx;
                    PutBits(2, static_cast<uint16_t>((idx - 3) & 3));
                }
            }
        }

        void WriteCharLen()
        {
            int bits_ = NC;
            while (bits_ > 0 && c_len[bits_ - 1] == 0) --bits_;
            PutBits(CBIT, static_cast<uint16_t>(bits_));
            int idx = 0;
            while (idx < bits_) {
                const int idx2 = c_len[idx];
                ++idx;
                if (idx2 == 0) {
                    int count = 1;
                    while (idx < bits_ && c_len[idx] == 0) {
                        ++idx;
                        ++count;
                    }
                    if (count <= 2) {
                        for (int i = 0; i < count; ++i) PutBits(p_len[0], p_code[0]);
                    } else if (count <= 18) {
                        PutBits(p_len[1], p_code[1]);
                        PutBits(4, static_cast<uint16_t>(count - 3));
                    } else if (count == 19) {
                        PutBits(p_len[0], p_code[0]);
                        PutBits(p_len[1], p_code[1]);
                        PutBits(4, 15);
                    } else {
                        PutBits(p_len[2], p_code[2]);
                        PutBits(CBIT, static_cast<uint16_t>(count - 20));
                    }
                } else {
                    PutBits(p_len[idx2 + 2], p_code[idx2 + 2]);
                }
            }
        }

        void EncodePtr(uint16_t ptr)
        {
            uint16_t idx = 0;
            uint16_t idx2 = ptr;
            while (idx2 != 0) {
                idx2 = static_cast<uint16_t>(idx2 >> 1);
                ++idx;
            }
            PutBits(p_len[idx], p_code[idx]);
            if (idx > 1) PutBits(idx - 1, static_cast<uint16_t>(ptr & (0xffffu >> (17 - idx))));
        }

        void SendBlock()
        {
            uint16_t root = static_cast<uint16_t>(MakeTree(NC, c_freq, c_len, c_code));
            const uint16_t size = c_freq[root];
            PutBits(16, size);
            if (root >= NC) {
                CountFreq();
                root = static_cast<uint16_t>(MakeTree(NT, t_freq, p_len, p_code));
                if (root >= NT) {
                    WritePtrLen(NT, TBIT, 3);
                } else {
                    PutBits(TBIT, 0);
                    PutBits(TBIT, root);
                }
                WriteCharLen();
            } else {
                PutBits(TBIT, 0);
                PutBits(TBIT, 0);
                PutBits(CBIT, 0);
                PutBits(CBIT, root);
            }
            root = static_cast<uint16_t>(MakeTree(NP, p_freq, p_len, p_code));
            if (root >= NP) {
                WritePtrLen(NP, PBIT, -1);
            } else {
                PutBits(PBIT, 0);
                PutBits(PBIT, root);
            }

            uint16_t p = 0;
            uint16_t flags = 0;
            for (uint32_t idx = 0; idx < size; ++idx) {
                if ((idx & 7) == 0) {
                    flags = buf(p);
                    ++p;
                } else {
                    flags = static_cast<uint16_t>(flags << 1);
                }
                if ((flags & (1 << 7)) != 0) {
                    uint16_t idx2 = static_cast<uint16_t>(buf(p) + (1 << 8));
                    ++p;
                    PutBits(c_len[idx2], c_code[idx2]);
                    idx2 = static_cast<uint16_t>(buf(p) << 8);
                    ++p;
                    idx2 = static_cast<uint16_t>(idx2 + buf(p));
                    ++p;
                    EncodePtr(idx2);
                } else {
                    const uint16_t idx2 = buf(p);
                    ++p;
                    PutBits(c_len[idx2], c_code[idx2]);
                }
            }
            for (int idx = 0; idx < NC; ++idx) c_freq[idx] = 0;
            for (int idx = 0; idx < NP; ++idx) p_freq[idx] = 0;
        }

        void Output(uint16_t code, uint16_t c_ptr)
        {
            out_mask = static_cast<uint16_t>(out_mask >> 1);
            if (out_mask == 0) {
                out_mask = 1 << 7;
                if (out_pos >= WIN_SIZE - 24) {
                    SendBlock();
                    out_pos = 0;
                }
                c_pos = out_pos;
                ++out_pos;
                buf(c_pos, 0);
            }
            buf(out_pos, code);
            ++out_pos;
            ++c_freq[code];
            if (code >= 1 << 8) {
                buf(c_pos, buf(c_pos) | out_mask);
                buf(out_pos, c_ptr >> 8);
                ++out_pos;
                buf(out_pos, c_ptr);
                ++out_pos;
                code = 0;
                while (c_ptr != 0) {
                    c_ptr = static_cast<uint16_t>(c_ptr >> 1);
                    ++code;
                }
                ++p_freq[code];
            }
        }

        void InitSlide()
        {
            for (uint32_t idx = DIC_SIZE; idx <= DIC_SIZE + 255u; ++idx) {
                level(idx, 1);
                pos_ptr(idx, 0);
            }
            for (uint32_t idx = DIC_SIZE; idx <= 2u * DIC_SIZE - 1; ++idx) parent(idx, 0);
            avail = 1;
            for (uint32_t idx = 1; idx <= DIC_SIZE - 2u; ++idx) next(idx, static_cast<int>(idx + 1));
            next(DIC_SIZE - 1, 0);
            for (uint32_t idx = 2u * DIC_SIZE; idx <= static_cast<uint32_t>(MAX_HASH_VAL); ++idx) next(idx, 0);
        }

        int16_t Child(int16_t pnode, uint8_t chr)
        {
            // 添字の式はそのまま (32 ビットで) 計算されるため、16 ビットに切り詰められない
            int16_t node = next(static_cast<int64_t>(pnode) + (chr << (DIC_BIT - 9)) + 2 * DIC_SIZE);
            parent(0, pnode);
            // 壊れた木で無限ループにならないよう回数に上限を設ける (オリジナルにはない)
            for (int guard = 0; parent(node) != pnode; ++guard) {
                if (guard > LOOP_GUARD) return 0;
                node = next(node);
            }
            return node;
        }

        void MakeChild(int16_t p_node, uint8_t chr, int16_t c_node)
        {
            // Integer の変数へ代入するため 16 ビットに切り詰められる (あふれて負になった添字は next() で補正する)
            const int16_t idx = static_cast<int16_t>(p_node + (chr << (DIC_BIT - 9)) + 2 * DIC_SIZE);
            const int16_t idx2 = next(idx);
            next(idx, c_node);
            next(c_node, idx2);
            previous(idx2, c_node);
            previous(c_node, idx);
            parent(c_node, p_node);
            child_count(p_node, child_count(p_node) + 1);
        }

        void SplitTree(int16_t old)
        {
            const int16_t new_ = static_cast<int16_t>(avail);
            avail = static_cast<uint16_t>(next(new_));
            child_count(new_, 0);
            int16_t idx = previous(old);
            previous(new_, idx);
            next(idx, new_);
            idx = next(old);
            next(new_, idx);
            previous(idx, new_);
            parent(new_, parent(old));
            level(new_, match_len);
            pos_ptr(new_, pos);
            MakeChild(new_, stream(static_cast<int64_t>(match_pos) + match_len), old);
            MakeChild(new_, stream(static_cast<int64_t>(pos) + match_len), static_cast<int16_t>(pos));
        }

        void InsertNode()
        {
            int16_t idx, idx2;
            if (match_len >= 4) {
                --match_len;
                idx2 = static_cast<int16_t>((match_pos + 1) | DIC_SIZE);
                idx = parent(idx2);
                // 壊れた木で無限ループにならないよう、以下のループには回数の上限を設ける (オリジナルにはない)
                int guard = 0;
                while (idx == 0) {
                    if (++guard > LOOP_GUARD) break;
                    idx2 = next(idx2);
                    idx = parent(idx2);
                }
                while (level(idx) >= match_len) {
                    if (++guard > LOOP_GUARD) break;
                    idx2 = idx;
                    idx = parent(idx);
                }
                int16_t idx4 = idx;
                while (pos_ptr(idx4) < 0) {
                    if (++guard > LOOP_GUARD) break;
                    pos_ptr(idx4, pos);
                    idx4 = parent(idx4);
                }
                if (idx4 < DIC_SIZE) pos_ptr(idx4, pos | PERC_FLAG);
            } else {
                idx = static_cast<int16_t>(stream(pos) + DIC_SIZE);
                const uint8_t chr = stream(static_cast<int64_t>(pos) + 1);
                idx2 = Child(idx, chr);
                if (idx2 == 0) {
                    MakeChild(idx, chr, static_cast<int16_t>(pos));
                    match_len = 1;
                    return;
                }
                match_len = 2;
            }

            for (;;) {
                int16_t idx3;
                if (idx2 >= DIC_SIZE) {
                    idx3 = MAX_MATCH;
                    match_pos = static_cast<uint16_t>(idx2);
                } else {
                    idx3 = level(idx2);
                    match_pos = static_cast<uint16_t>(pos_ptr(idx2) & ~(1 << 15));
                }
                if (match_pos >= pos) match_pos = static_cast<uint16_t>(match_pos - DIC_SIZE);
                int64_t ptr1 = static_cast<int64_t>(pos) + match_len;
                int64_t ptr2 = static_cast<int64_t>(match_pos) + match_len;
                while (match_len < idx3) {
                    if (stream(ptr1) != stream(ptr2)) {
                        SplitTree(idx2);
                        return;
                    }
                    ++match_len;
                    ++ptr1;
                    ++ptr2;
                }
                if (match_len >= MAX_MATCH) break;
                pos_ptr(idx2, pos);
                idx = idx2;
                idx2 = Child(idx, stream(ptr1));
                if (idx2 == 0) {
                    MakeChild(idx, stream(ptr1), static_cast<int16_t>(pos));
                    return;
                }
                ++match_len;
            }

            int16_t idx4 = previous(idx2);
            previous(pos, idx4);
            next(idx4, pos);
            idx4 = next(idx2);
            next(pos, idx4);
            previous(idx4, pos);
            parent(pos, idx);
            parent(idx2, 0);
            next(idx2, pos);
        }

        void DeleteNode()
        {
            if (parent(pos) == 0) return;
            int16_t idx = previous(pos);
            int16_t idx2 = next(pos);
            next(idx, idx2);
            previous(idx2, idx);
            idx = parent(pos);
            parent(pos, 0);

            child_count(idx, child_count(idx) - 1);
            if (idx >= DIC_SIZE || child_count(idx) > 1) return;
            int16_t idx3 = static_cast<int16_t>(pos_ptr(idx) & ~PERC_FLAG);
            if (idx3 >= pos) idx3 = static_cast<int16_t>(idx3 - DIC_SIZE);

            idx2 = idx3;
            int16_t perc_idx = parent(idx);
            int16_t idx4 = pos_ptr(perc_idx);
            for (int guard = 0; (idx4 & PERC_FLAG) != 0 && guard <= LOOP_GUARD; ++guard) {
                idx4 = static_cast<int16_t>(idx4 & ~PERC_FLAG);
                if (idx4 >= pos) idx4 = static_cast<int16_t>(idx4 - DIC_SIZE);
                if (idx4 > idx2) idx2 = idx4;
                pos_ptr(perc_idx, idx2 | DIC_SIZE);
                perc_idx = parent(perc_idx);
                idx4 = pos_ptr(perc_idx);
            }
            if (perc_idx < DIC_SIZE) {
                if (idx4 >= pos) idx4 = static_cast<int16_t>(idx4 - DIC_SIZE);
                if (idx4 > idx2) idx2 = idx4;
                pos_ptr(perc_idx, idx2 | DIC_SIZE | PERC_FLAG);
            }

            idx2 = Child(idx, stream(static_cast<int64_t>(idx3) + level(idx)));
            idx3 = previous(idx2);
            idx4 = next(idx2);
            next(idx3, idx4);
            previous(idx4, idx3);
            idx3 = previous(idx);
            next(idx3, idx2);
            previous(idx2, idx3);
            idx3 = next(idx);
            previous(idx3, idx2);
            next(idx2, idx3);
            parent(idx2, parent(idx));
            parent(idx, 0);
            next(idx, avail);
            avail = static_cast<uint16_t>(idx);
        }

        void GetNextMatch()
        {
            --remain;
            ++pos;
            if (pos == 2 * DIC_SIZE) {
                uint8_t* s = a_.ptr(a_.stream);
                std::memmove(s, s + DIC_SIZE, DIC_SIZE + MAX_MATCH);
                const uint16_t n = ReadDataBlock(a_.stream + DIC_SIZE + MAX_MATCH, DIC_SIZE);
                remain = static_cast<int16_t>(remain + n);
                pos = DIC_SIZE;
            }
            DeleteNode();
            InsertNode();
        }
    };
}

size_t LZH_compress(const uint8_t* source, uint32_t size, std::vector<uint8_t>& dest, const LzhProgress& progress)
{
    auto packer = std::make_unique<LzhPacker>();
    return packer->compress(source, size, dest, false, progress);
}

size_t LZH_compress_ultra(const uint8_t* source, uint32_t size, std::vector<uint8_t>& dest, const LzhProgress& progress)
{
    auto packer = std::make_unique<LzhPacker>();
    return packer->compress(source, size, dest, true, progress);
}
