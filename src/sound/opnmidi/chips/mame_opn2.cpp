#include "mame_opn2.h"
#include "mame/mame_ym2612fm.h"
#include <cstdlib>
#include <assert.h>

MameOPN2::MameOPN2()
{
    chip = NULL;
    setRate(m_rate, m_clock);
}

MameOPN2::~MameOPN2()
{
    ym2612_shutdown(chip);
}

void MameOPN2::setRate(uint32_t rate, uint32_t clock)
{
    OPNChipBaseT::setRate(rate, clock);
    if(chip)
        ym2612_shutdown(chip);
    uint32_t chipRate = isRunningAtPcmRate() ? rate : static_cast<uint32_t>(nativeRate);
    chip = ym2612_init(NULL, (int)clock, (int)chipRate, NULL, NULL);
    ym2612_reset_chip(chip);
}

void MameOPN2::reset()
{
    OPNChipBaseT::reset();
    ym2612_reset_chip(chip);
}

void MameOPN2::writeReg(uint32_t port, uint16_t addr, uint8_t data)
{
    ym2612_write(chip, 0 + (int)(port) * 2, (uint8_t)addr);
    ym2612_write(chip, 1 + (int)(port) * 2, data);
}

void MameOPN2::nativePreGenerate()
{
    void *chip = this->chip;
    ym2612_pre_generate(chip);
}

void MameOPN2::nativeGenerate(int16_t *frame)
{
    void *chip = this->chip;
    ym2612_generate_one_native(chip, frame);
}

const char *MameOPN2::emulatorName()
{
    return "MAME YM2612";
}
