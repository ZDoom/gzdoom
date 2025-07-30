/*
 * Interfaces over Yamaha OPN2 (YM2612) chip emulators
 *
 * Copyright (c) 2018-2025 Vitaly Novichkov (Wohlstand)
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

#include "mame_opna.h"
#include "mamefm/fm.h"
#include "mamefm/fmopn_2608rom.h"
#include "mamefm/2608intf.h"
#include "mamefm/resampler.hpp"

struct MameOPNA::Impl {
    ym2608_device dev;
    void *chip;

    // typedef chip::SincResampler Resampler;
    typedef chip::LinearResampler Resampler;

    Resampler *psgrsm;
    int32_t *psgbuffer;

    static const ssg_callbacks cbssg;

    // callbacks
    static uint8_t cbInternalReadByte(device_t *, offs_t);
    static uint8_t cbExternalReadByte(device_t *, offs_t) { return 0; }
    static void cbExternalWriteByte(device_t *, offs_t, uint8_t) {}
    static void cbHandleTimer(device_t *, int, int, int) {}
    static void cbHandleIRQ(device_t *, int) {}

    static void cbSsgSetClock(device_t *dev, int clock);
    static void cbSsgWrite(device_t *dev, int addr, int data);
    static int cbSsgRead(device_t *dev);
    static void cbSsgReset(device_t *dev);
};

const ssg_callbacks MameOPNA::Impl::cbssg =
{
    &cbSsgSetClock,
    &cbSsgWrite,
    &cbSsgRead,
    &cbSsgReset,
};


MameOPNA::MameOPNA(OPNFamily f)
    : OPNChipBaseBufferedT(f), impl(new Impl)
{
    impl->chip = NULL;
    impl->psgrsm = NULL;
    impl->psgbuffer = NULL;
    MameOPNA::setRate(m_rate, m_clock);
}

MameOPNA::~MameOPNA()
{
    delete impl->psgrsm;
    delete[] impl->psgbuffer;
    ym2608_shutdown(impl->chip);
    delete impl;
}

void MameOPNA::setRate(uint32_t rate, uint32_t clock)
{
    OPNChipBaseBufferedT::setRate(rate, clock);
    if(impl->chip)
        ym2608_shutdown(impl->chip);

    uint32_t chipRate = isRunningAtPcmRate() ? rate : nativeRate();
    ym2608_device *device = &impl->dev;
    void *chip = impl->chip = ym2608_init(
        device, (int)clock, (int)chipRate,
        &Impl::cbInternalReadByte, &Impl::cbExternalReadByte,
        &Impl::cbExternalWriteByte,
        &Impl::cbHandleTimer, &Impl::cbHandleIRQ, &Impl::cbssg);

    PSG *psg = &device->m_psg;
    memset(psg, 0, sizeof(PSG));

    uint32_t psgRate = clock / 32;
    PSG_init(psg, clock / 4, psgRate);  // TODO libOPNMIDI verify clocks
    PSG_setVolumeMode(psg, 1);  // YM2149 volume mode

    delete impl->psgrsm;
    Impl::Resampler *psgrsm = impl->psgrsm = new Impl::Resampler;
    psgrsm->init(psgRate, chipRate, 40);

    delete[] impl->psgbuffer;
    impl->psgbuffer = new int32_t[2 * psgrsm->calculateInternalSampleSize(buffer_size)];

    ym2608_reset_chip(chip);
    ym2608_write(chip, 0, 0x29);
    ym2608_write(chip, 1, 0x9f);
}

void MameOPNA::reset()
{
    OPNChipBaseBufferedT::reset();
    void *chip = impl->chip;
    ym2608_reset_chip(chip);
    ym2608_write(chip, 0, 0x29);
    ym2608_write(chip, 1, 0x9f);
}

void MameOPNA::writeReg(uint32_t port, uint16_t addr, uint8_t data)
{
    void *chip = impl->chip;
    ym2608_write(chip, 0 + (int)(port) * 2, (uint8_t)addr);
    ym2608_write(chip, 1 + (int)(port) * 2, data);
}

void MameOPNA::writePan(uint16_t chan, uint8_t data)
{
    void *chip = impl->chip;
    ym2608_write_pan(chip, (int)chan, data);
}

void MameOPNA::nativeGenerateN(int16_t *output, size_t frames)
{
    void *chip = impl->chip;

    FMSAMPLE fmLR[2 * buffer_size];
    FMSAMPLE *fmR = fmLR + buffer_size;
    FMSAMPLE *fmbufs[2] = { fmLR, fmR };

    ym2608_update_one(chip, fmbufs, (int)frames);

    PSG *psg = &impl->dev.m_psg;
    Impl::Resampler *psgrsm = impl->psgrsm;
    size_t psgframes = psgrsm->calculateInternalSampleSize(frames);

    int32_t *rawpsgLR = impl->psgbuffer;
    int32_t *rawpsgR = rawpsgLR + psgframes;
    int32_t *rawpsgbufs[2] = { rawpsgLR, rawpsgR };
    PSG_calc_stereo(psg, rawpsgbufs, (int32_t)psgframes);

    int32_t **psgbufs = psgrsm->interpolate(rawpsgbufs, frames, psgframes);
    int32_t *psgL = psgbufs[0];
    int32_t *psgR = psgbufs[1];

    for(size_t i = 0; i < frames; ++i)
    {
        int32_t l = fmLR[i] + psgL[i];
        l = (l > -32768) ? l : -32768;
        l = (l < 32767) ? l : 32767;
        int32_t r = fmR[i] + psgR[i];
        r = (r > -32768) ? r : -32768;
        r = (r < 32767) ? r : 32767;
        output[2 * i] = l;
        output[2 * i + 1] = r;
    }
}

const char *MameOPNA::emulatorName()
{
    return "MAME YM2608";  // git 2018-12-15 rev 8ab05c0
}

uint8_t MameOPNA::Impl::cbInternalReadByte(device_t *dev, offs_t off)
{
    (void)dev;
    return YM2608_ADPCM_ROM[off & 0x1fff];
}

void MameOPNA::Impl::cbSsgSetClock(device_t *dev, int clock)
{
    ym2608_device *ym = static_cast<ym2608_device *>(dev);
    PSG_set_clock(&ym->m_psg, (uint32_t)clock);
}

void MameOPNA::Impl::cbSsgWrite(device_t *dev, int addr, int data)
{
    ym2608_device *ym = static_cast<ym2608_device *>(dev);
    PSG_writeIO(&ym->m_psg, (uint32_t)addr, (uint32_t)data);
}

int MameOPNA::Impl::cbSsgRead(device_t *dev)
{
    ym2608_device *ym = static_cast<ym2608_device *>(dev);
    return PSG_readIO(&ym->m_psg);
}

void MameOPNA::Impl::cbSsgReset(device_t *dev)
{
    ym2608_device *ym = static_cast<ym2608_device *>(dev);
    return PSG_reset(&ym->m_psg);
}
