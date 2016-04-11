// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//
//-----------------------------------------------------------------------------


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fnmatch.h>
#include <unistd.h>

#include <stdarg.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#ifndef NO_GTK
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#endif

#include "doomerrors.h"
#include <math.h>

#include "doomtype.h"
#include "doomstat.h"
#include "version.h"
#include "doomdef.h"
#include "cmdlib.h"
#include "m_argv.h"
#include "m_misc.h"
#include "i_video.h"
#include "i_sound.h"
#include "i_music.h"
#include "x86.h"

#include "d_main.h"
#include "d_net.h"
#include "g_game.h"
#include "i_system.h"
#include "c_dispatch.h"
#include "templates.h"
#include "v_palette.h"
#include "textures.h"
#include "bitmap.h"

#include "stats.h"
#include "hardware.h"
#include "gameconfigfile.h"

#include "m_fixed.h"
#include "g_level.h"

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#endif // __APPLE__

EXTERN_CVAR (String, language)

extern "C"
{
	double		SecondsPerCycle = 1e-8;
	double		CyclesPerSecond = 1e8;
}

#ifndef NO_GTK
extern bool GtkAvailable;
#elif defined(__APPLE__)
int I_PickIWad_Cocoa (WadStuff *wads, int numwads, bool showwin, int defaultiwad);
#endif

DWORD LanguageIDs[4];
	
int (*I_GetTime) (bool saveMS);
int (*I_WaitForTic) (int);
void (*I_FreezeTime) (bool frozen);

void I_Tactile (int /*on*/, int /*off*/, int /*total*/)
{
}

ticcmd_t emptycmd;
ticcmd_t *I_BaseTiccmd(void)
{
    return &emptycmd;
}

void I_BeginRead(void)
{
}

void I_EndRead(void)
{
}


void I_WaitVBL (int count)
{
    // I_WaitVBL is never used to actually synchronize to the
    // vertical blank. Instead, it's used for delay purposes.
    usleep (1000000 * count / 70);
}

//
// SetLanguageIDs
//
void SetLanguageIDs ()
{
	size_t langlen = strlen(language);

	DWORD lang = (langlen < 2 || langlen > 3) ?
		MAKE_ID('e','n','u','\0') :
		MAKE_ID(language[0],language[1],language[2],'\0');

	LanguageIDs[3] = LanguageIDs[2] = LanguageIDs[1] = LanguageIDs[0] = lang;
}

void I_InitTimer ();
void I_ShutdownTimer ();

//
// I_Init
//
void I_Init (void)
{
	CheckCPUID (&CPU);
	DumpCPUInfo (&CPU);

	atterm (I_ShutdownSound);
    I_InitSound ();
	I_InitTimer ();
}

//
// I_Quit
//
static int has_exited;

void I_Quit (void)
{
    has_exited = 1;		/* Prevent infinitely recursive exits -- killough */

    if (demorecording)
		G_CheckDemoStatus();

	C_DeinitConsole();

	I_ShutdownTimer();
}


//
// I_Error
//
extern FILE *Logfile;
bool gameisdead;

#ifdef __APPLE__
void Mac_I_FatalError(const char* errortext);
#endif

void I_FatalError (const char *error, ...)
{
    static bool alreadyThrown = false;
    gameisdead = true;

    if (!alreadyThrown)		// ignore all but the first message -- killough
    {
		alreadyThrown = true;
		char errortext[MAX_ERRORTEXT];
		int index;
		va_list argptr;
		va_start (argptr, error);
		index = vsnprintf (errortext, MAX_ERRORTEXT, error, argptr);
		va_end (argptr);

#ifdef __APPLE__
		Mac_I_FatalError(errortext);
#endif // __APPLE__		
		
		// Record error to log (if logging)
		if (Logfile)
		{
			fprintf (Logfile, "\n**** DIED WITH FATAL ERROR:\n%s\n", errortext);
			fflush (Logfile);
		}
//		throw CFatalError (errortext);
		fprintf (stderr, "%s\n", errortext);
		exit (-1);
    }

    if (!has_exited)	// If it hasn't exited yet, exit now -- killough
    {
		has_exited = 1;	// Prevent infinitely recursive exits -- killough
		exit(-1);
    }
}

void I_Error (const char *error, ...)
{
    va_list argptr;
    char errortext[MAX_ERRORTEXT];

    va_start (argptr, error);
    vsprintf (errortext, error, argptr);
    va_end (argptr);

    throw CRecoverableError (errortext);
}

void I_SetIWADInfo ()
{
}

void I_DebugPrint(const char *cp)
{
}

void I_PrintStr (const char *cp)
{
	// Strip out any color escape sequences before writing to the log file
	char * copy = new char[strlen(cp)+1];
	const char * srcp = cp;
	char * dstp = copy;

	while (*srcp != 0)
	{
		if (*srcp!=0x1c && *srcp!=0x1d && *srcp!=0x1e && *srcp!=0x1f)
		{
			*dstp++=*srcp++;
		}
		else
		{
			if (srcp[1]!=0) srcp+=2;
			else break;
		}
	}
	*dstp=0;

	fputs (copy, stdout);
	delete [] copy;
	fflush (stdout);
}

#ifndef NO_GTK
// GtkTreeViews eats return keys. I want this to be like a Windows listbox
// where pressing Return can still activate the default button.
gint AllowDefault(GtkWidget *widget, GdkEventKey *event, gpointer func_data)
{
	if (event->type == GDK_KEY_PRESS && event->keyval == GDK_Return)
	{
		gtk_window_activate_default (GTK_WINDOW(func_data));
	}
	return FALSE;
}

// Double-clicking an entry in the list is the same as pressing OK.
gint DoubleClickChecker(GtkWidget *widget, GdkEventButton *event, gpointer func_data)
{
	if (event->type == GDK_2BUTTON_PRESS)
	{
		*(int *)func_data = 1;
		gtk_main_quit();
	}
	return FALSE;
}

// When the user presses escape, that should be the same as canceling the dialog.
gint CheckEscape (GtkWidget *widget, GdkEventKey *event, gpointer func_data)
{
	if (event->type == GDK_KEY_PRESS && event->keyval == GDK_Escape)
	{
		gtk_main_quit();
	}
	return FALSE;
}

void ClickedOK(GtkButton *button, gpointer func_data)
{
	*(int *)func_data = 1;
	gtk_main_quit();
}

EXTERN_CVAR (Bool, queryiwad);

int I_PickIWad_Gtk (WadStuff *wads, int numwads, bool showwin, int defaultiwad)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *bbox;
	GtkWidget *widget;
	GtkWidget *tree;
	GtkWidget *check;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkTreeIter iter, defiter;
	int close_style = 0;
	int i;
	char caption[100];

	// Create the dialog window.
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	mysnprintf(caption, countof(caption), GAMESIG " %s: Select an IWAD to use", GetVersionString());
	gtk_window_set_title (GTK_WINDOW(window), caption);
	gtk_window_set_position (GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_container_set_border_width (GTK_CONTAINER(window), 10);
	g_signal_connect (window, "delete_event", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect (window, "key_press_event", G_CALLBACK(CheckEscape), NULL);

	// Create the vbox container.
	vbox = gtk_vbox_new (FALSE, 10);
	gtk_container_add (GTK_CONTAINER(window), vbox);

	// Create the top label.
	widget = gtk_label_new (GAMENAME " found more than one IWAD\nSelect from the list below to determine which one to use:");
	gtk_box_pack_start (GTK_BOX(vbox), widget, false, false, 0);
	gtk_misc_set_alignment (GTK_MISC(widget), 0, 0);

	// Create a list store with all the found IWADs.
	store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
	for (i = 0; i < numwads; ++i)
	{
		const char *filepart = strrchr (wads[i].Path, '/');
		if (filepart == NULL)
			filepart = wads[i].Path;
		else
			filepart++;
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
			0, filepart,
			1, wads[i].Name.GetChars(),
			2, i,
			-1);
		if (i == defaultiwad)
		{
			defiter = iter;
		}
	}

	// Create the tree view control to show the list.
	tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL(store));
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("IWAD", renderer, "text", 0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(tree), column);
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Game", renderer, "text", 1, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(tree), column);
	gtk_box_pack_start (GTK_BOX(vbox), GTK_WIDGET(tree), true, true, 0);
	g_signal_connect(G_OBJECT(tree), "button_press_event", G_CALLBACK(DoubleClickChecker), &close_style);
	g_signal_connect(G_OBJECT(tree), "key_press_event", G_CALLBACK(AllowDefault), window);

	// Select the default IWAD.
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(tree));
	gtk_tree_selection_select_iter (selection, &defiter);

	// Create the hbox for the bottom row.
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_end (GTK_BOX(vbox), hbox, false, false, 0);

	// Create the "Don't ask" checkbox.
	check = gtk_check_button_new_with_label ("Don't ask me this again");
	gtk_box_pack_start (GTK_BOX(hbox), check, false, false, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(check), !showwin);

	// Create the OK/Cancel button box.
	bbox = gtk_hbutton_box_new ();
	gtk_button_box_set_layout (GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing (GTK_BOX(bbox), 10);
	gtk_box_pack_end (GTK_BOX(hbox), bbox, false, false, 0);

	// Create the OK button.
	widget = gtk_button_new_from_stock (GTK_STOCK_OK);
	gtk_box_pack_start (GTK_BOX(bbox), widget, false, false, 0);
	GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);
	gtk_widget_grab_default (widget);
	g_signal_connect (widget, "clicked", G_CALLBACK(ClickedOK), &close_style);
	g_signal_connect (widget, "activate", G_CALLBACK(ClickedOK), &close_style);

	// Create the cancel button.
	widget = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	gtk_box_pack_start (GTK_BOX(bbox), widget, false, false, 0);
	g_signal_connect (widget, "clicked", G_CALLBACK(gtk_main_quit), &window);

	// Finally we can show everything.
	gtk_widget_show_all (window);

	gtk_main ();

	if (close_style == 1)
	{
		GtkTreeModel *model;
		GValue value = { 0, { {0} } };
		
		// Find out which IWAD was selected.
		gtk_tree_selection_get_selected (selection, &model, &iter);
		gtk_tree_model_get_value (GTK_TREE_MODEL(model), &iter, 2, &value);
		i = g_value_get_int (&value);
		g_value_unset (&value);
		
		// Set state of queryiwad based on the checkbox.
		queryiwad = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(check));
	}
	else
	{
		i = -1;
	}
	
	if (GTK_IS_WINDOW(window))
	{
		gtk_widget_destroy (window);
		// If we don't do this, then the X window might not actually disappear.
		while (g_main_context_iteration (NULL, FALSE)) {}
	}

	return i;
}
#endif

int I_PickIWad (WadStuff *wads, int numwads, bool showwin, int defaultiwad)
{
	int i;

	if (!showwin)
	{
		return defaultiwad;
	}

#if !defined(__APPLE__)
	const char *str;
	if((str=getenv("KDE_FULL_SESSION")) && strcmp(str, "true") == 0)
	{
		FString cmd("kdialog --title \"" GAMESIG " ");
		cmd << GetVersionString() << ": Select an IWAD to use\""
		            " --menu \"" GAMENAME " found more than one IWAD\n"
		            "Select from the list below to determine which one to use:\"";

		for(i = 0; i < numwads; ++i)
		{
			const char *filepart = strrchr(wads[i].Path, '/');
			if(filepart == NULL)
				filepart = wads[i].Path;
			else
				filepart++;
			// Menu entries are specified in "tag" "item" pairs, where when a
			// particular item is selected (and the Okay button clicked), its
			// corresponding tag is printed to stdout for identification.
			cmd.AppendFormat(" \"%d\" \"%s (%s)\"", i, wads[i].Name.GetChars(), filepart);
		}

		if(defaultiwad >= 0 && defaultiwad < numwads)
		{
			const char *filepart = strrchr(wads[defaultiwad].Path, '/');
			if(filepart == NULL)
				filepart = wads[defaultiwad].Path;
			else
				filepart++;
			cmd.AppendFormat(" --default \"%s (%s)\"", wads[defaultiwad].Name.GetChars(), filepart);
		}

		FILE *f = popen(cmd, "r");
		if(f != NULL)
		{
			char gotstr[16];

			if(fgets(gotstr, sizeof(gotstr), f) == NULL ||
			   sscanf(gotstr, "%d", &i) != 1)
				i = -1;

			// Exit status = 1 means the selection was canceled (either by
			// Cancel/Esc or the X button), not that there was an error running
			// the program. In that case, nothing was printed so fgets will
			// have failed. Other values can indicate an error running the app,
			// so fall back to whatever else can be used.
			int status = pclose(f);
			if(WIFEXITED(status) && (WEXITSTATUS(status) == 0 || WEXITSTATUS(status) == 1))
				return i;
		}
	}
#endif
#ifndef NO_GTK
	if (GtkAvailable)
	{
		return I_PickIWad_Gtk (wads, numwads, showwin, defaultiwad);
	}
#elif defined(__APPLE__)
	return I_PickIWad_Cocoa (wads, numwads, showwin, defaultiwad);
#endif
	
	printf ("Please select a game wad (or 0 to exit):\n");
	for (i = 0; i < numwads; ++i)
	{
		const char *filepart = strrchr (wads[i].Path, '/');
		if (filepart == NULL)
			filepart = wads[i].Path;
		else
			filepart++;
		printf ("%d. %s (%s)\n", i+1, wads[i].Name.GetChars(), filepart);
	}
	printf ("Which one? ");
	if (scanf ("%d", &i) != 1 || i > numwads)
		return -1;
	return i-1;
}

bool I_WriteIniFailed ()
{
	printf ("The config file %s could not be saved:\n%s\n", GameConfig->GetPathName(), strerror(errno));
	return false;
	// return true to retry
}

static const char *pattern;

#if defined(__APPLE__) && MAC_OS_X_VERSION_MAX_ALLOWED < 1080
static int matchfile (struct dirent *ent)
#else
static int matchfile (const struct dirent *ent)
#endif
{
    return fnmatch (pattern, ent->d_name, FNM_NOESCAPE) == 0;
}

void *I_FindFirst (const char *filespec, findstate_t *fileinfo)
{
	FString dir;
	
	const char *slash = strrchr (filespec, '/');
	if (slash)
	{
		pattern = slash+1;
		dir = FString(filespec, slash-filespec+1);
	}
	else
	{
		pattern = filespec;
		dir = ".";
	}

    fileinfo->current = 0;
    fileinfo->count = scandir (dir.GetChars(), &fileinfo->namelist,
							   matchfile, alphasort);
    if (fileinfo->count > 0)
    {
		return fileinfo;
    }
    return (void*)-1;
}

int I_FindNext (void *handle, findstate_t *fileinfo)
{
    findstate_t *state = (findstate_t *)handle;
    if (state->current < fileinfo->count)
    {
	    return ++state->current < fileinfo->count ? 0 : -1;
	}
	return -1;
}

int I_FindClose (void *handle)
{
	findstate_t *state = (findstate_t *)handle;
	if (handle != (void*)-1 && state->count > 0)
	{
		for(int i = 0;i < state->count;++i)
			free (state->namelist[i]);
		state->count = 0;
		free (state->namelist);
		state->namelist = NULL;
	}
	return 0;
}

int I_FindAttr (findstate_t *fileinfo)
{
	dirent *ent = fileinfo->namelist[fileinfo->current];
	struct stat buf;

	if (stat(ent->d_name, &buf) == 0)
	{
		return S_ISDIR(buf.st_mode) ? FA_DIREC : 0;
	}
	return 0;
}

#ifdef __APPLE__
static PasteboardRef s_clipboard;

static CFDataRef GetPasteboardData(const PasteboardItemID itemID, const CFStringRef flavorType)
{
	CFDataRef data = NULL;

	const OSStatus result = PasteboardCopyItemFlavorData(s_clipboard, itemID, flavorType, &data);

	return noErr == result ? data : NULL;
}
#endif // __APPLE__

// Clipboard support requires GTK+
// TODO: GTK+ uses UTF-8. We don't, so some conversions would be appropriate.
void I_PutInClipboard (const char *str)
{
#ifndef NO_GTK
	if (GtkAvailable)
	{
		GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
		if (clipboard != NULL)
		{
			gtk_clipboard_set_text(clipboard, str, -1);
		}
		/* Should I? I don't know. It's not actually a selection.
		clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
		if (clipboard != NULL)
		{
			gtk_clipboard_set_text(clipboard, str, -1);
		}
		*/
	}
#elif defined __APPLE__
	if (NULL == s_clipboard)
	{
		PasteboardCreate(kPasteboardClipboard, &s_clipboard);
	}

	PasteboardClear(s_clipboard);
	PasteboardSynchronize(s_clipboard);

	const CFDataRef textData = CFDataCreate(kCFAllocatorDefault,
		reinterpret_cast<const UInt8*>(str), strlen(str));

	if (NULL != textData)
	{
		PasteboardPutItemFlavor(s_clipboard, PasteboardItemID(1),
			CFSTR("public.utf8-plain-text"), textData, 0);
	}
#endif
}

FString I_GetFromClipboard (bool use_primary_selection)
{
#ifndef NO_GTK
	if (GtkAvailable)
	{
		GtkClipboard *clipboard = gtk_clipboard_get(use_primary_selection ?
			GDK_SELECTION_PRIMARY : GDK_SELECTION_CLIPBOARD);
		if (clipboard != NULL)
		{
			gchar *text = gtk_clipboard_wait_for_text(clipboard);
			if (text != NULL)
			{
				FString copy(text);
				g_free(text);
				return copy;
			}
		}
	}
#elif defined __APPLE__
	FString result;

	if (NULL == s_clipboard)
	{
		PasteboardCreate(kPasteboardClipboard, &s_clipboard);
	}

	PasteboardSynchronize(s_clipboard);

	ItemCount itemCount = 0;
	PasteboardGetItemCount(s_clipboard, &itemCount);

	if (0 == itemCount)
	{
		return FString();
	}

	PasteboardItemID itemID;

	if (0 != PasteboardGetItemIdentifier(s_clipboard, 1, &itemID))
	{
		return FString();
	}

	if (CFDataRef data = GetPasteboardData(itemID, kUTTypeUTF8PlainText))
	{
		const CFIndex bufferLength = CFDataGetLength(data);
		char* const buffer = result.LockNewBuffer(bufferLength);

		memcpy(buffer, CFDataGetBytePtr(data), bufferLength);

		result.UnlockBuffer();
	}
	else if (CFDataRef data = GetPasteboardData(itemID, kUTTypeUTF16PlainText))
	{
#ifdef __LITTLE_ENDIAN__
		static const CFStringEncoding ENCODING = kCFStringEncodingUTF16LE;
#else // __BIG_ENDIAN__
		static const CFStringEncoding ENCODING = kCFStringEncodingUTF16BE;
#endif // __LITTLE_ENDIAN__

		if (const CFStringRef utf16 = CFStringCreateFromExternalRepresentation(kCFAllocatorDefault, data, ENCODING))
		{
			const CFRange range = { 0, CFStringGetLength(utf16) };
			CFIndex bufferLength = 0;

			if (CFStringGetBytes(utf16, range, kCFStringEncodingUTF8, '?', false, NULL, 0, &bufferLength) > 0)
			{
				UInt8* const buffer = reinterpret_cast<UInt8*>(result.LockNewBuffer(bufferLength));

				CFStringGetBytes(utf16, range, kCFStringEncodingUTF8, '?', false, buffer, bufferLength, NULL);

				result.UnlockBuffer();
			}

			CFRelease(utf16);
		}
	}

	return result;
#endif
	return "";
}

// Return a random seed, preferably one with lots of entropy.
unsigned int I_MakeRNGSeed()
{
	unsigned int seed;
	int file;

	// Try reading from /dev/urandom first, then /dev/random, then
	// if all else fails, use a crappy seed from time().
	seed = time(NULL);
	file = open("/dev/urandom", O_RDONLY);
	if (file < 0)
	{
		file = open("/dev/random", O_RDONLY);
	}
	if (file >= 0)
	{
		read(file, &seed, sizeof(seed));
		close(file);
	}
	return seed;
}

TArray<FString> I_GetGogPaths()
{
    // GOG's Doom games are Windows only at the moment
    return TArray<FString>();
}
