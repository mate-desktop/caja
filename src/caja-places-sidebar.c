/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *  Caja
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Authors : Mr Jamie McCracken (jamiemcc at blueyonder dot co dot uk)
 *            Cosimo Cecchi <cosimoc@gnome.org>
 *
 */

#include <config.h>

#include <eel/eel-debug.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-glib-extensions.h>
#if GTK_CHECK_VERSION(3,0,0)
#include <eel/eel-graphic-effects.h>
#endif
#include <eel/eel-string.h>
#include <eel/eel-stock-dialogs.h>
#if !GTK_CHECK_VERSION(3,0,0)
#include <eel/eel-gdk-pixbuf-extensions.h>
#endif
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libcaja-private/caja-debug-log.h>
#include <libcaja-private/caja-dnd.h>
#include <libcaja-private/caja-bookmark.h>
#include <libcaja-private/caja-global-preferences.h>
#include <libcaja-private/caja-sidebar-provider.h>
#include <libcaja-private/caja-module.h>
#include <libcaja-private/caja-file.h>
#include <libcaja-private/caja-file-utilities.h>
#include <libcaja-private/caja-file-operations.h>
#include <libcaja-private/caja-trash-monitor.h>
#include <libcaja-private/caja-icon-names.h>
#include <libcaja-private/caja-autorun.h>
#include <libcaja-private/caja-window-info.h>
#include <libcaja-private/caja-window-slot-info.h>
#include <gio/gio.h>

#include "caja-bookmark-list.h"
#include "caja-places-sidebar.h"
#include "caja-window.h"

#define EJECT_BUTTON_XPAD 6
#define ICON_CELL_XPAD 6

typedef struct
{
    GtkScrolledWindow  parent;
    GtkTreeView        *tree_view;
    GtkCellRenderer    *eject_text_cell_renderer;
    GtkCellRenderer    *icon_cell_renderer;
    GtkCellRenderer    *icon_padding_cell_renderer;
    GtkCellRenderer    *padding_cell_renderer;
    char               *uri;
    GtkListStore       *store;
    GtkTreeModel       *filter_model;
    CajaWindowInfo *window;
    CajaBookmarkList *bookmarks;
    GVolumeMonitor *volume_monitor;

    gboolean devices_header_added;
    gboolean bookmarks_header_added;

    /* DnD */
    GList     *drag_list;
    gboolean  drag_data_received;
    int       drag_data_info;
    gboolean  drop_occured;

    GtkWidget *popup_menu;
    GtkWidget *popup_menu_open_in_new_tab_item;
    GtkWidget *popup_menu_remove_item;
    GtkWidget *popup_menu_rename_item;
    GtkWidget *popup_menu_separator_item;
    GtkWidget *popup_menu_mount_item;
    GtkWidget *popup_menu_unmount_item;
    GtkWidget *popup_menu_eject_item;
    GtkWidget *popup_menu_rescan_item;
    GtkWidget *popup_menu_format_item;
    GtkWidget *popup_menu_empty_trash_item;
    GtkWidget *popup_menu_start_item;
    GtkWidget *popup_menu_stop_item;

    /* volume mounting - delayed open process */
    gboolean mounting;
    CajaWindowSlotInfo *go_to_after_mount_slot;
    CajaWindowOpenFlags go_to_after_mount_flags;

    GtkTreePath *eject_highlight_path;
} CajaPlacesSidebar;

typedef struct
{
    GtkScrolledWindowClass parent;
} CajaPlacesSidebarClass;

typedef struct
{
    GObject parent;
} CajaPlacesSidebarProvider;

typedef struct
{
    GObjectClass parent;
} CajaPlacesSidebarProviderClass;

enum
{
    PLACES_SIDEBAR_COLUMN_ROW_TYPE,
    PLACES_SIDEBAR_COLUMN_URI,
    PLACES_SIDEBAR_COLUMN_DRIVE,
    PLACES_SIDEBAR_COLUMN_VOLUME,
    PLACES_SIDEBAR_COLUMN_MOUNT,
    PLACES_SIDEBAR_COLUMN_NAME,
    PLACES_SIDEBAR_COLUMN_ICON,
    PLACES_SIDEBAR_COLUMN_INDEX,
    PLACES_SIDEBAR_COLUMN_EJECT,
    PLACES_SIDEBAR_COLUMN_NO_EJECT,
    PLACES_SIDEBAR_COLUMN_BOOKMARK,
    PLACES_SIDEBAR_COLUMN_TOOLTIP,
    PLACES_SIDEBAR_COLUMN_EJECT_ICON,
    PLACES_SIDEBAR_COLUMN_SECTION_TYPE,
    PLACES_SIDEBAR_COLUMN_HEADING_TEXT,

    PLACES_SIDEBAR_COLUMN_COUNT
};

typedef enum
{
    PLACES_BUILT_IN,
    PLACES_MOUNTED_VOLUME,
    PLACES_BOOKMARK,
    PLACES_HEADING,
} PlaceType;

typedef enum {
    SECTION_COMPUTER,
    SECTION_DEVICES,
    SECTION_BOOKMARKS,
    SECTION_NETWORK,
} SectionType;

static void  caja_places_sidebar_iface_init        (CajaSidebarIface         *iface);
static void  sidebar_provider_iface_init               (CajaSidebarProviderIface *iface);
static GType caja_places_sidebar_provider_get_type (void);
static void  open_selected_bookmark                    (CajaPlacesSidebar        *sidebar,
        GtkTreeModel                 *model,
        GtkTreePath                  *path,
        CajaWindowOpenFlags flags);

#if GTK_CHECK_VERSION (3, 0, 0)
static void  caja_places_sidebar_style_updated         (GtkWidget                    *widget);
#else
static void  caja_places_sidebar_style_set             (GtkWidget                    *widget,
        GtkStyle                     *previous_style);
#endif
static gboolean eject_or_unmount_bookmark              (CajaPlacesSidebar *sidebar,
        GtkTreePath *path);
static gboolean eject_or_unmount_selection             (CajaPlacesSidebar *sidebar);
static void  check_unmount_and_eject                   (GMount *mount,
        GVolume *volume,
        GDrive *drive,
        gboolean *show_unmount,
        gboolean *show_eject);

static void bookmarks_check_popup_sensitivity          (CajaPlacesSidebar *sidebar);

/* Identifiers for target types */
enum
{
    GTK_TREE_MODEL_ROW,
    TEXT_URI_LIST
};

/* Target types for dragging from the shortcuts list */
static const GtkTargetEntry caja_shortcuts_source_targets[] =
{
    { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, GTK_TREE_MODEL_ROW }
};

/* Target types for dropping into the shortcuts list */
static const GtkTargetEntry caja_shortcuts_drop_targets [] =
{
    { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, GTK_TREE_MODEL_ROW },
    { "text/uri-list", 0, TEXT_URI_LIST }
};

/* Drag and drop interface declarations */
typedef struct
{
    GtkTreeModelFilter parent;

    CajaPlacesSidebar *sidebar;
} CajaShortcutsModelFilter;

typedef struct
{
    GtkTreeModelFilterClass parent_class;
} CajaShortcutsModelFilterClass;

#define CAJA_SHORTCUTS_MODEL_FILTER_TYPE (_caja_shortcuts_model_filter_get_type ())
#define CAJA_SHORTCUTS_MODEL_FILTER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_SHORTCUTS_MODEL_FILTER_TYPE, CajaShortcutsModelFilter))

GType _caja_shortcuts_model_filter_get_type (void);
static void caja_shortcuts_model_filter_drag_source_iface_init (GtkTreeDragSourceIface *iface);

G_DEFINE_TYPE_WITH_CODE (CajaShortcutsModelFilter,
                         _caja_shortcuts_model_filter,
                         GTK_TYPE_TREE_MODEL_FILTER,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
                                 caja_shortcuts_model_filter_drag_source_iface_init));

static GtkTreeModel *caja_shortcuts_model_filter_new (CajaPlacesSidebar *sidebar,
        GtkTreeModel          *child_model,
        GtkTreePath           *root);

G_DEFINE_TYPE_WITH_CODE (CajaPlacesSidebar, caja_places_sidebar, GTK_TYPE_SCROLLED_WINDOW,
                         G_IMPLEMENT_INTERFACE (CAJA_TYPE_SIDEBAR,
                                 caja_places_sidebar_iface_init));

G_DEFINE_TYPE_WITH_CODE (CajaPlacesSidebarProvider, caja_places_sidebar_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CAJA_TYPE_SIDEBAR_PROVIDER,
                                 sidebar_provider_iface_init));

static GdkPixbuf *
get_eject_icon (gboolean highlighted)
{
    GdkPixbuf *eject;
    CajaIconInfo *eject_icon_info;
    int icon_size;

    icon_size = caja_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU);

    eject_icon_info = caja_icon_info_lookup_from_name ("media-eject", icon_size);
    eject = caja_icon_info_get_pixbuf_at_size (eject_icon_info, icon_size);

    if (highlighted) {
        GdkPixbuf *high;
#if GTK_CHECK_VERSION(3,0,0)
        high = eel_create_spotlight_pixbuf (eject);
#else
        high = eel_gdk_pixbuf_render (eject, 1, 255, 255, 0, 0);
#endif
        g_object_unref (eject);
        eject = high;
    }

    g_object_unref (eject_icon_info);

    return eject;
}

static gboolean
is_built_in_bookmark (CajaFile *file)
{
    gboolean built_in;
    gint idx;

    built_in = FALSE;

    for (idx = 0; idx < G_USER_N_DIRECTORIES; idx++) {
        /* PUBLIC_SHARE and TEMPLATES are not in our built-in list */
        if (caja_file_is_user_special_directory (file, idx)) {
            if (idx != G_USER_DIRECTORY_PUBLIC_SHARE &&  idx != G_USER_DIRECTORY_TEMPLATES) {
                built_in = TRUE;
            }

            break;
        }
    }

    return built_in;
}

static GtkTreeIter
add_heading (CajaPlacesSidebar *sidebar,
         SectionType section_type,
         const gchar *title)
{
    GtkTreeIter iter, child_iter;

    gtk_list_store_append (sidebar->store, &iter);
    gtk_list_store_set (sidebar->store, &iter,
                PLACES_SIDEBAR_COLUMN_ROW_TYPE, PLACES_HEADING,
                PLACES_SIDEBAR_COLUMN_SECTION_TYPE, section_type,    
                PLACES_SIDEBAR_COLUMN_HEADING_TEXT, title,
                PLACES_SIDEBAR_COLUMN_EJECT, FALSE,
                PLACES_SIDEBAR_COLUMN_NO_EJECT, TRUE,
                -1);

    gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (sidebar->filter_model));
    gtk_tree_model_filter_convert_child_iter_to_iter (GTK_TREE_MODEL_FILTER (sidebar->filter_model),
                              &child_iter,
                              &iter);

    return child_iter;
}

static void
check_heading_for_section (CajaPlacesSidebar *sidebar,
               SectionType section_type)
{
    switch (section_type) {
    case SECTION_DEVICES:
        if (!sidebar->devices_header_added) {
            add_heading (sidebar, SECTION_DEVICES,
                     _("Devices"));
            sidebar->devices_header_added = TRUE;
        }

        break;
    case SECTION_BOOKMARKS:
        if (!sidebar->bookmarks_header_added) {
            add_heading (sidebar, SECTION_BOOKMARKS,
                     _("Bookmarks"));
            sidebar->bookmarks_header_added = TRUE;
        }

        break;
    default:
        break;
    }
}

static GtkTreeIter
add_place (CajaPlacesSidebar *sidebar,
           PlaceType place_type,
           SectionType section_type,
           const char *name,
           GIcon *icon,
           const char *uri,
           GDrive *drive,
           GVolume *volume,
           GMount *mount,
           const int index,
           const char *tooltip)
{
    GdkPixbuf      *pixbuf;
    GtkTreeIter     iter, child_iter;
    GdkPixbuf      *eject;
    CajaIconInfo   *icon_info;
    int             icon_size;
    gboolean        show_eject;
    gboolean        show_unmount;
    gboolean        show_eject_button;

    check_heading_for_section (sidebar, section_type);

    icon_size = caja_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU);
    icon_info = caja_icon_info_lookup (icon, icon_size);

    pixbuf = caja_icon_info_get_pixbuf_at_size (icon_info, icon_size);
    g_object_unref (icon_info);

    check_unmount_and_eject (mount, volume, drive,
                             &show_unmount, &show_eject);

    if (show_unmount || show_eject)
    {
        g_assert (place_type != PLACES_BOOKMARK);
    }

    if (mount == NULL)
    {
        show_eject_button = FALSE;
    }
    else
    {
        show_eject_button = (show_unmount || show_eject);
    }

    if (show_eject_button) {
        eject = get_eject_icon (FALSE);
    } else {
        eject = NULL;
    }

    gtk_list_store_append (sidebar->store, &iter);
    gtk_list_store_set (sidebar->store, &iter,
                        PLACES_SIDEBAR_COLUMN_ICON, pixbuf,
                        PLACES_SIDEBAR_COLUMN_NAME, name,
                        PLACES_SIDEBAR_COLUMN_URI, uri,
                        PLACES_SIDEBAR_COLUMN_DRIVE, drive,
                        PLACES_SIDEBAR_COLUMN_VOLUME, volume,
                        PLACES_SIDEBAR_COLUMN_MOUNT, mount,
                        PLACES_SIDEBAR_COLUMN_ROW_TYPE, place_type,
                        PLACES_SIDEBAR_COLUMN_INDEX, index,
                        PLACES_SIDEBAR_COLUMN_EJECT, show_eject_button,
                        PLACES_SIDEBAR_COLUMN_NO_EJECT, !show_eject_button,
                        PLACES_SIDEBAR_COLUMN_BOOKMARK, place_type != PLACES_BOOKMARK,
                        PLACES_SIDEBAR_COLUMN_TOOLTIP, tooltip,
                        PLACES_SIDEBAR_COLUMN_EJECT_ICON, eject,
                        PLACES_SIDEBAR_COLUMN_SECTION_TYPE, section_type,
                        -1);

    if (pixbuf != NULL)
    {
        g_object_unref (pixbuf);
    }
    gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (sidebar->filter_model));
    gtk_tree_model_filter_convert_child_iter_to_iter (GTK_TREE_MODEL_FILTER (sidebar->filter_model),
            &child_iter,
            &iter);
    return child_iter;
}

static void
compare_for_selection (CajaPlacesSidebar *sidebar,
                       const gchar *location,
                       const gchar *added_uri,
                       const gchar *last_uri,
                       GtkTreeIter *iter,
                       GtkTreePath **path)
{
    int res;

    res = g_strcmp0 (added_uri, last_uri);

    if (res == 0)
    {
        /* last_uri always comes first */
        if (*path != NULL)
        {
            gtk_tree_path_free (*path);
        }
        *path = gtk_tree_model_get_path (sidebar->filter_model,
                                         iter);
    }
    else if (g_strcmp0 (location, added_uri) == 0)
    {
        if (*path == NULL)
        {
            *path = gtk_tree_model_get_path (sidebar->filter_model,
                                             iter);
        }
    }
}

static void
update_places (CajaPlacesSidebar *sidebar)
{
    CajaBookmark *bookmark;
    GtkTreeSelection *selection;
    GtkTreeIter last_iter;
    GtkTreePath *select_path;
    GtkTreeModel *model;
    GVolumeMonitor *volume_monitor;
    GList *mounts, *l, *ll;
    GMount *mount;
    GList *drives;
    GDrive *drive;
    GList *volumes;
    GVolume *volume;
    int bookmark_count, index;
    char *location, *mount_uri, *name, *desktop_path, *last_uri;
    const gchar *path;
    GIcon *icon;
    GFile *root;
    CajaWindowSlotInfo *slot;
    char *tooltip;
    GList *network_mounts;
    GList *xdg_dirs;
    CajaFile *file;

    model = NULL;
    last_uri = NULL;
    select_path = NULL;

    selection = gtk_tree_view_get_selection (sidebar->tree_view);
    if (gtk_tree_selection_get_selected (selection, &model, &last_iter))
    {
        gtk_tree_model_get (model,
                            &last_iter,
                            PLACES_SIDEBAR_COLUMN_URI, &last_uri, -1);
    }
    gtk_list_store_clear (sidebar->store);

    sidebar->devices_header_added = FALSE;
    sidebar->bookmarks_header_added = FALSE;

    slot = caja_window_info_get_active_slot (sidebar->window);
    location = caja_window_slot_info_get_current_location (slot);

    volume_monitor = sidebar->volume_monitor;

    /* COMPUTER */
    last_iter = add_heading (sidebar, SECTION_COMPUTER,
                             _("Computer"));

    /* add built in bookmarks */
    desktop_path = caja_get_desktop_directory ();

    /* home folder */
    if (strcmp (g_get_home_dir(), desktop_path) != 0) {
        char *display_name;

        mount_uri = caja_get_home_directory_uri ();
        display_name = g_filename_display_basename (g_get_home_dir ());
        icon = g_themed_icon_new (CAJA_ICON_HOME);
        last_iter = add_place (sidebar, PLACES_BUILT_IN,
                               SECTION_COMPUTER,
                               display_name, icon,
                               mount_uri, NULL, NULL, NULL, 0,
                               _("Open your personal folder"));
        g_object_unref (icon);
        g_free (display_name);
        compare_for_selection (sidebar,
                               location, mount_uri, last_uri,
                               &last_iter, &select_path);
        g_free (mount_uri);
    }

    /* desktop */
    mount_uri = g_filename_to_uri (desktop_path, NULL, NULL);
    icon = g_themed_icon_new (CAJA_ICON_DESKTOP);
    last_iter = add_place (sidebar, PLACES_BUILT_IN,
                           SECTION_COMPUTER,
                           _("Desktop"), icon,
                           mount_uri, NULL, NULL, NULL, 0,
                           _("Open the contents of your desktop in a folder"));
    g_object_unref (icon);
    compare_for_selection (sidebar,
                           location, mount_uri, last_uri,
                           &last_iter, &select_path);
    g_free (mount_uri);
    g_free (desktop_path);

    /* file system root */
    mount_uri = "file:///"; /* No need to strdup */
    icon = g_themed_icon_new (CAJA_ICON_FILESYSTEM);
    last_iter = add_place (sidebar, PLACES_BUILT_IN,
                           SECTION_COMPUTER,
                           _("File System"), icon,
                           mount_uri, NULL, NULL, NULL, 0,
                           _("Open the contents of the File System"));
    g_object_unref (icon);
    compare_for_selection (sidebar,
                           location, mount_uri, last_uri,
                           &last_iter, &select_path);

    
    /* XDG directories */
    xdg_dirs = NULL;
    for (index = 0; index < G_USER_N_DIRECTORIES; index++) {

        if (index == G_USER_DIRECTORY_DESKTOP ||
            index == G_USER_DIRECTORY_TEMPLATES ||
            index == G_USER_DIRECTORY_PUBLIC_SHARE) {
            continue;
        }

        path = g_get_user_special_dir (index);

        /* xdg resets special dirs to the home directory in case
         * it's not finiding what it expects. We don't want the home
         * to be added multiple times in that weird configuration.
         */
        if (path == NULL
            || g_strcmp0 (path, g_get_home_dir ()) == 0
            || g_list_find_custom (xdg_dirs, path, (GCompareFunc) g_strcmp0) != NULL) {
            continue;
        }

        root = g_file_new_for_path (path);
        name = g_file_get_basename (root);
        icon = caja_user_special_directory_get_gicon (index);
        mount_uri = g_file_get_uri (root);
        tooltip = g_file_get_parse_name (root);

        last_iter = add_place (sidebar, PLACES_BUILT_IN,
                               SECTION_COMPUTER,
                               name, icon, mount_uri,
                               NULL, NULL, NULL, 0,
                               tooltip);
        compare_for_selection (sidebar,
                               location, mount_uri, last_uri,
                               &last_iter, &select_path);
        g_free (name);
        g_object_unref (root);
        g_object_unref (icon);
        g_free (mount_uri);
        g_free (tooltip);

        xdg_dirs = g_list_prepend (xdg_dirs, (char *)path);
    }
    g_list_free (xdg_dirs);

    mount_uri = "trash:///"; /* No need to strdup */
    icon = caja_trash_monitor_get_icon ();
    last_iter = add_place (sidebar, PLACES_BUILT_IN,
                           SECTION_COMPUTER,
                           _("Trash"), icon, mount_uri,
                           NULL, NULL, NULL, 0,
                           _("Open the trash"));
    compare_for_selection (sidebar,
                           location, mount_uri, last_uri,
                           &last_iter, &select_path);
    g_object_unref (icon);

    /* first go through all connected drives */
    drives = g_volume_monitor_get_connected_drives (volume_monitor);

    for (l = drives; l != NULL; l = l->next)
    {
        drive = l->data;

        volumes = g_drive_get_volumes (drive);
        if (volumes != NULL)
        {
            for (ll = volumes; ll != NULL; ll = ll->next)
            {
                volume = ll->data;
                mount = g_volume_get_mount (volume);
                if (mount != NULL)
                {
                    /* Show mounted volume in the sidebar */
                    icon = g_mount_get_icon (mount);
                    root = g_mount_get_default_location (mount);
                    mount_uri = g_file_get_uri (root);
                    name = g_mount_get_name (mount);
                    tooltip = g_file_get_parse_name (root);

                    last_iter = add_place (sidebar, PLACES_MOUNTED_VOLUME,
                                           SECTION_DEVICES,
                                           name, icon, mount_uri,
                                           drive, volume, mount, 0, tooltip);
                    compare_for_selection (sidebar,
                                           location, mount_uri, last_uri,
                                           &last_iter, &select_path);
                    g_object_unref (root);
                    g_object_unref (mount);
                    g_object_unref (icon);
                    g_free (tooltip);
                    g_free (name);
                    g_free (mount_uri);
                }
                else
                {
                    /* Do show the unmounted volumes in the sidebar;
                     * this is so the user can mount it (in case automounting
                     * is off).
                     *
                     * Also, even if automounting is enabled, this gives a visual
                     * cue that the user should remember to yank out the media if
                     * he just unmounted it.
                     */
                    icon = g_volume_get_icon (volume);
                    name = g_volume_get_name (volume);
                    tooltip = g_strdup_printf (_("Mount and open %s"), name);

                    last_iter = add_place (sidebar, PLACES_MOUNTED_VOLUME,
                                           SECTION_DEVICES,
                                           name, icon, NULL,
                                           drive, volume, NULL, 0, tooltip);
                    g_object_unref (icon);
                    g_free (name);
                    g_free (tooltip);
                }
                g_object_unref (volume);
            }
            g_list_free (volumes);
        }
        else
        {
            if (g_drive_is_media_removable (drive) && !g_drive_is_media_check_automatic (drive))
            {
                /* If the drive has no mountable volumes and we cannot detect media change.. we
                 * display the drive in the sidebar so the user can manually poll the drive by
                 * right clicking and selecting "Rescan..."
                 *
                 * This is mainly for drives like floppies where media detection doesn't
                 * work.. but it's also for human beings who like to turn off media detection
                 * in the OS to save battery juice.
                 */
                icon = g_drive_get_icon (drive);
                name = g_drive_get_name (drive);
                tooltip = g_strdup_printf (_("Mount and open %s"), name);

                last_iter = add_place (sidebar, PLACES_BUILT_IN,
                                       SECTION_DEVICES,
                                       name, icon, NULL,
                                       drive, NULL, NULL, 0, tooltip);
                g_object_unref (icon);
                g_free (tooltip);
                g_free (name);
            }
        }
        g_object_unref (drive);
    }
    g_list_free (drives);

    /* add all volumes that is not associated with a drive */
    volumes = g_volume_monitor_get_volumes (volume_monitor);
    for (l = volumes; l != NULL; l = l->next)
    {
        volume = l->data;
        drive = g_volume_get_drive (volume);
        if (drive != NULL)
        {
            g_object_unref (volume);
            g_object_unref (drive);
            continue;
        }
        mount = g_volume_get_mount (volume);
        if (mount != NULL)
        {
            icon = g_mount_get_icon (mount);
            root = g_mount_get_default_location (mount);
            mount_uri = g_file_get_uri (root);
            tooltip = g_file_get_parse_name (root);
            g_object_unref (root);
            name = g_mount_get_name (mount);
            last_iter = add_place (sidebar, PLACES_MOUNTED_VOLUME,
                                   SECTION_DEVICES,
                                   name, icon, mount_uri,
                                   NULL, volume, mount, 0, tooltip);
            compare_for_selection (sidebar,
                                   location, mount_uri, last_uri,
                                   &last_iter, &select_path);
            g_object_unref (mount);
            g_object_unref (icon);
            g_free (name);
            g_free (tooltip);
            g_free (mount_uri);
        }
        else
        {
            /* see comment above in why we add an icon for an unmounted mountable volume */
            icon = g_volume_get_icon (volume);
            name = g_volume_get_name (volume);
            last_iter = add_place (sidebar, PLACES_MOUNTED_VOLUME,
                                   SECTION_DEVICES,
                                   name, icon, NULL,
                                   NULL, volume, NULL, 0, name);
            g_object_unref (icon);
            g_free (name);
        }
        g_object_unref (volume);
    }
    g_list_free (volumes);

    /* add mounts that has no volume (/etc/mtab mounts, ftp, sftp,...) */
    network_mounts = NULL;
    mounts = g_volume_monitor_get_mounts (volume_monitor);

    for (l = mounts; l != NULL; l = l->next)
    {
        mount = l->data;
        if (g_mount_is_shadowed (mount))
        {
            g_object_unref (mount);
            continue;
        }
        volume = g_mount_get_volume (mount);
        if (volume != NULL)
        {
            g_object_unref (volume);
            g_object_unref (mount);
            continue;
        }
        root = g_mount_get_default_location (mount);

        if (!g_file_is_native (root)) {
            network_mounts = g_list_prepend (network_mounts, g_object_ref (mount));
            continue;
        }

        icon = g_mount_get_icon (mount);
        mount_uri = g_file_get_uri (root);
        name = g_mount_get_name (mount);
        tooltip = g_file_get_parse_name (root);
        last_iter = add_place (sidebar, PLACES_MOUNTED_VOLUME,
                               SECTION_COMPUTER,
                               name, icon, mount_uri,
                               NULL, NULL, mount, 0, tooltip);
        compare_for_selection (sidebar,
                               location, mount_uri, last_uri,
                               &last_iter, &select_path);
        g_object_unref (root);
        g_object_unref (mount);
        g_object_unref (icon);
        g_free (name);
        g_free (mount_uri);
        g_free (tooltip);
    }
    g_list_free (mounts);


    /* add bookmarks */
    bookmark_count = caja_bookmark_list_length (sidebar->bookmarks);

    for (index = 0; index < bookmark_count; ++index) {
        bookmark = caja_bookmark_list_item_at (sidebar->bookmarks, index);

        if (caja_bookmark_uri_known_not_to_exist (bookmark)) {
            continue;
        }

        root = caja_bookmark_get_location (bookmark);
        file = caja_file_get (root);

        if (is_built_in_bookmark (file)) {
            g_object_unref (root);
            caja_file_unref (file);
            continue;
        }

        name = caja_bookmark_get_name (bookmark);
        icon = caja_bookmark_get_icon (bookmark);
        mount_uri = caja_bookmark_get_uri (bookmark);
        tooltip = g_file_get_parse_name (root);

        last_iter = add_place (sidebar, PLACES_BOOKMARK,
                               SECTION_BOOKMARKS,
                               name, icon, mount_uri,
                               NULL, NULL, NULL, index,
                               tooltip);
        compare_for_selection (sidebar,
                               location, mount_uri, last_uri,
                               &last_iter, &select_path);
        g_free (name);
        g_object_unref (root);
        g_object_unref (icon);
        g_free (mount_uri);
        g_free (tooltip);
    }

    /* network */
    last_iter = add_heading (sidebar, SECTION_NETWORK,
                             _("Network"));

    network_mounts = g_list_reverse (network_mounts);
    for (l = network_mounts; l != NULL; l = l->next) {
        mount = l->data;
        root = g_mount_get_default_location (mount);
        icon = g_mount_get_icon (mount);
        mount_uri = g_file_get_uri (root);
        name = g_mount_get_name (mount);
        tooltip = g_file_get_parse_name (root);
        last_iter = add_place (sidebar, PLACES_MOUNTED_VOLUME,
                               SECTION_NETWORK,
                               name, icon, mount_uri,
                               NULL, NULL, mount, 0, tooltip);
        compare_for_selection (sidebar,
                               location, mount_uri, last_uri,
                               &last_iter, &select_path);
        g_object_unref (root);
        g_object_unref (mount);
        g_object_unref (icon);
        g_free (name);
        g_free (mount_uri);
        g_free (tooltip);
    }

    g_list_free_full (network_mounts, g_object_unref);

    /* network:// */
    mount_uri = "network:///"; /* No need to strdup */
    icon = g_themed_icon_new (CAJA_ICON_NETWORK);
    last_iter = add_place (sidebar, PLACES_BUILT_IN,
                           SECTION_NETWORK,
                           _("Browse Network"), icon,
                           mount_uri, NULL, NULL, NULL, 0,
                           _("Browse the contents of the network"));
    g_object_unref (icon);
    compare_for_selection (sidebar,
                           location, mount_uri, last_uri,
                           &last_iter, &select_path);
    
    g_free (location);

    if (select_path != NULL) {
        gtk_tree_selection_select_path (selection, select_path);
    }

    if (select_path != NULL) {
        gtk_tree_path_free (select_path);
    }

    g_free (last_uri);
}

static void
mount_added_callback (GVolumeMonitor *volume_monitor,
                      GMount *mount,
                      CajaPlacesSidebar *sidebar)
{
    update_places (sidebar);
}

static void
mount_removed_callback (GVolumeMonitor *volume_monitor,
                        GMount *mount,
                        CajaPlacesSidebar *sidebar)
{
    update_places (sidebar);
}

static void
mount_changed_callback (GVolumeMonitor *volume_monitor,
                        GMount *mount,
                        CajaPlacesSidebar *sidebar)
{
    update_places (sidebar);
}

static void
volume_added_callback (GVolumeMonitor *volume_monitor,
                       GVolume *volume,
                       CajaPlacesSidebar *sidebar)
{
    update_places (sidebar);
}

static void
volume_removed_callback (GVolumeMonitor *volume_monitor,
                         GVolume *volume,
                         CajaPlacesSidebar *sidebar)
{
    update_places (sidebar);
}

static void
volume_changed_callback (GVolumeMonitor *volume_monitor,
                         GVolume *volume,
                         CajaPlacesSidebar *sidebar)
{
    update_places (sidebar);
}

static void
drive_disconnected_callback (GVolumeMonitor *volume_monitor,
                             GDrive         *drive,
                             CajaPlacesSidebar *sidebar)
{
    update_places (sidebar);
}

static void
drive_connected_callback (GVolumeMonitor *volume_monitor,
                          GDrive         *drive,
                          CajaPlacesSidebar *sidebar)
{
    update_places (sidebar);
}

static void
drive_changed_callback (GVolumeMonitor *volume_monitor,
                        GDrive         *drive,
                        CajaPlacesSidebar *sidebar)
{
    update_places (sidebar);
}

static gboolean
over_eject_button (CajaPlacesSidebar *sidebar,
                   gint x,
                   gint y,
                   GtkTreePath **path)
{
    GtkTreeViewColumn *column;
    GtkTextDirection direction;
    int width, total_width;
    int eject_button_size;
    gboolean show_eject;
    GtkTreeIter iter;
    GtkTreeModel *model;

    *path = NULL;
    model = gtk_tree_view_get_model (sidebar->tree_view);

   if (gtk_tree_view_get_path_at_pos (sidebar->tree_view,
                                      x, y,
                                      path, &column, NULL, NULL)) {

        gtk_tree_model_get_iter (model, &iter, *path);
        gtk_tree_model_get (model, &iter,
                            PLACES_SIDEBAR_COLUMN_EJECT, &show_eject,
                            -1);

        if (!show_eject) {
            goto out;
        }

        total_width = 0;

        gtk_widget_style_get (GTK_WIDGET (sidebar->tree_view),
                              "horizontal-separator", &width,
                              NULL);
        total_width += width;

        direction = gtk_widget_get_direction (GTK_WIDGET (sidebar->tree_view));
        if (direction != GTK_TEXT_DIR_RTL) {
            gtk_tree_view_column_cell_get_position (column,
                                                    sidebar->padding_cell_renderer,
                                                    NULL, &width);
            total_width += width;

            gtk_tree_view_column_cell_get_position (column,
                                                    sidebar->icon_padding_cell_renderer,
                                                    NULL, &width);
            total_width += width;
            
            gtk_tree_view_column_cell_get_position (column,
                                                    sidebar->icon_cell_renderer,
                                                    NULL, &width);
            total_width += width;

            gtk_tree_view_column_cell_get_position (column,
                                                    sidebar->eject_text_cell_renderer,
                                                    NULL, &width);
            total_width += width;
        }

        total_width += EJECT_BUTTON_XPAD;

        eject_button_size = caja_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU);

        if (x - total_width >= 0 &&
#if GTK_CHECK_VERSION (3, 0, 0)
            /* fix unwanted unmount requests if clicking on the label */
            x >= total_width - eject_button_size &&
            x >= 80 &&
#endif
            x - total_width <= eject_button_size) {
            return TRUE;
        }
    }

out:
    if (*path != NULL) {
        gtk_tree_path_free (*path);
        *path = NULL;
    }

    return FALSE;
}

static gboolean
clicked_eject_button (CajaPlacesSidebar *sidebar,
                      GtkTreePath **path)
{
    GdkEvent *event = gtk_get_current_event ();
    GdkEventButton *button_event = (GdkEventButton *) event;

    if ((event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE) &&
         over_eject_button (sidebar, button_event->x, button_event->y, path)) {
        return TRUE;
    }

    return FALSE;
}

static void
desktop_location_changed_callback (gpointer user_data)
{
    CajaPlacesSidebar *sidebar;

    sidebar = CAJA_PLACES_SIDEBAR (user_data);

    update_places (sidebar);
}

static void
loading_uri_callback (CajaWindowInfo *window,
                      char *location,
                      CajaPlacesSidebar *sidebar)
{
    GtkTreeSelection *selection;
    GtkTreeIter       iter;
    gboolean          valid;
    char             *uri;

    if (strcmp (sidebar->uri, location) != 0)
    {
        g_free (sidebar->uri);
        sidebar->uri = g_strdup (location);

        /* set selection if any place matches location */
        selection = gtk_tree_view_get_selection (sidebar->tree_view);
        gtk_tree_selection_unselect_all (selection);
        valid = gtk_tree_model_get_iter_first (sidebar->filter_model, &iter);

        while (valid)
        {
            gtk_tree_model_get (sidebar->filter_model, &iter,
                                PLACES_SIDEBAR_COLUMN_URI, &uri,
                                -1);
            if (uri != NULL)
            {
                if (strcmp (uri, location) == 0)
                {
                    g_free (uri);
                    gtk_tree_selection_select_iter (selection, &iter);
                    break;
                }
                g_free (uri);
            }
            valid = gtk_tree_model_iter_next (sidebar->filter_model, &iter);
        }
    }
}

/* Computes the appropriate row and position for dropping */
static gboolean
compute_drop_position (GtkTreeView *tree_view,
                       int                      x,
                       int                      y,
                       GtkTreePath            **path,
                       GtkTreeViewDropPosition *pos,
                       CajaPlacesSidebar *sidebar)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    PlaceType place_type;
    SectionType section_type;

    if (!gtk_tree_view_get_dest_row_at_pos (tree_view,
                                            x,
                                            y,
                                            path,
                                            pos)) {
        return FALSE;
    }

    model = gtk_tree_view_get_model (tree_view);

    gtk_tree_model_get_iter (model, &iter, *path);
    gtk_tree_model_get (model, &iter,
                        PLACES_SIDEBAR_COLUMN_ROW_TYPE, &place_type,
                        PLACES_SIDEBAR_COLUMN_SECTION_TYPE, &section_type,
                        -1);

    if (place_type == PLACES_HEADING &&
        section_type != SECTION_BOOKMARKS &&
        section_type != SECTION_NETWORK) {
        /* never drop on headings, but the bookmarks or network heading
         * is a special case, so we can create new bookmarks by dragging
         * at the beginning or end of the bookmark list.
         */
        gtk_tree_path_free (*path);
        *path = NULL;

        return FALSE;
    }

    if (section_type != SECTION_BOOKMARKS &&
        sidebar->drag_data_received &&
        sidebar->drag_data_info == GTK_TREE_MODEL_ROW) {
        /* don't allow dropping bookmarks into non-bookmark areas */
        gtk_tree_path_free (*path);
        *path = NULL;

        return FALSE;
    }

    /* drag to top or bottom of bookmark list to add a bookmark */
    if (place_type == PLACES_HEADING && section_type == SECTION_BOOKMARKS) {
        *pos = GTK_TREE_VIEW_DROP_AFTER;
    } else if (place_type == PLACES_HEADING && section_type == SECTION_NETWORK) {
        *pos = GTK_TREE_VIEW_DROP_BEFORE;
    } else {
        /* or else you want to drag items INTO the existing bookmarks */
        *pos = GTK_TREE_VIEW_DROP_INTO_OR_BEFORE;
    }

    if (*pos != GTK_TREE_VIEW_DROP_BEFORE &&
        sidebar->drag_data_received &&
        sidebar->drag_data_info == GTK_TREE_MODEL_ROW) {
        /* bookmark rows are never dragged into other bookmark rows */
        *pos = GTK_TREE_VIEW_DROP_AFTER;
    }

    return TRUE;
}

static gboolean
get_drag_data (GtkTreeView *tree_view,
               GdkDragContext *context,
               unsigned int time)
{
    GdkAtom target;

    target = gtk_drag_dest_find_target (GTK_WIDGET (tree_view),
                                        context,
                                        NULL);

    if (target == GDK_NONE)
    {
        return FALSE;
    }

    gtk_drag_get_data (GTK_WIDGET (tree_view),
                       context, target, time);

    return TRUE;
}

static void
free_drag_data (CajaPlacesSidebar *sidebar)
{
    sidebar->drag_data_received = FALSE;

    if (sidebar->drag_list)
    {
        caja_drag_destroy_selection_list (sidebar->drag_list);
        sidebar->drag_list = NULL;
    }
}

static gboolean
can_accept_file_as_bookmark (CajaFile *file)
{
    return (caja_file_is_directory (file) &&
            !is_built_in_bookmark (file));
}

static gboolean
can_accept_items_as_bookmarks (const GList *items)
{
    int max;
    char *uri;
    CajaFile *file;

    /* Iterate through selection checking if item will get accepted as a bookmark.
     * If more than 100 items selected, return an over-optimistic result.
     */
    for (max = 100; items != NULL && max >= 0; items = items->next, max--)
    {
        uri = ((CajaDragSelectionItem *)items->data)->uri;
        file = caja_file_get_by_uri (uri);
        if (!can_accept_file_as_bookmark (file))
        {
            caja_file_unref (file);
            return FALSE;
        }
        caja_file_unref (file);
    }

    return TRUE;
}

static gboolean
drag_motion_callback (GtkTreeView *tree_view,
                      GdkDragContext *context,
                      int x,
                      int y,
                      unsigned int time,
                      CajaPlacesSidebar *sidebar)
{
    GtkTreePath *path;
    GtkTreeViewDropPosition pos;
    int action = 0;
    GtkTreeIter iter;
    char *uri;
    gboolean res;

    if (!sidebar->drag_data_received)
    {
        if (!get_drag_data (tree_view, context, time))
        {
            return FALSE;
        }
    }

    path = NULL;
    res = compute_drop_position (tree_view, x, y, &path, &pos, sidebar);

    if (!res) {
        goto out;
    }

    if (pos == GTK_TREE_VIEW_DROP_BEFORE ||
        pos == GTK_TREE_VIEW_DROP_AFTER )
    {
        if (sidebar->drag_data_received &&
            sidebar->drag_data_info == GTK_TREE_MODEL_ROW)
        {
            action = GDK_ACTION_MOVE;
        }
        else if (can_accept_items_as_bookmarks (sidebar->drag_list))
        {
            action = GDK_ACTION_COPY;
        }
        else
        {
            action = 0;
        }
    }
    else
    {
        if (sidebar->drag_list == NULL)
        {
            action = 0;
        }
        else
        {
            gtk_tree_model_get_iter (sidebar->filter_model,
                                     &iter, path);
            gtk_tree_model_get (GTK_TREE_MODEL (sidebar->filter_model),
                                &iter,
                                PLACES_SIDEBAR_COLUMN_URI, &uri,
                                -1);
            caja_drag_default_drop_action_for_icons (context, uri,
                    sidebar->drag_list,
                    &action);
            g_free (uri);
        }
    }

    if (action != 0) {
        gtk_tree_view_set_drag_dest_row (tree_view, path, pos);
    }

    if (path != NULL) {
        gtk_tree_path_free (path);
    }

 out:
    g_signal_stop_emission_by_name (tree_view, "drag-motion");

    if (action != 0)
    {
        gdk_drag_status (context, action, time);
    }
    else
    {
        gdk_drag_status (context, 0, time);
    }

    return TRUE;
}

static void
drag_leave_callback (GtkTreeView *tree_view,
                     GdkDragContext *context,
                     unsigned int time,
                     CajaPlacesSidebar *sidebar)
{
    free_drag_data (sidebar);
    gtk_tree_view_set_drag_dest_row (tree_view, NULL, GTK_TREE_VIEW_DROP_BEFORE);
    g_signal_stop_emission_by_name (tree_view, "drag-leave");
}

/* Parses a "text/uri-list" string and inserts its URIs as bookmarks */
static void
bookmarks_drop_uris (CajaPlacesSidebar *sidebar,
                     GtkSelectionData      *selection_data,
                     int                    position)
{
    CajaBookmark *bookmark;
    CajaFile *file;
    char *uri, *name;
    char **uris;
    int i;
    GFile *location;
    GIcon *icon;

    uris = gtk_selection_data_get_uris (selection_data);
    if (!uris)
        return;

    for (i = 0; uris[i]; i++)
    {
        uri = uris[i];
        file = caja_file_get_by_uri (uri);

        if (!can_accept_file_as_bookmark (file))
        {
            caja_file_unref (file);
            continue;
        }

        uri = caja_file_get_drop_target_uri (file);
        location = g_file_new_for_uri (uri);
        caja_file_unref (file);

        name = caja_compute_title_for_location (location);
        icon = g_themed_icon_new (CAJA_ICON_FOLDER);
        bookmark = caja_bookmark_new (location, name, TRUE, icon);

        if (!caja_bookmark_list_contains (sidebar->bookmarks, bookmark))
        {
            caja_bookmark_list_insert_item (sidebar->bookmarks, bookmark, position++);
        }

        g_object_unref (location);
        g_object_unref (bookmark);
        g_object_unref (icon);
        g_free (name);
        g_free (uri);
    }

    g_strfreev (uris);
}

static GList *
uri_list_from_selection (GList *selection)
{
    CajaDragSelectionItem *item;
    GList *ret;
    GList *l;

    ret = NULL;
    for (l = selection; l != NULL; l = l->next)
    {
        item = l->data;
        ret = g_list_prepend (ret, item->uri);
    }

    return g_list_reverse (ret);
}

static GList*
build_selection_list (const char *data)
{
    CajaDragSelectionItem *item;
    GList *result;
    char **uris;
    char *uri;
    int i;

    uris = g_uri_list_extract_uris (data);

    result = NULL;
    for (i = 0; uris[i]; i++)
    {
        uri = uris[i];
        item = caja_drag_selection_item_new ();
        item->uri = g_strdup (uri);
        item->got_icon_position = FALSE;
        result = g_list_prepend (result, item);
    }

    g_strfreev (uris);

    return g_list_reverse (result);
}

static gboolean
get_selected_iter (CajaPlacesSidebar *sidebar,
                   GtkTreeIter *iter)
{
    GtkTreeSelection *selection;

    selection = gtk_tree_view_get_selection (sidebar->tree_view);

    return gtk_tree_selection_get_selected (selection, NULL, iter);
}

/* Reorders the selected bookmark to the specified position */
static void
reorder_bookmarks (CajaPlacesSidebar *sidebar,
                   int                new_position)
{
    GtkTreeIter iter;
    PlaceType type;
    int old_position;

    /* Get the selected path */

    if (!get_selected_iter (sidebar, &iter))
        return;

    gtk_tree_model_get (GTK_TREE_MODEL (sidebar->filter_model), &iter,
                        PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
                        PLACES_SIDEBAR_COLUMN_INDEX, &old_position,
                        -1);

    if (type != PLACES_BOOKMARK ||
            old_position < 0 ||
            old_position >= caja_bookmark_list_length (sidebar->bookmarks))
    {
        return;
    }

    caja_bookmark_list_move_item (sidebar->bookmarks, old_position,
                                  new_position);
}

static void
drag_data_received_callback (GtkWidget *widget,
                             GdkDragContext *context,
                             int x,
                             int y,
                             GtkSelectionData *selection_data,
                             unsigned int info,
                             unsigned int time,
                             CajaPlacesSidebar *sidebar)
{
    GtkTreeView *tree_view;
    GtkTreePath *tree_path;
    GtkTreeViewDropPosition tree_pos;
    GtkTreeIter iter;
    int position;
    GtkTreeModel *model;
    char *drop_uri;
    GList *selection_list, *uris;
    PlaceType place_type;
    SectionType section_type;
    gboolean success;

    tree_view = GTK_TREE_VIEW (widget);

    if (!sidebar->drag_data_received)
    {
        if (gtk_selection_data_get_target (selection_data) != GDK_NONE &&
                info == TEXT_URI_LIST)
        {
            sidebar->drag_list = build_selection_list (gtk_selection_data_get_data (selection_data));
        }
        else
        {
            sidebar->drag_list = NULL;
        }
        sidebar->drag_data_received = TRUE;
        sidebar->drag_data_info = info;
    }

    g_signal_stop_emission_by_name (widget, "drag-data-received");

    if (!sidebar->drop_occured)
    {
        return;
    }

    /* Compute position */
    success = compute_drop_position (tree_view, x, y, &tree_path, &tree_pos, sidebar);
    if (!success)
        goto out;

    success = FALSE;

    if (tree_pos == GTK_TREE_VIEW_DROP_BEFORE ||
        tree_pos == GTK_TREE_VIEW_DROP_AFTER)
    {
        model = gtk_tree_view_get_model (tree_view);

        if (!gtk_tree_model_get_iter (model, &iter, tree_path))
        {
            goto out;
        }

        gtk_tree_model_get (model, &iter,
                            PLACES_SIDEBAR_COLUMN_SECTION_TYPE, &section_type,
                            PLACES_SIDEBAR_COLUMN_ROW_TYPE, &place_type,
                            PLACES_SIDEBAR_COLUMN_INDEX, &position,
                            -1);

        if (section_type != SECTION_BOOKMARKS &&
            !(section_type == SECTION_NETWORK && place_type == PLACES_HEADING)) {
            goto out;
        }

        if (section_type == SECTION_NETWORK && place_type == PLACES_HEADING &&
            tree_pos == GTK_TREE_VIEW_DROP_BEFORE) {
            position = caja_bookmark_list_length (sidebar->bookmarks);
        }

        if (tree_pos == GTK_TREE_VIEW_DROP_AFTER && place_type != PLACES_HEADING) {
            /* heading already has position 0 */
            position++;
        }

        switch (info)
        {
        case TEXT_URI_LIST:
            bookmarks_drop_uris (sidebar, selection_data, position);
            success = TRUE;
            break;
        case GTK_TREE_MODEL_ROW:
            reorder_bookmarks (sidebar, position);
            success = TRUE;
            break;
        default:
            g_assert_not_reached ();
            break;
        }
    }
    else
    {
        GdkDragAction real_action;

        /* file transfer requested */
        real_action = gdk_drag_context_get_selected_action (context);

        if (real_action == GDK_ACTION_ASK)
        {
            real_action =
                caja_drag_drop_action_ask (GTK_WIDGET (tree_view),
                                           gdk_drag_context_get_actions (context));
        }

        if (real_action > 0)
        {
            model = gtk_tree_view_get_model (tree_view);

            gtk_tree_model_get_iter (model, &iter, tree_path);
            gtk_tree_model_get (model, &iter,
                                PLACES_SIDEBAR_COLUMN_URI, &drop_uri,
                                -1);

            switch (info)
            {
            case TEXT_URI_LIST:
                selection_list = build_selection_list (gtk_selection_data_get_data (selection_data));
                uris = uri_list_from_selection (selection_list);
                caja_file_operations_copy_move (uris, NULL, drop_uri,
                                                real_action, GTK_WIDGET (tree_view),
                                                NULL, NULL);
                caja_drag_destroy_selection_list (selection_list);
                g_list_free (uris);
                success = TRUE;
                break;
            case GTK_TREE_MODEL_ROW:
                success = FALSE;
                break;
            default:
                g_assert_not_reached ();
                break;
            }

            g_free (drop_uri);
        }
    }

out:
    sidebar->drop_occured = FALSE;
    free_drag_data (sidebar);
    gtk_drag_finish (context, success, FALSE, time);

    gtk_tree_path_free (tree_path);
}

static gboolean
drag_drop_callback (GtkTreeView *tree_view,
                    GdkDragContext *context,
                    int x,
                    int y,
                    unsigned int time,
                    CajaPlacesSidebar *sidebar)
{
    gboolean retval = FALSE;
    sidebar->drop_occured = TRUE;
    retval = get_drag_data (tree_view, context, time);
    g_signal_stop_emission_by_name (tree_view, "drag-drop");
    return retval;
}

/* Callback used when the file list's popup menu is detached */
static void
bookmarks_popup_menu_detach_cb (GtkWidget *attach_widget,
                                GtkMenu   *menu)
{
    CajaPlacesSidebar *sidebar;

    sidebar = CAJA_PLACES_SIDEBAR (attach_widget);
    g_assert (CAJA_IS_PLACES_SIDEBAR (sidebar));

    sidebar->popup_menu = NULL;
    sidebar->popup_menu_remove_item = NULL;
    sidebar->popup_menu_rename_item = NULL;
    sidebar->popup_menu_separator_item = NULL;
    sidebar->popup_menu_mount_item = NULL;
    sidebar->popup_menu_unmount_item = NULL;
    sidebar->popup_menu_eject_item = NULL;
    sidebar->popup_menu_rescan_item = NULL;
    sidebar->popup_menu_format_item = NULL;
    sidebar->popup_menu_start_item = NULL;
    sidebar->popup_menu_stop_item = NULL;
    sidebar->popup_menu_empty_trash_item = NULL;
}

static void
check_unmount_and_eject (GMount *mount,
                         GVolume *volume,
                         GDrive *drive,
                         gboolean *show_unmount,
                         gboolean *show_eject)
{
    *show_unmount = FALSE;
    *show_eject = FALSE;

    if (drive != NULL)
    {
        *show_eject = g_drive_can_eject (drive);
    }

    if (volume != NULL)
    {
        *show_eject |= g_volume_can_eject (volume);
    }
    if (mount != NULL)
    {
        *show_eject |= g_mount_can_eject (mount);
        *show_unmount = g_mount_can_unmount (mount) && !*show_eject;
    }
}

static void
check_visibility (GMount           *mount,
                  GVolume          *volume,
                  GDrive           *drive,
                  gboolean         *show_mount,
                  gboolean         *show_unmount,
                  gboolean         *show_eject,
                  gboolean         *show_rescan,
                  gboolean         *show_format,
                  gboolean         *show_start,
                  gboolean         *show_stop)
{
    *show_mount = FALSE;
    *show_format = FALSE;
    *show_rescan = FALSE;
    *show_start = FALSE;
    *show_stop = FALSE;

    check_unmount_and_eject (mount, volume, drive, show_unmount, show_eject);

    if (drive != NULL)
    {
        if (g_drive_is_media_removable (drive) &&
                !g_drive_is_media_check_automatic (drive) &&
                g_drive_can_poll_for_media (drive))
            *show_rescan = TRUE;

        *show_start = g_drive_can_start (drive) || g_drive_can_start_degraded (drive);
        *show_stop  = g_drive_can_stop (drive);

        if (*show_stop)
            *show_unmount = FALSE;
    }

    if (volume != NULL)
    {
        if (mount == NULL)
            *show_mount = g_volume_can_mount (volume);
    }
}

static void
bookmarks_check_popup_sensitivity (CajaPlacesSidebar *sidebar)
{
    GtkTreeIter iter;
    PlaceType type;
    GDrive *drive = NULL;
    GVolume *volume = NULL;
    GMount *mount = NULL;
    gboolean show_mount;
    gboolean show_unmount;
    gboolean show_eject;
    gboolean show_rescan;
    gboolean show_format;
    gboolean show_start;
    gboolean show_stop;
    gboolean show_empty_trash;
    char *uri = NULL;

    type = PLACES_BUILT_IN;

    if (sidebar->popup_menu == NULL)
    {
        return;
    }

    if (get_selected_iter (sidebar, &iter))
    {
        gtk_tree_model_get (GTK_TREE_MODEL (sidebar->filter_model), &iter,
                            PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
                            PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
                            PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
                            PLACES_SIDEBAR_COLUMN_MOUNT, &mount,
                            PLACES_SIDEBAR_COLUMN_URI, &uri,
                            -1);
    }

    gtk_widget_show (sidebar->popup_menu_open_in_new_tab_item);

    gtk_widget_set_sensitive (sidebar->popup_menu_remove_item, (type == PLACES_BOOKMARK));
    gtk_widget_set_sensitive (sidebar->popup_menu_rename_item, (type == PLACES_BOOKMARK));
    gtk_widget_set_sensitive (sidebar->popup_menu_empty_trash_item, !caja_trash_monitor_is_empty ());

    check_visibility (mount, volume, drive,
                      &show_mount, &show_unmount, &show_eject, &show_rescan, &show_format, &show_start, &show_stop);

    /* We actually want both eject and unmount since eject will unmount all volumes.
     * TODO: hide unmount if the drive only has a single mountable volume
     */

    show_empty_trash = (uri != NULL) &&
                       (!strcmp (uri, "trash:///"));

    gtk_widget_set_visible (sidebar->popup_menu_separator_item,
                              show_mount || show_unmount || show_eject || show_format || show_empty_trash);
    gtk_widget_set_visible (sidebar->popup_menu_mount_item, show_mount);
    gtk_widget_set_visible (sidebar->popup_menu_unmount_item, show_unmount);
    gtk_widget_set_visible (sidebar->popup_menu_eject_item, show_eject);
    gtk_widget_set_visible (sidebar->popup_menu_rescan_item, show_rescan);
    gtk_widget_set_visible (sidebar->popup_menu_format_item, show_format);
    gtk_widget_set_visible (sidebar->popup_menu_start_item, show_start);
    gtk_widget_set_visible (sidebar->popup_menu_stop_item, show_stop);
    gtk_widget_set_visible (sidebar->popup_menu_empty_trash_item, show_empty_trash);

    /* Adjust start/stop items to reflect the type of the drive */
    gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_start_item), _("_Start"));
    gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_stop_item), _("_Stop"));
    if ((show_start || show_stop) && drive != NULL)
    {
        switch (g_drive_get_start_stop_type (drive))
        {
        case G_DRIVE_START_STOP_TYPE_SHUTDOWN:
            /* start() for type G_DRIVE_START_STOP_TYPE_SHUTDOWN is normally not used */
            gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_start_item), _("_Power On"));
            gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_stop_item), _("_Safely Remove Drive"));
            break;
        case G_DRIVE_START_STOP_TYPE_NETWORK:
            gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_start_item), _("_Connect Drive"));
            gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_stop_item), _("_Disconnect Drive"));
            break;
        case G_DRIVE_START_STOP_TYPE_MULTIDISK:
            gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_start_item), _("_Start Multi-disk Device"));
            gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_stop_item), _("_Stop Multi-disk Device"));
            break;
        case G_DRIVE_START_STOP_TYPE_PASSWORD:
            /* stop() for type G_DRIVE_START_STOP_TYPE_PASSWORD is normally not used */
            gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_start_item), _("_Unlock Drive"));
            gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_stop_item), _("_Lock Drive"));
            break;

        default:
        case G_DRIVE_START_STOP_TYPE_UNKNOWN:
            /* uses defaults set above */
            break;
        }
    }


    g_free (uri);
}

/* Callback used when the selection in the shortcuts tree changes */
static void
bookmarks_selection_changed_cb (GtkTreeSelection      *selection,
                                CajaPlacesSidebar *sidebar)
{
    bookmarks_check_popup_sensitivity (sidebar);
}

static void
volume_mounted_cb (GVolume *volume,
                   GObject *user_data)
{
    GMount *mount;
    CajaPlacesSidebar *sidebar;
    GFile *location;

    sidebar = CAJA_PLACES_SIDEBAR (user_data);

    sidebar->mounting = FALSE;

    mount = g_volume_get_mount (volume);
    if (mount != NULL)
    {
        location = g_mount_get_default_location (mount);

        if (sidebar->go_to_after_mount_slot != NULL)
        {
            if ((sidebar->go_to_after_mount_flags & CAJA_WINDOW_OPEN_FLAG_NEW_WINDOW) == 0)
            {
                caja_window_slot_info_open_location (sidebar->go_to_after_mount_slot, location,
                                                     CAJA_WINDOW_OPEN_ACCORDING_TO_MODE,
                                                     sidebar->go_to_after_mount_flags, NULL);
            }
            else
            {
                CajaWindow *cur, *new;

                cur = CAJA_WINDOW (sidebar->window);
                new = caja_application_create_navigation_window (cur->application,
#if !GTK_CHECK_VERSION (3, 0, 0)
                        NULL,
#endif
                        gtk_window_get_screen (GTK_WINDOW (cur)));
                caja_window_go_to (new, location);
            }
        }

        g_object_unref (G_OBJECT (location));
        g_object_unref (G_OBJECT (mount));
    }


    eel_remove_weak_pointer (&(sidebar->go_to_after_mount_slot));
}

static void
drive_start_from_bookmark_cb (GObject      *source_object,
                              GAsyncResult *res,
                              gpointer      user_data)
{
    GError *error;
    char *primary;
    char *name;

    error = NULL;
    if (!g_drive_poll_for_media_finish (G_DRIVE (source_object), res, &error))
    {
        if (error->code != G_IO_ERROR_FAILED_HANDLED)
        {
            name = g_drive_get_name (G_DRIVE (source_object));
            primary = g_strdup_printf (_("Unable to start %s"), name);
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
open_selected_bookmark (CajaPlacesSidebar   *sidebar,
                        GtkTreeModel        *model,
                        GtkTreePath         *path,
                        CajaWindowOpenFlags  flags)
{
    CajaWindowSlotInfo *slot;
    GtkTreeIter iter;
    GFile *location;
    char *uri;

    if (!path)
    {
        return;
    }

    if (!gtk_tree_model_get_iter (model, &iter, path))
    {
        return;
    }

    gtk_tree_model_get (model, &iter, PLACES_SIDEBAR_COLUMN_URI, &uri, -1);

    if (uri != NULL)
    {
        caja_debug_log (FALSE, CAJA_DEBUG_LOG_DOMAIN_USER,
                        "activate from places sidebar window=%p: %s",
                        sidebar->window, uri);
        location = g_file_new_for_uri (uri);
        /* Navigate to the clicked location */
        if ((flags & CAJA_WINDOW_OPEN_FLAG_NEW_WINDOW) == 0)
        {
            slot = caja_window_info_get_active_slot (sidebar->window);
            caja_window_slot_info_open_location (slot, location,
                                                 CAJA_WINDOW_OPEN_ACCORDING_TO_MODE,
                                                 flags, NULL);
        }
        else
        {
            CajaWindow *cur, *new;

            cur = CAJA_WINDOW (sidebar->window);
            new = caja_application_create_navigation_window (cur->application,
#if !GTK_CHECK_VERSION (3, 0, 0)
                    NULL,
#endif
                    gtk_window_get_screen (GTK_WINDOW (cur)));
            caja_window_go_to (new, location);
        }
        g_object_unref (location);
        g_free (uri);

    }
    else
    {
        GDrive *drive;
        GVolume *volume;
        CajaWindowSlot *slot;

        gtk_tree_model_get (model, &iter,
                            PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
                            PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
                            -1);

        if (volume != NULL && !sidebar->mounting)
        {
            sidebar->mounting = TRUE;

            g_assert (sidebar->go_to_after_mount_slot == NULL);

            slot = caja_window_info_get_active_slot (sidebar->window);
            sidebar->go_to_after_mount_slot = slot;
            eel_add_weak_pointer (&(sidebar->go_to_after_mount_slot));

            sidebar->go_to_after_mount_flags = flags;

            caja_file_operations_mount_volume_full (NULL, volume, FALSE,
                                                    volume_mounted_cb,
                                                    G_OBJECT (sidebar));
        }
        else if (volume == NULL && drive != NULL &&
                 (g_drive_can_start (drive) || g_drive_can_start_degraded (drive)))
        {
            GMountOperation *mount_op;

            mount_op = gtk_mount_operation_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (sidebar))));
            g_drive_start (drive, G_DRIVE_START_NONE, mount_op, NULL, drive_start_from_bookmark_cb, NULL);
            g_object_unref (mount_op);
        }

        if (drive != NULL)
            g_object_unref (drive);
        if (volume != NULL)
            g_object_unref (volume);
    }
}

static void
open_shortcut_from_menu (CajaPlacesSidebar   *sidebar,
                         CajaWindowOpenFlags  flags)
{
    GtkTreeModel *model;
    GtkTreePath *path;

    model = gtk_tree_view_get_model (sidebar->tree_view);
    gtk_tree_view_get_cursor (sidebar->tree_view, &path, NULL);

    open_selected_bookmark (sidebar, model, path, flags);

    gtk_tree_path_free (path);
}

static void
open_shortcut_cb (GtkMenuItem       *item,
                  CajaPlacesSidebar *sidebar)
{
    open_shortcut_from_menu (sidebar, 0);
}

static void
open_shortcut_in_new_window_cb (GtkMenuItem       *item,
                                CajaPlacesSidebar *sidebar)
{
    open_shortcut_from_menu (sidebar, CAJA_WINDOW_OPEN_FLAG_NEW_WINDOW);
}

static void
open_shortcut_in_new_tab_cb (GtkMenuItem       *item,
                             CajaPlacesSidebar *sidebar)
{
    open_shortcut_from_menu (sidebar, CAJA_WINDOW_OPEN_FLAG_NEW_TAB);
}

/* Rename the selected bookmark */
static void
rename_selected_bookmark (CajaPlacesSidebar *sidebar)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    GtkTreeViewColumn *column;
    GtkCellRenderer *cell;
    GList *renderers;

    if (get_selected_iter (sidebar, &iter))
    {
        path = gtk_tree_model_get_path (GTK_TREE_MODEL (sidebar->filter_model), &iter);
        column = gtk_tree_view_get_column (GTK_TREE_VIEW (sidebar->tree_view), 0);
        renderers = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
        cell = g_list_nth_data (renderers, 6);
        g_list_free (renderers);
        g_object_set (cell, "editable", TRUE, NULL);
        gtk_tree_view_set_cursor_on_cell (GTK_TREE_VIEW (sidebar->tree_view),
                                          path, column, cell, TRUE);
        gtk_tree_path_free (path);
    }
}

static void
rename_shortcut_cb (GtkMenuItem           *item,
                    CajaPlacesSidebar *sidebar)
{
    rename_selected_bookmark (sidebar);
}

/* Removes the selected bookmarks */
static void
remove_selected_bookmarks (CajaPlacesSidebar *sidebar)
{
    GtkTreeIter iter;
    PlaceType type;
    int index;

    if (!get_selected_iter (sidebar, &iter))
    {
        return;
    }

    gtk_tree_model_get (GTK_TREE_MODEL (sidebar->filter_model), &iter,
                        PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
                        -1);

    if (type != PLACES_BOOKMARK)
    {
        return;
    }

    gtk_tree_model_get (GTK_TREE_MODEL (sidebar->filter_model), &iter,
                        PLACES_SIDEBAR_COLUMN_INDEX, &index,
                        -1);

    caja_bookmark_list_delete_item_at (sidebar->bookmarks, index);
}

static void
remove_shortcut_cb (GtkMenuItem           *item,
                    CajaPlacesSidebar *sidebar)
{
    remove_selected_bookmarks (sidebar);
}

static void
mount_shortcut_cb (GtkMenuItem           *item,
                   CajaPlacesSidebar *sidebar)
{
    GtkTreeIter iter;
    GVolume *volume;

    if (!get_selected_iter (sidebar, &iter))
    {
        return;
    }

    gtk_tree_model_get (GTK_TREE_MODEL (sidebar->filter_model), &iter,
                        PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
                        -1);

    if (volume != NULL)
    {
        caja_file_operations_mount_volume (NULL, volume, FALSE);
        g_object_unref (volume);
    }
}

static void
unmount_done (gpointer data)
{
    CajaWindow *window;

    window = data;
    caja_window_info_set_initiated_unmount (window, FALSE);
    g_object_unref (window);
}

static void
do_unmount (GMount *mount,
            CajaPlacesSidebar *sidebar)
{
    if (mount != NULL)
    {
        caja_window_info_set_initiated_unmount (sidebar->window, TRUE);
        caja_file_operations_unmount_mount_full (NULL, mount, FALSE, TRUE,
                unmount_done,
                g_object_ref (sidebar->window));
    }
}

static void
do_unmount_selection (CajaPlacesSidebar *sidebar)
{
    GtkTreeIter iter;
    GMount *mount;

    if (!get_selected_iter (sidebar, &iter))
    {
        return;
    }

    gtk_tree_model_get (GTK_TREE_MODEL (sidebar->filter_model), &iter,
                        PLACES_SIDEBAR_COLUMN_MOUNT, &mount,
                        -1);

    if (mount != NULL)
    {
        do_unmount (mount, sidebar);
        g_object_unref (mount);
    }
}

static void
unmount_shortcut_cb (GtkMenuItem           *item,
                     CajaPlacesSidebar *sidebar)
{
    do_unmount_selection (sidebar);
}

static void
drive_eject_cb (GObject *source_object,
                GAsyncResult *res,
                gpointer user_data)
{
    CajaWindow *window;
    GError *error;
    char *primary;
    char *name;

    window = user_data;
    caja_window_info_set_initiated_unmount (window, FALSE);
    g_object_unref (window);

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
volume_eject_cb (GObject *source_object,
                 GAsyncResult *res,
                 gpointer user_data)
{
    CajaWindow *window;
    GError *error;
    char *primary;
    char *name;

    window = user_data;
    caja_window_info_set_initiated_unmount (window, FALSE);
    g_object_unref (window);

    error = NULL;
    if (!g_volume_eject_with_operation_finish (G_VOLUME (source_object), res, &error))
    {
        if (error->code != G_IO_ERROR_FAILED_HANDLED)
        {
            name = g_volume_get_name (G_VOLUME (source_object));
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
mount_eject_cb (GObject *source_object,
                GAsyncResult *res,
                gpointer user_data)
{
    CajaWindow *window;
    GError *error;
    char *primary;
    char *name;

    window = user_data;
    caja_window_info_set_initiated_unmount (window, FALSE);
    g_object_unref (window);

    error = NULL;
    if (!g_mount_eject_with_operation_finish (G_MOUNT (source_object), res, &error))
    {
        if (error->code != G_IO_ERROR_FAILED_HANDLED)
        {
            name = g_mount_get_name (G_MOUNT (source_object));
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
do_eject (GMount *mount,
          GVolume *volume,
          GDrive *drive,
          CajaPlacesSidebar *sidebar)
{
    GMountOperation *mount_op;

    mount_op = gtk_mount_operation_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (sidebar))));
    if (mount != NULL)
    {
        caja_window_info_set_initiated_unmount (sidebar->window, TRUE);
        g_mount_eject_with_operation (mount, 0, mount_op, NULL, mount_eject_cb,
                                      g_object_ref (sidebar->window));
    }
    else if (volume != NULL)
    {
        caja_window_info_set_initiated_unmount (sidebar->window, TRUE);
        g_volume_eject_with_operation (volume, 0, mount_op, NULL, volume_eject_cb,
                                       g_object_ref (sidebar->window));
    }
    else if (drive != NULL)
    {
        caja_window_info_set_initiated_unmount (sidebar->window, TRUE);
        g_drive_eject_with_operation (drive, 0, mount_op, NULL, drive_eject_cb,
                                      g_object_ref (sidebar->window));
    }
    g_object_unref (mount_op);
}

static void
eject_shortcut_cb (GtkMenuItem           *item,
                   CajaPlacesSidebar *sidebar)
{
    GtkTreeIter iter;
    GMount *mount;
    GVolume *volume;
    GDrive *drive;

    if (!get_selected_iter (sidebar, &iter))
    {
        return;
    }

    gtk_tree_model_get (GTK_TREE_MODEL (sidebar->filter_model), &iter,
                        PLACES_SIDEBAR_COLUMN_MOUNT, &mount,
                        PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
                        PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
                        -1);

    do_eject (mount, volume, drive, sidebar);
}

static gboolean
eject_or_unmount_bookmark (CajaPlacesSidebar *sidebar,
                           GtkTreePath *path)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    gboolean can_unmount, can_eject;
    GMount *mount;
    GVolume *volume;
    GDrive *drive;
    gboolean ret;

    model = GTK_TREE_MODEL (sidebar->filter_model);

    if (!path)
    {
        return FALSE;
    }
    if (!gtk_tree_model_get_iter (model, &iter, path))
    {
        return FALSE;
    }

    gtk_tree_model_get (model, &iter,
                        PLACES_SIDEBAR_COLUMN_MOUNT, &mount,
                        PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
                        PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
                        -1);

    ret = FALSE;

    check_unmount_and_eject (mount, volume, drive, &can_unmount, &can_eject);
    /* if we can eject, it has priority over unmount */
    if (can_eject)
    {
        do_eject (mount, volume, drive, sidebar);
        ret = TRUE;
    }
    else if (can_unmount)
    {
        do_unmount (mount, sidebar);
        ret = TRUE;
    }

    if (mount != NULL)
        g_object_unref (mount);
    if (volume != NULL)
        g_object_unref (volume);
    if (drive != NULL)
        g_object_unref (drive);

    return ret;
}

static gboolean
eject_or_unmount_selection (CajaPlacesSidebar *sidebar)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    gboolean ret;

    if (!get_selected_iter (sidebar, &iter)) {
        return FALSE;
    }

    path = gtk_tree_model_get_path (GTK_TREE_MODEL (sidebar->filter_model), &iter);
    if (path == NULL) {
        return FALSE;
    }

    ret = eject_or_unmount_bookmark (sidebar, path);

    gtk_tree_path_free (path);

    return ret;
}

static void
drive_poll_for_media_cb (GObject *source_object,
                         GAsyncResult *res,
                         gpointer user_data)
{
    GError *error;
    char *primary;
    char *name;

    error = NULL;
    if (!g_drive_poll_for_media_finish (G_DRIVE (source_object), res, &error))
    {
        if (error->code != G_IO_ERROR_FAILED_HANDLED)
        {
            name = g_drive_get_name (G_DRIVE (source_object));
            primary = g_strdup_printf (_("Unable to poll %s for media changes"), name);
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
rescan_shortcut_cb (GtkMenuItem           *item,
                    CajaPlacesSidebar *sidebar)
{
    GtkTreeIter iter;
    GDrive  *drive;

    if (!get_selected_iter (sidebar, &iter))
    {
        return;
    }

    gtk_tree_model_get (GTK_TREE_MODEL (sidebar->filter_model), &iter,
                        PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
                        -1);

    if (drive != NULL)
    {
        g_drive_poll_for_media (drive, NULL, drive_poll_for_media_cb, NULL);
    }
    g_object_unref (drive);
}

static void
format_shortcut_cb (GtkMenuItem           *item,
                    CajaPlacesSidebar *sidebar)
{
    g_spawn_command_line_async ("gfloppy", NULL);
}

static void
drive_start_cb (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
    GError *error;
    char *primary;
    char *name;

    error = NULL;
    if (!g_drive_poll_for_media_finish (G_DRIVE (source_object), res, &error))
    {
        if (error->code != G_IO_ERROR_FAILED_HANDLED)
        {
            name = g_drive_get_name (G_DRIVE (source_object));
            primary = g_strdup_printf (_("Unable to start %s"), name);
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
start_shortcut_cb (GtkMenuItem           *item,
                   CajaPlacesSidebar *sidebar)
{
    GtkTreeIter iter;
    GDrive  *drive;

    if (!get_selected_iter (sidebar, &iter))
    {
        return;
    }

    gtk_tree_model_get (GTK_TREE_MODEL (sidebar->filter_model), &iter,
                        PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
                        -1);

    if (drive != NULL)
    {
        GMountOperation *mount_op;

        mount_op = gtk_mount_operation_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (sidebar))));

        g_drive_start (drive, G_DRIVE_START_NONE, mount_op, NULL, drive_start_cb, NULL);

        g_object_unref (mount_op);
    }
    g_object_unref (drive);
}

static void
drive_stop_cb (GObject *source_object,
               GAsyncResult *res,
               gpointer user_data)
{
    CajaWindow *window;
    GError *error;
    char *primary;
    char *name;

    window = user_data;
    caja_window_info_set_initiated_unmount (window, FALSE);
    g_object_unref (window);

    error = NULL;
    if (!g_drive_poll_for_media_finish (G_DRIVE (source_object), res, &error))
    {
        if (error->code != G_IO_ERROR_FAILED_HANDLED)
        {
            name = g_drive_get_name (G_DRIVE (source_object));
            primary = g_strdup_printf (_("Unable to stop %s"), name);
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
stop_shortcut_cb (GtkMenuItem           *item,
                  CajaPlacesSidebar *sidebar)
{
    GtkTreeIter iter;
    GDrive  *drive;

    if (!get_selected_iter (sidebar, &iter))
    {
        return;
    }

    gtk_tree_model_get (GTK_TREE_MODEL (sidebar->filter_model), &iter,
                        PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
                        -1);

    if (drive != NULL)
    {
        GMountOperation *mount_op;

        mount_op = gtk_mount_operation_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (sidebar))));
        caja_window_info_set_initiated_unmount (sidebar->window, TRUE);
        g_drive_stop (drive, G_MOUNT_UNMOUNT_NONE, mount_op, NULL, drive_stop_cb,
                      g_object_ref (sidebar->window));
        g_object_unref (mount_op);
    }
    g_object_unref (drive);
}

static void
empty_trash_cb (GtkMenuItem           *item,
                CajaPlacesSidebar *sidebar)
{
    caja_file_operations_empty_trash (GTK_WIDGET (sidebar->window));
}

/* Handler for GtkWidget::key-press-event on the shortcuts list */
static gboolean
bookmarks_key_press_event_cb (GtkWidget             *widget,
                              GdkEventKey           *event,
                              CajaPlacesSidebar *sidebar)
{
    guint modifiers;
    GtkTreeModel *model;
    GtkTreePath *path;
    CajaWindowOpenFlags flags = 0;

    modifiers = gtk_accelerator_get_default_mod_mask ();

    if (event->keyval == GDK_KEY_Return ||
        event->keyval == GDK_KEY_KP_Enter ||
        event->keyval == GDK_KEY_ISO_Enter ||
        event->keyval == GDK_KEY_space)
    {
        if ((event->state & modifiers) == GDK_SHIFT_MASK)
            flags = CAJA_WINDOW_OPEN_FLAG_NEW_TAB;
        else if ((event->state & modifiers) == GDK_CONTROL_MASK)
            flags = CAJA_WINDOW_OPEN_FLAG_NEW_WINDOW;

        model = gtk_tree_view_get_model(sidebar->tree_view);
        gtk_tree_view_get_cursor(sidebar->tree_view, &path, NULL);

        open_selected_bookmark(sidebar, model, path, flags);

        gtk_tree_path_free(path);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_Down &&
            (event->state & modifiers) == GDK_MOD1_MASK)
    {
        return eject_or_unmount_selection (sidebar);
    }

    if ((event->keyval == GDK_KEY_Delete
            || event->keyval == GDK_KEY_KP_Delete)
            && (event->state & modifiers) == 0)
    {
        remove_selected_bookmarks (sidebar);
        return TRUE;
    }

    if ((event->keyval == GDK_KEY_F2)
            && (event->state & modifiers) == 0)
    {
        rename_selected_bookmark (sidebar);
        return TRUE;
    }

    return FALSE;
}

/* Constructs the popup menu for the file list if needed */
static void
bookmarks_build_popup_menu (CajaPlacesSidebar *sidebar)
{
    GtkWidget *item;

    if (sidebar->popup_menu)
    {
        return;
    }

    sidebar->popup_menu = gtk_menu_new ();
    gtk_menu_attach_to_widget (GTK_MENU (sidebar->popup_menu),
                               GTK_WIDGET (sidebar),
                               bookmarks_popup_menu_detach_cb);

    item = gtk_image_menu_item_new_with_mnemonic (_("_Open"));
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
                                   gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU));
    g_signal_connect (item, "activate",
                      G_CALLBACK (open_shortcut_cb), sidebar);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

    item = gtk_menu_item_new_with_mnemonic (_("Open in New _Tab"));
    sidebar->popup_menu_open_in_new_tab_item = item;
    g_signal_connect (item, "activate",
                      G_CALLBACK (open_shortcut_in_new_tab_cb), sidebar);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

    item = gtk_menu_item_new_with_mnemonic (_("Open in New _Window"));
    g_signal_connect (item, "activate",
                      G_CALLBACK (open_shortcut_in_new_window_cb), sidebar);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

    eel_gtk_menu_append_separator (GTK_MENU (sidebar->popup_menu));

    item = gtk_image_menu_item_new_with_label (_("Remove"));
    sidebar->popup_menu_remove_item = item;
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
                                   gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));
    g_signal_connect (item, "activate",
                      G_CALLBACK (remove_shortcut_cb), sidebar);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

    item = gtk_menu_item_new_with_label (_("Rename..."));
    sidebar->popup_menu_rename_item = item;
    g_signal_connect (item, "activate",
                      G_CALLBACK (rename_shortcut_cb), sidebar);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

    /* Mount/Unmount/Eject menu items */

    sidebar->popup_menu_separator_item =
        GTK_WIDGET (eel_gtk_menu_append_separator (GTK_MENU (sidebar->popup_menu)));

    item = gtk_menu_item_new_with_mnemonic (_("_Mount"));
    sidebar->popup_menu_mount_item = item;
    g_signal_connect (item, "activate",
                      G_CALLBACK (mount_shortcut_cb), sidebar);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

    item = gtk_menu_item_new_with_mnemonic (_("_Unmount"));
    sidebar->popup_menu_unmount_item = item;
    g_signal_connect (item, "activate",
                      G_CALLBACK (unmount_shortcut_cb), sidebar);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

    item = gtk_menu_item_new_with_mnemonic (_("_Eject"));
    sidebar->popup_menu_eject_item = item;
    g_signal_connect (item, "activate",
                      G_CALLBACK (eject_shortcut_cb), sidebar);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

    item = gtk_menu_item_new_with_mnemonic (_("_Detect Media"));
    sidebar->popup_menu_rescan_item = item;
    g_signal_connect (item, "activate",
                      G_CALLBACK (rescan_shortcut_cb), sidebar);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

    item = gtk_menu_item_new_with_mnemonic (_("_Format"));
    sidebar->popup_menu_format_item = item;
    g_signal_connect (item, "activate",
                      G_CALLBACK (format_shortcut_cb), sidebar);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

    item = gtk_menu_item_new_with_mnemonic (_("_Start"));
    sidebar->popup_menu_start_item = item;
    g_signal_connect (item, "activate",
                      G_CALLBACK (start_shortcut_cb), sidebar);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

    item = gtk_menu_item_new_with_mnemonic (_("_Stop"));
    sidebar->popup_menu_stop_item = item;
    g_signal_connect (item, "activate",
                      G_CALLBACK (stop_shortcut_cb), sidebar);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

    /* Empty Trash menu item */

    item = gtk_menu_item_new_with_mnemonic (_("Empty _Trash"));
    sidebar->popup_menu_empty_trash_item = item;
    g_signal_connect (item, "activate",
                      G_CALLBACK (empty_trash_cb), sidebar);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

    bookmarks_check_popup_sensitivity (sidebar);
}

static void
bookmarks_update_popup_menu (CajaPlacesSidebar *sidebar)
{
    bookmarks_build_popup_menu (sidebar);
}

static void
bookmarks_popup_menu (CajaPlacesSidebar *sidebar,
                      GdkEventButton        *event)
{
    bookmarks_update_popup_menu (sidebar);
    eel_pop_up_context_menu (GTK_MENU(sidebar->popup_menu),
                             EEL_DEFAULT_POPUP_MENU_DISPLACEMENT,
                             EEL_DEFAULT_POPUP_MENU_DISPLACEMENT,
                             event);
}

/* Callback used for the GtkWidget::popup-menu signal of the shortcuts list */
static gboolean
bookmarks_popup_menu_cb (GtkWidget *widget,
                         CajaPlacesSidebar *sidebar)
{
    bookmarks_popup_menu (sidebar, NULL);
    return TRUE;
}

static gboolean
bookmarks_button_release_event_cb (GtkWidget *widget,
                                   GdkEventButton *event,
                                   CajaPlacesSidebar *sidebar)
{
    GtkTreePath *path;
    GtkTreeModel *model;
    GtkTreeView *tree_view;

    path = NULL;

    if (event->type != GDK_BUTTON_RELEASE)
    {
        return TRUE;
    }

    if (clicked_eject_button (sidebar, &path))
    {
        eject_or_unmount_bookmark (sidebar, path);
        gtk_tree_path_free (path);
        return FALSE;
    }

    tree_view = GTK_TREE_VIEW (widget);
    model = gtk_tree_view_get_model (tree_view);

    if (event->button == 1)
    {

        if (event->window != gtk_tree_view_get_bin_window (tree_view))
        {
            return FALSE;
        }

        gtk_tree_view_get_path_at_pos (tree_view, (int) event->x, (int) event->y,
                                       &path, NULL, NULL, NULL);

        open_selected_bookmark (sidebar, model, path, 0);

        gtk_tree_path_free (path);
    }

    return FALSE;
}

static void
update_eject_buttons (CajaPlacesSidebar *sidebar,
                      GtkTreePath         *path)
{
    GtkTreeIter iter;
    gboolean icon_visible, path_same;

    icon_visible = TRUE;

    if (path == NULL && sidebar->eject_highlight_path == NULL) {
        /* Both are null - highlight up to date */
        return;
    }

    path_same = (path != NULL) &&
        (sidebar->eject_highlight_path != NULL) &&
        (gtk_tree_path_compare (sidebar->eject_highlight_path, path) == 0);

    if (path_same) {
        /* Same path - highlight up to date */
        return;
    }

    if (path) {
        gtk_tree_model_get_iter (GTK_TREE_MODEL (sidebar->filter_model),
                     &iter,
                     path);

        gtk_tree_model_get (GTK_TREE_MODEL (sidebar->filter_model),
                    &iter,
                    PLACES_SIDEBAR_COLUMN_EJECT, &icon_visible,
                    -1);
    }

    if (!icon_visible || path == NULL || !path_same) {
        /* remove highlighting and reset the saved path, as we are leaving
         * an eject button area.
         */
        if (sidebar->eject_highlight_path) {
            gtk_tree_model_get_iter (GTK_TREE_MODEL (sidebar->store),
                         &iter,
                         sidebar->eject_highlight_path);

            gtk_list_store_set (sidebar->store,
                        &iter,
                        PLACES_SIDEBAR_COLUMN_EJECT_ICON, get_eject_icon (FALSE),
                        -1);
            gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (sidebar->filter_model));

            gtk_tree_path_free (sidebar->eject_highlight_path);
            sidebar->eject_highlight_path = NULL;
        }

        if (!icon_visible) {
            return;
        }
    }

    if (path != NULL) {
        /* add highlighting to the selected path, as the icon is visible and
         * we're hovering it.
         */
        gtk_tree_model_get_iter (GTK_TREE_MODEL (sidebar->store),
                     &iter,
                     path);
        gtk_list_store_set (sidebar->store,
                    &iter,
                    PLACES_SIDEBAR_COLUMN_EJECT_ICON, get_eject_icon (TRUE),
                    -1);
        gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (sidebar->filter_model));

        sidebar->eject_highlight_path = gtk_tree_path_copy (path);
    }
}

static gboolean
bookmarks_motion_event_cb (GtkWidget             *widget,
                           GdkEventMotion        *event,
                           CajaPlacesSidebar *sidebar)
{
    GtkTreePath *path;
    GtkTreeModel *model;

    model = GTK_TREE_MODEL (sidebar->filter_model);
    path = NULL;

    if (over_eject_button (sidebar, event->x, event->y, &path)) {
        update_eject_buttons (sidebar, path);
        gtk_tree_path_free (path);

        return TRUE;
    }

    update_eject_buttons (sidebar, NULL);

    return FALSE;
}

/* Callback used when a button is pressed on the shortcuts list.
 * We trap button 3 to bring up a popup menu, and button 2 to
 * open in a new tab.
 */
static gboolean
bookmarks_button_press_event_cb (GtkWidget             *widget,
                                 GdkEventButton        *event,
                                 CajaPlacesSidebar *sidebar)
{
    if (event->type != GDK_BUTTON_PRESS)
    {
        /* ignore multiple clicks */
        return TRUE;
    }

    if (event->button == 3)
    {
        bookmarks_popup_menu (sidebar, event);
    }
    else if (event->button == 2)
    {
        GtkTreeModel *model;
        GtkTreePath *path;
        GtkTreeView *tree_view;

        tree_view = GTK_TREE_VIEW (widget);
        g_assert (tree_view == sidebar->tree_view);

        model = gtk_tree_view_get_model (tree_view);

        gtk_tree_view_get_path_at_pos (tree_view, (int) event->x, (int) event->y,
                                       &path, NULL, NULL, NULL);

        open_selected_bookmark (sidebar, model, path,
                                event->state & GDK_CONTROL_MASK ?
                                CAJA_WINDOW_OPEN_FLAG_NEW_WINDOW :
                                CAJA_WINDOW_OPEN_FLAG_NEW_TAB);

        if (path != NULL)
        {
            gtk_tree_path_free (path);
            return TRUE;
        }
    }

    return FALSE;
}


static void
bookmarks_edited (GtkCellRenderer       *cell,
                  gchar                 *path_string,
                  gchar                 *new_text,
                  CajaPlacesSidebar *sidebar)
{
    GtkTreePath *path;
    GtkTreeIter iter;
    CajaBookmark *bookmark;
    int index;

    g_object_set (cell, "editable", FALSE, NULL);

    path = gtk_tree_path_new_from_string (path_string);
    gtk_tree_model_get_iter (GTK_TREE_MODEL (sidebar->filter_model), &iter, path);
    gtk_tree_model_get (GTK_TREE_MODEL (sidebar->filter_model), &iter,
                        PLACES_SIDEBAR_COLUMN_INDEX, &index,
                        -1);
    gtk_tree_path_free (path);
    bookmark = caja_bookmark_list_item_at (sidebar->bookmarks, index);

    if (bookmark != NULL)
    {
        caja_bookmark_set_name (bookmark, new_text);
    }
}

static void
bookmarks_editing_canceled (GtkCellRenderer       *cell,
                            CajaPlacesSidebar *sidebar)
{
    g_object_set (cell, "editable", FALSE, NULL);
}

static void
trash_state_changed_cb (CajaTrashMonitor    *trash_monitor,
                        gboolean             state,
                        gpointer             data)
{
    CajaPlacesSidebar *sidebar;

    sidebar = CAJA_PLACES_SIDEBAR (data);

    /* The trash icon changed, update the sidebar */
    update_places (sidebar);

    bookmarks_check_popup_sensitivity (sidebar);
}

static gboolean
tree_selection_func (GtkTreeSelection *selection,
                     GtkTreeModel *model,
                     GtkTreePath *path,
                     gboolean path_currently_selected,
                     gpointer user_data)
{
    GtkTreeIter iter;
    PlaceType row_type;

    gtk_tree_model_get_iter (model, &iter, path);
    gtk_tree_model_get (model, &iter,
                PLACES_SIDEBAR_COLUMN_ROW_TYPE, &row_type,
                -1);

    if (row_type == PLACES_HEADING) {
        return FALSE;
    }

    return TRUE;
}

static void
icon_cell_renderer_func (GtkTreeViewColumn *column,
                         GtkCellRenderer *cell,
                         GtkTreeModel *model,
                         GtkTreeIter *iter,
                         gpointer user_data)
{
    CajaPlacesSidebar *sidebar;
    PlaceType type;

    sidebar = user_data;

    gtk_tree_model_get (model, iter,
                PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
                -1);

    if (type == PLACES_HEADING) {
        g_object_set (cell,
                  "visible", FALSE,
                  NULL);
    } else {
        g_object_set (cell,
                  "visible", TRUE,
                  NULL);
    }
}

static void
padding_cell_renderer_func (GtkTreeViewColumn *column,
                            GtkCellRenderer *cell,
                            GtkTreeModel *model,
                            GtkTreeIter *iter,
                            gpointer user_data)
{
    PlaceType type;

    gtk_tree_model_get (model, iter,
                        PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
                        -1);

    if (type == PLACES_HEADING) {
        g_object_set (cell,
                      "visible", FALSE,
                      "xpad", 0,
                      "ypad", 0,
                      NULL);
    } else {
        g_object_set (cell,
                      "visible", TRUE,
                      "xpad", 3,
                      "ypad", 0,
                      NULL);
    }
}

static void
heading_cell_renderer_func (GtkTreeViewColumn *column,
                        GtkCellRenderer *cell,
                        GtkTreeModel *model,
                        GtkTreeIter *iter,
                        gpointer user_data)
{
    PlaceType type;

    gtk_tree_model_get (model, iter,
                        PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
                        -1);

    if (type == PLACES_HEADING) {
        g_object_set (cell,
                      "visible", TRUE,
                      NULL);
    } else {
        g_object_set (cell,
                      "visible", FALSE,
                      NULL);
    }
}

static void
caja_places_sidebar_init (CajaPlacesSidebar *sidebar)
{
    GtkTreeView       *tree_view;
    GtkTreeViewColumn *col;
    GtkCellRenderer   *cell;
    GtkTreeSelection  *selection;

    sidebar->volume_monitor = g_volume_monitor_get ();

    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sidebar),
                                    GTK_POLICY_NEVER,
                                    GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (sidebar), NULL);
    gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (sidebar), NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sidebar), GTK_SHADOW_IN);

    /* tree view */
    tree_view = GTK_TREE_VIEW (gtk_tree_view_new ());
    gtk_tree_view_set_headers_visible (tree_view, FALSE);

    col = GTK_TREE_VIEW_COLUMN (gtk_tree_view_column_new ());

    /* initial padding */
    cell = gtk_cell_renderer_text_new ();
    sidebar->padding_cell_renderer = cell;
    gtk_tree_view_column_pack_start (col, cell, FALSE);
    g_object_set (cell,
                  "xpad", 6,
                  NULL);

    /* headings */
    cell = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (col, cell, FALSE);
    gtk_tree_view_column_set_attributes (col, cell,
                                         "text", PLACES_SIDEBAR_COLUMN_HEADING_TEXT,
                                         NULL);
    g_object_set (cell,
                  "weight", PANGO_WEIGHT_BOLD,
                  "weight-set", TRUE,
                  "ypad", 1,
                  "xpad", 0,
                  NULL);
    gtk_tree_view_column_set_cell_data_func (col, cell,
                         heading_cell_renderer_func,
                         sidebar, NULL);

    /* icon padding */
    cell = gtk_cell_renderer_text_new ();
    sidebar->icon_padding_cell_renderer = cell;
    gtk_tree_view_column_pack_start (col, cell, FALSE);
    gtk_tree_view_column_set_cell_data_func (col, cell,
                                             padding_cell_renderer_func,
                                             sidebar, NULL);

    /* icon renderer */
    cell = gtk_cell_renderer_pixbuf_new ();
    sidebar->icon_cell_renderer = cell;
    gtk_tree_view_column_pack_start (col, cell, FALSE);
    gtk_tree_view_column_set_attributes (col, cell,
                                         "pixbuf", PLACES_SIDEBAR_COLUMN_ICON,
                                         NULL);
    gtk_tree_view_column_set_cell_data_func (col, cell,
                                             icon_cell_renderer_func,
                                             sidebar, NULL);

    /* eject text renderer */
    cell = gtk_cell_renderer_text_new ();
    sidebar->eject_text_cell_renderer = cell;
    gtk_tree_view_column_pack_start (col, cell, TRUE);
    gtk_tree_view_column_set_attributes (col, cell,
                                         "text", PLACES_SIDEBAR_COLUMN_NAME,
                                         "visible", PLACES_SIDEBAR_COLUMN_EJECT,
                                         NULL);
    g_object_set (cell,
                  "ellipsize", PANGO_ELLIPSIZE_END,
                  "ellipsize-set", TRUE,
                  NULL);

    /* eject icon renderer */
    cell = gtk_cell_renderer_pixbuf_new ();
    g_object_set (cell,
                  "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
                  "stock-size", GTK_ICON_SIZE_MENU,
                  "xpad", EJECT_BUTTON_XPAD,
                  NULL);
    gtk_tree_view_column_pack_start (col, cell, FALSE);
    gtk_tree_view_column_set_attributes (col, cell,
                                         "visible", PLACES_SIDEBAR_COLUMN_EJECT,
                                         "pixbuf", PLACES_SIDEBAR_COLUMN_EJECT_ICON,
                                         NULL);

    /* normal text renderer */
    cell = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (col, cell, TRUE);
    g_object_set (G_OBJECT (cell), "editable", FALSE, NULL);
    gtk_tree_view_column_set_attributes (col, cell,
                                         "text", PLACES_SIDEBAR_COLUMN_NAME,
                                         "visible", PLACES_SIDEBAR_COLUMN_NO_EJECT,
                                         "editable-set", PLACES_SIDEBAR_COLUMN_BOOKMARK,
                                         NULL);
    g_object_set (cell,
                  "ellipsize", PANGO_ELLIPSIZE_END,
                  "ellipsize-set", TRUE,
                  NULL);

    g_signal_connect (cell, "edited",
                      G_CALLBACK (bookmarks_edited), sidebar);
    g_signal_connect (cell, "editing-canceled",
                      G_CALLBACK (bookmarks_editing_canceled), sidebar);

    /* this is required to align the eject buttons to the right */
    gtk_tree_view_column_set_max_width (GTK_TREE_VIEW_COLUMN (col), CAJA_ICON_SIZE_SMALLER);
    gtk_tree_view_append_column (tree_view, col);

    sidebar->store = gtk_list_store_new (PLACES_SIDEBAR_COLUMN_COUNT,
                                         G_TYPE_INT,
                                         G_TYPE_STRING,
                                         G_TYPE_DRIVE,
                                         G_TYPE_VOLUME,
                                         G_TYPE_MOUNT,
                                         G_TYPE_STRING,
                                         GDK_TYPE_PIXBUF,
                                         G_TYPE_INT,
                                         G_TYPE_BOOLEAN,
                                         G_TYPE_BOOLEAN,
                                         G_TYPE_BOOLEAN,
                                         G_TYPE_STRING,
                                         GDK_TYPE_PIXBUF,
                                         G_TYPE_INT,
                                         G_TYPE_STRING);

    gtk_tree_view_set_tooltip_column (tree_view, PLACES_SIDEBAR_COLUMN_TOOLTIP);

    sidebar->filter_model = caja_shortcuts_model_filter_new (sidebar,
                            GTK_TREE_MODEL (sidebar->store),
                            NULL);

    gtk_tree_view_set_model (tree_view, sidebar->filter_model);
    gtk_container_add (GTK_CONTAINER (sidebar), GTK_WIDGET (tree_view));
    gtk_widget_show (GTK_WIDGET (tree_view));

    gtk_widget_show (GTK_WIDGET (sidebar));
    sidebar->tree_view = tree_view;

    gtk_tree_view_set_search_column (tree_view, PLACES_SIDEBAR_COLUMN_NAME);
    selection = gtk_tree_view_get_selection (tree_view);
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

    gtk_tree_selection_set_select_function (selection,
                                            tree_selection_func,
                                            sidebar,
                                            NULL);

    gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (tree_view),
                                            GDK_BUTTON1_MASK,
                                            caja_shortcuts_source_targets,
                                            G_N_ELEMENTS (caja_shortcuts_source_targets),
                                            GDK_ACTION_MOVE);
    gtk_drag_dest_set (GTK_WIDGET (tree_view),
                       0,
                       caja_shortcuts_drop_targets, G_N_ELEMENTS (caja_shortcuts_drop_targets),
                       GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK);

    g_signal_connect (tree_view, "key-press-event",
                      G_CALLBACK (bookmarks_key_press_event_cb), sidebar);

    g_signal_connect (tree_view, "drag-motion",
                      G_CALLBACK (drag_motion_callback), sidebar);
    g_signal_connect (tree_view, "drag-leave",
                      G_CALLBACK (drag_leave_callback), sidebar);
    g_signal_connect (tree_view, "drag-data-received",
                      G_CALLBACK (drag_data_received_callback), sidebar);
    g_signal_connect (tree_view, "drag-drop",
                      G_CALLBACK (drag_drop_callback), sidebar);

    g_signal_connect (selection, "changed",
                      G_CALLBACK (bookmarks_selection_changed_cb), sidebar);
    g_signal_connect (tree_view, "popup-menu",
                      G_CALLBACK (bookmarks_popup_menu_cb), sidebar);
    g_signal_connect (tree_view, "button-press-event",
                      G_CALLBACK (bookmarks_button_press_event_cb), sidebar);
    g_signal_connect (tree_view, "motion-notify-event",
                      G_CALLBACK (bookmarks_motion_event_cb), sidebar);
    g_signal_connect (tree_view, "button-release-event",
                      G_CALLBACK (bookmarks_button_release_event_cb), sidebar);

    eel_gtk_tree_view_set_activate_on_single_click (sidebar->tree_view,
            TRUE);

    g_signal_connect_swapped (caja_preferences, "changed::" CAJA_PREFERENCES_DESKTOP_IS_HOME_DIR,
                              G_CALLBACK(desktop_location_changed_callback),
                              sidebar);

    g_signal_connect_object (caja_trash_monitor_get (),
                             "trash_state_changed",
                             G_CALLBACK (trash_state_changed_cb),
                             sidebar, 0);
}

static void
caja_places_sidebar_dispose (GObject *object)
{
    CajaPlacesSidebar *sidebar;

    sidebar = CAJA_PLACES_SIDEBAR (object);

    sidebar->window = NULL;
    sidebar->tree_view = NULL;

    g_free (sidebar->uri);
    sidebar->uri = NULL;

    free_drag_data (sidebar);

    if (sidebar->eject_highlight_path != NULL) {
        gtk_tree_path_free (sidebar->eject_highlight_path);
        sidebar->eject_highlight_path = NULL;
    }

    g_clear_object (&sidebar->store);
    g_clear_object (&sidebar->volume_monitor);
    g_clear_object (&sidebar->bookmarks);
    g_clear_object (&sidebar->filter_model);

    eel_remove_weak_pointer (&(sidebar->go_to_after_mount_slot));

    g_signal_handlers_disconnect_by_func (caja_preferences,
                                          desktop_location_changed_callback,
                                          sidebar);

    G_OBJECT_CLASS (caja_places_sidebar_parent_class)->dispose (object);
}

static void
caja_places_sidebar_class_init (CajaPlacesSidebarClass *class)
{
    G_OBJECT_CLASS (class)->dispose = caja_places_sidebar_dispose;

#if GTK_CHECK_VERSION (3, 0, 0)
    GTK_WIDGET_CLASS (class)->style_updated = caja_places_sidebar_style_updated;
#else
    GTK_WIDGET_CLASS (class)->style_set = caja_places_sidebar_style_set;
#endif
}

static const char *
caja_places_sidebar_get_sidebar_id (CajaSidebar *sidebar)
{
    return CAJA_PLACES_SIDEBAR_ID;
}

static char *
caja_places_sidebar_get_tab_label (CajaSidebar *sidebar)
{
    return g_strdup (_("Places"));
}

static char *
caja_places_sidebar_get_tab_tooltip (CajaSidebar *sidebar)
{
    return g_strdup (_("Show Places"));
}

static GdkPixbuf *
caja_places_sidebar_get_tab_icon (CajaSidebar *sidebar)
{
    return NULL;
}

static void
caja_places_sidebar_is_visible_changed (CajaSidebar *sidebar,
                                        gboolean         is_visible)
{
    /* Do nothing */
}

static void
caja_places_sidebar_iface_init (CajaSidebarIface *iface)
{
    iface->get_sidebar_id = caja_places_sidebar_get_sidebar_id;
    iface->get_tab_label = caja_places_sidebar_get_tab_label;
    iface->get_tab_tooltip = caja_places_sidebar_get_tab_tooltip;
    iface->get_tab_icon = caja_places_sidebar_get_tab_icon;
    iface->is_visible_changed = caja_places_sidebar_is_visible_changed;
}

static void
caja_places_sidebar_set_parent_window (CajaPlacesSidebar *sidebar,
                                       CajaWindowInfo *window)
{
    CajaWindowSlotInfo *slot;

    sidebar->window = window;

    slot = caja_window_info_get_active_slot (window);

    sidebar->bookmarks = caja_bookmark_list_new ();
    sidebar->uri = caja_window_slot_info_get_current_location (slot);

    g_signal_connect_object (sidebar->bookmarks, "contents_changed",
                             G_CALLBACK (update_places),
                             sidebar, G_CONNECT_SWAPPED);

    g_signal_connect_object (window, "loading_uri",
                             G_CALLBACK (loading_uri_callback),
                             sidebar, 0);

    g_signal_connect_object (sidebar->volume_monitor, "volume_added",
                             G_CALLBACK (volume_added_callback), sidebar, 0);
    g_signal_connect_object (sidebar->volume_monitor, "volume_removed",
                             G_CALLBACK (volume_removed_callback), sidebar, 0);
    g_signal_connect_object (sidebar->volume_monitor, "volume_changed",
                             G_CALLBACK (volume_changed_callback), sidebar, 0);
    g_signal_connect_object (sidebar->volume_monitor, "mount_added",
                             G_CALLBACK (mount_added_callback), sidebar, 0);
    g_signal_connect_object (sidebar->volume_monitor, "mount_removed",
                             G_CALLBACK (mount_removed_callback), sidebar, 0);
    g_signal_connect_object (sidebar->volume_monitor, "mount_changed",
                             G_CALLBACK (mount_changed_callback), sidebar, 0);
    g_signal_connect_object (sidebar->volume_monitor, "drive_disconnected",
                             G_CALLBACK (drive_disconnected_callback), sidebar, 0);
    g_signal_connect_object (sidebar->volume_monitor, "drive_connected",
                             G_CALLBACK (drive_connected_callback), sidebar, 0);
    g_signal_connect_object (sidebar->volume_monitor, "drive_changed",
                             G_CALLBACK (drive_changed_callback), sidebar, 0);

    update_places (sidebar);
}

static void
#if GTK_CHECK_VERSION (3, 0, 0)
caja_places_sidebar_style_updated (GtkWidget *widget)
#else
caja_places_sidebar_style_set (GtkWidget *widget,
                               GtkStyle  *previous_style)
#endif
{
    CajaPlacesSidebar *sidebar;

    sidebar = CAJA_PLACES_SIDEBAR (widget);

    update_places (sidebar);
}

static CajaSidebar *
caja_places_sidebar_create (CajaSidebarProvider *provider,
                            CajaWindowInfo *window)
{
    CajaPlacesSidebar *sidebar;

    sidebar = g_object_new (caja_places_sidebar_get_type (), NULL);
    caja_places_sidebar_set_parent_window (sidebar, window);
    g_object_ref_sink (sidebar);

    return CAJA_SIDEBAR (sidebar);
}

static void
sidebar_provider_iface_init (CajaSidebarProviderIface *iface)
{
    iface->create = caja_places_sidebar_create;
}

static void
caja_places_sidebar_provider_init (CajaPlacesSidebarProvider *sidebar)
{
}

static void
caja_places_sidebar_provider_class_init (CajaPlacesSidebarProviderClass *class)
{
}

void
caja_places_sidebar_register (void)
{
    caja_module_add_type (caja_places_sidebar_provider_get_type ());
}

/* Drag and drop interfaces */

static void
_caja_shortcuts_model_filter_class_init (CajaShortcutsModelFilterClass *class)
{
}

static void
_caja_shortcuts_model_filter_init (CajaShortcutsModelFilter *model)
{
    model->sidebar = NULL;
}

/* GtkTreeDragSource::row_draggable implementation for the shortcuts filter model */
static gboolean
caja_shortcuts_model_filter_row_draggable (GtkTreeDragSource *drag_source,
                                           GtkTreePath       *path)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    PlaceType place_type;
    SectionType section_type;

    model = GTK_TREE_MODEL (drag_source);

    gtk_tree_model_get_iter (model, &iter, path);
    gtk_tree_model_get (model, &iter,
                        PLACES_SIDEBAR_COLUMN_ROW_TYPE, &place_type,
                        PLACES_SIDEBAR_COLUMN_SECTION_TYPE, &section_type,
                        -1);

    if (place_type != PLACES_HEADING && section_type == SECTION_BOOKMARKS)
        return TRUE;

    return FALSE;
}

/* Fill the GtkTreeDragSourceIface vtable */
static void
caja_shortcuts_model_filter_drag_source_iface_init (GtkTreeDragSourceIface *iface)
{
    iface->row_draggable = caja_shortcuts_model_filter_row_draggable;
}

static GtkTreeModel *
caja_shortcuts_model_filter_new (CajaPlacesSidebar *sidebar,
                                 GtkTreeModel          *child_model,
                                 GtkTreePath           *root)
{
    CajaShortcutsModelFilter *model;

    model = g_object_new (CAJA_SHORTCUTS_MODEL_FILTER_TYPE,
                          "child-model", child_model,
                          "virtual-root", root,
                          NULL);

    model->sidebar = sidebar;

    return GTK_TREE_MODEL (model);
}
