#include "Opl3Emulator.h"

#include "../../third_party/nukedopl3/opl3.h"

Opl3Emulator::Opl3Emulator(uint32_t sampleRate)
    : chip_(std::make_unique<opl3_chip>())
    , sampleRate_(sampleRate)
{
    OPL3_Reset(chip_.get(), sampleRate_);
}

Opl3Emulator::~Opl3Emulator() = default;

void Opl3Emulator::reset()
{
    std::lock_guard lock(mutex_);
    OPL3_Reset(chip_.get(), sampleRate_);
}

void Opl3Emulator::writeReg(uint16_t reg, uint8_t value)
{
    std::lock_guard lock(mutex_);
    OPL3_WriteReg(chip_.get(), reg, value);
}

void Opl3Emulator::render(int16_t* buffer, uint32_t frames)
{
    std::lock_guard lock(mutex_);
    OPL3_GenerateStream(chip_.get(), buffer, frames);
}
