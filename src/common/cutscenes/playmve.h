/*
 * InterplayDecoder
 * Copyright (C) 2020 sirlemonhead
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* This code is based on interplayvideo.c, dpcm.c and ipmovie.c from the FFmpeg project which can be obtained
 * from http://www.ffmpeg.org/. Below is the license from interplayvideo.c
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Interplay MVE Video Decoder
 * Copyright (C) 2003 The FFmpeg project
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#pragma once

#include "files.h"
#include "animtexture.h"
#include "s_music.h"

#ifndef playmve_h_
#define playmve_h_

class InterplayDecoder
{
public:
    enum
    {
        CHUNK_PREAMBLE_SIZE = 4,
        OPCODE_PREAMBLE_SIZE = 4,

        CHUNK_INIT_AUDIO   = 0x0000,
        CHUNK_AUDIO_ONLY   = 0x0001,
        CHUNK_INIT_VIDEO   = 0x0002,
        CHUNK_VIDEO        = 0x0003,
        CHUNK_SHUTDOWN     = 0x0004,
        CHUNK_END          = 0x0005,
        /* these last types are used internally */
        CHUNK_DONE         = 0xFFFC,
        CHUNK_NOMEM        = 0xFFFD,
        CHUNK_EOF          = 0xFFFE,
        CHUNK_BAD          = 0xFFFF,

        OPCODE_END_OF_STREAM           = 0x00,
        OPCODE_END_OF_CHUNK            = 0x01,
        OPCODE_CREATE_TIMER            = 0x02,
        OPCODE_INIT_AUDIO_BUFFERS      = 0x03,
        OPCODE_START_STOP_AUDIO        = 0x04,
        OPCODE_INIT_VIDEO_BUFFERS      = 0x05,
        OPCODE_UNKNOWN_06              = 0x06,
        OPCODE_SEND_BUFFER             = 0x07,
        OPCODE_AUDIO_FRAME             = 0x08,
        OPCODE_SILENCE_FRAME           = 0x09,
        OPCODE_INIT_VIDEO_MODE         = 0x0A,
        OPCODE_CREATE_GRADIENT         = 0x0B,
        OPCODE_SET_PALETTE             = 0x0C,
        OPCODE_SET_PALETTE_COMPRESSED  = 0x0D,
        OPCODE_UNKNOWN_0E              = 0x0E,
        OPCODE_SET_DECODING_MAP        = 0x0F,
        OPCODE_UNKNOWN_10              = 0x10,
        OPCODE_VIDEO_DATA              = 0x11,
        OPCODE_UNKNOWN_12              = 0x12,
        OPCODE_UNKNOWN_13              = 0x13,
        OPCODE_UNKNOWN_14              = 0x14,
        OPCODE_UNKNOWN_15              = 0x15,

        PALETTE_COUNT = 256,
        kAudioBlocks    = 20 // alloc a lot of blocks - need to store lots of audio data before video frames start.
    };

    InterplayDecoder(bool soundenabled);
    ~InterplayDecoder();

    bool Open(FileReader &fr);
    void Close();
    bool RunFrame(uint64_t clock);

    struct AudioData
    {
        int hFx;
        int nChannels;
        uint16_t nSampleRate;
        uint8_t nBitDepth;

        int16_t samples[6000 * kAudioBlocks]; // must be a multiple of the stream buffer size
        int nWrite;
        int nRead;
    };

    AudioData audio;
    AnimTextures animtex;

    AnimTextures& animTex() { return animtex; }

private:
    struct DecodeMap
    {
        uint8_t* pData;
        uint32_t nSize;
    };

    struct Palette
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };

    uint8_t* GetCurrentFrame();
    uint8_t* GetPreviousFrame();
    void SwapFrames();
    void CopyBlock(uint8_t* pDest, uint8_t* pSrc);
    void DecodeBlock0(int32_t offset);
    void DecodeBlock1(int32_t offset);
    void DecodeBlock2(int32_t offset);
    void DecodeBlock3(int32_t offset);
    void DecodeBlock4(int32_t offset);
    void DecodeBlock5(int32_t offset);
    void DecodeBlock7(int32_t offset);
    void DecodeBlock8(int32_t offset);
    void DecodeBlock9(int32_t offset);
    void DecodeBlock10(int32_t offset);
    void DecodeBlock11(int32_t offset);
    void DecodeBlock12(int32_t offset);
    void DecodeBlock13(int32_t offset);
    void DecodeBlock14(int32_t offset);
    void DecodeBlock15(int32_t offset);

    FileReader fr;

    bool bIsPlaying, bAudioStarted;

    uint32_t nTimerRate, nTimerDiv;
    uint32_t nWidth, nHeight, nFrame;
    double nFps;
    uint64_t nFrameDuration;

    uint8_t* pVideoBuffers[2];
    uint32_t nCurrentVideoBuffer, nPreviousVideoBuffer;
    int32_t videoStride;

    DecodeMap decodeMap;

    Palette palette[256];
    uint64_t nNextFrameTime = 0;
    SoundStream* stream = nullptr;
};

#endif
