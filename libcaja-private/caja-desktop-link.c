/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-desktop-link.c: Class that handles the links on the desktop

   Copyright (C) 2003 Red Hat, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Author: Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <string.h>

#include "caja-desktop-link.h"
#include "caja-desktop-link-monitor.h"
#include "caja-desktop-icon-file.h"
#include "caja-directory-private.h"
#include "caja-desktop-directory.h"
#include "caja-icon-names.h"
#include "caja-file-utilities.h"
#include "caja-trash-monitor.h"
#include "caja-global-preferences.h"

struct _CajaDesktopLinkPrivate
{
    CajaDesktopLinkType type;
    char *filename;
    char *display_name;
    GFile *activation_location;
    GIcon *icon;

    CajaDesktopIconFile *icon_file;

    GObject *signal_handler_obj;
    gulong signal_handler;

    /* Just for mount icons: */
    GMount *mount;
};

G_DEFINE_TYPE_WITH_PRIVATE (CajaDesktopLink, caja_desktop_link, G_TYPE_OBJECT)

static void
create_icon_file (CajaDesktopLink *link)
{
    link->details->icon_file = caja_desktop_icon_file_new (link);
}

static void
caja_desktop_link_changed (CajaDesktopLink *link)
{
    if (link->details->icon_file != NULL)
    {
        caja_desktop_icon_file_update (link->details->icon_file);
    }
}

static void
mount_changed_callback (GMount *mount, CajaDesktopLink *link)
{
    g_free (link->details->display_name);
    if (link->details->activation_location)
    {
        g_object_unref (link->details->activation_location);
    }
    if (link->details->icon)
    {
        g_object_unref (link->details->icon);
    }

    link->details->display_name = g_mount_get_name (mount);
    link->details->activation_location = g_mount_get_default_location (mount);
    link->details->icon = g_mount_get_icon (mount);

    caja_desktop_link_changed (link);
}

static void
caja_desktop_link_ensure_display_name (CajaDesktopLink *link)
{
    if (link->details->display_name[0] == 0) {
        g_free (link->details->display_name);

        switch (link->details->type)
        {
        case CAJA_DESKTOP_LINK_HOME:
            /* Translators: If it's hard to compose a good home
             * icon name from the user name, you can use a string without
             * an "%s" here, in which case the home icon name will not
             * include the user's name, which should be fine. To avoid a
             * warning, put "%.0s" somewhere in the string, which will
             * match the user name string passed by the C code, but not
             * put the user name in the final string.
             */
            link->details->display_name = g_strdup_printf (_("%s's Home"), g_get_user_name ());
            break;
        case CAJA_DESKTOP_LINK_COMPUTER:
            link->details->display_name = g_strdup (_("Computer"));
            break;
        case CAJA_DESKTOP_LINK_NETWORK:
            link->details->display_name = g_strdup (_("Network Servers"));
            break;
        case CAJA_DESKTOP_LINK_TRASH:
            link->details->display_name = g_strdup (_("Trash"));
            break;
        default:
        case CAJA_DESKTOP_LINK_MOUNT:
            g_assert_not_reached();
        }
    }
}

static void
trash_state_changed_callback (CajaTrashMonitor *trash_monitor,
                              gboolean state,
                              gpointer callback_data)
{
    CajaDesktopLink *link;

    link = CAJA_DESKTOP_LINK (callback_data);
    g_assert (link->details->type == CAJA_DESKTOP_LINK_TRASH);

    if (link->details->icon)
    {
        g_object_unref (link->details->icon);
    }
    link->details->icon = caja_trash_monitor_get_icon ();

    caja_desktop_link_changed (link);
}

static void
home_name_changed (gpointer callback_data)
{
    CajaDesktopLink *link;

    link = CAJA_DESKTOP_LINK (callback_data);
    g_assert (link->details->type == CAJA_DESKTOP_LINK_HOME);

    g_free (link->details->display_name);

    link->details->display_name = g_settings_get_string (caja_desktop_preferences,
                                                         CAJA_PREFERENCES_DESKTOP_HOME_NAME);

    caja_desktop_link_ensure_display_name (link);
    caja_desktop_link_changed (link);
}

static void
computer_name_changed (gpointer callback_data)
{
    CajaDesktopLink *link;

    link = CAJA_DESKTOP_LINK (callback_data);
    g_assert (link->details->type == CAJA_DESKTOP_LINK_COMPUTER);

    g_free (link->details->display_name);
    link->details->display_name = g_settings_get_string (caja_desktop_preferences, CAJA_PREFERENCES_DESKTOP_COMPUTER_NAME);

    caja_desktop_link_ensure_display_name (link);
    caja_desktop_link_changed (link);
}

static void
trash_name_changed (gpointer callback_data)
{
    CajaDesktopLink *link;

    link = CAJA_DESKTOP_LINK (callback_data);
    g_assert (link->details->type == CAJA_DESKTOP_LINK_TRASH);

    g_free (link->details->display_name);
    link->details->display_name = g_settings_get_string (caja_desktop_preferences, CAJA_PREFERENCES_DESKTOP_TRASH_NAME);

    caja_desktop_link_ensure_display_name (link);
    caja_desktop_link_changed (link);
}

static void
network_name_changed (gpointer callback_data)
{
    CajaDesktopLink *link;

    link = CAJA_DESKTOP_LINK (callback_data);
    g_assert (link->details->type == CAJA_DESKTOP_LINK_NETWORK);

    g_free (link->details->display_name);
    link->details->display_name = g_settings_get_string (caja_desktop_preferences, CAJA_PREFERENCES_DESKTOP_NETWORK_NAME);

    caja_desktop_link_ensure_display_name (link);
    caja_desktop_link_changed (link);
}

CajaDesktopLink *
caja_desktop_link_new (CajaDesktopLinkType type)
{
    CajaDesktopLink *link;

    link = CAJA_DESKTOP_LINK (g_object_new (CAJA_TYPE_DESKTOP_LINK, NULL));

    link->details->type = type;
    switch (type)
    {
    case CAJA_DESKTOP_LINK_HOME:
        link->details->filename = g_strdup ("home");
        link->details->display_name = g_settings_get_string (caja_desktop_preferences, CAJA_PREFERENCES_DESKTOP_HOME_NAME);
        link->details->activation_location = g_file_new_for_path (g_get_home_dir ());
        link->details->icon = g_themed_icon_new (CAJA_ICON_HOME);

        g_signal_connect_swapped (caja_desktop_preferences,
                                  "changed::" CAJA_PREFERENCES_DESKTOP_HOME_NAME,
                                  G_CALLBACK (home_name_changed),
                                  link);

        break;

    case CAJA_DESKTOP_LINK_COMPUTER:
        link->details->filename = g_strdup ("computer");
        link->details->display_name = g_settings_get_string (caja_desktop_preferences, CAJA_PREFERENCES_DESKTOP_COMPUTER_NAME);
        link->details->activation_location = g_file_new_for_uri ("computer:///");
        /* TODO: This might need a different icon: */
        link->details->icon = g_themed_icon_new (CAJA_ICON_COMPUTER);

        g_signal_connect_swapped (caja_desktop_preferences,
                                  "changed::" CAJA_PREFERENCES_DESKTOP_COMPUTER_NAME,
                                  G_CALLBACK (computer_name_changed),
                                  link);

        break;

    case CAJA_DESKTOP_LINK_TRASH:
        link->details->filename = g_strdup ("trash");
        link->details->display_name = g_settings_get_string (caja_desktop_preferences, CAJA_PREFERENCES_DESKTOP_TRASH_NAME);
        link->details->activation_location = g_file_new_for_uri (EEL_TRASH_URI);
        link->details->icon = caja_trash_monitor_get_icon ();

        g_signal_connect_swapped (caja_desktop_preferences,
                                  "changed::" CAJA_PREFERENCES_DESKTOP_TRASH_NAME,
                                  G_CALLBACK (trash_name_changed),
                                  link);
        link->details->signal_handler_obj = G_OBJECT (caja_trash_monitor_get ());
        link->details->signal_handler =
            g_signal_connect_object (caja_trash_monitor_get (), "trash_state_changed",
                                     G_CALLBACK (trash_state_changed_callback), link, 0);
        break;

    case CAJA_DESKTOP_LINK_NETWORK:
        link->details->filename = g_strdup ("network");
        link->details->display_name = g_settings_get_string (caja_desktop_preferences, CAJA_PREFERENCES_DESKTOP_NETWORK_NAME);
        link->details->activation_location = g_file_new_for_uri ("network:///");
        link->details->icon = g_themed_icon_new (CAJA_ICON_NETWORK);

        g_signal_connect_swapped (caja_desktop_preferences,
                                  "changed::" CAJA_PREFERENCES_DESKTOP_NETWORK_NAME,
                                  G_CALLBACK (network_name_changed),
                                  link);
        break;

    default:
    case CAJA_DESKTOP_LINK_MOUNT:
        g_assert_not_reached();
    }

    caja_desktop_link_ensure_display_name (link);
    create_icon_file (link);

    return link;
}

CajaDesktopLink *
caja_desktop_link_new_from_mount (GMount *mount)
{
    CajaDesktopLink *link;
    GVolume *volume;
    char *name, *filename;

    link = CAJA_DESKTOP_LINK (g_object_new (CAJA_TYPE_DESKTOP_LINK, NULL));

    link->details->type = CAJA_DESKTOP_LINK_MOUNT;

    link->details->mount = g_object_ref (mount);

    /* We try to use the drive name to get somewhat stable filenames
       for metadata */
    volume = g_mount_get_volume (mount);
    if (volume != NULL)
    {
        name = g_volume_get_name (volume);
        g_object_unref (volume);
    }
    else
    {
        name = g_mount_get_name (mount);
    }

    /* Replace slashes in name */
    filename = g_strconcat (g_strdelimit (name, "/", '-'), ".volume", NULL);
    link->details->filename =
        caja_desktop_link_monitor_make_filename_unique (caja_desktop_link_monitor_get (),
                filename);
    g_free (filename);
    g_free (name);

    link->details->display_name = g_mount_get_name (mount);

    link->details->activation_location = g_mount_get_default_location (mount);
    link->details->icon = g_mount_get_icon (mount);

    link->details->signal_handler_obj = G_OBJECT (mount);
    link->details->signal_handler =
        g_signal_connect (mount, "changed",
                          G_CALLBACK (mount_changed_callback), link);

    create_icon_file (link);

    return link;
}

GMount *
caja_desktop_link_get_mount (CajaDesktopLink *link)
{
    if (link->details->mount)
    {
        return g_object_ref (link->details->mount);
    }
    return NULL;
}

CajaDesktopLinkType
caja_desktop_link_get_link_type (CajaDesktopLink *link)
{
    return link->details->type;
}

char *
caja_desktop_link_get_file_name (CajaDesktopLink *link)
{
    return g_strdup (link->details->filename);
}

char *
caja_desktop_link_get_display_name (CajaDesktopLink *link)
{
    return g_strdup (link->details->display_name);
}

GIcon *
caja_desktop_link_get_icon (CajaDesktopLink *link)
{
    if (link->details->icon != NULL)
    {
        return g_object_ref (link->details->icon);
    }
    return NULL;
}

GFile *
caja_desktop_link_get_activation_location (CajaDesktopLink *link)
{
    if (link->details->activation_location)
    {
        return g_object_ref (link->details->activation_location);
    }
    return NULL;
}

char *
caja_desktop_link_get_activation_uri (CajaDesktopLink *link)
{
    if (link->details->activation_location)
    {
        return g_file_get_uri (link->details->activation_location);
    }
    return NULL;
}


gboolean
caja_desktop_link_get_date (CajaDesktopLink *link,
                            CajaDateType     date_type,
                            time_t               *date)
{
    return FALSE;
}

gboolean
caja_desktop_link_can_rename (CajaDesktopLink     *link)
{
    return (link->details->type == CAJA_DESKTOP_LINK_HOME ||
            link->details->type == CAJA_DESKTOP_LINK_TRASH ||
            link->details->type == CAJA_DESKTOP_LINK_NETWORK ||
            link->details->type == CAJA_DESKTOP_LINK_COMPUTER);
}

gboolean
caja_desktop_link_rename (CajaDesktopLink     *link,
                          const char              *name)
{
    switch (link->details->type)
    {
    case CAJA_DESKTOP_LINK_HOME:
        g_settings_set_string (caja_desktop_preferences,
                               CAJA_PREFERENCES_DESKTOP_HOME_NAME,
                               name);
        break;
    case CAJA_DESKTOP_LINK_COMPUTER:
        g_settings_set_string (caja_desktop_preferences,
                               CAJA_PREFERENCES_DESKTOP_COMPUTER_NAME,
                               name);
        break;
    case CAJA_DESKTOP_LINK_TRASH:
        g_settings_set_string (caja_desktop_preferences,
                               CAJA_PREFERENCES_DESKTOP_TRASH_NAME,
                               name);
        break;
    case CAJA_DESKTOP_LINK_NETWORK:
        g_settings_set_string (caja_desktop_preferences,
                               CAJA_PREFERENCES_DESKTOP_NETWORK_NAME,
                               name);
        break;
    default:
        g_assert_not_reached ();
        /* FIXME: Do we want volume renaming?
         * We didn't support that before. */
        break;
    }

    return TRUE;
}

static void
caja_desktop_link_init (CajaDesktopLink *link)
{
    link->details = caja_desktop_link_get_instance_private (link);
}

static void
desktop_link_finalize (GObject *object)
{
    CajaDesktopLink *link;

    link = CAJA_DESKTOP_LINK (object);

    if (link->details->signal_handler != 0)
    {
        if (g_signal_handler_is_connected(link->details->signal_handler_obj,
                                             link->details->signal_handler)){
            g_signal_handler_disconnect (link->details->signal_handler_obj,
                                             link->details->signal_handler);
        }
    }

    if (link->details->icon_file != NULL)
    {
        caja_desktop_icon_file_remove (link->details->icon_file);
        caja_file_unref (CAJA_FILE (link->details->icon_file));
        link->details->icon_file = NULL;
    }

    if (link->details->type == CAJA_DESKTOP_LINK_HOME)
    {
        g_signal_handlers_disconnect_by_func (caja_desktop_preferences,
                                         home_name_changed,
                                         link);
    }

    if (link->details->type == CAJA_DESKTOP_LINK_COMPUTER)
    {
        g_signal_handlers_disconnect_by_func (caja_desktop_preferences,
                                         computer_name_changed,
                                         link);
    }

    if (link->details->type == CAJA_DESKTOP_LINK_TRASH)
    {
        g_signal_handlers_disconnect_by_func (caja_desktop_preferences,
                                         trash_name_changed,
                                         link);
    }

    if (link->details->type == CAJA_DESKTOP_LINK_NETWORK)
    {
        g_signal_handlers_disconnect_by_func (caja_desktop_preferences,
                                         network_name_changed,
                                         link);
    }

    if (link->details->type == CAJA_DESKTOP_LINK_MOUNT)
    {
        g_object_unref (link->details->mount);
    }

    g_free (link->details->filename);
    g_free (link->details->display_name);
    if (link->details->activation_location)
    {
        g_object_unref (link->details->activation_location);
    }
    if (link->details->icon)
    {
        g_object_unref (link->details->icon);
    }

    G_OBJECT_CLASS (caja_desktop_link_parent_class)->finalize (object);
}

static void
caja_desktop_link_class_init (CajaDesktopLinkClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = desktop_link_finalize;
}
