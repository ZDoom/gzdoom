#include "nuked_opn2.h"
#include "nuked/ym3438.h"
#include <cstring>

NukedOPN2::NukedOPN2()
{
    OPN2_SetChipType(ym3438_type_asic);
    chip = new ym3438_t;
    setRate(m_rate, m_clock);
}

NukedOPN2::~NukedOPN2()
{
    ym3438_t *chip_r = reinterpret_cast<ym3438_t*>(chip);
    delete chip_r;
}

void NukedOPN2::setRate(uint32_t rate, uint32_t clock)
{
    OPNChipBaseT::setRate(rate, clock);
    ym3438_t *chip_r = reinterpret_cast<ym3438_t*>(chip);
    OPN2_Reset(chip_r, rate, clock);
}

void NukedOPN2::reset()
{
    OPNChipBaseT::reset();
    ym3438_t *chip_r = reinterpret_cast<ym3438_t*>(chip);
    OPN2_Reset(chip_r, m_rate, m_clock);
}

void NukedOPN2::writeReg(uint32_t port, uint16_t addr, uint8_t data)
{
    ym3438_t *chip_r = reinterpret_cast<ym3438_t*>(chip);
    OPN2_WriteBuffered(chip_r, 0 + (port) * 2, (uint8_t)addr);
    OPN2_WriteBuffered(chip_r, 1 + (port) * 2, data);
    //qDebug() << QString("%1: 0x%2 => 0x%3").arg(port).arg(addr, 2, 16, QChar('0')).arg(data, 2, 16, QChar('0'));
}

void NukedOPN2::nativeGenerate(int16_t *frame)
{
    ym3438_t *chip_r = reinterpret_cast<ym3438_t*>(chip);
    OPN2_Generate(chip_r, frame);
}

const char *NukedOPN2::emulatorName()
{
    return "Nuked OPN2";
}
