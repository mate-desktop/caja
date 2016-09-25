/*
 *  caja-menu.h - Menus exported by CajaMenuProvider objects.
 *
 *  Copyright (C) 2005 Raffaele Sandrini
 *  Copyright (C) 2003 Novell, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Author:  Dave Camp <dave@ximian.com>
 *           Raffaele Sandrini <rasa@gmx.ch>
 *
 */

#ifndef CAJA_MENU_H
#define CAJA_MENU_H

#include <glib-object.h>
#include "caja-extension-types.h"

G_BEGIN_DECLS

/* CajaMenu defines */
#define CAJA_TYPE_MENU         (caja_menu_get_type ())
#define CAJA_MENU(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CAJA_TYPE_MENU, CajaMenu))
#define CAJA_MENU_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CAJA_TYPE_MENU, CajaMenuClass))
#define CAJA_IS_MENU(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CAJA_TYPE_MENU))
#define CAJA_IS_MENU_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CAJA_TYPE_MENU))
#define CAJA_MENU_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CAJA_TYPE_MENU, CajaMenuClass))
/* CajaMenuItem defines */
#define CAJA_TYPE_MENU_ITEM            (caja_menu_item_get_type())
#define CAJA_MENU_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_MENU_ITEM, CajaMenuItem))
#define CAJA_MENU_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_MENU_ITEM, CajaMenuItemClass))
#define CAJA_MENU_IS_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_MENU_ITEM))
#define CAJA_MENU_IS_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), CAJA_TYPE_MENU_ITEM))
#define CAJA_MENU_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), CAJA_TYPE_MENU_ITEM, CajaMenuItemClass))

/* CajaMenu types */
typedef struct _CajaMenu		CajaMenu;
typedef struct _CajaMenuPrivate	CajaMenuPrivate;
typedef struct _CajaMenuClass	CajaMenuClass;
/* CajaMenuItem types */
typedef struct _CajaMenuItem        CajaMenuItem;
typedef struct _CajaMenuItemDetails CajaMenuItemDetails;
typedef struct _CajaMenuItemClass   CajaMenuItemClass;

/* CajaMenu structs */
struct _CajaMenu {
    GObject parent;
    CajaMenuPrivate *priv;
};

struct _CajaMenuClass {
    GObjectClass parent_class;
};

/* CajaMenuItem structs */
struct _CajaMenuItem {
    GObject parent;

    CajaMenuItemDetails *details;
};

struct _CajaMenuItemClass {
    GObjectClass parent;

    void (*activate) (CajaMenuItem *item);
};

/* CajaMenu methods */
GType     caja_menu_get_type       (void);
CajaMenu *caja_menu_new            (void);

void      caja_menu_append_item    (CajaMenu     *menu,
                                    CajaMenuItem *item);
GList    *caja_menu_get_items      (CajaMenu *menu);
void      caja_menu_item_list_free (GList *item_list);

/* CajaMenuItem methods */
GType         caja_menu_item_get_type    (void);
CajaMenuItem *caja_menu_item_new         (const char   *name,
                                          const char   *label,
                                          const char   *tip,
                                          const char   *icon);

void          caja_menu_item_activate    (CajaMenuItem *item);
void          caja_menu_item_set_submenu (CajaMenuItem *item,
                                          CajaMenu     *menu);

/* CajaMenuItem has the following properties:
 *   name (string)        - the identifier for the menu item
 *   label (string)       - the user-visible label of the menu item
 *   tip (string)         - the tooltip of the menu item
 *   icon (string)        - the name of the icon to display in the menu item
 *   sensitive (boolean)  - whether the menu item is sensitive or not
 *   priority (boolean)   - used for toolbar items, whether to show priority
 *                          text.
 *   menu (CajaMenu)      - The menu belonging to this item. May be null.
 */

G_END_DECLS

#endif /* CAJA_MENU_H */
