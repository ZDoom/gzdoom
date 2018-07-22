#ifndef DOSBOX_OPL3_H
#define DOSBOX_OPL3_H

#include "opl_chip_base.h"

class DosBoxOPL3 final : public OPLChipBaseBufferedT<DosBoxOPL3>
{
    void *m_chip;
public:
    DosBoxOPL3();
    ~DosBoxOPL3() override;

    bool canRunAtPcmRate() const override { return true; }
    void setRate(uint32_t rate) override;
    void reset() override;
    void writeReg(uint16_t addr, uint8_t data) override;
    void nativePreGenerate() override {}
    void nativePostGenerate() override {}
    void nativeGenerateN(int16_t *output, size_t frames) override;
    const char *emulatorName() override;
};

#endif // DOSBOX_OPL3_H
