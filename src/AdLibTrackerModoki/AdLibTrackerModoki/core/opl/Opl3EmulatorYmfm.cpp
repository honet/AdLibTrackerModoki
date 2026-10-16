#include "Opl3EmulatorYmfm.h"

#include <algorithm>

#include "../../third_party/ymfm/ymfm_opl.h"

namespace
{
    // fm_engine_base::assign_operators() (protected) を外から呼ぶための踏み台
    struct FmEngineAccess : ymfm::ymf262::fm_engine
    {
        static void assign(ymfm::ymf262::fm_engine& fm)
        {
            (fm.*&FmEngineAccess::assign_operators)();
        }
    };

    class Ymf262 : public ymfm::ymf262
    {
    public:
        using ymfm::ymf262::ymf262;

        void reset()
        {
            ymfm::ymf262::reset();
            FmEngineAccess::assign(m_fm);
        }

        void writeReg(uint16_t reg, uint8_t value)
        {
            if (reg & 0x100)
                write_address_hi(static_cast<uint8_t>(reg));
            else
                write_address(static_cast<uint8_t>(reg));
            write_data(value);

            // ymfm は 0x104 (4op の有効化) によるオペレータの組み替えを次のサンプル生成まで遅らせるため、
            // サンプルを挟まずにキーオンを書くと、ペア側のオペレータがキーオンされない。
            // Nuked OPL3 や AT2 互換エミュレータと同じく、書いた時点で組み替える。
            if (reg == 0x104)
                FmEngineAccess::assign(m_fm);
        }

        // ymf262::generate() はオペレータの出力をそのまま合算するため、AT2 互換・Nuked OPL3 の 2 倍の音量になる。
        // オペレータの出力を 1/2 にしてから合算・クランプし、音量を揃える。
        void generate(output_data& output)
        {
            m_fm.clock(fm_engine::ALL_CHANNELS);
            m_fm.output(output.clear(), 1, 32767, fm_engine::ALL_CHANNELS);
            output.clamp16();
        }
    };
}

// タイマー・IRQ はプレイヤー側で扱うため、ymfm_interface の既定実装 (何もしない) をそのまま使う
struct Opl3EmulatorYmfm::Impl
{
    ymfm::ymfm_interface intf;
    Ymf262 chip{ intf };
};

Opl3EmulatorYmfm::Opl3EmulatorYmfm()
    : impl_(std::make_unique<Impl>())
{
    impl_->chip.reset();
}

Opl3EmulatorYmfm::~Opl3EmulatorYmfm() = default;

void Opl3EmulatorYmfm::reset()
{
    std::lock_guard lock(mutex_);
    impl_->chip.reset();
}

void Opl3EmulatorYmfm::writeReg(uint16_t reg, uint8_t value)
{
    std::lock_guard lock(mutex_);
    impl_->chip.writeReg(reg, value);
}

void Opl3EmulatorYmfm::render(int16_t* buffer, uint32_t frames)
{
    std::lock_guard lock(mutex_);
    ymfm::ymf262::output_data out;
    for (uint32_t i = 0; i < frames; i++) {
        impl_->chip.generate(out);
        // 出力 A/B を L/R とする (C/D は 4ch 出力用)
        buffer[i * 2 + 0] = static_cast<int16_t>(std::clamp<int32_t>(out.data[0], -32768, 32767));
        buffer[i * 2 + 1] = static_cast<int16_t>(std::clamp<int32_t>(out.data[1], -32768, 32767));
    }
}
