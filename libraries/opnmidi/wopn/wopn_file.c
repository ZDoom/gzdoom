/*
 * Wohlstand's OPN2 Bank File - a bank format to store OPN2 timbre data and setup
 *
 * Copyright (c) 2018 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "wopn_file.h"
#include <string.h>
#include <stdlib.h>

static const char       *wopn2_magic1 = "WOPN2-BANK\0";
static const char       *wopn2_magic2 = "WOPN2-B2NK\0";
static const char       *opni_magic1 = "WOPN2-INST\0";
static const char       *opni_magic2 = "WOPN2-IN2T\0";

static const uint16_t   wopn_latest_version = 2;

enum
{
    WOPN_INST_SIZE_V1 = 65,
    WOPN_INST_SIZE_V2 = 69
};

static uint16_t toUint16LE(const uint8_t *arr)
{
    uint16_t num = arr[0];
    num |= ((arr[1] << 8) & 0xFF00);
    return num;
}

static uint16_t toUint16BE(const uint8_t *arr)
{
    uint16_t num = arr[1];
    num |= ((arr[0] << 8) & 0xFF00);
    return num;
}

static int16_t toSint16BE(const uint8_t *arr)
{
    int16_t num = *(const int8_t *)(&arr[0]);
    num *= 1 << 8;
    num |= arr[1];
    return num;
}

static void fromUint16LE(uint16_t in, uint8_t *arr)
{
    arr[0] =  in & 0x00FF;
    arr[1] = (in >> 8) & 0x00FF;
}

static void fromUint16BE(uint16_t in, uint8_t *arr)
{
    arr[1] =  in & 0x00FF;
    arr[0] = (in >> 8) & 0x00FF;
}

static void fromSint16BE(int16_t in, uint8_t *arr)
{
    arr[1] =  in & 0x00FF;
    arr[0] = ((uint16_t)in >> 8) & 0x00FF;
}


WOPNFile *WOPN_Init(uint16_t melodic_banks, uint16_t percussive_banks)
{
    WOPNFile *file = NULL;
    if(melodic_banks == 0)
        return NULL;
    if(percussive_banks == 0)
        return NULL;
    file = (WOPNFile*)calloc(1, sizeof(WOPNFile));
    if(!file)
        return NULL;
    file->banks_count_melodic = melodic_banks;
    file->banks_count_percussion = percussive_banks;
    file->banks_melodic = (WOPNBank*)calloc(1, sizeof(WOPNBank) * melodic_banks );
    file->banks_percussive = (WOPNBank*)calloc(1, sizeof(WOPNBank) * percussive_banks );
    return file;
}

void WOPN_Free(WOPNFile *file)
{
    if(file)
    {
        if(file->banks_melodic)
            free(file->banks_melodic);
        if(file->banks_percussive)
            free(file->banks_percussive);
        free(file);
    }
}

int WOPN_BanksCmp(const WOPNFile *bank1, const WOPNFile *bank2)
{
    int res = 1;

    res &= (bank1->version == bank2->version);
    res &= (bank1->lfo_freq == bank2->lfo_freq);
    res &= (bank1->volume_model == bank2->volume_model);
    res &= (bank1->banks_count_melodic == bank2->banks_count_melodic);
    res &= (bank1->banks_count_percussion == bank2->banks_count_percussion);

    if(res)
    {
        int i;
        for(i = 0; i < bank1->banks_count_melodic; i++)
            res &= (memcmp(&bank1->banks_melodic[i], &bank2->banks_melodic[i], sizeof(WOPNBank)) == 0);
        if(res)
        {
            for(i = 0; i < bank1->banks_count_percussion; i++)
                res &= (memcmp(&bank1->banks_percussive[i], &bank2->banks_percussive[i], sizeof(WOPNBank)) == 0);
        }
    }

    return res;
}

static void WOPN_parseInstrument(WOPNInstrument *ins, uint8_t *cursor, uint16_t version, uint8_t has_sounding_delays)
{
    int l;
    strncpy(ins->inst_name, (const char*)cursor, 32);
    ins->inst_name[32] = '\0';
    ins->note_offset = toSint16BE(cursor + 32);
    ins->midi_velocity_offset = 0;  /* TODO: for future version > 2 */
    ins->percussion_key_number = cursor[34];
    ins->inst_flags = 0;  /* TODO: for future version > 2 */
    ins->fbalg = cursor[35];
    ins->lfosens = cursor[36];
    for(l = 0; l < 4; l++)
    {
        size_t off = 37 + (size_t)(l) * 7;
        ins->operators[l].dtfm_30     = cursor[off + 0];
        ins->operators[l].level_40    = cursor[off + 1];
        ins->operators[l].rsatk_50    = cursor[off + 2];
        ins->operators[l].amdecay1_60 = cursor[off + 3];
        ins->operators[l].decay2_70   = cursor[off + 4];
        ins->operators[l].susrel_80   = cursor[off + 5];
        ins->operators[l].ssgeg_90    = cursor[off + 6];
    }
    if((version >= 2) && has_sounding_delays)
    {
        ins->delay_on_ms  = toUint16BE(cursor + 65);
        ins->delay_off_ms = toUint16BE(cursor + 67);

        /* Null delays indicate the blank instrument in version 2 */
        if((version < 3) && ins->delay_on_ms == 0 && ins->delay_off_ms == 0)
            ins->inst_flags |= WOPN_Ins_IsBlank;
    }
}

static void WOPN_writeInstrument(WOPNInstrument *ins, uint8_t *cursor, uint16_t version, uint8_t has_sounding_delays)
{
    int l;
    strncpy((char*)cursor, ins->inst_name, 32);
    fromSint16BE(ins->note_offset, cursor + 32);
    cursor[34] = ins->percussion_key_number;
    cursor[35] = ins->fbalg;
    cursor[36] = ins->lfosens;
    for(l = 0; l < 4; l++)
    {
        size_t off = 37 + (size_t)(l) * 7;
        cursor[off + 0] = ins->operators[l].dtfm_30;
        cursor[off + 1] = ins->operators[l].level_40;
        cursor[off + 2] = ins->operators[l].rsatk_50;
        cursor[off + 3] = ins->operators[l].amdecay1_60;
        cursor[off + 4] = ins->operators[l].decay2_70;
        cursor[off + 5] = ins->operators[l].susrel_80;
        cursor[off + 6] = ins->operators[l].ssgeg_90;
    }
    if((version >= 2) && has_sounding_delays)
    {
        if((version < 3) && (ins->inst_flags & WOPN_Ins_IsBlank) != 0)
        {
            /* Null delays indicate the blank instrument in version 2 */
            fromUint16BE(0, cursor + 65);
            fromUint16BE(0, cursor + 67);
        }
        else
        {
            fromUint16BE(ins->delay_on_ms, cursor + 65);
            fromUint16BE(ins->delay_off_ms, cursor + 67);
        }
    }
}

WOPNFile *WOPN_LoadBankFromMem(void *mem, size_t length, int *error)
{
    WOPNFile *outFile = NULL;
    uint16_t i = 0, j = 0, k = 0;
    uint16_t version = 0;
    uint16_t count_melodic_banks     = 1;
    uint16_t count_percussive_banks   = 1;
    uint8_t *cursor = (uint8_t *)mem;

    WOPNBank *bankslots[2];
    uint16_t  bankslots_sizes[2];

#define SET_ERROR(err) \
{\
    WOPN_Free(outFile);\
    if(error)\
    {\
        *error = err;\
    }\
}

#define GO_FORWARD(bytes) { cursor += bytes; length -= bytes; }

    if(!cursor)
    {
        SET_ERROR(WOPN_ERR_NULL_POINTER);
        return NULL;
    }

    {/* Magic number */
        if(length < 11)
        {
            SET_ERROR(WOPN_ERR_UNEXPECTED_ENDING);
            return NULL;
        }
        if(memcmp(cursor, wopn2_magic1, 11) == 0)
        {
            version = 1;
        }
        else if(memcmp(cursor, wopn2_magic2, 11) != 0)
        {
            SET_ERROR(WOPN_ERR_BAD_MAGIC);
            return NULL;
        }
        GO_FORWARD(11);
    }

    if (version == 0)
    {/* Version code */
        if(length < 2)
        {
            SET_ERROR(WOPN_ERR_UNEXPECTED_ENDING);
            return NULL;
        }
        version = toUint16LE(cursor);
        if(version > wopn_latest_version)
        {
            SET_ERROR(WOPN_ERR_NEWER_VERSION);
            return NULL;
        }
        GO_FORWARD(2);
    }

    {/* Header of WOPN */
        uint8_t head[5];
        if(length < 5)
        {
            SET_ERROR(WOPN_ERR_UNEXPECTED_ENDING);
            return NULL;
        }
        memcpy(head, cursor, 5);
        count_melodic_banks = toUint16BE(head);
        count_percussive_banks = toUint16BE(head + 2);
        GO_FORWARD(5);

        outFile = WOPN_Init(count_melodic_banks, count_percussive_banks);
        if(!outFile)
        {
            SET_ERROR(WOPN_ERR_OUT_OF_MEMORY);
            return NULL;
        }

        outFile->version      = version;
        outFile->lfo_freq      = head[4];
        outFile->volume_model = 0;
    }

    bankslots_sizes[0] = count_melodic_banks;
    bankslots[0] = outFile->banks_melodic;
    bankslots_sizes[1] = count_percussive_banks;
    bankslots[1] = outFile->banks_percussive;

    if(version >= 2) /* Bank names and LSB/MSB titles */
    {
        for(i = 0; i < 2; i++)
        {
            for(j = 0; j < bankslots_sizes[i]; j++)
            {
                if(length < 34)
                {
                    SET_ERROR(WOPN_ERR_UNEXPECTED_ENDING);
                    return NULL;
                }
                strncpy(bankslots[i][j].bank_name, (const char*)cursor, 32);
                bankslots[i][j].bank_name[32] = '\0';
                bankslots[i][j].bank_midi_lsb = cursor[32];
                bankslots[i][j].bank_midi_msb = cursor[33];
                GO_FORWARD(34);
            }
        }
    }

    {/* Read instruments data */
        uint16_t insSize = 0;
        if(version > 1)
            insSize = WOPN_INST_SIZE_V2;
        else
            insSize = WOPN_INST_SIZE_V1;
        for(i = 0; i < 2; i++)
        {
            if(length < (insSize * 128) * (size_t)bankslots_sizes[i])
            {
                SET_ERROR(WOPN_ERR_UNEXPECTED_ENDING);
                return NULL;
            }

            for(j = 0; j < bankslots_sizes[i]; j++)
            {
                for(k = 0; k < 128; k++)
                {
                    WOPNInstrument *ins = &bankslots[i][j].ins[k];
                    WOPN_parseInstrument(ins, cursor, version, 1);
                    GO_FORWARD(insSize);
                }
            }
        }
    }

#undef GO_FORWARD
#undef SET_ERROR

    return outFile;
}

int WOPN_LoadInstFromMem(OPNIFile *file, void *mem, size_t length)
{
    uint16_t version = 0;
    uint8_t *cursor = (uint8_t *)mem;
    uint16_t ins_size;

    if(!cursor)
        return WOPN_ERR_NULL_POINTER;

#define GO_FORWARD(bytes) { cursor += bytes; length -= bytes; }

    {/* Magic number */
        if(length < 11)
            return WOPN_ERR_UNEXPECTED_ENDING;
        if(memcmp(cursor, opni_magic1, 11) == 0)
            version = 1;
        else if(memcmp(cursor, opni_magic2, 11) != 0)
            return WOPN_ERR_BAD_MAGIC;
        GO_FORWARD(11);
    }

    if (version == 0) {/* Version code */
        if(length < 2)
            return WOPN_ERR_UNEXPECTED_ENDING;
        version = toUint16LE(cursor);
        if(version > wopn_latest_version)
            return WOPN_ERR_NEWER_VERSION;
        GO_FORWARD(2);
    }

    file->version = version;

    {/* is drum flag */
        if(length < 1)
            return WOPN_ERR_UNEXPECTED_ENDING;
        file->is_drum = *cursor;
        GO_FORWARD(1);
    }

    if(version > 1)
        /* Skip sounding delays are not part of single-instrument file
         * two sizes of uint16_t will be subtracted */
        ins_size = WOPN_INST_SIZE_V2 - (sizeof(uint16_t) * 2);
    else
        ins_size = WOPN_INST_SIZE_V1;

    if(length < ins_size)
        return WOPN_ERR_UNEXPECTED_ENDING;

    WOPN_parseInstrument(&file->inst, cursor, version, 0);
    GO_FORWARD(ins_size);

    return WOPN_ERR_OK;
#undef GO_FORWARD
}

size_t WOPN_CalculateBankFileSize(WOPNFile *file, uint16_t version)
{
    size_t final_size = 0;
    size_t ins_size = 0;

    if(version == 0)
        version = wopn_latest_version;

    if(!file)
        return 0;
    final_size += 11 + 2 + 2 + 2 + 1;
    /*
     * Magic number,
     * Version,
     * Count of melodic banks,
     * Count of percussive banks,
     * Chip specific flags
     */

    if(version >= 2)
    {
        /* Melodic banks meta-data */
        final_size += (32 + 1 + 1) * file->banks_count_melodic;
        /* Percussive banks meta-data */
        final_size += (32 + 1 + 1) * file->banks_count_percussion;
    }

    if(version >= 2)
        ins_size = WOPN_INST_SIZE_V2;
    else
        ins_size = WOPN_INST_SIZE_V1;
    /* Melodic instruments */
    final_size += (ins_size * 128) * file->banks_count_melodic;
    /* Percussive instruments */
    final_size += (ins_size * 128) * file->banks_count_percussion;

    return final_size;
}

size_t WOPN_CalculateInstFileSize(OPNIFile *file, uint16_t version)
{
    size_t final_size = 0;
    size_t ins_size = 0;

    if(version == 0)
        version = wopn_latest_version;

    if(!file)
        return 0;
    final_size += 11 + 1;
    /*
     * Magic number,
     * is percussive instrument
     */

    /* Version */
    if (version > 1)
        final_size += 2;

    if(version > 1)
        /* Skip sounding delays are not part of single-instrument file
         * two sizes of uint16_t will be subtracted */
        ins_size = WOPN_INST_SIZE_V2 - (sizeof(uint16_t) * 2);
    else
        ins_size = WOPN_INST_SIZE_V1;
    final_size += ins_size;

    return final_size;
}

int WOPN_SaveBankToMem(WOPNFile *file, void *dest_mem, size_t length, uint16_t version, uint16_t force_gm)
{
    uint8_t *cursor = (uint8_t *)dest_mem;
    uint16_t ins_size = 0;
    uint16_t i, j, k;
    uint16_t banks_melodic = force_gm ? 1 : file->banks_count_melodic;
    uint16_t banks_percussive = force_gm ? 1 : file->banks_count_percussion;

    WOPNBank *bankslots[2];
    uint16_t  bankslots_sizes[2];

    if(version == 0)
        version = wopn_latest_version;

#define GO_FORWARD(bytes) { cursor += bytes; length -= bytes; }

    if(length < 11)
        return WOPN_ERR_UNEXPECTED_ENDING;
    if(version > 1)
        memcpy(cursor, wopn2_magic2, 11);
    else
        memcpy(cursor, wopn2_magic1, 11);
    GO_FORWARD(11);

    if(version > 1)
    {
        if(length < 2)
            return WOPN_ERR_UNEXPECTED_ENDING;
        fromUint16LE(version, cursor);
        GO_FORWARD(2);
    }

    if(length < 2)
        return WOPN_ERR_UNEXPECTED_ENDING;
    fromUint16BE(banks_melodic, cursor);
    GO_FORWARD(2);

    if(length < 2)
        return WOPN_ERR_UNEXPECTED_ENDING;
    fromUint16BE(banks_percussive, cursor);
    GO_FORWARD(2);

    if(length < 1)
        return WOPN_ERR_UNEXPECTED_ENDING;
    cursor[0] = file->lfo_freq;
    GO_FORWARD(1);

    bankslots[0]        = file->banks_melodic;
    bankslots_sizes[0]  = banks_melodic;
    bankslots[1]        = file->banks_percussive;
    bankslots_sizes[1]  = banks_percussive;

    if(version >= 2)
    {
        for(i = 0; i < 2; i++)
        {
            for(j = 0; j < bankslots_sizes[i]; j++)
            {
                if(length < 34)
                    return WOPN_ERR_UNEXPECTED_ENDING;
                strncpy((char*)cursor, bankslots[i][j].bank_name, 32);
                cursor[32] = bankslots[i][j].bank_midi_lsb;
                cursor[33] = bankslots[i][j].bank_midi_msb;
                GO_FORWARD(34);
            }
        }
    }

    {/* Write instruments data */
        if(version >= 2)
            ins_size = WOPN_INST_SIZE_V2;
        else
            ins_size = WOPN_INST_SIZE_V1;
        for(i = 0; i < 2; i++)
        {
            if(length < (ins_size * 128) * (size_t)bankslots_sizes[i])
                return WOPN_ERR_UNEXPECTED_ENDING;

            for(j = 0; j < bankslots_sizes[i]; j++)
            {
                for(k = 0; k < 128; k++)
                {
                    WOPNInstrument *ins = &bankslots[i][j].ins[k];
                    WOPN_writeInstrument(ins, cursor, version, 1);
                    GO_FORWARD(ins_size);
                }
            }
        }
    }

    return WOPN_ERR_OK;
#undef GO_FORWARD
}

int WOPN_SaveInstToMem(OPNIFile *file, void *dest_mem, size_t length, uint16_t version)
{
    uint8_t *cursor = (uint8_t *)dest_mem;
    uint16_t ins_size;

    if(!cursor)
        return WOPN_ERR_NULL_POINTER;

    if(version == 0)
        version = wopn_latest_version;

#define GO_FORWARD(bytes) { cursor += bytes; length -= bytes; }

    {/* Magic number */
        if(length < 11)
            return WOPN_ERR_UNEXPECTED_ENDING;
        if(version > 1)
            memcpy(cursor, opni_magic2, 11);
        else
            memcpy(cursor, opni_magic1, 11);
        GO_FORWARD(11);
    }

    if (version > 1)
    {/* Version code */
        if(length < 2)
            return WOPN_ERR_UNEXPECTED_ENDING;
        fromUint16LE(version, cursor);
        GO_FORWARD(2);
    }

    {/* is drum flag */
        if(length < 1)
            return WOPN_ERR_UNEXPECTED_ENDING;
        *cursor = file->is_drum;
        GO_FORWARD(1);
    }

    if(version > 1)
        /* Skip sounding delays are not part of single-instrument file
         * two sizes of uint16_t will be subtracted */
        ins_size = WOPN_INST_SIZE_V2 - (sizeof(uint16_t) * 2);
    else
        ins_size = WOPN_INST_SIZE_V1;

    if(length < ins_size)
        return WOPN_ERR_UNEXPECTED_ENDING;

    WOPN_writeInstrument(&file->inst, cursor, version, 0);
    GO_FORWARD(ins_size);

    return WOPN_ERR_OK;
#undef GO_FORWARD
}
