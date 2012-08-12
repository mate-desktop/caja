/*
 *  caja-menu.h - Menus exported by CajaMenuProvider objects.
 *
 *  Copyright (C) 2005 Raffaele Sandrini
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
 *  Author:  Raffaele Sandrini <rasa@gmx.ch>
 *
 */

#include <config.h>
#include "caja-menu.h"
#include "caja-extension-i18n.h"

#include <glib.h>

#define CAJA_MENU_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CAJA_TYPE_MENU, CajaMenuPrivate))
G_DEFINE_TYPE (CajaMenu, caja_menu, G_TYPE_OBJECT);

struct _CajaMenuPrivate
{
    GList *item_list;
};

void
caja_menu_append_item (CajaMenu *menu, CajaMenuItem *item)
{
    g_return_if_fail (menu != NULL);
    g_return_if_fail (item != NULL);

    menu->priv->item_list = g_list_append (menu->priv->item_list, g_object_ref (item));
}

GList *
caja_menu_get_items (CajaMenu *menu)
{
    GList *item_list;

    g_return_val_if_fail (menu != NULL, NULL);

    item_list = g_list_copy (menu->priv->item_list);
    g_list_foreach (item_list, (GFunc)g_object_ref, NULL);

    return item_list;
}

void
caja_menu_item_list_free (GList *item_list)
{
    g_return_if_fail (item_list != NULL);

    g_list_foreach (item_list, (GFunc)g_object_unref, NULL);
    g_list_free (item_list);
}

/* Type initialization */

static void
caja_menu_finalize (GObject *object)
{
    CajaMenu *menu = CAJA_MENU (object);

    if (menu->priv->item_list)
    {
        g_list_free (menu->priv->item_list);
    }

    G_OBJECT_CLASS (caja_menu_parent_class)->finalize (object);
}

static void
caja_menu_init (CajaMenu *menu)
{
    menu->priv = CAJA_MENU_GET_PRIVATE (menu);

    menu->priv->item_list = NULL;
}

static void
caja_menu_class_init (CajaMenuClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (CajaMenuPrivate));

    object_class->finalize = caja_menu_finalize;
}

/* public constructors */

CajaMenu *
caja_menu_new (void)
{
    CajaMenu *obj;

    obj = CAJA_MENU (g_object_new (CAJA_TYPE_MENU, NULL));

    return obj;
}
