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
#include "d_main.h"
#include "i_module.h"
#include "i_system.h"
#include "version.h"

EXTERN_CVAR (Bool, queryiwad);

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

static int PickIWad (WadStuff *wads, int numwads, bool showwin, int defaultiwad)
{
	GtkWidget *window;
	GtkWidget *vbox = nullptr;
	GtkWidget *hbox = nullptr;
	GtkWidget *bbox = nullptr;
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
	gtk_window_set_gravity (GTK_WINDOW(window), GDK_GRAVITY_CENTER);
	gtk_container_set_border_width (GTK_CONTAINER(window), 10);
	g_signal_connect (window, "delete_event", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect (window, "key_press_event", G_CALLBACK(CheckEscape), NULL);

	// Create the vbox container.
	if (gtk_box_new) // Gtk3
		vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
	else if (gtk_vbox_new) // Gtk2
		vbox = gtk_vbox_new (FALSE, 10);

	gtk_container_add (GTK_CONTAINER(window), vbox);

	// Create the top label.
	widget = gtk_label_new (GAMENAME " found more than one IWAD\nSelect from the list below to determine which one to use:");
	gtk_box_pack_start (GTK_BOX(vbox), widget, false, false, 0);

	if (gtk_widget_set_halign && gtk_widget_set_valign) // Gtk3
	{
		gtk_widget_set_halign (widget, GTK_ALIGN_START);
		gtk_widget_set_valign (widget, GTK_ALIGN_START);
	}
	else if (gtk_misc_set_alignment && gtk_misc_get_type) // Gtk2
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
	if (gtk_box_new) // Gtk3
		hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	else if (gtk_hbox_new) // Gtk2
		hbox = gtk_hbox_new (FALSE, 0);

	gtk_box_pack_end (GTK_BOX(vbox), hbox, false, false, 0);

	// Create the "Don't ask" checkbox.
	check = gtk_check_button_new_with_label ("Don't ask me this again");
	gtk_box_pack_start (GTK_BOX(hbox), check, false, false, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(check), !showwin);

	// Create the OK/Cancel button box.
	if (gtk_button_box_new) // Gtk3
		bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	else if (gtk_hbutton_box_new) // Gtk2
		bbox = gtk_hbutton_box_new ();

	gtk_button_box_set_layout (GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing (GTK_BOX(bbox), 10);
	gtk_box_pack_end (GTK_BOX(hbox), bbox, false, false, 0);

	// Create the OK button.
	widget = gtk_button_new_with_label ("OK");

	gtk_box_pack_start (GTK_BOX(bbox), widget, false, false, 0);

	gtk_widget_set_can_default (widget, true);

	gtk_widget_grab_default (widget);
	g_signal_connect (widget, "clicked", G_CALLBACK(ClickedOK), &close_style);
	g_signal_connect (widget, "activate", G_CALLBACK(ClickedOK), &close_style);

	// Create the cancel button.
	widget = gtk_button_new_with_label ("Cancel");

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

static void ShowError(const char* errortext)
{
	GtkWidget *window;
	GtkWidget *widget;
	GtkWidget *vbox = nullptr;
	GtkWidget *bbox = nullptr;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW(window), "Fatal error");
	gtk_window_set_position (GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_gravity (GTK_WINDOW(window), GDK_GRAVITY_CENTER);
	gtk_window_set_resizable (GTK_WINDOW(window), false);
	gtk_container_set_border_width (GTK_CONTAINER(window), 10);
	g_signal_connect (window, "delete_event", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect (window, "key_press_event", G_CALLBACK(CheckEscape), NULL);

	// Create the vbox container.
	if (gtk_box_new) // Gtk3
		vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
	else if (gtk_vbox_new) // Gtk2
		vbox = gtk_vbox_new (FALSE, 10);

	gtk_container_add (GTK_CONTAINER(window), vbox);

	// Create the label.
	widget = gtk_label_new ((const gchar *) errortext);
	gtk_box_pack_start (GTK_BOX(vbox), widget, false, false, 0);

	if (gtk_widget_set_halign && gtk_widget_set_valign) // Gtk3
	{
		gtk_widget_set_halign (widget, GTK_ALIGN_START);
		gtk_widget_set_valign (widget, GTK_ALIGN_START);
	}
	else if (gtk_misc_set_alignment && gtk_misc_get_type) // Gtk2
		gtk_misc_set_alignment (GTK_MISC(widget), 0, 0);

	// Create the Exit button box.
	if (gtk_button_box_new) // Gtk3
		bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	else if (gtk_hbutton_box_new) // Gtk2
		bbox = gtk_hbutton_box_new ();

	gtk_button_box_set_layout (GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing (GTK_BOX(bbox), 10);
	gtk_box_pack_end (GTK_BOX(vbox), bbox, false, false, 0);

	// Create the cancel button.
	widget = gtk_button_new_with_label ("Exit");
	gtk_box_pack_start (GTK_BOX(bbox), widget, false, false, 0);
	g_signal_connect (widget, "clicked", G_CALLBACK(gtk_main_quit), &window);

	// Finally we can show everything.
	gtk_widget_show_all (window);

	gtk_main ();

	if (GTK_IS_WINDOW(window))
	{
		gtk_widget_destroy (window);
		// If we don't do this, then the X window might not actually disappear.
		while (g_main_context_iteration (NULL, FALSE)) {}
	}
}

} // namespace Gtk

int I_PickIWad_Gtk (WadStuff *wads, int numwads, bool showwin, int defaultiwad)
{
	return Gtk::PickIWad (wads, numwads, showwin, defaultiwad);
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
