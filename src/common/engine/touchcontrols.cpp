/*
** touchcontrols.cpp
**
** On-screen touch controls: virtual joystick, look-drag and buttons.
**
**---------------------------------------------------------------------------
** Copyright 2026 GZDoom Maintainers and Contributors
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

#include <math.h>

#include "touchcontrols.h"
#include "c_cvars.h"
#include "d_eventbase.h"
#include "i_time.h"
#include "keydef.h"
#include "tarray.h"
#include "v_draw.h"
#include "v_font.h"
#include "v_video.h"
#include "vectors.h"

CVAR(Int, touch_enable, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)		// -1 auto, 0 off, 1 on
CVAR(Float, touch_sensitivity, 1.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, touch_scale, 1.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, touch_alpha, 0.35f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

// Button positions: anchor (fraction of screen) + offset in button-size units.
static const struct BtnDef
{
	int key;
	const char* label;
	float ax, ay, ox, oy, w, h;
} ButtonDefs[] =
{
	{ KEY_TOUCH_FIRE,		"FIRE",	1.f, 1.f,		-2.4f, -2.5f,	1.0f, 1.0f },	// small; hold for automatic weapons, tap-anywhere also fires
	{ KEY_TOUCH_USE,		"USE",	1.f, 1.f,		-1.1f, -2.7f,	1.0f, 1.0f },
	{ KEY_TOUCH_JUMP,		"JMP",	1.f, 1.f,		-1.1f, -4.0f,	1.0f, 1.0f },
	{ KEY_TOUCH_CROUCH,		"CRCH",	1.f, 1.f,		-2.7f, -1.1f,	1.0f, 1.0f },
	{ KEY_TOUCH_WEAPNEXT,	"W+",	1.f, 0.45f,		-1.1f, -1.8f,	1.0f, 0.8f },
	{ KEY_TOUCH_WEAPPREV,	"W-",	1.f, 0.45f,		-1.1f, -0.8f,	1.0f, 0.8f },
	{ KEY_ESCAPE,			"ESC",	1.f, 0.f,		-1.1f, 0.15f,	1.0f, 0.7f },
};
constexpr int NumButtons = int(sizeof(ButtonDefs) / sizeof(ButtonDefs[0]));

enum
{
	ROLE_LOOK,
	ROLE_JOY,
	ROLE_BUTTON0,	// + button index
};

struct TouchFinger
{
	int64_t id;
	int role;
	float downx, downy;
	float x, y;
	uint64_t downMs;
	bool moved;
};

// Joystick direction bits, doubling as JoyKeys[] indices.
enum
{
	JOY_UP, JOY_DOWN, JOY_LEFT, JOY_RIGHT, JOY_RUN,
	NUM_JOYBITS
};
static const int JoyKeys[NUM_JOYBITS] =
{
	KEY_TOUCH_UP, KEY_TOUCH_DOWN, KEY_TOUCH_LEFT, KEY_TOUCH_RIGHT, KEY_TOUCH_RUN
};

static bool TouchAvailable;
static TArray<TouchFinger> Fingers;
static int JoyMask;						// currently pressed JOY_* bits
static int ButtonPressCount[NumButtons];
static float LookRemX, LookRemY;		// sub-pixel look remainders
static uint64_t TapFireReleaseMs;		// when to release a tap-triggered fire

// A quick tap on free space (no drag, no button) fires. The synthetic press is
// held briefly so it cannot begin and end within the same game tic.
constexpr uint64_t TAP_MAX_MS = 300;
constexpr uint64_t TAP_FIRE_HOLD_MS = 100;

bool Touch_Enabled()
{
	return touch_enable == 1 || (touch_enable == -1 && TouchAvailable);
}

void Touch_SetAvailable()
{
	TouchAvailable = true;
}

static void PostKey(int key, bool down)
{
	event_t ev = {};
	ev.type = down ? EV_KeyDown : EV_KeyUp;
	ev.data1 = (int16_t)key;
	D_PostEvent(&ev);
}

// One source of truth for all layout metrics.
static float ButtonSize()
{
	return min(0.11f * twod->GetHeight() * touch_scale, 0.13f * twod->GetWidth());
}

static void GetButtonRect(int i, float& x, float& y, float& w, float& h)
{
	const float S = ButtonSize();
	const BtnDef& b = ButtonDefs[i];
	x = b.ax * twod->GetWidth() + b.ox * S;
	y = b.ay * twod->GetHeight() + b.oy * S;
	w = b.w * S;
	h = b.h * S;
}

static int HitTestButtons(float x, float y)
{
	for (int i = 0; i < NumButtons; i++)
	{
		float bx, by, bw, bh;
		GetButtonRect(i, bx, by, bw, bh);
		if (x >= bx && x < bx + bw && y >= by && y < by + bh)
			return i;
	}
	return -1;
}

static TouchFinger* FindFinger(int64_t id)
{
	for (auto& f : Fingers)
	{
		if (f.id == id) return &f;
	}
	return nullptr;
}

static void SetJoyMask(int mask)
{
	int changed = mask ^ JoyMask;
	for (int bit = 0; bit < NUM_JOYBITS; bit++)
	{
		if (changed & (1 << bit))
			PostKey(JoyKeys[bit], !!(mask & (1 << bit)));
	}
	JoyMask = mask;
}

static void UpdateJoystick(const TouchFinger& f)
{
	const float R = 1.5f * ButtonSize();
	const float dead = 0.35f * R;
	const float dx = f.x - f.downx;
	const float dy = f.y - f.downy;

	int mask = 0;
	if (dy < -dead) mask |= 1 << JOY_UP;
	if (dy > dead) mask |= 1 << JOY_DOWN;
	if (dx < -dead) mask |= 1 << JOY_LEFT;
	if (dx > dead) mask |= 1 << JOY_RIGHT;
	if (dx * dx + dy * dy > (1.5f * R) * (1.5f * R)) mask |= 1 << JOY_RUN;
	// ponytail: no dead-zone hysteresis; add a +-10% band if edge jitter is felt in play
	SetJoyMask(mask);
}

static bool FireButtonHeld()
{
	for (int i = 0; i < NumButtons; i++)
	{
		if (ButtonDefs[i].key == KEY_TOUCH_FIRE && ButtonPressCount[i] > 0)
			return true;
	}
	return false;
}

void Touch_FingerDown(int64_t id, int x, int y)
{
	Touch_FingerUp(id);		// in case an up event was lost

	TouchFinger f = { id, ROLE_LOOK, (float)x, (float)y, (float)x, (float)y, I_msTime(), false };

	int btn = HitTestButtons((float)x, (float)y);
	if (btn >= 0)
	{
		f.role = ROLE_BUTTON0 + btn;
		if (ButtonPressCount[btn]++ == 0)
			PostKey(ButtonDefs[btn].key, true);
	}
	else if (x < twod->GetWidth() / 3)
	{
		bool haveJoy = false;
		for (auto& other : Fingers) haveJoy |= other.role == ROLE_JOY;
		if (!haveJoy) f.role = ROLE_JOY;	// auto-follow: joystick centers on the down point
	}
	Fingers.Push(f);
}

void Touch_FingerMotion(int64_t id, int x, int y, float dxpix, float dypix)
{
	TouchFinger* f = FindFinger(id);
	if (f == nullptr) return;

	f->x = (float)x;
	f->y = (float)y;

	float mdx = f->x - f->downx, mdy = f->y - f->downy;
	float slop = 0.25f * ButtonSize();
	if (mdx * mdx + mdy * mdy > slop * slop)
		f->moved = true;

	if (f->role == ROLE_JOY)
	{
		UpdateJoystick(*f);
	}
	else if (f->role == ROLE_LOOK)
	{
		LookRemX += dxpix * 2.f * touch_sensitivity;
		LookRemY += dypix * 2.f * touch_sensitivity;
		int ix = (int)LookRemX, iy = (int)LookRemY;
		if (ix || iy)
		{
			LookRemX -= ix;
			LookRemY -= iy;
			PostMouseMove(ix, iy);
		}
	}
}

void Touch_FingerUp(int64_t id)
{
	TouchFinger* f = FindFinger(id);
	if (f == nullptr) return;

	if (f->role == ROLE_JOY)
	{
		SetJoyMask(0);
	}
	else if (f->role == ROLE_LOOK)
	{
		if (!f->moved && I_msTime() - f->downMs <= TAP_MAX_MS && !FireButtonHeld())
		{
			if (TapFireReleaseMs == 0)
				PostKey(KEY_TOUCH_FIRE, true);
			TapFireReleaseMs = I_msTime() + TAP_FIRE_HOLD_MS;
		}
	}
	else if (f->role >= ROLE_BUTTON0)
	{
		int btn = f->role - ROLE_BUTTON0;
		if (--ButtonPressCount[btn] == 0)
			PostKey(ButtonDefs[btn].key, false);
	}
	Fingers.Delete((unsigned)(f - &Fingers[0]));
}

static void TapFireCheckRelease()
{
	if (TapFireReleaseMs != 0 && I_msTime() >= TapFireReleaseMs)
	{
		TapFireReleaseMs = 0;
		// while the fire button is held it owns the key; its release will post the keyup
		if (!FireButtonHeld())
			PostKey(KEY_TOUCH_FIRE, false);
	}
}

void Touch_ReleaseAll()
{
	// don't let a pending tap-fire release trigger a tap itself
	for (auto& f : Fingers) f.moved = true;
	while (Fingers.Size() > 0)
	{
		Touch_FingerUp(Fingers[0].id);
	}
	if (TapFireReleaseMs != 0)
	{
		TapFireReleaseMs = 0;
		PostKey(KEY_TOUCH_FIRE, false);
	}
	LookRemX = LookRemY = 0;
}

static void DrawRing(float cx, float cy, float r, uint8_t alpha)
{
	constexpr int SEG = 24;
	DVector2 prev(cx + r, cy);
	for (int i = 1; i <= SEG; i++)
	{
		double a = i * (2 * M_PI / SEG);
		DVector2 cur(cx + r * cos(a), cy + r * sin(a));
		twod->AddThickLine(prev, cur, 2, 0xFFFFFFFF, alpha);
		prev = cur;
	}
}

void Touch_Draw(bool show)
{
	TapFireCheckRelease();
	if (!show || !Touch_Enabled()) return;

	const float alpha = clamp<float>(touch_alpha, 0.f, 1.f);
	FFont* font = NewSmallFont;

	for (int i = 0; i < NumButtons; i++)
	{
		float x, y, w, h;
		GetButtonRect(i, x, y, w, h);
		bool pressed = ButtonPressCount[i] > 0;

		Dim(twod, pressed ? PalEntry(255, 255, 255) : PalEntry(0, 0, 0), alpha * (pressed ? 0.8f : 0.5f),
			(int)x, (int)y, (int)w, (int)h);
		DrawFrame(twod, PalEntry(255, 255, 255), (int)x, (int)y, (int)w, (int)h, 2);

		double ts = (h * 0.4) / font->GetHeight();
		DrawText(twod, font, CR_WHITE,
			x + (w - font->StringWidth(ButtonDefs[i].label) * ts) / 2,
			y + (h - font->GetHeight() * ts) / 2,
			ButtonDefs[i].label,
			DTA_ScaleX, ts, DTA_ScaleY, ts, DTA_Alpha, (double)alpha, TAG_DONE);
	}

	// Joystick: faint resting ring at the default spot, full rings while held.
	const float S = ButtonSize();
	const float R = 1.5f * S;
	const TouchFinger* joy = nullptr;
	for (auto& f : Fingers)
	{
		if (f.role == ROLE_JOY) joy = &f;
	}

	if (joy == nullptr)
	{
		DrawRing(2.2f * S, twod->GetHeight() - 2.2f * S, R, uint8_t(alpha * 128));
	}
	else
	{
		uint8_t a = uint8_t(alpha * 255);
		DrawRing(joy->downx, joy->downy, R, a);
		if (JoyMask & (1 << JOY_RUN)) DrawRing(joy->downx, joy->downy, 1.5f * R, a);

		// knob, clamped to the ring
		float dx = joy->x - joy->downx, dy = joy->y - joy->downy;
		float len = sqrtf(dx * dx + dy * dy);
		if (len > R) { dx *= R / len; dy *= R / len; }
		float kx = joy->downx + dx, ky = joy->downy + dy;
		Dim(twod, PalEntry(255, 255, 255), alpha * 0.8f, int(kx - 0.25f * S), int(ky - 0.25f * S), int(0.5f * S), int(0.5f * S));
	}
}
