#ifndef ONP_CHIP_BASE_H
#define ONP_CHIP_BASE_H

#include <stdint.h>
#include <stddef.h>

#if !defined(_MSC_VER) && (__cplusplus <= 199711L)
#define final
#define override
#endif

#if defined(OPNMIDI_ENABLE_HQ_RESAMPLER)
class VResampler;
#endif

class OPNChipBase
{
public:
    enum { nativeRate = 53267 };
protected:
    uint32_t m_rate;
    uint32_t m_clock;
public:
    OPNChipBase();
    virtual ~OPNChipBase();

    virtual bool canRunAtPcmRate() const = 0;
    virtual bool isRunningAtPcmRate() const = 0;
    virtual bool setRunningAtPcmRate(bool r) = 0;

    virtual void setRate(uint32_t rate, uint32_t clock) = 0;
    virtual void reset() = 0;
    virtual void writeReg(uint32_t port, uint16_t addr, uint8_t data) = 0;

    virtual void nativePreGenerate() = 0;
    virtual void nativePostGenerate() = 0;
    virtual void nativeGenerate(int16_t *frame) = 0;

    virtual void generate(int16_t *output, size_t frames) = 0;
    virtual void generateAndMix(int16_t *output, size_t frames) = 0;
    virtual void generate32(int32_t *output, size_t frames) = 0;
    virtual void generateAndMix32(int32_t *output, size_t frames) = 0;

    virtual const char* emulatorName() = 0;
private:
    OPNChipBase(const OPNChipBase &c);
    OPNChipBase &operator=(const OPNChipBase &c);
};

// A base class providing F-bounded generic and efficient implementations,
// supporting resampling of chip outputs
template <class T>
class OPNChipBaseT : public OPNChipBase
{
public:
    OPNChipBaseT();
    virtual ~OPNChipBaseT();

    bool isRunningAtPcmRate() const override;
    bool setRunningAtPcmRate(bool r) override;

    virtual void setRate(uint32_t rate, uint32_t clock) override;
    virtual void reset() override;
    void generate(int16_t *output, size_t frames) override;
    void generateAndMix(int16_t *output, size_t frames) override;
    void generate32(int32_t *output, size_t frames) override;
    void generateAndMix32(int32_t *output, size_t frames) override;
private:
    bool m_runningAtPcmRate;
    void setupResampler(uint32_t rate);
    void resetResampler();
    void resampledGenerate(int32_t *output);
#if defined(OPNMIDI_ENABLE_HQ_RESAMPLER)
    VResampler *m_resampler;
#else
    int32_t m_oldsamples[2];
    int32_t m_samples[2];
    int32_t m_samplecnt;
    int32_t m_rateratio;
    enum { rsm_frac = 10 };
#endif
    // amplitude scale factors in and out of resampler, varying for chips;
    // values are OK to "redefine", the static polymorphism will accept it.
    enum { resamplerPreAmplify = 1, resamplerPostAttenuate = 1 };
};

// A base class which provides frame-by-frame interfaces on emulations which
// don't have a routine for it. It produces outputs in fixed size buffers.
// Fast register updates will suffer some latency because of buffering.
template <class T, unsigned Buffer = 256>
class OPNChipBaseBufferedT : public OPNChipBaseT<T>
{
public:
    OPNChipBaseBufferedT()
        : OPNChipBaseT<T>(), m_bufferIndex(0) {}
    virtual ~OPNChipBaseBufferedT()
        {}
public:
    void reset() override;
    void nativeGenerate(int16_t *frame) override;
protected:
    virtual void nativeGenerateN(int16_t *output, size_t frames) = 0;
private:
    unsigned m_bufferIndex;
    int16_t m_buffer[2 * Buffer];
};

#include "opn_chip_base.tcc"

#endif // ONP_CHIP_BASE_H
