/*
 * xdx - GTK+ DX-cluster client for amateur radio
 * Copyright (C) 2002 Joop Stakenborg <pa4tu@amsat.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * gui.c - where the main window is created
 */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "types.h"
#include "gui.h"
#include "preferences.h"
#include "net.h"
#include "utils.h"
#include "hyperlink.h"
#include "gui_opendialog.h"
#include "gui_closedialog.h"
#include "gui_aboutdialog.h"
#include "gui_settingsdialog.h"

extern preferencestype preferences;

/**********************************MAIN WINDOW********************************/

static GtkItemFactoryEntry menu_items[] = {
  {N_("/_Program"), NULL, NULL, 0, "<Branch>"},
  {N_("/_Program/Quit"), "<control>Q", gtk_main_quit, 0, "<StockItem>",
   GTK_STOCK_QUIT},
  {N_("/_Host"), NULL, NULL, 0, "<Branch>"},
  {N_("/_Host/Open"), "<control>O", on_open_activate, 0, "<StockItem>",
   GTK_STOCK_OPEN},
  {N_("/_Host/Close"), "<control>C", on_close_activate, 0, "<StockItem>",
   GTK_STOCK_CLOSE},
  {N_("/_Settings"), NULL, NULL, 0, "<Branch>"},
  {N_("/_Settings/Preferences"), "<control>P", on_settings_activate, 0, 
    "<StockItem>", GTK_STOCK_PREFERENCES},
  {N_("/H_elp"), NULL, NULL, 0, "<LastBranch>"},
  {N_("/H_elp/About"), "<control>H", on_about_activate, 0, "<StockItem>",
   GTK_STOCK_HELP},
};

guitype *new_gui(void)
{
  guitype *gui = g_new0(guitype, 1);
  gui->window = NULL;
	gui->item_factory = NULL;
	gui->hostnamehistory = NULL;
	gui->porthistory = NULL;
	gui->txhistory = NULL;
	gui->preferencesdir = NULL;
	gui->updown = 0;
	gui->txitem = 0;
  gui->statusbartimer = -1;
  gui->statusbarmessage = NULL;
  return(gui);
}

static gchar *menu_translate(const gchar *path, gpointer data)
{
  gchar *retval = gettext(path);
  return retval;
}


static void
get_main_menu (GtkWidget * window, GtkWidget ** menubar)
{
  GtkAccelGroup *accel_group;

  gint nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);

  accel_group = gtk_accel_group_new ();
  gui->item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>",
				       accel_group);
  gtk_item_factory_set_translate_func(gui->item_factory, menu_translate, NULL, NULL);
  gtk_item_factory_create_items (gui->item_factory, nmenu_items, menu_items, NULL);
  gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
  if (menubar)
    *menubar = gtk_item_factory_get_widget (gui->item_factory, "<main>");
}

void
create_mainwindow (void)
{
  GtkWidget *mainvbox, *handlebox, *mainmenubar, *vpaned1, *clistscrolledwindow,
	  *mainscrolledwindow, *maintext, *mainentry, *mainstatusbar, *treeview;
  GtkCellRenderer *renderer, *boldrenderer;
  GtkTreeViewColumn *column;
  GtkTextBuffer *buffer;
  GtkTreeStore *model;
  GdkPixbuf *icon = NULL;
  GError *err = NULL;
  GString *msg = g_string_new ("");
  servertype *cluster;

  gui = new_gui();
  gui->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  icon = gdk_pixbuf_new_from_file (PACKAGE_DATA_DIR "/pixmaps/xdx.png", &err);
  if (err)
  {
    g_string_printf (msg, _("Error loading icon: %s"), err->message);
    updatestatusbar (msg, TRUE);
    g_string_free (msg, TRUE);
    g_error_free (err);
    err = NULL;
  }
  if (icon)
  {
  gtk_window_set_icon (GTK_WINDOW (gui->window), icon);
  gdk_pixbuf_unref (icon);
  }

  mainvbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (gui->window), mainvbox);

  handlebox = gtk_handle_box_new ();
  gtk_box_pack_start (GTK_BOX (mainvbox), handlebox, FALSE, FALSE, 0);
  get_main_menu (gui->window, &mainmenubar);
  gtk_container_add (GTK_CONTAINER (handlebox), mainmenubar);

  clistscrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (clistscrolledwindow),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

  model = gtk_tree_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING,
			      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
			      G_TYPE_STRING);
  treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  g_object_unref (G_OBJECT (model));

  renderer = gtk_cell_renderer_text_new ();
  boldrenderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (boldrenderer), "weight", "bold", NULL);

  column =
    gtk_tree_view_column_new_with_attributes (_("Spotter"), renderer, "text",
					      FROM_COLUMN, NULL);
  gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column), GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  column =
    gtk_tree_view_column_new_with_attributes ("QRG", renderer, "text",
					      FREQ_COLUMN, NULL);
  gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column), GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  column =
    gtk_tree_view_column_new_with_attributes ("DX", boldrenderer, "text",
					      DX_COLUMN, NULL);
  gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column), GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  column =
    gtk_tree_view_column_new_with_attributes (_("Remarks"), renderer, "text",
					      REM_COLUMN, NULL);
  gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column), GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  column =
    gtk_tree_view_column_new_with_attributes (_("Time"), renderer, "text",
					      TIME_COLUMN, NULL);
  gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column), GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  column =
    gtk_tree_view_column_new_with_attributes ("Info", renderer, "text",
					      INFO_COLUMN, NULL);
  gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column), GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  gtk_container_add (GTK_CONTAINER (clistscrolledwindow), treeview);

  mainscrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (mainscrolledwindow),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

  maintext = gtk_text_view_new ();
  gtk_container_add (GTK_CONTAINER (mainscrolledwindow), maintext);
  GTK_WIDGET_UNSET_FLAGS (maintext, GTK_CAN_FOCUS);
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (maintext));
  gtk_text_buffer_create_tag (buffer, "wwv", "foreground", "darkgreen",
			      NULL);
  gtk_text_buffer_create_tag (buffer, "wx", "foreground", "magenta",
			      NULL);
  gtk_text_buffer_create_tag (buffer, "sent", "foreground", "red",
			      NULL);
  gtk_text_buffer_create_tag (buffer, "url", "foreground", "blue", 
				    "underline", PANGO_UNDERLINE_SINGLE, NULL);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (maintext), GTK_WRAP_WORD);

  vpaned1 = gtk_vpaned_new ();
  gtk_paned_pack1 (GTK_PANED (vpaned1), clistscrolledwindow, TRUE, FALSE);
  gtk_paned_pack2 (GTK_PANED (vpaned1), mainscrolledwindow, FALSE, TRUE);
  gtk_paned_set_position (GTK_PANED(vpaned1), preferences.panedpos);
  gtk_box_pack_start (GTK_BOX (mainvbox), vpaned1, TRUE, TRUE, 0);

  mainentry = gtk_entry_new ();
  gtk_entry_set_max_length (GTK_ENTRY (mainentry), 80);
  gtk_box_pack_start (GTK_BOX (mainvbox), mainentry, FALSE, FALSE, 0);

  mainstatusbar = gtk_statusbar_new ();
  gtk_box_pack_start (GTK_BOX (mainvbox), mainstatusbar, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (gui->window), "destroy",
		    G_CALLBACK (on_mainwindow_destroy_event), NULL);
  g_signal_connect (G_OBJECT (gui->window), "delete_event",
		    G_CALLBACK (on_mainwindow_delete_event), NULL);
  g_signal_connect (G_OBJECT (mainentry), "activate",
		    G_CALLBACK (on_mainentry_activate), NULL);
  g_signal_connect (G_OBJECT (gui->window), "key_press_event",
		    G_CALLBACK (on_mainwindow_key_press_event), NULL);
  g_signal_connect (G_OBJECT(maintext), "motion_notify_event",
		    G_CALLBACK (on_maintext_motion_notify_event), NULL);
  g_signal_connect (G_OBJECT (maintext), "event-after",
        G_CALLBACK (on_maintext_event_after), NULL);
  g_signal_connect (G_OBJECT (treeview), "button-press-event",
        G_CALLBACK (double_click), NULL);

  g_object_set_data (G_OBJECT (gui->window), "maintext", maintext);
  g_object_set_data (G_OBJECT (gui->window), "treeview", treeview);
  g_object_set_data (G_OBJECT (gui->window), "mainstatusbar", mainstatusbar);
  g_object_set_data (G_OBJECT (gui->window), "mainentry", mainentry);
  g_object_set_data (G_OBJECT (gui->window), "model", model);
  g_object_set_data (G_OBJECT (gui->window), "buffer", buffer);
  g_object_set_data (G_OBJECT (gui->window), "vpaned1", vpaned1);

  cluster = new_cluster();
  g_object_set_data(G_OBJECT (gui->window), "cluster", cluster);

  gtk_widget_grab_focus (mainentry);

  return;
}

/*
 * hit <enter> in the entry widget
 */
void
on_mainentry_activate (GtkEditable * editable, gpointer user_data)
{
  gchar *entry;
  GString *str = g_string_new ("");
  GtkWidget *mainentry;

  mainentry = g_object_get_data (G_OBJECT (gui->window), "mainentry");
  entry = gtk_editable_get_chars (GTK_EDITABLE (mainentry), 0, -1);
  str = g_string_new(entry);
  tx (str);
  g_string_free(str, TRUE);
  g_free (entry);
  gtk_editable_delete_text (GTK_EDITABLE (mainentry), 0, -1);
  gtk_widget_grab_focus (GTK_WIDGET (mainentry));
}

/*
 * called at program exit
 */
gboolean
on_mainwindow_delete_event (GtkWidget * widget, GdkEvent * event,
			    gpointer user_data)
{
  GtkWidget *vpaned1, *treeview;
  GList * columns;
  gint i, length;
  servertype *cluster;
  GString *w = g_string_new("");

  cluster = g_object_get_data(G_OBJECT(widget), "cluster");
  if (cluster->sockethandle != -1)
    cldisconnect(NULL, FALSE);

  gtk_window_get_position(GTK_WINDOW(gui->window), &preferences.x, &preferences.y);
  gtk_window_get_size(GTK_WINDOW(gui->window), &preferences.width, &preferences.height);

  vpaned1 = g_object_get_data (G_OBJECT(gui->window), "vpaned1");
  preferences.panedpos = gtk_paned_get_position (GTK_PANED(vpaned1));

  treeview = g_object_get_data (G_OBJECT(gui->window), "treeview");
  columns = gtk_tree_view_get_columns(GTK_TREE_VIEW(treeview));
  length = g_list_length(columns);
  for (i = 0; i < length; i++) 
      g_string_append_printf(w, "%d,", gtk_tree_view_column_get_width
        (gtk_tree_view_get_column(GTK_TREE_VIEW(treeview), i)));
  g_list_free(columns);
  preferences.columnwidths = g_strdup(w->str);
  g_string_free(w, TRUE);

  savehistory ();
  savepreferences ();

  return FALSE;
}

gboolean
on_mainwindow_destroy_event (GtkWidget * widget, GdkEvent * event,
			    gpointer user_data)
{
  gint i, n;
  GList *link;
  servertype *cluster;

  gui->window = NULL;
  gui->item_factory = NULL;

  cluster = g_object_get_data(G_OBJECT(widget), "cluster");
  g_free(cluster);

  g_free(preferences.columnwidths);
  g_free(preferences.callsign);

  link = gui->hostnamehistory;
  while (link)
	{
		g_free(link->data);
	  link = link->next;
	}
  g_list_free(gui->hostnamehistory);
  gui->hostnamehistory = NULL;
	
  link = gui->porthistory;
  while (link)
	{
		g_free(link->data);
	  link = link->next;
	}
  g_list_free(gui->porthistory);
  gui->porthistory = NULL;

  link = gui->txhistory;
  while (link)
	{
		g_free(link->data);
	  link = link->next;
	}
  g_list_free(gui->txhistory);
  gui->txhistory = NULL;
	
  g_free(gui->preferencesdir);
  gui->preferencesdir = NULL;
  g_free(gui->statusbarmessage);
  gui->statusbarmessage = NULL;
  g_free(gui->url);
  gui->url = NULL;
  g_free(gui);

  gtk_main_quit ();

  return FALSE;
}


/* 
 * history of the entry widget 
 */
gboolean on_mainwindow_key_press_event(GtkWidget *widget, GdkEventKey *event,
					 gpointer user_data)
{

  GtkWidget *mainentry;

  mainentry = g_object_get_data (G_OBJECT (gui->window), "mainentry");
  if (GTK_WIDGET_HAS_FOCUS(mainentry))
  {
    switch (event->keyval)
    {
      case GDK_Up:
        gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event");
        tx_previous();
      break;
      case GDK_Down:
        gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event");
        tx_next();
      break;
      default:
      break;
    }
  }
  return FALSE;
}

gboolean 
double_click (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{  
  GtkTreeIter selected;
  GtkTreeModel *model;
  gchar *getf, **fsplit, *hamlibstr;
  gint setf;

  if (event->type == GDK_2BUTTON_PRESS)
  {
    if (gtk_tree_selection_get_selected
      (gtk_tree_view_get_selection (GTK_TREE_VIEW(widget)), &model, &selected))
    {
      gtk_tree_model_get (model, &selected, 1, &getf, -1);
      fsplit = g_strsplit (getf, ".", -1);
      setf = atoi(fsplit[0]) * 1000 + atoi(fsplit[1]) * 100;
      hamlibstr = g_strdup_printf ("rigctl -m 210 set_freq %d", setf);
      system (hamlibstr);
      g_free (hamlibstr);
      g_strfreev (fsplit);
    }
  }
  return FALSE;
}
