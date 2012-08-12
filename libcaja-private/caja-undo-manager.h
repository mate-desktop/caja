/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* CajaUndoManager - Manages undo and redo transactions.
 *                       This is the public interface used by the application.
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Author: Gene Z. Ragan <gzr@eazel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef CAJA_UNDO_MANAGER_H
#define CAJA_UNDO_MANAGER_H

#include <libcaja-private/caja-undo.h>

#define CAJA_TYPE_UNDO_MANAGER caja_undo_manager_get_type()
#define CAJA_UNDO_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_UNDO_MANAGER, CajaUndoManager))
#define CAJA_UNDO_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_UNDO_MANAGER, CajaUndoManagerClass))
#define CAJA_IS_UNDO_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_UNDO_MANAGER))
#define CAJA_IS_UNDO_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_UNDO_MANAGER))
#define CAJA_UNDO_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_UNDO_MANAGER, CajaUndoManagerClass))

typedef struct CajaUndoManagerDetails CajaUndoManagerDetails;

typedef struct
{
    GObject parent;
    CajaUndoManagerDetails *details;
} CajaUndoManager;

typedef struct
{
    GObjectClass parent_slot;
    void (* changed) (GObject *object, gpointer data);
} CajaUndoManagerClass;

GType                caja_undo_manager_get_type                           (void);
CajaUndoManager *caja_undo_manager_new                                (void);

/* Undo operations. */
void                 caja_undo_manager_undo                               (CajaUndoManager *undo_manager);

#ifdef UIH
/* Connect the manager to a particular menu item. */
void                 caja_undo_manager_set_up_matecomponent_ui_handler_undo_item (CajaUndoManager *manager,
        MateComponentUIHandler     *handler,
        const char          *path,
        const char          *no_undo_menu_item_label,
        const char          *no_undo_menu_item_hint);

#endif

/* Attach the undo manager to a Gtk object so that object and the widgets inside it can participate in undo. */
void                 caja_undo_manager_attach                             (CajaUndoManager *manager,
        GObject             *object);

void		caja_undo_manager_append (CajaUndoManager *manager,
                                      CajaUndoTransaction *transaction);
void            caja_undo_manager_forget (CajaUndoManager *manager,
        CajaUndoTransaction *transaction);

#endif /* CAJA_UNDO_MANAGER_H */
