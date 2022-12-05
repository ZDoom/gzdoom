/*
** gtk_dialogs.cpp
**
**---------------------------------------------------------------------------
** Copyright 2008-2016 Braden Obrzut
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

#ifndef NO_GTK

#if !DYN_GTK
// Function addresses will never be NULL, but that's because we're using the
// same code for both dynamic and static.
#pragma GCC diagnostic ignored "-Waddress"
#endif

#include <gtk/gtk.h>
#if GTK_MAJOR_VERSION >= 3
#include <gdk/gdk.h>
#else
#include <gdk/gdkkeysyms.h>
typedef enum
{
	GTK_ALIGN_FULL,
	GTK_ALIGN_START,
	GTK_ALIGN_END,
	GTK_ALIGN_CENTER,
	GTK_ALIGN_BASELINE
} GtkAlign;
#endif

#include "c_cvars.h"
#include "i_module.h"
#include "i_system.h"
#include "version.h"
#include "startupinfo.h"
#include "cmdlib.h"
#include "i_interface.h"
#include "printf.h"

EXTERN_CVAR (Bool, queryiwad);
EXTERN_CVAR (Int, vid_preferbackend);
EXTERN_CVAR (Bool, vid_fullscreen);

namespace Gtk {

FModuleMaybe<DYN_GTK> GtkModule{"GTK"};
static int GtkAvailable = -1;

#define DYN_GTK_SYM(x) const FModuleMaybe<DYN_GTK>::Req<GtkModule, decltype(::x)*, &x> x{#x};
#define DYN_GTK_REQ_SYM(x, proto) const FModuleMaybe<DYN_GTK>::Req<GtkModule, proto, &x> x{#x};
#if GTK_MAJOR_VERSION >= 3
#define DYN_GTK_OPT2_SYM(x, proto) const FModuleMaybe<DYN_GTK>::Opt<GtkModule, proto, nullptr> x{#x};
#define DYN_GTK_OPT3_SYM(x, proto) const FModuleMaybe<DYN_GTK>::Opt<GtkModule, proto, &x> x{#x};
#else
#define DYN_GTK_OPT2_SYM(x, proto) const FModuleMaybe<DYN_GTK>::Opt<GtkModule, proto, &x> x{#x};
#define DYN_GTK_OPT3_SYM(x, proto) const FModuleMaybe<DYN_GTK>::Opt<GtkModule, proto, nullptr> x{#x};
#endif

DYN_GTK_SYM(g_main_context_iteration);
DYN_GTK_SYM(g_signal_connect_data);
DYN_GTK_SYM(g_type_check_instance_cast);
DYN_GTK_SYM(g_type_check_instance_is_a);
DYN_GTK_SYM(g_value_get_int);
DYN_GTK_SYM(g_value_unset);
DYN_GTK_SYM(gtk_box_get_type);
DYN_GTK_SYM(gtk_box_pack_end);
DYN_GTK_SYM(gtk_box_pack_start);
DYN_GTK_SYM(gtk_box_set_spacing);
DYN_GTK_SYM(gtk_button_box_get_type);
DYN_GTK_SYM(gtk_button_box_set_layout);
DYN_GTK_SYM(gtk_button_new_with_label);
DYN_GTK_SYM(gtk_cell_renderer_text_new);
DYN_GTK_SYM(gtk_check_button_new_with_label);
DYN_GTK_SYM(gtk_radio_button_new_with_label);
DYN_GTK_SYM(gtk_radio_button_new_with_label_from_widget);
DYN_GTK_SYM(gtk_radio_button_get_type);
DYN_GTK_SYM(gtk_container_add);
DYN_GTK_SYM(gtk_container_get_type);
DYN_GTK_SYM(gtk_container_set_border_width);
DYN_GTK_SYM(gtk_init_check);
DYN_GTK_SYM(gtk_label_new);
DYN_GTK_SYM(gtk_list_store_append);
DYN_GTK_SYM(gtk_list_store_new);
DYN_GTK_SYM(gtk_list_store_set);
DYN_GTK_SYM(gtk_toggle_button_get_type);
DYN_GTK_SYM(gtk_toggle_button_set_active);
DYN_GTK_SYM(gtk_tree_model_get_type);
DYN_GTK_SYM(gtk_tree_model_get_value);
DYN_GTK_SYM(gtk_tree_selection_get_selected);
DYN_GTK_SYM(gtk_tree_selection_select_iter);
DYN_GTK_SYM(gtk_tree_view_append_column);
// Explicitly give the type so that attributes don't cause a warning.
DYN_GTK_REQ_SYM(gtk_tree_view_column_new_with_attributes, GtkTreeViewColumn *(*)(const gchar *, GtkCellRenderer *, ...));
DYN_GTK_SYM(gtk_toggle_button_get_active);
DYN_GTK_SYM(gtk_tree_view_get_selection);
DYN_GTK_SYM(gtk_tree_view_get_type);
DYN_GTK_SYM(gtk_tree_view_new_with_model);
DYN_GTK_SYM(gtk_main);
DYN_GTK_SYM(gtk_main_quit);
DYN_GTK_SYM(gtk_widget_destroy);
DYN_GTK_SYM(gtk_widget_grab_default);
DYN_GTK_SYM(gtk_widget_get_type);
DYN_GTK_SYM(gtk_widget_set_can_default);
DYN_GTK_SYM(gtk_widget_show_all);
DYN_GTK_SYM(gtk_window_activate_default);
DYN_GTK_SYM(gtk_window_get_type);
DYN_GTK_SYM(gtk_window_new);
DYN_GTK_SYM(gtk_window_set_gravity);
DYN_GTK_SYM(gtk_window_set_position);
DYN_GTK_SYM(gtk_window_set_title);
DYN_GTK_SYM(gtk_window_set_resizable);
DYN_GTK_SYM(gtk_dialog_run);
DYN_GTK_SYM(gtk_dialog_get_type);

// Gtk3 Only
DYN_GTK_OPT3_SYM(gtk_box_new, GtkWidget *(*)(GtkOrientation, gint));
DYN_GTK_OPT3_SYM(gtk_button_box_new, GtkWidget *(*)(GtkOrientation));
DYN_GTK_OPT3_SYM(gtk_widget_set_halign, void(*)(GtkWidget *, GtkAlign));
DYN_GTK_OPT3_SYM(gtk_widget_set_valign, void(*)(GtkWidget *, GtkAlign));
DYN_GTK_OPT3_SYM(gtk_message_dialog_new, GtkWidget* (*)(GtkWindow*, GtkDialogFlags, GtkMessageType, GtkButtonsType, const gchar*, ...));

// Gtk2 Only
DYN_GTK_OPT2_SYM(gtk_misc_get_type, GType(*)());
DYN_GTK_OPT2_SYM(gtk_hbox_new, GtkWidget *(*)(gboolean, gint));
DYN_GTK_OPT2_SYM(gtk_hbutton_box_new, GtkWidget *(*)());
DYN_GTK_OPT2_SYM(gtk_misc_set_alignment, void(*)(GtkMisc *, gfloat, gfloat));
DYN_GTK_OPT2_SYM(gtk_vbox_new, GtkWidget *(*)(gboolean, gint));

#undef DYN_GTK_SYM
#undef DYN_GTK_REQ_SYM
#undef DYN_GTK_OPT2_SYM
#undef DYN_GTK_OPT3_SYM

// GtkTreeViews eats return keys. I want this to be like a Windows listbox
// where pressing Return can still activate the default button.
static gint AllowDefault(GtkWidget *widget, GdkEventKey *event, gpointer func_data)
{
	if (event->type == GDK_KEY_PRESS && event->keyval == GDK_KEY_Return)
	{
		gtk_window_activate_default (GTK_WINDOW(func_data));
	}
	return FALSE;
}

// Double-clicking an entry in the list is the same as pressing OK.
static gint DoubleClickChecker(GtkWidget *widget, GdkEventButton *event, gpointer func_data)
{
	if (event->type == GDK_2BUTTON_PRESS)
	{
		*(int *)func_data = 1;
		gtk_main_quit();
	}
	return FALSE;
}

// When the user presses escape, that should be the same as canceling the dialog.
static gint CheckEscape (GtkWidget *widget, GdkEventKey *event, gpointer func_data)
{
	if (event->type == GDK_KEY_PRESS && event->keyval == GDK_KEY_Escape)
	{
		gtk_main_quit();
	}
	return FALSE;
}

static void ClickedOK(GtkButton *button, gpointer func_data)
{
	*(int *)func_data = 1;
	gtk_main_quit();
}

class ZUIWidget
{
public:
	virtual ~ZUIWidget() = default;

	GtkWidget *widget = nullptr;
};

class ZUIWindow : public ZUIWidget
{
public:
	ZUIWindow(const char* title)
	{
		widget = gtk_window_new (GTK_WINDOW_TOPLEVEL);

		gtk_window_set_title (GTK_WINDOW(widget), title);
		gtk_window_set_position (GTK_WINDOW(widget), GTK_WIN_POS_CENTER);
		gtk_window_set_gravity (GTK_WINDOW(widget), GDK_GRAVITY_CENTER);

		gtk_container_set_border_width (GTK_CONTAINER(widget), 15);

		g_signal_connect (widget, "delete_event", G_CALLBACK(gtk_main_quit), NULL);
		g_signal_connect (widget, "key_press_event", G_CALLBACK(CheckEscape), NULL);
	}

	~ZUIWindow()
	{
		if (GTK_IS_WINDOW(widget))
		{
			gtk_widget_destroy (widget);
			// If we don't do this, then the X window might not actually disappear.
			while (g_main_context_iteration (NULL, FALSE)) {}
		}
	}

	void AddWidget(ZUIWidget* child)
	{
		gtk_container_add (GTK_CONTAINER(widget), child->widget);
	}

	void RunModal()
	{
		gtk_widget_show_all (widget);
		gtk_main ();
	}
};

class ZUIVBox : public ZUIWidget
{
public:
	ZUIVBox()
	{
		if (gtk_box_new) // Gtk3
			widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
		else if (gtk_vbox_new) // Gtk2
			widget = gtk_vbox_new (FALSE, 10);
	}

	void PackStart(ZUIWidget* child, bool expand, bool fill, int padding)
	{
		gtk_box_pack_start (GTK_BOX(widget), child->widget, expand, fill, padding);
	}

	void PackEnd(ZUIWidget* child, bool expand, bool fill, int padding)
	{
		gtk_box_pack_end (GTK_BOX(widget), child->widget, expand, fill, padding);
	}
};

class ZUILabel : public ZUIWidget
{
public:
	ZUILabel(const char* text)
	{
		widget = gtk_label_new (text);

		if (gtk_widget_set_halign && gtk_widget_set_valign) // Gtk3
		{
			gtk_widget_set_halign (widget, GTK_ALIGN_START);
			gtk_widget_set_valign (widget, GTK_ALIGN_START);
		}
		else if (gtk_misc_set_alignment && gtk_misc_get_type) // Gtk2
			gtk_misc_set_alignment (GTK_MISC(widget), 0, 0);
	}
};

class ZUIListView : public ZUIWidget
{
public:
	ZUIListView(WadStuff *wads, int numwads, int defaultiwad)
	{
		// Create a list store with all the found IWADs.
		store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
		for (int i = 0; i < numwads; ++i)
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
		widget = gtk_tree_view_new_with_model (GTK_TREE_MODEL(store));
		renderer = gtk_cell_renderer_text_new ();
		column = gtk_tree_view_column_new_with_attributes ("IWAD", renderer, "text", 0, NULL);
		gtk_tree_view_append_column (GTK_TREE_VIEW(widget), column);
		renderer = gtk_cell_renderer_text_new ();
		column = gtk_tree_view_column_new_with_attributes ("Game", renderer, "text", 1, NULL);
		gtk_tree_view_append_column (GTK_TREE_VIEW(widget), column);

		// Select the default IWAD.
		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(widget));
		gtk_tree_selection_select_iter (selection, &defiter);
	}

	int GetSelectedIndex()
	{
		GtkTreeModel *model;
		GValue value = { 0, { {0} } };

		// Find out which IWAD was selected.
		gtk_tree_selection_get_selected (selection, &model, &iter);
		gtk_tree_model_get_value (GTK_TREE_MODEL(model), &iter, 2, &value);
		int i = g_value_get_int (&value);
		g_value_unset (&value);
		return i;
	}

	void ConnectButtonPress(int *close_style)
	{
		g_signal_connect(G_OBJECT(widget), "button_press_event", G_CALLBACK(DoubleClickChecker), &close_style);
	}

	void ConnectKeyPress(ZUIWindow* window)
	{
		g_signal_connect(G_OBJECT(widget), "key_press_event", G_CALLBACK(AllowDefault), window->widget);
	}

	GtkListStore *store = nullptr;
	GtkCellRenderer *renderer = nullptr;
	GtkTreeViewColumn *column = nullptr;
	GtkTreeSelection *selection = nullptr;
	GtkTreeIter iter, defiter;
};

class ZUIHBox : public ZUIWidget
{
public:
	ZUIHBox()
	{
		// Create the hbox for the bottom row.
		if (gtk_box_new) // Gtk3
			widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		else if (gtk_hbox_new) // Gtk2
			widget = gtk_hbox_new (FALSE, 0);
	}

	void PackStart(ZUIWidget* child, bool expand, bool fill, int padding)
	{
		gtk_box_pack_start (GTK_BOX(widget), child->widget, expand, fill, padding);
	}

	void PackEnd(ZUIWidget* child, bool expand, bool fill, int padding)
	{
		gtk_box_pack_end (GTK_BOX(widget), child->widget, expand, fill, padding);
	}
};

class ZUICheckButton : public ZUIWidget
{
public:
	ZUICheckButton(const char* text)
	{
		widget = gtk_check_button_new_with_label (text);
	}

	void SetChecked(bool value)
	{
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(widget), value);
	}

	int GetChecked()
	{
		return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
	}
};

class ZUIRadioButton : public ZUIWidget
{
public:
	ZUIRadioButton(const char* text)
	{
		widget = gtk_radio_button_new_with_label (nullptr, text);
	}

	ZUIRadioButton(ZUIRadioButton* group, const char* text)
	{
		widget = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(group->widget), text);
	}

	void SetChecked(bool value)
	{
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(widget), value);
	}

	int GetChecked()
	{
		return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
	}
};

class ZUIButtonBox : public ZUIWidget
{
public:
	ZUIButtonBox()
	{
		if (gtk_button_box_new) // Gtk3
			widget = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
		else if (gtk_hbutton_box_new) // Gtk2
			widget = gtk_hbutton_box_new ();

		gtk_button_box_set_layout (GTK_BUTTON_BOX(widget), GTK_BUTTONBOX_END);
		gtk_box_set_spacing (GTK_BOX(widget), 10);
	}

	void PackStart(ZUIWidget* child, bool expand, bool fill, int padding)
	{
		gtk_box_pack_start (GTK_BOX(widget), child->widget, expand, fill, padding);
	}

	void PackEnd(ZUIWidget* child, bool expand, bool fill, int padding)
	{
		gtk_box_pack_end (GTK_BOX(widget), child->widget, expand, fill, padding);
	}
};

class ZUIButton : public ZUIWidget
{
public:
	ZUIButton(const char* text, bool defaultButton)
	{
		widget = gtk_button_new_with_label (text);

		if (defaultButton)
		{
			gtk_widget_set_can_default (widget, true);
		}
	}

	void GrabDefault()
	{
		gtk_widget_grab_default (widget);
	}

	void ConnectClickedOK(int *close_style)
	{
		g_signal_connect (widget, "clicked", G_CALLBACK(ClickedOK), close_style);
		g_signal_connect (widget, "activate", G_CALLBACK(ClickedOK), close_style);
	}

	void ConnectClickedExit(ZUIWindow* window)
	{
		g_signal_connect (widget, "clicked", G_CALLBACK(gtk_main_quit), &window->widget);
	}
};

static int PickIWad (WadStuff *wads, int numwads, bool showwin, int defaultiwad, int& autoloadflags)
{
	char caption[100];
	mysnprintf(caption, countof(caption), GAMENAME " %s: Select an IWAD to use", GetVersionString());

	ZUIWindow window(caption);

	ZUIVBox vbox;

	ZUILabel label(GAMENAME " found more than one IWAD\nSelect from the list below to determine which one to use:");
	ZUIListView listview(wads, numwads, defaultiwad);

	ZUIHBox hboxOptions;

	ZUIVBox vboxVideo;
	ZUILabel videoSettings("Video settings");
	ZUIRadioButton opengl("OpenGL");
	ZUIRadioButton vulkan(&opengl, "Vulkan");
	ZUIRadioButton openglES(&opengl, "OpenGL ES");
	ZUICheckButton fullscreen("Full screen");

	ZUIVBox vboxMisc;
	ZUICheckButton noautoload("Disable autoload");
	ZUICheckButton dontAskAgain("Don't ask me this again");

	ZUIVBox vboxExtra;
	ZUILabel extraGraphics("Extra graphics");
	ZUICheckButton lights("Lights");
	ZUICheckButton brightmaps("Brightmaps");
	ZUICheckButton widescreen("Widescreen");

	ZUIHBox hboxButtons;

	ZUIButtonBox bbox;
	ZUIButton playButton("Play Game!", true);
	ZUIButton exitButton("Exit", false);

	window.AddWidget(&vbox);
	vbox.PackStart(&label, false, false, 0);
	vbox.PackStart(&listview, true, true, 0);
	vbox.PackEnd(&hboxButtons, false, false, 0);
	vbox.PackEnd(&hboxOptions, false, false, 0);
	hboxOptions.PackStart(&vboxVideo, false, false, 15);
	hboxOptions.PackStart(&vboxMisc, true, false, 15);
	hboxOptions.PackStart(&vboxExtra, false, false, 15);
	vboxVideo.PackStart(&videoSettings, false, false, 0);
	vboxVideo.PackStart(&opengl, false, false, 0);
	vboxVideo.PackStart(&vulkan, false, false, 0);
	vboxVideo.PackStart(&openglES, false, false, 0);
	vboxVideo.PackStart(&fullscreen, false, false, 15);
	vboxMisc.PackStart(&noautoload, false, false, 0);
	vboxMisc.PackStart(&dontAskAgain, false, false, 0);
	vboxExtra.PackStart(&extraGraphics, false, false, 0);
	vboxExtra.PackStart(&lights, false, false, 0);
	vboxExtra.PackStart(&brightmaps, false, false, 0);
	vboxExtra.PackStart(&widescreen, false, false, 0);
	hboxButtons.PackStart(&bbox, true, true, 0);
	bbox.PackStart(&playButton, false, false, 0);
	bbox.PackEnd(&exitButton, false, false, 0);

	dontAskAgain.SetChecked(!showwin);

	switch (vid_preferbackend)
	{
	case 0: opengl.SetChecked(true); break;
	case 1: vulkan.SetChecked(true); break;
	case 2: openglES.SetChecked(true); break;
	default: break;
	}

	if (vid_fullscreen) fullscreen.SetChecked(true);

	if (autoloadflags & 1) noautoload.SetChecked(true);
	if (autoloadflags & 2) lights.SetChecked(true);
	if (autoloadflags & 4) brightmaps.SetChecked(true);
	if (autoloadflags & 8) widescreen.SetChecked(true);

	int close_style = 0;
	listview.ConnectButtonPress(&close_style);
	listview.ConnectKeyPress(&window);
	playButton.ConnectClickedOK(&close_style);
	exitButton.ConnectClickedExit(&window);

	playButton.GrabDefault();

	window.RunModal();

	if (close_style == 1)
	{
		int i = listview.GetSelectedIndex();

		// Set state of queryiwad based on the checkbox.
		queryiwad = !dontAskAgain.GetChecked();

		if (opengl.GetChecked()) vid_preferbackend = 0;
		if (vulkan.GetChecked()) vid_preferbackend = 1;
		if (openglES.GetChecked()) vid_preferbackend = 2;

		vid_fullscreen = fullscreen.GetChecked();

		autoloadflags = 0;
		if (noautoload.GetChecked()) autoloadflags |= 1;
		if (lights.GetChecked()) autoloadflags |= 2;
		if (brightmaps.GetChecked()) autoloadflags |= 4;
		if (widescreen.GetChecked()) autoloadflags |= 8;

		return i;
	}
	else
	{
		return -1;
	}
}

static void ShowError(const char* errortext)
{
	ZUIWindow window("Fatal error");
	ZUIVBox vbox;
	ZUILabel label(errortext);
	ZUIButtonBox bbox;
	ZUIButton exitButton("Exit", true);

	window.AddWidget(&vbox);
	vbox.PackStart(&label, true, true, 0);
	vbox.PackEnd(&bbox, false, false, 0);
	bbox.PackEnd(&exitButton, false, false, 0);

	exitButton.ConnectClickedExit(&window);

	exitButton.GrabDefault();
	window.RunModal();
}

} // namespace Gtk

int I_PickIWad_Gtk (WadStuff *wads, int numwads, bool showwin, int defaultiwad, int& autoloadflags)
{
	return Gtk::PickIWad (wads, numwads, showwin, defaultiwad, autoloadflags);
}

void I_ShowFatalError_Gtk(const char* errortext) {
	Gtk::ShowError(errortext);
}

bool I_GtkAvailable()
{
	using namespace Gtk;

	if(GtkAvailable < 0)
	{
		if (!GtkModule.Load({"libgtk-3.so.0", "libgtk-x11-2.0.so.0"}))
		{
			GtkAvailable = 0;
			return false;
		}

		int argc = 0;
		char **argv = nullptr;
		GtkAvailable = Gtk::gtk_init_check (&argc, &argv);
	}

	return GtkAvailable != 0;
}

#endif
