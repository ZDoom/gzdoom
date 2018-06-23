#include "nuked_opl3_v174.h"
#include "nuked/nukedopl3_174.h"
#include <cstring>

NukedOPL3v174::NukedOPL3v174() :
    OPLChipBaseT()
{
    m_chip = new opl3_chip;
    setRate(m_rate);
}

NukedOPL3v174::~NukedOPL3v174()
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    delete chip_r;
}

void NukedOPL3v174::setRate(uint32_t rate)
{
    OPLChipBaseT::setRate(rate);
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    std::memset(chip_r, 0, sizeof(opl3_chip));
    OPL3v17_Reset(chip_r, rate);
}

void NukedOPL3v174::reset()
{
    OPLChipBaseT::reset();
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    std::memset(chip_r, 0, sizeof(opl3_chip));
    OPL3v17_Reset(chip_r, m_rate);
}

void NukedOPL3v174::writeReg(uint16_t addr, uint8_t data)
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    OPL3v17_WriteReg(chip_r, addr, data);
}

void NukedOPL3v174::nativeGenerate(int16_t *frame)
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    OPL3v17_Generate(chip_r, frame);
}

const char *NukedOPL3v174::emulatorName()
{
    return "Nuked OPL3 (v 1.7.4)";
}
