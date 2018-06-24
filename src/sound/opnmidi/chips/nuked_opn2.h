#ifndef NUKED_OPN2_H
#define NUKED_OPN2_H

#include "opn_chip_base.h"

class NukedOPN2 final : public OPNChipBaseT<NukedOPN2>
{
    void *chip;
public:
    NukedOPN2();
    ~NukedOPN2() override;

    bool canRunAtPcmRate() const override { return false; }
    void setRate(uint32_t rate, uint32_t clock) override;
    void reset() override;
    void writeReg(uint32_t port, uint16_t addr, uint8_t data) override;
    void nativePreGenerate() override {}
    void nativePostGenerate() override {}
    void nativeGenerate(int16_t *frame) override;
    const char *emulatorName() override;
    // amplitude scale factors to use in resampling
    enum { resamplerPreAmplify = 11, resamplerPostAttenuate = 2 };
};

#endif // NUKED_OPN2_H
