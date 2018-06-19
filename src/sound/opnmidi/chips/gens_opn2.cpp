#include "gens_opn2.h"
#include <cstring>

#include "gens/Ym2612_Emu.h"

GensOPN2::GensOPN2()
    : chip(new Ym2612_Emu())
{
    setRate(m_rate, m_clock);
}

GensOPN2::~GensOPN2()
{
    delete chip;
}

void GensOPN2::setRate(uint32_t rate, uint32_t clock)
{
    OPNChipBaseBufferedT::setRate(rate, clock);
    uint32_t chipRate = isRunningAtPcmRate() ? rate : static_cast<uint32_t>(nativeRate);
    chip->set_rate(chipRate, clock);  // implies reset()
}

void GensOPN2::reset()
{
    OPNChipBaseBufferedT::reset();
    chip->reset();
}

void GensOPN2::writeReg(uint32_t port, uint16_t addr, uint8_t data)
{
    switch (port)
    {
    case 0:
        chip->write0(addr, data);
        break;
    case 1:
        chip->write1(addr, data);
        break;
    }
}

void GensOPN2::nativeGenerateN(int16_t *output, size_t frames)
{
    std::memset(output, 0, frames * sizeof(int16_t) * 2);
    chip->run((int)frames, output);
}

const char *GensOPN2::emulatorName()
{
    return "GENS 2.10 OPN2";
}
