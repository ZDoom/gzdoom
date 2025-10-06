/*
 * Interfaces over Yamaha OPN2 (YM2612) chip emulators
 *
 * Copyright (c) 2017-2025 Vitaly Novichkov (Wohlstand)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef ONP_CHIP_BASE_H
#define ONP_CHIP_BASE_H

#include "opn_chip_family.h"
#include <stdint.h>
#include <stddef.h>

#if !defined(_MSC_VER) && (__cplusplus <= 199711L)
#define final
#define override
#endif

#if defined(OPNMIDI_ENABLE_HQ_RESAMPLER)
class VResampler;
#endif

#if defined(OPNMIDI_AUDIO_TICK_HANDLER)
extern void opn2_audioTickHandler(void *instance, uint32_t chipId, uint32_t rate);
#endif

class OPNChipBase
{
protected:
    uint32_t m_id;
    uint32_t m_rate;
    uint32_t m_clock;
    OPNFamily m_family;
public:
    explicit OPNChipBase(OPNFamily f);
    virtual ~OPNChipBase();

    virtual OPNFamily family() const = 0;
    uint32_t clockRate() const;
    virtual uint32_t nativeClockRate() const = 0;

    uint32_t chipId() const { return m_id; }
    void setChipId(uint32_t id) { m_id = id; }

    virtual bool canRunAtPcmRate() const = 0;
    virtual bool isRunningAtPcmRate() const = 0;
    virtual bool setRunningAtPcmRate(bool r) = 0;
#if defined(OPNMIDI_AUDIO_TICK_HANDLER)
    virtual void setAudioTickHandlerInstance(void *instance) = 0;
#endif

    virtual void setRate(uint32_t rate, uint32_t clock) = 0;
    virtual uint32_t effectiveRate() const = 0;
    virtual uint32_t nativeRate() const = 0;
    virtual void reset() = 0;
    virtual void writeReg(uint32_t port, uint16_t addr, uint8_t data) = 0;

    // extended
    virtual void writePan(uint16_t addr, uint8_t data) { (void)addr; (void)data; }

    virtual void nativePreGenerate() = 0;
    virtual void nativePostGenerate() = 0;
    virtual void nativeGenerate(int16_t *frame) = 0;

    virtual void generate(int16_t *output, size_t frames) = 0;
    virtual void generateAndMix(int16_t *output, size_t frames) = 0;
    virtual void generate32(int32_t *output, size_t frames) = 0;
    virtual void generateAndMix32(int32_t *output, size_t frames) = 0;

    virtual const char* emulatorName() = 0;
    /**
     * @brief Does emulator has the per-channel full-panning extension?
     * @return true if emulator has this extension, false if emulator has only original behaviour
     */
    virtual bool hasFullPanning() = 0;
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
    explicit OPNChipBaseT(OPNFamily f);
    virtual ~OPNChipBaseT();

    OPNFamily family() const override;
    uint32_t nativeClockRate() const override;

    bool isRunningAtPcmRate() const override;
    bool setRunningAtPcmRate(bool r) override;
#if defined(OPNMIDI_AUDIO_TICK_HANDLER)
    void setAudioTickHandlerInstance(void *instance);
#endif

    virtual void setRate(uint32_t rate, uint32_t clock) override;
    uint32_t effectiveRate() const override;
    uint32_t nativeRate() const override;
    virtual void reset() override;
    void generate(int16_t *output, size_t frames) override;
    void generateAndMix(int16_t *output, size_t frames) override;
    void generate32(int32_t *output, size_t frames) override;
    void generateAndMix32(int32_t *output, size_t frames) override;
private:
    bool m_runningAtPcmRate;
#if defined(OPNMIDI_AUDIO_TICK_HANDLER)
    void *m_audioTickHandlerInstance;
#endif
    void nativeTick(int16_t *frame);
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
    explicit OPNChipBaseBufferedT(OPNFamily f)
        : OPNChipBaseT<T>(f), m_bufferIndex(0) {}
    virtual ~OPNChipBaseBufferedT()
        {}
    enum { buffer_size = Buffer };
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
