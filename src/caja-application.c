/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */

/*
 *  Caja
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 2000, 2001 Eazel, Inc.
 *
 *  Caja is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Caja is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Authors: Elliot Lee <sopwith@redhat.com>,
 *           Darin Adler <darin@bentspoon.com>,
 *          Cosimo Cecchi <cosimoc@gnome.org>
 *
 */

#include <config.h>
#include "caja-application.h"

#include "file-manager/fm-desktop-icon-view.h"
#include "file-manager/fm-icon-view.h"
#include "file-manager/fm-list-view.h"
#include "file-manager/fm-tree-view.h"
#if ENABLE_EMPTY_VIEW
#include "file-manager/fm-empty-view.h"
#endif /* ENABLE_EMPTY_VIEW */
#include "caja-information-panel.h"
#include "caja-history-sidebar.h"
#include "caja-places-sidebar.h"
#if GTK_CHECK_VERSION (3, 0, 0)
#include "caja-self-check-functions.h"
#endif
#include "caja-notes-viewer.h"
#include "caja-emblem-sidebar.h"
#include "caja-image-properties-page.h"
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "caja-desktop-window.h"
#if !GTK_CHECK_VERSION (3, 0, 0)
#include "caja-main.h"
#endif
#include "caja-spatial-window.h"
#include "caja-navigation-window.h"
#include "caja-window-slot.h"
#include "caja-navigation-window-slot.h"
#include "caja-window-bookmarks.h"
#include "libcaja-private/caja-file-operations.h"
#include "caja-window-private.h"
#include "caja-window-manage-views.h"
#include "caja-freedesktop-dbus.h"
#include <libxml/xmlsave.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libcaja-private/caja-debug-log.h>
#include <libcaja-private/caja-file-utilities.h>
#include <libcaja-private/caja-global-preferences.h>
#if GTK_CHECK_VERSION (3, 0, 0)
#include <libcaja-private/caja-lib-self-check-functions.h>
#endif
#include <libcaja-private/caja-extensions.h>
#include <libcaja-private/caja-module.h>
#include <libcaja-private/caja-desktop-link-monitor.h>
#include <libcaja-private/caja-directory-private.h>
#include <libcaja-private/caja-signaller.h>
#include <libcaja-extension/caja-menu-provider.h>
#include <libcaja-private/caja-autorun.h>
#define MATE_DESKTOP_USE_UNSTABLE_API
#include <libmate-desktop/mate-bg.h>

#if GTK_CHECK_VERSION (3, 0, 0)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#else
enum {
	COMMAND_0, /* unused: 0 is an invalid command */

	COMMAND_START_DESKTOP,
	COMMAND_STOP_DESKTOP,
	COMMAND_OPEN_BROWSER,
};
#endif
/* Keep window from shrinking down ridiculously small; numbers are somewhat arbitrary */
#define APPLICATION_WINDOW_MIN_WIDTH	300
#define APPLICATION_WINDOW_MIN_HEIGHT	100

#define CAJA_ACCEL_MAP_SAVE_DELAY 30

/* Keeps track of all the desktop windows. */
static GList *caja_application_desktop_windows;

#if !GTK_CHECK_VERSION (3, 0, 0)
/* Keeps track of all the caja windows. */
static GList *caja_application_window_list;
#endif
/* Keeps track of all the object windows */
static GList *caja_application_spatial_window_list;

/* The saving of the accelerator map was requested  */
static gboolean save_of_accel_map_requested = FALSE;

/* File Manager DBus Interface */
static CajaFreedesktopDBus *fdb_manager = NULL;

static void     desktop_changed_callback          (gpointer                  user_data);
#if !GTK_CHECK_VERSION (3, 0, 0)
static void     desktop_location_changed_callback (gpointer                  user_data);
#endif
static void     mount_removed_callback            (GVolumeMonitor            *monitor,
        GMount                    *mount,
        CajaApplication       *application);
static void     mount_added_callback              (GVolumeMonitor            *monitor,
        GMount                    *mount,
        CajaApplication       *application);
static void     volume_added_callback              (GVolumeMonitor           *monitor,
        GVolume                  *volume,
        CajaApplication      *application);
static void     volume_removed_callback            (GVolumeMonitor           *monitor,
	    GVolume                  *volume,
	    CajaApplication      *application);
 static void     drive_connected_callback           (GVolumeMonitor           *monitor,
        GDrive                   *drive,
        CajaApplication      *application);
static void     drive_listen_for_eject_button      (GDrive *drive,
        CajaApplication *application);
#if GTK_CHECK_VERSION (3, 0, 0)
static void     caja_application_load_session    (CajaApplication *application);
static char *   caja_application_get_session_data (CajaApplication *self);
void caja_application_quit (CajaApplication *self);
#else
static void     caja_application_load_session     (CajaApplication *application);
static char *   caja_application_get_session_data (void);
#endif
#if GTK_CHECK_VERSION (3, 0, 0)
G_DEFINE_TYPE (CajaApplication, caja_application, GTK_TYPE_APPLICATION);
struct _CajaApplicationPriv {
	GVolumeMonitor *volume_monitor;
    gboolean no_desktop;
    gchar *geometry;
};

#else
G_DEFINE_TYPE (CajaApplication, caja_application, G_TYPE_OBJECT);

static gboolean
_unique_message_data_set_geometry_and_uris (UniqueMessageData  *message_data,
        const char *geometry,
        char **uris)
{
    GString *list;
    gint i;
    gchar *result;
    gsize length;

    list = g_string_new (NULL);
    if (geometry != NULL)
    {
        g_string_append (list, geometry);
    }
    g_string_append (list, "\r\n");

    for (i = 0; uris != NULL && uris[i]; i++)
    {
        g_string_append (list, uris[i]);
        g_string_append (list, "\r\n");
    }

    result = g_convert (list->str, list->len,
                        "ASCII", "UTF-8",
                        NULL, &length, NULL);
    g_string_free (list, TRUE);

    if (result)
    {
        unique_message_data_set (message_data, (guchar *) result, length);
        g_free (result);
        return TRUE;
    }

    return FALSE;
}

static gchar **
_unique_message_data_get_geometry_and_uris (UniqueMessageData *message_data,
        char **geometry)
{
    gchar **result = NULL;

    *geometry = NULL;

    gchar *text, *newline, *uris;
    text = unique_message_data_get_text (message_data);
    if (text)
    {
        newline = strchr (text, '\n');
        if (newline)
        {
            *geometry = g_strndup (text, newline-text);
            uris = newline+1;
        }
        else
        {
            uris = text;
        }

        result = g_uri_list_extract_uris (uris);
        g_free (text);
    }

    /* if the string is empty, make it NULL */
    if (*geometry && strlen (*geometry) == 0)
    {
        g_free (*geometry);
        *geometry = NULL;
    }

    return result;
}

GList *
caja_application_get_window_list (void)
{
    return caja_application_window_list;
}
#endif

GList *

caja_application_get_spatial_window_list (void)
{
    return caja_application_spatial_window_list;
}

#if !GTK_CHECK_VERSION (3, 0, 0)
unsigned int
caja_application_get_n_windows (void)
{
    return g_list_length (caja_application_window_list) +
           g_list_length (caja_application_desktop_windows);
}
#endif
static void
startup_volume_mount_cb (GObject *source_object,
                         GAsyncResult *res,
                         gpointer user_data)
{
    g_volume_mount_finish (G_VOLUME (source_object), res, NULL);
}

static void
automount_all_volumes (CajaApplication *application)
{
    GList *volumes, *l;
    GMount *mount;
    GVolume *volume;

    if (g_settings_get_boolean (caja_media_preferences, CAJA_PREFERENCES_MEDIA_AUTOMOUNT))
    {
        /* automount all mountable volumes at start-up */
#if GTK_CHECK_VERSION (3, 0, 0)
        volumes = g_volume_monitor_get_volumes (application->priv->volume_monitor);
#else
        volumes = g_volume_monitor_get_volumes (application->volume_monitor);
#endif
        for (l = volumes; l != NULL; l = l->next)
        {
            volume = l->data;

            if (!g_volume_should_automount (volume) ||
                    !g_volume_can_mount (volume))
            {
                continue;
            }

            mount = g_volume_get_mount (volume);
            if (mount != NULL)
            {
                g_object_unref (mount);
                continue;
            }

            /* pass NULL as GMountOperation to avoid user interaction */
            g_volume_mount (volume, 0, NULL, NULL, startup_volume_mount_cb, NULL);
        }
    	g_list_free_full (volumes, g_object_unref);
    }

}

static void
smclient_save_state_cb (EggSMClient   *client,
                        GKeyFile      *state_file,
                        CajaApplication *application)
{
    char *data;
#if GTK_CHECK_VERSION (3, 0, 0)
    data = caja_application_get_session_data (application);

    if (data != NULL)
#else
    data = caja_application_get_session_data ();

    if (data)
#endif  
    {
        g_key_file_set_string (state_file,
                               "Caja",
                               "documents",
                               data);
    }
    g_free (data);
}

static void
smclient_quit_cb (EggSMClient   *client,
                  CajaApplication *application)
{
#if GTK_CHECK_VERSION (3, 0, 0)
    caja_application_quit (application);
#else
    caja_main_event_loop_quit (TRUE);
#endif
}

#if GTK_CHECK_VERSION (3, 0, 0)

static void
caja_application_smclient_initialize (CajaApplication *self)
{
    egg_sm_client_set_mode (EGG_SM_CLIENT_MODE_NORMAL);

    g_signal_connect (self->smclient, "save_state",
                          G_CALLBACK (smclient_save_state_cb),
                          self);
    g_signal_connect (self->smclient, "quit",
              G_CALLBACK (smclient_quit_cb),
              self);

    /* TODO: Should connect to quit_requested and block logout on active transfer? */
}

void
caja_application_smclient_startup (CajaApplication *self)
{
    g_assert (self->smclient == NULL);

    egg_sm_client_set_mode (EGG_SM_CLIENT_MODE_DISABLED);
    self->smclient = egg_sm_client_get ();
}

static void
caja_empty_callback_to_ensure_read() {
/*do nothing, just exist to suppress runtime error*/
}

static void
open_window (CajaApplication *application,
         GFile *location, GdkScreen *screen, const char *geometry, gboolean browser_window)
{
    CajaApplication *self = CAJA_APPLICATION (application);
    CajaWindow *window;
    gchar *uri;

    uri = g_file_get_uri (location);
    g_debug ("Opening new window at uri %s", uri);

    /*monitor the preference to use browser or spatial windows */
    /*connect before trying to read or this preference won't be read by root or after change*/
     g_signal_connect_swapped(caja_preferences, "changed::"CAJA_PREFERENCES_ALWAYS_USE_BROWSER,
                      G_CALLBACK (caja_empty_callback_to_ensure_read),
                      self);

    if (browser_window ||g_settings_get_boolean (caja_preferences, CAJA_PREFERENCES_ALWAYS_USE_BROWSER)) {
        window = caja_application_create_navigation_window (application,
                 screen);
    }

    else {
        window = caja_application_get_spatial_window (application,
                 NULL,
                 NULL,
                 location,
                 screen,
                 NULL);
    }

    caja_window_go_to (window, location);

    if (geometry != NULL && !gtk_widget_get_visible (GTK_WIDGET (window))) {
        /* never maximize windows opened from shell if a
         * custom geometry has been requested.
         */
        gtk_window_unmaximize (GTK_WINDOW (window));
        eel_gtk_window_set_initial_geometry_from_string (GTK_WINDOW (window),
                                 geometry,
                                 APPLICATION_WINDOW_MIN_WIDTH,
                                 APPLICATION_WINDOW_MIN_HEIGHT,
                                 FALSE);
    }

    g_free (uri);
}


static void
open_windows (CajaApplication *application,
          GFile **files,
          GdkScreen *screen,
          const char *geometry,
          guint len,
          gboolean browser_window)
{
    guint i;

    if (files == NULL || files[0] == NULL) {
        /* Open a window pointing at the default location. */
        open_window (application, NULL, screen, geometry, browser_window );
    } else {
        /* Open windows at each requested location. */ 
        i = 0;
        while (i < len ){ 
              open_window (application, files[i], screen, geometry, browser_window);
               i++ ;
         }
    }
}

static void
caja_application_open (GApplication *app,
               GFile **files,
               gint n_files,
               const gchar *hint)
{
    CajaApplication *self = CAJA_APPLICATION (app);
    gboolean browser_window = FALSE;
    const gchar *geometry;
    const char splitter = '=';

    g_debug ("Open called on the GApplication instance; %d files", n_files);

    /*Check if local command line passed --browser or --geometry */
    if (strcmp(hint,"") != 0 ){
        if (g_str_match_string ("browser",
                    hint,
                    FALSE) == TRUE){
            browser_window = TRUE;
            geometry = strchr(hint, splitter);
        }
        else {
        geometry = hint;
        }
        /*Reset this or 3ed and later invocations will use same
	     *geometry even if the user has resized open window
         */
        self->priv->geometry = NULL;
    }

    open_windows (self, files,
              gdk_screen_get_default (),
              geometry,
              n_files,
              browser_window);
}

void
caja_application_open_location (CajaApplication *application,
                                GFile *location,
                                GFile *selection,
                                const char *startup_id)
{
    CajaWindow *window;
    GList *sel_list = NULL;

    window = caja_application_create_navigation_window (application, gdk_screen_get_default ());

    if (selection != NULL) {
        sel_list = g_list_prepend (NULL, g_object_ref (selection));
    }

    caja_window_slot_open_location_full (caja_window_get_active_slot (window), location,
                                         0, CAJA_WINDOW_OPEN_FLAG_NEW_WINDOW, sel_list, NULL, NULL);

    if (sel_list != NULL) {
        caja_file_list_free (sel_list);
    }
}

void
caja_application_quit (CajaApplication *self)
{
    GApplication *app = G_APPLICATION (self);
    GList *windows;

    windows = gtk_application_get_windows (GTK_APPLICATION (app));
    g_list_foreach (windows, (GFunc) gtk_widget_destroy, NULL);
     /* we have been asked to force quit */
    g_application_quit (G_APPLICATION (self));
}

static void
caja_application_init (CajaApplication *application)
{
    GSimpleAction *action;
    application->priv =
        G_TYPE_INSTANCE_GET_PRIVATE (application, CAJA_TYPE_APPLICATION,
                         CajaApplicationPriv);
    action = g_simple_action_new ("quit", NULL);

    g_action_map_add_action (G_ACTION_MAP (application), G_ACTION (action));

	g_signal_connect_swapped (action, "activate",
				  G_CALLBACK (caja_application_quit), application);

	g_object_unref (action);
}

#else
static void
caja_application_init (CajaApplication *application)
{
    application->unique_app = unique_app_new_with_commands ("org.mate.Caja", NULL,
                              "start_desktop", COMMAND_START_DESKTOP,
                              "stop_desktop", COMMAND_STOP_DESKTOP,
                              "open_browser", COMMAND_OPEN_BROWSER,
                              NULL); 
    application->smclient = egg_sm_client_get ();
    g_signal_connect (application->smclient, "save_state",
                      G_CALLBACK (smclient_save_state_cb),
                      application);
    g_signal_connect (application->smclient, "quit",
                      G_CALLBACK (smclient_quit_cb),
                      application);
    /* TODO: Should connect to quit_requested and block logout on active transfer? */

    /* register views */
    fm_icon_view_register ();
    fm_desktop_icon_view_register ();
    fm_list_view_register ();
    fm_compact_view_register ();
#if ENABLE_EMPTY_VIEW
    fm_empty_view_register ();
#endif /* ENABLE_EMPTY_VIEW */

    /* register sidebars */
    caja_places_sidebar_register ();
    caja_information_panel_register ();
    fm_tree_view_register ();
    caja_history_sidebar_register ();
    caja_notes_viewer_register (); /* also property page */
    caja_emblem_sidebar_register ();

    /* register property pages */
    caja_image_properties_page_register ();

    /* initialize search path for custom icons */
    gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
                                       CAJA_DATADIR G_DIR_SEPARATOR_S "icons");
}

CajaApplication *
caja_application_new (void)
{
    return g_object_new (CAJA_TYPE_APPLICATION, NULL);
}
#endif

static void
caja_application_finalize (GObject *object)
{
    CajaApplication *application;

    application = CAJA_APPLICATION (object);

    caja_bookmarks_exiting ();
#if GTK_CHECK_VERSION (3, 0, 0)
   if (application->volume_monitor)
    {
        g_object_unref (application->priv->volume_monitor);
        application->priv->volume_monitor = NULL;
    }

    g_free (application->priv->geometry);
#else
    if (application->volume_monitor)
    {
        g_object_unref (application->volume_monitor);
        application->volume_monitor = NULL;
    }

    g_object_unref (application->unique_app);
#endif
	if (application->ss_watch_id > 0)
	{
		g_bus_unwatch_name (application->ss_watch_id);
	}
	
	if (application->volume_queue != NULL)
	{
		g_list_free_full (application->volume_queue, g_object_unref);
		application->volume_queue = NULL;
	}

 	if (application->automount_idle_id != 0)
    {
        g_source_remove (application->automount_idle_id);
        application->automount_idle_id = 0;
    }

    if (fdb_manager != NULL)
    {
        g_object_unref (fdb_manager);
        fdb_manager = NULL;
    }

    if (application->ss_proxy != NULL)
    {
		g_object_unref (application->ss_proxy);
		application->ss_proxy = NULL;
	}

    G_OBJECT_CLASS (caja_application_parent_class)->finalize (object);
}

static gboolean
check_required_directories (CajaApplication *application)
{
    char *user_directory;
    char *desktop_directory;
    GSList *directories;
    gboolean ret;

    g_assert (CAJA_IS_APPLICATION (application));

    ret = TRUE;

    user_directory = caja_get_user_directory ();
    desktop_directory = caja_get_desktop_directory ();

    directories = NULL;

    if (!g_file_test (user_directory, G_FILE_TEST_IS_DIR))
    {
        directories = g_slist_prepend (directories, user_directory);
    }

    if (!g_file_test (desktop_directory, G_FILE_TEST_IS_DIR))
    {
        directories = g_slist_prepend (directories, desktop_directory);
    }

    if (directories != NULL)
    {
        int failed_count;
        GString *directories_as_string;
        GSList *l;
        char *error_string;
        const char *detail_string;
        GtkDialog *dialog;

        ret = FALSE;

        failed_count = g_slist_length (directories);

        directories_as_string = g_string_new ((const char *)directories->data);
        for (l = directories->next; l != NULL; l = l->next)
        {
            g_string_append_printf (directories_as_string, ", %s", (const char *)l->data);
        }

        if (failed_count == 1)
        {
            error_string = g_strdup_printf (_("Caja could not create the required folder \"%s\"."),
                                            directories_as_string->str);
            detail_string = _("Before running Caja, please create the following folder, or "
                              "set permissions such that Caja can create it.");
        }
        else
        {
            error_string = g_strdup_printf (_("Caja could not create the following required folders: "
                                              "%s."), directories_as_string->str);
            detail_string = _("Before running Caja, please create these folders, or "
                              "set permissions such that Caja can create them.");
        }

        dialog = eel_show_error_dialog (error_string, detail_string, NULL);
        /* We need the main event loop so the user has a chance to see the dialog. */
#if GTK_CHECK_VERSION (3, 0, 0)
        gtk_application_add_window (GTK_APPLICATION (application),
                                    GTK_WINDOW (dialog));
#else
        caja_main_event_loop_register (GTK_OBJECT (dialog));
#endif

        g_string_free (directories_as_string, TRUE);
        g_free (error_string);
    }

    g_slist_free (directories);
    g_free (user_directory);
    g_free (desktop_directory);

    return ret;
}

static void
menu_provider_items_updated_handler (CajaMenuProvider *provider, GtkWidget* parent_window, gpointer data)
{

    g_signal_emit_by_name (caja_signaller_get_current (),
                           "popup_menu_changed");
}

static void
menu_provider_init_callback (void)
{
    GList *providers;
    GList *l;

    providers = caja_extensions_get_for_type (CAJA_TYPE_MENU_PROVIDER);

    for (l = providers; l != NULL; l = l->next)
    {
        CajaMenuProvider *provider = CAJA_MENU_PROVIDER (l->data);

        g_signal_connect_after (G_OBJECT (provider), "items_updated",
                                (GCallback)menu_provider_items_updated_handler,
                                NULL);
    }

    caja_module_extension_list_free (providers);
}

static gboolean
automount_all_volumes_idle_cb (gpointer data)
{
    CajaApplication *application = CAJA_APPLICATION (data);

    automount_all_volumes (application);

    application->automount_idle_id = 0;
    return FALSE;
}

static void
mark_desktop_files_trusted (void)
{
    char *do_once_file;
    GFile *f, *c;
    GFileEnumerator *e;
    GFileInfo *info;
    const char *name;
    int fd;

    do_once_file = g_build_filename (g_get_user_data_dir (),
                                     ".converted-launchers", NULL);

    if (g_file_test (do_once_file, G_FILE_TEST_EXISTS))
    {
        goto out;
    }

    f = caja_get_desktop_location ();
    e = g_file_enumerate_children (f,
                                   G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                   G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                   G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE
                                   ,
                                   G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                   NULL, NULL);
    if (e == NULL)
    {
        goto out2;
    }

    while ((info = g_file_enumerator_next_file (e, NULL, NULL)) != NULL)
    {
        name = g_file_info_get_name (info);

        if (g_str_has_suffix (name, ".desktop") &&
                !g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE))
        {
            c = g_file_get_child (f, name);
            caja_file_mark_desktop_file_trusted (c,
                                                 NULL, FALSE,
                                                 NULL, NULL);
            g_object_unref (c);
        }
        g_object_unref (info);
    }

    g_object_unref (e);
out2:
    fd = g_creat (do_once_file, 0666);
    close (fd);

    g_object_unref (f);
out:
    g_free (do_once_file);
}

static void
check_volume_queue (CajaApplication *application)
{
        GList *l, *next;
        GVolume *volume;

        l = application->volume_queue;

        if (application->screensaver_active)
        {
                return;
        }

        while (l != NULL) {
		volume = l->data;
		next = l->next;

		caja_file_operations_mount_volume (NULL, volume, TRUE);
		application->volume_queue =
			g_list_remove (application->volume_queue, volume);

		g_object_unref (volume);
		l = next;
        }

        application->volume_queue = NULL;
}

#define SCREENSAVER_NAME "org.mate.ScreenSaver"
#define SCREENSAVER_PATH "/org/mate/ScreenSaver"
#define SCREENSAVER_INTERFACE "org.mate.ScreenSaver"

static void
screensaver_signal_callback (GDBusProxy *proxy,
                             const gchar *sender_name,
                             const gchar *signal_name,
                             GVariant *parameters,
                             gpointer user_data)
{
	CajaApplication *application = user_data;

	if (g_strcmp0 (signal_name, "ActiveChanged") == 0)
	{
		g_variant_get (parameters, "(b)", &application->screensaver_active);
		g_debug ("Screensaver active changed to %d", application->screensaver_active);

		check_volume_queue (application);
	}
}

static void
screensaver_get_active_ready_cb (GObject *source,
				 GAsyncResult *res,
				 gpointer user_data)
{
	CajaApplication *application = user_data;
	GDBusProxy *proxy = application->ss_proxy;
	GVariant *result;
	GError *error = NULL;

	result = g_dbus_proxy_call_finish (proxy,
					   res,
					   &error);

	if (error != NULL) {
		g_warning ("Can't call GetActive() on the ScreenSaver object: %s",
			   error->message);
		g_error_free (error);

		return;
	}

	g_variant_get (result, "(b)", &application->screensaver_active);
	g_variant_unref (result);

	g_debug ("Screensaver GetActive() returned %d", application->screensaver_active);
}

static void
screensaver_proxy_ready_cb (GObject *source,
			    GAsyncResult *res,
			    gpointer user_data)
{
	CajaApplication *application = user_data;
	GError *error = NULL;
	GDBusProxy *ss_proxy;
	
	ss_proxy = g_dbus_proxy_new_finish (res, &error);

	if (error != NULL)
	{
		g_warning ("Can't get proxy for the ScreenSaver object: %s",
			   error->message);
		g_error_free (error);

		return;
	}

	g_debug ("ScreenSaver proxy ready");

	application->ss_proxy = ss_proxy;

	g_signal_connect (ss_proxy, "g-signal",
			  G_CALLBACK (screensaver_signal_callback), application);

	g_dbus_proxy_call (ss_proxy,
			   "GetActive",
			   NULL,
			   G_DBUS_CALL_FLAGS_NO_AUTO_START,
			   -1,
			   NULL,
			   screensaver_get_active_ready_cb,
			   application);
}

static void
screensaver_appeared_callback (GDBusConnection *connection,
			       const gchar *name,
			       const gchar *name_owner,
			       gpointer user_data)
{
	CajaApplication *application = user_data;

	g_debug ("ScreenSaver name appeared");

	application->screensaver_active = FALSE;

	g_dbus_proxy_new (connection,
			  G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
			  NULL,
			  name,
			  SCREENSAVER_PATH,
			  SCREENSAVER_INTERFACE,
			  NULL,
			  screensaver_proxy_ready_cb,
			  application);
}

static void
screensaver_vanished_callback (GDBusConnection *connection,
			       const gchar *name,
			       gpointer user_data)
{
	CajaApplication *application = user_data;

	g_debug ("ScreenSaver name vanished");

	application->screensaver_active = FALSE;
	if (application->ss_proxy != NULL)
	{
		g_object_unref (application->ss_proxy);
		application->ss_proxy = NULL;
	}

	/* in this case force a clear of the volume queue, without
	 * mounting them.
	 */
	if (application->volume_queue != NULL)
	{
		g_list_free_full (application->volume_queue, g_object_unref);
		application->volume_queue = NULL;
	}
}

static void
do_initialize_screensaver (CajaApplication *application)
{
	application->ss_watch_id =
		g_bus_watch_name (G_BUS_TYPE_SESSION,
				  SCREENSAVER_NAME,
				  G_BUS_NAME_WATCHER_FLAGS_NONE,
				  screensaver_appeared_callback,
				  screensaver_vanished_callback,
				  application,
				  NULL);
}

#if GTK_CHECK_VERSION (3, 0, 0)

static void
do_upgrades_once (CajaApplication *self)
{
    char *metafile_dir, *updated, *caja_dir, *xdg_dir;
    const gchar *message;
    int fd, res;

    if (!self->priv->no_desktop) {
        mark_desktop_files_trusted ();
    }

    metafile_dir = g_build_filename (g_get_home_dir (),
                     ".caja/metafiles", NULL);
    if (g_file_test (metafile_dir, G_FILE_TEST_IS_DIR)) {
        updated = g_build_filename (metafile_dir, "migrated-to-gvfs", NULL);
        if (!g_file_test (updated, G_FILE_TEST_EXISTS)) {
            g_spawn_command_line_async (LIBEXECDIR"/caja-convert-metadata --quiet", NULL);
            fd = g_creat (updated, 0600);
            if (fd != -1) {
                close (fd);
            }
        }
        g_free (updated);
    }
    g_free (metafile_dir);

    caja_dir = g_build_filename (g_get_home_dir (),
                     ".caja", NULL);
    xdg_dir = caja_get_user_directory ();
    if (g_file_test (caja_dir, G_FILE_TEST_IS_DIR)) {
        /* test if we already attempted to migrate first */
        updated = g_build_filename (caja_dir, "DEPRECATED-DIRECTORY", NULL);
        message = _("Caja 3.0 deprecated this directory and tried migrating "
                "this configuration to ~/.config/caja");
        if (!g_file_test (updated, G_FILE_TEST_EXISTS)) {
            /* rename() works fine if the destination directory is
             * empty.
             */
            res = g_rename (caja_dir, xdg_dir);

            if (res == -1) {
                fd = g_creat (updated, 0600);
                if (fd != -1) {
                    res = write (fd, message, strlen (message));
                    close (fd);
                }
            }
        }

        g_free (updated);
    }

    g_free (caja_dir);
    g_free (xdg_dir);
}

#else
static void
do_upgrades_once (CajaApplication *application,
                  gboolean no_desktop)
{
    char *metafile_dir, *updated;
    int fd;

    if (!no_desktop)
    {
        mark_desktop_files_trusted ();
    }

    metafile_dir = g_build_filename(g_get_user_config_dir(), "caja", "metafiles", NULL);

    if (g_file_test (metafile_dir, G_FILE_TEST_IS_DIR))
    {
        updated = g_build_filename (metafile_dir, "migrated-to-gvfs", NULL);
        if (!g_file_test (updated, G_FILE_TEST_EXISTS))
        {
            g_spawn_command_line_async (LIBEXECDIR "/caja-convert-metadata --quiet", NULL);
            fd = g_creat (updated, 0600);
            if (fd != -1)
            {
                close (fd);
            }
        }
        g_free (updated);
    }
    g_free (metafile_dir);
}

static void
finish_startup (CajaApplication *application,
                gboolean no_desktop)
{
    GList *drives;

    do_upgrades_once (application, no_desktop);

    /* initialize caja modules */
    caja_module_setup ();

    /* attach menu-provider module callback */
    menu_provider_init_callback ();

    /* Initialize the desktop link monitor singleton */
    caja_desktop_link_monitor_get ();

    /* Initialize MATE screen saver listener to control automount
	 * permission */
	do_initialize_screensaver (application);

 	/* Watch for mounts so we can restore open windows This used
     * to be for showing new window on mount, but is not used
     * anymore */

    /* Watch for unmounts so we can close open windows */
    /* TODO-gio: This should be using the UNMOUNTED feature of GFileMonitor instead */

    application->volume_monitor = g_volume_monitor_get ();
    g_signal_connect_object (application->volume_monitor, "mount_removed",
                             G_CALLBACK (mount_removed_callback), application, 0);
    g_signal_connect_object (application->volume_monitor, "mount_pre_unmount",
                             G_CALLBACK (mount_removed_callback), application, 0);
    g_signal_connect_object (application->volume_monitor, "mount_added",
                             G_CALLBACK (mount_added_callback), application, 0);
    g_signal_connect_object (application->volume_monitor, "volume_added",
                             G_CALLBACK (volume_added_callback), application, 0);
    g_signal_connect_object (application->volume_monitor, "volume_removed",
                             G_CALLBACK (volume_removed_callback), application, 0);
    g_signal_connect_object (application->volume_monitor, "drive_connected",
                             G_CALLBACK (drive_connected_callback), application, 0);

    /* listen for eject button presses */
    drives = g_volume_monitor_get_connected_drives (application->volume_monitor);
    g_list_foreach (drives, (GFunc) drive_listen_for_eject_button, application);
    g_list_free_full (drives, g_object_unref);

    application->automount_idle_id =
        g_idle_add_full (G_PRIORITY_LOW,
                         automount_all_volumes_idle_cb,
                         application, NULL);
}

static void
open_window (CajaApplication *application,
             const char *startup_id,
             const char *uri, GdkScreen *screen, const char *geometry,
             gboolean browser_window)
{
    GFile *location;
    CajaWindow *window;

    if (uri == NULL) {
    	location = g_file_new_for_path (g_get_home_dir ());
    } else {
    	location = g_file_new_for_uri (uri);
    }

    if (browser_window ||
            g_settings_get_boolean (caja_preferences, CAJA_PREFERENCES_ALWAYS_USE_BROWSER)) {
        window = caja_application_create_navigation_window (application,
                 startup_id,
                 screen);
    } else {
        window = caja_application_get_spatial_window (application,
                 NULL,
                 startup_id,
                 location,
                 screen,
                 NULL);
    }

    caja_window_go_to (window, location);

    g_object_unref (location);

    if (geometry != NULL && !gtk_widget_get_visible (GTK_WIDGET (window)))
    {
        /* never maximize windows opened from shell if a
         * custom geometry has been requested.
         */
        gtk_window_unmaximize (GTK_WINDOW (window));
        eel_gtk_window_set_initial_geometry_from_string (GTK_WINDOW (window),
                geometry,
                APPLICATION_WINDOW_MIN_WIDTH,
                APPLICATION_WINDOW_MIN_HEIGHT,
                FALSE);
    }
}

static void
open_windows (CajaApplication *application,
              const char *startup_id,
              char **uris,
              GdkScreen *screen,
              const char *geometry,
              gboolean browser_window)
{
    guint i;

    if (uris == NULL || uris[0] == NULL)
    {
        /* Open a window pointing at the default location. */
        open_window (application, startup_id, NULL, screen, geometry, browser_window);
    }
    else
    {
        /* Open windows at each requested location. */
        for (i = 0; uris[i] != NULL; i++)
        {
            open_window (application, startup_id, uris[i], screen, geometry, browser_window);
        }
    }
}

void
caja_application_open_location (CajaApplication *application,
                                GFile *location,
                                GFile *selection,
                                const char *startup_id)
{
    CajaWindow *window;
    GList *sel_list = NULL;

    window = caja_application_create_navigation_window (application, startup_id, gdk_screen_get_default ());

    if (selection != NULL) {
        sel_list = g_list_prepend (NULL, g_object_ref (selection));
    }

    caja_window_slot_open_location_full (caja_window_get_active_slot (window), location,
                                         0, CAJA_WINDOW_OPEN_FLAG_NEW_WINDOW, sel_list, NULL, NULL);

    if (sel_list != NULL) {
        caja_file_list_free (sel_list);
    }
}

static UniqueResponse
message_received_cb (UniqueApp         *unique_app,
                     gint               command,
                     UniqueMessageData *message,
                     guint              time_,
                     gpointer           user_data)
{
    CajaApplication *application;
    UniqueResponse res;
    char **uris;
    char *geometry;
    GdkScreen *screen;

    application =  user_data;
    res = UNIQUE_RESPONSE_OK;

    switch (command)
    {
    case UNIQUE_CLOSE:
        res = UNIQUE_RESPONSE_OK;
        caja_main_event_loop_quit (TRUE);

        break;
    case UNIQUE_OPEN:
    case COMMAND_OPEN_BROWSER:
        uris = _unique_message_data_get_geometry_and_uris (message, &geometry);
        screen = unique_message_data_get_screen (message);
        open_windows (application,
                      unique_message_data_get_startup_id (message),
                      uris,
                      screen,
                      geometry,
                      command == COMMAND_OPEN_BROWSER);
        g_strfreev (uris);
        g_free (geometry);
        break;
    case COMMAND_START_DESKTOP:
        caja_application_open_desktop (application);
        break;
    case COMMAND_STOP_DESKTOP:
        caja_application_close_desktop ();
        break;
    default:
        res = UNIQUE_RESPONSE_PASSTHROUGH;
        break;
    }

    return res;
}

gboolean
caja_application_save_accel_map (gpointer data)
{
    if (save_of_accel_map_requested)
    {
        char *accel_map_filename;
        accel_map_filename = caja_get_accel_map_file ();
        if (accel_map_filename)
        {
            gtk_accel_map_save (accel_map_filename);
            g_free (accel_map_filename);
        }
        save_of_accel_map_requested = FALSE;
    }

    return FALSE;
}


static void
queue_accel_map_save_callback (GtkAccelMap *object, gchar *accel_path,
                               guint accel_key, GdkModifierType accel_mods,
                               gpointer user_data)
{
    if (!save_of_accel_map_requested)
    {
        save_of_accel_map_requested = TRUE;
        g_timeout_add_seconds (CAJA_ACCEL_MAP_SAVE_DELAY,
                               caja_application_save_accel_map, NULL);
    }
}

static gboolean
desktop_changed_callback_connect (CajaApplication *application)
{
    g_signal_connect_swapped (caja_preferences, "changed::" CAJA_PREFERENCES_DESKTOP_IS_HOME_DIR,
                              G_CALLBACK(desktop_location_changed_callback),
                              G_OBJECT (application));
    return FALSE;
}

void
caja_application_startup (CajaApplication *application,
                          gboolean kill_shell,
                          gboolean no_default_window,
                          gboolean no_desktop,
                          gboolean browser_window,
                          const char *geometry,
                          char **urls)
{
    UniqueMessageData *message;

    /* Check the user's ~/.config/caja directories and post warnings
     * if there are problems.
     */
    if (!kill_shell && !check_required_directories (application))
    {
        return;
    }

    if (kill_shell)
    {
        if (unique_app_is_running (application->unique_app))
        {
            unique_app_send_message (application->unique_app,
                                     UNIQUE_CLOSE, NULL);

        }
    }
    else
    {
        char *accel_map_filename;

        if (!no_desktop &&
            !g_settings_get_boolean (mate_background_preferences, MATE_BG_KEY_SHOW_DESKTOP))
        {
            no_desktop = TRUE;
        }

        if (!no_desktop)
        {
            if (unique_app_is_running (application->unique_app))
            {
                unique_app_send_message (application->unique_app,
                                         COMMAND_START_DESKTOP, NULL);
            }
            else
            {
                caja_application_open_desktop (application);
            }
        }

        if (!unique_app_is_running (application->unique_app))
        {
            finish_startup (application, no_desktop);
            g_signal_connect (application->unique_app, "message-received", G_CALLBACK (message_received_cb), application);
        }

        /* Start the File Manager DBus Interface */
        fdb_manager = caja_freedesktop_dbus_new (application);

        /* Monitor the preference to show or hide the desktop */
        g_signal_connect_swapped (mate_background_preferences,
                                  "changed::" MATE_BG_KEY_SHOW_DESKTOP,
                                  G_CALLBACK(desktop_changed_callback),
                                  G_OBJECT (application));

        /* Monitor the preference to have the desktop */
        /* point to the Unix home folder */
        g_timeout_add_seconds (30, (GSourceFunc) desktop_changed_callback_connect, application);

        /* Create the other windows. */
        if (urls != NULL || !no_default_window)
        {
            if (unique_app_is_running (application->unique_app))
            {
                message = unique_message_data_new ();
                _unique_message_data_set_geometry_and_uris (message, geometry, urls);
                if (browser_window)
                {
                    unique_app_send_message (application->unique_app,
                                             COMMAND_OPEN_BROWSER, message);
                }
                else
                {
                    unique_app_send_message (application->unique_app,
                                             UNIQUE_OPEN, message);
                }
                unique_message_data_free (message);
            }
            else
            {
                open_windows (application, NULL,
                              urls,
                              gdk_display_get_default_screen (gdk_display_get_default()),
                              // gdk_screen_get_default (),
                              geometry,
                              browser_window);
            }
        }

        /* Load session info if availible */
        caja_application_load_session (application);

        /* load accelerator map, and register save callback */
        accel_map_filename = caja_get_accel_map_file ();
        if (accel_map_filename)
        {
            gtk_accel_map_load (accel_map_filename);
            g_free (accel_map_filename);
        }
        g_signal_connect (gtk_accel_map_get (), "changed", G_CALLBACK (queue_accel_map_save_callback), NULL);
    }
}
#endif

static void
selection_get_cb (GtkWidget          *widget,
                  GtkSelectionData   *selection_data,
                  guint               info,
                  guint               time)
{
    /* No extra targets atm */
}

static GtkWidget *
get_desktop_manager_selection (GdkDisplay *display)
{
    char selection_name[32];
    GdkAtom selection_atom;
    Window selection_owner;
    GtkWidget *selection_widget;

    g_snprintf (selection_name, sizeof (selection_name), "_NET_DESKTOP_MANAGER_S0");
    selection_atom = gdk_atom_intern (selection_name, FALSE);

    selection_owner = XGetSelectionOwner (GDK_DISPLAY_XDISPLAY (display),
                                          gdk_x11_atom_to_xatom_for_display (display,
                                                  selection_atom));
    if (selection_owner != None)
    {
        return NULL;
    }

    selection_widget = gtk_invisible_new_for_screen (gdk_display_get_default_screen (display));
    /* We need this for gdk_x11_get_server_time() */
    gtk_widget_add_events (selection_widget, GDK_PROPERTY_CHANGE_MASK);

    if (gtk_selection_owner_set_for_display (display,
            selection_widget,
            selection_atom,
            gdk_x11_get_server_time (gtk_widget_get_window (selection_widget))))
    {

        g_signal_connect (selection_widget, "selection_get",
                          G_CALLBACK (selection_get_cb), NULL);
        return selection_widget;
    }

    gtk_widget_destroy (selection_widget);

    return NULL;
}

static void
desktop_unrealize_cb (GtkWidget        *widget,
                      GtkWidget        *selection_widget)
{
    gtk_widget_destroy (selection_widget);
}

static gboolean
selection_clear_event_cb (GtkWidget	        *widget,
                          GdkEventSelection     *event,
                          CajaDesktopWindow *window)
{
    gtk_widget_destroy (GTK_WIDGET (window));

    caja_application_desktop_windows =
        g_list_remove (caja_application_desktop_windows, window);

    return TRUE;
}

static void
caja_application_create_desktop_windows (CajaApplication *application)
{
#if !GTK_CHECK_VERSION (3, 0, 0)
    static gboolean create_in_progress = FALSE;
#endif
    GdkDisplay *display;
    CajaDesktopWindow *window;
    GtkWidget *selection_widget;

    g_return_if_fail (caja_application_desktop_windows == NULL);
    g_return_if_fail (CAJA_IS_APPLICATION (application));
#if !GTK_CHECK_VERSION (3, 0, 0)
    if (create_in_progress)
    {
        return;
    }
    create_in_progress = TRUE;
#endif
    display = gdk_display_get_default ();

    selection_widget = get_desktop_manager_selection (display);
    if (selection_widget != NULL)
    {
        window = caja_desktop_window_new (application, gdk_display_get_default_screen (display));

        g_signal_connect (selection_widget, "selection_clear_event",
                          G_CALLBACK (selection_clear_event_cb), window);

        g_signal_connect (window, "unrealize",
                          G_CALLBACK (desktop_unrealize_cb), selection_widget);

        /* We realize it immediately so that the CAJA_DESKTOP_WINDOW_ID
           property is set so mate-settings-daemon doesn't try to set the
           background. And we do a gdk_flush() to be sure X gets it. */
        gtk_widget_realize (GTK_WIDGET (window));
        gdk_flush ();

        caja_application_desktop_windows =
            g_list_prepend (caja_application_desktop_windows, window);
#if GTK_CHECK_VERSION (3, 0, 0)
        /* Hold Caja open if the desktop is showing as autostart mode  
         * fails to read from here and exiting will cause an exit/restart cycle
         */
            gtk_application_add_window (GTK_APPLICATION (application),
							    GTK_WINDOW (window));
    }
#else
    }
    create_in_progress = FALSE;
#endif
}

void
caja_application_open_desktop (CajaApplication *application)
{
    if (caja_application_desktop_windows == NULL)
    {
        caja_application_create_desktop_windows (application);
    }
}
#if GTK_CHECK_VERSION (3, 0, 0)
static void
#else
void
#endif
caja_application_close_desktop (void)
{
    if (caja_application_desktop_windows != NULL)
    {
        g_list_free_full (caja_application_desktop_windows, (GDestroyNotify) gtk_widget_destroy);
        caja_application_desktop_windows = NULL;
    }
}

void
#if GTK_CHECK_VERSION (3, 0, 0)
caja_application_close_all_navigation_windows (CajaApplication *self)
{
    GList *list_copy;
    GList *l;
    list_copy = g_list_copy (gtk_application_get_windows (GTK_APPLICATION (self)));
#else
caja_application_close_all_navigation_windows (void)
{
    GList *list_copy;
    GList *l;
    list_copy = g_list_copy (caja_application_window_list);
#endif
    /* First hide all window to get the feeling of quick response */
    for (l = list_copy; l != NULL; l = l->next)
    {
        CajaWindow *window;

        window = CAJA_WINDOW (l->data);

        if (CAJA_IS_NAVIGATION_WINDOW (window))
        {
            gtk_widget_hide (GTK_WIDGET (window));
        }
    }

    for (l = list_copy; l != NULL; l = l->next)
    {
        CajaWindow *window;

        window = CAJA_WINDOW (l->data);

        if (CAJA_IS_NAVIGATION_WINDOW (window))
        {
            caja_window_close (window);
        }
    }
    g_list_free (list_copy);
}

static CajaSpatialWindow *
caja_application_get_existing_spatial_window (GFile *location)
{
    GList *l;
    CajaWindowSlot *slot;
    GFile *window_location;

    for (l = caja_application_get_spatial_window_list ();
            l != NULL; l = l->next) {
        slot = CAJA_WINDOW (l->data)->details->active_pane->active_slot;

        window_location = slot->pending_location;

        if (window_location == NULL) {
        	window_location = slot->location;
        }

        if (window_location != NULL) {
        	if (g_file_equal (location, window_location)) {
            	return CAJA_SPATIAL_WINDOW (l->data);
            }
        }
    }

    return NULL;
}

static CajaSpatialWindow *
find_parent_spatial_window (CajaSpatialWindow *window)
{
    CajaFile *file;
    CajaFile *parent_file;
    CajaWindowSlot *slot;
    GFile *location;

    slot = CAJA_WINDOW (window)->details->active_pane->active_slot;

    location = slot->location;
    if (location == NULL)
    {
        return NULL;
    }
    file = caja_file_get (location);

    if (!file)
    {
        return NULL;
    }

    parent_file = caja_file_get_parent (file);
    caja_file_unref (file);
    while (parent_file)
    {
        CajaSpatialWindow *parent_window;

        location = caja_file_get_location (parent_file);
        parent_window = caja_application_get_existing_spatial_window (location);
        g_object_unref (location);

        /* Stop at the desktop directory if it's not explicitely opened
         * in a spatial window of its own.
         */
        if (caja_file_is_desktop_directory (parent_file) && !parent_window)
        {
            caja_file_unref (parent_file);
            return NULL;
        }

        if (parent_window)
        {
            caja_file_unref (parent_file);
            return parent_window;
        }
        file = parent_file;
        parent_file = caja_file_get_parent (file);
        caja_file_unref (file);
    }

    return NULL;
}

void
caja_application_close_parent_windows (CajaSpatialWindow *window)
{
    CajaSpatialWindow *parent_window;
    CajaSpatialWindow *new_parent_window;

    g_return_if_fail (CAJA_IS_SPATIAL_WINDOW (window));

    parent_window = find_parent_spatial_window (window);

    while (parent_window)
    {

        new_parent_window = find_parent_spatial_window (parent_window);
        caja_window_close (CAJA_WINDOW (parent_window));
        parent_window = new_parent_window;
    }
}

void
caja_application_close_all_spatial_windows (void)
{
    GList *list_copy;
    GList *l;

    list_copy = g_list_copy (caja_application_spatial_window_list);
    /* First hide all window to get the feeling of quick response */
    for (l = list_copy; l != NULL; l = l->next)
    {
        CajaWindow *window;

        window = CAJA_WINDOW (l->data);

        if (CAJA_IS_SPATIAL_WINDOW (window))
        {
            gtk_widget_hide (GTK_WIDGET (window));
        }
    }

    for (l = list_copy; l != NULL; l = l->next)
    {
        CajaWindow *window;

        window = CAJA_WINDOW (l->data);

        if (CAJA_IS_SPATIAL_WINDOW (window))
        {
            caja_window_close (window);
        }
    }
    g_list_free (list_copy);
}

#if !GTK_CHECK_VERSION (3, 0, 0)
static void
caja_application_destroyed_window (GtkObject *object, CajaApplication *application)
{
    caja_application_window_list = g_list_remove (caja_application_window_list, object);
}
#endif
static gboolean
caja_window_delete_event_callback (GtkWidget *widget,
                                   GdkEvent *event,
                                   gpointer user_data)
{
    CajaWindow *window;

    window = CAJA_WINDOW (widget);
    caja_window_close (window);

    return TRUE;
}


static CajaWindow *
create_window (CajaApplication *application,
               GType window_type,
#if !GTK_CHECK_VERSION (3, 0, 0)
               const char *startup_id,
#endif
               GdkScreen *screen)
{
    CajaWindow *window;

    g_return_val_if_fail (CAJA_IS_APPLICATION (application), NULL);

    window = CAJA_WINDOW (gtk_widget_new (window_type,
                                          "app", application,
                                          "screen", screen,
                                          NULL));
#if !GTK_CHECK_VERSION (3, 0, 0)
    if (startup_id)
    {
        gtk_window_set_startup_id (GTK_WINDOW (window), startup_id);
    }
#endif
    g_signal_connect_data (window, "delete_event",
                           G_CALLBACK (caja_window_delete_event_callback), NULL, NULL,
                           G_CONNECT_AFTER);
#if GTK_CHECK_VERSION (3, 0, 0)
    gtk_application_add_window (GTK_APPLICATION (application),
				    GTK_WINDOW (window));
#else
    g_signal_connect_object (window, "destroy",
                             G_CALLBACK (caja_application_destroyed_window), application, 0);
    caja_application_window_list = g_list_prepend (caja_application_window_list, window);
#endif
    /* Do not yet show the window. It will be shown later on if it can
     * successfully display its initial URI. Otherwise it will be destroyed
     * without ever having seen the light of day.
     */

    return window;
}

static void
spatial_window_destroyed_callback (void *user_data, GObject *window)
{
    caja_application_spatial_window_list = g_list_remove (caja_application_spatial_window_list, window);

}

CajaWindow *
caja_application_get_spatial_window (CajaApplication *application,
                                    CajaWindow      *requesting_window,
                                    const char      *startup_id,
                                    GFile           *location,
                                    GdkScreen       *screen,
                                    gboolean        *existing)
{
    CajaWindow *window;
    gchar *uri;

    g_return_val_if_fail (CAJA_IS_APPLICATION (application), NULL);
    window = CAJA_WINDOW
    		(caja_application_get_existing_spatial_window (location));

	if (window != NULL) {
		if (existing != NULL) {
			*existing = TRUE;
        }

		return window;
    }

	if (existing != NULL) {
		*existing = FALSE;
	}
#if GTK_CHECK_VERSION (3, 0, 0)
    window = create_window (application, CAJA_TYPE_SPATIAL_WINDOW, screen);
#else
    window = create_window (application, CAJA_TYPE_SPATIAL_WINDOW, startup_id, screen);
#endif
    if (requesting_window)
    {
        /* Center the window over the requesting window by default */
        int orig_x, orig_y, orig_width, orig_height;
        int new_x, new_y, new_width, new_height;

        gtk_window_get_position (GTK_WINDOW (requesting_window),
                                 &orig_x, &orig_y);
        gtk_window_get_size (GTK_WINDOW (requesting_window),
                             &orig_width, &orig_height);
        gtk_window_get_default_size (GTK_WINDOW (window),
                                     &new_width, &new_height);

        new_x = orig_x + (orig_width - new_width) / 2;
        new_y = orig_y + (orig_height - new_height) / 2;

        if (orig_width - new_width < 10)
        {
            new_x += 10;
            new_y += 10;
        }

        gtk_window_move (GTK_WINDOW (window), new_x, new_y);
    }

    caja_application_spatial_window_list = g_list_prepend (caja_application_spatial_window_list, window);
    g_object_weak_ref (G_OBJECT (window),
                       spatial_window_destroyed_callback, NULL);

    uri = g_file_get_uri (location);
    caja_debug_log (FALSE, CAJA_DEBUG_LOG_DOMAIN_USER,
                    "present NEW spatial window=%p: %s",
                    window, uri);
    g_free (uri);

    return window;
}

CajaWindow *
caja_application_create_navigation_window (CajaApplication *application,
#if !GTK_CHECK_VERSION (3, 0, 0)
        const char          *startup_id,
#endif
        GdkScreen           *screen)
{
    CajaWindow *window;
    char *geometry_string;
    gboolean maximized;

    g_return_val_if_fail (CAJA_IS_APPLICATION (application), NULL);
#if GTK_CHECK_VERSION (3, 0, 0)
    window = create_window (application, CAJA_TYPE_NAVIGATION_WINDOW, screen);
#else
    window = create_window (application, CAJA_TYPE_NAVIGATION_WINDOW, startup_id, screen);
#endif
    maximized = g_settings_get_boolean (caja_window_state,
                    CAJA_WINDOW_STATE_MAXIMIZED);
    if (maximized)
    {
        gtk_window_maximize (GTK_WINDOW (window));
    }
    else
    {
        gtk_window_unmaximize (GTK_WINDOW (window));
    }

    geometry_string = g_settings_get_string (caja_window_state,
                        CAJA_WINDOW_STATE_GEOMETRY);
    if (geometry_string != NULL &&
            geometry_string[0] != 0)
    {
        eel_gtk_window_set_initial_geometry_from_string
        (GTK_WINDOW (window),
         geometry_string,
         CAJA_NAVIGATION_WINDOW_MIN_WIDTH,
         CAJA_NAVIGATION_WINDOW_MIN_HEIGHT,
         TRUE);
    }
    g_free (geometry_string);

    caja_debug_log (FALSE, CAJA_DEBUG_LOG_DOMAIN_USER,
                    "create new navigation window=%p",
                    window);

    return window;
}
#if !GTK_CHECK_VERSION (3, 0, 0)
/* callback for changing the directory the desktop points to */
static void
desktop_location_changed_callback (gpointer user_data)
{
    if (caja_application_desktop_windows != NULL)
    {
        g_list_foreach (caja_application_desktop_windows,
                        (GFunc) caja_desktop_window_update_directory, NULL);
    }
}
#endif
/* callback for showing or hiding the desktop based on the user's preference */
static void
desktop_changed_callback (gpointer user_data)
{
    CajaApplication *application;

    application = CAJA_APPLICATION (user_data);
    if (g_settings_get_boolean (mate_background_preferences, MATE_BG_KEY_SHOW_DESKTOP))
    {
        caja_application_open_desktop (application);
    }
    else
    {
        caja_application_close_desktop ();
    }
}

static gboolean
window_can_be_closed (CajaWindow *window)
{
    if (!CAJA_IS_DESKTOP_WINDOW (window))
    {
        return TRUE;
    }

    return FALSE;
}

static void
check_screen_lock_and_mount (CajaApplication *application,
                             GVolume *volume)
{
        if (application->screensaver_active)
        {
                /* queue the volume, to mount it after the screensaver state changed */
                g_debug ("Queuing volume %p", volume);
                application->volume_queue = g_list_prepend (application->volume_queue,
                                                              g_object_ref (volume));
        } else {
                /* mount it immediately */
		caja_file_operations_mount_volume (NULL, volume, TRUE);
        }       
}

static void
volume_removed_callback (GVolumeMonitor *monitor,
                         GVolume *volume,
                         CajaApplication *application)
{
        g_debug ("Volume %p removed, removing from the queue", volume);

        /* clear it from the queue, if present */
        application->volume_queue =
                g_list_remove (application->volume_queue, volume);
}

static void
volume_added_callback (GVolumeMonitor *monitor,
                       GVolume *volume,
                       CajaApplication *application)
{
    if (g_settings_get_boolean (caja_media_preferences, CAJA_PREFERENCES_MEDIA_AUTOMOUNT) &&
            g_volume_should_automount (volume) &&
            g_volume_can_mount (volume))
    {
        check_screen_lock_and_mount (application, volume);
    }
    else
    {
        /* Allow caja_autorun() to run. When the mount is later
         * added programmatically (i.e. for a blank CD),
         * caja_autorun() will be called by mount_added_callback(). */
        caja_allow_autorun_for_volume (volume);
        caja_allow_autorun_for_volume_finish (volume);
    }
}

static void
drive_eject_cb (GObject *source_object,
                GAsyncResult *res,
                gpointer user_data)
{
    GError *error;
    char *primary;
    char *name;
    error = NULL;
    if (!g_drive_eject_with_operation_finish (G_DRIVE (source_object), res, &error))
    {
        if (error->code != G_IO_ERROR_FAILED_HANDLED)
        {
            name = g_drive_get_name (G_DRIVE (source_object));
            primary = g_strdup_printf (_("Unable to eject %s"), name);
            g_free (name);
            eel_show_error_dialog (primary,
                                   error->message,
                                   NULL);
            g_free (primary);
        }
        g_error_free (error);
    }
}

static void
drive_eject_button_pressed (GDrive *drive,
                            CajaApplication *application)
{
    GMountOperation *mount_op;

    mount_op = gtk_mount_operation_new (NULL);
    g_drive_eject_with_operation (drive, 0, mount_op, NULL, drive_eject_cb, NULL);
    g_object_unref (mount_op);
}

static void
drive_listen_for_eject_button (GDrive *drive, CajaApplication *application)
{
    g_signal_connect (drive,
                      "eject-button",
                      G_CALLBACK (drive_eject_button_pressed),
                      application);
}

static void
drive_connected_callback (GVolumeMonitor *monitor,
                          GDrive *drive,
                          CajaApplication *application)
{
    drive_listen_for_eject_button (drive, application);
}

static void
autorun_show_window (GMount *mount, gpointer user_data)
{
    GFile *location;
    CajaApplication *application = user_data;
    CajaWindow *window;
    gboolean existing;

    location = g_mount_get_root (mount);
    existing = FALSE;

    /* There should probably be an easier way to do this */
    if (g_settings_get_boolean (caja_preferences, CAJA_PREFERENCES_ALWAYS_USE_BROWSER)) {
        window = caja_application_create_navigation_window (application,
#if !GTK_CHECK_VERSION (3, 0, 0)
                                                            NULL,
#endif
                                                            gdk_screen_get_default ());
    }
    else
    {
        window = caja_application_get_spatial_window (application,
                                                      NULL,
                                                      NULL,
                                                      location,
                                                      gdk_screen_get_default (),
                                                      NULL);
    }

    caja_window_go_to (window, location);

    g_object_unref (location);
}
#if GTK_CHECK_VERSION (3, 0, 0)
static void
mount_added_callback (GVolumeMonitor *monitor,
              GMount *mount,
              CajaApplication *application)
{
    CajaDirectory *directory;
    GFile *root;
    gchar *uri;
        
    root = g_mount_get_root (mount);
    uri = g_file_get_uri (root);

    g_debug ("Added mount at uri %s", uri);
    g_free (uri);
    
    directory = caja_directory_get_existing (root);
    g_object_unref (root);
    if (directory != NULL) {
        caja_directory_force_reload (directory);
        caja_directory_unref (directory);
    }
}
#else
static void
mount_added_callback (GVolumeMonitor *monitor,
                      GMount *mount,
                      CajaApplication *application)
{
    CajaDirectory *directory;
    GFile *root;

    root = g_mount_get_root (mount);
    directory = caja_directory_get_existing (root);
    g_object_unref (root);
    if (directory != NULL)
    {
        caja_directory_force_reload (directory);
        caja_directory_unref (directory);
    }

    caja_autorun (mount, autorun_show_window, application);
}
#endif
static CajaWindowSlot *
get_first_navigation_slot (GList *slot_list)
{
    GList *l;

    for (l = slot_list; l != NULL; l = l->next)
    {
        if (CAJA_IS_NAVIGATION_WINDOW_SLOT (l->data))
        {
            return l->data;
        }
    }

    return NULL;
}

/* We redirect some slots and close others */
static gboolean
should_close_slot_with_mount (CajaWindow *window,
                              CajaWindowSlot *slot,
                              GMount *mount)
{
    if (CAJA_IS_SPATIAL_WINDOW (window))
    {
        return TRUE;
    }
    return caja_navigation_window_slot_should_close_with_mount (CAJA_NAVIGATION_WINDOW_SLOT (slot),
            mount);
}

/* Called whenever a mount is unmounted. Check and see if there are
 * any windows open displaying contents on the mount. If there are,
 * close them.  It would also be cool to save open window and position
 * info.
 *
 * This is also called on pre_unmount.
 */
static void
mount_removed_callback (GVolumeMonitor *monitor,
                        GMount *mount,
                        CajaApplication *application)
{
    GList *window_list, *node, *close_list;
    CajaWindow *window;
    CajaWindowSlot *slot;
    CajaWindowSlot *force_no_close_slot;
    GFile *root, *computer;
    gboolean unclosed_slot;

    close_list = NULL;
    force_no_close_slot = NULL;
    unclosed_slot = FALSE;

    /* Check and see if any of the open windows are displaying contents from the unmounted mount */
#if GTK_CHECK_VERSION (3, 0, 0)
    window_list = gtk_application_get_windows (GTK_APPLICATION (application));
#else
    window_list = caja_application_get_window_list ();
#endif
    root = g_mount_get_root (mount);
    /* Construct a list of windows to be closed. Do not add the non-closable windows to the list. */
    for (node = window_list; node != NULL; node = node->next)
    {
        window = CAJA_WINDOW (node->data);
        if (window != NULL && window_can_be_closed (window))
        {
            GList *l;
            GList *lp;
            GFile *location;

            for (lp = window->details->panes; lp != NULL; lp = lp->next)
            {
                CajaWindowPane *pane;
                pane = (CajaWindowPane*) lp->data;
                for (l = pane->slots; l != NULL; l = l->next)
                {
                    slot = l->data;
                    location = slot->location;
                    if (g_file_has_prefix (location, root) ||
                            g_file_equal (location, root))
                    {
                        close_list = g_list_prepend (close_list, slot);

                        if (!should_close_slot_with_mount (window, slot, mount))
                        {
                            /* We'll be redirecting this, not closing */
                            unclosed_slot = TRUE;
                        }
                    }
                    else
                    {
                        unclosed_slot = TRUE;
                    }
                } /* for all slots */
            } /* for all panes */
        }
    }

    if (caja_application_desktop_windows == NULL &&
            !unclosed_slot)
    {
        /* We are trying to close all open slots. Keep one navigation slot open. */
        force_no_close_slot = get_first_navigation_slot (close_list);
    }

    /* Handle the windows in the close list. */
    for (node = close_list; node != NULL; node = node->next)
    {
        slot = node->data;
        window = slot->pane->window;

        if (should_close_slot_with_mount (window, slot, mount) &&
                slot != force_no_close_slot)
        {
            caja_window_slot_close (slot);
        }
        else
        {
            computer = g_file_new_for_uri ("computer:///");
            caja_window_slot_go_to (slot, computer, FALSE);
            g_object_unref(computer);
        }
    }

    g_list_free (close_list);
}

static char *
icon_to_string (GIcon *icon)
{
    const char * const *names;
    GFile *file;

    if (icon == NULL)
    {
        return NULL;
    }
    else if (G_IS_THEMED_ICON (icon))
    {
        names = g_themed_icon_get_names (G_THEMED_ICON (icon));
        return g_strjoinv (":", (char **)names);
    }
    else if (G_IS_FILE_ICON (icon))
    {
        file = g_file_icon_get_file (G_FILE_ICON (icon));
        return g_file_get_path (file);
    }
    return NULL;
}

static GIcon *
icon_from_string (const char *string)
{
    GFile *file;
    GIcon *icon;
    gchar **names;

    if (g_path_is_absolute (string))
    {
        file = g_file_new_for_path (string);
        icon = g_file_icon_new (file);
        g_object_unref (file);
        return icon;
    }
    else
    {
        names = g_strsplit (string, ":", 0);
        icon = g_themed_icon_new_from_names (names, -1);
        g_strfreev (names);
        return icon;
    }
    return NULL;
}
#if GTK_CHECK_VERSION (3, 0, 0)
static char *
caja_application_get_session_data (CajaApplication *self)
#else
static char *
caja_application_get_session_data (void)
#endif
{
    xmlDocPtr doc;
    xmlNodePtr root_node, history_node;
#if GTK_CHECK_VERSION (3, 0, 0)
    GList *l, *window_list;
#else
    GList *l;
#endif
    char *data;
    unsigned n_processed;
    xmlSaveCtxtPtr ctx;
    xmlBufferPtr buffer;

    doc = xmlNewDoc ("1.0");

    root_node = xmlNewNode (NULL, "session");
    xmlDocSetRootElement (doc, root_node);

    history_node = xmlNewChild (root_node, NULL, "history", NULL);

    n_processed = 0;
    for (l = caja_get_history_list (); l != NULL; l = l->next) {
        CajaBookmark *bookmark;
        xmlNodePtr bookmark_node;
        GIcon *icon;
        char *tmp;

        bookmark = l->data;

        bookmark_node = xmlNewChild (history_node, NULL, "bookmark", NULL);

        tmp = caja_bookmark_get_name (bookmark);
        xmlNewProp (bookmark_node, "name", tmp);
        g_free (tmp);

        icon = caja_bookmark_get_icon (bookmark);
#if GTK_CHECK_VERSION (3, 0, 0)
        tmp = g_icon_to_string (icon);
#else
        tmp = icon_to_string (icon);
#endif
        g_object_unref (icon);
        if (tmp) {
            xmlNewProp (bookmark_node, "icon", tmp);
            g_free (tmp);
        }

        tmp = caja_bookmark_get_uri (bookmark);
        xmlNewProp (bookmark_node, "uri", tmp);
        g_free (tmp);

        if (caja_bookmark_get_has_custom_name (bookmark)) {
            xmlNewProp (bookmark_node, "has_custom_name", "TRUE");
        }

        if (++n_processed > 50) { /* prevent history list from growing arbitrarily large. */
            break;
        }
    }
#if GTK_CHECK_VERSION (3, 0, 0)
    window_list = gtk_application_get_windows (GTK_APPLICATION (self));
    for (l = window_list; l != NULL; l = l->next) {
#else
    for (l = caja_application_window_list; l != NULL; l = l->next) {
#endif
        xmlNodePtr win_node, slot_node;
        CajaWindow *window;
        CajaWindowSlot *slot, *active_slot;
        GList *slots, *m;
        char *tmp;

        window = l->data;

        slots = caja_window_get_slots (window);
        active_slot = caja_window_get_active_slot (window);

        /* store one slot as window location. Otherwise
         * older Caja versions will bail when reading the file. */
        tmp = caja_window_slot_get_location_uri (active_slot);

        if (eel_uri_is_desktop (tmp)) {
            g_list_free (slots);
            g_free (tmp);
            continue;
        }

        win_node = xmlNewChild (root_node, NULL, "window", NULL);
        
        xmlNewProp (win_node, "location", tmp);
        g_free (tmp);
        
        xmlNewProp (win_node, "type", CAJA_IS_NAVIGATION_WINDOW (window) ? "navigation" : "spatial");

        if (CAJA_IS_NAVIGATION_WINDOW (window)) { /* spatial windows store their state as file metadata */
            GdkWindow *gdk_window;

            tmp = eel_gtk_window_get_geometry_string (GTK_WINDOW (window));
            xmlNewProp (win_node, "geometry", tmp);
            g_free (tmp);

            gdk_window = gtk_widget_get_window (GTK_WIDGET (window));

            if (gdk_window &&
                gdk_window_get_state (gdk_window) & GDK_WINDOW_STATE_MAXIMIZED) {
                xmlNewProp (win_node, "maximized", "TRUE");
            }

            if (gdk_window &&
                gdk_window_get_state (gdk_window) & GDK_WINDOW_STATE_STICKY) {
                xmlNewProp (win_node, "sticky", "TRUE");
            }

            if (gdk_window &&
                gdk_window_get_state (gdk_window) & GDK_WINDOW_STATE_ABOVE) {
                xmlNewProp (win_node, "keep-above", "TRUE");
            }
        }

        for (m = slots; m != NULL; m = m->next) {
            slot = CAJA_WINDOW_SLOT (m->data);

            slot_node = xmlNewChild (win_node, NULL, "slot", NULL);

            tmp = caja_window_slot_get_location_uri (slot);
            xmlNewProp (slot_node, "location", tmp);
            g_free (tmp);

            if (slot == active_slot) {
                xmlNewProp (slot_node, "active", "TRUE");
            }
        }

        g_list_free (slots);
    }

    buffer = xmlBufferCreate ();
    xmlIndentTreeOutput = 1;
    ctx = xmlSaveToBuffer (buffer, "UTF-8", XML_SAVE_FORMAT);
    if (xmlSaveDoc (ctx, doc) < 0 ||
        xmlSaveFlush (ctx) < 0) {
        g_message ("failed to save session");
    }
    
    xmlSaveClose(ctx);
    data = g_strndup (buffer->content, buffer->use);
    xmlBufferFree (buffer);

    xmlFreeDoc (doc);

    return data;
}
void
caja_application_load_session (CajaApplication *application)

{
    xmlDocPtr doc;
    gboolean bail;
    xmlNodePtr root_node;
    GKeyFile *state_file;
    char *data;
#if GTK_CHECK_VERSION (3, 0, 0)
    caja_application_smclient_initialize (application);
#endif
   if (!egg_sm_client_is_resumed (application->smclient))
    {
        return;
  } 

    state_file = egg_sm_client_get_state_file (application->smclient);
    if (!state_file)
    {
        return;
    }

    data = g_key_file_get_string (state_file,
                                  "Caja",
                                  "documents",
                                  NULL);
    if (data == NULL)
    {
        return;
    }

    bail = TRUE;
    
    doc = xmlReadMemory (data, strlen (data), NULL, "UTF-8", 0);
    if (doc != NULL && (root_node = xmlDocGetRootElement (doc)) != NULL)
    {
        xmlNodePtr node;

        bail = FALSE;

        for (node = root_node->children; node != NULL; node = node->next)
        {

            if (g_strcmp0 (node->name, "text") == 0)
            {
                continue;
            }
            else if (g_strcmp0 (node->name, "history") == 0)
            {
                xmlNodePtr bookmark_node;
                gboolean emit_change;

                emit_change = FALSE;

                for (bookmark_node = node->children; bookmark_node != NULL; bookmark_node = bookmark_node->next)
                {
                    if (g_strcmp0 (bookmark_node->name, "text") == 0)
                    {
                        continue;
                    }
                    else if (g_strcmp0 (bookmark_node->name, "bookmark") == 0)
                    {
                        xmlChar *name, *icon_str, *uri;
                        gboolean has_custom_name;
                        GIcon *icon;
                        GFile *location;

                        uri = xmlGetProp (bookmark_node, "uri");
                        name = xmlGetProp (bookmark_node, "name");
                        has_custom_name = xmlHasProp (bookmark_node, "has_custom_name") ? TRUE : FALSE;
                        icon_str = xmlGetProp (bookmark_node, "icon");
                        icon = NULL;
                        if (icon_str)
                        {
#if GTK_CHECK_VERSION (3, 0, 0)
                            icon = g_icon_new_for_string (icon_str, NULL);
#else
                            icon = icon_from_string (icon_str);
#endif
                        }
                        location = g_file_new_for_uri (uri);

                        emit_change |= caja_add_to_history_list_no_notify (location, name, has_custom_name, icon);

                        g_object_unref (location);

                        if (icon)
                        {
                            g_object_unref (icon);
                        }
                        xmlFree (name);
                        xmlFree (uri);
                        xmlFree (icon_str);
                    }
                    else
                    {
                        g_message ("unexpected bookmark node %s while parsing session data", bookmark_node->name);
                        bail = TRUE;
                        continue;
                    }
                }

                if (emit_change)
                {
                    caja_send_history_list_changed ();
                }
            }

            else if (g_strcmp0 (node->name, "window") == 0)

            {
                CajaWindow *window;
                xmlChar *type, *location_uri, *slot_uri;
                xmlNodePtr slot_node;
                GFile *location;
                int i;

                type = xmlGetProp (node, "type");
                if (type == NULL)
                {
                    g_message ("empty type node while parsing session data");
                    bail = TRUE;
                    continue;
                }

                location_uri = xmlGetProp (node, "location");
                if (location_uri == NULL)
                {
                    g_message ("empty location node while parsing session data");
                    bail = TRUE;
                    xmlFree (type);
                    continue;
                }

                if (g_strcmp0 (type, "navigation") == 0)
                {
                    xmlChar *geometry;
#if GTK_CHECK_VERSION (3, 0, 0)
                    window = caja_application_create_navigation_window (application, gdk_screen_get_default ());
#else
                    window = caja_application_create_navigation_window (application, NULL, gdk_screen_get_default ());
#endif
                    geometry = xmlGetProp (node, "geometry");
                    if (geometry != NULL)
                    {
                        eel_gtk_window_set_initial_geometry_from_string
                        (GTK_WINDOW (window),
                         geometry,
                         CAJA_NAVIGATION_WINDOW_MIN_WIDTH,
                         CAJA_NAVIGATION_WINDOW_MIN_HEIGHT,
                         FALSE);
                    }
                    xmlFree (geometry);

                    if (xmlHasProp (node, "maximized"))
                    {
                        gtk_window_maximize (GTK_WINDOW (window));
                    }
                    else
                    {
                        gtk_window_unmaximize (GTK_WINDOW (window));
                    }

                    if (xmlHasProp (node, "sticky"))
                    {
                        gtk_window_stick (GTK_WINDOW (window));
                    }
                    else
                    {
                        gtk_window_unstick (GTK_WINDOW (window));
                    }

                    if (xmlHasProp (node, "keep-above"))
                    {
                        gtk_window_set_keep_above (GTK_WINDOW (window), TRUE);
                    }
                    else
                    {
                        gtk_window_set_keep_above (GTK_WINDOW (window), FALSE);
                    }

                    for (i = 0, slot_node = node->children; slot_node != NULL; slot_node = slot_node->next)
                    {
                        if (g_strcmp0 (slot_node->name, "slot") == 0)
                        {
                            slot_uri = xmlGetProp (slot_node, "location");
                            if (slot_uri != NULL)
                            {
                                CajaWindowSlot *slot;

                                if (i == 0)
                                {
                                    slot = window->details->active_pane->active_slot;
                                }
                                else
                                {
                                    slot = caja_window_open_slot (window->details->active_pane, CAJA_WINDOW_OPEN_SLOT_APPEND);
                                }

                                location = g_file_new_for_uri (slot_uri);
                                caja_window_slot_open_location (slot, location, FALSE);

                                if (xmlHasProp (slot_node, "active"))
                                {
                                    caja_window_set_active_slot (slot->pane->window, slot);
                                }

                                i++;
                            }
                            xmlFree (slot_uri);
                        }
                    }

                    if (i == 0)
                    {
                        /* This may be an old session file */
                        location = g_file_new_for_uri (location_uri);
                        caja_window_slot_open_location (window->details->active_pane->active_slot, location, FALSE);
                        g_object_unref (location);
                    }
                }
                else if (g_strcmp0 (type, "spatial") == 0)
                {
                    location = g_file_new_for_uri (location_uri);
                    window = caja_application_get_spatial_window (application, NULL, NULL, 
                    											  location, gdk_screen_get_default (),
                    											  NULL);

					caja_window_go_to (window, location);

                    g_object_unref (location);
                }
                else
                {
                    g_message ("unknown window type \"%s\" while parsing session data", type);
                    bail = TRUE;
                }

                xmlFree (type);
                xmlFree (location_uri);
            }
            else
            {
                g_message ("unexpected node %s while parsing session data", node->name);
                bail = TRUE;
                continue;
            }
        }
    }

    if (doc != NULL)
    {
        xmlFreeDoc (doc);
    }

    g_free (data);

    if (bail)
    {
        g_message ("failed to load session");
    }
}

#if GTK_CHECK_VERSION (3, 0, 0)
static gboolean
do_cmdline_sanity_checks (CajaApplication *self,
              gboolean perform_self_check,
              gboolean version,
              gboolean kill_shell,
              gchar **remaining)
{
    gboolean retval = FALSE;

    if (perform_self_check && (remaining != NULL || kill_shell)) {
        g_printerr ("%s\n",
                _("--check cannot be used with other options."));
        goto out;
    }

    if (kill_shell && remaining != NULL) {
        g_printerr ("%s\n",
                _("--quit cannot be used with URIs."));
        goto out;
    }

    if (self->priv->geometry != NULL &&
        remaining != NULL && remaining[0] != NULL && remaining[1] != NULL) {
        g_printerr ("%s\n",
                _("--geometry cannot be used with more than one URI."));
        goto out;
    }

    retval = TRUE;

 out:
    return retval;
}

static void
do_perform_self_checks (gint *exit_status)
{
#ifndef CAJA_OMIT_SELF_CHECK
    /* Run the checks (each twice) for caja and libcaja-private. */

    caja_run_self_checks ();
    caja_run_lib_self_checks ();
    eel_exit_if_self_checks_failed ();

    caja_run_self_checks ();
    caja_run_lib_self_checks ();
    eel_exit_if_self_checks_failed ();
#endif

    *exit_status = EXIT_SUCCESS;
}

static gboolean
running_in_mate (void)
{
    return (g_strcmp0 (g_getenv ("XDG_CURRENT_DESKTOP"), "MATE") == 0)
        || (g_strcmp0 (g_getenv ("XDG_SESSION_DESKTOP"), "MATE") == 0)
        || (g_strcmp0 (g_getenv ("DESKTOP_SESSION"), "MATE") == 0);
}

static gboolean
running_as_root (void)
{
    return geteuid () == 0;
}

static gboolean
caja_application_local_command_line (GApplication *application,
                     gchar ***arguments,
                     gint *exit_status)
{
    gboolean perform_self_check = FALSE;
    gboolean version = FALSE;
    gboolean browser_window = FALSE;
    gboolean kill_shell = FALSE;
    gboolean autostart_mode = FALSE;
    const gchar *autostart_id;
    gboolean no_default_window = FALSE;
    gchar **remaining = NULL;
    const gchar *hint = "";
    CajaApplication *self = CAJA_APPLICATION (application);

    const GOptionEntry options[] = {
#ifndef CAJA_OMIT_SELF_CHECK
        { "check", 'c', 0, G_OPTION_ARG_NONE, &perform_self_check, 
          N_("Perform a quick set of self-check tests."), NULL },
#endif
        { "version", '\0', 0, G_OPTION_ARG_NONE, &version,
          N_("Show the version of the program."), NULL },
        { "geometry", 'g', 0, G_OPTION_ARG_STRING, &self->priv->geometry,
          N_("Create the initial window with the given geometry."), N_("GEOMETRY") },
        { "no-default-window", 'n', 0, G_OPTION_ARG_NONE, &no_default_window,
          N_("Only create windows for explicitly specified URIs."), NULL },
        { "no-desktop", '\0', 0, G_OPTION_ARG_NONE, &self->priv->no_desktop,
          N_("Do not manage the desktop (ignore the preference set in the preferences dialog)."), NULL },
        { "browser", '\0', 0, G_OPTION_ARG_NONE, &browser_window, 
          N_("Open a browser window."), NULL },
        { "quit", 'q', 0, G_OPTION_ARG_NONE, &kill_shell, 
          N_("Quit Caja."), NULL },
        { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &remaining, NULL,  N_("[URI...]") },

        { NULL }
    };
    GOptionContext *context;
    GError *error = NULL;
    gint argc = 0;
    gchar **argv = NULL;

    *exit_status = EXIT_SUCCESS;

    context = g_option_context_new (_("\n\nBrowse the file system with the file manager"));
    g_option_context_add_main_entries (context, options, NULL);
    g_option_context_add_group (context, gtk_get_option_group (TRUE));

	g_option_context_add_group (context, egg_sm_client_get_option_group ());


    /* we need to do this here, as parsing the EggSMClient option context,
	 * unsets this variable.
	 */
	autostart_id = g_getenv ("DESKTOP_AUTOSTART_ID");
	if (autostart_id != NULL && *autostart_id != '\0') {
		autostart_mode = TRUE;
        }


    argv = *arguments;
    argc = g_strv_length (argv);

    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_printerr ("Could not parse arguments: %s\n", error->message);
        g_error_free (error);

        *exit_status = EXIT_FAILURE;
        goto out;
    }

    if (version) {
        g_print ("MATE caja " PACKAGE_VERSION "\n");
        goto out;
    }

    if (!do_cmdline_sanity_checks (self, perform_self_check,
                       version, kill_shell, remaining)) {
        *exit_status = EXIT_FAILURE;
        goto out;
    }

    if (perform_self_check) {
        do_perform_self_checks (exit_status);
        goto out;
    }

    g_debug ("Parsing local command line, no_default_window %d, quit %d, "
           "self checks %d, no_desktop %d",
           no_default_window, kill_shell, perform_self_check, self->priv->no_desktop);

    g_application_register (application, NULL, &error);

    if (error != NULL) {
        g_printerr ("Could not register the application: %s\n", error->message);
        g_error_free (error);

        *exit_status = EXIT_FAILURE;
        goto out;
    }

    if (kill_shell) {
        g_debug ("Killing application, as requested");
        g_action_group_activate_action (G_ACTION_GROUP (application),
                        "quit", NULL);
        goto out;
    }

    /* Initialize  and load session info if available */
    /* Load session if and only if autostarted        */
    /* This avoids errors on command line invocation  */
    if (autostart_id != NULL ) {
        caja_application_load_session (self);
    }


    GFile **files;
    gint idx, len;

    len = 0;
    files = NULL;

    /* Convert args to GFiles */
    if (remaining != NULL) {
        GFile *file;
        GPtrArray *file_array;

        file_array = g_ptr_array_new ();

        for (idx = 0; remaining[idx] != NULL; idx++) {
            file = g_file_new_for_commandline_arg (remaining[idx]);
            if (file != NULL) {
                g_ptr_array_add (file_array, file);
            }
        }

        len = file_array->len;
        files = (GFile **) g_ptr_array_free (file_array, FALSE);
        g_strfreev (remaining);
    }

    if (files == NULL && !no_default_window) {
        files = g_malloc0 (2 * sizeof (GFile *));
        len = 1;

        files[0] = g_file_new_for_path (g_get_home_dir ());
        files[1] = NULL;
    }

    /*Set up geometry and --browser options for "Open" */

    if (browser_window == TRUE && self->priv->geometry == NULL){
            hint = "browser";
    }

    else if (browser_window == FALSE && self->priv->geometry != NULL){
            hint = g_strdup(self->priv->geometry);
    }

    else if (browser_window == TRUE && self->priv->geometry != NULL){
             hint = g_strconcat("browser","=", self->priv->geometry, NULL);
    }

    else {
        hint = ("");
    }

    /* Invoke "Open" to create new windows */
    if (len > 0)  {
        g_application_open (application, files, len, hint);
    }

    for (idx = 0; idx < len; idx++) {
        g_object_unref (files[idx]);
    }
    g_free (files);

 out:
    g_option_context_free (context);


    return TRUE;    
}


static void
init_icons_and_styles (void)
{
    GtkCssProvider *provider;
    GError *error = NULL;

    /* add our custom CSS provider */
    provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_path (provider,
				CAJA_DATADIR G_DIR_SEPARATOR_S "caja.css", &error);

    if (error != NULL) {
        g_warning ("Can't parse Caja' CSS custom description: %s\n", error->message);
        g_error_free (error);
    } else {
        gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                               GTK_STYLE_PROVIDER (provider),
                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

    g_object_unref (provider);

    /* initialize search path for custom icons */
    gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
                       CAJA_DATADIR G_DIR_SEPARATOR_S "icons");
}

static void
init_desktop (CajaApplication *self)
{
    /* Initialize the desktop link monitor singleton */
    caja_desktop_link_monitor_get ();

    if (!self->priv->no_desktop &&
        !g_settings_get_boolean (mate_background_preferences,
                     MATE_BG_KEY_SHOW_DESKTOP)) {
        self->priv->no_desktop = TRUE;
    }

    if (running_as_root () || !running_in_mate ())
	{
        /* do not manage desktop when running as root or on other desktops */
        self->priv->no_desktop = TRUE;
    }

    if (!self->priv->no_desktop) {
        caja_application_open_desktop (self);
    }

    /* Monitor the preference to show or hide the desktop */
    g_signal_connect_swapped (mate_background_preferences, "changed::" MATE_BG_KEY_SHOW_DESKTOP,
                  G_CALLBACK (desktop_changed_callback),
                  self);
}

static gboolean 
caja_application_save_accel_map (gpointer data)
{
    if (save_of_accel_map_requested) {
        char *accel_map_filename;
         accel_map_filename = caja_get_accel_map_file ();
         if (accel_map_filename) {
             gtk_accel_map_save (accel_map_filename);
             g_free (accel_map_filename);
         }
        save_of_accel_map_requested = FALSE;
    }

    return FALSE;
}

static void 
queue_accel_map_save_callback (GtkAccelMap *object, gchar *accel_path,
        guint accel_key, GdkModifierType accel_mods,
        gpointer user_data)
{
    if (!save_of_accel_map_requested) {
        save_of_accel_map_requested = TRUE;
        g_timeout_add_seconds (CAJA_ACCEL_MAP_SAVE_DELAY, 
                caja_application_save_accel_map, NULL);
    }
}

static void
init_gtk_accels (void)
{
    char *accel_map_filename;

    /* load accelerator map, and register save callback */
    accel_map_filename = caja_get_accel_map_file ();
    if (accel_map_filename) {
        gtk_accel_map_load (accel_map_filename);
        g_free (accel_map_filename);
    }

    g_signal_connect (gtk_accel_map_get (), "changed",
              G_CALLBACK (queue_accel_map_save_callback), NULL);
}


static void
caja_application_startup (GApplication *app)
{
    GList *drives;
    CajaApplication *application;
    CajaApplication *self = CAJA_APPLICATION (app);
    GApplication *instance;
    gboolean exit_with_last_window;
    const gchar *autostart_id;
    exit_with_last_window = TRUE;

    /* chain up to the GTK+ implementation early, so gtk_init()
     * is called for us.
     */
    G_APPLICATION_CLASS (caja_application_parent_class)->startup (app);

    /* Initialize preferences. This is needed so that proper
     * defaults are available before any preference peeking
     * happens.
     */
    caja_global_preferences_init ();

	/* initialize the session manager client */
	caja_application_smclient_startup (self);

    /* register views */
    fm_icon_view_register ();
    fm_desktop_icon_view_register ();
    fm_list_view_register ();
    fm_compact_view_register ();
#if ENABLE_EMPTY_VIEW
    fm_empty_view_register ();
#endif /* ENABLE_EMPTY_VIEW */

    /* register sidebars */
    caja_places_sidebar_register ();
    caja_information_panel_register ();
    fm_tree_view_register ();
    caja_history_sidebar_register ();
    caja_notes_viewer_register (); /* also property page */
    caja_emblem_sidebar_register ();

    /* register property pages */
    caja_image_properties_page_register ();

    /* initialize theming */
    init_icons_and_styles ();
    init_gtk_accels ();
    
    /* initialize caja modules */
    caja_module_setup ();

    /* attach menu-provider module callback */
    menu_provider_init_callback ();
    
    /* Initialize the UI handler singleton for file operations */
    /*notify_init (GETTEXT_PACKAGE);  */

    /* Watch for unmounts so we can close open windows */
    /* TODO-gio: This should be using the UNMOUNTED feature of GFileMonitor instead */
     self->priv->volume_monitor = g_volume_monitor_get ();
    g_signal_connect_object ( self->priv->volume_monitor, "mount_removed",
                             G_CALLBACK (mount_removed_callback), self, 0);
    g_signal_connect_object ( self->priv->volume_monitor, "mount_pre_unmount",
                             G_CALLBACK (mount_removed_callback), self, 0);
    g_signal_connect_object ( self->priv->volume_monitor, "mount_added",
                             G_CALLBACK (mount_added_callback), self, 0);
    g_signal_connect_object ( self->priv->volume_monitor, "volume_added",
                             G_CALLBACK (volume_added_callback), self, 0);
    g_signal_connect_object ( self->priv->volume_monitor, "volume_removed",
                             G_CALLBACK (volume_removed_callback), self, 0);
    g_signal_connect_object ( self->priv->volume_monitor, "drive_connected",
                             G_CALLBACK (drive_connected_callback), self, 0);

    /* listen for eject button presses */
    drives = g_volume_monitor_get_connected_drives ( self->priv->volume_monitor);
    self->automount_idle_id =
    g_idle_add_full (G_PRIORITY_LOW,
                     automount_all_volumes_idle_cb,
                     self, NULL);

    /* Check the user's ~/.caja directories and post warnings
     * if there are problems.
     */
    check_required_directories (self);
    init_desktop (self);

    /* exit_with_last_window is already set to TRUE, and we need to keep that value
     * on other desktops, running from the command line,  or when running caja as root. 
     * Otherwise, we read the value from the configuration.
     */

    if (running_in_mate () && !running_as_root())
    {
        exit_with_last_window = g_settings_get_boolean (caja_preferences,   
                                CAJA_PREFERENCES_EXIT_WITH_LAST_WINDOW);
    }

    instance = g_application_get_default ();

    if (exit_with_last_window == FALSE){
        g_application_hold (G_APPLICATION (instance));
    }

    do_upgrades_once (self);
}

static void
caja_application_quit_mainloop (GApplication *app)
{
    caja_icon_info_clear_caches ();
    caja_application_save_accel_map (NULL);

    G_APPLICATION_CLASS (caja_application_parent_class)->quit_mainloop (app);
}

static void
caja_application_class_init (CajaApplicationClass *class)
{
        GObjectClass *object_class;
    GApplicationClass *application_class;

        object_class = G_OBJECT_CLASS (class);
        object_class->finalize = caja_application_finalize;

    application_class = G_APPLICATION_CLASS (class);
    application_class->startup = caja_application_startup;
    application_class->quit_mainloop = caja_application_quit_mainloop;
    application_class->open = caja_application_open;
    application_class->local_command_line = caja_application_local_command_line;

g_type_class_add_private (class, sizeof (CajaApplicationPriv));
}

CajaApplication *
caja_application_new (void)
{
    /*only register application when running in MATE/not as root 
    to avoid errors in some GTK versions when invoking "sudo caja" */

    if (!running_as_root ()){
        return g_object_new (CAJA_TYPE_APPLICATION,
                    "application-id", "org.mate.caja",
                    "register-session", TRUE,
                    "flags", G_APPLICATION_HANDLES_OPEN,
                     NULL);
    }
    else{
    return g_object_new (CAJA_TYPE_APPLICATION,
                    "application-id", "org.mate.caja",
                    "flags", G_APPLICATION_HANDLES_OPEN,
                     NULL);
   }
}
#else
static void
caja_application_class_init (CajaApplicationClass *class)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (class);
    object_class->finalize = caja_application_finalize;
}
#endif
