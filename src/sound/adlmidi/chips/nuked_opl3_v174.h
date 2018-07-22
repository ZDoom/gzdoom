#ifndef NUKED_OPL3174_H
#define NUKED_OPL3174_H

#include "opl_chip_base.h"

class NukedOPL3v174 final : public OPLChipBaseT<NukedOPL3v174>
{
    void *m_chip;
public:
    NukedOPL3v174();
    ~NukedOPL3v174() override;

    bool canRunAtPcmRate() const override { return false; }
    void setRate(uint32_t rate) override;
    void reset() override;
    void writeReg(uint16_t addr, uint8_t data) override;
    void nativePreGenerate() override {}
    void nativePostGenerate() override {}
    void nativeGenerate(int16_t *frame) override;
    const char *emulatorName() override;
};

#endif // NUKED_OPL3174_H
