#include "nuked_opl3.h"
#include "nuked/nukedopl3.h"
#include <cstring>

NukedOPL3::NukedOPL3() :
    OPLChipBaseT()
{
    m_chip = new opl3_chip;
    setRate(m_rate);
}

NukedOPL3::~NukedOPL3()
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    delete chip_r;
}

void NukedOPL3::setRate(uint32_t rate)
{
    OPLChipBaseT::setRate(rate);
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    std::memset(chip_r, 0, sizeof(opl3_chip));
    OPL3_Reset(chip_r, rate);
}

void NukedOPL3::reset()
{
    OPLChipBaseT::reset();
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    std::memset(chip_r, 0, sizeof(opl3_chip));
    OPL3_Reset(chip_r, m_rate);
}

void NukedOPL3::writeReg(uint16_t addr, uint8_t data)
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    OPL3_WriteRegBuffered(chip_r, addr, data);
}

void NukedOPL3::nativeGenerate(int16_t *frame)
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    OPL3_Generate(chip_r, frame);
}

const char *NukedOPL3::emulatorName()
{
    return "Nuked OPL3 (v 1.8)";
}
