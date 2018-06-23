#include "dosbox_opl3.h"
#include "dosbox/dbopl.h"
#include <new>
#include <cstdlib>
#include <assert.h>

DosBoxOPL3::DosBoxOPL3() :
    OPLChipBaseBufferedT(),
    m_chip(new DBOPL::Handler)
{
    reset();
}

DosBoxOPL3::~DosBoxOPL3()
{
    DBOPL::Handler *chip_r = reinterpret_cast<DBOPL::Handler*>(m_chip);
    delete chip_r;
}

void DosBoxOPL3::setRate(uint32_t rate)
{
    OPLChipBaseBufferedT::setRate(rate);
    DBOPL::Handler *chip_r = reinterpret_cast<DBOPL::Handler*>(m_chip);
    chip_r->~Handler();
    new(chip_r) DBOPL::Handler;
    chip_r->Init(effectiveRate());
}

void DosBoxOPL3::reset()
{
    OPLChipBaseBufferedT::reset();
    DBOPL::Handler *chip_r = reinterpret_cast<DBOPL::Handler*>(m_chip);
    chip_r->~Handler();
    new(chip_r) DBOPL::Handler;
    chip_r->Init(effectiveRate());
}

void DosBoxOPL3::writeReg(uint16_t addr, uint8_t data)
{
    DBOPL::Handler *chip_r = reinterpret_cast<DBOPL::Handler*>(m_chip);
    chip_r->WriteReg(static_cast<Bit32u>(addr), data);
}

void DosBoxOPL3::nativeGenerateN(int16_t *output, size_t frames)
{
    DBOPL::Handler *chip_r = reinterpret_cast<DBOPL::Handler*>(m_chip);
    Bitu frames_i = frames;
    chip_r->GenerateArr(output, &frames_i);
}

const char *DosBoxOPL3::emulatorName()
{
    return "DosBox 0.74-r4111 OPL3";
}
