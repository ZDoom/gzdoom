//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1999-2016 Randy Heit
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//


#include <string.h>
#include <ctype.h>
#include "doomdef.h"
#include "m_swap.h"
#include "hu_stuff.h"
#include "s_sound.h"
#include "g_game.h"
#include "st_stuff.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "d_player.h"
#include "v_text.h"
#include "d_gui.h"
#include "g_input.h"
#include "d_net.h"
#include "d_eventbase.h"
#include "sbar.h"
#include "v_video.h"
#include "utf8.h"
#include "gstrings.h"
#include "vm.h"
#include "c_buttons.h"
#include "d_buttons.h"
#include "v_draw.h"
#include "r_utility.h"

enum
{
	QUEUESIZE = 128
};


EXTERN_CVAR (Bool, sb_cooperative_enable)
EXTERN_CVAR (Bool, sb_deathmatch_enable)
EXTERN_CVAR (Bool, sb_teamdeathmatch_enable)
EXTERN_CVAR (Int, cl_showchat)

int active_con_scaletext();

// Public data

void CT_Init ();
void CT_Drawer ();
bool CT_Responder (event_t *ev);
void CT_PasteChat(const char *clip);

// Private data

static void CT_ClearChatMessage ();
static void CT_AddChar (int c);
static void CT_BackSpace ();
static void ShoveChatStr (const char *str, uint8_t who);
static bool DoSubstitution (FString &out, const char *in);

constexpr int MessageLimit = 2; // Clamp the amount of messages you can send in a brief period
constexpr uint64_t MessageThrottleTime = 1000u; // Time in ms that spam messages will be tracked.
constexpr uint64_t SpamCoolDown = 3000u;

static TArray<uint8_t> ChatQueue;
static uint64_t ChatThrottle = 0u;
static int ChatSpamCount = 0;
static uint64_t ChatCoolDown = 0u;	// Spam limiter

CVAR (Int, net_chatslowmode, 0, CVAR_SERVERINFO | CVAR_NOSAVE)
CVAR (String, chatmacro1, "I'm ready to kick butt!", CVAR_ARCHIVE)
CVAR (String, chatmacro2, "I'm OK.", CVAR_ARCHIVE)
CVAR (String, chatmacro3, "I'm not looking too good!", CVAR_ARCHIVE)
CVAR (String, chatmacro4, "Help!", CVAR_ARCHIVE)
CVAR (String, chatmacro5, "You suck!", CVAR_ARCHIVE)
CVAR (String, chatmacro6, "Next time, scumbag...", CVAR_ARCHIVE)
CVAR (String, chatmacro7, "Come here!", CVAR_ARCHIVE)
CVAR (String, chatmacro8, "I'll take care of it.", CVAR_ARCHIVE)
CVAR (String, chatmacro9, "Yes", CVAR_ARCHIVE)
CVAR (String, chatmacro0, "No", CVAR_ARCHIVE)

FStringCVarRef *chat_macros[10] =
{
	&chatmacro0,
	&chatmacro1,
	&chatmacro2,
	&chatmacro3,
	&chatmacro4,
	&chatmacro5,
	&chatmacro6,
	&chatmacro7,
	&chatmacro8,
	&chatmacro9
};

CVAR (Bool, chat_substitution, false, CVAR_ARCHIVE)

//===========================================================================
//
// CT_Init
//
// 	Initialize chat mode data
//===========================================================================

void CT_Init ()
{
	ChatQueue.Clear();
	chatmodeon = 0;
}

//===========================================================================
//
// CT_Stop
//
//===========================================================================

void CT_Stop ()
{
	chatmodeon = 0;
}

//===========================================================================
//
// CT_Responder
//
//===========================================================================

bool CT_Responder (event_t *ev)
{
	if (chatmodeon && ev->type == EV_GUI_Event)
	{
		if (ev->subtype == EV_GUI_KeyDown || ev->subtype == EV_GUI_KeyRepeat)
		{
			if (ev->data1 == '\r')
			{
				ChatQueue.Push(0);
				ShoveChatStr ((char *)ChatQueue.Data(), chatmodeon - 1);
				ChatQueue.Pop();
				CT_Stop ();
				return true;
			}
			else if (ev->data1 == GK_ESCAPE)
			{
				CT_Stop ();
				return true;
			}
			else if (ev->data1 == '\b')
			{
				CT_BackSpace ();
				return true;
			}
#ifdef __APPLE__
			else if (ev->data1 == 'C' && (ev->data3 & GKM_META))
#else // !__APPLE__
			else if (ev->data1 == 'C' && (ev->data3 & GKM_CTRL))
#endif // __APPLE__
			{
				ChatQueue.Push(0);
				I_PutInClipboard ((char *)ChatQueue.Data());
				ChatQueue.Pop();
				return true;
			}
#ifdef __APPLE__
			else if (ev->data1 == 'V' && (ev->data3 & GKM_META))
#else // !__APPLE__
			else if (ev->data1 == 'V' && (ev->data3 & GKM_CTRL))
#endif // __APPLE__
			{
				CT_PasteChat(I_GetFromClipboard(false).GetChars());
			}
		}
		else if (ev->subtype == EV_GUI_Char)
		{
			// send a macro
			if (ev->data2 && (ev->data1 >= '0' && ev->data1 <= '9'))
			{
				ShoveChatStr (*chat_macros[ev->data1 - '0'], chatmodeon - 1);
				CT_Stop ();
			}
			else
			{
				CT_AddChar (ev->data1);
			}
			return true;
		}
#ifdef __unix__
		else if (ev->subtype == EV_GUI_MButtonDown)
		{
			CT_PasteChat(I_GetFromClipboard(true).GetChars());
		}
#endif
	}

	return false;
}

//===========================================================================
//
// CT_PasteChat
//
//===========================================================================

void CT_PasteChat(const char *clip)
{
	if (clip != nullptr && *clip != '\0')
	{
		auto p = (const uint8_t *)clip;
		// Only paste the first line.
		while (auto chr = GetCharFromString(p))
		{
			if (chr == '\n' || chr == '\r' || chr == '\b')
			{
				break;
			}
			CT_AddChar (chr);
		}
	}
}

//===========================================================================
//
// CT_Drawer
//
//===========================================================================

void CT_Drawer (void)
{
	auto &vp = r_viewpoint;
	auto drawer = twod;
	FFont *displayfont = NewConsoleFont;

	if (players[consoleplayer].camera != NULL &&
		(buttonMap.ButtonDown(Button_ShowScores) ||
		 players[consoleplayer].camera->health <= 0 ||
		 SB_ForceActive))
	{
		bool skipit = false;
		if (gamestate == GS_CUTSCENE)
		{
			// todo: check for summary screen
		}
		if (!skipit) HU_DrawScores (consoleplayer, vp.TicFrac);
	}
	if (chatmodeon)
	{
		// [MK] allow the status bar to take over chat prompt drawing
		bool skip = false;

		if(StatusBar)
		{
			IFVIRTUALPTR(StatusBar, DBaseStatusBar, DrawChat)
			{
				FString txt = ChatQueue;
				VMValue params[] = { (DObject*)StatusBar, &txt };
				int rv;
				VMReturn ret(&rv);
				VMCall(func, params, countof(params), &ret, 1);
				if (!!rv) return;
			}
		}

		FStringf prompt("%s ", chatmodeon == 2 && deathmatch && teamplay ? GStrings.GetString("TXT_SAYTEAM") : GStrings.GetString("TXT_SAY"));
		int x, scalex, y, promptwidth;

		y = (viewactive || gamestate != GS_LEVEL) ? -displayfont->GetHeight()-2 : -displayfont->GetHeight() - 22;

		scalex = 1;
		int scale = active_con_scale(drawer);
		int screen_width = twod->GetWidth() / scale;
		int screen_height = twod->GetHeight() / scale;
		int st_y = StatusBar ? (StatusBar->GetTopOfStatusbar() / scale) : screen_height;

		y += ((twod->GetHeight() == viewheight && viewactive) || gamestate != GS_LEVEL) ? screen_height : st_y;

		promptwidth = displayfont->StringWidth (prompt) * scalex;
		x = displayfont->GetCharWidth (displayfont->GetCursor()) * scalex * 2 + promptwidth;

		FString printstr = ChatQueue;
		// figure out if the text is wider than the screen
		// if so, only draw the right-most portion of it.
		const uint8_t *textp = (const uint8_t*)printstr.GetChars();
		while(*textp)
		{
			auto textw = displayfont->StringWidth(textp);
			if (x + textw * scalex < screen_width) break;
			GetCharFromString(textp);
		}
		printstr += displayfont->GetCursor();

		DrawText(drawer, displayfont, CR_GREEN, 0, y, prompt.GetChars(), 
			DTA_VirtualWidth, screen_width, DTA_VirtualHeight, screen_height, DTA_KeepRatio, true, TAG_DONE);
		DrawText(drawer, displayfont, CR_GREY, promptwidth, y, printstr.GetChars(),
			DTA_VirtualWidth, screen_width, DTA_VirtualHeight, screen_height, DTA_KeepRatio, true, TAG_DONE);
	}
}

//===========================================================================
//
// CT_AddChar
//
//===========================================================================

static void CT_AddChar (int c)
{
	if (ChatQueue.Size() < QUEUESIZE-2)
	{
		int size;
		auto encode = MakeUTF8(c, &size);
		if (*encode)
		{
			for (int i = 0; i < size; i++)
			{
				ChatQueue.Push(encode[i]);
			}
		}
	}
}

//===========================================================================
//
// CT_BackSpace
//
// 	Backs up a space, when the user hits (obviously) backspace
//===========================================================================

static void CT_BackSpace ()
{
	if (ChatQueue.Size())
	{
		int endpos = ChatQueue.Size() - 1;
		while (endpos > 0 && ChatQueue[endpos] >= 0x80 && ChatQueue[endpos] < 0xc0) endpos--;
		ChatQueue.Clamp(endpos);
	}
}

//===========================================================================
//
// CT_ClearChatMessage
//
// 	Clears out the data for the chat message.
//===========================================================================

static void CT_ClearChatMessage ()
{
	ChatQueue.Clear();
}

//===========================================================================
//
// ShoveChatStr
//
// Sends the chat message across the network
//
//===========================================================================

static void ShoveChatStr (const char *str, uint8_t who)
{
	// Don't send empty messages
	if (str == NULL || str[0] == '\0')
		return;

	if (netgame)
	{
		const uint64_t time = I_msTime();
		if (time >= ChatThrottle)
		{
			ChatSpamCount = 0;
		}
		else if (++ChatSpamCount >= MessageLimit)
		{
			ChatSpamCount = 0;
			ChatCoolDown = time + SpamCoolDown;
		}

		ChatThrottle = time + MessageThrottleTime;
		if (net_chatslowmode > 0)
			ChatCoolDown = max<uint64_t>(time + net_chatslowmode * 1000, ChatCoolDown);
	}

	FString substBuff;

	if (str[0] == '/' &&
		(str[1] == 'm' || str[1] == 'M') &&
		(str[2] == 'e' || str[2] == 'E'))
	{ // This is a /me message
		str += 3;
		who |= 2;
	}

	Net_WriteInt8 (DEM_SAY);
	Net_WriteInt8 (who);

	if (chat_substitution && DoSubstitution (substBuff, str))
	{
		str = substBuff.GetChars();
	}

	Net_WriteString(CleanseString(const_cast<char*>(MakeUTF8(str))));

}

//===========================================================================
//
// DoSubstitution
//
// Replace certain special substrings with different values to reflect
// the player's current state.
//
//===========================================================================

static bool DoSubstitution (FString &out, const char *in)
{
	player_t *player = &players[consoleplayer];
	auto weapon = player->ReadyWeapon;
	auto ammo1 = weapon ? weapon->PointerVar<AActor>(NAME_Ammo1) : nullptr;
	auto ammo2 = weapon ? weapon->PointerVar<AActor>(NAME_Ammo2) : nullptr;
	const char *a, *b;

	a = in;
	out = "";
	while ( (b = strchr(a, '$')) )
	{
		out.AppendCStrPart(a, b - a);

		a = ++b;
		while (*b && isalpha(*b))
		{
			++b;
		}

		ptrdiff_t ByteLen = b - a;

		if (ByteLen == 6)
		{
			if (strnicmp(a, "health", 6) == 0)
			{
				out.AppendFormat("%d", player->health);
			}
			else if (strnicmp(a, "weapon", 6) == 0)
			{
				if (weapon == NULL)
				{
					out += "no weapon";
				}
				else
				{
					out += weapon->GetClass()->TypeName.GetChars();
				}
			}
		}
		else if (ByteLen == 5)
		{
			if (strnicmp(a, "armor", 5) == 0)
			{
				auto armor = player->mo->FindInventory(NAME_BasicArmor, true);
				out.AppendFormat("%d", armor != NULL ? armor->IntVar(NAME_Amount) : 0);
			}
		}
		else if (ByteLen == 9)
		{
			if (strnicmp(a, "ammocount", 9) == 0)
			{
				if (weapon == NULL)
				{
					out += '0';
				}
				else
				{
					out.AppendFormat("%d", ammo1 != NULL ? ammo1->IntVar(NAME_Amount) : 0);
					if (ammo2 != NULL)
					{
						out.AppendFormat("/%d", ammo2->IntVar(NAME_Amount));
					}
				}
			}
		}
		else if (ByteLen == 4)
		{
			if (strnicmp(a, "ammo", 4) == 0)
			{
				if (ammo1 == NULL)
				{
					out += "no ammo";
				}
				else
				{
					out.AppendFormat("%s", ammo1->GetClass()->TypeName.GetChars());
					if (ammo2 != NULL)
					{
						out.AppendFormat("/%s", ammo2->GetClass()->TypeName.GetChars());
					}
				}
			}
		}
		else if (ByteLen == 0)
		{
			out += '$';
			if (*b == '$')
			{
				b++;
			}
		}
		else
		{
			out += '$';
			out.AppendCStrPart(a, ByteLen);
		}
		a = b;
	}

	// Return false if no substitution was performed
	if (a == in)
	{
		return false;
	}

	out += a;
	return true;
}

CCMD (messagemode)
{
	if (menuactive != MENU_Off)
		return;

	const uint64_t time = I_msTime();
	if (ChatCoolDown > time)
	{
		Printf("You must wait %d more seconds before being able to chat again\n", int((ChatCoolDown - time) * 0.001));
		return;
	}

	if (multiplayer && deathmatch)
	{
		if (cl_showchat < CHAT_GLOBAL)
		{
			Printf("Global chat is currently disabled\n");
			return;
		}

		chatmodeon = 1;
	}
	else
	{
		if (cl_showchat < CHAT_TEAM_ONLY)
		{
			Printf("Team chat is currently disabled\n");
			return;
		}

		chatmodeon = 2;
	}

	buttonMap.ResetButtonStates();
	C_HideConsole ();
	CT_ClearChatMessage ();
}

CCMD (say)
{
	if (argv.argc() == 1)
	{
		Printf ("Usage: say <message>\n");
		return;
	}

	const uint64_t time = I_msTime();
	if (ChatCoolDown > time)
	{
		Printf("You must wait %d more seconds before being able to chat again\n", int((ChatCoolDown - time) * 0.001));
		return;
	}

	// If not in a DM lobby, route it to team chat instead (helps improve chat
	// filtering).
	if (multiplayer && deathmatch)
	{
		if (cl_showchat < CHAT_GLOBAL)
		{
			Printf("Global chat is currently disabled\n");
		}
		else
		{
			ShoveChatStr(argv[1], 0);
		}
	}
	else if (cl_showchat < CHAT_TEAM_ONLY)
	{
		Printf("Team chat is currently disabled\n");
	}
	else
	{
		ShoveChatStr(argv[1], 1);
	}
}

CCMD (messagemode2)
{
	if (menuactive != MENU_Off)
		return;

	const uint64_t time = I_msTime();
	if (ChatCoolDown > time)
	{
		Printf("You must wait %d more seconds before being able to chat again\n", int((ChatCoolDown - time) * 0.001));
		return;
	}

	if (multiplayer && deathmatch && !teamplay)
	{
		if (cl_showchat < CHAT_GLOBAL)
		{
			Printf("Global chat is currently disabled\n");
			return;
		}

		chatmodeon = 1;
	}
	else
	{
		if (cl_showchat < CHAT_TEAM_ONLY)
		{
			Printf("Team chat is currently disabled\n");
			return;
		}

		chatmodeon = 2;
	}

	buttonMap.ResetButtonStates();
	C_HideConsole();
	CT_ClearChatMessage();
}

CCMD (say_team)
{
	if (argv.argc() == 1)
	{
		Printf ("Usage: say_team <message>\n");
		return;
	}

	const uint64_t time = I_msTime();
	if (ChatCoolDown > time)
	{
		Printf("You must wait %d more seconds before being able to chat again\n", int((ChatCoolDown - time) * 0.001));
		return;
	}

	// If in a DM lobby, route it to global chat instead (helps
	// improve chat filtering).
	if (multiplayer && deathmatch && !teamplay)
	{
		if (cl_showchat < CHAT_GLOBAL)
		{
			Printf("Global chat is currently disabled\n");
		}
		else
		{
			ShoveChatStr(argv[1], 0);
		}
	}
	else if (cl_showchat < CHAT_TEAM_ONLY)
	{
		Printf("Team chat is currently disabled\n");
	}
	else
	{
		ShoveChatStr (argv[1], 1);
	}
}
