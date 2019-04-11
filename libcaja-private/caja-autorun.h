/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 2008 Red Hat, Inc.
 *
 * Caja is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

/* TODO:
 *
 * - automount all user-visible media on startup
 *  - but avoid doing autorun for these
 * - unmount all the media we've automounted on shutdown
 * - finish x-content / * types
 *  - finalize the semi-spec
 *  - add probing/sniffing code
 * - clean up code
 * - implement missing features
 *  - "Open Folder when mounted"
 *  - Autorun spec (e.g. $ROOT/.autostart)
 *
 */

#ifndef CAJA_AUTORUN_H
#define CAJA_AUTORUN_H

#include <gtk/gtk.h>

#include <eel/eel-background.h>

#include "caja-file.h"

typedef void (*CajaAutorunComboBoxChanged) (gboolean selected_ask,
        gboolean selected_ignore,
        gboolean selected_open_folder,
        GAppInfo *selected_app,
        gpointer user_data);

typedef void (*CajaAutorunOpenWindow) (GMount *mount, gpointer user_data);
typedef void (*CajaAutorunGetContent) (char **content, gpointer user_data);

void caja_autorun_prepare_combo_box (GtkWidget *combo_box,
                                     const char *x_content_type,
                                     gboolean include_ask,
                                     gboolean include_open_with_other_app,
                                     gboolean update_settings,
                                     CajaAutorunComboBoxChanged changed_cb,
                                     gpointer user_data);

void caja_autorun_set_preferences (const char *x_content_type, gboolean pref_ask, gboolean pref_ignore, gboolean pref_open_folder);
void caja_autorun_get_preferences (const char *x_content_type, gboolean *pref_ask, gboolean *pref_ignore, gboolean *pref_open_folder);

void caja_autorun (GMount *mount, CajaAutorunOpenWindow open_window_func, gpointer user_data);

char **caja_autorun_get_cached_x_content_types_for_mount (GMount       *mount);

void caja_autorun_get_x_content_types_for_mount_async (GMount *mount,
        CajaAutorunGetContent callback,
        GCancellable *cancellable,
        gpointer user_data);

void caja_autorun_launch_for_mount (GMount *mount, GAppInfo *app_info);

void caja_allow_autorun_for_volume (GVolume *volume);
void caja_allow_autorun_for_volume_finish (GVolume *volume);

#endif /* CAJA_AUTORUN_H */
