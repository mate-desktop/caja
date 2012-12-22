/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  glibcompat.h - GLib version-dependent definitions
 *
 *  Copyright (C) 2012 MATE Desktop Project
 *
 *  Caja is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  Caja is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Authors: Jasmine Hassan <jasmine.aura@gmail.com>
 *
 */

#ifndef GLIB_COMPAT_H
#define GLIB_COMPAT_H

#include <glib.h>
#include <glib-object.h>

#if !GLIB_CHECK_VERSION (2, 27, 2)
static inline void
g_list_free_full (GList *list, GDestroyNotify free_func)
{
    g_list_foreach (list, (GFunc) free_func, NULL);
    g_list_free (list);
}
#endif

#if !GLIB_CHECK_VERSION(2,28,0)
static inline void
g_clear_object_inline(volatile GObject **object_ptr)
{
    gpointer * const ptr = (gpointer)object_ptr;
    gpointer old;

    do {
        old = g_atomic_pointer_get(ptr);
    } while G_UNLIKELY(!g_atomic_pointer_compare_and_exchange(ptr, old, NULL));

    if (old)
        g_object_unref(old);
}
#undef  g_clear_object
#define g_clear_object(obj) g_clear_object_inline((volatile GObject **)(obj))
#endif

#endif /* GLIB_COMPAT_H */
