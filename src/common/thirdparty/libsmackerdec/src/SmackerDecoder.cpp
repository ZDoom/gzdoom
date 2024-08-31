/*
 * libsmackerdec - Smacker video decoder
 * Copyright (C) 2011 Barry Duncan
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

/* This code is heavily based on smacker.c from the FFmpeg project which can be obtained from http://www.ffmpeg.org/
 * below is the license from smacker.c
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Smacker decoder
 * Copyright (c) 2006 Konstantin Shishkov
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

#include "SmackerDecoder.h"
#include "HuffmanVLC.h"
#include "LogError.h"
#include "printf.h"
#include "limits.h"
#include <assert.h>
#include <algorithm>
#include <memory>

std::vector<class SmackerDecoder*> classInstances;

SmackerHandle Smacker_Open(const char* fileName)
{
	SmackerHandle newHandle;
	newHandle.isValid = false;
	newHandle.instanceIndex = -1;

	SmackerDecoder *newDecoder = new SmackerDecoder();
	if (!newDecoder->Open(fileName))
	{
		delete newDecoder;
		return newHandle;
	}

	// add instance to global instance vector
	classInstances.push_back(newDecoder);

	// get a handle ID
	newHandle.instanceIndex = int(classInstances.size()) - 1;

	// loaded ok, make handle valid
	newHandle.isValid = true;

	return newHandle;
}

void Smacker_Close(SmackerHandle &handle)
{
	if (!classInstances.at(handle.instanceIndex))
	{
		// invalid handle
		return;
	}

	// close bink decoder
	delete classInstances[handle.instanceIndex];
	classInstances[handle.instanceIndex] = 0;

	handle.instanceIndex = -1;
	handle.isValid = false;
}

uint32_t Smacker_GetNumAudioTracks(SmackerHandle &handle)
{
	return classInstances[handle.instanceIndex]->GetNumAudioTracks();
}

SmackerAudioInfo Smacker_GetAudioTrackDetails(SmackerHandle &handle, uint32_t trackIndex)
{
	return classInstances[handle.instanceIndex]->GetAudioTrackDetails(trackIndex);
}

/* Get a frame's worth of audio data. 
 * 
 * 'data' needs to be a pointer to allocated memory that this function will fill.
 * You can find the size (in bytes) to make this buffer by calling Bink_GetAudioTrackDetails()
 * and checking the 'idealBufferSize' member in the returned AudioInfo struct
 */
uint32_t Smacker_GetAudioData(SmackerHandle &handle, uint32_t trackIndex, int16_t *data)
{
	return classInstances[handle.instanceIndex]->GetAudioData(trackIndex, data);
}

/* Disables an audio track if it's enabled.
 *
 * When getting video or audio data, encoded data is stored for other tracks so
 * they can be read and decoded later. This function prevents a build-up of
 * encoded audio data for the specified track if such data isn't going to be
 * decoded and read by the caller.
 */
void Smacker_DisableAudioTrack(SmackerHandle &handle, uint32_t trackIndex)
{
	classInstances[handle.instanceIndex]->DisableAudioTrack(trackIndex);
}

uint32_t Smacker_GetNumFrames(SmackerHandle &handle)
{
	return classInstances[handle.instanceIndex]->GetNumFrames();
}

void Smacker_GetFrameSize(SmackerHandle &handle, uint32_t &width, uint32_t &height)
{
	width  = classInstances[handle.instanceIndex]->frameWidth;
	height = classInstances[handle.instanceIndex]->frameHeight;
}

uint32_t Smacker_GetCurrentFrameNum(SmackerHandle &handle)
{
	return classInstances[handle.instanceIndex]->GetCurrentFrameNum();
}

uint32_t Smacker_GetNextFrame(SmackerHandle &handle)
{
	SmackerDecoder *decoder = classInstances[handle.instanceIndex];

	uint32_t frameIndex = decoder->GetCurrentFrameNum();

	decoder->GetNextFrame();

	return frameIndex;
}

float Smacker_GetFrameRate(SmackerHandle &handle)
{
	return classInstances[handle.instanceIndex]->GetFrameRate();
}

void Smacker_GetPalette(SmackerHandle &handle, uint8_t *palette)
{
	classInstances[handle.instanceIndex]->GetPalette(palette);
}

void Smacker_GetFrame(SmackerHandle &handle, uint8_t *frame)
{
	classInstances[handle.instanceIndex]->GetFrame(frame);
}

void Smacker_GotoFrame(SmackerHandle &handle, uint32_t frameNum)
{
	classInstances[handle.instanceIndex]->GotoFrame(frameNum);
}

SmackerDecoder::SmackerDecoder()
{
	isVer4 = false;
	currentReadFrame = 0;
	currentFrame = 0;
	picture = 0;
	nextPos = 0;

	for (int i = 0; i < kMaxAudioTracks; i++)
	{
		audioTracks[i].buffer = 0;
	}
}

SmackerDecoder::~SmackerDecoder()
{
	for (int i = 0; i < kMaxAudioTracks; i++)
	{
		delete[] audioTracks[i].buffer;
	}

	delete[] picture;
}

// from bswap.h
static /*av_always_inline av_const*/ uint16_t av_bswap16(uint16_t x)
{
    x= (x>>8) | (x<<8);
    return x;
}

/* possible runs of blocks */
static const int block_runs[64] = {
      1,    2,    3,    4,    5,    6,    7,    8,
      9,   10,   11,   12,   13,   14,   15,   16,
     17,   18,   19,   20,   21,   22,   23,   24,
     25,   26,   27,   28,   29,   30,   31,   32,
     33,   34,   35,   36,   37,   38,   39,   40,
     41,   42,   43,   44,   45,   46,   47,   48,
     49,   50,   51,   52,   53,   54,   55,   56,
     57,   58,   59,  128,  256,  512, 1024, 2048 };

enum SmkBlockTypes {
    SMK_BLK_MONO = 0,
    SMK_BLK_FULL = 1,
    SMK_BLK_SKIP = 2,
    SMK_BLK_FILL = 3 };

/* palette used in Smacker */
static const uint8_t smk_pal[64] = {
    0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C,
    0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C,
    0x41, 0x45, 0x49, 0x4D, 0x51, 0x55, 0x59, 0x5D,
    0x61, 0x65, 0x69, 0x6D, 0x71, 0x75, 0x79, 0x7D,
    0x82, 0x86, 0x8A, 0x8E, 0x92, 0x96, 0x9A, 0x9E,
    0xA2, 0xA6, 0xAA, 0xAE, 0xB2, 0xB6, 0xBA, 0xBE,
    0xC3, 0xC7, 0xCB, 0xCF, 0xD3, 0xD7, 0xDB, 0xDF,
    0xE3, 0xE7, 0xEB, 0xEF, 0xF3, 0xF7, 0xFB, 0xFF
};

enum SAudFlags {
    SMK_AUD_PACKED  = 0x80000000, 
    SMK_AUD_PRESENT = 0x40000000,
    SMK_AUD_16BITS  = 0x20000000, 
    SMK_AUD_STEREO  = 0x10000000, 
    SMK_AUD_BINKAUD = 0x08000000, 
    SMK_AUD_USEDCT  = 0x04000000 
};

const int kSMKpal = 0x01;
const int kFlagRingFrame = 0x01;

const int kTreeBits = 9;
const int kSMKnode = 0x80000000;

const char *kSMK2iD = "SMK2";
const char *kSMK4iD = "SMK4";


/**
 * Context used for code reconstructing
 */
typedef struct HuffContext {
    int length = 0;
    int maxlength = 0;
    int current = 0;

	std::vector<uint32_t> bits;
	std::vector<int> lengths;
	std::vector<int> values;

} HuffContext;

/* common parameters used for decode_bigtree */
typedef struct DBCtx {
	SmackerCommon::VLCtable v1;
	SmackerCommon::VLCtable v2;
	std::vector<int> recode1, recode2;
    int escapes[3];
    int *last;
    int lcur;
} DBCtx;


static void last_reset(std::vector<int> &recode, int *last) {
    recode[last[0]] = recode[last[1]] = recode[last[2]] = 0;
}

/* get code and update history */
int SmackerDecoder::GetCode(SmackerCommon::BitReader &bits, std::vector<int> &recode, int *last)
{
	int *table = &recode[0];

    int v, b;

    b = bits.GetPosition();

    while (*table & kSMKnode)
	{
        if (bits.GetBit())
            table += (*table) & (~kSMKnode);
        table++;
    }
    v = *table;
    b = bits.GetPosition() - b;

    if (v != recode[last[0]]) {
        recode[last[2]] = recode[last[1]];
        recode[last[1]] = recode[last[0]];
        recode[last[0]] = v;
    }
    return v;
}

bool SmackerDecoder::Open(const char *fileName)
{
	// open the file (read only)
	file.Open(fileName);
	if (!file.Is_Open())
	{
        Printf("SmackerDecoder::Open() - Can't open file %s\n", fileName);
		return false;
	}

	// check the file signature
	file.ReadBytes((uint8_t*)signature, 4);
	if (memcmp(signature, kSMK2iD, 4) != 0
		&& memcmp(signature, kSMK4iD, 4) != 0)
	{
        Printf("SmackerDecoder::Open() - Unknown Smacker signature\n");
		return false;
	}

	if (!memcmp(signature, kSMK4iD, 4)) {
		isVer4 = true;
	}

	frameWidth  = file.ReadUint32LE();
	frameHeight = file.ReadUint32LE();
	nFrames = file.ReadUint32LE();

	picture = new uint8_t[frameWidth * frameHeight];

	int32_t frameRate = file.ReadUint32LE();

	if (frameRate > 0)
		fps = 1000 / frameRate;
	else if (frameRate < 0)
		fps = 100000 / (-frameRate);
	else
		fps = 10;

	uint32_t flags = file.ReadUint32LE();

	if (flags & kFlagRingFrame) {
		nFrames++;
	}

	for (int i = 0; i < kMaxAudioTracks; i++) {
		audioTracks[i].sizeInBytes = file.ReadUint32LE();
	}

	treeSize = file.ReadUint32LE();
	mMapSize = file.ReadUint32LE();
	MClrSize = file.ReadUint32LE();
	fullSize = file.ReadUint32LE();
	typeSize = file.ReadUint32LE();

	for (int i = 0; i < kMaxAudioTracks; i++) {
		audioTracks[i].flags = file.ReadUint32LE();
	}

	// skip pad
	file.Skip(4);

	if (nFrames > 0xFFFFFF)
	{
        Printf("SmackerDecoder::Open() - Too many frames\n");
		return false;
	}

	frameSizes.resize(nFrames);
	frameFlags.resize(nFrames);

	// read frame info
	for (uint32_t i = 0; i < nFrames; i++) {
		frameSizes[i] = file.ReadUint32LE();
	}
	for (uint32_t i = 0; i < nFrames; i++) {
		frameFlags[i] = file.ReadByte();
	}

	auto maxFrameSize = std::max_element(frameSizes.begin(), frameSizes.end());
	if (maxFrameSize != frameSizes.end())
		packetData.reserve(*maxFrameSize);

	// handle possible audio streams
	for (int i = 0; i < kMaxAudioTracks; i++)
	{
		audioTracks[i].buffer = 0;
		audioTracks[i].bufferSize = 0;
		audioTracks[i].bytesReadThisFrame = 0;

		// Disable non-consecutive enabled tracks. Not sure how to otherwise report
		// them properly for Smacker_GetNumAudioTracks.
		if (i > 0 && !(audioTracks[i-1].flags & SMK_AUD_PRESENT))
			audioTracks[i].flags &= ~SMK_AUD_PRESENT;

		if (audioTracks[i].flags & 0xFFFFFF)
		{
/*
            if (audioTracks[i].flags & SMK_AUD_BINKAUD) {
                audioTracks[i].compressionType = SMK_AUD_BINKAUD;
            } else if (audioTracks[i].flags & SMK_AUD_USEDCT) {
                audioTracks[i].compressionType = SMK_AUD_USEDCT;
            } else if (audioTracks[i].flags & SMK_AUD_PACKED){
                ast[i]->codec->codec_id = CODEC_ID_SMACKAUDIO;
                ast[i]->codec->codec_tag = MKTAG('S', 'M', 'K', 'A');
            } else {
                ast[i]->codec->codec_id = CODEC_ID_PCM_U8;
            }
*/
			audioTracks[i].nChannels = (audioTracks[i].flags & SMK_AUD_STEREO) ? 2 : 1;
			audioTracks[i].sampleRate = audioTracks[i].flags & 0xFFFFFF;
			audioTracks[i].bitsPerSample = (audioTracks[i].flags & SMK_AUD_16BITS) ? 16 : 8;
		}
	}

	memset(palette, 0, 768);
	
	DecodeHeaderTrees();

	// set nextPos to where we are now, as next data is frame 1
	nextPos = file.GetPosition();
    firstFrameFilePos = nextPos;

	// determine max buffer sizes for audio tracks
//	file.Seek(nextPos, SmackerCommon::FileStream::kSeekStart);

	uint8_t frameFlag  = frameFlags[0];

	// skip over palette
	if (frameFlag & kSMKpal)
	{
		uint32_t size = file.ReadByte();
		size = size * 4 - 1;
		file.Skip(size);
	}

	frameFlag >>= 1;

	for (int i = 0; i < kMaxAudioTracks; i++) 
	{
		if (frameFlag & 1) 
		{
			uint32_t size = file.ReadUint32LE();
			uint32_t unpackedSize = file.ReadUint32LE();

			// If the track isn't 16-bit, double the buffer size for converting 8-bit to 16-bit.
			if (!(audioTracks[i].flags & SMK_AUD_16BITS))
				unpackedSize *= 2;

			audioTracks[i].bufferSize = unpackedSize;
			audioTracks[i].buffer = new uint8_t[unpackedSize];

			// skip size
			file.Skip(size - 8);
		}	
		frameFlag >>= 1;
	}

	return true;
}

// static int smacker_decode_tree(GetBitContext *gb, HuffContext *hc, uint32_t prefix, int length)
int SmackerDecoder::DecodeTree(SmackerCommon::BitReader &bits, HuffContext *hc, uint32_t prefix, int length)
{
    if (!bits.GetBit()) // Leaf
	{
        if (hc->current >= 256){
            Printf("SmackerDecoder::DecodeTree() - Tree size exceeded\n");
            return -1;
        }
        if (length){
            hc->bits[hc->current] = prefix;
            hc->lengths[hc->current] = length;
        } else {
            hc->bits[hc->current] = 0;
            hc->lengths[hc->current] = 0;
        }
        hc->values[hc->current] = bits.GetBits(8);

        hc->current++;
        if (hc->maxlength < length)
            hc->maxlength = length;
        return 0;
    } else { //Node
        int r;
        length++;
        r = DecodeTree(bits, hc, prefix, length);
        if (r)
            return r;
        return DecodeTree(bits, hc, prefix | (1 << (length - 1)), length);
    }
}

/**
 * Decode header tree
 */
int SmackerDecoder::DecodeBigTree(SmackerCommon::BitReader &bits, HuffContext *hc, DBCtx *ctx)
{
	if (!bits.GetBit()) // Leaf
	{
		int val, i1, i2, b1, b2;

		i1 = 0;
		i2 = 0;

		if (hc->current >= hc->length){
            Printf("SmackerDecoder::DecodeBigTree() - Tree size exceeded");
            return -1;
        }

        b1 = bits.GetPosition();

		if (VLC_GetSize(ctx->v1))
		{
			i1 = VLC_GetCodeBits(bits, ctx->v1);
		}

        b1 = bits.GetPosition() - b1;
        b2 = bits.GetPosition();

		if (VLC_GetSize(ctx->v2))
		{
			i2 = VLC_GetCodeBits(bits, ctx->v2);
		}

        b2 = bits.GetPosition() - b2;
        if (i1 < 0 || i2 < 0)
            return -1;
        val = ctx->recode1[i1] | (ctx->recode2[i2] << 8);
        if (val == ctx->escapes[0]) {
            ctx->last[0] = hc->current;
            val = 0;
        } else if (val == ctx->escapes[1]) {
            ctx->last[1] = hc->current;
            val = 0;
        } else if (val == ctx->escapes[2]) {
            ctx->last[2] = hc->current;
            val = 0;
        }

        hc->values[hc->current++] = val;
        return 1;
    } else { //Node
        int r = 0, t;

        t = hc->current++;
        r = DecodeBigTree(bits, hc, ctx);
        if (r < 0)
            return r;
        hc->values[t] = kSMKnode | r;
        r++;
        r += DecodeBigTree(bits, hc, ctx);
        return r;
    }
}

/**
 * Store large tree as Libav's vlc codes
 */
int SmackerDecoder::DecodeHeaderTree(SmackerCommon::BitReader &bits, std::vector<int> &recodes, int *last, int size)
{
	HuffContext huff;
	HuffContext tmp1, tmp2;
	int escapes[3];
	DBCtx ctx;

	if ((uint32_t)size >= UINT_MAX>>4)
	{
        Printf("SmackerDecoder::DecodeHeaderTree() - Size too large\n");
		return -1;
	}

	tmp1.length = 256;
	tmp1.maxlength = 0;
	tmp1.current = 0;

	tmp1.bits.resize(256);
	tmp1.lengths.resize(256);
	tmp1.values.resize(256);

	tmp2.length = 256;
	tmp2.maxlength = 0;
	tmp2.current = 0;

	tmp2.bits.resize(256);
	tmp2.lengths.resize(256);
	tmp2.values.resize(256);

	// low byte tree
	if (bits.GetBit()) // 1: Read Tag
	{
		DecodeTree(bits, &tmp1, 0, 0);

		bits.SkipBits(1);

		VLC_InitTable(ctx.v1, tmp1.maxlength, tmp1.current, &tmp1.lengths[0], &tmp1.bits[0]);
	}
	else
	{
		// Skipping low bytes tree
	}

	// high byte tree
	if (bits.GetBit())
	{
		DecodeTree(bits, &tmp2, 0, 0);

		bits.SkipBits(1);

		VLC_InitTable(ctx.v2, tmp2.maxlength, tmp2.current, &tmp2.lengths[0], &tmp2.bits[0]);
	}
	else
	{
		// Skipping high bytes tree
	}

	escapes[0]  = bits.GetBits(8);
	escapes[0] |= bits.GetBits(8) << 8;
	escapes[1]  = bits.GetBits(8);
	escapes[1] |= bits.GetBits(8) << 8;
	escapes[2]  = bits.GetBits(8);
	escapes[2] |= bits.GetBits(8) << 8;

	last[0] = last[1] = last[2] = -1;

	ctx.escapes[0] = escapes[0];
    ctx.escapes[1] = escapes[1];
    ctx.escapes[2] = escapes[2];

    ctx.recode1 = tmp1.values;
    ctx.recode2 = tmp2.values;
    ctx.last = last;

	huff.length = ((size + 3) >> 2) + 3;
    huff.maxlength = 0;
    huff.current = 0;
	huff.values.resize(huff.length);

	DecodeBigTree(bits, &huff, &ctx);

    bits.SkipBits(1);

    if (ctx.last[0] == -1) ctx.last[0] = huff.current++;
    if (ctx.last[1] == -1) ctx.last[1] = huff.current++;
    if (ctx.last[2] == -1) ctx.last[2] = huff.current++;

	recodes = huff.values;

	return 0;
}

// static int decode_header_trees(SmackVContext *smk) {
bool SmackerDecoder::DecodeHeaderTrees()
{
	auto treeData = std::make_unique<uint8_t[]>(treeSize);
	file.ReadBytes(treeData.get(), treeSize);

	SmackerCommon::BitReader bits(treeData.get(), treeSize);

	if (!bits.GetBit())
	{
		// Skipping MMAP tree
		mmap_tbl.resize(2);
		mmap_tbl[0] = 0;
		mmap_last[0] = mmap_last[1] = mmap_last[2] = 1;
	}
	else
	{
		DecodeHeaderTree(bits, mmap_tbl, mmap_last, mMapSize);
	}

	if (!bits.GetBit())
	{
		// Skipping MCLR tree
		mclr_tbl.resize(2);
		mclr_tbl[0] = 0;
		mclr_last[0] = mclr_last[1] = mclr_last[2] = 1;
	}
	else
	{
		DecodeHeaderTree(bits, mclr_tbl, mclr_last, MClrSize);
	}

	if (!bits.GetBit())
	{
		// Skipping FULL tree
		full_tbl.resize(2);
		full_tbl[0] = 0;
		full_last[0] = full_last[1] = full_last[2] = 1;
	}
	else
	{
		DecodeHeaderTree(bits, full_tbl, full_last, fullSize);
	}

	if (!bits.GetBit())
	{
		// Skipping TYPE tree
		type_tbl.resize(2);
		type_tbl[0] = 0;
		type_last[0] = type_last[1] = type_last[2] = 1;
	}
	else
	{
		DecodeHeaderTree(bits, type_tbl, type_last, typeSize);
	}

	return true;
}

void SmackerDecoder::GetNextFrame()
{
	if (currentFrame >= nFrames)
		return;

	std::unique_lock flock(fileMutex);
	while (framePacketData.empty())
	{
		if (ReadPacket() > 0)
			return;
	}
	SmackerPacket pkt = std::move(framePacketData.front());
	framePacketData.pop_front();
	flock.unlock();

	uint32_t frameSize = (uint32_t)pkt.size;
	uint8_t frameFlag  = frameFlags[currentFrame];

	const uint8_t *packetDataPtr = pkt.data.get();

	// handle palette change
	if (frameFlag & kSMKpal)
	{
		int size, sz, t, off, j;
        uint8_t *pal = palette;
        uint8_t oldpal[768];
        const uint8_t *dataEnd;

        memcpy(oldpal, pal, 768);
        size = *(packetDataPtr++);
        size = size * 4 - 1;
        frameSize -= size;
        frameSize--;
        sz = 0;
        dataEnd = packetDataPtr + size;

        while (sz < 256)
		{
            t = *(packetDataPtr++);
            if (t & 0x80){ /* skip palette entries */
                sz  += (t & 0x7F)  + 1;
                pal += ((t & 0x7F) + 1) * 3;
            } else if (t & 0x40){ /* copy with offset */
                off = *(packetDataPtr++) * 3;
                j = (t & 0x3F) + 1;
                while (j-- && sz < 256) {
                    *pal++ = oldpal[off + 0];
                    *pal++ = oldpal[off + 1];
                    *pal++ = oldpal[off + 2];
                    sz++;
                    off += 3;
                }
            } else { /* new entries */
                *pal++ = smk_pal[t];
                *pal++ = smk_pal[*(packetDataPtr++) & 0x3F];
                *pal++ = smk_pal[*(packetDataPtr++) & 0x3F];
                sz++;
            }
        }

		packetDataPtr = dataEnd;
	}

	if (frameSize > 0)
		DecodeFrame(packetDataPtr, frameSize);

	currentFrame++;
}

int SmackerDecoder::ReadPacket()
{
	// test-remove
	if (currentReadFrame >= nFrames)
		return 1;

	// seek to next frame position
	file.Seek(nextPos, SmackerCommon::FileStream::kSeekStart);

	uint32_t frameSize = frameSizes[currentReadFrame] & (~3);
	uint8_t frameFlag  = frameFlags[currentReadFrame];

	packetData.resize(frameSize);
	file.ReadBytes(packetData.data(), frameSize);

	const uint8_t *packetDataPtr = packetData.data();

	// skip pal data for later, after getting audio data
	if (frameFlag & kSMKpal)
	{
		int size = (*packetDataPtr * 4);
		packetDataPtr += size;
		frameSize -= size;
	}

	frameFlag >>= 1;
	for (int i = 0; i < kMaxAudioTracks; i++)
	{
		if (frameFlag & 1)
		{
			uint32_t size = *(packetDataPtr++);
			size |= *(packetDataPtr++) << 8;
			size |= *(packetDataPtr++) << 16;
			size |= *(packetDataPtr++) << 24;
			frameSize -= size;
			size -= 4;

			if (audioTracks[i].flags & SMK_AUD_PRESENT)
			{
				SmackerPacket pkt;
				pkt.size = size;
				pkt.data = std::make_unique<uint8_t[]>(size);
				memcpy(pkt.data.get(), packetDataPtr, size);
				audioTracks[i].packetData.emplace_back(std::move(pkt));
			}
			packetDataPtr += size;
		}
		frameFlag >>= 1;
	}

	SmackerPacket pkt;
	frameFlag = frameFlags[currentReadFrame];
	if (frameSize != 0 || (frameFlag & kSMKpal))
	{
		int palsize = (frameFlag & kSMKpal) ? packetData[0] * 4 : 0;
		int totalSize = frameSize + palsize;
		pkt.size = totalSize;
		pkt.data = std::make_unique<uint8_t[]>(totalSize);
		memcpy(pkt.data.get(), packetData.data(), palsize);
		memcpy(pkt.data.get()+palsize, packetDataPtr, frameSize);
	}
	framePacketData.emplace_back(std::move(pkt));

	++currentReadFrame;

	nextPos = file.GetPosition();

	return 0;
}

int SmackerDecoder::DecodeFrame(const uint8_t *dataPtr, uint32_t frameSize)
{
	last_reset(mmap_tbl, mmap_last);
    last_reset(mclr_tbl, mclr_last);
    last_reset(full_tbl, full_last);
    last_reset(type_tbl, type_last);

	int blocks, blk, bw, bh;
    int i;
    int stride;

	uint8_t *out = picture; // set to output image

	blk = 0;
    bw = frameWidth  >> 2;
    bh = frameHeight >> 2;
    blocks = bw * bh;

	stride = frameWidth;

	SmackerCommon::BitReader bits(dataPtr, frameSize);
	dataPtr += frameSize;

	while (blk < blocks)
	{
		int type, run, mode;
        uint16_t pix;

        type = GetCode(bits, type_tbl, type_last);
        run = block_runs[(type >> 2) & 0x3F];
        switch (type & 3)
		{
        case SMK_BLK_MONO:
            while (run-- && blk < blocks)
			{
                int clr, map;
                int hi, lo;
                clr = GetCode(bits, mclr_tbl, mclr_last);
                map = GetCode(bits, mmap_tbl, mmap_last);

                out = picture + (blk / bw) * (stride * 4) + (blk % bw) * 4;

                hi = clr >> 8;
                lo = clr & 0xFF;
                for (i = 0; i < 4; i++) 
				{
                    if (map & 1) out[0] = hi; else out[0] = lo;
                    if (map & 2) out[1] = hi; else out[1] = lo;
                    if (map & 4) out[2] = hi; else out[2] = lo;
                    if (map & 8) out[3] = hi; else out[3] = lo;
                    map >>= 4;
                    out += stride;
                }
                blk++;
            }
            break;
        case SMK_BLK_FULL:
            mode = 0;
			if (isVer4) // In case of Smacker v4 we have three modes
			{
				if (bits.GetBit()) mode = 1;
				else if (bits.GetBit()) mode = 2;
			}

            while (run-- && blk < blocks)
			{
                out = picture + (blk / bw) * (stride * 4) + (blk % bw) * 4;
                switch (mode)
				{
                case 0:
                    for (i = 0; i < 4; i++)
					{
                        pix = GetCode(bits, full_tbl, full_last);
// FIX                        AV_WL16(out+2, pix);
						out[2] = pix & 0xff;
						out[3] = pix >> 8;

                        pix = GetCode(bits, full_tbl, full_last);
// FIX                        AV_WL16(out, pix);
						out[0] = pix & 0xff;
						out[1] = pix >> 8;
                        out += stride;
                    }
                    break;
                case 1:
                    pix = GetCode(bits, full_tbl, full_last);
                    out[0] = out[1] = pix & 0xFF;
                    out[2] = out[3] = pix >> 8;
                    out += stride;
                    out[0] = out[1] = pix & 0xFF;
                    out[2] = out[3] = pix >> 8;
                    out += stride;
                    pix = GetCode(bits, full_tbl, full_last);
                    out[0] = out[1] = pix & 0xFF;
                    out[2] = out[3] = pix >> 8;
                    out += stride;
                    out[0] = out[1] = pix & 0xFF;
                    out[2] = out[3] = pix >> 8;
                    out += stride;
                    break;
                case 2:
                    for (i = 0; i < 2; i++)
					{
                        uint16_t pix1, pix2;
                        pix2 = GetCode(bits, full_tbl, full_last);
                        pix1 = GetCode(bits, full_tbl, full_last);

// FIX                        AV_WL16(out, pix1);
// FIX                        AV_WL16(out+2, pix2);
						out[0] = pix1 & 0xff;
						out[1] = pix1 >> 8;
						out[2] = pix2 & 0xff;
						out[3] = pix2 >> 8;

                        out += stride;

// FIX                        AV_WL16(out, pix1);
// FIX                        AV_WL16(out+2, pix2);
						out[0] = pix1 & 0xff;
						out[1] = pix1 >> 8;
						out[2] = pix2 & 0xff;
						out[3] = pix2 >> 8;

                        out += stride;
                    }
                    break;
                }
                blk++;
            }
            break;
        case SMK_BLK_SKIP:
            while (run-- && blk < blocks)
                blk++;
            break;
        case SMK_BLK_FILL:
            mode = type >> 8;
            while (run-- && blk < blocks)
			{
                uint32_t col;
                out = picture + (blk / bw) * (stride * 4) + (blk % bw) * 4;
                col = mode * 0x01010101;
                for (i = 0; i < 4; i++) {
                    *((uint32_t*)out) = col;
                    out += stride;
                }
                blk++;
            }
            break;
        }
	}

	return 0;
}

/**
 * Decode Smacker audio data
 */
int SmackerDecoder::DecodeAudio(const uint8_t *dataPtr, uint32_t size, SmackerAudioTrack &track)
{
	SmackerCommon::VLCtable vlc[4];
    int val;
    int i, res;
    int unpackedSize;
    int sampleBits, stereo;
    int pred[2] = {0, 0};

	int16_t *samples = reinterpret_cast<int16_t*>(track.buffer);

	SmackerCommon::BitReader bits(dataPtr, size);

    unpackedSize = bits.GetBits(32);
    if (unpackedSize <= 4) {
        Printf("SmackerDecoder::DecodeAudio() - Packet is too small\n");
		return -1;
    }

    if (!bits.GetBit()) {
		// no sound data
        return 1;
    }

    stereo     = bits.GetBit();
    sampleBits = bits.GetBit();

    if (stereo ^ (track.nChannels != 1)) {
        Printf("SmackerDecoder::DecodeAudio() - Channels mismatch\n");
		return -1;
    }

    HuffContext h[4];

    // Initialize
    for (i = 0; i < (1 << (sampleBits + stereo)); i++) {
        h[i].length = 256;
        h[i].maxlength = 0;
        h[i].current = 0;
        h[i].bits.resize(256);
        h[i].lengths.resize(256);
        h[i].values.resize(256);

        bits.SkipBits(1);
		DecodeTree(bits, &h[i], 0, 0);
        bits.SkipBits(1);

        if (h[i].current > 1) {
			VLC_InitTable(vlc[i], h[i].maxlength, h[i].current, &h[i].lengths[0], &h[i].bits[0]);
        }
    }
    if (sampleBits) { //decode 16-bit data
        for (i = stereo; i >= 0; i--)
            pred[i] = av_bswap16(bits.GetBits(16));
        for (i = 0; i <= stereo; i++)
            *samples++ = pred[i];
        for (; i < unpackedSize / 2; i++) {
            if (i & stereo) {
				if (VLC_GetSize(vlc[2]))
					res = VLC_GetCodeBits(bits, vlc[2]);
                else
                    res = 0;
                val  = h[2].values[res];
				if (VLC_GetSize(vlc[3]))
					res = VLC_GetCodeBits(bits, vlc[3]);
                else
                    res = 0;
                val |= h[3].values[res] << 8;
                pred[1] += (int16_t)val;
                *samples++ = pred[1];
            } else {
				if (VLC_GetSize(vlc[0]))
					res = VLC_GetCodeBits(bits, vlc[0]);
                else
                    res = 0;
                val  = h[0].values[res];
				if (VLC_GetSize(vlc[1]))
					res = VLC_GetCodeBits(bits, vlc[1]);
                else
                    res = 0;
                val |= h[1].values[res] << 8;
                pred[0] += val;
                *samples++ = pred[0];
            }
        }
    } 
	else { //8-bit data
        for (i = stereo; i >= 0; i--)
            pred[i] = bits.GetBits(8);
        for (i = 0; i <= stereo; i++)
            *samples++ = (pred[i]-128) << 8;
        for (; i < unpackedSize; i++) {
            if (i & stereo){
				if (VLC_GetSize(vlc[1]))
					res = VLC_GetCodeBits(bits, vlc[1]);
                else
                    res = 0;
                pred[1] += (int8_t)h[1].values[res];
                *samples++ = (pred[1]-128) << 8;
            } else {
				if (VLC_GetSize(vlc[0]))
				res = VLC_GetCodeBits(bits, vlc[0]);
                else
                    res = 0;
                pred[0] += (int8_t)h[0].values[res];
                *samples++ = (pred[0]-128) << 8;
            }
        }
        unpackedSize *= 2;
    }

	track.bytesReadThisFrame = unpackedSize;

	return 0;
}

void SmackerDecoder::GetPalette(uint8_t *palette)
{
	memcpy(palette, this->palette, 768);
}

void SmackerDecoder::GetFrame(uint8_t *frame)
{
	memcpy(frame, this->picture, frameWidth * frameHeight);
}

void SmackerDecoder::GetFrameSize(uint32_t &width, uint32_t &height)
{
	width  = this->frameWidth;
	height = this->frameHeight;
}

uint32_t SmackerDecoder::GetNumFrames()
{
	return nFrames;
}

uint32_t SmackerDecoder::GetCurrentFrameNum()
{
	return currentFrame;
}

float SmackerDecoder::GetFrameRate()
{
	return (float)fps;
}

void SmackerDecoder::GotoFrame(uint32_t frameNum)
{
	if (frameNum > nFrames) {
        Printf("SmackerDecoder::GotoFrame() - Invalid frame number\n");
		return;
	}

//    file.Seek(firstFrameFilePos, SmackerCommon::FileStream::kSeekStart);

    currentReadFrame = 0;
    currentFrame = 0;
    nextPos = firstFrameFilePos;

    framePacketData.clear();
    for (unsigned j = 0; j < kMaxAudioTracks; j++)
        audioTracks[j].packetData.clear();

    for (unsigned i = 0; i < frameNum; i++)
    {
        GetNextFrame();
        for (unsigned j = 0; j < kMaxAudioTracks; j++)
            audioTracks[j].packetData.clear();
    }

    GetNextFrame();
}

uint32_t SmackerDecoder::GetNumAudioTracks()
{
	for(uint32_t i = 0;i < kMaxAudioTracks;++i)
	{
		if (!(audioTracks[i].flags & SMK_AUD_PRESENT))
			return i;
	}
	return kMaxAudioTracks;
}

SmackerAudioInfo SmackerDecoder::GetAudioTrackDetails(uint32_t trackIndex)
{
	SmackerAudioInfo info;
	SmackerAudioTrack *track = &audioTracks[trackIndex];

	info.sampleRate = track->sampleRate;
	info.nChannels  = track->nChannels;

	// audio buffer size in bytes
	info.idealBufferSize = track->bufferSize;

	return info;
}

uint32_t SmackerDecoder::GetAudioData(uint32_t trackIndex, int16_t *audioBuffer)
{
	if (!audioBuffer) {
		return 0;
	}

	SmackerAudioTrack *track = &audioTracks[trackIndex];

	std::unique_lock flock(fileMutex);
	if (!(track->flags & SMK_AUD_PRESENT))
		return 0;

	while (track->packetData.empty())
	{
		if (ReadPacket() > 0)
			return 0;
	}
	SmackerPacket pkt = std::move(track->packetData.front());
	track->packetData.pop_front();
	flock.unlock();

	track->bytesReadThisFrame = 0;

	const uint8_t *packetDataPtr = pkt.data.get();
	uint32_t size = (uint32_t)pkt.size;

	DecodeAudio(packetDataPtr, size, *track);

	if (track->bytesReadThisFrame) {
		memcpy(audioBuffer, track->buffer, std::min(track->bufferSize, track->bytesReadThisFrame));
	}

	return track->bytesReadThisFrame;
}

void SmackerDecoder::DisableAudioTrack(uint32_t trackIndex)
{
	SmackerAudioTrack *track = &audioTracks[trackIndex];
	std::unique_lock flock(fileMutex);
	track->flags &= ~SMK_AUD_PRESENT;
	track->packetData.clear();
}
