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


#include <algorithm>
#include "playmve.h"
#include "printf.h"
#include "v_draw.h"
#include "s_music.h"
#include "cmdlib.h"




static const int16_t delta_table[] = {
         0,      1,      2,      3,      4,      5,      6,      7,
         8,      9,     10,     11,     12,     13,     14,     15,
        16,     17,     18,     19,     20,     21,     22,     23,
        24,     25,     26,     27,     28,     29,     30,     31,
        32,     33,     34,     35,     36,     37,     38,     39,
        40,     41,     42,     43,     47,     51,     56,     61,
        66,     72,     79,     86,     94,    102,    112,    122,
       133,    145,    158,    173,    189,    206,    225,    245,
       267,    292,    318,    348,    379,    414,    452,    493,
       538,    587,    640,    699,    763,    832,    908,    991,
      1081,   1180,   1288,   1405,   1534,   1673,   1826,   1993,
      2175,   2373,   2590,   2826,   3084,   3365,   3672,   4008,
      4373,   4772,   5208,   5683,   6202,   6767,   7385,   8059,
      8794,   9597,  10472,  11428,  12471,  13609,  14851,  16206,
     17685,  19298,  21060,  22981,  25078,  27367,  29864,  32589,
    -29973, -26728, -23186, -19322, -15105, -10503,  -5481,     -1,
         1,      1,   5481,  10503,  15105,  19322,  23186,  26728,
     29973, -32589, -29864, -27367, -25078, -22981, -21060, -19298,
    -17685, -16206, -14851, -13609, -12471, -11428, -10472,  -9597,
     -8794,  -8059,  -7385,  -6767,  -6202,  -5683,  -5208,  -4772,
     -4373,  -4008,  -3672,  -3365,  -3084,  -2826,  -2590,  -2373,
     -2175,  -1993,  -1826,  -1673,  -1534,  -1405,  -1288,  -1180,
     -1081,   -991,   -908,   -832,   -763,   -699,   -640,   -587,
      -538,   -493,   -452,   -414,   -379,   -348,   -318,   -292,
      -267,   -245,   -225,   -206,   -189,   -173,   -158,   -145,
      -133,   -122,   -112,   -102,    -94,    -86,    -79,    -72,
       -66,    -61,    -56,    -51,    -47,    -43,    -42,    -41,
       -40,    -39,    -38,    -37,    -36,    -35,    -34,    -33,
       -32,    -31,    -30,    -29,    -28,    -27,    -26,    -25,
       -24,    -23,    -22,    -21,    -20,    -19,    -18,    -17,
       -16,    -15,    -14,    -13,    -12,    -11,    -10,     -9,
        -8,     -7,     -6,     -5,     -4,     -3,     -2,     -1
};


// macro to fetch 16-bit little-endian words from a bytestream
#define LE_16(x)  ((*x) | ((*(x+1)) << 8))

static bool StreamCallbackFunc(SoundStream* stream, void* buff, int len, void* userdata)
{
    InterplayDecoder* pId = (InterplayDecoder*)userdata;
    memcpy(buff, &pId->audio.samples[pId->audio.nRead], len);
    pId->audio.nRead += len / 2;
    if (pId->audio.nRead >= (int)countof(pId->audio.samples)) pId->audio.nRead = 0;
    return true;
}

InterplayDecoder::InterplayDecoder(bool soundenabled)
{
    bIsPlaying = false;
    bAudioStarted = !soundenabled;  // This prevents the stream from getting created

    nWidth  = 0;
    nHeight = 0;
    nFrame  = 0;

    memset(palette, 0, sizeof(palette));
    memset(&audio, 0, sizeof(audio));
    audio.nRead = 18000;    // skip the initial silence. This is needed to sync audio and video because OpenAL's lag is a bit on the high side.


    nFps = 0.0;
    nFrameDuration = 0;
    nTimerRate = 0;
    nTimerDiv  = 0;

    pVideoBuffers[0] = nullptr;
    pVideoBuffers[1] = nullptr;

    decodeMap.pData = nullptr;
    decodeMap.nSize = 0;

    nCurrentVideoBuffer  = 0;
    nPreviousVideoBuffer = 1;

    videoStride = 0;
}

InterplayDecoder::~InterplayDecoder()
{
    Close();
}

void InterplayDecoder::SwapFrames()
{
    int t = nPreviousVideoBuffer;
    nPreviousVideoBuffer = nCurrentVideoBuffer;
    nCurrentVideoBuffer = t;
}

void InterplayDecoder::Close()
{
    fr.Close();
    bIsPlaying = false;
    if (stream)
        S_StopCustomStream(stream);
    stream = nullptr;

    if (decodeMap.pData) {
        delete[] decodeMap.pData;
        decodeMap.pData = nullptr;
    }

    if (pVideoBuffers[0]) {
        delete[] pVideoBuffers[0];
        pVideoBuffers[0] = nullptr;
    }
    if (pVideoBuffers[1]) {
        delete[] pVideoBuffers[1];
        pVideoBuffers[1] = nullptr;
    }

}

bool InterplayDecoder::Open(FileReader &fr_)
{
    // open the file (read only)
    char lsig[20];

    // check the file signature
    fr_.Read((uint8_t*)lsig, sizeof(lsig));
    if (memcmp(lsig, "Interplay MVE File\x1A\0", sizeof(lsig)) != 0)
    {
        Printf(TEXTCOLOR_RED "InterplayDecoder: Unknown MVE signature\n ");
        return false;
    }

    // skip the next 6 bytes
    fr_.Seek(6, FileReader::SeekCur);
    fr = std::move(fr_);

    //Run();

    return true;
}

bool InterplayDecoder::RunFrame(uint64_t clock)
{
    uint8_t chunkPreamble[CHUNK_PREAMBLE_SIZE];
    uint8_t opcodePreamble[OPCODE_PREAMBLE_SIZE];
    uint8_t opcodeType;
    uint8_t opcodeVersion;
    int opcodeSize, chunkSize;
    int chunkType = 0;

    // iterate through the chunks in the file
    do
    {
        // handle timing - wait until we're ready to process the next frame.
        if (nNextFrameTime > clock) {
            return true;
        }
        else {
            nNextFrameTime += nFrameDuration;
        }

        if (fr.Read(chunkPreamble, CHUNK_PREAMBLE_SIZE) != CHUNK_PREAMBLE_SIZE) {
            Printf(TEXTCOLOR_RED "InterplayDecoder: could not read from file (EOF?)\n");
            return false;
        }

        chunkSize = LE_16(&chunkPreamble[0]);
        chunkType = LE_16(&chunkPreamble[2]);

        // iterate through individual opcodes
        while (chunkSize > 0)
        {
            if (fr.Read(opcodePreamble, OPCODE_PREAMBLE_SIZE) != OPCODE_PREAMBLE_SIZE)
            {
                Printf(TEXTCOLOR_RED "InterplayDecoder: could not read from file (EOF?)\n");
                return false;
            }

            opcodeSize = LE_16(&opcodePreamble[0]);
            opcodeType = opcodePreamble[2];
            opcodeVersion = opcodePreamble[3];

            chunkSize -= OPCODE_PREAMBLE_SIZE;
            chunkSize -= opcodeSize;

            switch (opcodeType)
            {
            case OPCODE_END_OF_STREAM:
            {
                fr.Seek(opcodeSize, FileReader::SeekCur);
                break;
            }

            case OPCODE_END_OF_CHUNK:
            {
                fr.Seek(opcodeSize, FileReader::SeekCur);
                break;
            }

            case OPCODE_CREATE_TIMER:
            {
                nTimerRate = fr.ReadUInt32();
                nTimerDiv  = fr.ReadUInt16();
                nFrameDuration = ((uint64_t)nTimerRate * nTimerDiv) * 1000;
                break;
            }

            case OPCODE_INIT_AUDIO_BUFFERS:
            {
                fr.Seek(2, FileReader::SeekCur);
                uint16_t flags = fr.ReadUInt16();
                audio.nSampleRate = fr.ReadUInt16();

                uint32_t nBufferBytes;

                if (opcodeVersion == 0) {
                    nBufferBytes = fr.ReadUInt16();
                }
                else {
                    nBufferBytes = fr.ReadUInt32();
                }

                if (flags & 0x1) {
                    audio.nChannels = 2;
                }
                else {
                    audio.nChannels = 1;
                }
                if (flags & 0x2) {
                    audio.nBitDepth = 16;
                }
                else {
                    audio.nBitDepth = 8;
                }
                break;
            }

            case OPCODE_START_STOP_AUDIO:
            {
                if (!bAudioStarted)
                {
                    // start audio playback
                    stream = S_CreateCustomStream(6000, audio.nSampleRate, audio.nChannels, StreamCallbackFunc, this);
                    bAudioStarted = true;
                }

                fr.Seek(opcodeSize, FileReader::SeekCur);
                break;
            }

            case OPCODE_INIT_VIDEO_BUFFERS:
            {
                assert(opcodeSize == 8);
                nWidth  = fr.ReadUInt16() * 8;
                nHeight = fr.ReadUInt16() * 8;

                int count = fr.ReadUInt16();
                int truecolour = fr.ReadUInt16();
                assert(truecolour == 0);

                pVideoBuffers[0] = new uint8_t[nWidth * nHeight];
                pVideoBuffers[1] = new uint8_t[nWidth * nHeight];

                videoStride = nWidth;

                animtex.SetSize(AnimTexture::Paletted, nWidth, nHeight);
                break;
            }

            case OPCODE_UNKNOWN_06:
            case OPCODE_UNKNOWN_0E:
            case OPCODE_UNKNOWN_10:
            case OPCODE_UNKNOWN_12:
            case OPCODE_UNKNOWN_13:
            case OPCODE_UNKNOWN_14:
            case OPCODE_UNKNOWN_15:
            {
                fr.Seek(opcodeSize, FileReader::SeekCur);
                break;
            }

            case OPCODE_SEND_BUFFER:
            {
                int nPalStart = fr.ReadUInt16();
                int nPalCount = fr.ReadUInt16();

                animtex.SetFrame(&palette[0].r , GetCurrentFrame());

                nFrame++;
                SwapFrames();

                fr.Seek(opcodeSize-4, FileReader::SeekCur);
                break;
            }

            case OPCODE_AUDIO_FRAME:
            {
                int nStart = (int)fr.Tell();
                uint16_t seqIndex   = fr.ReadUInt16();
                uint16_t streamMask = fr.ReadUInt16();
                uint16_t nSamples   = fr.ReadUInt16(); // number of samples this chunk

                int predictor[2];
                int i = 0;

                for (int ch = 0; ch < audio.nChannels; ch++)
                {
                    predictor[ch] = fr.ReadUInt16();
                    i++;

                    if (predictor[ch] & 0x8000) {
                        predictor[ch] |= 0xFFFF0000; // sign extend
                    }

                    audio.samples[audio.nWrite++] = predictor[ch];
                    if (audio.nWrite >= (int)countof(audio.samples)) audio.nWrite = 0;
                }

                int ch = 0;
                for (; i < (nSamples / 2); i++)
                {
                    predictor[ch] += delta_table[fr.ReadUInt8()];
                    predictor[ch] = clamp(predictor[ch], -32768, 32768);

                    audio.samples[audio.nWrite++] = predictor[ch];
                    if (audio.nWrite >= (int)countof(audio.samples)) audio.nWrite = 0;

                    // toggle channel
                    ch ^= audio.nChannels - 1;
                }

                int nEnd = (int)fr.Tell();
                int nRead = nEnd - nStart;
                assert(opcodeSize == nRead);
                break;
            }

            case OPCODE_SILENCE_FRAME:
            {
                uint16_t seqIndex = fr.ReadUInt16();
                uint16_t streamMask = fr.ReadUInt16();
                uint16_t nStreamLen = fr.ReadUInt16();
                break;
            }

            case OPCODE_INIT_VIDEO_MODE:
            {
                fr.Seek(opcodeSize, FileReader::SeekCur);
                break;
            }

            case OPCODE_CREATE_GRADIENT:
            {
                fr.Seek(opcodeSize, FileReader::SeekCur);
                Printf("InterplayDecoder: Create gradient not supported.\n");
                break;
            }

            case OPCODE_SET_PALETTE:
            {
                if (opcodeSize > 0x304 || opcodeSize < 4) {
                    Printf("set_palette opcode with invalid size\n");
                    chunkType = CHUNK_BAD;
                    break;
                }

                int nPalStart = fr.ReadUInt16();
                int nPalCount = fr.ReadUInt16();
                for (int i = nPalStart; i <= nPalCount; i++)
                {
                    palette[i].r = fr.ReadUInt8() << 2;
                    palette[i].g = fr.ReadUInt8() << 2;
                    palette[i].b = fr.ReadUInt8() << 2;
                }
                break;
            }

            case OPCODE_SET_PALETTE_COMPRESSED:
            {
                fr.Seek(opcodeSize, FileReader::SeekCur);
                Printf("InterplayDecoder: Set palette compressed not supported.\n");
                break;
            }

            case OPCODE_SET_DECODING_MAP:
            {
                if (!decodeMap.pData)
                {
                    decodeMap.pData = new uint8_t[opcodeSize];
                    decodeMap.nSize = opcodeSize;
                }
                else
                {
                    if (opcodeSize != (int)decodeMap.nSize) {
                        delete[] decodeMap.pData;
                        decodeMap.pData = new uint8_t[opcodeSize];
                        decodeMap.nSize = opcodeSize;
                    }
                }

                int nRead = (int)fr.Read(decodeMap.pData, opcodeSize);
                assert(nRead == opcodeSize);
                break;
            }

            case OPCODE_VIDEO_DATA:
            {
                int nStart = (int)fr.Tell();

                // need to skip 14 bytes
                fr.Seek(14, FileReader::SeekCur);

                if (decodeMap.nSize)
                {
                    int i = 0;

                    for (uint32_t y = 0; y < nHeight; y += 8)
                    {
                        for (uint32_t x = 0; x < nWidth; x += 8)
                        {
                            uint32_t opcode;

                            // alternate between getting low and high 4 bits
                            if (i & 1) {
                                opcode = decodeMap.pData[i >> 1] >> 4;
                            }
                            else {
                                opcode = decodeMap.pData[i >> 1] & 0x0F;
                            }
                            i++;

                            int32_t offset = x + (y * videoStride);

                            switch (opcode)
                            {
                                default:
                                    break;
                                case 0:
                                    DecodeBlock0(offset);
                                    break;
                                case 1:
                                    DecodeBlock1(offset);
                                    break;
                                case 2:
                                    DecodeBlock2(offset);
                                    break;
                                case 3:
                                    DecodeBlock3(offset);
                                    break;
                                case 4:
                                    DecodeBlock4(offset);
                                    break;
                                case 5:
                                    DecodeBlock5(offset);
                                    break;
                                case 7:
                                    DecodeBlock7(offset);
                                    break;
                                case 8:
                                    DecodeBlock8(offset);
                                    break;
                                case 9:
                                    DecodeBlock9(offset);
                                    break;
                                case 10:
                                    DecodeBlock10(offset);
                                    break;
                                case 11:
                                    DecodeBlock11(offset);
                                    break;
                                case 12:
                                    DecodeBlock12(offset);
                                    break;
                                case 13:
                                    DecodeBlock13(offset);
                                    break;
                                case 14:
                                    DecodeBlock14(offset);
                                    break;
                                case 15:
                                    DecodeBlock15(offset);
                                    break;
                            }
                        }
                    }
                }

                int nEnd = (int)fr.Tell();
                int nSkipBytes = opcodeSize - (nEnd - nStart); // we can end up with 1 byte left we need to skip
                assert(nSkipBytes <= 1);

                fr.Seek(nSkipBytes, FileReader::SeekCur);
                break;
            }

            default:
                break;
            }
        }

    }
    while (chunkType < CHUNK_VIDEO && bIsPlaying);
    return chunkType != CHUNK_END;
}

void InterplayDecoder::CopyBlock(uint8_t* pDest, uint8_t* pSrc)
{
    for (int y = 0; y < 8; y++)
    {
        memcpy(pDest, pSrc, 8);
        pSrc  += (intptr_t)videoStride;
        pDest += (intptr_t)videoStride;
    }
}

void InterplayDecoder::DecodeBlock0(int32_t offset)
{
    // copy from the same offset but from the previous frame
    uint8_t* pDest = GetCurrentFrame() + (intptr_t)offset;
    uint8_t* pSrc = GetPreviousFrame() + (intptr_t)offset;

    CopyBlock(pDest, pSrc);
}

void InterplayDecoder::DecodeBlock1(int32_t offset)
{
    // nothing to do for this.
}

void InterplayDecoder::DecodeBlock2(int32_t offset)
{
    // copy block from 2 frames ago using a motion vector; need 1 more byte
    uint8_t B = fr.ReadUInt8();

    int x, y;

    if (B < 56) {
        x = 8 + (B % 7);
        y = B / 7;
    }
    else {
        x = -14 + ((B - 56) % 29);
        y = 8 + ((B - 56) / 29);
    }

    uint8_t* pDest = GetCurrentFrame() + (intptr_t)offset;
    uint8_t* pSrc  = GetCurrentFrame() + (intptr_t)(int64_t)offset + (int64_t)x + (int64_t(y) * (int64_t)videoStride);

    CopyBlock(pDest, pSrc);
}

void InterplayDecoder::DecodeBlock3(int32_t offset)
{
    // copy 8x8 block from current frame from an up/left block
    uint8_t B = fr.ReadUInt8();

    int x, y;

    // need 1 more byte for motion
    if (B < 56) {
        x = -(8 + (B % 7));
        y = -(B / 7);
    }
    else {
        x = -(-14 + ((B - 56) % 29));
        y = -(8 + ((B - 56) / 29));
    }

    uint8_t* pDest = GetCurrentFrame() + (intptr_t)offset;
    uint8_t* pSrc  = GetCurrentFrame() + (intptr_t)(int64_t)offset + (int64_t)x + (int64_t(y) * (int64_t)videoStride);

    CopyBlock(pDest, pSrc);
}

void InterplayDecoder::DecodeBlock4(int32_t offset)
{
    // copy a block from the previous frame; need 1 more byte
    int x, y;
    uint8_t B, BL, BH;

    B = fr.ReadUInt8();

    BL = B & 0x0F;
    BH = (B >> 4) & 0x0F;
    x = -8 + BL;
    y = -8 + BH;

    uint8_t* pDest = GetCurrentFrame() + (intptr_t)offset;
    uint8_t* pSrc = GetPreviousFrame() + (intptr_t)(int64_t)offset + (int64_t)x + (int64_t(y) * (int64_t)videoStride);

    CopyBlock(pDest, pSrc);
}

void InterplayDecoder::DecodeBlock5(int32_t offset)
{
    // copy a block from the previous frame using an expanded range; need 2 more bytes
    int8_t x = fr.ReadUInt8();
    int8_t y = fr.ReadUInt8();

    uint8_t* pDest = GetCurrentFrame() + (intptr_t)offset;
    uint8_t* pSrc = GetPreviousFrame() + (intptr_t)(int64_t)offset + (int64_t)x + (int64_t(y) * (int64_t)videoStride);

    CopyBlock(pDest, pSrc);
}

// Block6 is unknown and skipped

void InterplayDecoder::DecodeBlock7(int32_t offset)
{
    uint8_t* pBuffer = GetCurrentFrame() + (intptr_t)offset;
    uint32_t flags = 0;

    uint8_t P[2];
    P[0] = fr.ReadUInt8();
    P[1] = fr.ReadUInt8();

    // 2-color encoding
    if (P[0] <= P[1])
    {
        // need 8 more bytes from the stream
        for (int y = 0; y < 8; y++)
        {
            flags = fr.ReadUInt8() | 0x100;
            for (; flags != 1; flags >>= 1) {
                *pBuffer++ = P[flags & 1];
            }
            pBuffer += (videoStride - 8);
        }
    }
    else
    {
        // need 2 more bytes from the stream
        flags = fr.ReadUInt16();

        for (int y = 0; y < 8; y += 2)
        {
            for (int x = 0; x < 8; x += 2, flags >>= 1)
            {
                pBuffer[x] =
                    pBuffer[x + 1] =
                    pBuffer[x + videoStride] =
                    pBuffer[x + 1 + videoStride] = P[flags & 1];
            }
            pBuffer += videoStride * 2;
        }
    }
}

void InterplayDecoder::DecodeBlock8(int32_t offset)
{
    uint8_t* pBuffer = GetCurrentFrame() + (intptr_t)offset;
    uint32_t flags = 0;
    uint8_t P[4];

    // 2-color encoding for each 4x4 quadrant, or 2-color encoding on either top and bottom or left and right halves
    P[0] = fr.ReadUInt8();
    P[1] = fr.ReadUInt8();

    if (P[0] <= P[1])
    {
        for (int y = 0; y < 16; y++)
        {
            // new values for each 4x4 block
            if (!(y & 3))
            {
                if (y) {
                    P[0] = fr.ReadUInt8();
                    P[1] = fr.ReadUInt8();
                }
                flags = fr.ReadUInt16();
            }

            for (int x = 0; x < 4; x++, flags >>= 1) {
                *pBuffer++ = P[flags & 1];
            }

            pBuffer += videoStride - 4;
            // switch to right half
            if (y == 7) pBuffer -= 8 * videoStride - 4;
        }
    }
    else
    {
        flags = fr.ReadUInt32();
        P[2] = fr.ReadUInt8();
        P[3] = fr.ReadUInt8();

        if (P[2] <= P[3])
        {
            // vertical split; left & right halves are 2-color encoded
            for (int y = 0; y < 16; y++)
            {
                for (int x = 0; x < 4; x++, flags >>= 1) {
                    *pBuffer++ = P[flags & 1];
                }

                pBuffer += videoStride - 4;

                // switch to right half
                if (y == 7) {
                    pBuffer -= 8 * videoStride - 4;
                    P[0] = P[2];
                    P[1] = P[3];
                    flags = fr.ReadUInt32();
                }
            }
        }
        else
        {
            // horizontal split; top & bottom halves are 2-color encoded
            for (int y = 0; y < 8; y++)
            {
                if (y == 4) {
                    P[0] = P[2];
                    P[1] = P[3];
                    flags = fr.ReadUInt32();
                }

                for (int x = 0; x < 8; x++, flags >>= 1)
                    *pBuffer++ = P[flags & 1];

                pBuffer += (videoStride - 8);
            }
        }
    }
}

void InterplayDecoder::DecodeBlock9(int32_t offset)
{
    uint8_t* pBuffer = GetCurrentFrame() + (intptr_t)offset;
    uint8_t P[4];

    fr.Read(P, 4);

    // 4-color encoding
    if (P[0] <= P[1])
    {
        if (P[2] <= P[3])
        {
            // 1 of 4 colors for each pixel, need 16 more bytes
            for (int y = 0; y < 8; y++)
            {
                // get the next set of 8 2-bit flags
                int flags = fr.ReadUInt16();

                for (int x = 0; x < 8; x++, flags >>= 2) {
                    *pBuffer++ = P[flags & 0x03];
                }

                pBuffer += (videoStride - 8);
            }
        }
        else
        {
            // 1 of 4 colors for each 2x2 block, need 4 more bytes
            uint32_t flags = fr.ReadUInt32();

            for (int y = 0; y < 8; y += 2)
            {
                for (int x = 0; x < 8; x += 2, flags >>= 2)
                {
                    pBuffer[x] =
                        pBuffer[x + 1] =
                        pBuffer[x + videoStride] =
                        pBuffer[x + 1 + videoStride] = P[flags & 0x03];
                }

                pBuffer += videoStride * 2;
            }
        }
    }
    else
    {
        // 1 of 4 colors for each 2x1 or 1x2 block, need 8 more bytes
        uint64_t flags = fr.ReadUInt64();

        if (P[2] <= P[3])
        {
            for (int y = 0; y < 8; y++)
            {
                for (int x = 0; x < 8; x += 2, flags >>= 2)
                {
                    pBuffer[x] =
                        pBuffer[x + 1] = P[flags & 0x03];
                }
                pBuffer += videoStride;
            }
        }
        else
        {
            for (int y = 0; y < 8; y += 2)
            {
                for (int x = 0; x < 8; x++, flags >>= 2)
                {
                    pBuffer[x] =
                        pBuffer[x + videoStride] = P[flags & 0x03];
                }
                pBuffer += videoStride * 2;
            }
        }
    }
}

void InterplayDecoder::DecodeBlock10(int32_t offset)
{
    uint8_t* pBuffer = GetCurrentFrame() + (intptr_t)offset;
    uint8_t P[8];

    fr.Read(P, 4);

    // 4-color encoding for each 4x4 quadrant, or 4-color encoding on either top and bottom or left and right halves
    if (P[0] <= P[1])
    {
        int flags = 0;

        // 4-color encoding for each quadrant; need 32 bytes
        for (int y = 0; y < 16; y++)
        {
            // new values for each 4x4 block
            if (!(y & 3)) {
                if (y) fr.Read(P, 4);
                flags = fr.ReadUInt32();
            }

            for (int x = 0; x < 4; x++, flags >>= 2) {
                *pBuffer++ = P[flags & 0x03];
            }

            pBuffer += videoStride - 4;
            // switch to right half
            if (y == 7) pBuffer -= 8 * videoStride - 4;
        }
    }
    else
    {
        // vertical split?
        int vert;
        uint64_t flags = fr.ReadUInt64();

        fr.Read(P + 4, 4);
        vert = P[4] <= P[5];

        // 4-color encoding for either left and right or top and bottom halves
        for (int y = 0; y < 16; y++)
        {
            for (int x = 0; x < 4; x++, flags >>= 2)
                *pBuffer++ = P[flags & 0x03];

            if (vert)
            {
                pBuffer += videoStride - 4;
                // switch to right half
                if (y == 7) pBuffer -= 8 * videoStride - 4;
            }
            else if (y & 1) pBuffer += (videoStride - 8);

            // load values for second half
            if (y == 7) {
                memcpy(P, P + 4, 4);
                flags = fr.ReadUInt64();
            }
        }
    }
}

void InterplayDecoder::DecodeBlock11(int32_t offset)
{
    // 64-color encoding (each pixel in block is a different color)
    uint8_t* pBuffer = GetCurrentFrame() + (intptr_t)offset;

    for (int y = 0; y < 8; y++)
    {
        fr.Read(pBuffer, 8);
        pBuffer += videoStride;
    }
}

void InterplayDecoder::DecodeBlock12(int32_t offset)
{
    // 16-color block encoding: each 2x2 block is a different color
    uint8_t* pBuffer = GetCurrentFrame() + (intptr_t)offset;

    for (int y = 0; y < 8; y += 2)
    {
        for (int x = 0; x < 8; x += 2)
        {
            pBuffer[x] =
                pBuffer[x + 1] =
                pBuffer[x + videoStride] =
                pBuffer[x + 1 + videoStride] = fr.ReadUInt8();
        }
        pBuffer += videoStride * 2;
    }
}

void InterplayDecoder::DecodeBlock13(int32_t offset)
{
    // 4-color block encoding: each 4x4 block is a different color
    uint8_t* pBuffer = GetCurrentFrame() + (intptr_t)offset;
    uint8_t P[2] = {};

    for (int y = 0; y < 8; y++)
    {
        if (!(y & 3))
        {
            P[0] = fr.ReadUInt8();
            P[1] = fr.ReadUInt8();
        }

        memset(pBuffer,     P[0], 4);
        memset(pBuffer + 4, P[1], 4);
        pBuffer += videoStride;
    }
}

void InterplayDecoder::DecodeBlock14(int32_t offset)
{
    // 1-color encoding : the whole block is 1 solid color
    uint8_t* pBuffer = GetCurrentFrame() + (intptr_t)offset;
    uint8_t pix = fr.ReadUInt8();

    for (int y = 0; y < 8; y++)
    {
        memset(pBuffer, pix, 8);
        pBuffer += videoStride;
    }
}

void InterplayDecoder::DecodeBlock15(int32_t offset)
{
    // dithered encoding
    uint8_t* pBuffer = GetCurrentFrame() + (intptr_t)offset;
    uint8_t P[2];

    P[0] = fr.ReadUInt8();
    P[1] = fr.ReadUInt8();

    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 8; x += 2)
        {
            *pBuffer++ = P[y & 1];
            *pBuffer++ = P[!(y & 1)];
        }
        pBuffer += (videoStride - 8);
    }
}

uint8_t* InterplayDecoder::GetCurrentFrame()
{
    return pVideoBuffers[nCurrentVideoBuffer];
}

uint8_t* InterplayDecoder::GetPreviousFrame()
{
    return pVideoBuffers[nPreviousVideoBuffer];
}
