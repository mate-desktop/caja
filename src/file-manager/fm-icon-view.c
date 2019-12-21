/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-icon-view.c - implementation of icon view of directory.

   Copyright (C) 2000, 2001 Eazel, Inc.

   The Mate Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Mate Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Mate Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Authors: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <eel/eel-background.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>

#include <libcaja-private/caja-clipboard-monitor.h>
#include <libcaja-private/caja-directory-background.h>
#include <libcaja-private/caja-directory.h>
#include <libcaja-private/caja-dnd.h>
#include <libcaja-private/caja-file-utilities.h>
#include <libcaja-private/caja-ui-utilities.h>
#include <libcaja-private/caja-global-preferences.h>
#include <libcaja-private/caja-icon-container.h>
#include <libcaja-private/caja-icon-dnd.h>
#include <libcaja-private/caja-link.h>
#include <libcaja-private/caja-metadata.h>
#include <libcaja-private/caja-view-factory.h>
#include <libcaja-private/caja-clipboard.h>
#include <libcaja-private/caja-desktop-icon-file.h>

#include "fm-icon-view.h"
#include "fm-actions.h"
#include "fm-icon-container.h"
#include "fm-desktop-icon-view.h"
#include "fm-error-reporting.h"
#include "caja-audio-mime-types.h"

#define POPUP_PATH_ICON_APPEARANCE		"/selection/Icon Appearance Items"

enum
{
    PROP_0,
    PROP_COMPACT
};

typedef struct
{
    const CajaFileSortType sort_type;
    const char *metadata_text;
    const char *action;
    const char *menu_label;
    const char *menu_hint;
} SortCriterion;

typedef enum
{
    MENU_ITEM_TYPE_STANDARD,
    MENU_ITEM_TYPE_CHECK,
    MENU_ITEM_TYPE_RADIO,
    MENU_ITEM_TYPE_TREE
} MenuItemType;

struct FMIconViewDetails
{
    GList *icons_not_positioned;

    guint react_to_icon_change_idle_id;

    const SortCriterion *sort;
    gboolean sort_reversed;

    GtkActionGroup *icon_action_group;
    guint icon_merge_id;

    int audio_preview_timeout;
    CajaFile *audio_preview_file;
    int audio_preview_child_watch;
    GPid audio_preview_child_pid;

    gboolean filter_by_screen;

    gboolean compact;

    gulong clipboard_handler_id;
};


/* Note that the first item in this list is the default sort,
 * and that the items show up in the menu in the order they
 * appear in this list.
 */
static const SortCriterion sort_criteria[] =
{
    {
        CAJA_FILE_SORT_BY_DISPLAY_NAME,
        "name",
        "Sort by Name",
        N_("by _Name"),
        N_("Keep icons sorted by name in rows")
    },
    {
        CAJA_FILE_SORT_BY_SIZE,
        "size",
        "Sort by Size",
        N_("by _Size"),
        N_("Keep icons sorted by size in rows")
    },
    {
        CAJA_FILE_SORT_BY_SIZE_ON_DISK,
        "size_on_disk",
        "Sort by Size on Disk",
        N_("by S_ize on Disk"),
        N_("Keep icons sorted by disk usage in rows")
    },
    {
        CAJA_FILE_SORT_BY_TYPE,
        "type",
        "Sort by Type",
        N_("by _Type"),
        N_("Keep icons sorted by type in rows")
    },
    {
        CAJA_FILE_SORT_BY_MTIME,
        "modification date",
        "Sort by Modification Date",
        N_("by Modification _Date"),
        N_("Keep icons sorted by modification date in rows")
    },
    {
        CAJA_FILE_SORT_BY_EMBLEMS,
        "emblems",
        "Sort by Emblems",
        N_("by _Emblems"),
        N_("Keep icons sorted by emblems in rows")
    },
    {
        CAJA_FILE_SORT_BY_TRASHED_TIME,
        "trashed",
        "Sort by Trash Time",
        N_("by T_rash Time"),
        N_("Keep icons sorted by trash time in rows")
    },
    {
        CAJA_FILE_SORT_BY_EXTENSION,
        "extension",
        "Sort by Extension",
        N_("by E_xtension"),
        N_("Keep icons sorted by reversed extension segments in rows")
    }
};

static gboolean default_sort_in_reverse_order = FALSE;
static int preview_sound_auto_value;

static void                 fm_icon_view_set_directory_sort_by        (FMIconView           *icon_view,
        CajaFile         *file,
        const char           *sort_by);
static void                 fm_icon_view_set_zoom_level               (FMIconView           *view,
        CajaZoomLevel     new_level,
        gboolean              always_emit);
static void                 fm_icon_view_update_click_mode            (FMIconView           *icon_view);
static void                 fm_icon_view_set_directory_tighter_layout (FMIconView           *icon_view,
        CajaFile         *file,
        gboolean              tighter_layout);
static gboolean             fm_icon_view_supports_manual_layout       (FMIconView           *icon_view);
static gboolean             fm_icon_view_supports_scaling	      (FMIconView           *icon_view);
static void                 fm_icon_view_reveal_selection             (FMDirectoryView      *view);
static const SortCriterion *get_sort_criterion_by_sort_type           (CajaFileSortType  sort_type);
static void                 set_sort_criterion_by_sort_type           (FMIconView           *icon_view,
        CajaFileSortType  sort_type);
static gboolean             set_sort_reversed                         (FMIconView           *icon_view,
        gboolean              new_value);
static void                 switch_to_manual_layout                   (FMIconView           *view);
static void                 preview_audio                             (FMIconView           *icon_view,
        CajaFile         *file,
        gboolean              start_flag);
static void                 update_layout_menus                       (FMIconView           *view);
static CajaFileSortType get_default_sort_order                    (CajaFile         *file,
        gboolean             *reversed);

static void default_sort_order_changed_callback            (gpointer callback_data);
static void default_sort_in_reverse_order_changed_callback (gpointer callback_data);
static void default_use_tighter_layout_changed_callback    (gpointer callback_data);
static void default_zoom_level_changed_callback            (gpointer callback_data);
static void labels_beside_icons_changed_callback           (gpointer callback_data);
static void all_columns_same_width_changed_callback        (gpointer callback_data);

static void fm_icon_view_iface_init (CajaViewIface *iface);

G_DEFINE_TYPE_WITH_CODE (FMIconView, fm_icon_view, FM_TYPE_DIRECTORY_VIEW,
                         G_IMPLEMENT_INTERFACE (CAJA_TYPE_VIEW,
                                 fm_icon_view_iface_init));

static void
fm_icon_view_destroy (GtkWidget *object)
{
    FMIconView *icon_view;

    icon_view = FM_ICON_VIEW (object);

    if (icon_view->details->react_to_icon_change_idle_id != 0)
    {
        g_source_remove (icon_view->details->react_to_icon_change_idle_id);
        icon_view->details->react_to_icon_change_idle_id = 0;
    }

    if (icon_view->details->clipboard_handler_id != 0)
    {
        g_signal_handler_disconnect (caja_clipboard_monitor_get (),
                                     icon_view->details->clipboard_handler_id);
        icon_view->details->clipboard_handler_id = 0;
    }

    /* kill any sound preview process that is ongoing */
    preview_audio (icon_view, NULL, FALSE);

    if (icon_view->details->icons_not_positioned)
    {
        caja_file_list_free (icon_view->details->icons_not_positioned);
        icon_view->details->icons_not_positioned = NULL;
    }

    GTK_WIDGET_CLASS (fm_icon_view_parent_class)->destroy (object);
}

static void
fm_icon_view_finalize (GObject *object)
{
    FMIconView *icon_view;

    icon_view = FM_ICON_VIEW (object);

    g_free (icon_view->details);

    g_signal_handlers_disconnect_by_func (caja_preferences,
                                          default_sort_order_changed_callback,
                                          icon_view);
    g_signal_handlers_disconnect_by_func (caja_preferences,
                                          default_sort_in_reverse_order_changed_callback,
                                          icon_view);
    g_signal_handlers_disconnect_by_func (caja_icon_view_preferences,
                                          default_use_tighter_layout_changed_callback,
                                          icon_view);
    g_signal_handlers_disconnect_by_func (caja_icon_view_preferences,
                                          default_zoom_level_changed_callback,
                                          icon_view);
    g_signal_handlers_disconnect_by_func (caja_icon_view_preferences,
                                          labels_beside_icons_changed_callback,
                                          icon_view);
    g_signal_handlers_disconnect_by_func (caja_compact_view_preferences,
                                          default_zoom_level_changed_callback,
                                          icon_view);
    g_signal_handlers_disconnect_by_func (caja_compact_view_preferences,
                                          all_columns_same_width_changed_callback,
                                          icon_view);

    G_OBJECT_CLASS (fm_icon_view_parent_class)->finalize (object);
}

static CajaIconContainer *
get_icon_container (FMIconView *icon_view)
{
    return CAJA_ICON_CONTAINER (gtk_bin_get_child (GTK_BIN (icon_view)));
}

static gboolean
get_stored_icon_position_callback (CajaIconContainer *container,
                                   CajaFile *file,
                                   CajaIconPosition *position,
                                   FMIconView *icon_view)
{
    char *position_string;
    gboolean position_good;
    char c;

    g_assert (CAJA_IS_ICON_CONTAINER (container));
    g_assert (CAJA_IS_FILE (file));
    g_assert (position != NULL);
    g_assert (FM_IS_ICON_VIEW (icon_view));

    if (!fm_icon_view_supports_manual_layout (icon_view))
    {
        return FALSE;
    }

    /* Get the current position of this icon from the metadata. */
    position_string = caja_file_get_metadata
                      (file, CAJA_METADATA_KEY_ICON_POSITION, "");
    position_good = sscanf
                    (position_string, " %d , %d %c",
                     &position->x, &position->y, &c) == 2;
    g_free (position_string);

    /* If it is the desktop directory, maybe the mate-libs metadata has information about it */

    /* Disable scaling if not on the desktop */
    if (fm_icon_view_supports_scaling (icon_view))
    {
        char *scale_string;

        /* Get the scale of the icon from the metadata. */
        scale_string = caja_file_get_metadata
                       (file, CAJA_METADATA_KEY_ICON_SCALE, "1");
        position->scale = g_ascii_strtod (scale_string, NULL);

        if (errno != 0)
        {
            position->scale = 1.0;
        }

        g_free (scale_string);
    }
    else
    {
        position->scale = 1.0;
    }

    return position_good;
}

static void
real_set_sort_criterion (FMIconView *icon_view,
                         const SortCriterion *sort,
                         gboolean clear)
{
    CajaFile *file;

    file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (icon_view));

    if (clear)
    {
        caja_file_set_metadata (file,
                                CAJA_METADATA_KEY_ICON_VIEW_SORT_BY, NULL, NULL);
        caja_file_set_metadata (file,
                                CAJA_METADATA_KEY_ICON_VIEW_SORT_REVERSED, NULL, NULL);
        icon_view->details->sort =
            get_sort_criterion_by_sort_type	(get_default_sort_order
                                             (file, &icon_view->details->sort_reversed));
    }
    else
    {
        /* Store the new sort setting. */
        fm_icon_view_set_directory_sort_by (icon_view,
                                            file,
                                            sort->metadata_text);
    }

    /* Update the layout menus to match the new sort setting. */
    update_layout_menus (icon_view);
}

static void
set_sort_criterion (FMIconView *icon_view, const SortCriterion *sort)
{
    if (sort == NULL ||
            icon_view->details->sort == sort)
    {
        return;
    }

    icon_view->details->sort = sort;

    real_set_sort_criterion (icon_view, sort, FALSE);
}

static void
clear_sort_criterion (FMIconView *icon_view)
{
    real_set_sort_criterion (icon_view, NULL, TRUE);
}

static void
action_stretch_callback (GtkAction *action,
                         gpointer callback_data)
{
    g_assert (FM_IS_ICON_VIEW (callback_data));

    caja_icon_container_show_stretch_handles
    (get_icon_container (FM_ICON_VIEW (callback_data)));
}

static void
action_unstretch_callback (GtkAction *action,
                           gpointer callback_data)
{
    g_assert (FM_IS_ICON_VIEW (callback_data));

    caja_icon_container_unstretch
    (get_icon_container (FM_ICON_VIEW (callback_data)));
}

static void
fm_icon_view_clean_up (FMIconView *icon_view)
{
    EEL_CALL_METHOD (FM_ICON_VIEW_CLASS, icon_view, clean_up, (icon_view));
}

static void
fm_icon_view_real_clean_up (FMIconView *icon_view)
{
    CajaIconContainer *icon_container;
    gboolean saved_sort_reversed;

    icon_container = get_icon_container (icon_view);

    /* Hardwire Clean Up to always be by name, in forward order */
    saved_sort_reversed = icon_view->details->sort_reversed;

    set_sort_reversed (icon_view, FALSE);
    set_sort_criterion (icon_view, &sort_criteria[0]);

    caja_icon_container_sort (icon_container);
    caja_icon_container_freeze_icon_positions (icon_container);

    set_sort_reversed (icon_view, saved_sort_reversed);
}

static void
action_clean_up_callback (GtkAction *action, gpointer callback_data)
{
    fm_icon_view_clean_up (FM_ICON_VIEW (callback_data));
}

static void
set_tighter_layout (FMIconView *icon_view, gboolean new_value)
{
    fm_icon_view_set_directory_tighter_layout (icon_view,
            fm_directory_view_get_directory_as_file
            (FM_DIRECTORY_VIEW (icon_view)),
            new_value);
    caja_icon_container_set_tighter_layout (get_icon_container (icon_view),
                                            new_value);
}

static void
action_tighter_layout_callback (GtkAction *action,
                                gpointer user_data)
{
    g_assert (FM_IS_ICON_VIEW (user_data));

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    set_tighter_layout (FM_ICON_VIEW (user_data),
                        gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
    G_GNUC_END_IGNORE_DEPRECATIONS;
}


static gboolean
fm_icon_view_using_auto_layout (FMIconView *icon_view)
{
    return caja_icon_container_is_auto_layout
           (get_icon_container (icon_view));
}

static gboolean
fm_icon_view_using_tighter_layout (FMIconView *icon_view)
{
    return caja_icon_container_is_tighter_layout
           (get_icon_container (icon_view));
}

static void
action_sort_radio_callback (GtkAction *action,
                            GtkRadioAction *current,
                            FMIconView *view)
{
    CajaFileSortType sort_type;

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    sort_type = gtk_radio_action_get_current_value (current);
    G_GNUC_END_IGNORE_DEPRECATIONS;

    /* Note that id might be a toggle item.
     * Ignore non-sort ids so that they don't cause sorting.
     */
    if (sort_type == CAJA_FILE_SORT_NONE)
    {
        switch_to_manual_layout (view);
    }
    else
    {
        set_sort_criterion_by_sort_type (view, sort_type);
    }
}

static void
list_covers (CajaIconData *data, gpointer callback_data)
{
    GSList **file_list;

    file_list = callback_data;

    *file_list = g_slist_prepend (*file_list, data);
}

static void
unref_cover (CajaIconData *data, gpointer callback_data)
{
    caja_file_unref (CAJA_FILE (data));
}

static void
fm_icon_view_clear (FMDirectoryView *view)
{
    CajaIconContainer *icon_container;
    GSList *file_list;

    g_return_if_fail (FM_IS_ICON_VIEW (view));

    icon_container = get_icon_container (FM_ICON_VIEW (view));
    if (!icon_container)
        return;

    /* Clear away the existing icons. */
    file_list = NULL;
    caja_icon_container_for_each (icon_container, list_covers, &file_list);
    caja_icon_container_clear (icon_container);
    g_slist_foreach (file_list, (GFunc)unref_cover, NULL);
    g_slist_free (file_list);
}


static gboolean
should_show_file_on_screen (FMDirectoryView *view, CajaFile *file)
{
    if (!fm_directory_view_should_show_file (view, file))
    {
        return FALSE;
    }

    return TRUE;
}

static void
fm_icon_view_remove_file (FMDirectoryView *view, CajaFile *file, CajaDirectory *directory)
{
    FMIconView *icon_view;

    /* This used to assert that 'directory == fm_directory_view_get_model (view)', but that
     * resulted in a lot of crash reports (bug #352592). I don't see how that trace happens.
     * It seems that somehow we get a files_changed event sent to the view from a directory
     * that isn't the model, but the code disables the monitor and signal callback handlers when
     * changing directories. Maybe we can get some more information when this happens.
     * Further discussion in bug #368178.
     */
    if (directory != fm_directory_view_get_model (view))
    {
        char *file_uri, *dir_uri, *model_uri;
        file_uri = caja_file_get_uri (file);
        dir_uri = caja_directory_get_uri (directory);
        model_uri = caja_directory_get_uri (fm_directory_view_get_model (view));
        g_warning ("fm_icon_view_remove_file() - directory not icon view model, shouldn't happen.\n"
                   "file: %p:%s, dir: %p:%s, model: %p:%s, view loading: %d\n"
                   "If you see this, please add this info to https://bugzilla.gnome.org/show_bug.cgi?id=368178",
                   file, file_uri, directory, dir_uri, fm_directory_view_get_model (view), model_uri, fm_directory_view_get_loading (view));
        g_free (file_uri);
        g_free (dir_uri);
        g_free (model_uri);
    }

    icon_view = FM_ICON_VIEW (view);

    if (caja_icon_container_remove (get_icon_container (icon_view),
                                    CAJA_ICON_CONTAINER_ICON_DATA (file)))
    {
        if (file == icon_view->details->audio_preview_file)
        {
            preview_audio (icon_view, NULL, FALSE);
        }

        caja_file_unref (file);
    }
}

static void
fm_icon_view_add_file (FMDirectoryView *view, CajaFile *file, CajaDirectory *directory)
{
    FMIconView *icon_view;
    CajaIconContainer *icon_container;

    g_assert (directory == fm_directory_view_get_model (view));

    icon_view = FM_ICON_VIEW (view);
    icon_container = get_icon_container (icon_view);

    if (icon_view->details->filter_by_screen &&
            !should_show_file_on_screen (view, file))
    {
        return;
    }

    /* Reset scroll region for the first icon added when loading a directory. */
    if (fm_directory_view_get_loading (view) && caja_icon_container_is_empty (icon_container))
    {
        caja_icon_container_reset_scroll_region (icon_container);
    }

    if (caja_icon_container_add (icon_container,
                                 CAJA_ICON_CONTAINER_ICON_DATA (file)))
    {
        caja_file_ref (file);
    }
}

static void
fm_icon_view_flush_added_files (FMDirectoryView *view)
{
    caja_icon_container_layout_now (get_icon_container (FM_ICON_VIEW (view)));
}

static void
fm_icon_view_file_changed (FMDirectoryView *view, CajaFile *file, CajaDirectory *directory)
{
    FMIconView *icon_view;

    g_assert (directory == fm_directory_view_get_model (view));

    g_return_if_fail (view != NULL);
    icon_view = FM_ICON_VIEW (view);

    if (!icon_view->details->filter_by_screen)
    {
        caja_icon_container_request_update
        (get_icon_container (icon_view),
         CAJA_ICON_CONTAINER_ICON_DATA (file));
        return;
    }

    if (!should_show_file_on_screen (view, file))
    {
        fm_icon_view_remove_file (view, file, directory);
    }
    else
    {

        caja_icon_container_request_update
        (get_icon_container (icon_view),
         CAJA_ICON_CONTAINER_ICON_DATA (file));
    }
}

static gboolean
fm_icon_view_supports_auto_layout (FMIconView *view)
{
    g_return_val_if_fail (FM_IS_ICON_VIEW (view), FALSE);

    return EEL_CALL_METHOD_WITH_RETURN_VALUE
           (FM_ICON_VIEW_CLASS, view,
            supports_auto_layout, (view));
}

static gboolean
fm_icon_view_supports_scaling (FMIconView *view)
{
    g_return_val_if_fail (FM_IS_ICON_VIEW (view), FALSE);

    return EEL_CALL_METHOD_WITH_RETURN_VALUE
           (FM_ICON_VIEW_CLASS, view,
            supports_scaling, (view));
}

static gboolean
fm_icon_view_supports_manual_layout (FMIconView *view)
{
    g_return_val_if_fail (FM_IS_ICON_VIEW (view), FALSE);

    return EEL_CALL_METHOD_WITH_RETURN_VALUE
           (FM_ICON_VIEW_CLASS, view,
            supports_manual_layout, (view));
}

static gboolean
fm_icon_view_supports_keep_aligned (FMIconView *view)
{
    g_return_val_if_fail (FM_IS_ICON_VIEW (view), FALSE);

    return EEL_CALL_METHOD_WITH_RETURN_VALUE
           (FM_ICON_VIEW_CLASS, view,
            supports_keep_aligned, (view));
}

static gboolean
fm_icon_view_supports_labels_beside_icons (FMIconView *view)
{
    g_return_val_if_fail (FM_IS_ICON_VIEW (view), FALSE);

    return EEL_CALL_METHOD_WITH_RETURN_VALUE
           (FM_ICON_VIEW_CLASS, view,
            supports_labels_beside_icons, (view));
}

static gboolean
fm_icon_view_supports_tighter_layout (FMIconView *view)
{
    return !fm_icon_view_is_compact (view);
}

static void
update_layout_menus (FMIconView *view)
{
    gboolean is_auto_layout;
    GtkAction *action;
    CajaFile *file;

    if (view->details->icon_action_group == NULL)
    {
        return;
    }

    is_auto_layout = fm_icon_view_using_auto_layout (view);
    file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (view));

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    if (fm_icon_view_supports_auto_layout (view))
    {
        const char *action_name;

        /* Mark sort criterion. */
        action_name = is_auto_layout ? view->details->sort->action : FM_ACTION_MANUAL_LAYOUT;
        action = gtk_action_group_get_action (view->details->icon_action_group,
                                              action_name);
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

        action = gtk_action_group_get_action (view->details->icon_action_group,
                                              FM_ACTION_TIGHTER_LAYOUT);
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
                                      fm_icon_view_using_tighter_layout (view));
        gtk_action_set_sensitive (action, fm_icon_view_supports_tighter_layout (view));
        gtk_action_set_visible (action, fm_icon_view_supports_tighter_layout (view));

        action = gtk_action_group_get_action (view->details->icon_action_group,
                                              FM_ACTION_REVERSED_ORDER);
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
                                      view->details->sort_reversed);
        gtk_action_set_sensitive (action, is_auto_layout);

        action = gtk_action_group_get_action (view->details->icon_action_group,
                                              FM_ACTION_SORT_TRASH_TIME);

        if (file != NULL && caja_file_is_in_trash (file))
        {
            gtk_action_set_visible (action, TRUE);
        }
        else
        {
            gtk_action_set_visible (action, FALSE);
        }
    }

    action = gtk_action_group_get_action (view->details->icon_action_group,
                                          FM_ACTION_MANUAL_LAYOUT);
    gtk_action_set_visible (action,
                            fm_icon_view_supports_manual_layout (view));

    /* Clean Up is only relevant for manual layout */
    action = gtk_action_group_get_action (view->details->icon_action_group,
                                          FM_ACTION_CLEAN_UP);
    gtk_action_set_sensitive (action, !is_auto_layout);

    if (FM_IS_DESKTOP_ICON_VIEW (view))
    {
        gtk_action_set_label (action, _("_Organize Desktop by Name"));
    }

    action = gtk_action_group_get_action (view->details->icon_action_group,
                                          FM_ACTION_KEEP_ALIGNED);
    gtk_action_set_visible (action,
                            fm_icon_view_supports_keep_aligned (view));
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
                                  caja_icon_container_is_keep_aligned (get_icon_container (view)));
    gtk_action_set_sensitive (action, !is_auto_layout);
    G_GNUC_END_IGNORE_DEPRECATIONS;
}


static char *
fm_icon_view_get_directory_sort_by (FMIconView *icon_view,
                                    CajaFile *file)
{
    if (!fm_icon_view_supports_auto_layout (icon_view))
    {
        return g_strdup ("name");
    }

    return EEL_CALL_METHOD_WITH_RETURN_VALUE
           (FM_ICON_VIEW_CLASS, icon_view,
            get_directory_sort_by, (icon_view, file));
}

static CajaFileSortType default_sort_order = CAJA_FILE_SORT_BY_DISPLAY_NAME;

static CajaFileSortType
get_default_sort_order (CajaFile *file, gboolean *reversed)
{
    static gboolean auto_storaged_added = FALSE;
    CajaFileSortType retval;

    if (auto_storaged_added == FALSE)
    {
        auto_storaged_added = TRUE;
        eel_g_settings_add_auto_enum (caja_preferences,
                                      CAJA_PREFERENCES_DEFAULT_SORT_ORDER,
                                      (int *) &default_sort_order);
        eel_g_settings_add_auto_boolean (caja_preferences,
                                         CAJA_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER,
                                         &default_sort_in_reverse_order);
    }

    retval = caja_file_get_default_sort_type (file, reversed);

    if (retval == CAJA_FILE_SORT_NONE)
    {

        if (reversed != NULL)
        {
            *reversed = default_sort_in_reverse_order;
        }

        retval = CLAMP (default_sort_order, CAJA_FILE_SORT_BY_DISPLAY_NAME,
                        CAJA_FILE_SORT_BY_EMBLEMS);
    }

    return retval;
}

static char *
fm_icon_view_real_get_directory_sort_by (FMIconView *icon_view,
        CajaFile *file)
{
    const SortCriterion *default_sort_criterion;
    default_sort_criterion = get_sort_criterion_by_sort_type (get_default_sort_order (file, NULL));
    g_return_val_if_fail (default_sort_criterion != NULL, NULL);

    return caja_file_get_metadata
           (file, CAJA_METADATA_KEY_ICON_VIEW_SORT_BY,
            default_sort_criterion->metadata_text);
}

static void
fm_icon_view_set_directory_sort_by (FMIconView *icon_view,
                                    CajaFile *file,
                                    const char *sort_by)
{
    if (!fm_icon_view_supports_auto_layout (icon_view))
    {
        return;
    }

    EEL_CALL_METHOD (FM_ICON_VIEW_CLASS, icon_view,
                     set_directory_sort_by, (icon_view, file, sort_by));
}

static void
fm_icon_view_real_set_directory_sort_by (FMIconView *icon_view,
        CajaFile *file,
        const char *sort_by)
{
    const SortCriterion *default_sort_criterion;
    default_sort_criterion = get_sort_criterion_by_sort_type (get_default_sort_order (file, NULL));
    g_return_if_fail (default_sort_criterion != NULL);

    caja_file_set_metadata
    (file, CAJA_METADATA_KEY_ICON_VIEW_SORT_BY,
     default_sort_criterion->metadata_text,
     sort_by);
}

static gboolean
fm_icon_view_get_directory_sort_reversed (FMIconView *icon_view,
        CajaFile *file)
{
    if (!fm_icon_view_supports_auto_layout (icon_view))
    {
        return FALSE;
    }

    return EEL_CALL_METHOD_WITH_RETURN_VALUE
           (FM_ICON_VIEW_CLASS, icon_view,
            get_directory_sort_reversed, (icon_view, file));
}

static gboolean
fm_icon_view_real_get_directory_sort_reversed (FMIconView *icon_view,
        CajaFile *file)
{
    gboolean reversed;

    get_default_sort_order (file, &reversed);
    return caja_file_get_boolean_metadata
           (file,
            CAJA_METADATA_KEY_ICON_VIEW_SORT_REVERSED,
            reversed);
}

static void
fm_icon_view_set_directory_sort_reversed (FMIconView *icon_view,
        CajaFile *file,
        gboolean sort_reversed)
{
    if (!fm_icon_view_supports_auto_layout (icon_view))
    {
        return;
    }

    EEL_CALL_METHOD (FM_ICON_VIEW_CLASS, icon_view,
                     set_directory_sort_reversed,
                     (icon_view, file, sort_reversed));
}

static void
fm_icon_view_real_set_directory_sort_reversed (FMIconView *icon_view,
        CajaFile *file,
        gboolean sort_reversed)
{
    gboolean reversed;

    get_default_sort_order (file, &reversed);
    caja_file_set_boolean_metadata
    (file,
     CAJA_METADATA_KEY_ICON_VIEW_SORT_REVERSED,
     reversed, sort_reversed);
}

static gboolean
get_default_directory_keep_aligned (void)
{
    return TRUE;
}

static gboolean
fm_icon_view_get_directory_keep_aligned (FMIconView *icon_view,
        CajaFile *file)
{
    if (!fm_icon_view_supports_keep_aligned (icon_view))
    {
        return FALSE;
    }

    return  caja_file_get_boolean_metadata
            (file,
             CAJA_METADATA_KEY_ICON_VIEW_KEEP_ALIGNED,
             get_default_directory_keep_aligned ());
}

static void
fm_icon_view_set_directory_keep_aligned (FMIconView *icon_view,
        CajaFile *file,
        gboolean keep_aligned)
{
    if (!fm_icon_view_supports_keep_aligned (icon_view))
    {
        return;
    }

    caja_file_set_boolean_metadata
    (file, CAJA_METADATA_KEY_ICON_VIEW_KEEP_ALIGNED,
     get_default_directory_keep_aligned (),
     keep_aligned);
}

static gboolean
fm_icon_view_get_directory_auto_layout (FMIconView *icon_view,
                                        CajaFile *file)
{
    if (!fm_icon_view_supports_auto_layout (icon_view))
    {
        return FALSE;
    }

    if (!fm_icon_view_supports_manual_layout (icon_view))
    {
        return TRUE;
    }

    return EEL_CALL_METHOD_WITH_RETURN_VALUE
           (FM_ICON_VIEW_CLASS, icon_view,
            get_directory_auto_layout, (icon_view, file));
}

static gboolean
fm_icon_view_real_get_directory_auto_layout (FMIconView *icon_view,
        CajaFile *file)
{


    return caja_file_get_boolean_metadata
           (file, CAJA_METADATA_KEY_ICON_VIEW_AUTO_LAYOUT, TRUE);
}

static void
fm_icon_view_set_directory_auto_layout (FMIconView *icon_view,
                                        CajaFile *file,
                                        gboolean auto_layout)
{
    if (!fm_icon_view_supports_auto_layout (icon_view) ||
            !fm_icon_view_supports_manual_layout (icon_view))
    {
        return;
    }

    EEL_CALL_METHOD (FM_ICON_VIEW_CLASS, icon_view,
                     set_directory_auto_layout, (icon_view, file, auto_layout));
}

static void
fm_icon_view_real_set_directory_auto_layout (FMIconView *icon_view,
        CajaFile *file,
        gboolean auto_layout)
{
    if (!fm_icon_view_supports_manual_layout (icon_view))
    {
        return;
    }

    caja_file_set_boolean_metadata
    (file, CAJA_METADATA_KEY_ICON_VIEW_AUTO_LAYOUT,
     TRUE,
     auto_layout);
}
/* maintainence of tighter layout boolean */

static gboolean
fm_icon_view_get_directory_tighter_layout (FMIconView *icon_view,
        CajaFile *file)
{
    return EEL_CALL_METHOD_WITH_RETURN_VALUE
           (FM_ICON_VIEW_CLASS, icon_view,
            get_directory_tighter_layout, (icon_view, file));
}

static gboolean default_directory_tighter_layout = FALSE;

static gboolean
get_default_directory_tighter_layout (void)
{
    static gboolean auto_storaged_added = FALSE;

    if (auto_storaged_added == FALSE)
    {
        auto_storaged_added = TRUE;
        eel_g_settings_add_auto_boolean (caja_icon_view_preferences,
                                         CAJA_PREFERENCES_ICON_VIEW_DEFAULT_USE_TIGHTER_LAYOUT,
                                         &default_directory_tighter_layout);
    }

    return default_directory_tighter_layout;
}

static gboolean
fm_icon_view_real_get_directory_tighter_layout (FMIconView *icon_view,
        CajaFile *file)
{
    if (!fm_icon_view_supports_tighter_layout (icon_view))
    {
        return FALSE;
    }

    return caja_file_get_boolean_metadata
           (file,
            CAJA_METADATA_KEY_ICON_VIEW_TIGHTER_LAYOUT,
            get_default_directory_tighter_layout ());
}

static void
fm_icon_view_set_directory_tighter_layout (FMIconView *icon_view,
        CajaFile *file,
        gboolean tighter_layout)
{
    EEL_CALL_METHOD (FM_ICON_VIEW_CLASS, icon_view,
                     set_directory_tighter_layout, (icon_view, file, tighter_layout));
}

static void
fm_icon_view_real_set_directory_tighter_layout (FMIconView *icon_view,
        CajaFile *file,
        gboolean tighter_layout)
{
    if (!fm_icon_view_supports_tighter_layout (icon_view))
    {
        return;
    }

    caja_file_set_boolean_metadata
    (file, CAJA_METADATA_KEY_ICON_VIEW_TIGHTER_LAYOUT,
     get_default_directory_tighter_layout (),
     tighter_layout);
}

static gboolean
real_supports_auto_layout (FMIconView *view)
{
    g_return_val_if_fail (FM_IS_ICON_VIEW (view), FALSE);

    return TRUE;
}

static gboolean
real_supports_scaling (FMIconView *view)
{
    g_return_val_if_fail (FM_IS_ICON_VIEW (view), FALSE);

    return FALSE;
}

static gboolean
real_supports_manual_layout (FMIconView *view)
{
    g_return_val_if_fail (FM_IS_ICON_VIEW (view), FALSE);

    return !fm_icon_view_is_compact (view);
}

static gboolean
real_supports_keep_aligned (FMIconView *view)
{
    g_return_val_if_fail (FM_IS_ICON_VIEW (view), FALSE);

    return FALSE;
}

static gboolean
real_supports_labels_beside_icons (FMIconView *view)
{
    g_return_val_if_fail (FM_IS_ICON_VIEW (view), TRUE);

    return TRUE;
}

static gboolean
set_sort_reversed (FMIconView *icon_view, gboolean new_value)
{
    if (icon_view->details->sort_reversed == new_value)
    {
        return FALSE;
    }
    icon_view->details->sort_reversed = new_value;

    /* Store the new sort setting. */
    fm_icon_view_set_directory_sort_reversed (icon_view, fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (icon_view)), new_value);

    /* Update the layout menus to match the new sort-order setting. */
    update_layout_menus (icon_view);

    return TRUE;
}

static const SortCriterion *
get_sort_criterion_by_metadata_text (const char *metadata_text)
{
    guint i;

    /* Figure out what the new sort setting should be. */
    for (i = 0; i < G_N_ELEMENTS (sort_criteria); i++)
    {
        if (g_strcmp0 (sort_criteria[i].metadata_text, metadata_text) == 0)
        {
            return &sort_criteria[i];
        }
    }
    return NULL;
}

static const SortCriterion *
get_sort_criterion_by_sort_type (CajaFileSortType sort_type)
{
    guint i;

    /* Figure out what the new sort setting should be. */
    for (i = 0; i < G_N_ELEMENTS (sort_criteria); i++)
    {
        if (sort_type == sort_criteria[i].sort_type)
        {
            return &sort_criteria[i];
        }
    }

    return NULL;
}

static CajaZoomLevel default_zoom_level = CAJA_ZOOM_LEVEL_STANDARD;
static CajaZoomLevel default_compact_zoom_level = CAJA_ZOOM_LEVEL_STANDARD;
#define DEFAULT_ZOOM_LEVEL(icon_view) icon_view->details->compact ? default_compact_zoom_level : default_zoom_level

static CajaZoomLevel
get_default_zoom_level (FMIconView *icon_view)
{
    static gboolean auto_storage_added = FALSE;

    if (!auto_storage_added)
    {
        auto_storage_added = TRUE;
        eel_g_settings_add_auto_enum (caja_icon_view_preferences,
                                      CAJA_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
                                      (int *) &default_zoom_level);
        eel_g_settings_add_auto_enum (caja_compact_view_preferences,
                                      CAJA_PREFERENCES_COMPACT_VIEW_DEFAULT_ZOOM_LEVEL,
                                      (int *) &default_compact_zoom_level);
    }

    return CLAMP (DEFAULT_ZOOM_LEVEL(icon_view), CAJA_ZOOM_LEVEL_SMALLEST, CAJA_ZOOM_LEVEL_LARGEST);
}

static void
set_labels_beside_icons (FMIconView *icon_view)
{
    gboolean labels_beside;

    if (fm_icon_view_supports_labels_beside_icons (icon_view))
    {
        labels_beside = fm_icon_view_is_compact (icon_view) ||
                        g_settings_get_boolean (caja_icon_view_preferences, CAJA_PREFERENCES_ICON_VIEW_LABELS_BESIDE_ICONS);

        if (labels_beside)
        {
            caja_icon_container_set_label_position
            (get_icon_container (icon_view),
             CAJA_ICON_LABEL_POSITION_BESIDE);
        }
        else
        {
            caja_icon_container_set_label_position
            (get_icon_container (icon_view),
             CAJA_ICON_LABEL_POSITION_UNDER);
        }
    }
}

static void
set_columns_same_width (FMIconView *icon_view)
{
    gboolean all_columns_same_width;

    if (fm_icon_view_is_compact (icon_view))
    {
        all_columns_same_width = g_settings_get_boolean (caja_compact_view_preferences, CAJA_PREFERENCES_COMPACT_VIEW_ALL_COLUMNS_SAME_WIDTH);
        caja_icon_container_set_all_columns_same_width (get_icon_container (icon_view), all_columns_same_width);
    }
}

static void
fm_icon_view_begin_loading (FMDirectoryView *view)
{
    FMIconView *icon_view;
    GtkWidget *icon_container;
    CajaFile *file;
    char *sort_name;

    g_return_if_fail (FM_IS_ICON_VIEW (view));

    icon_view = FM_ICON_VIEW (view);
    file = fm_directory_view_get_directory_as_file (view);
    icon_container = GTK_WIDGET (get_icon_container (icon_view));

    caja_icon_container_begin_loading (CAJA_ICON_CONTAINER (icon_container));

    caja_icon_container_set_allow_moves (CAJA_ICON_CONTAINER (icon_container),
                                         fm_directory_view_get_allow_moves (view));

    /* kill any sound preview process that is ongoing */
    preview_audio (icon_view, NULL, FALSE);
    /* FIXME bugzilla.gnome.org 45060: Should use methods instead
     * of hardcoding desktop knowledge in here.
     */
    if (FM_IS_DESKTOP_ICON_VIEW (view))
    {
        caja_connect_desktop_background_to_settings (CAJA_ICON_CONTAINER (icon_container));
    }
    else
    {
        GdkDragAction default_action;

        if (caja_window_info_get_window_type (fm_directory_view_get_caja_window (view)) == CAJA_WINDOW_NAVIGATION)
        {
            default_action = CAJA_DND_ACTION_SET_AS_GLOBAL_BACKGROUND;
        }
        else
        {
            default_action = CAJA_DND_ACTION_SET_AS_FOLDER_BACKGROUND;
        }

        caja_connect_background_to_file_metadata (icon_container, file, default_action);
    }

    /* Set up the zoom level from the metadata. */
    if (fm_directory_view_supports_zooming (FM_DIRECTORY_VIEW (icon_view)))
    {
        int level;

        if (icon_view->details->compact)
        {
            level = caja_file_get_integer_metadata
                    (file,
                     CAJA_METADATA_KEY_COMPACT_VIEW_ZOOM_LEVEL,
                     get_default_zoom_level (icon_view));
        }
        else
        {
            level = caja_file_get_integer_metadata
                    (file,
                     CAJA_METADATA_KEY_ICON_VIEW_ZOOM_LEVEL,
                     get_default_zoom_level (icon_view));
        }

        fm_icon_view_set_zoom_level (icon_view, level, TRUE);
    }

    /* Set the sort mode.
     * It's OK not to resort the icons because the
     * container doesn't have any icons at this point.
     */
    sort_name = fm_icon_view_get_directory_sort_by (icon_view, file);
    set_sort_criterion (icon_view, get_sort_criterion_by_metadata_text (sort_name));
    g_free (sort_name);

    /* Set the sort direction from the metadata. */
    set_sort_reversed (icon_view, fm_icon_view_get_directory_sort_reversed (icon_view, file));

    caja_icon_container_set_keep_aligned
    (get_icon_container (icon_view),
     fm_icon_view_get_directory_keep_aligned (icon_view, file));
    caja_icon_container_set_tighter_layout
    (get_icon_container (icon_view),
     fm_icon_view_get_directory_tighter_layout (icon_view, file));

    set_labels_beside_icons (icon_view);
    set_columns_same_width (icon_view);

    /* We must set auto-layout last, because it invokes the layout_changed
     * callback, which works incorrectly if the other layout criteria are
     * not already set up properly (see bug 6500, e.g.)
     */
    caja_icon_container_set_auto_layout
    (get_icon_container (icon_view),
     fm_icon_view_get_directory_auto_layout (icon_view, file));

    /* e.g. keep aligned may have changed */
    update_layout_menus (icon_view);
}

static void
icon_view_notify_clipboard_info (CajaClipboardMonitor *monitor,
                                 CajaClipboardInfo *info,
                                 FMIconView *icon_view)
{
    GList *icon_data;

    icon_data = NULL;
    if (info && info->cut)
    {
        icon_data = info->files;
    }

    caja_icon_container_set_highlighted_for_clipboard (
        get_icon_container (icon_view), icon_data);
}

static void
fm_icon_view_end_loading (FMDirectoryView *view,
                          gboolean all_files_seen)
{
    FMIconView *icon_view;
    GtkWidget *icon_container;
    CajaClipboardMonitor *monitor;
    CajaClipboardInfo *info;

    icon_view = FM_ICON_VIEW (view);

    icon_container = GTK_WIDGET (get_icon_container (icon_view));
    caja_icon_container_end_loading (CAJA_ICON_CONTAINER (icon_container), all_files_seen);

    monitor = caja_clipboard_monitor_get ();
    info = caja_clipboard_monitor_get_clipboard_info (monitor);

    icon_view_notify_clipboard_info (monitor, info, icon_view);
}

static CajaZoomLevel
fm_icon_view_get_zoom_level (FMDirectoryView *view)
{
    g_return_val_if_fail (FM_IS_ICON_VIEW (view), CAJA_ZOOM_LEVEL_STANDARD);

    return caja_icon_container_get_zoom_level (get_icon_container (FM_ICON_VIEW (view)));
}

static void
fm_icon_view_set_zoom_level (FMIconView *view,
                             CajaZoomLevel new_level,
                             gboolean always_emit)
{
    CajaIconContainer *icon_container;

    g_return_if_fail (FM_IS_ICON_VIEW (view));
    g_return_if_fail (new_level >= CAJA_ZOOM_LEVEL_SMALLEST &&
                      new_level <= CAJA_ZOOM_LEVEL_LARGEST);

    icon_container = get_icon_container (view);
    if (caja_icon_container_get_zoom_level (icon_container) == new_level)
    {
        if (always_emit)
        {
            g_signal_emit_by_name (view, "zoom_level_changed");
        }
        return;
    }

    if (view->details->compact)
    {
        caja_file_set_integer_metadata
        (fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (view)),
         CAJA_METADATA_KEY_COMPACT_VIEW_ZOOM_LEVEL,
         get_default_zoom_level (view),
         new_level);
    }
    else
    {
        caja_file_set_integer_metadata
        (fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (view)),
         CAJA_METADATA_KEY_ICON_VIEW_ZOOM_LEVEL,
         get_default_zoom_level (view),
         new_level);
    }

    caja_icon_container_set_zoom_level (icon_container, new_level);

    g_signal_emit_by_name (view, "zoom_level_changed");

    if (fm_directory_view_get_active (FM_DIRECTORY_VIEW (view)))
    {
        fm_directory_view_update_menus (FM_DIRECTORY_VIEW (view));
    }
}

static void
fm_icon_view_bump_zoom_level (FMDirectoryView *view, int zoom_increment)
{
    CajaZoomLevel new_level;

    g_return_if_fail (FM_IS_ICON_VIEW (view));

    new_level = fm_icon_view_get_zoom_level (view) + zoom_increment;

    if (new_level >= CAJA_ZOOM_LEVEL_SMALLEST &&
            new_level <= CAJA_ZOOM_LEVEL_LARGEST)
    {
        fm_directory_view_zoom_to_level (view, new_level);
    }
}

static void
fm_icon_view_zoom_to_level (FMDirectoryView *view,
                            CajaZoomLevel zoom_level)
{
    FMIconView *icon_view;

    g_assert (FM_IS_ICON_VIEW (view));

    icon_view = FM_ICON_VIEW (view);
    fm_icon_view_set_zoom_level (icon_view, zoom_level, FALSE);
}

static void
fm_icon_view_restore_default_zoom_level (FMDirectoryView *view)
{
    FMIconView *icon_view;

    g_return_if_fail (FM_IS_ICON_VIEW (view));

    icon_view = FM_ICON_VIEW (view);
    fm_directory_view_zoom_to_level
    (view, get_default_zoom_level (icon_view));
}

static gboolean
fm_icon_view_can_zoom_in (FMDirectoryView *view)
{
    g_return_val_if_fail (FM_IS_ICON_VIEW (view), FALSE);

    return fm_icon_view_get_zoom_level (view)
           < CAJA_ZOOM_LEVEL_LARGEST;
}

static gboolean
fm_icon_view_can_zoom_out (FMDirectoryView *view)
{
    g_return_val_if_fail (FM_IS_ICON_VIEW (view), FALSE);

    return fm_icon_view_get_zoom_level (view)
           > CAJA_ZOOM_LEVEL_SMALLEST;
}

static GtkWidget *
fm_icon_view_get_background_widget (FMDirectoryView *view)
{
    g_return_val_if_fail (FM_IS_ICON_VIEW (view), NULL);

    return GTK_WIDGET (get_icon_container (FM_ICON_VIEW (view)));
}

static gboolean
fm_icon_view_is_empty (FMDirectoryView *view)
{
    g_assert (FM_IS_ICON_VIEW (view));

    return caja_icon_container_is_empty
           (get_icon_container (FM_ICON_VIEW (view)));
}

static GList *
fm_icon_view_get_selection (FMDirectoryView *view)
{
    GList *list;

    g_return_val_if_fail (FM_IS_ICON_VIEW (view), NULL);

    list = caja_icon_container_get_selection
           (get_icon_container (FM_ICON_VIEW (view)));
    caja_file_list_ref (list);
    return list;
}

static void
count_item (CajaIconData *icon_data,
            gpointer callback_data)
{
    guint *count;

    count = callback_data;
    (*count)++;
}

static guint
fm_icon_view_get_item_count (FMDirectoryView *view)
{
    guint count;

    g_return_val_if_fail (FM_IS_ICON_VIEW (view), 0);

    count = 0;

    caja_icon_container_for_each
    (get_icon_container (FM_ICON_VIEW (view)),
     count_item, &count);

    return count;
}

static void
set_sort_criterion_by_sort_type (FMIconView *icon_view,
                                 CajaFileSortType  sort_type)
{
    const SortCriterion *sort;

    g_assert (FM_IS_ICON_VIEW (icon_view));

    sort = get_sort_criterion_by_sort_type (sort_type);
    g_return_if_fail (sort != NULL);

    if (sort == icon_view->details->sort
            && fm_icon_view_using_auto_layout (icon_view))
    {
        return;
    }

    set_sort_criterion (icon_view, sort);
    caja_icon_container_sort (get_icon_container (icon_view));
    fm_icon_view_reveal_selection (FM_DIRECTORY_VIEW (icon_view));
}


static void
action_reversed_order_callback (GtkAction *action,
                                gpointer user_data)
{
    FMIconView *icon_view;

    icon_view = FM_ICON_VIEW (user_data);

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    if (set_sort_reversed (icon_view,
                           gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))))
    {
        caja_icon_container_sort (get_icon_container (icon_view));
        fm_icon_view_reveal_selection (FM_DIRECTORY_VIEW (icon_view));
    }
    G_GNUC_END_IGNORE_DEPRECATIONS;
}

static void
action_keep_aligned_callback (GtkAction *action,
                              gpointer user_data)
{
    FMIconView *icon_view;
    CajaFile *file;
    gboolean keep_aligned;

    icon_view = FM_ICON_VIEW (user_data);

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    keep_aligned = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
    G_GNUC_END_IGNORE_DEPRECATIONS;

    file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (icon_view));
    fm_icon_view_set_directory_keep_aligned (icon_view,
            file,
            keep_aligned);

    caja_icon_container_set_keep_aligned (get_icon_container (icon_view),
                                          keep_aligned);
}

static void
switch_to_manual_layout (FMIconView *icon_view)
{
    if (!fm_icon_view_using_auto_layout (icon_view))
    {
        return;
    }

    icon_view->details->sort = &sort_criteria[0];

    caja_icon_container_set_auto_layout
    (get_icon_container (icon_view), FALSE);
}

static void
layout_changed_callback (CajaIconContainer *container,
                         FMIconView *icon_view)
{
    CajaFile *file;

    g_assert (FM_IS_ICON_VIEW (icon_view));
    g_assert (container == get_icon_container (icon_view));

    file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (icon_view));

    if (file != NULL)
    {
        fm_icon_view_set_directory_auto_layout
        (icon_view,
         file,
         fm_icon_view_using_auto_layout (icon_view));
        fm_icon_view_set_directory_tighter_layout
        (icon_view,
         file,
         fm_icon_view_using_tighter_layout (icon_view));
    }

    update_layout_menus (icon_view);
}

static gboolean
fm_icon_view_can_rename_file (FMDirectoryView *view, CajaFile *file)
{
    if (!(fm_icon_view_get_zoom_level (view) > CAJA_ZOOM_LEVEL_SMALLEST))
    {
        return FALSE;
    }

    return FM_DIRECTORY_VIEW_CLASS(fm_icon_view_parent_class)->can_rename_file (view, file);
}

static void
fm_icon_view_start_renaming_file (FMDirectoryView *view,
                                  CajaFile *file,
                                  gboolean select_all)
{
    /* call parent class to make sure the right icon is selected */
    FM_DIRECTORY_VIEW_CLASS(fm_icon_view_parent_class)->start_renaming_file (view, file, select_all);

    /* start renaming */
    caja_icon_container_start_renaming_selected_item
    (get_icon_container (FM_ICON_VIEW (view)), select_all);
}

static const GtkActionEntry icon_view_entries[] =
{
    /* name, stock id, label */  { "Arrange Items", NULL, N_("Arran_ge Items") },
    /* name, stock id */         { "Stretch", NULL,
        /* label, accelerator */       N_("Resize Icon..."), NULL,
        /* tooltip */                  N_("Make the selected icon resizable"),
        G_CALLBACK (action_stretch_callback)
    },
    /* name, stock id */         { "Unstretch", NULL,
        /* label, accelerator */       N_("Restore Icons' Original Si_zes"), NULL,
        /* tooltip */                  N_("Restore each selected icon to its original size"),
        G_CALLBACK (action_unstretch_callback)
    },
    /* name, stock id */         { "Clean Up", NULL,
        /* label, accelerator */       N_("_Organize by Name"), NULL,
        /* tooltip */                  N_("Reposition icons to better fit in the window and avoid overlapping"),
        G_CALLBACK (action_clean_up_callback)
    },
};

static const GtkToggleActionEntry icon_view_toggle_entries[] =
{
    /* name, stock id */      { "Tighter Layout", NULL,
        /* label, accelerator */    N_("Compact _Layout"), NULL,
        /* tooltip */               N_("Toggle using a tighter layout scheme"),
        G_CALLBACK (action_tighter_layout_callback),
        0
    },
    /* name, stock id */      { "Reversed Order", NULL,
        /* label, accelerator */    N_("Re_versed Order"), NULL,
        /* tooltip */               N_("Display icons in the opposite order"),
        G_CALLBACK (action_reversed_order_callback),
        0
    },
    /* name, stock id */      { "Keep Aligned", NULL,
        /* label, accelerator */    N_("_Keep Aligned"), NULL,
        /* tooltip */               N_("Keep icons lined up on a grid"),
        G_CALLBACK (action_keep_aligned_callback),
        0
    },
};

static const GtkRadioActionEntry arrange_radio_entries[] =
{
    {
        "Manual Layout", NULL,
        N_("_Manually"), NULL,
        N_("Leave icons wherever they are dropped"),
        CAJA_FILE_SORT_NONE
    },
    {
        "Sort by Name", NULL,
        N_("By _Name"), NULL,
        N_("Keep icons sorted by name in rows"),
        CAJA_FILE_SORT_BY_DISPLAY_NAME
    },
    {
        "Sort by Size", NULL,
        N_("By _Size"), NULL,
        N_("Keep icons sorted by size in rows"),
        CAJA_FILE_SORT_BY_SIZE
    },
    {
        "Sort by Size on Disk", NULL,
        N_("By S_ize on Disk"), NULL,
        N_("Keep icons sorted by disk usage in rows"),
        CAJA_FILE_SORT_BY_SIZE_ON_DISK
    },
    {
        "Sort by Type", NULL,
        N_("By _Type"), NULL,
        N_("Keep icons sorted by type in rows"),
        CAJA_FILE_SORT_BY_TYPE
    },
    {
        "Sort by Modification Date", NULL,
        N_("By Modification _Date"), NULL,
        N_("Keep icons sorted by modification date in rows"),
        CAJA_FILE_SORT_BY_MTIME
    },
    {
        "Sort by Emblems", NULL,
        N_("By _Emblems"), NULL,
        N_("Keep icons sorted by emblems in rows"),
        CAJA_FILE_SORT_BY_EMBLEMS
    },
    {
        "Sort by Trash Time", NULL,
        N_("By T_rash Time"), NULL,
        N_("Keep icons sorted by trash time in rows"),
        CAJA_FILE_SORT_BY_TRASHED_TIME
    },
    {
        "Sort by Extension", NULL,
        N_("By E_xtension"), NULL,
        N_("Keep icons sorted by reverse extension segments in rows"),
        CAJA_FILE_SORT_BY_EXTENSION
    },
};

static void
fm_icon_view_merge_menus (FMDirectoryView *view)
{
    FMIconView *icon_view;
    GtkUIManager *ui_manager;
    GtkActionGroup *action_group;
    const char *ui;

    g_assert (FM_IS_ICON_VIEW (view));

    FM_DIRECTORY_VIEW_CLASS (fm_icon_view_parent_class)->merge_menus (view);

    icon_view = FM_ICON_VIEW (view);

    ui_manager = fm_directory_view_get_ui_manager (FM_DIRECTORY_VIEW (icon_view));

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    action_group = gtk_action_group_new ("IconViewActions");
    gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
    icon_view->details->icon_action_group = action_group;
    gtk_action_group_add_actions (action_group,
                                  icon_view_entries, G_N_ELEMENTS (icon_view_entries),
                                  icon_view);
    gtk_action_group_add_toggle_actions (action_group,
                                         icon_view_toggle_entries, G_N_ELEMENTS (icon_view_toggle_entries),
                                         icon_view);
    gtk_action_group_add_radio_actions (action_group,
                                        arrange_radio_entries,
                                        G_N_ELEMENTS (arrange_radio_entries),
                                        -1,
                                        G_CALLBACK (action_sort_radio_callback),
                                        icon_view);
    G_GNUC_END_IGNORE_DEPRECATIONS;

    gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
    g_object_unref (action_group); /* owned by ui manager */

    ui = caja_ui_string_get ("caja-icon-view-ui.xml");
    icon_view->details->icon_merge_id =
        gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, NULL);

    /* Do one-time state-setting here; context-dependent state-setting
     * is done in update_menus.
     */
    if (!fm_icon_view_supports_auto_layout (icon_view))
    {
        GtkAction *action;

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        action = gtk_action_group_get_action (action_group,
                                              FM_ACTION_ARRANGE_ITEMS);
        gtk_action_set_visible (action, FALSE);
        G_GNUC_END_IGNORE_DEPRECATIONS;
    }

    if (fm_icon_view_supports_scaling (icon_view))
    {
        gtk_ui_manager_add_ui (ui_manager,
                               icon_view->details->icon_merge_id,
                               POPUP_PATH_ICON_APPEARANCE,
                               FM_ACTION_STRETCH,
                               FM_ACTION_STRETCH,
                               GTK_UI_MANAGER_MENUITEM,
                               FALSE);
        gtk_ui_manager_add_ui (ui_manager,
                               icon_view->details->icon_merge_id,
                               POPUP_PATH_ICON_APPEARANCE,
                               FM_ACTION_UNSTRETCH,
                               FM_ACTION_UNSTRETCH,
                               GTK_UI_MANAGER_MENUITEM,
                               FALSE);
    }

    update_layout_menus (icon_view);
}

static void
fm_icon_view_unmerge_menus (FMDirectoryView *view)
{
    FMIconView *icon_view;
    GtkUIManager *ui_manager;

    icon_view = FM_ICON_VIEW (view);

    FM_DIRECTORY_VIEW_CLASS (fm_icon_view_parent_class)->unmerge_menus (view);

    ui_manager = fm_directory_view_get_ui_manager (view);
    if (ui_manager != NULL)
    {
        caja_ui_unmerge_ui (ui_manager,
                            &icon_view->details->icon_merge_id,
                            &icon_view->details->icon_action_group);
    }
}

static void
fm_icon_view_update_menus (FMDirectoryView *view)
{
    FMIconView *icon_view;
    GList *selection;
    int selection_count;
    GtkAction *action;
    CajaIconContainer *icon_container;
    gboolean editable;

    icon_view = FM_ICON_VIEW (view);

    FM_DIRECTORY_VIEW_CLASS (fm_icon_view_parent_class)->update_menus(view);

    selection = fm_directory_view_get_selection (view);
    selection_count = g_list_length (selection);
    icon_container = get_icon_container (icon_view);

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    action = gtk_action_group_get_action (icon_view->details->icon_action_group,
                                          FM_ACTION_STRETCH);
    gtk_action_set_sensitive (action,
                              selection_count == 1
                              && icon_container != NULL
                              && !caja_icon_container_has_stretch_handles (icon_container));

    gtk_action_set_visible (action,
                            fm_icon_view_supports_scaling (icon_view));

    action = gtk_action_group_get_action (icon_view->details->icon_action_group,
                                          FM_ACTION_UNSTRETCH);
    g_object_set (action, "label",
                  eel_g_list_more_than_one_item (selection)
                  ? _("Restore Icons' Original Si_zes")
                  : _("Restore Icon's Original Si_ze"),
                  NULL);
    gtk_action_set_sensitive (action,
                              icon_container != NULL
                              && caja_icon_container_is_stretched (icon_container));

    gtk_action_set_visible (action,
                            fm_icon_view_supports_scaling (icon_view));

    caja_file_list_free (selection);

    editable = fm_directory_view_is_editable (view);
    action = gtk_action_group_get_action (icon_view->details->icon_action_group,
                                          FM_ACTION_MANUAL_LAYOUT);
    gtk_action_set_sensitive (action, editable);
    G_GNUC_END_IGNORE_DEPRECATIONS;
}

static void
fm_icon_view_reset_to_defaults (FMDirectoryView *view)
{
    CajaIconContainer *icon_container;
    FMIconView *icon_view;

    icon_view = FM_ICON_VIEW (view);
    icon_container = get_icon_container (icon_view);

    clear_sort_criterion (icon_view);
    caja_icon_container_set_keep_aligned
    (icon_container, get_default_directory_keep_aligned ());
    caja_icon_container_set_tighter_layout
    (icon_container, get_default_directory_tighter_layout ());

    caja_icon_container_sort (icon_container);

    update_layout_menus (icon_view);

    fm_icon_view_restore_default_zoom_level (view);
}

static void
fm_icon_view_select_all (FMDirectoryView *view)
{
    CajaIconContainer *icon_container;

    g_return_if_fail (FM_IS_ICON_VIEW (view));

    icon_container = get_icon_container (FM_ICON_VIEW (view));
    caja_icon_container_select_all (icon_container);
}

static void
fm_icon_view_reveal_selection (FMDirectoryView *view)
{
    GList *selection;

    g_return_if_fail (FM_IS_ICON_VIEW (view));

    selection = fm_directory_view_get_selection (view);

    /* Make sure at least one of the selected items is scrolled into view */
    if (selection != NULL)
    {
        caja_icon_container_reveal
        (get_icon_container (FM_ICON_VIEW (view)),
         selection->data);
    }

    caja_file_list_free (selection);
}

static GArray *
fm_icon_view_get_selected_icon_locations (FMDirectoryView *view)
{
    g_return_val_if_fail (FM_IS_ICON_VIEW (view), NULL);

    return caja_icon_container_get_selected_icon_locations
           (get_icon_container (FM_ICON_VIEW (view)));
}


static void
fm_icon_view_set_selection (FMDirectoryView *view, GList *selection)
{
    g_return_if_fail (FM_IS_ICON_VIEW (view));

    caja_icon_container_set_selection
    (get_icon_container (FM_ICON_VIEW (view)), selection);
}

static void
fm_icon_view_invert_selection (FMDirectoryView *view)
{
    g_return_if_fail (FM_IS_ICON_VIEW (view));

    caja_icon_container_invert_selection
    (get_icon_container (FM_ICON_VIEW (view)));
}

static gboolean
fm_icon_view_using_manual_layout (FMDirectoryView *view)
{
    g_return_val_if_fail (FM_IS_ICON_VIEW (view), FALSE);

    return !fm_icon_view_using_auto_layout (FM_ICON_VIEW (view));
}

static void
fm_icon_view_widget_to_file_operation_position (FMDirectoryView *view,
        GdkPoint *position)
{
    g_assert (FM_IS_ICON_VIEW (view));

    caja_icon_container_widget_to_file_operation_position
    (get_icon_container (FM_ICON_VIEW (view)), position);
}

static void
icon_container_activate_callback (CajaIconContainer *container,
                                  GList *file_list,
                                  FMIconView *icon_view)
{
    g_assert (FM_IS_ICON_VIEW (icon_view));
    g_assert (container == get_icon_container (icon_view));

    fm_directory_view_activate_files (FM_DIRECTORY_VIEW (icon_view),
                                      file_list,
                                      CAJA_WINDOW_OPEN_ACCORDING_TO_MODE, 0,
                                      TRUE);
}

static void
icon_container_activate_alternate_callback (CajaIconContainer *container,
        GList *file_list,
        FMIconView *icon_view)
{
    gboolean open_in_tab;
    CajaWindowInfo *window_info;
    CajaWindowOpenFlags flags;

    g_assert (FM_IS_ICON_VIEW (icon_view));
    g_assert (container == get_icon_container (icon_view));

    open_in_tab = TRUE;

    window_info = fm_directory_view_get_caja_window (FM_DIRECTORY_VIEW (icon_view));

    if (caja_window_info_get_window_type (window_info) == CAJA_WINDOW_NAVIGATION)
    {
        GdkEvent *event;

        event = gtk_get_current_event ();

        if (event->type == GDK_BUTTON_PRESS ||
                event->type == GDK_BUTTON_RELEASE ||
                event->type == GDK_2BUTTON_PRESS ||
                event->type == GDK_3BUTTON_PRESS)
        {
            GdkEventButton *button_event;

            button_event = (GdkEventButton *) event;
            open_in_tab = (button_event->state & GDK_SHIFT_MASK) == 0;
        }
        else if (event->type == GDK_KEY_PRESS ||
                 event->type == GDK_KEY_RELEASE)
        {
            GdkEventKey *key_event;

            key_event = (GdkEventKey *) event;
            open_in_tab = !((key_event->state & GDK_SHIFT_MASK) != 0 &&
                            (key_event->state & GDK_CONTROL_MASK) != 0);
        }
        else
        {
            open_in_tab = TRUE;
        }
    }

    flags = CAJA_WINDOW_OPEN_FLAG_CLOSE_BEHIND;
    if (open_in_tab)
    {
        flags |= CAJA_WINDOW_OPEN_FLAG_NEW_TAB;
    }
    else
    {
        flags |= CAJA_WINDOW_OPEN_FLAG_NEW_WINDOW;
    }

    fm_directory_view_activate_files (FM_DIRECTORY_VIEW (icon_view),
                                      file_list,
                                      CAJA_WINDOW_OPEN_ACCORDING_TO_MODE,
                                      flags,
                                      TRUE);
}

static void
band_select_started_callback (CajaIconContainer *container,
                              FMIconView *icon_view)
{
    g_assert (FM_IS_ICON_VIEW (icon_view));
    g_assert (container == get_icon_container (icon_view));

    fm_directory_view_start_batching_selection_changes (FM_DIRECTORY_VIEW (icon_view));
}

static void
band_select_ended_callback (CajaIconContainer *container,
                            FMIconView *icon_view)
{
    g_assert (FM_IS_ICON_VIEW (icon_view));
    g_assert (container == get_icon_container (icon_view));

    fm_directory_view_stop_batching_selection_changes (FM_DIRECTORY_VIEW (icon_view));
}

/* handle the preview signal by inspecting the mime type.  For now, we only preview local sound files. */

static char **
get_preview_argv (char *uri)
{
    char *command;
    char **argv;

    command = g_find_program_in_path ("totem-audio-preview");

    if (command)
    {
        argv = g_new (char *, 3);
        argv[0] = command;
        argv[1] = g_strdup (uri);
        argv[2] = NULL;

        return argv;
    }

    command = g_find_program_in_path ("gst-launch-1.0");

    if (!command)
    {
        command = g_find_program_in_path ("gst-launch-0.10");
    }

    if (command)
    {
        int i;

        argv = g_new (char *, 10);
        i = 0;
        argv[i++] = command;
        argv[i++] = g_strdup ("playbin");
        argv[i++] = g_strconcat ("uri=", uri, NULL);
        /* do not display videos */
        argv[i++] = g_strdup ("video-sink=fakesink");
        argv[i++] = NULL;
        return argv;
    }

    return NULL;
}

static void
audio_child_died (GPid     pid,
                  gint     status,
                  gpointer data)
{
    FMIconView *icon_view;

    g_return_if_fail (FM_IS_ICON_VIEW (data));

    icon_view = FM_ICON_VIEW (data);

    icon_view->details->audio_preview_child_watch = 0;
    icon_view->details->audio_preview_child_pid = 0;
}

/* here's the timer task that actually plays the file using mpg123, ogg123 or play. */
/* FIXME bugzilla.gnome.org 41258: we should get the application from our mime-type stuff */
static gboolean
play_file (gpointer callback_data)
{
    CajaFile *file;
    FMIconView *icon_view;
    GPid child_pid;
    char **argv;
    GError *error;
    char *uri;
    GFile *gfile;
    char *path;

    icon_view = FM_ICON_VIEW (callback_data);

    /* Stop timeout */
    icon_view->details->audio_preview_timeout = 0;

    file = icon_view->details->audio_preview_file;
    gfile = caja_file_get_location (file);
    path = g_file_get_path (gfile);

    /* if we have a local path, use that instead of the native URI.
     * this can be useful for special GVfs mounts, such as cdda://
     */
    if (path) {
        uri = g_filename_to_uri (path, NULL, NULL);
    } else {
        uri = caja_file_get_uri (file);
    }

    g_object_unref (gfile);
    g_free (path);

    argv = get_preview_argv (uri);
    g_free (uri);
    if (argv == NULL)
    {
        return FALSE;
    }

    error = NULL;
    if (!g_spawn_async_with_pipes (NULL,
                                   argv,
                                   NULL,
                                   G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                                   NULL,
                                   NULL /* user_data */,
                                   &child_pid,
                                   NULL, NULL, NULL,
                                   &error))
    {
        g_strfreev (argv);
        g_warning ("Error spawning sound preview: %s\n", error->message);
        g_error_free (error);
        return FALSE;
    }
    g_strfreev (argv);

    icon_view->details->audio_preview_child_watch =
        g_child_watch_add (child_pid,
                           audio_child_died, NULL);
    icon_view->details->audio_preview_child_pid = child_pid;

    return FALSE;
}

/* FIXME bugzilla.gnome.org 42530: Hardcoding this here sucks. We should be using components
 * for open ended things like this.
 */

/* this routine is invoked from the preview signal handler to preview a sound file.  We
   want to wait a suitable delay until we actually do it, so set up a timer task to actually
   start playing.  If we move out before the task files, we remove it. */

static void
preview_audio (FMIconView *icon_view, CajaFile *file, gboolean start_flag)
{
    /* Stop current audio playback */
    if (icon_view->details->audio_preview_child_pid != 0)
    {
        kill (icon_view->details->audio_preview_child_pid, SIGTERM);
        g_source_remove (icon_view->details->audio_preview_child_watch);
        waitpid (icon_view->details->audio_preview_child_pid, NULL, 0);
        icon_view->details->audio_preview_child_pid = 0;
    }

    if (icon_view->details->audio_preview_timeout != 0)
    {
        g_source_remove (icon_view->details->audio_preview_timeout);
        icon_view->details->audio_preview_timeout = 0;
    }

    if (start_flag)
    {
        icon_view->details->audio_preview_file = file;
        icon_view->details->audio_preview_timeout = g_timeout_add_seconds (1, play_file, icon_view);
    }
}

static gboolean
sound_preview_type_supported (CajaFile *file)
{
    char *mime_type;
    guint i;

    mime_type = caja_file_get_mime_type (file);
    if (mime_type == NULL)
    {
        return FALSE;
    }
    for (i = 0; i < G_N_ELEMENTS (audio_mime_types); i++)
    {
        if (g_content_type_is_a (mime_type, audio_mime_types[i]))
        {
            g_free (mime_type);
            return TRUE;
        }
    }

    g_free (mime_type);
    return FALSE;
}


static gboolean
should_preview_sound (CajaFile *file)
{
    GFile *location;
    GFilesystemPreviewType use_preview;

    use_preview = caja_file_get_filesystem_use_preview (file);

    location = caja_file_get_location (file);
    if (g_file_has_uri_scheme (location, "burn"))
    {
        g_object_unref (location);
        return FALSE;
    }
    g_object_unref (location);

    /* Check user performance preference */
    if (preview_sound_auto_value == CAJA_SPEED_TRADEOFF_NEVER)
    {
        return FALSE;
    }

    if (preview_sound_auto_value == CAJA_SPEED_TRADEOFF_ALWAYS)
    {
        if (use_preview == G_FILESYSTEM_PREVIEW_TYPE_NEVER)
        {
            return FALSE;
        }
        else
        {
            return TRUE;
        }
    }

    if (use_preview == G_FILESYSTEM_PREVIEW_TYPE_NEVER)
    {
        /* file system says to never preview anything */
        return FALSE;
    }
    else if (use_preview == G_FILESYSTEM_PREVIEW_TYPE_IF_LOCAL)
    {
        /* file system says we should treat file as if it's local */
        return TRUE;
    }
    else
    {
        /* only local files */
        return caja_file_is_local (file);
    }
}

static int
icon_container_preview_callback (CajaIconContainer *container,
                                 CajaFile *file,
                                 gboolean start_flag,
                                 FMIconView *icon_view)
{
    int result;

    result = 0;

    /* preview files based on the mime_type. */
    /* at first, we just handle sounds */
    if (should_preview_sound (file))
    {
        if (sound_preview_type_supported (file))
        {
            result = 1;
            preview_audio (icon_view, file, start_flag);
        }
    }

    /* Display file name in status area at low zoom levels, since
     * the name is not displayed or hard to read in the icon view.
     */
    if (fm_icon_view_get_zoom_level (FM_DIRECTORY_VIEW (icon_view)) <= CAJA_ZOOM_LEVEL_SMALLER)
    {
        if (start_flag)
        {
            char *file_name, *message;

            file_name = caja_file_get_display_name (file);
            message = g_strdup_printf (_("pointing at \"%s\""), file_name);
            g_free (file_name);
            caja_window_slot_info_set_status
            (fm_directory_view_get_caja_window_slot (FM_DIRECTORY_VIEW (icon_view)),
             message);
            g_free (message);
        }
        else
        {
            fm_directory_view_display_selection_info (FM_DIRECTORY_VIEW(icon_view));
        }
    }

    return result;
}

static void
renaming_icon_callback (CajaIconContainer *container,
                        GtkWidget *widget,
                        gpointer callback_data)
{
    FMDirectoryView *directory_view;

    directory_view = FM_DIRECTORY_VIEW (callback_data);
    caja_clipboard_set_up_editable
    (GTK_EDITABLE (widget),
     fm_directory_view_get_ui_manager (directory_view),
     FALSE);
}

int
fm_icon_view_compare_files (FMIconView   *icon_view,
                            CajaFile *a,
                            CajaFile *b)
{
    return caja_file_compare_for_sort
           (a, b, icon_view->details->sort->sort_type,
            /* Use type-unsafe cast for performance */
            fm_directory_view_should_sort_directories_first ((FMDirectoryView *)icon_view),
            icon_view->details->sort_reversed);
}

static int
compare_files (FMDirectoryView   *icon_view,
               CajaFile *a,
               CajaFile *b)
{
    return fm_icon_view_compare_files ((FMIconView *)icon_view, a, b);
}


void
fm_icon_view_filter_by_screen (FMIconView *icon_view,
                               gboolean filter)
{
    icon_view->details->filter_by_screen = filter;
}

static void
fm_icon_view_screen_changed (GtkWidget *widget,
                             GdkScreen *previous_screen)
{
    FMDirectoryView *view;
    GList *files, *l;

    if (GTK_WIDGET_CLASS (fm_icon_view_parent_class)->screen_changed)
    {
        GTK_WIDGET_CLASS (fm_icon_view_parent_class)->screen_changed (widget, previous_screen);
    }

    view = FM_DIRECTORY_VIEW (widget);
    if (FM_ICON_VIEW (view)->details->filter_by_screen)
    {
        CajaDirectory *directory;
        CajaIconContainer *icon_container;
        CajaFile *file = NULL;

        icon_container = get_icon_container (FM_ICON_VIEW (view));

        directory = fm_directory_view_get_model (view);
        files = caja_directory_get_file_list (directory);

        for (l = files; l != NULL; l = l->next)
        {
            file = l->data;

            if (!should_show_file_on_screen (view, file))
            {
                fm_icon_view_remove_file (view, file, directory);
            }
            else
            {
                if (caja_icon_container_add (icon_container,
                                             CAJA_ICON_CONTAINER_ICON_DATA (file)))
                {
                    caja_file_ref (file);
                }
            }
        }

        caja_file_list_unref (files);
        g_list_free (files);
    }
}

static gboolean
fm_icon_view_scroll_event (GtkWidget *widget,
                           GdkEventScroll *scroll_event)
{
    FMIconView *icon_view;
    gboolean ret;

    icon_view = FM_ICON_VIEW (widget);

    if(icon_view->details->compact)
    {
        if((ret = fm_directory_view_handle_scroll_event(FM_DIRECTORY_VIEW(icon_view), scroll_event)))
            return ret;

        if(scroll_event->direction == GDK_SCROLL_UP)
            scroll_event->direction = GDK_SCROLL_LEFT;
        else if(scroll_event->direction == GDK_SCROLL_DOWN)
            scroll_event->direction = GDK_SCROLL_RIGHT;
        else if(scroll_event->direction == GDK_SCROLL_SMOOTH)
        {
            /* no x value implies only vertical scrolling enabled */
            if(scroll_event->delta_x == 0.0)
            {
                /* convert vertical to horizontal */
                scroll_event->delta_x = scroll_event->delta_y;
                scroll_event->delta_y = 0.0;
            }
        }
    }

    return GTK_WIDGET_CLASS (fm_icon_view_parent_class)->scroll_event (widget, scroll_event);
}

static void
selection_changed_callback (CajaIconContainer *container,
                            FMIconView *icon_view)
{
    g_assert (FM_IS_ICON_VIEW (icon_view));
    g_assert (container == get_icon_container (icon_view));

    fm_directory_view_notify_selection_changed (FM_DIRECTORY_VIEW (icon_view));
}

static void
icon_container_context_click_selection_callback (CajaIconContainer *container,
        GdkEventButton *event,
        FMIconView *icon_view)
{
    g_assert (CAJA_IS_ICON_CONTAINER (container));
    g_assert (FM_IS_ICON_VIEW (icon_view));

    fm_directory_view_pop_up_selection_context_menu
    (FM_DIRECTORY_VIEW (icon_view), event);
}

static void
icon_container_context_click_background_callback (CajaIconContainer *container,
        GdkEventButton *event,
        FMIconView *icon_view)
{
    g_assert (CAJA_IS_ICON_CONTAINER (container));
    g_assert (FM_IS_ICON_VIEW (icon_view));

    /* FIXME: passing event from here won't work
     * for gtk_menu_popup_at_pointer (in eel_pop_up_context_menu() )
     * if the menu is being triggered from here by the menu key
     */
    fm_directory_view_pop_up_background_context_menu
    (FM_DIRECTORY_VIEW (icon_view), NULL);
}

static gboolean
fm_icon_view_react_to_icon_change_idle_callback (gpointer data)
{
    FMIconView *icon_view;

    g_assert (FM_IS_ICON_VIEW (data));

    icon_view = FM_ICON_VIEW (data);
    icon_view->details->react_to_icon_change_idle_id = 0;

    /* Rebuild the menus since some of them (e.g. Restore Stretched Icons)
     * may be different now.
     */
    fm_directory_view_update_menus (FM_DIRECTORY_VIEW (icon_view));

    /* Don't call this again (unless rescheduled) */
    return FALSE;
}

static void
icon_position_changed_callback (CajaIconContainer *container,
                                CajaFile *file,
                                const CajaIconPosition *position,
                                FMIconView *icon_view)
{
    char scale_string[G_ASCII_DTOSTR_BUF_SIZE];

    g_assert (FM_IS_ICON_VIEW (icon_view));
    g_assert (container == get_icon_container (icon_view));
    g_assert (CAJA_IS_FILE (file));

    /* Schedule updating menus for the next idle. Doing it directly here
     * noticeably slows down icon stretching.  The other work here to
     * store the icon position and scale does not seem to noticeably
     * slow down icon stretching. It would be trickier to move to an
     * idle call, because we'd have to keep track of potentially multiple
     * sets of file/geometry info.
     */
    if (fm_directory_view_get_active (FM_DIRECTORY_VIEW (icon_view)) &&
            icon_view->details->react_to_icon_change_idle_id == 0)
    {
        icon_view->details->react_to_icon_change_idle_id
            = g_idle_add (fm_icon_view_react_to_icon_change_idle_callback,
                          icon_view);
    }

    /* Store the new position of the icon in the metadata. */
    if (!fm_icon_view_using_auto_layout (icon_view))
    {
        char *position_string;

        position_string = g_strdup_printf
                          ("%d,%d", position->x, position->y);
        caja_file_set_metadata
        (file, CAJA_METADATA_KEY_ICON_POSITION,
         NULL, position_string);
        g_free (position_string);
    }


    g_ascii_dtostr (scale_string, sizeof (scale_string), position->scale);
    caja_file_set_metadata
    (file, CAJA_METADATA_KEY_ICON_SCALE,
     "1.0", scale_string);
}

/* Attempt to change the filename to the new text.  Notify user if operation fails. */
static void
fm_icon_view_icon_text_changed_callback (CajaIconContainer *container,
        CajaFile *file,
        char *new_name,
        FMIconView *icon_view)
{
    g_assert (CAJA_IS_FILE (file));

    /* Don't allow a rename with an empty string. Revert to original
     * without notifying the user.
     */
    if ((new_name == NULL) || (new_name[0] == '\0'))
    {
        return;
    }
    fm_rename_file (file, new_name, NULL, NULL);
}

static char *
get_icon_uri_callback (CajaIconContainer *container,
                       CajaFile *file,
                       FMIconView *icon_view)
{
    g_assert (CAJA_IS_ICON_CONTAINER (container));
    g_assert (CAJA_IS_FILE (file));
    g_assert (FM_IS_ICON_VIEW (icon_view));

    return caja_file_get_uri (file);
}

static char *
get_icon_drop_target_uri_callback (CajaIconContainer *container,
                                   CajaFile *file,
                                   FMIconView *icon_view)
{
    g_return_val_if_fail (CAJA_IS_ICON_CONTAINER (container), NULL);
    g_return_val_if_fail (CAJA_IS_FILE (file), NULL);
    g_return_val_if_fail (FM_IS_ICON_VIEW (icon_view), NULL);

    return caja_file_get_drop_target_uri (file);
}

/* Preferences changed callbacks */
static void
fm_icon_view_text_attribute_names_changed (FMDirectoryView *directory_view)
{
    g_assert (FM_IS_ICON_VIEW (directory_view));

    caja_icon_container_request_update_all (get_icon_container (FM_ICON_VIEW (directory_view)));
}

static void
fm_icon_view_embedded_text_policy_changed (FMDirectoryView *directory_view)
{
    g_assert (FM_IS_ICON_VIEW (directory_view));

    caja_icon_container_request_update_all (get_icon_container (FM_ICON_VIEW (directory_view)));
}

static void
fm_icon_view_image_display_policy_changed (FMDirectoryView *directory_view)
{
    g_assert (FM_IS_ICON_VIEW (directory_view));

    caja_icon_container_request_update_all (get_icon_container (FM_ICON_VIEW (directory_view)));
}

static void
fm_icon_view_click_policy_changed (FMDirectoryView *directory_view)
{
    g_assert (FM_IS_ICON_VIEW (directory_view));

    fm_icon_view_update_click_mode (FM_ICON_VIEW (directory_view));
}

static void
fm_icon_view_emblems_changed (FMDirectoryView *directory_view)
{
    g_assert (FM_IS_ICON_VIEW (directory_view));

    caja_icon_container_request_update_all (get_icon_container (FM_ICON_VIEW (directory_view)));
}

static void
default_sort_order_changed_callback (gpointer callback_data)
{
    FMIconView *icon_view;
    CajaFile *file;
    char *sort_name;
    CajaIconContainer *icon_container;

    g_return_if_fail (FM_IS_ICON_VIEW (callback_data));

    icon_view = FM_ICON_VIEW (callback_data);

    file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (icon_view));
    sort_name = fm_icon_view_get_directory_sort_by (icon_view, file);
    set_sort_criterion (icon_view, get_sort_criterion_by_metadata_text (sort_name));
    g_free (sort_name);

    icon_container = get_icon_container (icon_view);
    g_return_if_fail (CAJA_IS_ICON_CONTAINER (icon_container));

    caja_icon_container_request_update_all (icon_container);
}

static void
default_sort_in_reverse_order_changed_callback (gpointer callback_data)
{
    FMIconView *icon_view;
    CajaFile *file;
    CajaIconContainer *icon_container;

    g_return_if_fail (FM_IS_ICON_VIEW (callback_data));

    icon_view = FM_ICON_VIEW (callback_data);

    file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (icon_view));
    set_sort_reversed (icon_view, fm_icon_view_get_directory_sort_reversed (icon_view, file));
    icon_container = get_icon_container (icon_view);
    g_return_if_fail (CAJA_IS_ICON_CONTAINER (icon_container));

    caja_icon_container_request_update_all (icon_container);
}

static void
default_use_tighter_layout_changed_callback (gpointer callback_data)
{
    FMIconView *icon_view;
    CajaFile *file;
    CajaIconContainer *icon_container;

    g_return_if_fail (FM_IS_ICON_VIEW (callback_data));

    icon_view = FM_ICON_VIEW (callback_data);

    file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (icon_view));
    icon_container = get_icon_container (icon_view);
    g_return_if_fail (CAJA_IS_ICON_CONTAINER (icon_container));

    caja_icon_container_set_tighter_layout (
        icon_container,
        fm_icon_view_get_directory_tighter_layout (icon_view, file));

    caja_icon_container_request_update_all (icon_container);
}

static void
default_zoom_level_changed_callback (gpointer callback_data)
{
    FMIconView *icon_view;

    g_return_if_fail (FM_IS_ICON_VIEW (callback_data));

    icon_view = FM_ICON_VIEW (callback_data);

    if (fm_directory_view_supports_zooming (FM_DIRECTORY_VIEW (icon_view)))
    {
        CajaFile *file;
        int level;

        file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (icon_view));

        if (fm_icon_view_is_compact (icon_view))
        {
            level = caja_file_get_integer_metadata (file,
                                                    CAJA_METADATA_KEY_COMPACT_VIEW_ZOOM_LEVEL,
                                                    get_default_zoom_level (icon_view));
        }
        else
        {
            level = caja_file_get_integer_metadata (file,
                                                    CAJA_METADATA_KEY_ICON_VIEW_ZOOM_LEVEL,
                                                    get_default_zoom_level (icon_view));
        }
        fm_directory_view_zoom_to_level (FM_DIRECTORY_VIEW (icon_view), level);
    }
}

static void
labels_beside_icons_changed_callback (gpointer callback_data)
{
    FMIconView *icon_view;

    g_return_if_fail (FM_IS_ICON_VIEW (callback_data));

    icon_view = FM_ICON_VIEW (callback_data);

    set_labels_beside_icons (icon_view);
}

static void
all_columns_same_width_changed_callback (gpointer callback_data)
{
    FMIconView *icon_view;

    g_assert (FM_IS_ICON_VIEW (callback_data));

    icon_view = FM_ICON_VIEW (callback_data);

    set_columns_same_width (icon_view);
}


static void
fm_icon_view_sort_directories_first_changed (FMDirectoryView *directory_view)
{
    FMIconView *icon_view;

    icon_view = FM_ICON_VIEW (directory_view);

    if (fm_icon_view_using_auto_layout (icon_view))
    {
        caja_icon_container_sort
        (get_icon_container (icon_view));
    }
}

static gboolean
icon_view_can_accept_item (CajaIconContainer *container,
                           CajaFile *target_item,
                           const char *item_uri,
                           FMDirectoryView *view)
{
    return fm_directory_view_can_accept_item (target_item, item_uri, view);
}

static char *
icon_view_get_container_uri (CajaIconContainer *container,
                             FMDirectoryView *view)
{
    return fm_directory_view_get_uri (view);
}

static void
icon_view_move_copy_items (CajaIconContainer *container,
                           const GList *item_uris,
                           GArray *relative_item_points,
                           const char *target_dir,
                           int copy_action,
                           int x, int y,
                           FMDirectoryView *view)
{
    caja_clipboard_clear_if_colliding_uris (GTK_WIDGET (view),
                                            item_uris,
                                            fm_directory_view_get_copied_files_atom (view));
    fm_directory_view_move_copy_items (item_uris, relative_item_points, target_dir,
                                       copy_action, x, y, view);
}

static void
fm_icon_view_update_click_mode (FMIconView *icon_view)
{
    CajaIconContainer	*icon_container;
    int			click_mode;

    icon_container = get_icon_container (icon_view);
    g_assert (icon_container != NULL);

    click_mode = g_settings_get_enum (caja_preferences, CAJA_PREFERENCES_CLICK_POLICY);

    caja_icon_container_set_single_click_mode (icon_container,
            click_mode == CAJA_CLICK_POLICY_SINGLE);
}

static gboolean
get_stored_layout_timestamp (CajaIconContainer *container,
                             CajaIconData *icon_data,
                             time_t *timestamp,
                             FMIconView *view)
{
    if (icon_data == NULL)
    {
        CajaFile *file;
        CajaDirectory *directory;

        directory = fm_directory_view_get_model (FM_DIRECTORY_VIEW (view));

        if (directory == NULL)
        {
            return FALSE;
        }

        file = caja_directory_get_corresponding_file (directory);
        *timestamp = caja_file_get_time_metadata (file,
                     CAJA_METADATA_KEY_ICON_VIEW_LAYOUT_TIMESTAMP);
        caja_file_unref (file);
    }
    else
    {
        *timestamp = caja_file_get_time_metadata (CAJA_FILE (icon_data),
                     CAJA_METADATA_KEY_ICON_POSITION_TIMESTAMP);
    }

    return TRUE;
}

static gboolean
store_layout_timestamp (CajaIconContainer *container,
                        CajaIconData *icon_data,
                        const time_t *timestamp,
                        FMIconView *view)
{
    if (icon_data == NULL)
    {
        CajaFile *file;
        CajaDirectory *directory;

        directory = fm_directory_view_get_model (FM_DIRECTORY_VIEW (view));

        if (directory == NULL)
        {
            return FALSE;
        }

        file = caja_directory_get_corresponding_file (directory);
        caja_file_set_time_metadata (file,
                                     CAJA_METADATA_KEY_ICON_VIEW_LAYOUT_TIMESTAMP,
                                     (time_t) *timestamp);
        caja_file_unref (file);
    }
    else
    {
        caja_file_set_time_metadata (CAJA_FILE (icon_data),
                                     CAJA_METADATA_KEY_ICON_POSITION_TIMESTAMP,
                                     (time_t) *timestamp);
    }

    return TRUE;
}

static gboolean
focus_in_event_callback (GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
    CajaWindowSlotInfo *slot_info;
    FMIconView *icon_view = FM_ICON_VIEW (user_data);

    /* make the corresponding slot (and the pane that contains it) active */
    slot_info = fm_directory_view_get_caja_window_slot (FM_DIRECTORY_VIEW (icon_view));
    caja_window_slot_info_make_hosting_pane_active (slot_info);

    return FALSE;
}

static CajaIconContainer *
create_icon_container (FMIconView *icon_view)
{
    CajaIconContainer *icon_container;

    icon_container = fm_icon_container_new (icon_view);

    gtk_widget_set_can_focus (GTK_WIDGET (icon_container), TRUE);

    g_signal_connect_object (icon_container, "focus_in_event",
                             G_CALLBACK (focus_in_event_callback), icon_view, 0);
    g_signal_connect_object (icon_container, "activate",
                             G_CALLBACK (icon_container_activate_callback), icon_view, 0);
    g_signal_connect_object (icon_container, "activate_alternate",
                             G_CALLBACK (icon_container_activate_alternate_callback), icon_view, 0);
    g_signal_connect_object (icon_container, "band_select_started",
                             G_CALLBACK (band_select_started_callback), icon_view, 0);
    g_signal_connect_object (icon_container, "band_select_ended",
                             G_CALLBACK (band_select_ended_callback), icon_view, 0);
    g_signal_connect_object (icon_container, "context_click_selection",
                             G_CALLBACK (icon_container_context_click_selection_callback), icon_view, 0);
    g_signal_connect_object (icon_container, "context_click_background",
                             G_CALLBACK (icon_container_context_click_background_callback), icon_view, 0);
    g_signal_connect_object (icon_container, "icon_position_changed",
                             G_CALLBACK (icon_position_changed_callback), icon_view, 0);
    g_signal_connect_object (icon_container, "icon_text_changed",
                             G_CALLBACK (fm_icon_view_icon_text_changed_callback), icon_view, 0);
    g_signal_connect_object (icon_container, "selection_changed",
                             G_CALLBACK (selection_changed_callback), icon_view, 0);
    /* FIXME: many of these should move into fm-icon-container as virtual methods */
    g_signal_connect_object (icon_container, "get_icon_uri",
                             G_CALLBACK (get_icon_uri_callback), icon_view, 0);
    g_signal_connect_object (icon_container, "get_icon_drop_target_uri",
                             G_CALLBACK (get_icon_drop_target_uri_callback), icon_view, 0);
    g_signal_connect_object (icon_container, "move_copy_items",
                             G_CALLBACK (icon_view_move_copy_items), icon_view, 0);
    g_signal_connect_object (icon_container, "get_container_uri",
                             G_CALLBACK (icon_view_get_container_uri), icon_view, 0);
    g_signal_connect_object (icon_container, "can_accept_item",
                             G_CALLBACK (icon_view_can_accept_item), icon_view, 0);
    g_signal_connect_object (icon_container, "get_stored_icon_position",
                             G_CALLBACK (get_stored_icon_position_callback), icon_view, 0);
    g_signal_connect_object (icon_container, "layout_changed",
                             G_CALLBACK (layout_changed_callback), icon_view, 0);
    g_signal_connect_object (icon_container, "preview",
                             G_CALLBACK (icon_container_preview_callback), icon_view, 0);
    g_signal_connect_object (icon_container, "renaming_icon",
                             G_CALLBACK (renaming_icon_callback), icon_view, 0);
    g_signal_connect_object (icon_container, "icon_stretch_started",
                             G_CALLBACK (fm_directory_view_update_menus), icon_view,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (icon_container, "icon_stretch_ended",
                             G_CALLBACK (fm_directory_view_update_menus), icon_view,
                             G_CONNECT_SWAPPED);

    g_signal_connect_object (icon_container, "get_stored_layout_timestamp",
                             G_CALLBACK (get_stored_layout_timestamp), icon_view, 0);
    g_signal_connect_object (icon_container, "store_layout_timestamp",
                             G_CALLBACK (store_layout_timestamp), icon_view, 0);

    gtk_container_add (GTK_CONTAINER (icon_view),
                       GTK_WIDGET (icon_container));

    fm_icon_view_update_click_mode (icon_view);

    gtk_widget_show (GTK_WIDGET (icon_container));

    return icon_container;
}

/* Handles an URL received from Mozilla */
static void
icon_view_handle_netscape_url (CajaIconContainer *container, const char *encoded_url,
                               const char *target_uri,
                               GdkDragAction action, int x, int y, FMIconView *view)
{
    fm_directory_view_handle_netscape_url_drop (FM_DIRECTORY_VIEW (view),
            encoded_url, target_uri, action, x, y);
}

static void
icon_view_handle_uri_list (CajaIconContainer *container, const char *item_uris,
                           const char *target_uri,
                           GdkDragAction action, int x, int y, FMIconView *view)
{
    fm_directory_view_handle_uri_list_drop (FM_DIRECTORY_VIEW (view),
                                            item_uris, target_uri, action, x, y);
}

static void
icon_view_handle_text (CajaIconContainer *container, const char *text,
                       const char *target_uri,
                       GdkDragAction action, int x, int y, FMIconView *view)
{
    fm_directory_view_handle_text_drop (FM_DIRECTORY_VIEW (view),
                                        text, target_uri, action, x, y);
}

static void
icon_view_handle_raw (CajaIconContainer *container, const char *raw_data,
                      int length, const char *target_uri, const char *direct_save_uri,
                      GdkDragAction action, int x, int y, FMIconView *view)
{
    fm_directory_view_handle_raw_drop (FM_DIRECTORY_VIEW (view),
                                       raw_data, length, target_uri, direct_save_uri, action, x, y);
}

static char *
icon_view_get_first_visible_file (CajaView *view)
{
    CajaFile *file;
    FMIconView *icon_view;

    icon_view = FM_ICON_VIEW (view);

    file = CAJA_FILE (caja_icon_container_get_first_visible_icon (get_icon_container (icon_view)));

    if (file)
    {
        return caja_file_get_uri (file);
    }

    return NULL;
}

static void
icon_view_scroll_to_file (CajaView *view,
                          const char *uri)
{
    FMIconView *icon_view;

    icon_view = FM_ICON_VIEW (view);

    if (uri != NULL)
    {
        CajaFile *file;

        /* Only if existing, since we don't want to add the file to
           the directory if it has been removed since then */
        file = caja_file_get_existing_by_uri (uri);
        if (file != NULL)
        {
            caja_icon_container_scroll_to_icon (get_icon_container (icon_view),
                                                CAJA_ICON_CONTAINER_ICON_DATA (file));
            caja_file_unref (file);
        }
    }
}

static void
fm_icon_view_set_property (GObject         *object,
                           guint            prop_id,
                           const GValue    *value,
                           GParamSpec      *pspec)
{
    FMIconView *icon_view;

    icon_view = FM_ICON_VIEW (object);

    switch (prop_id)
    {
    case PROP_COMPACT:
        icon_view->details->compact = g_value_get_boolean (value);
        if (icon_view->details->compact)
        {
            caja_icon_container_set_layout_mode (get_icon_container (icon_view),
                                                 gtk_widget_get_direction (GTK_WIDGET(icon_view)) == GTK_TEXT_DIR_RTL ?
                                                 CAJA_ICON_LAYOUT_T_B_R_L :
                                                 CAJA_ICON_LAYOUT_T_B_L_R);
            caja_icon_container_set_forced_icon_size (get_icon_container (icon_view),
                    CAJA_ICON_SIZE_SMALLEST);
        }
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}



static void
fm_icon_view_class_init (FMIconViewClass *klass)
{
    FMDirectoryViewClass *fm_directory_view_class;

    fm_directory_view_class = FM_DIRECTORY_VIEW_CLASS (klass);

    G_OBJECT_CLASS (klass)->set_property = fm_icon_view_set_property;
    G_OBJECT_CLASS (klass)->finalize = fm_icon_view_finalize;

    GTK_WIDGET_CLASS (klass)->destroy = fm_icon_view_destroy;
    GTK_WIDGET_CLASS (klass)->screen_changed = fm_icon_view_screen_changed;
    GTK_WIDGET_CLASS (klass)->scroll_event = fm_icon_view_scroll_event;

    fm_directory_view_class->add_file = fm_icon_view_add_file;
    fm_directory_view_class->flush_added_files = fm_icon_view_flush_added_files;
    fm_directory_view_class->begin_loading = fm_icon_view_begin_loading;
    fm_directory_view_class->bump_zoom_level = fm_icon_view_bump_zoom_level;
    fm_directory_view_class->can_rename_file = fm_icon_view_can_rename_file;
    fm_directory_view_class->can_zoom_in = fm_icon_view_can_zoom_in;
    fm_directory_view_class->can_zoom_out = fm_icon_view_can_zoom_out;
    fm_directory_view_class->clear = fm_icon_view_clear;
    fm_directory_view_class->end_loading = fm_icon_view_end_loading;
    fm_directory_view_class->file_changed = fm_icon_view_file_changed;
    fm_directory_view_class->get_background_widget = fm_icon_view_get_background_widget;
    fm_directory_view_class->get_selected_icon_locations = fm_icon_view_get_selected_icon_locations;
    fm_directory_view_class->get_selection = fm_icon_view_get_selection;
    fm_directory_view_class->get_selection_for_file_transfer = fm_icon_view_get_selection;
    fm_directory_view_class->get_item_count = fm_icon_view_get_item_count;
    fm_directory_view_class->is_empty = fm_icon_view_is_empty;
    fm_directory_view_class->remove_file = fm_icon_view_remove_file;
    fm_directory_view_class->reset_to_defaults = fm_icon_view_reset_to_defaults;
    fm_directory_view_class->restore_default_zoom_level = fm_icon_view_restore_default_zoom_level;
    fm_directory_view_class->reveal_selection = fm_icon_view_reveal_selection;
    fm_directory_view_class->select_all = fm_icon_view_select_all;
    fm_directory_view_class->set_selection = fm_icon_view_set_selection;
    fm_directory_view_class->invert_selection = fm_icon_view_invert_selection;
    fm_directory_view_class->compare_files = compare_files;
    fm_directory_view_class->zoom_to_level = fm_icon_view_zoom_to_level;
    fm_directory_view_class->get_zoom_level = fm_icon_view_get_zoom_level;
    fm_directory_view_class->click_policy_changed = fm_icon_view_click_policy_changed;
    fm_directory_view_class->embedded_text_policy_changed = fm_icon_view_embedded_text_policy_changed;
    fm_directory_view_class->emblems_changed = fm_icon_view_emblems_changed;
    fm_directory_view_class->image_display_policy_changed = fm_icon_view_image_display_policy_changed;
    fm_directory_view_class->merge_menus = fm_icon_view_merge_menus;
    fm_directory_view_class->unmerge_menus = fm_icon_view_unmerge_menus;
    fm_directory_view_class->sort_directories_first_changed = fm_icon_view_sort_directories_first_changed;
    fm_directory_view_class->start_renaming_file = fm_icon_view_start_renaming_file;
    fm_directory_view_class->text_attribute_names_changed = fm_icon_view_text_attribute_names_changed;
    fm_directory_view_class->update_menus = fm_icon_view_update_menus;
    fm_directory_view_class->using_manual_layout = fm_icon_view_using_manual_layout;
    fm_directory_view_class->widget_to_file_operation_position = fm_icon_view_widget_to_file_operation_position;

    klass->clean_up = fm_icon_view_real_clean_up;
    klass->supports_auto_layout = real_supports_auto_layout;
    klass->supports_scaling = real_supports_scaling;
    klass->supports_manual_layout = real_supports_manual_layout;
    klass->supports_keep_aligned = real_supports_keep_aligned;
    klass->supports_labels_beside_icons = real_supports_labels_beside_icons;
    klass->get_directory_auto_layout = fm_icon_view_real_get_directory_auto_layout;
    klass->get_directory_sort_by = fm_icon_view_real_get_directory_sort_by;
    klass->get_directory_sort_reversed = fm_icon_view_real_get_directory_sort_reversed;
    klass->get_directory_tighter_layout = fm_icon_view_real_get_directory_tighter_layout;
    klass->set_directory_auto_layout = fm_icon_view_real_set_directory_auto_layout;
    klass->set_directory_sort_by = fm_icon_view_real_set_directory_sort_by;
    klass->set_directory_sort_reversed = fm_icon_view_real_set_directory_sort_reversed;
    klass->set_directory_tighter_layout = fm_icon_view_real_set_directory_tighter_layout;

    g_object_class_install_property (G_OBJECT_CLASS (klass),
                                     PROP_COMPACT,
                                     g_param_spec_boolean ("compact",
                                             "Compact",
                                             "Whether this view provides a compact listing",
                                             FALSE,
                                             G_PARAM_WRITABLE |
                                             G_PARAM_CONSTRUCT_ONLY));

}

static const char *
fm_icon_view_get_id (CajaView *view)
{
    if (FM_IS_DESKTOP_ICON_VIEW (view))
    {
        return FM_DESKTOP_ICON_VIEW_ID;
    }

    if (fm_icon_view_is_compact (FM_ICON_VIEW (view)))
    {
        return FM_COMPACT_VIEW_ID;
    }

    return FM_ICON_VIEW_ID;
}

static void
fm_icon_view_iface_init (CajaViewIface *iface)
{
    fm_directory_view_init_view_iface (iface);

    iface->get_view_id = fm_icon_view_get_id;
    iface->get_first_visible_file = icon_view_get_first_visible_file;
    iface->scroll_to_file = icon_view_scroll_to_file;
    iface->get_title = NULL;
}

static void
fm_icon_view_init (FMIconView *icon_view)
{
    static gboolean setup_sound_preview = FALSE;
    CajaIconContainer *icon_container;

    g_return_if_fail (gtk_bin_get_child (GTK_BIN (icon_view)) == NULL);

    icon_view->details = g_new0 (FMIconViewDetails, 1);
    icon_view->details->sort = &sort_criteria[0];
    icon_view->details->filter_by_screen = FALSE;

    icon_container = create_icon_container (icon_view);

    /* Set our default layout mode */
    caja_icon_container_set_layout_mode (icon_container,
                                         gtk_widget_get_direction (GTK_WIDGET(icon_container)) == GTK_TEXT_DIR_RTL ?
                                         CAJA_ICON_LAYOUT_R_L_T_B :
                                         CAJA_ICON_LAYOUT_L_R_T_B);

    if (!setup_sound_preview)
    {
        eel_g_settings_add_auto_enum (caja_preferences,
                                      CAJA_PREFERENCES_PREVIEW_SOUND,
                                      &preview_sound_auto_value);

        setup_sound_preview = TRUE;
    }

    g_signal_connect_swapped (caja_preferences,
                              "changed::" CAJA_PREFERENCES_DEFAULT_SORT_ORDER,
                              G_CALLBACK (default_sort_order_changed_callback),
                              icon_view);
    g_signal_connect_swapped (caja_preferences,
                              "changed::" CAJA_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER,
                              G_CALLBACK (default_sort_in_reverse_order_changed_callback),
                              icon_view);
    g_signal_connect_swapped (caja_icon_view_preferences,
                              "changed::" CAJA_PREFERENCES_ICON_VIEW_DEFAULT_USE_TIGHTER_LAYOUT,
                              G_CALLBACK (default_use_tighter_layout_changed_callback),
                              icon_view);
    g_signal_connect_swapped (caja_icon_view_preferences,
                              "changed::" CAJA_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
                              G_CALLBACK (default_zoom_level_changed_callback),
                              icon_view);
    g_signal_connect_swapped (caja_icon_view_preferences,
                              "changed::" CAJA_PREFERENCES_ICON_VIEW_LABELS_BESIDE_ICONS,
                              G_CALLBACK (labels_beside_icons_changed_callback),
                              icon_view);

    g_signal_connect_swapped (caja_compact_view_preferences,
                              "changed::" CAJA_PREFERENCES_COMPACT_VIEW_DEFAULT_ZOOM_LEVEL,
                              G_CALLBACK (default_zoom_level_changed_callback),
                              icon_view);
    g_signal_connect_swapped (caja_compact_view_preferences,
                              "changed::" CAJA_PREFERENCES_COMPACT_VIEW_ALL_COLUMNS_SAME_WIDTH,
                              G_CALLBACK (all_columns_same_width_changed_callback),
                              icon_view);

    g_signal_connect_object (get_icon_container (icon_view), "handle_netscape_url",
                             G_CALLBACK (icon_view_handle_netscape_url), icon_view, 0);
    g_signal_connect_object (get_icon_container (icon_view), "handle_uri_list",
                             G_CALLBACK (icon_view_handle_uri_list), icon_view, 0);
    g_signal_connect_object (get_icon_container (icon_view), "handle_text",
                             G_CALLBACK (icon_view_handle_text), icon_view, 0);
    g_signal_connect_object (get_icon_container (icon_view), "handle_raw",
                             G_CALLBACK (icon_view_handle_raw), icon_view, 0);

    icon_view->details->clipboard_handler_id =
        g_signal_connect (caja_clipboard_monitor_get (),
                          "clipboard_info",
                          G_CALLBACK (icon_view_notify_clipboard_info), icon_view);
}

static CajaView *
fm_icon_view_create (CajaWindowSlotInfo *slot)
{
    FMIconView *view;

    view = g_object_new (FM_TYPE_ICON_VIEW,
                         "window-slot", slot,
                         "compact", FALSE,
                         NULL);

    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (view)), GTK_STYLE_CLASS_VIEW);

    return CAJA_VIEW (view);
}

static CajaView *
fm_compact_view_create (CajaWindowSlotInfo *slot)
{
    FMIconView *view;

    view = g_object_new (FM_TYPE_ICON_VIEW,
                         "window-slot", slot,
                         "compact", TRUE,
                         NULL);

    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (view)), GTK_STYLE_CLASS_VIEW);

    return CAJA_VIEW (view);
}

static gboolean
fm_icon_view_supports_uri (const char *uri,
                           GFileType file_type,
                           const char *mime_type)
{
    if (file_type == G_FILE_TYPE_DIRECTORY)
    {
        return TRUE;
    }
    if (strcmp (mime_type, CAJA_SAVED_SEARCH_MIMETYPE) == 0)
    {
        return TRUE;
    }
    if (g_str_has_prefix (uri, "trash:"))
    {
        return TRUE;
    }
    if (g_str_has_prefix (uri, EEL_SEARCH_URI))
    {
        return TRUE;
    }

    return FALSE;
}

#define TRANSLATE_VIEW_INFO(view_info) \
	view_info.view_combo_label = _(view_info.view_combo_label); \
	view_info.view_menu_label_with_mnemonic = _(view_info.view_menu_label_with_mnemonic); \
	view_info.error_label = _(view_info.error_label); \
	view_info.startup_error_label = _(view_info.startup_error_label); \
	view_info.display_location_label = _(view_info.display_location_label); \


static CajaViewInfo fm_icon_view =
{
    .id = FM_ICON_VIEW_ID,
    /* Translators: this is used in the view selection dropdown
     * of navigation windows and in the preferences dialog */
    .view_combo_label = N_("Icon View"),
    /* Translators: this is used in the view menu */
    .view_menu_label_with_mnemonic = N_("_Icons"),
    .error_label = N_("The icon view encountered an error."),
    .startup_error_label = N_("The icon view encountered an error while starting up."),
    .display_location_label = N_("Display this location with the icon view."),
    .create = fm_icon_view_create,
    .supports_uri = fm_icon_view_supports_uri
};

static CajaViewInfo fm_compact_view =
{
    .id = FM_COMPACT_VIEW_ID,
    /* Translators: this is used in the view selection dropdown
     * of navigation windows and in the preferences dialog */
    .view_combo_label = N_("Compact View"),
    /* Translators: this is used in the view menu */
    .view_menu_label_with_mnemonic = N_("_Compact"),
    .error_label = N_("The compact view encountered an error."),
    .startup_error_label = N_("The compact view encountered an error while starting up."),
    .display_location_label = N_("Display this location with the compact view."),
    .create = fm_compact_view_create,
    .supports_uri = fm_icon_view_supports_uri
};

gboolean
fm_icon_view_is_compact (FMIconView *view)
{
    return view->details->compact;
}

void
fm_icon_view_register (void)
{
    TRANSLATE_VIEW_INFO (fm_icon_view)
    caja_view_factory_register (&fm_icon_view);
}

void
fm_compact_view_register (void)
{
    TRANSLATE_VIEW_INFO (fm_compact_view)
    caja_view_factory_register (&fm_compact_view);
}
