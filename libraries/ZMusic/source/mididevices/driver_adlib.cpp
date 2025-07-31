/*
Copyright (C) 1994-1995 Apogee Software, Ltd.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/**********************************************************************
   module: AL_MIDI.C

   author: James R. Dose
   date:   April 1, 1994

   Low level routines to support General MIDI music on AdLib compatible
   cards.

   (c) Copyright 1994 James R. Dose.  All Rights Reserved.
**********************************************************************/

#include "driver_adlib.h"

#include "_al_midi.h"
//#include "_multivc.h"
#include "../adlmidi/chips/nuked_opl3.h"
//#include "c_cvars.h"

/*
CUSTOM_CVAR(Bool, mus_al_stereo, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	AL_Stereo = self;
	AL_SetStereo(AL_Stereo);
}

CVAR(Bool, mus_al_additivemode, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
*/

enum
{
   AdLibErr_Warning = -2,
   AdLibErr_Error   = -1,
   AdLibErr_Ok      = 0,
};

static void AL_Shutdown(void);
static int ErrorCode;

int AdLibDrv_GetError(void) { return ErrorCode; }

const char *AdLibDrv_ErrorString(int const ErrorNumber)
{
    const char *ErrorString;
    
    switch( ErrorNumber )
    {
        case AdLibErr_Warning :
        case AdLibErr_Error :
            ErrorString = AdLibDrv_ErrorString( ErrorCode );
            break;

        case AdLibErr_Ok :
            ErrorString = "AdLib ok.";
            break;
            
        default:
            ErrorString = "Unknown AdLib error.";
            break;
    }
        
    return ErrorString;
}

#if 0
int AdLibDrv_MIDI_Init(midifuncs * const funcs)
{
    AdLibDrv_MIDI_Shutdown();
    Bmemset(funcs, 0, sizeof(midifuncs));

    funcs->NoteOff           = AL_NoteOff;
    funcs->NoteOn            = AL_NoteOn;
    funcs->PolyAftertouch    = nullptr;
    funcs->ControlChange     = AL_ControlChange;
    funcs->ProgramChange     = AL_ProgramChange;
    funcs->ChannelAftertouch = nullptr;
    funcs->PitchBend         = AL_SetPitchBend;
    
    return AdLibErr_Ok;
}
#endif

//void AdLibDrv_MIDI_HaltPlayback(void) { MV_UnhookMusicRoutine(); }

void AdLibDrv_MIDI_Shutdown(void)
{
    AdLibDrv_MIDI_HaltPlayback();
    AL_Shutdown();
}

int AdLibDrv_MIDI_StartPlayback(void (*service)(void))
{
    AdLibDrv_MIDI_HaltPlayback();

    //AL_Init(MV_MixRate);
    //MV_HookMusicRoutine(service);

	return 0;// MIDI_Ok;
}

void AdLibDrv_MIDI_SetTempo(int const tempo, int const division)
{
    //MV_MIDIRenderTempo = tempo * division / 60;
    //MV_MIDIRenderTimer = 0;
}

static opl3_chip chip;

opl3_chip *AL_GetChip(void) { return &chip; }

static constexpr uint32_t OctavePitch[MAX_OCTAVE+1] = {
    OCTAVE_0, OCTAVE_1, OCTAVE_2, OCTAVE_3, OCTAVE_4, OCTAVE_5, OCTAVE_6, OCTAVE_7,
};

static uint32_t NoteMod12[MAX_NOTE+1];
static uint32_t NoteDiv12[MAX_NOTE+1];

// Pitch table

//static unsigned NotePitch[ FINETUNE_MAX+1 ][ 12 ] =
//   {
//      { C, C_SHARP, D, D_SHARP, E, F, F_SHARP, G, G_SHARP, A, A_SHARP, B },
//   };

static constexpr uint32_t NotePitch[FINETUNE_MAX+1][12] = {
    { 0x157, 0x16b, 0x181, 0x198, 0x1b0, 0x1ca, 0x1e5, 0x202, 0x220, 0x241, 0x263, 0x287 },
    { 0x157, 0x16b, 0x181, 0x198, 0x1b0, 0x1ca, 0x1e5, 0x202, 0x220, 0x242, 0x264, 0x288 },
    { 0x158, 0x16c, 0x182, 0x199, 0x1b1, 0x1cb, 0x1e6, 0x203, 0x221, 0x243, 0x265, 0x289 },
    { 0x158, 0x16c, 0x183, 0x19a, 0x1b2, 0x1cc, 0x1e7, 0x204, 0x222, 0x244, 0x266, 0x28a },
    { 0x159, 0x16d, 0x183, 0x19a, 0x1b3, 0x1cd, 0x1e8, 0x205, 0x223, 0x245, 0x267, 0x28b },
    { 0x15a, 0x16e, 0x184, 0x19b, 0x1b3, 0x1ce, 0x1e9, 0x206, 0x224, 0x246, 0x268, 0x28c },
    { 0x15a, 0x16e, 0x185, 0x19c, 0x1b4, 0x1ce, 0x1ea, 0x207, 0x225, 0x247, 0x269, 0x28e },
    { 0x15b, 0x16f, 0x185, 0x19d, 0x1b5, 0x1cf, 0x1eb, 0x208, 0x226, 0x248, 0x26a, 0x28f },
    { 0x15b, 0x170, 0x186, 0x19d, 0x1b6, 0x1d0, 0x1ec, 0x209, 0x227, 0x249, 0x26b, 0x290 },
    { 0x15c, 0x170, 0x187, 0x19e, 0x1b7, 0x1d1, 0x1ec, 0x20a, 0x228, 0x24a, 0x26d, 0x291 },
    { 0x15d, 0x171, 0x188, 0x19f, 0x1b7, 0x1d2, 0x1ed, 0x20b, 0x229, 0x24b, 0x26e, 0x292 },
    { 0x15d, 0x172, 0x188, 0x1a0, 0x1b8, 0x1d3, 0x1ee, 0x20c, 0x22a, 0x24c, 0x26f, 0x293 },
    { 0x15e, 0x172, 0x189, 0x1a0, 0x1b9, 0x1d4, 0x1ef, 0x20d, 0x22b, 0x24d, 0x270, 0x295 },
    { 0x15f, 0x173, 0x18a, 0x1a1, 0x1ba, 0x1d4, 0x1f0, 0x20e, 0x22c, 0x24e, 0x271, 0x296 },
    { 0x15f, 0x174, 0x18a, 0x1a2, 0x1bb, 0x1d5, 0x1f1, 0x20f, 0x22d, 0x24f, 0x272, 0x297 },
    { 0x160, 0x174, 0x18b, 0x1a3, 0x1bb, 0x1d6, 0x1f2, 0x210, 0x22e, 0x250, 0x273, 0x298 },
    { 0x161, 0x175, 0x18c, 0x1a3, 0x1bc, 0x1d7, 0x1f3, 0x211, 0x22f, 0x251, 0x274, 0x299 },
    { 0x161, 0x176, 0x18c, 0x1a4, 0x1bd, 0x1d8, 0x1f4, 0x212, 0x230, 0x252, 0x276, 0x29b },
    { 0x162, 0x176, 0x18d, 0x1a5, 0x1be, 0x1d9, 0x1f5, 0x212, 0x231, 0x254, 0x277, 0x29c },
    { 0x162, 0x177, 0x18e, 0x1a6, 0x1bf, 0x1d9, 0x1f5, 0x213, 0x232, 0x255, 0x278, 0x29d },
    { 0x163, 0x178, 0x18f, 0x1a6, 0x1bf, 0x1da, 0x1f6, 0x214, 0x233, 0x256, 0x279, 0x29e },
    { 0x164, 0x179, 0x18f, 0x1a7, 0x1c0, 0x1db, 0x1f7, 0x215, 0x235, 0x257, 0x27a, 0x29f },
    { 0x164, 0x179, 0x190, 0x1a8, 0x1c1, 0x1dc, 0x1f8, 0x216, 0x236, 0x258, 0x27b, 0x2a1 },
    { 0x165, 0x17a, 0x191, 0x1a9, 0x1c2, 0x1dd, 0x1f9, 0x217, 0x237, 0x259, 0x27c, 0x2a2 },
    { 0x166, 0x17b, 0x192, 0x1aa, 0x1c3, 0x1de, 0x1fa, 0x218, 0x238, 0x25a, 0x27e, 0x2a3 },
    { 0x166, 0x17b, 0x192, 0x1aa, 0x1c3, 0x1df, 0x1fb, 0x219, 0x239, 0x25b, 0x27f, 0x2a4 },
    { 0x167, 0x17c, 0x193, 0x1ab, 0x1c4, 0x1e0, 0x1fc, 0x21a, 0x23a, 0x25c, 0x280, 0x2a6 },
    { 0x168, 0x17d, 0x194, 0x1ac, 0x1c5, 0x1e0, 0x1fd, 0x21b, 0x23b, 0x25d, 0x281, 0x2a7 },
    { 0x168, 0x17d, 0x194, 0x1ad, 0x1c6, 0x1e1, 0x1fe, 0x21c, 0x23c, 0x25e, 0x282, 0x2a8 },
    { 0x169, 0x17e, 0x195, 0x1ad, 0x1c7, 0x1e2, 0x1ff, 0x21d, 0x23d, 0x260, 0x283, 0x2a9 },
    { 0x16a, 0x17f, 0x196, 0x1ae, 0x1c8, 0x1e3, 0x1ff, 0x21e, 0x23e, 0x261, 0x284, 0x2ab },
    { 0x16a, 0x17f, 0x197, 0x1af, 0x1c8, 0x1e4, 0x200, 0x21f, 0x23f, 0x262, 0x286, 0x2ac }
};

// Slot numbers as a function of the voice and the operator.
// ( melodic only)

static constexpr int slotVoice[NUMADLIBVOICES][2] = {
    { 0, 3 },    // voice 0
    { 1, 4 },    // 1
    { 2, 5 },    // 2
    { 6, 9 },    // 3
    { 7, 10 },   // 4
    { 8, 11 },   // 5
    { 12, 15 },  // 6
    { 13, 16 },  // 7
    { 14, 17 },  // 8
};

static int VoiceLevel[AL_NumChipSlots][2];
static int VoiceKsl[AL_NumChipSlots][2];

// This table gives the offset of each slot within the chip.
// offset = fn( slot)

static constexpr int8_t offsetSlot[AL_NumChipSlots] = { 0, 1, 2, 3, 4, 5, 8, 9, 10, 11, 12, 13, 16, 17, 18, 19, 20, 21 };

static int VoiceReserved[NUMADLIBVOICES * 2];

static AdLibVoice     Voice[NUMADLIBVOICES * 2];
static AdLibVoiceList Voice_Pool;

static AdLibChannel Channel[NUMADLIBCHANNELS];

static int AL_LeftPort  = ADLIB_PORT;
static int AL_RightPort = ADLIB_PORT;
int        AL_Stereo    = TRUE;
static int AL_MaxMidiChannel = 16;

// TODO: clean up this shit...
#define OFFSET(structure, offset) (*((char **)&(structure)[offset]))

#define LL_AddToTail(type, listhead, node)                                                                                 \
    LL_AddNode((char *)(node), (char **)&((listhead)->end), (char **)&((listhead)->start), (intptr_t) & ((type *)0)->prev, \
               (intptr_t) & ((type *)0)->next)

#define LL_Remove(type, listhead, node)                                                                                       \
    LL_RemoveNode((char *)(node), (char **)&((listhead)->start), (char **)&((listhead)->end), (intptr_t) & ((type *)0)->next, \
                  (intptr_t) & ((type *)0)->prev)

static void LL_RemoveNode(char *item, char **head, char **tail, intptr_t next, intptr_t prev)
{
    if (OFFSET(item, prev) == nullptr)
        *head = OFFSET(item, next);
    else
        OFFSET(OFFSET(item, prev), next) = OFFSET(item, next);

    if (OFFSET(item, next) == nullptr)
        *tail = OFFSET(item, prev);
    else
        OFFSET(OFFSET(item, next), prev) = OFFSET(item, prev);

    OFFSET(item, next) = nullptr;
    OFFSET(item, prev) = nullptr;
}

static void LL_AddNode(char *item, char **head, char **tail, intptr_t next, intptr_t prev)
{
    OFFSET(item, prev) = nullptr;
    OFFSET(item, next) = *head;

    if (*head)
        OFFSET(*head, prev) = item;
    else
        *tail = item;

    *head = item;
}


static void AL_SendOutputToPort(int const port, int const reg, int const data)
{
    OPL3_WriteRegBuffered(&chip, (Bit16u)(reg + ((port & 2) << 7)), (Bit8u)data);
}


static void AL_SendOutput(int const voice, int const reg, int const data)
{
    int port = (voice == 0) ? AL_RightPort : AL_LeftPort;
    AL_SendOutputToPort(port, reg, data);
}


static void AL_SetVoiceTimbre(int const voice)
{
    int const channel = Voice[voice].channel;
    int const patch = (channel == 9) ? Voice[voice].key + 128 : Channel[channel].Timbre;

    if (Voice[voice].timbre == patch)
        return;

    Voice[voice].timbre = patch;

    auto const timbre = &ADLIB_TimbreBank[patch];

    int const port = Voice[voice].port;
    int const voc  = (voice >= NUMADLIBVOICES) ? voice - NUMADLIBVOICES : voice;
    int       slot = slotVoice[voc][0];
    int       off  = offsetSlot[slot];

    VoiceLevel[slot][port] = 63 - (timbre->Level[0] & 0x3f);
    VoiceKsl[slot][port]   = timbre->Level[0] & 0xc0;

    AL_SendOutput(port, 0xA0 + voc, 0);
    AL_SendOutput(port, 0xB0 + voc, 0);

    // Let voice clear the release
    AL_SendOutput(port, 0x80 + off, 0xff);

    AL_SendOutput(port, 0x60 + off, timbre->Env1[0]);
    AL_SendOutput(port, 0x80 + off, timbre->Env2[0]);
    AL_SendOutput(port, 0x20 + off, timbre->SAVEK[0]);
    AL_SendOutput(port, 0xE0 + off, timbre->Wave[0]);

    AL_SendOutput(port, 0x40 + off, timbre->Level[0]);
    slot = slotVoice[voc][1];

    AL_SendOutput(port, 0xC0 + voc, (timbre->Feedback & 0x0f) | 0x30);

    off = offsetSlot[slot];

    VoiceLevel[slot][port] = 63 - (timbre->Level[1] & 0x3f);
    VoiceKsl[slot][port]   = timbre->Level[1] & 0xc0;

    AL_SendOutput(port, 0x40 + off, 63);

    // Let voice clear the release
    AL_SendOutput(port, 0x80 + off, 0xff);

    AL_SendOutput(port, 0x60 + off, timbre->Env1[1]);
    AL_SendOutput(port, 0x80 + off, timbre->Env2[1]);
    AL_SendOutput(port, 0x20 + off, timbre->SAVEK[1]);
    AL_SendOutput(port, 0xE0 + off, timbre->Wave[1]);
}


static void AL_SetVoiceVolume(int const voice)
{
    int  const channel  = Voice[voice].channel;
    auto const timbre   = &ADLIB_TimbreBank[Voice[voice].timbre];
    int  const velocity = min<int>(Voice[voice].velocity + timbre->Velocity, MAX_VELOCITY);

    int const voc  = (voice >= NUMADLIBVOICES) ? voice - NUMADLIBVOICES : voice;
    int const slot = slotVoice[voc][1];
    int const port = Voice[voice].port;

    // amplitude
    auto t1 = (uint32_t)VoiceLevel[slot][port] * (velocity + 0x80);
    t1 = (Channel[channel].Volume * t1) >> 15;

    uint32_t volume = t1 ^ 63;
    volume |= (uint32_t)VoiceKsl[slot][port];

    AL_SendOutput(port, 0x40 + offsetSlot[slot], volume);

    // Check if this timbre is Additive
    if (timbre->Feedback & 0x01)
    {
        int const slot = slotVoice[voc][0];
        uint32_t t2;

        // amplitude
        if (mus_al_additivemode)
            t1 = (uint32_t)VoiceLevel[slot][port] * (velocity + 0x80);

        t2 = (Channel[channel].Volume * t1) >> 15;

        volume = t2 ^ 63;
        volume |= (uint32_t)VoiceKsl[slot][port];

        AL_SendOutput(port, 0x40 + offsetSlot[slot], volume);
    }
}


static int AL_AllocVoice(void)
{
    if (Voice_Pool.start)
    {
        int const voice = Voice_Pool.start->num;
        LL_Remove(AdLibVoice, &Voice_Pool, &Voice[voice]);
        return voice;
    }

    return  AL_VoiceNotFound;
}


static int AL_GetVoice(int const channel, int const key)
{
    auto const *voice = Channel[channel].Voices.start;

    while (voice != nullptr)
    {
        if (voice->key == (uint32_t)key)
            return  voice->num;
        voice = voice->next;
    }

    return  AL_VoiceNotFound;
}


static void AL_SetVoicePitch(int const voice)
{
    int const port    = Voice[voice].port;
    int const voc     = (voice >= NUMADLIBVOICES) ? voice - NUMADLIBVOICES : voice;
    int const channel = Voice[voice].channel;

    int patch, note;

    if (channel == 9)
    {
        patch = Voice[voice].key + 128;
        note  = ADLIB_TimbreBank[patch].Transpose;
    }
    else
    {
        patch = Channel[channel].Timbre;
        note  = Voice[voice].key + ADLIB_TimbreBank[patch].Transpose;
    }

    note += Channel[channel].KeyOffset - 12;
    note = clamp(note, 0, MAX_NOTE);

    int detune = Channel[channel].KeyDetune;

    int ScaleNote = NoteMod12[note];
    int Octave    = NoteDiv12[note];

    int pitch = OctavePitch[Octave] | NotePitch[detune][ScaleNote];

    Voice[voice].pitchleft = pitch;

    pitch |= Voice[voice].status;

    AL_SendOutput(port, 0xA0 + voc, pitch);
    AL_SendOutput(port, 0xB0 + voc, pitch >> 8);
}

static void AL_SetVoicePan(int const voice)
{
    int const port    = Voice[voice].port;
    int const voc     = (voice >= NUMADLIBVOICES) ? voice - NUMADLIBVOICES : voice;
    int const channel = Voice[voice].channel;

    if (AL_Stereo)
        AL_SendOutput(port, 0xD0 + voc, Channel[channel].Pan << 1);
}


static void AL_SetChannelVolume(int const channel, int const volume)
{
    Channel[channel].Volume = clamp(volume, 0, AL_MaxVolume);

    auto voice = Channel[channel].Voices.start;

    while (voice != nullptr)
    {
        AL_SetVoiceVolume(voice->num);
        voice = voice->next;
    }
}


static void AL_SetChannelPan(int const channel, int const pan)
{
    // Don't pan drum sounds
    if (channel != 9)
        Channel[channel].Pan = pan;

    auto voice = Channel[channel].Voices.start;
    while (voice != nullptr)
    {
        AL_SetVoicePan(voice->num);
        voice = voice->next;
    }
}


static void AL_SetChannelDetune(int const channel, int const detune) { Channel[channel].Detune = detune; }


static void AL_ResetVoices(void)
{
    Voice_Pool.start = nullptr;
    Voice_Pool.end   = nullptr;

    int const numvoices = NUMADLIBVOICES * 2;

    for (int index = 0; index < numvoices; index++)
    {
        if (VoiceReserved[index] == FALSE)
        {
            Voice[index].num      = index;
            Voice[index].key      = 0;
            Voice[index].velocity = 0;
            Voice[index].channel  = -1;
            Voice[index].timbre   = -1;
            Voice[index].port     = (index < NUMADLIBVOICES) ? 0 : 1;
            Voice[index].status   = NOTE_OFF;
            LL_AddToTail(AdLibVoice, &Voice_Pool, &Voice[index]);
        }
    }

    for (int index = 0; index < NUMADLIBCHANNELS; index++)
    {
        Channel[index] = {};
        Channel[index].Volume             = AL_DefaultChannelVolume;
        Channel[index].Pan                = 64;
        Channel[index].PitchBendRange     = AL_DefaultPitchBendRange;
        Channel[index].PitchBendSemiTones = AL_DefaultPitchBendRange / 100;
        Channel[index].PitchBendHundreds  = AL_DefaultPitchBendRange % 100;
    }
}


static void AL_CalcPitchInfo(void)
{
    //   int    finetune;
    //   double detune;

    for (int note = 0; note <= MAX_NOTE; note++)
    {
        NoteMod12[note] = note % 12;
        NoteDiv12[note] = note / 12;
    }

    //   for( finetune = 1; finetune <= FINETUNE_MAX; finetune++ )
    //      {
    //      detune = pow( 2, ( double )finetune / ( 12.0 * FINETUNE_RANGE ) );
    //      for( note = 0; note < 12; note++ )
    //         {
    //         NotePitch[ finetune ][ note ] = ( ( double )NotePitch[ 0 ][ note ] * detune );
    //         }
    //      }
}


static void AL_FlushCard(int const port)
{
    for (int i = 0; i < NUMADLIBVOICES; i++)
    {
        if (VoiceReserved[i])
            continue;

        auto slot1 = offsetSlot[slotVoice[i][0]];
        auto slot2 = offsetSlot[slotVoice[i][1]];

        AL_SendOutputToPort(port, 0xA0 + i, 0);
        AL_SendOutputToPort(port, 0xB0 + i, 0);

        AL_SendOutputToPort(port, 0xE0 + slot1, 0);
        AL_SendOutputToPort(port, 0xE0 + slot2, 0);

        // Set the envelope to be fast and quiet
        AL_SendOutputToPort(port, 0x60 + slot1, 0xff);
        AL_SendOutputToPort(port, 0x60 + slot2, 0xff);
        AL_SendOutputToPort(port, 0x80 + slot1, 0xff);
        AL_SendOutputToPort(port, 0x80 + slot2, 0xff);

        // Maximum attenuation
        AL_SendOutputToPort(port, 0x40 + slot1, 0xff);
        AL_SendOutputToPort(port, 0x40 + slot2, 0xff);
    }
}


static void AL_Reset(void)
{
    AL_SendOutputToPort(ADLIB_PORT, 1, 0x20);
    AL_SendOutputToPort(ADLIB_PORT, 0x08, 0);

    // Set the values: AM Depth, VIB depth & Rhythm
    AL_SendOutputToPort(ADLIB_PORT, 0xBD, 0);

    AL_SetStereo(AL_Stereo);

    AL_FlushCard(AL_LeftPort);
    AL_FlushCard(AL_RightPort);
}


void AL_SetStereo(int const stereo)
{
    AL_SendOutputToPort(AL_RightPort, 0x5, (stereo<<1)+1);
}


static void AL_NoteOff(int const channel, int const key, int velocity)
{
    UNREFERENCED_PARAMETER(velocity);

    // We only play channels 1 through 10
    if (channel > AL_MaxMidiChannel)
        return;

    int voice = AL_GetVoice(channel, key);

    if (voice == AL_VoiceNotFound)
        return;

    Voice[voice].status = NOTE_OFF;

    int port = Voice[voice].port;
    int voc  = (voice >= NUMADLIBVOICES) ? voice - NUMADLIBVOICES : voice;

    AL_SendOutput(port, 0xB0 + voc, hibyte(Voice[voice].pitchleft));

    LL_Remove(AdLibVoice, &Channel[channel].Voices, &Voice[voice]);
    LL_AddToTail(AdLibVoice, &Voice_Pool, &Voice[voice]);
}


static void AL_NoteOn(int const channel, int const key, int const velocity)
{
    // We only play channels 1 through 10
    if (channel > AL_MaxMidiChannel)
        return;

    if (velocity == 0)
    {
        AL_NoteOff(channel, key, velocity);
        return;
    }

    int voice = AL_AllocVoice();

    if (voice == AL_VoiceNotFound)
    {
        if (Channel[9].Voices.start)
        {
            AL_NoteOff(9, Channel[9].Voices.start->key, 0);
            voice = AL_AllocVoice();
        }

        if (voice == AL_VoiceNotFound)
            return;
    }

    Voice[voice].key      = key;
    Voice[voice].channel  = channel;
    Voice[voice].velocity = velocity;
    Voice[voice].status   = NOTE_ON;

    LL_AddToTail(AdLibVoice, &Channel[channel].Voices, &Voice[voice]);

    AL_SetVoiceTimbre(voice);
    AL_SetVoiceVolume(voice);
    AL_SetVoicePitch(voice);
    AL_SetVoicePan(voice);
}


static inline void AL_AllNotesOff(int const channel)
{
    while (Channel[channel].Voices.start != nullptr)
        AL_NoteOff(channel, Channel[channel].Voices.start->key, 0);
}


static void AL_ControlChange(int const channel, int const type, int const data)
{
    // We only play channels 1 through 10
    if (channel > AL_MaxMidiChannel)
        return;

    switch (type)
    {
        case MIDI_VOLUME:
            AL_SetChannelVolume(channel, data);
            break;

        case MIDI_PAN:
            AL_SetChannelPan(channel, data);
            break;

        case MIDI_DETUNE:
            AL_SetChannelDetune(channel, data);
            break;

        case MIDI_ALL_NOTES_OFF:
            AL_AllNotesOff(channel);
            break;

        case MIDI_RESET_ALL_CONTROLLERS:
            AL_ResetVoices();
            AL_SetChannelVolume(channel, AL_DefaultChannelVolume);
            AL_SetChannelPan(channel, 64);
            AL_SetChannelDetune(channel, 0);
            break;

        case MIDI_RPN_MSB:
            Channel[channel].RPN &= 0x00FF;
            Channel[channel].RPN |= (data & 0xFF) << 8;
            break;

        case MIDI_RPN_LSB:
            Channel[channel].RPN &= 0xFF00;
            Channel[channel].RPN |= data & 0xFF;
            break;

        case MIDI_DATAENTRY_MSB:
            if (Channel[channel].RPN == MIDI_PITCHBEND_RPN)
            {
                Channel[channel].PitchBendSemiTones = data;
                Channel[channel].PitchBendRange     = Channel[channel].PitchBendSemiTones * 100 + Channel[channel].PitchBendHundreds;
            }
            break;

        case MIDI_DATAENTRY_LSB:
            if (Channel[channel].RPN == MIDI_PITCHBEND_RPN)
            {
                Channel[channel].PitchBendHundreds = data;
                Channel[channel].PitchBendRange    = Channel[channel].PitchBendSemiTones * 100 + Channel[channel].PitchBendHundreds;
            }
            break;
    }
}


static void AL_ProgramChange(int const channel, int const patch)
{
    // We only play channels 1 through 10
    if (channel > AL_MaxMidiChannel)
        return;

    Channel[channel].Timbre = patch;
}


static void AL_SetPitchBend(int const channel, int const lsb, int const msb)
{
    // We only play channels 1 through 10
    if (channel > AL_MaxMidiChannel)
        return;

    int const pitchbend = lsb + (msb << 8);

    Channel[channel].Pitchbend = pitchbend;

    int TotalBend = pitchbend * Channel[channel].PitchBendRange;
    TotalBend /= (PITCHBEND_CENTER / FINETUNE_RANGE);

    Channel[channel].KeyOffset = (int)(TotalBend / FINETUNE_RANGE);
    Channel[channel].KeyOffset -= Channel[channel].PitchBendSemiTones;

    Channel[channel].KeyDetune = (uint32_t)(TotalBend % FINETUNE_RANGE);

    auto voice = Channel[channel].Voices.start;
    while (voice != nullptr)
    {
        AL_SetVoicePitch(voice->num);
        voice = voice->next;
    }
}


static void AL_Shutdown(void)
{
    AL_ResetVoices();
    AL_Reset();
}


static int AL_Init(int const rate)
{
    OPL3_Reset(&chip, rate);

    AL_LeftPort  = ADLIB_PORT;
    AL_RightPort = ADLIB_PORT + 2;

    AL_CalcPitchInfo();
    AL_Reset();
    AL_ResetVoices();

    return AdLibErr_Ok;
}


void AL_RegisterTimbreBank(uint8_t *timbres)
{
    for (int i = 0; i < 256; i++)
    {
        ADLIB_TimbreBank[i].SAVEK[0]  = *(timbres++);
        ADLIB_TimbreBank[i].SAVEK[1]  = *(timbres++);
        ADLIB_TimbreBank[i].Level[0]  = *(timbres++);
        ADLIB_TimbreBank[i].Level[1]  = *(timbres++);
        ADLIB_TimbreBank[i].Env1[0]   = *(timbres++);
        ADLIB_TimbreBank[i].Env1[1]   = *(timbres++);
        ADLIB_TimbreBank[i].Env2[0]   = *(timbres++);
        ADLIB_TimbreBank[i].Env2[1]   = *(timbres++);
        ADLIB_TimbreBank[i].Wave[0]   = *(timbres++);
        ADLIB_TimbreBank[i].Wave[1]   = *(timbres++);
        ADLIB_TimbreBank[i].Feedback  = *(timbres++);
        ADLIB_TimbreBank[i].Transpose = *(int8_t *)(timbres++);
        ADLIB_TimbreBank[i].Velocity  = *(int8_t *)(timbres++);
    }
}
