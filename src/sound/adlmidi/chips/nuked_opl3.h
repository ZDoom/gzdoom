#ifndef NUKED_OPL3_H
#define NUKED_OPL3_H

#include "opl_chip_base.h"

class NukedOPL3 final : public OPLChipBaseT<NukedOPL3>
{
    void *m_chip;
public:
    NukedOPL3();
    ~NukedOPL3() override;

    bool canRunAtPcmRate() const override { return false; }
    void setRate(uint32_t rate) override;
    void reset() override;
    void writeReg(uint16_t addr, uint8_t data) override;
    void nativePreGenerate() override {}
    void nativePostGenerate() override {}
    void nativeGenerate(int16_t *frame) override;
    const char *emulatorName() override;
};

#endif // NUKED_OPL3_H
