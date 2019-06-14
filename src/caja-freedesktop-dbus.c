/*
 * caja-freedesktop-dbus: Implementation for the org.freedesktop DBus file-management interfaces
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
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Authors: Akshay Gupta <kitallis@gmail.com>
 *          Federico Mena Quintero <federico@gnome.org>
 *          Stefano Karapetsas <stefano@karapetsas.com>
 */

#include <config.h>

#include <gio/gio.h>

#include <libcaja-private/caja-debug-log.h>

#include "file-manager/fm-properties-window.h"

#include "caja-application.h"
#include "caja-freedesktop-dbus.h"
#include "caja-freedesktop-generated.h"

struct _CajaFreedesktopDBus {
    GObject parent;

    /* Id from g_dbus_own_name() */
    guint owner_id;

    /* DBus paraphernalia */
    GDBusObjectManagerServer *object_manager;

    /* Our DBus implementation skeleton */
    CajaFreedesktopFileManager1 *skeleton;

    /* Caja application */
    CajaApplication *application;
};

struct _CajaFreedesktopDBusClass {
    GObjectClass parent_class;
};

G_DEFINE_TYPE (CajaFreedesktopDBus, caja_freedesktop_dbus, G_TYPE_OBJECT);

static gboolean
skeleton_handle_show_items_cb (CajaFreedesktopFileManager1 *object,
                               GDBusMethodInvocation *invocation,
                               const gchar *const *uris,
                               const gchar *startup_id,
                               CajaFreedesktopDBus *fdb)
{
    CajaApplication *application;
    int i;

    application = CAJA_APPLICATION (fdb->application);

    for (i = 0; uris[i] != NULL; i++) {
        GFile *file;
        GFile *parent;

        file = g_file_new_for_uri (uris[i]);
        parent = g_file_get_parent (file);

        if (parent != NULL) {
            caja_application_open_location (application, parent, file, startup_id, 0);
            g_object_unref (parent);
        } else {
            caja_application_open_location (application, file, NULL, startup_id, 0);
        }

        g_object_unref (file);
    }

    caja_freedesktop_file_manager1_complete_show_items (object, invocation);
    return TRUE;
}

static gboolean
skeleton_handle_show_folders_cb (CajaFreedesktopFileManager1 *object,
                                 GDBusMethodInvocation *invocation,
                                 const gchar *const *uris,
                                 const gchar *startup_id,
                                 CajaFreedesktopDBus *fdb)
{
    CajaApplication *application;
    int i;

    application = CAJA_APPLICATION (fdb->application);

    for (i = 0; uris[i] != NULL; i++) {
        GFile *file;

        file = g_file_new_for_uri (uris[i]);

        caja_application_open_location (application, file, NULL, startup_id, 0);

        g_object_unref (file);
    }

    caja_freedesktop_file_manager1_complete_show_folders (object, invocation);
    return TRUE;
}

static gboolean
skeleton_handle_show_item_properties_cb (CajaFreedesktopFileManager1 *object,
                     GDBusMethodInvocation *invocation,
                     const gchar *const *uris,
                     const gchar *startup_id,
                     CajaFreedesktopDBus *fdb)
{
    CajaApplication *application;
    GList *files;
    int i;

    application = CAJA_APPLICATION (fdb->application);
    files = NULL;

    for (i = 0; uris[i] != NULL; i++) {
        files = g_list_prepend (files, caja_file_get_by_uri (uris[i]));
    }

    files = g_list_reverse (files);

    if (uris[0] != NULL) {
        GFile *file;
        CajaWindow *window;

        file = g_file_new_for_uri (uris[i]);
        window = caja_application_get_spatial_window (application,
                                                      NULL,
                                                      startup_id,
                                                      file,
                                                      gdk_screen_get_default (),
                                                      NULL);
        fm_properties_window_present (files, GTK_WIDGET (window));
        g_object_unref (file);
    }

    caja_file_list_free (files);

    caja_freedesktop_file_manager1_complete_show_item_properties (object, invocation);
    return TRUE;
}

static void
bus_acquired_cb (GDBusConnection *conn,
                 const gchar     *name,
                 gpointer         user_data)
{
    CajaFreedesktopDBus *fdb = user_data;

    caja_debug_log (FALSE, CAJA_DEBUG_LOG_DOMAIN_USER, "Bus acquired at %s", name);

    fdb->object_manager = g_dbus_object_manager_server_new (CAJA_FDO_DBUS_PATH);

    fdb->skeleton = caja_freedesktop_file_manager1_skeleton_new ();

    g_signal_connect (fdb->skeleton, "handle-show-items",
                      G_CALLBACK (skeleton_handle_show_items_cb), fdb);
    g_signal_connect (fdb->skeleton, "handle-show-folders",
                      G_CALLBACK (skeleton_handle_show_folders_cb), fdb);
    g_signal_connect (fdb->skeleton, "handle-show-item-properties",
                      G_CALLBACK (skeleton_handle_show_item_properties_cb), fdb);

    g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (fdb->skeleton), conn, CAJA_FDO_DBUS_PATH, NULL);

    g_dbus_object_manager_server_set_connection (fdb->object_manager, conn);
}

static void
name_acquired_cb (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
    caja_debug_log (FALSE, CAJA_DEBUG_LOG_DOMAIN_USER, "Acquired the name %s on the session message bus\n", name);
}

static void
name_lost_cb (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
    caja_debug_log (FALSE, CAJA_DEBUG_LOG_DOMAIN_USER, "Lost (or failed to acquire) the name %s on the session message bus\n", name);
}

static void
caja_freedesktop_dbus_dispose (GObject *object)
{
    CajaFreedesktopDBus *fdb = (CajaFreedesktopDBus *) object;

    if (fdb->owner_id != 0) {
        g_bus_unown_name (fdb->owner_id);
        fdb->owner_id = 0;
    }

    if (fdb->skeleton != NULL) {
        g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (fdb->skeleton));
        g_object_unref (fdb->skeleton);
        fdb->skeleton = NULL;
    }

    g_clear_object (&fdb->object_manager);

    G_OBJECT_CLASS (caja_freedesktop_dbus_parent_class)->dispose (object);
}

static void
caja_freedesktop_dbus_class_init (CajaFreedesktopDBusClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = caja_freedesktop_dbus_dispose;
}

static void
caja_freedesktop_dbus_init (CajaFreedesktopDBus *fdb)
{
    fdb->owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                    CAJA_FDO_DBUS_NAME,
                                    G_BUS_NAME_OWNER_FLAGS_NONE,
                                    bus_acquired_cb,
                                    name_acquired_cb,
                                    name_lost_cb,
                                    fdb,
                                    NULL);
}

/* Tries to own the org.freedesktop.FileManager1 service name */
CajaFreedesktopDBus *
caja_freedesktop_dbus_new (CajaApplication *application)
{
    CajaFreedesktopDBus *fdb = g_object_new (caja_freedesktop_dbus_get_type (), NULL);
    fdb->application = application;
    return fdb;
}
