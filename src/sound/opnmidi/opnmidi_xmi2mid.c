/*
 * XMIDI: Miles XMIDI to MID Library
 *
 * Copyright (C) 2001  Ryan Nunn
 * Copyright (C) 2014  Bret Curtis
 * Copyright (C) WildMIDI Developers 2015-2016
 * Copyright (c) 2017-2018 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

/* XMIDI Converter */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "opnmidi_xmi2mid.h"

/* Midi Status Bytes */
#define MIDI_STATUS_NOTE_OFF    0x8
#define MIDI_STATUS_NOTE_ON     0x9
#define MIDI_STATUS_AFTERTOUCH  0xA
#define MIDI_STATUS_CONTROLLER  0xB
#define MIDI_STATUS_PROG_CHANGE 0xC
#define MIDI_STATUS_PRESSURE    0xD
#define MIDI_STATUS_PITCH_WHEEL 0xE
#define MIDI_STATUS_SYSEX       0xF

typedef struct _midi_event {
    int32_t time;
    uint8_t status;
    uint8_t data[2];
    uint32_t len;
    uint8_t *buffer;
    struct _midi_event *next;
} midi_event;

typedef struct {
    uint16_t type;
    uint16_t tracks;
} midi_descriptor;

struct xmi_ctx {
    uint8_t *src, *src_ptr;
    uint32_t srcsize;
    uint32_t datastart;
    uint8_t *dst, *dst_ptr;
    uint32_t dstsize, dstrem;
    uint32_t convert_type;
    midi_descriptor info;
    int bank127[16];
    midi_event **events;
    signed short *timing;
    midi_event *list;
    midi_event *current;
};

/* forward declarations of private functions */
static void DeleteEventList(midi_event *mlist);
static void CreateNewEvent(struct xmi_ctx *ctx, int32_t time); /* List manipulation */
static int GetVLQ(struct xmi_ctx *ctx, uint32_t *quant); /* Variable length quantity */
static int GetVLQ2(struct xmi_ctx *ctx, uint32_t *quant);/* Variable length quantity */
static int PutVLQ(struct xmi_ctx *ctx, uint32_t value);  /* Variable length quantity */
static int ConvertEvent(struct xmi_ctx *ctx,
            const int32_t time, const uint8_t status, const int size);
static int32_t ConvertSystemMessage(struct xmi_ctx *ctx,
                const int32_t time, const uint8_t status);
static int32_t ConvertFiletoList(struct xmi_ctx *ctx);
static uint32_t ConvertListToMTrk(struct xmi_ctx *ctx, midi_event *mlist);
static int ParseXMI(struct xmi_ctx *ctx);
static int ExtractTracks(struct xmi_ctx *ctx);
static uint32_t ExtractTracksFromXmi(struct xmi_ctx *ctx);

static uint32_t read1(struct xmi_ctx *ctx)
{
    uint8_t b0;
    b0 = *ctx->src_ptr++;
    return (b0);
}

static uint32_t read2(struct xmi_ctx *ctx)
{
    uint8_t b0, b1;
    b0 = *ctx->src_ptr++;
    b1 = *ctx->src_ptr++;
    return (b0 + ((uint32_t)b1 << 8));
}

static uint32_t read4(struct xmi_ctx *ctx)
{
    uint8_t b0, b1, b2, b3;
    b3 = *ctx->src_ptr++;
    b2 = *ctx->src_ptr++;
    b1 = *ctx->src_ptr++;
    b0 = *ctx->src_ptr++;
    return (b0 + ((uint32_t)b1<<8) + ((uint32_t)b2<<16) + ((uint32_t)b3<<24));
}

static void copy(struct xmi_ctx *ctx, char *b, uint32_t len)
{
    memcpy(b, ctx->src_ptr, len);
    ctx->src_ptr += len;
}

#define DST_CHUNK 8192
static void resize_dst(struct xmi_ctx *ctx) {
    uint32_t pos = (uint32_t)(ctx->dst_ptr - ctx->dst);
    ctx->dst = realloc(ctx->dst, ctx->dstsize + DST_CHUNK);
    ctx->dstsize += DST_CHUNK;
    ctx->dstrem += DST_CHUNK;
    ctx->dst_ptr = ctx->dst + pos;
}

static void write1(struct xmi_ctx *ctx, uint32_t val)
{
    if (ctx->dstrem < 1)
        resize_dst(ctx);
    *ctx->dst_ptr++ = val & 0xff;
    ctx->dstrem--;
}

static void write2(struct xmi_ctx *ctx, uint32_t val)
{
    if (ctx->dstrem < 2)
        resize_dst(ctx);
    *ctx->dst_ptr++ = (val>>8) & 0xff;
    *ctx->dst_ptr++ = val & 0xff;
    ctx->dstrem -= 2;
}

static void write4(struct xmi_ctx *ctx, uint32_t val)
{
    if (ctx->dstrem < 4)
        resize_dst(ctx);
    *ctx->dst_ptr++ = (val>>24)&0xff;
    *ctx->dst_ptr++ = (val>>16)&0xff;
    *ctx->dst_ptr++ = (val>>8) & 0xff;
    *ctx->dst_ptr++ = val & 0xff;
    ctx->dstrem -= 4;
}

static void seeksrc(struct xmi_ctx *ctx, uint32_t pos) {
    ctx->src_ptr = ctx->src + pos;
}

static void seekdst(struct xmi_ctx *ctx, uint32_t pos) {
    ctx->dst_ptr = ctx->dst + pos;
    while (ctx->dstsize < pos)
        resize_dst(ctx);
    ctx->dstrem = ctx->dstsize - pos;
}

static void skipsrc(struct xmi_ctx *ctx, int32_t pos) {
    ctx->src_ptr += pos;
}

static void skipdst(struct xmi_ctx *ctx, int32_t pos) {
    size_t newpos;
    ctx->dst_ptr += pos;
    newpos = ctx->dst_ptr - ctx->dst;
    while (ctx->dstsize < newpos)
        resize_dst(ctx);
    ctx->dstrem = (uint32_t)(ctx->dstsize - newpos);
}

static uint32_t getsrcsize(struct xmi_ctx *ctx) {
    return (ctx->srcsize);
}

static uint32_t getsrcpos(struct xmi_ctx *ctx) {
    return (uint32_t)(ctx->src_ptr - ctx->src);
}

static uint32_t getdstpos(struct xmi_ctx *ctx) {
    return (uint32_t)(ctx->dst_ptr - ctx->dst);
}

/* This is a default set of patches to convert from MT32 to GM
 * The index is the MT32 Patch number and the value is the GM Patch
 * This is only suitable for music that doesn't do timbre changes
 * XMIDIs that contain Timbre changes will not convert properly.
 */
static const char mt32asgm[128] = {
    0,  /* 0   Piano 1 */
    1,  /* 1   Piano 2 */
    2,  /* 2   Piano 3 (synth) */
    4,  /* 3   EPiano 1 */
    4,  /* 4   EPiano 2 */
    5,  /* 5   EPiano 3 */
    5,  /* 6   EPiano 4 */
    3,  /* 7   Honkytonk */
    16, /* 8   Organ 1 */
    17, /* 9   Organ 2 */
    18, /* 10  Organ 3 */
    16, /* 11  Organ 4 */
    19, /* 12  Pipe Organ 1 */
    19, /* 13  Pipe Organ 2 */
    19, /* 14  Pipe Organ 3 */
    21, /* 15  Accordion */
    6,  /* 16  Harpsichord 1 */
    6,  /* 17  Harpsichord 2 */
    6,  /* 18  Harpsichord 3 */
    7,  /* 19  Clavinet 1 */
    7,  /* 20  Clavinet 2 */
    7,  /* 21  Clavinet 3 */
    8,  /* 22  Celesta 1 */
    8,  /* 23  Celesta 2 */
    62, /* 24  Synthbrass 1 (62) */
    63, /* 25  Synthbrass 2 (63) */
    62, /* 26  Synthbrass 3 Bank 8 */
    63, /* 27  Synthbrass 4 Bank 8 */
    38, /* 28  Synthbass 1 */
    39, /* 29  Synthbass 2 */
    38, /* 30  Synthbass 3 Bank 8 */
    39, /* 31  Synthbass 4 Bank 8 */
    88, /* 32  Fantasy */
    90, /* 33  Harmonic Pan - No equiv closest is polysynth(90) :( */
    52, /* 34  Choral ?? Currently set to SynthVox(54). Should it be ChoirAhhs(52)??? */
    92, /* 35  Glass */
    97, /* 36  Soundtrack */
    99, /* 37  Atmosphere */
    14, /* 38  Warmbell, sounds kind of like crystal(98) perhaps Tubular Bells(14) would be better. It is! */
    54, /* 39  FunnyVox, sounds alot like Bagpipe(109) and Shania(111) */
    98, /* 40  EchoBell, no real equiv, sounds like Crystal(98) */
    96, /* 41  IceRain */
    68, /* 42  Oboe 2001, no equiv, just patching it to normal oboe(68) */
    95, /* 43  EchoPans, no equiv, setting to SweepPad */
    81, /* 44  DoctorSolo Bank 8 */
    87, /* 45  SchoolDaze, no real equiv */
    112,/* 46  Bell Singer */
    80, /* 47  SquareWave */
    48, /* 48  Strings 1 */
    48, /* 49  Strings 2 - should be 49 */
    44, /* 50  Strings 3 (Synth) - Experimental set to Tremollo Strings - should be 50 */
    45, /* 51  Pizzicato Strings */
    40, /* 52  Violin 1 */
    40, /* 53  Violin 2 ? Viola */
    42, /* 54  Cello 1 */
    42, /* 55  Cello 2 */
    43, /* 56  Contrabass */
    46, /* 57  Harp 1 */
    46, /* 58  Harp 2 */
    24, /* 59  Guitar 1 (Nylon) */
    25, /* 60  Guitar 2 (Steel) */
    26, /* 61  Elec Guitar 1 */
    27, /* 62  Elec Guitar 2 */
    104,/* 63  Sitar */
    32, /* 64  Acou Bass 1 */
    32, /* 65  Acou Bass 2 */
    33, /* 66  Elec Bass 1 */
    34, /* 67  Elec Bass 2 */
    36, /* 68  Slap Bass 1 */
    37, /* 69  Slap Bass 2 */
    35, /* 70  Fretless Bass 1 */
    35, /* 71  Fretless Bass 2 */
    73, /* 72  Flute 1 */
    73, /* 73  Flute 2 */
    72, /* 74  Piccolo 1 */
    72, /* 75  Piccolo 2 */
    74, /* 76  Recorder */
    75, /* 77  Pan Pipes */
    64, /* 78  Sax 1 */
    65, /* 79  Sax 2 */
    66, /* 80  Sax 3 */
    67, /* 81  Sax 4 */
    71, /* 82  Clarinet 1 */
    71, /* 83  Clarinet 2 */
    68, /* 84  Oboe */
    69, /* 85  English Horn (Cor Anglais) */
    70, /* 86  Bassoon */
    22, /* 87  Harmonica */
    56, /* 88  Trumpet 1 */
    56, /* 89  Trumpet 2 */
    57, /* 90  Trombone 1 */
    57, /* 91  Trombone 2 */
    60, /* 92  French Horn 1 */
    60, /* 93  French Horn 2 */
    58, /* 94  Tuba */
    61, /* 95  Brass Section 1 */
    61, /* 96  Brass Section 2 */
    11, /* 97  Vibes 1 */
    11, /* 98  Vibes 2 */
    99, /* 99  Syn Mallet Bank 1 */
    112,/* 100 WindBell no real equiv Set to TinkleBell(112) */
    9,  /* 101 Glockenspiel */
    14, /* 102 Tubular Bells */
    13, /* 103 Xylophone */
    12, /* 104 Marimba */
    107,/* 105 Koto */
    111,/* 106 Sho?? set to Shanai(111) */
    77, /* 107 Shakauhachi */
    78, /* 108 Whistle 1 */
    78, /* 109 Whistle 2 */
    76, /* 110 Bottle Blow */
    76, /* 111 Breathpipe no real equiv set to bottle blow(76) */
    47, /* 112 Timpani */
    117,/* 113 Melodic Tom */
    116,/* 114 Deap Snare no equiv, set to Taiko(116) */
    118,/* 115 Electric Perc 1 */
    118,/* 116 Electric Perc 2 */
    116,/* 117 Taiko */
    115,/* 118 Taiko Rim, no real equiv, set to Woodblock(115) */
    119,/* 119 Cymbal, no real equiv, set to reverse cymbal(119) */
    115,/* 120 Castanets, no real equiv, in GM set to Woodblock(115) */
    112,/* 121 Triangle, no real equiv, set to TinkleBell(112) */
    55, /* 122 Orchestral Hit */
    124,/* 123 Telephone */
    123,/* 124 BirdTweet */
    94, /* 125 Big Notes Pad no equiv, set to halo pad (94) */
    98, /* 126 Water Bell set to Crystal Pad(98) */
    121 /* 127 Jungle Tune set to Breath Noise */
};

/* Same as above, except include patch changes
 * so GS instruments can be used */
static const char mt32asgs[256] = {
    0, 0,   /* 0   Piano 1 */
    1, 0,   /* 1   Piano 2 */
    2, 0,   /* 2   Piano 3 (synth) */
    4, 0,   /* 3   EPiano 1 */
    4, 0,   /* 4   EPiano 2 */
    5, 0,   /* 5   EPiano 3 */
    5, 0,   /* 6   EPiano 4 */
    3, 0,   /* 7   Honkytonk */
    16, 0,  /* 8   Organ 1 */
    17, 0,  /* 9   Organ 2 */
    18, 0,  /* 10  Organ 3 */
    16, 0,  /* 11  Organ 4 */
    19, 0,  /* 12  Pipe Organ 1 */
    19, 0,  /* 13  Pipe Organ 2 */
    19, 0,  /* 14  Pipe Organ 3 */
    21, 0,  /* 15  Accordion */
    6, 0,   /* 16  Harpsichord 1 */
    6, 0,   /* 17  Harpsichord 2 */
    6, 0,   /* 18  Harpsichord 3 */
    7, 0,   /* 19  Clavinet 1 */
    7, 0,   /* 20  Clavinet 2 */
    7, 0,   /* 21  Clavinet 3 */
    8, 0,   /* 22  Celesta 1 */
    8, 0,   /* 23  Celesta 2 */
    62, 0,  /* 24  Synthbrass 1 (62) */
    63, 0,  /* 25  Synthbrass 2 (63) */
    62, 0,  /* 26  Synthbrass 3 Bank 8 */
    63, 0,  /* 27  Synthbrass 4 Bank 8 */
    38, 0,  /* 28  Synthbass 1 */
    39, 0,  /* 29  Synthbass 2 */
    38, 0,  /* 30  Synthbass 3 Bank 8 */
    39, 0,  /* 31  Synthbass 4 Bank 8 */
    88, 0,  /* 32  Fantasy */
    90, 0,  /* 33  Harmonic Pan - No equiv closest is polysynth(90) :( */
    52, 0,  /* 34  Choral ?? Currently set to SynthVox(54). Should it be ChoirAhhs(52)??? */
    92, 0,  /* 35  Glass */
    97, 0,  /* 36  Soundtrack */
    99, 0,  /* 37  Atmosphere */
    14, 0,  /* 38  Warmbell, sounds kind of like crystal(98) perhaps Tubular Bells(14) would be better. It is! */
    54, 0,  /* 39  FunnyVox, sounds alot like Bagpipe(109) and Shania(111) */
    98, 0,  /* 40  EchoBell, no real equiv, sounds like Crystal(98) */
    96, 0,  /* 41  IceRain */
    68, 0,  /* 42  Oboe 2001, no equiv, just patching it to normal oboe(68) */
    95, 0,  /* 43  EchoPans, no equiv, setting to SweepPad */
    81, 0,  /* 44  DoctorSolo Bank 8 */
    87, 0,  /* 45  SchoolDaze, no real equiv */
    112, 0, /* 46  Bell Singer */
    80, 0,  /* 47  SquareWave */
    48, 0,  /* 48  Strings 1 */
    48, 0,  /* 49  Strings 2 - should be 49 */
    44, 0,  /* 50  Strings 3 (Synth) - Experimental set to Tremollo Strings - should be 50 */
    45, 0,  /* 51  Pizzicato Strings */
    40, 0,  /* 52  Violin 1 */
    40, 0,  /* 53  Violin 2 ? Viola */
    42, 0,  /* 54  Cello 1 */
    42, 0,  /* 55  Cello 2 */
    43, 0,  /* 56  Contrabass */
    46, 0,  /* 57  Harp 1 */
    46, 0,  /* 58  Harp 2 */
    24, 0,  /* 59  Guitar 1 (Nylon) */
    25, 0,  /* 60  Guitar 2 (Steel) */
    26, 0,  /* 61  Elec Guitar 1 */
    27, 0,  /* 62  Elec Guitar 2 */
    104, 0, /* 63  Sitar */
    32, 0,  /* 64  Acou Bass 1 */
    32, 0,  /* 65  Acou Bass 2 */
    33, 0,  /* 66  Elec Bass 1 */
    34, 0,  /* 67  Elec Bass 2 */
    36, 0,  /* 68  Slap Bass 1 */
    37, 0,  /* 69  Slap Bass 2 */
    35, 0,  /* 70  Fretless Bass 1 */
    35, 0,  /* 71  Fretless Bass 2 */
    73, 0,  /* 72  Flute 1 */
    73, 0,  /* 73  Flute 2 */
    72, 0,  /* 74  Piccolo 1 */
    72, 0,  /* 75  Piccolo 2 */
    74, 0,  /* 76  Recorder */
    75, 0,  /* 77  Pan Pipes */
    64, 0,  /* 78  Sax 1 */
    65, 0,  /* 79  Sax 2 */
    66, 0,  /* 80  Sax 3 */
    67, 0,  /* 81  Sax 4 */
    71, 0,  /* 82  Clarinet 1 */
    71, 0,  /* 83  Clarinet 2 */
    68, 0,  /* 84  Oboe */
    69, 0,  /* 85  English Horn (Cor Anglais) */
    70, 0,  /* 86  Bassoon */
    22, 0,  /* 87  Harmonica */
    56, 0,  /* 88  Trumpet 1 */
    56, 0,  /* 89  Trumpet 2 */
    57, 0,  /* 90  Trombone 1 */
    57, 0,  /* 91  Trombone 2 */
    60, 0,  /* 92  French Horn 1 */
    60, 0,  /* 93  French Horn 2 */
    58, 0,  /* 94  Tuba */
    61, 0,  /* 95  Brass Section 1 */
    61, 0,  /* 96  Brass Section 2 */
    11, 0,  /* 97  Vibes 1 */
    11, 0,  /* 98  Vibes 2 */
    99, 0,  /* 99  Syn Mallet Bank 1 */
    112, 0, /* 100 WindBell no real equiv Set to TinkleBell(112) */
    9, 0,   /* 101 Glockenspiel */
    14, 0,  /* 102 Tubular Bells */
    13, 0,  /* 103 Xylophone */
    12, 0,  /* 104 Marimba */
    107, 0, /* 105 Koto */
    111, 0, /* 106 Sho?? set to Shanai(111) */
    77, 0,  /* 107 Shakauhachi */
    78, 0,  /* 108 Whistle 1 */
    78, 0,  /* 109 Whistle 2 */
    76, 0,  /* 110 Bottle Blow */
    76, 0,  /* 111 Breathpipe no real equiv set to bottle blow(76) */
    47, 0,  /* 112 Timpani */
    117, 0, /* 113 Melodic Tom */
    116, 0, /* 114 Deap Snare no equiv, set to Taiko(116) */
    118, 0, /* 115 Electric Perc 1 */
    118, 0, /* 116 Electric Perc 2 */
    116, 0, /* 117 Taiko */
    115, 0, /* 118 Taiko Rim, no real equiv, set to Woodblock(115) */
    119, 0, /* 119 Cymbal, no real equiv, set to reverse cymbal(119) */
    115, 0, /* 120 Castanets, no real equiv, in GM set to Woodblock(115) */
    112, 0, /* 121 Triangle, no real equiv, set to TinkleBell(112) */
    55, 0,  /* 122 Orchestral Hit */
    124, 0, /* 123 Telephone */
    123, 0, /* 124 BirdTweet */
    94, 0,  /* 125 Big Notes Pad no equiv, set to halo pad (94) */
    98, 0,  /* 126 Water Bell set to Crystal Pad(98) */
    121, 0  /* 127 Jungle Tune set to Breath Noise */
};

int OpnMidi_xmi2midi(uint8_t *in, uint32_t insize,
                 uint8_t **out, uint32_t *outsize,
                 uint32_t convert_type) {
    struct xmi_ctx ctx;
    unsigned int i;
    int ret = -1;

    if (convert_type > XMIDI_CONVERT_MT32_TO_GS) {
        /*_WM_ERROR_NEW("%s:%i:  %d is an invalid conversion type.", __FUNCTION__, __LINE__, convert_type);*/
        return (ret);
    }

    memset(&ctx, 0, sizeof(struct xmi_ctx));
    ctx.src = ctx.src_ptr = in;
    ctx.srcsize = insize;
    ctx.convert_type = convert_type;

    if (ParseXMI(&ctx) < 0) {
        /*_WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_XMI, NULL, 0);*/
        goto _end;
    }

    if (ExtractTracks(&ctx) < 0) {
        /*_WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_MIDI, NULL, 0);*/
        goto _end;
    }

    ctx.dst = malloc(DST_CHUNK);
    ctx.dst_ptr = ctx.dst;
    ctx.dstsize = DST_CHUNK;
    ctx.dstrem = DST_CHUNK;

    /* Header is 14 bytes long and add the rest as well */
    write1(&ctx, 'M');
    write1(&ctx, 'T');
    write1(&ctx, 'h');
    write1(&ctx, 'd');

    write4(&ctx, 6);

    write2(&ctx, ctx.info.type);
    write2(&ctx, ctx.info.tracks);
    write2(&ctx, ctx.timing[0]);/* write divisions from track0 */

    for (i = 0; i < ctx.info.tracks; i++)
        ConvertListToMTrk(&ctx, ctx.events[i]);
    *out = ctx.dst;
    *outsize = ctx.dstsize - ctx.dstrem;
    ret = 0;

_end:   /* cleanup */
    if (ret < 0) {
        free(ctx.dst);
        *out = NULL;
        *outsize = 0;
    }
    if (ctx.events) {
        for (i = 0; i < ctx.info.tracks; i++)
            DeleteEventList(ctx.events[i]);
        free(ctx.events);
    }
    free(ctx.timing);

    return (ret);
}

static void DeleteEventList(midi_event *mlist) {
    midi_event *event;
    midi_event *next;

    next = mlist;

    while ((event = next) != NULL) {
        next = event->next;
        free(event->buffer);
        free(event);
    }
}

/* Sets current to the new event and updates list */
static void CreateNewEvent(struct xmi_ctx *ctx, int32_t time) {
    if (!ctx->list) {
        ctx->list = ctx->current = calloc(1, sizeof(midi_event));
        ctx->current->time = (time < 0)? 0 : time;
        return;
    }

    if (time < 0) {
        midi_event *event = calloc(1, sizeof(midi_event));
        event->next = ctx->list;
        ctx->list = ctx->current = event;
        return;
    }

    if (ctx->current->time > time)
        ctx->current = ctx->list;

    while (ctx->current->next) {
        if (ctx->current->next->time > time) {
            midi_event *event = calloc(1, sizeof(midi_event));
            event->next = ctx->current->next;
            ctx->current->next = event;
            ctx->current = event;
            ctx->current->time = time;
            return;
        }

        ctx->current = ctx->current->next;
    }

    ctx->current->next = calloc(1, sizeof(midi_event));
    ctx->current = ctx->current->next;
    ctx->current->time = time;
}

/* Conventional Variable Length Quantity */
static int GetVLQ(struct xmi_ctx *ctx, uint32_t *quant) {
    int i;
    uint32_t data;

    *quant = 0;
    for (i = 0; i < 4; i++) {
        data = read1(ctx);
        *quant <<= 7;
        *quant |= data & 0x7F;

        if (!(data & 0x80)) {
            i++;
            break;
        }
    }
    return (i);
}

/* XMIDI Delta Variable Length Quantity */
static int GetVLQ2(struct xmi_ctx *ctx, uint32_t *quant) {
    int i;
    int32_t data;

    *quant = 0;
    for (i = 0; i < 4; i++) {
        data = read1(ctx);
        if (data & 0x80) {
            skipsrc(ctx, -1);
            break;
        }
        *quant += data;
    }
    return (i);
}

static int PutVLQ(struct xmi_ctx *ctx, uint32_t value) {
    int32_t buffer;
    int i = 1, j;
    buffer = value & 0x7F;
    while (value >>= 7) {
        buffer <<= 8;
        buffer |= ((value & 0x7F) | 0x80);
        i++;
    }
    for (j = 0; j < i; j++) {
        write1(ctx, buffer & 0xFF);
        buffer >>= 8;
    }

    return (i);
}

/* Converts Events
 *
 * Source is at the first data byte
 * size 1 is single data byte
 * size 2 is dual data byte
 * size 3 is XMI Note on
 * Returns bytes converted  */
static int ConvertEvent(struct xmi_ctx *ctx, const int32_t time,
                        const uint8_t status, const int size) {
    uint32_t delta = 0;
    int32_t data;
    midi_event *prev;
    int i;

    data = read1(ctx);

    /*HACK!*/
    if (((status >> 4) == 0xB) && (status & 0xF) != 9 && (data == 114)) {
        data = 32; /*Change XMI 114 controller into XG bank*/
    }

    /* Bank changes are handled here */
    if ((status >> 4) == 0xB && data == 0) {
        data = read1(ctx);

        ctx->bank127[status & 0xF] = 0;

        if ( ctx->convert_type == XMIDI_CONVERT_MT32_TO_GM ||
             ctx->convert_type == XMIDI_CONVERT_MT32_TO_GS ||
             ctx->convert_type == XMIDI_CONVERT_MT32_TO_GS127 ||
            (ctx->convert_type == XMIDI_CONVERT_MT32_TO_GS127DRUM
                    && (status & 0xF) == 9) )
            return (2);

        CreateNewEvent(ctx, time);
        ctx->current->status = status;
        ctx->current->data[0] = 0;
        ctx->current->data[1] = data == 127 ? 0 : data;/*HACK:*/

        if (ctx->convert_type == XMIDI_CONVERT_GS127_TO_GS && data == 127)
            ctx->bank127[status & 0xF] = 1;

        return (2);
    }

    /* Handling for patch change mt32 conversion, probably should go elsewhere */
    if ((status >> 4) == 0xC && (status&0xF) != 9
            && ctx->convert_type != XMIDI_CONVERT_NOCONVERSION)
    {
        if (ctx->convert_type == XMIDI_CONVERT_MT32_TO_GM)
        {
            data = mt32asgm[data];
        }
        else if ((ctx->convert_type == XMIDI_CONVERT_GS127_TO_GS && ctx->bank127[status&0xF]) ||
            ctx->convert_type == XMIDI_CONVERT_MT32_TO_GS ||
            ctx->convert_type == XMIDI_CONVERT_MT32_TO_GS127DRUM)
        {
            CreateNewEvent (ctx, time);
            ctx->current->status = 0xB0 | (status&0xF);
            ctx->current->data[0] = 0;
            ctx->current->data[1] = mt32asgs[data*2+1];

            data = mt32asgs[data*2];
        }
        else if (ctx->convert_type == XMIDI_CONVERT_MT32_TO_GS127)
        {
            CreateNewEvent (ctx, time);
            ctx->current->status = 0xB0 | (status&0xF);
            ctx->current->data[0] = 0;
            ctx->current->data[1] = 127;
        }
    }
    /* Drum track handling */
    else if ((status >> 4) == 0xC && (status&0xF) == 9 &&
        (ctx->convert_type == XMIDI_CONVERT_MT32_TO_GS127DRUM || ctx->convert_type == XMIDI_CONVERT_MT32_TO_GS127))
    {
        CreateNewEvent (ctx, time);
        ctx->current->status = 0xB9;
        ctx->current->data[0] = 0;
        ctx->current->data[1] = 127;
    }

    CreateNewEvent(ctx, time);
    ctx->current->status = status;

    ctx->current->data[0] = data;

    if (size == 1)
        return (1);

    ctx->current->data[1] = read1(ctx);

    if (size == 2)
        return (2);

    /* XMI Note On handling */
    prev = ctx->current;
    i = GetVLQ(ctx, &delta);
    CreateNewEvent(ctx, time + delta * 3);

    ctx->current->status = status;
    ctx->current->data[0] = data;
    ctx->current->data[1] = 0;
    ctx->current = prev;

    return (i + 2);
}

/* Simple routine to convert system messages */
static int32_t ConvertSystemMessage(struct xmi_ctx *ctx, const int32_t time,
                                    const uint8_t status) {
    int32_t i = 0;

    CreateNewEvent(ctx, time);
    ctx->current->status = status;

    /* Handling of Meta events */
    if (status == 0xFF) {
        ctx->current->data[0] = read1(ctx);
        i++;
    }

    i += GetVLQ(ctx, &ctx->current->len);

    if (!ctx->current->len)
        return (i);

    ctx->current->buffer = malloc(sizeof(uint8_t)*ctx->current->len);
    copy(ctx, (char *) ctx->current->buffer, ctx->current->len);

    return (i + ctx->current->len);
}

/* XMIDI and Midi to List
 * Returns XMIDI PPQN   */
static int32_t ConvertFiletoList(struct xmi_ctx *ctx) {
    int32_t time = 0;
    uint32_t data;
    int32_t end = 0;
    int32_t tempo = 500000;
    int32_t tempo_set = 0;
    uint32_t status = 0;
    uint32_t file_size = getsrcsize(ctx);

    /* Set Drum track to correct setting if required */
    if (ctx->convert_type == XMIDI_CONVERT_MT32_TO_GS127) {
        CreateNewEvent(ctx, 0);
        ctx->current->status = 0xB9;
        ctx->current->data[0] = 0;
        ctx->current->data[1] = 127;
    }

    while (!end && getsrcpos(ctx) < file_size) {
        GetVLQ2(ctx, &data);
        time += data * 3;

        status = read1(ctx);

        switch (status >> 4) {
        case MIDI_STATUS_NOTE_ON:
            ConvertEvent(ctx, time, status, 3);
            break;

        /* 2 byte data */
        case MIDI_STATUS_NOTE_OFF:
        case MIDI_STATUS_AFTERTOUCH:
        case MIDI_STATUS_CONTROLLER:
        case MIDI_STATUS_PITCH_WHEEL:
            ConvertEvent(ctx, time, status, 2);
            break;

        /* 1 byte data */
        case MIDI_STATUS_PROG_CHANGE:
        case MIDI_STATUS_PRESSURE:
            ConvertEvent(ctx, time, status, 1);
            break;

        case MIDI_STATUS_SYSEX:
            if (status == 0xFF) {
                int32_t pos = getsrcpos(ctx);
                uint32_t dat = read1(ctx);

                if (dat == 0x2F) /* End */
                    end = 1;
                else if (dat == 0x51 && !tempo_set) /* Tempo. Need it for PPQN */
                {
                    skipsrc(ctx, 1);
                    tempo = read1(ctx) << 16;
                    tempo += read1(ctx) << 8;
                    tempo += read1(ctx);
                    tempo *= 3;
                    tempo_set = 1;
                } else if (dat == 0x51 && tempo_set) /* Skip any other tempo changes */
                {
                    GetVLQ(ctx, &dat);
                    skipsrc(ctx, dat);
                    break;
                }

                seeksrc(ctx, pos);
            }
            ConvertSystemMessage(ctx, time, status);
            break;

        default:
            break;
        }
    }
    return ((tempo * 3) / 25000);
}

/* Converts and event list to a MTrk
 * Returns bytes of the array
 * buf can be NULL */
static uint32_t ConvertListToMTrk(struct xmi_ctx *ctx, midi_event *mlist) {
    int32_t time = 0;
    midi_event *event;
    uint32_t delta;
    uint8_t last_status = 0;
    uint32_t i = 8;
    uint32_t j;
    uint32_t size_pos, cur_pos;
    int end = 0;

    write1(ctx, 'M');
    write1(ctx, 'T');
    write1(ctx, 'r');
    write1(ctx, 'k');

    size_pos = getdstpos(ctx);
    skipdst(ctx, 4);

    for (event = mlist; event && !end; event = event->next) {
        delta = (event->time - time);
        time = event->time;

        i += PutVLQ(ctx, delta);

        if ((event->status != last_status) || (event->status >= 0xF0)) {
            write1(ctx, event->status);
            i++;
        }

        last_status = event->status;

        switch (event->status >> 4) {
        /* 2 bytes data
         * Note off, Note on, Aftertouch, Controller and Pitch Wheel */
        case 0x8:
        case 0x9:
        case 0xA:
        case 0xB:
        case 0xE:
            write1(ctx, event->data[0]);
            write1(ctx, event->data[1]);
            i += 2;
            break;

        /* 1 bytes data
         * Program Change and Channel Pressure */
        case 0xC:
        case 0xD:
            write1(ctx, event->data[0]);
            i++;
            break;

        /* Variable length
         * SysEx */
        case 0xF:
            if (event->status == 0xFF) {
                if (event->data[0] == 0x2f)
                    end = 1;
                write1(ctx, event->data[0]);
                i++;
            }
            i += PutVLQ(ctx, event->len);
            if (event->len) {
                for (j = 0; j < event->len; j++) {
                    write1(ctx, event->buffer[j]);
                    i++;
                }
            }
            break;

        /* Never occur */
        default:
            /*_WM_DEBUG_MSG("%s: unrecognized event", __FUNCTION__);*/
            break;
        }
    }

    cur_pos = getdstpos(ctx);
    seekdst(ctx, size_pos);
    write4(ctx, i - 8);
    seekdst(ctx, cur_pos);

    return (i);
}

/* Assumes correct xmidi */
static uint32_t ExtractTracksFromXmi(struct xmi_ctx *ctx) {
    uint32_t num = 0;
    signed short ppqn;
    uint32_t len = 0;
    int32_t begin;
    char buf[32];

    while (getsrcpos(ctx) < getsrcsize(ctx) && num != ctx->info.tracks) {
        /* Read first 4 bytes of name */
        copy(ctx, buf, 4);
        len = read4(ctx);

        /* Skip the FORM entries */
        if (!memcmp(buf, "FORM", 4)) {
            skipsrc(ctx, 4);
            copy(ctx, buf, 4);
            len = read4(ctx);
        }

        if (memcmp(buf, "EVNT", 4)) {
            skipsrc(ctx, (len + 1) & ~1);
            continue;
        }

        ctx->list = NULL;
        begin = getsrcpos(ctx);

        /* Convert it */
        if (!(ppqn = ConvertFiletoList(ctx))) {
            /*_WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, NULL, 0);*/
            break;
        }
        ctx->timing[num] = ppqn;
        ctx->events[num] = ctx->list;

        /* Increment Counter */
        num++;

        /* go to start of next track */
        seeksrc(ctx, begin + ((len + 1) & ~1));
    }

    /* Return how many were converted */
    return (num);
}

static int ParseXMI(struct xmi_ctx *ctx) {
    uint32_t i;
    uint32_t start;
    uint32_t len;
    uint32_t chunk_len;
    uint32_t file_size;
    char buf[32];

    file_size = getsrcsize(ctx);
    if (getsrcpos(ctx) + 8 > file_size) {
badfile:    /*_WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(too short)", 0);*/
        return (-1);
    }

    /* Read first 4 bytes of header */
    copy(ctx, buf, 4);

    /* Could be XMIDI */
    if (!memcmp(buf, "FORM", 4)) {
        /* Read length of */
        len = read4(ctx);

        start = getsrcpos(ctx);
        if (start + 4 > file_size)
            goto badfile;

        /* Read 4 bytes of type */
        copy(ctx, buf, 4);

        /* XDIRless XMIDI, we can handle them here. */
        if (!memcmp(buf, "XMID", 4)) {
            /*_WM_DEBUG_MSG("Warning: XMIDI without XDIR");*/
            ctx->info.tracks = 1;
        }
        /* Not an XMIDI that we recognise */
        else if (memcmp(buf, "XDIR", 4)) {
            goto badfile;
        }
        else { /* Seems Valid */
            ctx->info.tracks = 0;

            for (i = 4; i < len; i++) {
                /* check too short files */
                if (getsrcpos(ctx) + 10 > file_size)
                    break;

                /* Read 4 bytes of type */
                copy(ctx, buf, 4);

                /* Read length of chunk */
                chunk_len = read4(ctx);

                /* Add eight bytes */
                i += 8;

                if (memcmp(buf, "INFO", 4)) {
                    /* Must align */
                    skipsrc(ctx, (chunk_len + 1) & ~1);
                    i += (chunk_len + 1) & ~1;
                    continue;
                }

                /* Must be at least 2 bytes long */
                if (chunk_len < 2)
                    break;

                ctx->info.tracks = read2(ctx);
                break;
            }

            /* Didn't get to fill the header */
            if (ctx->info.tracks == 0) {
                goto badfile;
            }

            /* Ok now to start part 2
             * Goto the right place */
            seeksrc(ctx, start + ((len + 1) & ~1));
            if (getsrcpos(ctx) + 12 > file_size)
                goto badfile;

            /* Read 4 bytes of type */
            copy(ctx, buf, 4);

            if (memcmp(buf, "CAT ", 4)) {
                /*_WM_ERROR_NEW("XMI error: expected \"CAT \", found \"%c%c%c%c\".",
                          buf[0], buf[1], buf[2], buf[3]);*/
                return (-1);
            }

            /* Now read length of this track */
            read4(ctx);

            /* Read 4 bytes of type */
            copy(ctx, buf, 4);

            if (memcmp(buf, "XMID", 4)) {
                /*_WM_ERROR_NEW("XMI error: expected \"XMID\", found \"%c%c%c%c\".",
                          buf[0], buf[1], buf[2], buf[3]);*/
                return (-1);
            }

            /* Valid XMID */
            ctx->datastart = getsrcpos(ctx);
            return (0);
        }
    }

    return (-1);
}

static int ExtractTracks(struct xmi_ctx *ctx) {
    uint32_t i;

    ctx->events = calloc(ctx->info.tracks, sizeof(midi_event*));
    ctx->timing = calloc(ctx->info.tracks, sizeof(int16_t));
    /* type-2 for multi-tracks, type-0 otherwise */
    ctx->info.type = (ctx->info.tracks > 1)? 2 : 0;

    seeksrc(ctx, ctx->datastart);
    i = ExtractTracksFromXmi(ctx);

    if (i != ctx->info.tracks) {
        /*_WM_ERROR_NEW("XMI error: extracted only %u out of %u tracks from XMIDI",
                 ctx->info.tracks, i);*/
        return (-1);
    }

    return (0);
}

