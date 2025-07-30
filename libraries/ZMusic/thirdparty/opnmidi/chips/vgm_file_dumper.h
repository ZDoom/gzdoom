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

#ifndef VGM_FILE_DUMPER_H
#define VGM_FILE_DUMPER_H

#include "opn_chip_base.h"

class VGMFileDumper final : public OPNChipBaseBufferedT<VGMFileDumper>
{
    //! Output file instance
    FILE    *m_output;
    //! Count of song bytes written into the file
    uint32_t m_bytes_written;
    //! Waiting delay of song in 1/44100'ths of second
    uint32_t m_samples_written;
    //! Waiting delay of loop part in 1/44100'ths of second
    uint32_t m_samples_loop;
    //! Requested sample rate
    uint32_t m_actual_rate;
    //! Cached delay value in 1/44100'ths of second
    uint64_t m_delay;
    //! Don't increase waiting delay after end of song caught
    bool     m_end_caught;
    //! Index of chip (0'th is master, 1 is a helper)
    int      m_chip_index;
    //! First chip
    VGMFileDumper* m_first;

    struct VgmHead
    {
        char     magic[4];
        uint32_t eof_offset;
        uint32_t version;
        uint32_t clock_sn76489;
        uint32_t clock_ym2413;
        uint32_t offset_gd3;
        uint32_t total_samples;
        uint32_t offset_loop;
        uint32_t loop_samples;
        uint32_t rate;
        uint16_t feedback_sn76489;
        uint8_t  shift_register_width_sn76489;
        uint8_t  flags_sn76489;
        uint32_t clock_ym2612;
        uint32_t clock_ym2151;
        uint32_t offset_data;
    };
    VgmHead m_vgm_head;

    void writeHead();
    void writeCommand(uint_fast8_t cmd, uint_fast16_t key = 0, uint_fast8_t value = 0);
    void writeWait(uint_fast16_t value);
    void flushWait();
public:
    explicit VGMFileDumper(OPNFamily f, int index, void *first);
    ~VGMFileDumper() override;

    bool canRunAtPcmRate() const override { return true; }
    void setRate(uint32_t rate, uint32_t clock) override;
    void reset() override;
    void writeReg(uint32_t port, uint16_t addr, uint8_t data) override;
    void writePan(uint16_t chan, uint8_t data) override;
    void nativePreGenerate() override {}
    void nativePostGenerate() override {}
    void nativeGenerateN(int16_t *output, size_t frames) override;
    const char *emulatorName() override;
    void writeLoopStart();
    void writeLoopEnd();
    static void loopStartHook(void *self);
    static void loopEndHook(void *self);
};

#endif // VGM_FILE_DUMPER_H
