/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    w32_a.c

    Functions to play sound on the Windows audio driver (Windows 95/98/NT).

    Modified by Masanao Izumo <mo@goice.co.jp>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "timidity.h"

/*****************************************************************************************************************************/


/*****************************************************************************************************************************/

#include "instrum.h"
#include "playmidi.h"
#include "mblock.h"

namespace TimidityPlus
{
	extern void *safe_malloc(size_t count);

	extern CRITICAL_SECTION critSect;

	static int opt_wmme_device_id = -2;

	UINT uDeviceID;

#define NOT !

static int  open_output     (void); /* 0=success, 1=warning, -1=fatal error */
static void close_output    (void);
static int  output_data     (char * Data, int32_t Size);
static int  acntl           (int request, void * arg);

static void print_device_list(void);

#define DATA_BLOCK_SIZE     (4 * AUDIO_BUFFER_SIZE)
#define DATA_BLOCK_NUM      8
static int data_block_trunc_size;

struct MMBuffer
{
    int                 Number;
    int                 Prepared;   // Non-zero if this buffer has been prepared.

    HGLOBAL             hData;
    void *              Data;

    HGLOBAL             hHead;
    WAVEHDR *           Head;

    struct MMBuffer *   Next;
};

static struct MMBuffer *            Buffers;

static volatile struct MMBuffer *   FreeBuffers;
static volatile int                 NumBuffersInUse;

static HWAVEOUT                     hDevice;
static int                          BufferDelay;                    // in milliseconds

static const int                    AllowSynchronousWaveforms = 1;

/*****************************************************************************************************************************/

static void CALLBACK                OnPlaybackEvent (HWAVE hWave, UINT Msg, DWORD_PTR UserData, DWORD_PTR Param1, DWORD_PTR Param2);
static void                         BufferPoolReset (void);
static struct MMBuffer *            GetBuffer       ();
static void                         PutBuffer       (struct MMBuffer *);
static const char *                 MMErrorMessage  (MMRESULT Result);
static void                         WaitForBuffer   (int WaitForAllBuffers);

/*****************************************************************************************************************************/

static int detect(void);

#define dpm w32_play_mode

PlayMode dpm =
{
    44100,
    PE_16BIT,
    PF_PCM_STREAM|PF_BUFF_FRAGM_OPT,
    -1,
    {32},
    "Windows audio driver", 'd',
    NULL,
    open_output,
    close_output,
    output_data,
    acntl,
	detect
};

/*****************************************************************************************************************************/
CRITICAL_SECTION critSect;


static int open_output(void)
{
    int             i;
    int             j;
    int             IsMono;
    WAVEFORMATEX    wf;
    WAVEOUTCAPS     woc;
    MMRESULT        Result;
    UINT            DeviceID;
	int ret;

	InitializeCriticalSection(&critSect);

	if( dpm.name != NULL)
		ret = sscanf(dpm.name, "%d", &opt_wmme_device_id);
	if ( dpm.name == NULL || ret == 0 || ret == EOF)
		opt_wmme_device_id = -2;
	
	if (opt_wmme_device_id == -1){
		print_device_list();
		return -1;
	}

/** Check if there is at least one audio device. **/

    if (waveOutGetNumDevs() == 0)
    {
        //ctl_cmsg (CMSG_ERROR, VERB_NORMAL, "No audio devices present!");
        return -1;
    }

/** They can't mean these. **/

	dpm.encoding = PE_16BIT;

    IsMono  = false;

    memset(&wf, 0, sizeof(wf));

    wf.wFormatTag     = WAVE_FORMAT_PCM;
    wf.nChannels      = 2;
    wf.nSamplesPerSec = dpm.rate;

    i = dpm.rate;
    j = 4;
    i *= j;
    data_block_trunc_size = DATA_BLOCK_SIZE - (DATA_BLOCK_SIZE % j);

    wf.nAvgBytesPerSec = i;
    wf.nBlockAlign     = j;
	wf.wBitsPerSample  = 16;
    wf.cbSize          = sizeof(WAVEFORMAT);

/** Open the device. **/

    { CHAR  b[256]; wsprintf(b, "Opening device...\n"); OutputDebugString(b); }

		hDevice = 0;

	if (opt_wmme_device_id == -2){
		uDeviceID = WAVE_MAPPER;
    }else{
    	uDeviceID= (UINT)opt_wmme_device_id;
	}

    if (AllowSynchronousWaveforms)
        Result = waveOutOpen(&hDevice, uDeviceID, (LPWAVEFORMATEX) &wf, (DWORD_PTR) OnPlaybackEvent, 0, CALLBACK_FUNCTION | WAVE_ALLOWSYNC);
    else
        Result = waveOutOpen(&hDevice, uDeviceID, (LPWAVEFORMATEX) &wf, (DWORD_PTR) OnPlaybackEvent, 0, CALLBACK_FUNCTION);

    if (Result)
    {
        return -1;
    }
    else
        { CHAR  b[256]; wsprintf(b, "Device opened.\n"); OutputDebugString(b); }

/** Get the device ID. **/

    DeviceID = 0;
    waveOutGetID(hDevice, &DeviceID);

/** Get the device capabilities. **/

    memset(&woc, 0, sizeof(WAVEOUTCAPS));
    Result = waveOutGetDevCaps(DeviceID, &woc, sizeof(WAVEOUTCAPS));

    //ctl_cmsg(CMSG_INFO, VERB_DEBUG, "Device ID: %d",              DeviceID);
    //ctl_cmsg(CMSG_INFO, VERB_DEBUG, "Manufacture ID: %d",         woc.wMid);
    //ctl_cmsg(CMSG_INFO, VERB_DEBUG, "Product ID: %d",             woc.wPid);
    //ctl_cmsg(CMSG_INFO, VERB_DEBUG, "Driver version: %d",         woc.vDriverVersion);
    //ctl_cmsg(CMSG_INFO, VERB_DEBUG, "Product name: %s",           woc.szPname);
    //ctl_cmsg(CMSG_INFO, VERB_DEBUG, "Formats supported: 0x%08X",  woc.dwFormats);
    //ctl_cmsg(CMSG_INFO, VERB_DEBUG, "Max. channels: %d",          woc.wChannels);
    //ctl_cmsg(CMSG_INFO, VERB_DEBUG, "Supported features: 0x%08X", woc.dwSupport);

/** Calculate the buffer delay. **/

    BufferDelay = AUDIO_BUFFER_SIZE * 4;

    BufferDelay = (BufferDelay * 1000) / dpm.rate;

/** Create the buffer pool. **/

    Buffers = (struct MMBuffer *) safe_malloc(DATA_BLOCK_NUM * sizeof(struct MMBuffer));

    for (i = 0; i < DATA_BLOCK_NUM; i++)
    {
        struct MMBuffer *   b;

        b = &Buffers[i];

        b->hData = GlobalAlloc(GMEM_ZEROINIT, DATA_BLOCK_SIZE);
        b->Data  = (WAVEHDR*)GlobalLock (b->hData);
        b->hHead = GlobalAlloc(GMEM_ZEROINIT, sizeof(WAVEHDR));
        b->Head  = (WAVEHDR*)GlobalLock (b->hHead);
    }

    BufferPoolReset();

/** Set the file descriptor. **/

    dpm.fd = 0;

    return 0;
}

/*****************************************************************************************************************************/

static void close_output(void)
{
    int i;

    if (dpm.fd != -1)
    {
        WaitForBuffer(1);

        { CHAR  b[256]; wsprintf(b, "Closing device...\n"); OutputDebugString(b); }

        waveOutReset(hDevice);
        waveOutClose(hDevice);

        { CHAR  b[256]; wsprintf(b, "Device closed.\n"); OutputDebugString(b); }

    /** Free all buffers. **/

        for (i = 0; i < DATA_BLOCK_NUM; i++)
        {
            struct MMBuffer *   block;

            block = &Buffers[i];

            GlobalUnlock(block->hHead);
            GlobalFree  (block->hHead);

            GlobalUnlock(block->hData);
            GlobalFree  (block->hData);
        }

        free(Buffers);

    /** Reset the file descriptor. **/

        dpm.fd = -1;
    }
}

static int detect(void)
{
	if (waveOutGetNumDevs() == 0) {return 0;}	/* not found */
	return 1;	/* found */
}


/*****************************************************************************************************************************/

#ifdef OutputDebugString
#undef OutputDebugString
#endif

void OutputDebugString(LPSTR sre)
{
}

static int output_data(char * Data, int32_t Size)
{
    char *  d;
    int32_t   s;

    d = Data;
    s = Size;

    while (s > 0)
    {
        int32_t               n;
        struct MMBuffer *   b;

        MMRESULT            Result;
        LPWAVEHDR           wh;

        if ((b = GetBuffer()) == NULL)
        {
            WaitForBuffer(0);
            continue;
        }

        if (s <= data_block_trunc_size)
            n = s;
        else
            n = data_block_trunc_size;

        CopyMemory(b->Data, d, n);

        wh = b->Head;

        wh->dwBufferLength = n;
        wh->lpData         = (LPSTR)b->Data;
        wh->dwUser         = b->Number;

    /** Prepare the buffer. **/

        { CHAR  b[256]; wsprintf(b, "%2d: Preparing buffer %d...\n", NumBuffersInUse, wh->dwUser); OutputDebugString(b); }

        Result = waveOutPrepareHeader(hDevice, wh, sizeof(WAVEHDR));

        if (Result)
        {
            { CHAR  b[256]; wsprintf(b, "%2d: Buffer preparation failed.\n", NumBuffersInUse); OutputDebugString(b); }

            //ctl_cmsg (CMSG_ERROR, VERB_NORMAL, "waveOutPrepareHeader(): %s", MMErrorMessage(Result));
            return -1;
        }
        else
            { CHAR  b[256]; wsprintf(b, "%2d: Buffer %d prepared.\n", NumBuffersInUse, wh->dwUser); OutputDebugString(b); }

        b->Prepared = 1;

    /** Queue the buffer. **/

        { CHAR  b[256]; wsprintf(b, "%2d: Queueing buffer %d...\n", NumBuffersInUse, wh->dwUser); OutputDebugString(b); }

        Result = waveOutWrite(hDevice, wh, sizeof(WAVEHDR));

        if (Result)
        {
            { CHAR  b[256]; wsprintf(b, "%2d: Buffer queueing failed.\n", NumBuffersInUse); OutputDebugString(b); }

            //ctl_cmsg(CMSG_ERROR, VERB_NORMAL, "waveOutWrite(): %s", MMErrorMessage(Result));
            return -1;
        }
        else
            { CHAR  b[256]; wsprintf(b, "%2d: Buffer %d queued.\n", NumBuffersInUse, wh->dwUser); OutputDebugString(b); }

        d += n;
        s -= n;
    }

    return 0;
}

/*****************************************************************************************************************************/

static int acntl(int request, void *arg)
{
  static char dummy_sounds[4*AUDIO_BUFFER_SIZE];

    switch(request)
    {
        case PM_REQ_DISCARD:
        {
            { CHAR  b[256]; wsprintf(b, "Resetting audio device.\n"); OutputDebugString(b); }

            waveOutReset(hDevice);
	    close_output();
	    open_output();

            { CHAR  b[256]; wsprintf(b, "Audio device reset.\n"); OutputDebugString(b); }

            return 0;
        }

        case PM_REQ_FLUSH:
        {
	    close_output();
	    open_output();
            return 0;
        }
    }

    return -1;
}

/*****************************************************************************************************************************/

static void CALLBACK OnPlaybackEvent(HWAVE hWave, UINT Msg, DWORD_PTR UserData, DWORD_PTR Param1, DWORD_PTR Param2)
{
    //ctl_cmsg(CMSG_INFO, VERB_DEBUG, "Msg: 0x%08X, Num. buffers in use: %d", Msg, NumBuffersInUse);

    switch (Msg)
    {
        case WOM_OPEN:
            { CHAR  b[256]; wsprintf(b, "%2d: Device opened.\n", NumBuffersInUse); OutputDebugString(b); }
            break;

        case WOM_CLOSE:
            { CHAR  b[256]; wsprintf(b, "%2d: Device closed.\n", NumBuffersInUse); OutputDebugString(b); }
            break;

        case WOM_DONE:
        {
            WAVEHDR *   wh;

            EnterCriticalSection(&critSect);

            wh = (WAVEHDR *) Param1;

/* It's not safe to do this here. Read the remarks of waveOutProc() in the SDK on which functions are safe to call.
            if (NOT Queueing)
            {
                { CHAR  b[256]; wsprintf(b, "%2d: Dequeueing buffer %d...\n", NumBuffersInUse, wh->dwUser); OutputDebugString(b); }

                waveOutUnprepareHeader(hDevice, wh, sizeof(WAVEHDR));

                { CHAR  b[256]; wsprintf(b, "%2d: Buffer %d dequeued.\n",     NumBuffersInUse, wh->dwUser); OutputDebugString(b); }
            }
            else
                { CHAR  b[256]; wsprintf(b, "%2d: *** Buffer %d not dequeued! ***\n", NumBuffersInUse, wh->dwUser); OutputDebugString(b); }
 */
            PutBuffer(&Buffers[wh->dwUser]);

            LeaveCriticalSection(&critSect);

            break;
        }

        default:
        {
            CHAR    b[256];

            wsprintf(b, "%2d: Unknown play back event 0x%08X.\n", NumBuffersInUse, Msg);
            OutputDebugString(b);
        }
    }
}

/*****************************************************************************************************************************/

#define DIM(a) sizeof(a) / sizeof(a[0])

static const char * mmsyserr_code_string[] =
{
    "no error",
    "unspecified error",
    "device ID out of range",
    "driver failed enable",
    "device already allocated",
    "device handle is invalid",
    "no device driver present",
    "memory allocation error",
    "function isn't supported",
    "error value out of range",
    "invalid flag passed",
    "invalid parameter passed",
    "handle being used",
};

static const char * waverr_code_sring[] =
{
    "unsupported wave format",
    "still something playing",
    "header not prepared",
    "device is synchronous",
};

static const char * MMErrorMessage(MMRESULT ErrorCode)
{
    static char s[32];

    if (ErrorCode >= WAVERR_BASE)
    {
        ErrorCode -= WAVERR_BASE;

        if (ErrorCode > DIM(waverr_code_sring))
        {
            wsprintf(s, "Unknown wave error %d", ErrorCode);
            return s;
        }
        else
            return waverr_code_sring[ErrorCode];
    }
    else
    if (ErrorCode > DIM(mmsyserr_code_string))
    {
        wsprintf(s, "Unknown multimedia error %d", ErrorCode);
        return s;
    }
    else
        return mmsyserr_code_string[ErrorCode];
}

#undef DIM

/*****************************************************************************************************************************/

static void BufferPoolReset(void)
{
    int i;

    { CHAR  b[256]; wsprintf(b, "Resetting buffer pool...\n"); OutputDebugString(b); }

    Buffers[0].Number   = 0;
    Buffers[0].Prepared = 0;
    Buffers[0].Next     = &Buffers[1];

    for (i = 1; i < DATA_BLOCK_NUM - 1; i++)
    {
        Buffers[i].Number   = i;
        Buffers[i].Prepared = 0;
        Buffers[i].Next     = &Buffers[i + 1];
    }

    Buffers[i].Number   = i;
    Buffers[i].Prepared = 0;
    Buffers[i].Next     = NULL;

    FreeBuffers     = &Buffers[0];
    NumBuffersInUse = 0;

    { CHAR  b[256]; wsprintf(b, "Buffer pool reset.\n", NumBuffersInUse); OutputDebugString(b); }
}

/*****************************************************************************************************************************/

static struct MMBuffer * GetBuffer()
{
    struct MMBuffer *   b;

    { CHAR  b[256]; wsprintf(b, "%2d: Getting buffer...\n", NumBuffersInUse); OutputDebugString(b); }

    EnterCriticalSection(&critSect);

    if (FreeBuffers)
    {
    	b           = (struct MMBuffer *)FreeBuffers;
        FreeBuffers = FreeBuffers->Next;
        NumBuffersInUse++;

    /** If this buffer is still prepared we can safely unprepare it because we got it from the free buffer list. **/

        if (b->Prepared)
        {
            waveOutUnprepareHeader(hDevice, (LPWAVEHDR) b->Head, sizeof(WAVEHDR));

            b->Prepared = 0;
        }

        b->Next     = NULL;
    }
    else
        b = NULL;

    LeaveCriticalSection(&critSect);

    { CHAR  b[256]; wsprintf(b, "%2d: Got buffer.\n", NumBuffersInUse); OutputDebugString(b); }

    return b;
}

/*****************************************************************************************************************************/

static void PutBuffer(struct MMBuffer * b)
{
    { CHAR  b[256]; wsprintf(b, "%2d: Putting buffer...\n", NumBuffersInUse); OutputDebugString(b); }

    b->Next     = (struct MMBuffer *)FreeBuffers;
    FreeBuffers = b;
    NumBuffersInUse--;

    { CHAR  b[256]; wsprintf(b, "%2d: Buffer put.\n", NumBuffersInUse); OutputDebugString(b); }
}

/*****************************************************************************************************************************/

static void WaitForBuffer(int WaitForAllBuffers)
{
  int numbuf;

    if (WaitForAllBuffers)
    {
        { CHAR  b[256]; wsprintf(b, "%2d: Waiting for all buffers to be dequeued...\n", NumBuffersInUse); OutputDebugString(b); }

	while (1) {
	  EnterCriticalSection(&critSect);
	  numbuf = NumBuffersInUse;
	  if (numbuf) {
            LeaveCriticalSection(&critSect);
	    Sleep(BufferDelay);
	    continue;
	  }
	  break;
	}
	LeaveCriticalSection(&critSect);


//        while (NumBuffersInUse)
//            Sleep(BufferDelay);

        { CHAR  b[256]; wsprintf(b, "%2d: All buffers dequeued.\n", NumBuffersInUse); OutputDebugString(b); }

        BufferPoolReset();
    }
    else
    {
        { CHAR  b[256]; wsprintf(b, "%2d: Waiting %dms...\n", NumBuffersInUse, BufferDelay); OutputDebugString(b); }

        Sleep(BufferDelay);

        { CHAR  b[256]; wsprintf(b, "%2d: Wait finished.\n", NumBuffersInUse); OutputDebugString(b); }
    }
}

/*****************************************************************************************************************************/

#define DEVLIST_MAX 20
static void print_device_list(void){
	UINT num;
	int i, list_num;
	WAVEOUTCAPS woc;
	typedef struct tag_DEVICELIST{
		int  deviceID;
		char name[256];
	} DEVICELIST;
	DEVICELIST device[DEVLIST_MAX];
	num = waveOutGetNumDevs();
	list_num=0;
	for(i = 0 ; i < (int)num  && i < DEVLIST_MAX ; i++){
		if (MMSYSERR_NOERROR == waveOutGetDevCaps((UINT)i, &woc, sizeof(woc)) ){
			device[list_num].deviceID=i;
			strcpy(device[list_num].name, woc.szPname);
			list_num++;
		}
	}
	for(i=0;i<list_num;i++){
		//ctl_cmsg(CMSG_ERROR, VERB_NORMAL, "%2d %s", device[i].deviceID, device[i].name);
	}
}
}