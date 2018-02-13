/*
** win32iface.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
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

#ifndef __WIN32IFACE_H
#define __WIN32IFACE_H

#include "hardware.h"


EXTERN_CVAR (Bool, vid_vsync)

class D3DTex;
class D3DPal;
struct FSoftwareRenderer;

class Win32Video : public IVideo
{
 public:
	Win32Video (int parm);
	~Win32Video ();

	bool InitD3D9();
	void InitDDraw();

	EDisplayType GetDisplayType () { return DISPLAY_Both; }
	void SetWindowedScale (float scale);

	DFrameBuffer *CreateFrameBuffer (int width, int height, bool bgra, bool fs, DFrameBuffer *old);

	void StartModeIterator (int bits, bool fs);
	bool NextMode (int *width, int *height, bool *letterbox);

	bool GoFullscreen (bool yes);
	void BlankForGDI ();

	void DumpAdapters ();
	void AddMode(int x, int y, int bits, int baseHeight, int doubling);

 private:
	struct ModeInfo
	{
		ModeInfo (int inX, int inY, int inBits, int inRealY, int inDoubling)
			: next (NULL),
			  width (inX),
			  height (inY),
			  bits (inBits),
			  realheight (inRealY),
			  doubling (inDoubling)
		{}

		ModeInfo *next;
		int width, height, bits;
		int realheight;
		int doubling;
	} *m_Modes;

	ModeInfo *m_IteratorMode;
	int m_IteratorBits;
	bool m_IteratorFS;
	bool m_IsFullscreen;
	unsigned int m_Adapter;

	void FreeModes ();

	void AddD3DModes (unsigned adapter);
	void AddLowResModes ();
	void AddLetterboxModes ();
	void ScaleModes (int doubling);

	friend class DDrawFB;
	friend class D3DFB;
};

class BaseWinFB : public DFrameBuffer
{
	typedef DFrameBuffer Super;
public:
	BaseWinFB(int width, int height, bool bgra) : DFrameBuffer(width, height, bgra), Windowed(true) {}

	bool IsFullscreen () { return !Windowed; }
	virtual void Blank () = 0;
	virtual bool PaintToWindow () = 0;
	virtual long/*HRESULT*/ GetHR () = 0;	// HRESULT is a long in Windows but this header should not be polluted with windows.h just for this single definition
	virtual void ScaleCoordsFromWindow(int16_t &x, int16_t &y);

protected:
	virtual bool CreateResources () = 0;
	virtual void ReleaseResources () = 0;
    virtual int GetTrueHeight() { return GetHeight(); }

	bool Windowed;

	friend class Win32Video;

	BaseWinFB() {}
};


#endif // __WIN32IFACE_H
