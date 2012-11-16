/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Caja is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Caja is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 *         Ettore Perazzoli <ettore@gnu.org>
 *         Michael Meeks <michael@nuclecu.unam.mx>
 *	   Andy Hertzfeld <andy@eazel.com>
 *
 */

/* caja-location-bar.c - Location bar for Caja
 */

#include <config.h>
#include "caja-location-entry.h"

#include "caja-window-private.h"
#include "caja-window.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <libcaja-private/caja-file-utilities.h>
#include <libcaja-private/caja-entry.h>
#include <libcaja-private/caja-icon-dnd.h>
#include <libcaja-private/caja-clipboard.h>
#include <stdio.h>
#include <string.h>

struct CajaLocationEntryDetails
{
    GtkLabel *label;

    char *current_directory;
    GFilenameCompleter *completer;

    guint idle_id;

    gboolean has_special_text;
    gboolean setting_special_text;
    gchar *special_text;
    CajaLocationEntryAction secondary_action;
};

static void  caja_location_entry_class_init       (CajaLocationEntryClass *class);
static void  caja_location_entry_init             (CajaLocationEntry      *entry);

EEL_CLASS_BOILERPLATE (CajaLocationEntry,
                       caja_location_entry,
                       CAJA_TYPE_ENTRY)

/* routine that performs the tab expansion.  Extract the directory name and
   incomplete basename, then iterate through the directory trying to complete it.  If we
   find something, add it to the entry */

static gboolean
try_to_expand_path (gpointer callback_data)
{
    CajaLocationEntry *entry;
    GtkEditable *editable;
    char *suffix, *user_location, *absolute_location, *uri_scheme;
    int user_location_length, pos;

    entry = CAJA_LOCATION_ENTRY (callback_data);
    editable = GTK_EDITABLE (entry);
    user_location = gtk_editable_get_chars (editable, 0, -1);
    user_location_length = g_utf8_strlen (user_location, -1);
    entry->details->idle_id = 0;

    uri_scheme = g_uri_parse_scheme (user_location);

    if (!g_path_is_absolute (user_location) && uri_scheme == NULL && user_location[0] != '~')
    {
        absolute_location = g_build_filename (entry->details->current_directory, user_location, NULL);
        suffix = g_filename_completer_get_completion_suffix (entry->details->completer,
                 absolute_location);
        g_free (absolute_location);
    }
    else
    {
        suffix = g_filename_completer_get_completion_suffix (entry->details->completer,
                 user_location);
    }

    g_free (user_location);
    g_free (uri_scheme);

    /* if we've got something, add it to the entry */
    if (suffix != NULL)
    {
        pos = user_location_length;
        gtk_editable_insert_text (editable,
                                  suffix, -1,  &pos);
        pos = user_location_length;
        gtk_editable_select_region (editable, pos, -1);

        g_free (suffix);
    }

    return FALSE;
}

/* Until we have a more elegant solution, this is how we figure out if
 * the GtkEntry inserted characters, assuming that the return value is
 * TRUE indicating that the GtkEntry consumed the key event for some
 * reason. This is a clone of code from GtkEntry.
 */
static gboolean
entry_would_have_inserted_characters (const GdkEventKey *event)
{
    switch (event->keyval)
    {
    case GDK_KEY_BackSpace:
    case GDK_KEY_Clear:
    case GDK_KEY_Insert:
    case GDK_KEY_Delete:
    case GDK_KEY_Home:
    case GDK_KEY_End:
    case GDK_KEY_KP_Home:
    case GDK_KEY_KP_End:
    case GDK_KEY_Left:
    case GDK_KEY_Right:
    case GDK_KEY_KP_Left:
    case GDK_KEY_KP_Right:
    case GDK_KEY_Return:
        return FALSE;
    default:
        if (event->keyval >= 0x20 && event->keyval <= 0xFF)
        {
            if ((event->state & GDK_CONTROL_MASK) != 0)
            {
                return FALSE;
            }
            if ((event->state & GDK_MOD1_MASK) != 0)
            {
                return FALSE;
            }
        }
        return event->length > 0;
    }
}

static int
get_editable_number_of_chars (GtkEditable *editable)
{
    char *text;
    int length;

    text = gtk_editable_get_chars (editable, 0, -1);
    length = g_utf8_strlen (text, -1);
    g_free (text);
    return length;
}

static void
set_position_and_selection_to_end (GtkEditable *editable)
{
    int end;

    end = get_editable_number_of_chars (editable);
    gtk_editable_select_region (editable, end, end);
    gtk_editable_set_position (editable, end);
}

static gboolean
position_and_selection_are_at_end (GtkEditable *editable)
{
    int end;
    int start_sel, end_sel;

    end = get_editable_number_of_chars (editable);
    if (gtk_editable_get_selection_bounds (editable, &start_sel, &end_sel))
    {
        if (start_sel != end || end_sel != end)
        {
            return FALSE;
        }
    }
    return gtk_editable_get_position (editable) == end;
}

static void
got_completion_data_callback (GFilenameCompleter *completer,
                              CajaLocationEntry *entry)
{
    if (entry->details->idle_id)
    {
        g_source_remove (entry->details->idle_id);
        entry->details->idle_id = 0;
    }
    try_to_expand_path (entry);
}

static void
editable_event_after_callback (GtkEntry *entry,
                               GdkEvent *event,
                               CajaLocationEntry *location_entry)
{
    GtkEditable *editable;
    GdkEventKey *keyevent;

    if (event->type != GDK_KEY_PRESS)
    {
        return;
    }

    editable = GTK_EDITABLE (entry);
    keyevent = (GdkEventKey *)event;

    /* After typing the right arrow key we move the selection to
     * the end, if we have a valid selection - since this is most
     * likely an auto-completion. We ignore shift / control since
     * they can validly be used to extend the selection.
     */
	if ((keyevent->keyval == GDK_KEY_Right || keyevent->keyval == GDK_KEY_End) &&
            !(keyevent->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) &&
            gtk_editable_get_selection_bounds (editable, NULL, NULL))
    {
        set_position_and_selection_to_end (editable);
    }

    /* Only do expanding when we are typing at the end of the
     * text. Do the expand at idle time to avoid slowing down
     * typing when the directory is large. Only trigger the expand
     * when we type a key that would have inserted characters.
     */
    if (position_and_selection_are_at_end (editable))
    {
        if (entry_would_have_inserted_characters (keyevent))
        {
            if (location_entry->details->idle_id == 0)
            {
                location_entry->details->idle_id = g_idle_add (try_to_expand_path, location_entry);
            }
        }
    }
    else
    {
        /* FIXME: Also might be good to do this when you click
         * to change the position or selection.
         */
        if (location_entry->details->idle_id != 0)
        {
            g_source_remove (location_entry->details->idle_id);
            location_entry->details->idle_id = 0;
        }
    }
}

static void
finalize (GObject *object)
{
    CajaLocationEntry *entry;

    entry = CAJA_LOCATION_ENTRY (object);

    g_object_unref (entry->details->completer);
    g_free (entry->details->special_text);
    g_free (entry->details);

    EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
#if GTK_CHECK_VERSION (3, 0, 0)
destroy (GtkWidget *object)
#else
destroy (GtkObject *object)
#endif
{
    CajaLocationEntry *entry;

    entry = CAJA_LOCATION_ENTRY (object);

    /* cancel the pending idle call, if any */
    if (entry->details->idle_id != 0)
    {
        g_source_remove (entry->details->idle_id);
        entry->details->idle_id = 0;
    }

    g_free (entry->details->current_directory);
    entry->details->current_directory = NULL;

#if GTK_CHECK_VERSION (3, 0, 0)
    EEL_CALL_PARENT (GTK_WIDGET_CLASS, destroy, (object));
#else
    EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
#endif
}

static void
caja_location_entry_text_changed (CajaLocationEntry *entry,
                                  GParamSpec            *pspec)
{
    if (entry->details->setting_special_text)
    {
        return;
    }

    entry->details->has_special_text = FALSE;
}

static void
caja_location_entry_icon_release (GtkEntry *gentry,
                                  GtkEntryIconPosition position,
                                  GdkEvent *event,
                                  gpointer unused)
{
    switch (CAJA_LOCATION_ENTRY (gentry)->details->secondary_action)
    {
    case CAJA_LOCATION_ENTRY_ACTION_GOTO:
        g_signal_emit_by_name (gentry, "activate", gentry);
        break;
    case CAJA_LOCATION_ENTRY_ACTION_CLEAR:
        gtk_entry_set_text (gentry, "");
        break;
    default:
        g_assert_not_reached ();
    }
}

static gboolean
caja_location_entry_focus_in (GtkWidget     *widget,
                              GdkEventFocus *event)
{
    CajaLocationEntry *entry = CAJA_LOCATION_ENTRY (widget);

    if (entry->details->has_special_text)
    {
        entry->details->setting_special_text = TRUE;
        gtk_entry_set_text (GTK_ENTRY (entry), "");
        entry->details->setting_special_text = FALSE;
    }

    return EEL_CALL_PARENT_WITH_RETURN_VALUE (GTK_WIDGET_CLASS, focus_in_event, (widget, event));
}

static void
caja_location_entry_activate (GtkEntry *entry)
{
    CajaLocationEntry *loc_entry;
    const gchar *entry_text;
    gchar *full_path, *uri_scheme = NULL;

    loc_entry = CAJA_LOCATION_ENTRY (entry);
    entry_text = gtk_entry_get_text (entry);

    if (entry_text != NULL && *entry_text != '\0')
    {
        uri_scheme = g_uri_parse_scheme (entry_text);

        if (!g_path_is_absolute (entry_text) && uri_scheme == NULL && entry_text[0] != '~')
        {
            /* Fix non absolute paths */
            full_path = g_build_filename (loc_entry->details->current_directory, entry_text, NULL);
            gtk_entry_set_text (entry, full_path);
            g_free (full_path);
        }

        g_free (uri_scheme);
    }

    EEL_CALL_PARENT (GTK_ENTRY_CLASS, activate, (entry));
}

static void
caja_location_entry_class_init (CajaLocationEntryClass *class)
{
    GTK_WIDGET_CLASS (class)->focus_in_event = caja_location_entry_focus_in;

#if GTK_CHECK_VERSION (3, 0, 0)
    GTK_WIDGET_CLASS (class)->destroy = destroy;
#else
    GTK_OBJECT_CLASS (class)->destroy = destroy;
#endif

    G_OBJECT_CLASS (class)->finalize = finalize;

    GTK_ENTRY_CLASS (class)->activate = caja_location_entry_activate;
}

void
caja_location_entry_update_current_location (CajaLocationEntry *entry,
        const char *location)
{
    g_free (entry->details->current_directory);
    entry->details->current_directory = g_strdup (location);

    caja_entry_set_text (CAJA_ENTRY (entry), location);
    set_position_and_selection_to_end (GTK_EDITABLE (entry));
}

void
caja_location_entry_set_secondary_action (CajaLocationEntry *entry,
        CajaLocationEntryAction secondary_action)
{
    if (entry->details->secondary_action == secondary_action)
    {
        return;
    }
    switch (secondary_action)
    {
    case CAJA_LOCATION_ENTRY_ACTION_CLEAR:
        gtk_entry_set_icon_from_stock (GTK_ENTRY (entry),
                                       GTK_ENTRY_ICON_SECONDARY,
                                       GTK_STOCK_CLEAR);
        break;
    case CAJA_LOCATION_ENTRY_ACTION_GOTO:
        gtk_entry_set_icon_from_stock (GTK_ENTRY (entry),
                                       GTK_ENTRY_ICON_SECONDARY,
                                       GTK_STOCK_GO_FORWARD);
        break;
    default:
        g_assert_not_reached ();
    }
    entry->details->secondary_action = secondary_action;
}

static void
caja_location_entry_init (CajaLocationEntry *entry)
{
    entry->details = g_new0 (CajaLocationEntryDetails, 1);

    entry->details->completer = g_filename_completer_new ();
    g_filename_completer_set_dirs_only (entry->details->completer, TRUE);

    caja_location_entry_set_secondary_action (entry,
            CAJA_LOCATION_ENTRY_ACTION_CLEAR);

    caja_entry_set_special_tab_handling (CAJA_ENTRY (entry), TRUE);

    g_signal_connect (entry, "event_after",
                      G_CALLBACK (editable_event_after_callback), entry);

    g_signal_connect (entry, "notify::text",
                      G_CALLBACK (caja_location_entry_text_changed), NULL);

    g_signal_connect (entry, "icon-release",
                      G_CALLBACK (caja_location_entry_icon_release), NULL);

    g_signal_connect (entry->details->completer, "got_completion_data",
                      G_CALLBACK (got_completion_data_callback), entry);
}

GtkWidget *
caja_location_entry_new (void)
{
    GtkWidget *entry;

    entry = gtk_widget_new (CAJA_TYPE_LOCATION_ENTRY, NULL);

    return entry;
}

void
caja_location_entry_set_special_text (CajaLocationEntry *entry,
                                      const char            *special_text)
{
    entry->details->has_special_text = TRUE;

    g_free (entry->details->special_text);
    entry->details->special_text = g_strdup (special_text);

    entry->details->setting_special_text = TRUE;
    gtk_entry_set_text (GTK_ENTRY (entry), special_text);
    entry->details->setting_special_text = FALSE;
}

