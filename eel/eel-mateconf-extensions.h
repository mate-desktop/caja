/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-mateconf-extensions.h - Stuff to make MateConf easier to use.

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

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef EEL_MATECONF_EXTENSIONS_H
#define EEL_MATECONF_EXTENSIONS_H

#include <glib.h>

#include <mateconf/mateconf.h>
#include <mateconf/mateconf-client.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EEL_MATECONF_UNDEFINED_CONNECTION 0

    MateConfClient *eel_mateconf_client_get_global     (void);
    gboolean     eel_mateconf_handle_error          (GError                **error);
    void         eel_mateconf_set_boolean           (const char             *key,
            gboolean                boolean_value);
    gboolean     eel_mateconf_get_boolean           (const char             *key);
    int          eel_mateconf_get_integer           (const char             *key);
    void         eel_mateconf_set_integer           (const char             *key,
            int                     int_value);
    char *       eel_mateconf_get_string            (const char             *key);
    void         eel_mateconf_set_string            (const char             *key,
            const char             *string_value);
    GSList *     eel_mateconf_get_string_list       (const char             *key);
    void         eel_mateconf_set_string_list       (const char             *key,
            const GSList           *string_list_value);
    void         eel_mateconf_unset                 (const char             *key);
    gboolean     eel_mateconf_key_is_writable       (const char             *key);
    gboolean     eel_mateconf_is_default            (const char             *key);
    gboolean     eel_mateconf_monitor_add           (const char             *directory);
    gboolean     eel_mateconf_monitor_remove        (const char             *directory);
    void         eel_mateconf_preload_cache         (const char             *directory,
            MateConfClientPreloadType  preload_type);
    void         eel_mateconf_suggest_sync          (void);
    MateConfValue*  eel_mateconf_get_value             (const char             *key);
    MateConfValue*  eel_mateconf_get_default_value     (const char             *key);
    gboolean     eel_mateconf_value_is_equal        (const MateConfValue       *a,
            const MateConfValue       *b);
    void         eel_mateconf_value_free            (MateConfValue             *value);
    guint        eel_mateconf_notification_add      (const char             *key,
            MateConfClientNotifyFunc   notification_callback,
            gpointer                callback_data);
    void         eel_mateconf_notification_remove   (guint                   notification_id);
    GSList *     eel_mateconf_value_get_string_list (const MateConfValue       *value);
    void         eel_mateconf_value_set_string_list (MateConfValue             *value,
            const GSList           *string_list);

#ifdef __cplusplus
}
#endif

#endif /* EEL_MATECONF_EXTENSIONS_H */
