#pragma once

#include "dobject.h"
class FGameTexture;
extern FGameTexture* CrosshairImage;
void ST_LoadCrosshair(int num, bool alwaysload);
void ST_UnloadCrosshair();
void ST_DrawCrosshair(int phealth, double xpos, double ypos, double scale);



class DStatusBarCore : public DObject
{
	DECLARE_CLASS(DStatusBarCore, DObject)
	
};