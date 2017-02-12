/*
** joystickmenu.cpp
** The joystick configuration menus
**
**---------------------------------------------------------------------------
** Copyright 2010 Christoph Oelckers
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

#include <float.h>

#include "menu/menu.h"
#include "c_dispatch.h"
#include "w_wad.h"
#include "sc_man.h"
#include "v_font.h"
#include "g_level.h"
#include "d_player.h"
#include "v_video.h"
#include "gi.h"
#include "i_system.h"
#include "c_bind.h"
#include "v_palette.h"
#include "d_event.h"
#include "d_gui.h"
#include "i_music.h"
#include "m_joy.h"

static TArray<IJoystickConfig *> Joysticks;
IJoystickConfig *SELECTED_JOYSTICK;

DEFINE_ACTION_FUNCTION(DMenu, GetCurrentJoystickConfig)
{
	ACTION_RETURN_POINTER(SELECTED_JOYSTICK);
}

DEFINE_ACTION_FUNCTION(IJoystickConfig, GetSensitivity)
{
	PARAM_SELF_STRUCT_PROLOGUE(IJoystickConfig);
	ACTION_RETURN_FLOAT(self->GetSensitivity());
}

DEFINE_ACTION_FUNCTION(IJoystickConfig, SetSensitivity)
{
	PARAM_SELF_STRUCT_PROLOGUE(IJoystickConfig);
	PARAM_FLOAT(sens);
	self->SetSensitivity((float)sens);
	return 0;
}

DEFINE_ACTION_FUNCTION(IJoystickConfig, GetAxisScale)
{
	PARAM_SELF_STRUCT_PROLOGUE(IJoystickConfig);
	PARAM_INT(axis);
	ACTION_RETURN_FLOAT(self->GetAxisScale(axis));
}

DEFINE_ACTION_FUNCTION(IJoystickConfig, SetAxisScale)
{
	PARAM_SELF_STRUCT_PROLOGUE(IJoystickConfig);
	PARAM_INT(axis);
	PARAM_FLOAT(sens);
	self->SetAxisScale(axis, (float)sens);
	return 0;
}

DEFINE_ACTION_FUNCTION(IJoystickConfig, GetAxisDeadZone)
{
	PARAM_SELF_STRUCT_PROLOGUE(IJoystickConfig);
	PARAM_INT(axis);
	ACTION_RETURN_FLOAT(self->GetAxisDeadZone(axis));
}

DEFINE_ACTION_FUNCTION(IJoystickConfig, SetAxisDeadZone)
{
	PARAM_SELF_STRUCT_PROLOGUE(IJoystickConfig);
	PARAM_INT(axis);
	PARAM_FLOAT(dz);
	self->SetAxisDeadZone(axis, (float)dz);
	return 0;
}

DEFINE_ACTION_FUNCTION(IJoystickConfig, GetAxisMap)
{
	PARAM_SELF_STRUCT_PROLOGUE(IJoystickConfig);
	PARAM_INT(axis);
	ACTION_RETURN_INT(self->GetAxisMap(axis));
}

DEFINE_ACTION_FUNCTION(IJoystickConfig, SetAxisMap)
{
	PARAM_SELF_STRUCT_PROLOGUE(IJoystickConfig);
	PARAM_INT(axis);
	PARAM_INT(map);
	self->SetAxisMap(axis, (EJoyAxis)map);
	return 0;
}


DOptionMenuDescriptor *UpdateJoystickConfigMenu(IJoystickConfig *joy);

class DJoystickConfigMenu : public DOptionMenu
{
	DECLARE_CLASS(DJoystickConfigMenu, DOptionMenu)
};

IMPLEMENT_CLASS(DJoystickConfigMenu, false, false)

/*=======================================
 *
 * Joystick Menu
 *
 *=======================================*/

DOptionMenuDescriptor *UpdateJoystickConfigMenu(IJoystickConfig *joy)
{
	DMenuDescriptor **desc = MenuDescriptors.CheckKey(NAME_JoystickConfigMenu);
	if (desc != NULL && (*desc)->IsKindOf(RUNTIME_CLASS(DOptionMenuDescriptor)))
	{
		DOptionMenuDescriptor *opt = (DOptionMenuDescriptor *)*desc;
		DMenuItemBase *it;
		opt->mItems.Clear();
		if (joy == NULL)
		{
			opt->mTitle = "Configure Controller";
			it = CreateOptionMenuItemStaticText("Invalid controller specified for menu", false);
			opt->mItems.Push(it);
		}
		else
		{
			opt->mTitle.Format("Configure %s", joy->GetName().GetChars());

			SELECTED_JOYSTICK = joy;

			it = CreateOptionMenuSliderJoySensitivity("Overall sensitivity", 0, 2, 0.1, 3);
			opt->mItems.Push(it);
			it = CreateOptionMenuItemStaticText(" ", false);
			opt->mItems.Push(it);

			if (joy->GetNumAxes() > 0)
			{
				it = CreateOptionMenuItemStaticText("Axis Configuration", true);
				opt->mItems.Push(it);

				for (int i = 0; i < joy->GetNumAxes(); ++i)
				{
					it = CreateOptionMenuItemStaticText(" ", false);
					opt->mItems.Push(it);

					it = CreateOptionMenuItemJoyMap(joy->GetAxisName(i), i, "JoyAxisMapNames", false);
					opt->mItems.Push(it);
					it = CreateOptionMenuSliderJoyScale("Overall sensitivity", i, 0, 4, 0.1, 3);
					opt->mItems.Push(it);
					it = CreateOptionMenuItemInverter("Invert", i, false);
					opt->mItems.Push(it);
					it = CreateOptionMenuSliderJoyDeadZone("Dead Zone", i, 0, 0.9, 0.05, 3);
					opt->mItems.Push(it);
				}
			}
			else
			{
				it = CreateOptionMenuItemStaticText("No configurable axes", false);
				opt->mItems.Push(it);
			}
		}
		for (auto &p : opt->mItems)
		{
			GC::WriteBarrier(p);
		}
		opt->mScrollPos = 0;
		opt->mSelectedItem = -1;
		opt->mIndent = 0;
		opt->mPosition = -25;
		opt->CalcIndent();
		return opt;
	}
	return NULL;
}



void UpdateJoystickMenu(IJoystickConfig *selected)
{
	DMenuDescriptor **desc = MenuDescriptors.CheckKey(NAME_JoystickOptions);
	if (desc != NULL && (*desc)->IsKindOf(RUNTIME_CLASS(DOptionMenuDescriptor)))
	{
		DOptionMenuDescriptor *opt = (DOptionMenuDescriptor *)*desc;
		DMenuItemBase *it;
		opt->mItems.Clear();

		int i;
		int itemnum = -1;

		I_GetJoysticks(Joysticks);
		if ((unsigned)itemnum >= Joysticks.Size())
		{
			itemnum = Joysticks.Size() - 1;
		}
		if (selected != NULL)
		{
			for (i = 0; (unsigned)i < Joysticks.Size(); ++i)
			{
				if (Joysticks[i] == selected)
				{
					itemnum = i;
					break;
				}
			}
		}

		// Todo: Block joystick for changing this one.
		it = CreateOptionMenuItemOption("Enable controller support", "use_joystick", "YesNo", NULL, false);
		opt->mItems.Push(it);
		#ifdef _WIN32
			it = CreateOptionMenuItemOption("Enable DirectInput controllers", "joy_dinput", "YesNo", NULL, false);
			opt->mItems.Push(it);
			it = CreateOptionMenuItemOption("Enable XInput controllers", "joy_xinput", "YesNo", NULL, false);
			opt->mItems.Push(it);
			it = CreateOptionMenuItemOption("Enable raw PlayStation 2 adapters", "joy_ps2raw", "YesNo", NULL, false);
			opt->mItems.Push(it);
		#endif

		it = CreateOptionMenuItemStaticText(" ", false);
		opt->mItems.Push(it);

		if (Joysticks.Size() == 0)
		{
			it = CreateOptionMenuItemStaticText("No controllers detected", false);
			opt->mItems.Push(it);
			if (!use_joystick)
			{
				it = CreateOptionMenuItemStaticText("Controller support must be", false);
				opt->mItems.Push(it);
				it = CreateOptionMenuItemStaticText("enabled to detect any", false);
				opt->mItems.Push(it);
			}
		}
		else
		{
			it = CreateOptionMenuItemStaticText("Configure controllers:", false);
			opt->mItems.Push(it);

			for (int i = 0; i < (int)Joysticks.Size(); ++i)
			{
				it = CreateOptionMenuItemJoyConfigMenu(Joysticks[i]->GetName(), Joysticks[i]);
				opt->mItems.Push(it);
				if (i == itemnum) opt->mSelectedItem = opt->mItems.Size();
			}
		}
		for (auto &p : opt->mItems)
		{
			GC::WriteBarrier(p);
		}
		if (opt->mSelectedItem >= (int)opt->mItems.Size())
		{
			opt->mSelectedItem = opt->mItems.Size() - 1;
		}

		opt->CalcIndent();

		// If the joystick config menu is open, close it if the device it's
		// open for is gone.
		for (i = 0; (unsigned)i < Joysticks.Size(); ++i)
		{
			if (Joysticks[i] == SELECTED_JOYSTICK)
			{
				break;
			}
		}
		if (i == (int)Joysticks.Size())
		{
			SELECTED_JOYSTICK = NULL;
			if (DMenu::CurrentMenu != NULL && DMenu::CurrentMenu->IsKindOf(RUNTIME_CLASS(DJoystickConfigMenu)))
			{
				DMenu::CurrentMenu->Close();
			}
		}
	}
}

