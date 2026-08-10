#pragma once

#include <stdint.h>

// On-screen touch controls: virtual joystick, look-drag and buttons.
// Platform backends feed finger events in 2D-canvas coordinates
// (i.e. after DFrameBuffer::ScaleCoordsFromWindow).

bool Touch_Enabled();
void Touch_SetAvailable();
void Touch_FingerDown(int64_t id, int x, int y);
void Touch_FingerMotion(int64_t id, int x, int y, float dxpix, float dypix);
void Touch_FingerUp(int64_t id);
void Touch_ReleaseAll();
void Touch_Draw(bool show);	// call every frame; draws only when show is true
