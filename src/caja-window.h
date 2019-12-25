/*
 *  Caja
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
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
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *           Darin Adler <darin@bentspoon.com>
 *
 */
/* caja-window.h: Interface of the main window object */

#ifndef CAJA_WINDOW_H
#define CAJA_WINDOW_H

#include <gtk/gtk.h>

#include <eel/eel-glib-extensions.h>

#include <libcaja-private/caja-bookmark.h>
#include <libcaja-private/caja-entry.h>
#include <libcaja-private/caja-window-info.h>
#include <libcaja-private/caja-search-directory.h>

#include "caja-application.h"
#include "caja-information-panel.h"
#include "caja-side-pane.h"

#define CAJA_TYPE_WINDOW caja_window_get_type()
#define CAJA_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_WINDOW, CajaWindow))
#define CAJA_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_WINDOW, CajaWindowClass))
#define CAJA_IS_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_WINDOW))
#define CAJA_IS_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_WINDOW))
#define CAJA_WINDOW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_WINDOW, CajaWindowClass))

#ifndef CAJA_WINDOW_DEFINED
#define CAJA_WINDOW_DEFINED
typedef struct CajaWindow CajaWindow;
#endif

#ifndef CAJA_WINDOW_SLOT_DEFINED
#define CAJA_WINDOW_SLOT_DEFINED
typedef struct CajaWindowSlot CajaWindowSlot;
#endif

typedef struct _CajaWindowPane      CajaWindowPane;

typedef struct CajaWindowSlotClass CajaWindowSlotClass;
typedef enum CajaWindowOpenSlotFlags CajaWindowOpenSlotFlags;

typedef enum
{
    CAJA_WINDOW_NOT_SHOWN,
    CAJA_WINDOW_POSITION_SET,
    CAJA_WINDOW_SHOULD_SHOW
} CajaWindowShowState;

enum CajaWindowOpenSlotFlags
{
    CAJA_WINDOW_OPEN_SLOT_NONE = 0,
    CAJA_WINDOW_OPEN_SLOT_APPEND = 1
};

typedef struct _CajaWindowPrivate CajaWindowPrivate;

typedef struct
{
    GtkWindowClass parent_spot;

    CajaWindowType window_type;
    const char *bookmarks_placeholder;

    /* Function pointers for overriding, without corresponding signals */

    char * (* get_title) (CajaWindow *window);
    void   (* sync_title) (CajaWindow *window,
                           CajaWindowSlot *slot);
    CajaIconInfo * (* get_icon) (CajaWindow *window,
                                 CajaWindowSlot *slot);

    void   (* sync_allow_stop) (CajaWindow *window,
                                CajaWindowSlot *slot);
    void   (* set_allow_up) (CajaWindow *window, gboolean allow);
    void   (* reload)              (CajaWindow *window);
    void   (* prompt_for_location) (CajaWindow *window, const char *initial);
    void   (* get_min_size) (CajaWindow *window, guint *default_width, guint *default_height);
    void   (* get_default_size) (CajaWindow *window, guint *default_width, guint *default_height);
    void   (* close) (CajaWindow *window);

    CajaWindowSlot * (* open_slot) (CajaWindowPane *pane,
                                    CajaWindowOpenSlotFlags flags);
    void                 (* close_slot) (CajaWindowPane *pane,
                                         CajaWindowSlot *slot);
    void                 (* set_active_slot) (CajaWindowPane *pane,
            CajaWindowSlot *slot);

    /* Signals used only for keybindings */
    gboolean (* go_up) (CajaWindow *window, gboolean close);
} CajaWindowClass;

struct CajaWindow
{
    GtkWindow parent_object;

    CajaWindowPrivate *details;

    CajaApplication *application;
};

GType            caja_window_get_type             (void);
void             caja_window_show_window          (CajaWindow    *window);
void             caja_window_close                (CajaWindow    *window);

void             caja_window_connect_content_view (CajaWindow    *window,
        CajaView      *view);
void             caja_window_disconnect_content_view (CajaWindow    *window,
        CajaView      *view);

void             caja_window_go_to                (CajaWindow    *window,
        GFile             *location);
void             caja_window_go_to_tab            (CajaWindow    *window,
        GFile             *location);
void             caja_window_go_to_full           (CajaWindow    *window,
        GFile             *location,
        CajaWindowGoToCallback callback,
        gpointer           user_data);
void             caja_window_go_to_with_selection (CajaWindow    *window,
        GFile             *location,
        GList             *new_selection);
void             caja_window_go_home              (CajaWindow    *window);
void             caja_window_new_tab              (CajaWindow    *window);
void             caja_window_new_window           (CajaWindow    *window);
void             caja_window_go_up                (CajaWindow    *window,
        gboolean           close_behind,
        gboolean           new_tab);
void             caja_window_prompt_for_location  (CajaWindow    *window,
        const char        *initial);
void             caja_window_display_error        (CajaWindow    *window,
        const char        *error_msg);
void		 caja_window_reload		      (CajaWindow	 *window);

void             caja_window_allow_reload         (CajaWindow    *window,
        gboolean           allow);
void             caja_window_allow_up             (CajaWindow    *window,
        gboolean           allow);
void             caja_window_allow_stop           (CajaWindow    *window,
        gboolean           allow);
GtkUIManager *   caja_window_get_ui_manager       (CajaWindow    *window);

#endif
