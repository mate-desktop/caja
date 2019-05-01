/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-desktop-link-monitor.c: singleton thatn manages the links

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
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <string.h>

#include <eel/eel-debug.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-stock-dialogs.h>

#include "caja-desktop-link-monitor.h"
#include "caja-desktop-link.h"
#include "caja-desktop-icon-file.h"
#include "caja-directory.h"
#include "caja-desktop-directory.h"
#include "caja-global-preferences.h"
#include "caja-trash-monitor.h"

struct CajaDesktopLinkMonitorDetails
{
    GVolumeMonitor *volume_monitor;
    CajaDirectory *desktop_dir;

    CajaDesktopLink *home_link;
    CajaDesktopLink *computer_link;
    CajaDesktopLink *trash_link;
    CajaDesktopLink *network_link;

    gulong mount_id;
    gulong unmount_id;
    gulong changed_id;

    GList *mount_links;
};


static void caja_desktop_link_monitor_init       (gpointer              object,
        gpointer              klass);
static void caja_desktop_link_monitor_class_init (gpointer              klass);

EEL_CLASS_BOILERPLATE (CajaDesktopLinkMonitor,
                       caja_desktop_link_monitor,
                       G_TYPE_OBJECT)

static CajaDesktopLinkMonitor *the_link_monitor = NULL;

static void
destroy_desktop_link_monitor (void)
{
    if (the_link_monitor != NULL)
    {
        g_object_unref (the_link_monitor);
    }
}

CajaDesktopLinkMonitor *
caja_desktop_link_monitor_get (void)
{
    if (the_link_monitor == NULL)
    {
        g_object_new (CAJA_TYPE_DESKTOP_LINK_MONITOR, NULL);
        eel_debug_call_at_shutdown (destroy_desktop_link_monitor);
    }
    return the_link_monitor;
}

static gboolean
volume_file_name_used (CajaDesktopLinkMonitor *monitor,
                       const char *name)
{
    GList *l;
    gboolean same;

    for (l = monitor->details->mount_links; l != NULL; l = l->next)
    {
        char *other_name;

        other_name = caja_desktop_link_get_file_name (l->data);
        same = strcmp (name, other_name) == 0;
        g_free (other_name);

        if (same)
        {
            return TRUE;
        }
    }

    return FALSE;
}

char *
caja_desktop_link_monitor_make_filename_unique (CajaDesktopLinkMonitor *monitor,
        const char *filename)
{
    char *unique_name;
    int i;

    i = 2;
    unique_name = g_strdup (filename);
    while (volume_file_name_used (monitor, unique_name))
    {
        g_free (unique_name);
        unique_name = g_strdup_printf ("%s.%d", filename, i++);
    }
    return unique_name;
}

static gboolean
has_mount (CajaDesktopLinkMonitor *monitor,
           GMount                     *mount)
{
    gboolean ret;
    GList *l;
    GMount *other_mount = NULL;

    ret = FALSE;

    for (l = monitor->details->mount_links; l != NULL; l = l->next)
    {
        other_mount = caja_desktop_link_get_mount (l->data);
        if (mount == other_mount)
        {
            g_object_unref (other_mount);
            ret = TRUE;
            break;
        }
        g_object_unref (other_mount);
    }

    return ret;
}

static void
create_mount_link (CajaDesktopLinkMonitor *monitor,
                   GMount *mount)
{
    if (has_mount (monitor, mount))
        return;

    if ((!g_mount_is_shadowed (mount)) &&
            g_settings_get_boolean (caja_desktop_preferences, CAJA_PREFERENCES_DESKTOP_VOLUMES_VISIBLE))
    {
        CajaDesktopLink *link;

        link = caja_desktop_link_new_from_mount (mount);
        monitor->details->mount_links = g_list_prepend (monitor->details->mount_links, link);
    }
}

static void
remove_mount_link (CajaDesktopLinkMonitor *monitor,
                   GMount *mount)
{
    GList *l;
    CajaDesktopLink *link;
    GMount *other_mount = NULL;

    link = NULL;
    for (l = monitor->details->mount_links; l != NULL; l = l->next)
    {
        other_mount = caja_desktop_link_get_mount (l->data);
        if (mount == other_mount)
        {
            g_object_unref (other_mount);
            link = l->data;
            break;
        }
        g_object_unref (other_mount);
    }

    if (link)
    {
        monitor->details->mount_links = g_list_remove (monitor->details->mount_links, link);
        g_object_unref (link);
    }
}



static void
mount_added_callback (GVolumeMonitor *volume_monitor,
                      GMount *mount,
                      CajaDesktopLinkMonitor *monitor)
{
    create_mount_link (monitor, mount);
}


static void
mount_removed_callback (GVolumeMonitor *volume_monitor,
                        GMount *mount,
                        CajaDesktopLinkMonitor *monitor)
{
    remove_mount_link (monitor, mount);
}

static void
mount_changed_callback (GVolumeMonitor *volume_monitor,
                        GMount *mount,
                        CajaDesktopLinkMonitor *monitor)
{
    /* TODO: update the mount with other details */

    /* remove a mount if it goes into the shadows */
    if (g_mount_is_shadowed (mount) && has_mount (monitor, mount))
    {
        remove_mount_link (monitor, mount);
    }
}

static void
update_link_visibility (CajaDesktopLinkMonitor *monitor,
                        CajaDesktopLink       **link_ref,
                        CajaDesktopLinkType     link_type,
                        const char                 *preference_key)
{
    if (g_settings_get_boolean (caja_desktop_preferences, preference_key))
    {
        if (*link_ref == NULL)
        {
            *link_ref = caja_desktop_link_new (link_type);
        }
    }
    else
    {
        if (*link_ref != NULL)
        {
            g_object_unref (*link_ref);
            *link_ref = NULL;
        }
    }
}

static void
desktop_home_visible_changed (gpointer callback_data)
{
    CajaDesktopLinkMonitor *monitor;

    monitor = CAJA_DESKTOP_LINK_MONITOR (callback_data);

    update_link_visibility (CAJA_DESKTOP_LINK_MONITOR (monitor),
                            &monitor->details->home_link,
                            CAJA_DESKTOP_LINK_HOME,
                            CAJA_PREFERENCES_DESKTOP_HOME_VISIBLE);
}

static void
desktop_computer_visible_changed (gpointer callback_data)
{
    CajaDesktopLinkMonitor *monitor;

    monitor = CAJA_DESKTOP_LINK_MONITOR (callback_data);

    update_link_visibility (CAJA_DESKTOP_LINK_MONITOR (callback_data),
                            &monitor->details->computer_link,
                            CAJA_DESKTOP_LINK_COMPUTER,
                            CAJA_PREFERENCES_DESKTOP_COMPUTER_VISIBLE);
}

static void
desktop_trash_visible_changed (gpointer callback_data)
{
    CajaDesktopLinkMonitor *monitor;

    monitor = CAJA_DESKTOP_LINK_MONITOR (callback_data);

    update_link_visibility (CAJA_DESKTOP_LINK_MONITOR (callback_data),
                            &monitor->details->trash_link,
                            CAJA_DESKTOP_LINK_TRASH,
                            CAJA_PREFERENCES_DESKTOP_TRASH_VISIBLE);
}

static void
desktop_network_visible_changed (gpointer callback_data)
{
    CajaDesktopLinkMonitor *monitor;

    monitor = CAJA_DESKTOP_LINK_MONITOR (callback_data);

    update_link_visibility (CAJA_DESKTOP_LINK_MONITOR (callback_data),
                            &monitor->details->network_link,
                            CAJA_DESKTOP_LINK_NETWORK,
                            CAJA_PREFERENCES_DESKTOP_NETWORK_VISIBLE);
}

static void
desktop_volumes_visible_changed (gpointer callback_data)
{
    CajaDesktopLinkMonitor *monitor;
    GList *l, *mounts;

    monitor = CAJA_DESKTOP_LINK_MONITOR (callback_data);

    if (g_settings_get_boolean (caja_desktop_preferences, CAJA_PREFERENCES_DESKTOP_VOLUMES_VISIBLE))
    {
        if (monitor->details->mount_links == NULL)
        {
            mounts = g_volume_monitor_get_mounts (monitor->details->volume_monitor);
            for (l = mounts; l != NULL; l = l->next)
            {
                create_mount_link (monitor, l->data);
                g_object_unref (l->data);
            }
            g_list_free (mounts);
        }
    }
    else
    {
        g_list_foreach (monitor->details->mount_links, (GFunc)g_object_unref, NULL);
        g_list_free (monitor->details->mount_links);
        monitor->details->mount_links = NULL;
    }
}

static void
create_link_and_add_preference (CajaDesktopLink   **link_ref,
                                CajaDesktopLinkType link_type,
                                const char         *preference_key,
                                GCallback           callback,
                                gpointer            callback_data)
{
    gchar *detailed_signal;

    if (g_settings_get_boolean (caja_desktop_preferences, preference_key))
    {
        *link_ref = caja_desktop_link_new (link_type);
    }

    detailed_signal = g_strconcat ("changed::", preference_key, NULL);
    g_signal_connect_swapped (caja_desktop_preferences,
                              detailed_signal,
                              callback, callback_data);
    g_free (detailed_signal);
}

static void
caja_desktop_link_monitor_init (gpointer object, gpointer klass)
{
    CajaDesktopLinkMonitor *monitor;
    GList *l, *mounts;
    GMount *mount = NULL;

    monitor = CAJA_DESKTOP_LINK_MONITOR (object);

    the_link_monitor = monitor;

    monitor->details = g_new0 (CajaDesktopLinkMonitorDetails, 1);

    monitor->details->volume_monitor = g_volume_monitor_get ();

    /* We keep around a ref to the desktop dir */
    monitor->details->desktop_dir = caja_directory_get_by_uri (EEL_DESKTOP_URI);

    /* Default links */

    create_link_and_add_preference (&monitor->details->home_link,
                                    CAJA_DESKTOP_LINK_HOME,
                                    CAJA_PREFERENCES_DESKTOP_HOME_VISIBLE,
                                    G_CALLBACK (desktop_home_visible_changed),
                                    monitor);

    create_link_and_add_preference (&monitor->details->computer_link,
                                    CAJA_DESKTOP_LINK_COMPUTER,
                                    CAJA_PREFERENCES_DESKTOP_COMPUTER_VISIBLE,
                                    G_CALLBACK (desktop_computer_visible_changed),
                                    monitor);

    create_link_and_add_preference (&monitor->details->trash_link,
                                    CAJA_DESKTOP_LINK_TRASH,
                                    CAJA_PREFERENCES_DESKTOP_TRASH_VISIBLE,
                                    G_CALLBACK (desktop_trash_visible_changed),
                                    monitor);

    create_link_and_add_preference (&monitor->details->network_link,
                                    CAJA_DESKTOP_LINK_NETWORK,
                                    CAJA_PREFERENCES_DESKTOP_NETWORK_VISIBLE,
                                    G_CALLBACK (desktop_network_visible_changed),
                                    monitor);

    /* Mount links */

    mounts = g_volume_monitor_get_mounts (monitor->details->volume_monitor);
    for (l = mounts; l != NULL; l = l->next)
    {
        mount = l->data;
        create_mount_link (monitor, mount);
        g_object_unref (mount);
    }
    g_list_free (mounts);

    g_signal_connect_swapped (caja_desktop_preferences,
                              "changed::" CAJA_PREFERENCES_DESKTOP_VOLUMES_VISIBLE,
                              G_CALLBACK (desktop_volumes_visible_changed),
                              monitor);

    monitor->details->mount_id =
        g_signal_connect_object (monitor->details->volume_monitor, "mount_added",
                                 G_CALLBACK (mount_added_callback), monitor, 0);
    monitor->details->unmount_id =
        g_signal_connect_object (monitor->details->volume_monitor, "mount_removed",
                                 G_CALLBACK (mount_removed_callback), monitor, 0);
    monitor->details->changed_id =
        g_signal_connect_object (monitor->details->volume_monitor, "mount_changed",
                                 G_CALLBACK (mount_changed_callback), monitor, 0);

}

static void
remove_link_and_preference (CajaDesktopLink       **link_ref,
                            const char             *preference_key,
                            GCallback               callback,
                            gpointer                callback_data)
{
    if (*link_ref != NULL)
    {
        g_object_unref (*link_ref);
        *link_ref = NULL;
    }

    g_signal_handlers_disconnect_by_func (caja_desktop_preferences,
                                          callback, callback_data);
}

static void
desktop_link_monitor_finalize (GObject *object)
{
    CajaDesktopLinkMonitor *monitor;

    monitor = CAJA_DESKTOP_LINK_MONITOR (object);

    g_object_unref (monitor->details->volume_monitor);

    /* Default links */

    remove_link_and_preference (&monitor->details->home_link,
                                CAJA_PREFERENCES_DESKTOP_HOME_VISIBLE,
                                G_CALLBACK (desktop_home_visible_changed),
                                monitor);

    remove_link_and_preference (&monitor->details->computer_link,
                                CAJA_PREFERENCES_DESKTOP_COMPUTER_VISIBLE,
                                G_CALLBACK (desktop_computer_visible_changed),
                                monitor);

    remove_link_and_preference (&monitor->details->trash_link,
                                CAJA_PREFERENCES_DESKTOP_TRASH_VISIBLE,
                                G_CALLBACK (desktop_trash_visible_changed),
                                monitor);

    remove_link_and_preference (&monitor->details->network_link,
                                CAJA_PREFERENCES_DESKTOP_NETWORK_VISIBLE,
                                G_CALLBACK (desktop_network_visible_changed),
                                monitor);

    /* Mounts */

    g_list_foreach (monitor->details->mount_links, (GFunc)g_object_unref, NULL);
    g_list_free (monitor->details->mount_links);
    monitor->details->mount_links = NULL;

    caja_directory_unref (monitor->details->desktop_dir);
    monitor->details->desktop_dir = NULL;

    g_signal_handlers_disconnect_by_func (caja_desktop_preferences,
                                          desktop_volumes_visible_changed,
                                          monitor);

/*  These sources are already gone,  this just causes errors
    if (monitor->details->mount_id != 0)
    {
        g_source_remove (monitor->details->mount_id);
    }
    if (monitor->details->unmount_id != 0)
    {
        g_source_remove (monitor->details->unmount_id);
    }
    if (monitor->details->changed_id != 0)
    {
        g_source_remove (monitor->details->changed_id);
    }
*/
    g_free (monitor->details);

    EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
caja_desktop_link_monitor_class_init (gpointer klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = desktop_link_monitor_finalize;

}
