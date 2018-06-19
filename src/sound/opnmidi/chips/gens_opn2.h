#ifndef GENS_OPN2_H
#define GENS_OPN2_H

#include "opn_chip_base.h"

class Ym2612_Emu;
class GensOPN2 final : public OPNChipBaseBufferedT<GensOPN2>
{
    Ym2612_Emu *chip;
public:
    GensOPN2();
    ~GensOPN2() override;

    bool canRunAtPcmRate() const override { return true; }
    void setRate(uint32_t rate, uint32_t clock) override;
    void reset() override;
    void writeReg(uint32_t port, uint16_t addr, uint8_t data) override;
    void nativePreGenerate() override {}
    void nativePostGenerate() override {}
    void nativeGenerateN(int16_t *output, size_t frames) override;
    const char *emulatorName() override;
};

#endif // GENS_OPN2_H
