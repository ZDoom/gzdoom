#include "doomtype.h"
#include "doomdef.h"
#include "cmdlib.h"
#include "c_dispatch.h"
#include "c_bindings.h"

#include <math.h>
#include <stdlib.h>

/* These bindings are equivalent to the
 * original DOOM's keymappings plus
 * some new keys.
 */
char DefBindings[] =
	"bind ` toggleconsole; "			// <- This is new
	"bind 1 \"impulse 1\"; "
	"bind 2 \"impulse 2\"; "
	"bind 3 \"impulse 3\"; "
	"bind 4 \"impulse 4\"; "
	"bind 5 \"impulse 5\"; "
	"bind 6 \"impulse 6\"; "
	"bind 7 \"impulse 7\"; "
	"bind - sizedown; "
	"bind = sizeup; "
	"bind ctrl +attack; "
	"bind alt +strafe; "
	"bind shift +speed; "
	"bind space +use; "
	"bind rightarrow +right; "
	"bind leftarrow +left; "
	"bind uparrow +forward; "
	"bind downarrow +back; "
	"bind , +moveleft; "
	"bind . +moveright; "
	"bind mouse1 +attack; "
	"bind mouse2 +strafe; "
	"bind mouse3 +forward; "
	"bind mouse4 +speed; "				// <- So is this
	"bind joy1 +attack; "
	"bind joy2 +strafe; "
	"bind joy3 +speed; "
	"bind joy4 +use; "
	"bind capslock \"toggle cl_run\"; "	// <- This too
	"bind f1 menu_help; "
	"bind f2 menu_save; "
	"bind f3 menu_load; "
	"bind f4 menu_options; "			// <- Since we don't have a separate sound menu anymore
	"bind f5 menu_video; "				// <- Since we no longer have a detail level setting
	"bind f6 quicksave; "
	"bind f7 menu_endgame; "
	"bind f8 togglemessages; "
	"bind f9 quickload; "
	"bind f10 menu_quit; "
	"bind tab togglemap";

static const char *KeyNames[256+8+32] = {
	// This array is dependant on the particular keyboard input
	// codes generated in i_input.c. If they change there, they
	// also need to change here. In this case, we use the
	// DirectInput codes and assume a qwerty keyboard layout.
	// See <dinput.h> for the DIK_* codes

	NULL,  "escape","1",	"2",	"3",	"4",	"5",		"6",		//00
	"7",	"8",	"9",	"0",	"-",	"=",	"backspace","tab",		//08
	"q",	"w",	"e",	"r",	"t",	"y",	"u",		"i",		//10
	"o",	"p",	"[",	"]",	"enter","ctrl",	"a",		"s",		//18
	"d",	"f",	"g",	"h",	"j",	"k",	"l",		";",		//20
	"-",	"`",	"shift","\\",	"z",	"x",	"c",		"v",		//28
	"b",	"n",	"m",	",",	".",	"/",	NULL,		"kp*",		//30
	"alt",	"space","capslock","f1","f2",	"f3",	"f4",		"f5",		//38
	"f6",	"f7",	"f8",	"f9",	"f10",	"numlock","scroll",	"kp7",		//40
	"kp8",	"kp9",	"kp-",	"kp4",	"kp5",	"kp6",	"kp+",		"kp1",		//48
	"kp2",	"kp3",	"kp0",	"kp.",	NULL,	NULL,	NULL,		"f11",		//50
	"f12",	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		NULL,		//58
	NULL,	NULL,	NULL,	NULL,	"f13",	"f14",	"f15",		NULL,		//60
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		NULL,		//68
	"kana",	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		NULL,		//70
	NULL,	"convert",NULL,	"noconvert",NULL,"yen",	NULL,		NULL,		//78
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		NULL,		//80
	NULL,	NULL,	NULL,	NULL,	NULL,	"kp=",	NULL,		NULL,		//88
	"circumflex","@",":",	"_",	"kanji","stop",	"ax",		"unlabeled",//90
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		NULL,		//98
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		NULL,		//A0
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		NULL,		//A8
	NULL,	NULL,	NULL,	"kp,",	NULL,	"kp/",	NULL,		"sysrq",	//B0
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		NULL,		//B8
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		"home",		//C0
	"uparrow","pgup",NULL,	"leftarrow",NULL,"rightarrow",NULL,	"end",		//C8
	"downarrow","pgdn","ins","del",	NULL,	NULL,	NULL,		NULL,		//D0
	NULL,	NULL,	NULL,	"lwin",	"rwin",	"apps",	NULL,		NULL,		//D8
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		NULL,		//E0
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		NULL,		//E8
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		NULL,		//F0
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		"pause",	//F8

	// non-keyboard buttons that can be bound
	"mouse1",	"mouse2",	"mouse3",	"mouse4",		// DInput has four of these...
	NULL,		NULL,		NULL,		NULL,			// Space for double clicking if I feel like it
	"joy1",		"joy2",		"joy3",		"joy4",			// 32 joystick buttons!
	"joy5",		"joy6",		"joy7",		"joy8",
	"joy9",		"joy10",	"joy11",	"joy12",
	"joy13",	"joy14",	"joy15",	"joy16",
	"joy17",	"joy18",	"joy19",	"joy20",
	"joy21",	"joy22",	"joy23",	"joy24",
	"joy25",	"joy26",	"joy27",	"joy28",
	"joy29",	"joy30",	"joy31",	"joy32"
};

char *Bindings[256+8+32];

static int GetKeyFromName (const char *name)
{
	int i;

	// Names of the form #xxx are translated to key xxx automatically
	if (name[0] == '#' && name[1] != 0) {
		return atoi (name + 1);
	}

	// Otherwise, we scan the KeyNames[] array for a matching name
	for (i = 0; i < 256+8+32; i++) {
		if (KeyNames[i] && !stricmp (KeyNames[i], name))
			return i;
	}
	return 0;
}

static const char *KeyName (int key)
{
	static char name[5];

	if (KeyNames[key])
		return KeyNames[key];

	sprintf (name, "#%d", key);
	return name;
}

void Cmd_Unbindall (void *plyr, int argc, char **argv)
{
	int i;

	for (i = 0; i < 256+8+32; i++) {
		if (Bindings[i]) {
			free (Bindings[i]);
			Bindings[i] = NULL;
		}
	}
}

void Cmd_Unbind (void *plyr, int argc, char **argv)
{
	int i;

	if (argc > 1) {
		if (i = GetKeyFromName (argv[1])) {
			if (Bindings[i]) {
				free (Bindings[i]);
				Bindings[i] = NULL;
			}
		} else {
			Printf ("Unknown key \"%s\"\n", argv[1]);
			return;
		}

	}
}

void Cmd_Bind (void *plyr, int argc, char **argv)
{
	int i;

	if (argc > 1) {
		i = GetKeyFromName (argv[1]);
		if (!i) {
			Printf ("Unknown key \"%s\"\n", argv[1]);
			return;
		}
		if (argc == 2) {
			Printf ("\"%s\" = \"%s\"\n", argv[1], (Bindings[i] ? Bindings[i] : ""));
		} else {
			if (Bindings[i])
				free (Bindings[i]);

			Bindings[i] = copystring (argv[2]);
		}
	} else {
		Printf ("Current key bindings:\n");
		
		for (i = 0; i < 256+8+32; i++) {
			if (Bindings[i])
				Printf ("%s \"%s\"\n", KeyName (i), Bindings[i]);
		}
	}
}

void Cmd_BindDefaults (void *plyr, int argc, char **argv)
{
	AddCommandString (DefBindings);
}

boolean C_DoKey (int key, boolean up)
{
	if (Bindings[key]) {
		if (!up) {
			AddCommandString (Bindings[key]);
		} else {
			char *achar;
			const char *name = KeyName (key);
		
			achar = strchr (Bindings[key], '+');
			if (!achar)
				return false;

			if ((achar == Bindings[key]) || (*(achar - 1) <= ' ')) {
				*achar = '-';
				AddCommandString (Bindings[key]);
				*achar = '+';
			}
		}
		return true;
	}
	return false;
}

void C_ArchiveBindings (FILE *f)
{
	int i;

	fprintf (f, "unbindall\n");
	for (i = 0; i < 256+8+32; i++) {
		if (Bindings[i]) {
			fprintf (f, "bind \"%s\" \"%s\"\n", KeyName (i), Bindings[i]);
		}
	}
}

int C_GetKeysForCommand (char *cmd, int *first, int *second)
{
	int c, i;

	*first = *second = c = i = 0;

	while (i < 256+8+32 && c < 2) {
		if (Bindings[i] && !stricmp (cmd, Bindings[i])) {
			if (c++ == 0)
				*first = i;
			else
				*second = i;
		}
		i++;
	}
	return c;
}

void C_NameKeys (char *str, int first, int second)
{
	int c = 0;

	*str = 0;
	if (first) {
		c++;
		strcpy (str, KeyName (first));
		if (second)
			strcat (str, " or ");
	}

	if (second) {
		c++;
		strcat (str, KeyName (second));
	}

	if (!c)
		strcpy (str, "???");
}

void C_UnbindACommand (char *str)
{
	int i;

	for (i = 0; i < 256+8+32; i++) {
		if (Bindings[i] && !stricmp (str, Bindings[i])) {
			free (Bindings[i]);
			Bindings[i] = NULL;
		}
	}
}

void C_ChangeBinding (char *str, int newone)
{
	if (Bindings[newone])
		free (Bindings[newone]);

	Bindings[newone] = copystring (str);
}