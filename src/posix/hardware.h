/*
** hardware.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef __HARDWARE_H__
#define __HARDWARE_H__

#include "i_video.h"
#include "v_video.h"

// Semaphores
#ifdef __APPLE__
#include <mach/mach_init.h>
#include <mach/semaphore.h>
#include <mach/task.h>
typedef semaphore_t Semaphore;
#define SEMAPHORE_WAIT(sem) \
	while(semaphore_wait(sem) != KERN_SUCCESS){}
#define SEMAPHORE_SIGNAL(sem) \
	semaphore_signal(sem);
#define SEMAPHORE_INIT(sem, shared, value) \
	semaphore_create(mach_task_self(), &sem, shared, value);
#else
#include <semaphore.h>
typedef sem_t Semaphore;
#define SEMAPHORE_WAIT(sem) \
	do { \
		while(sem_wait(&sem) != 0); \
		int semValue; \
		sem_getvalue(&sem, &semValue); \
		if(semValue < 1) \
			break; \
	} while(true);
#define SEMAPHORE_SIGNAL(sem) \
	sem_post(&sem);
#define SEMAPHORE_INIT(sem, shared, value) \
	sem_init(&sem, shared, value);
#endif

class IVideo
{
 public:
	virtual ~IVideo () {}

	virtual EDisplayType GetDisplayType () = 0;
	virtual void SetWindowedScale (float scale) = 0;

	virtual DFrameBuffer *CreateFrameBuffer (int width, int height, bool bgra, bool fs, DFrameBuffer *old) = 0;

	virtual void StartModeIterator (int bits, bool fs) = 0;
	virtual bool NextMode (int *width, int *height, bool *letterbox) = 0;

	virtual bool SetResolution (int width, int height, int bits);

	virtual void DumpAdapters();
};

void I_InitGraphics ();
void I_ShutdownGraphics ();

extern Semaphore FPSLimitSemaphore;
void I_SetFPSLimit(int limit);

extern IVideo *Video;

#endif	// __HARDWARE_H__
