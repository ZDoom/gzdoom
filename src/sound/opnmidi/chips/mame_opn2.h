#ifndef MAME_OPN2_H
#define MAME_OPN2_H

#include "opn_chip_base.h"

class MameOPN2 final : public OPNChipBaseT<MameOPN2>
{
    void *chip;
public:
    MameOPN2();
    ~MameOPN2() override;

    bool canRunAtPcmRate() const override { return true; }
    void setRate(uint32_t rate, uint32_t clock) override;
    void reset() override;
    void writeReg(uint32_t port, uint16_t addr, uint8_t data) override;
    void nativePreGenerate() override;
    void nativePostGenerate() override {}
    void nativeGenerate(int16_t *frame) override;
    const char *emulatorName() override;
};

#endif // MAME_OPN2_H
