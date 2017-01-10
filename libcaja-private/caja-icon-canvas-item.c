/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Caja - Icon canvas item class for icon container.
 *
 * Copyright (C) 2000 Eazel, Inc
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
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

#include <config.h>
#include <math.h>
#include "caja-icon-canvas-item.h"

#include <glib/gi18n.h>

#include "caja-file-utilities.h"
#include "caja-global-preferences.h"
#include "caja-icon-private.h"
#include <eel/eel-art-extensions.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-graphic-effects.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-string.h>
#include <eel/eel-accessibility.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>
#include <atk/atkimage.h>
#include <atk/atkcomponent.h>
#include <atk/atknoopobject.h>
#include <stdio.h>
#include <string.h>

#define EMBLEM_SPACING 2

/* gap between bottom of icon and start of text box */
#define LABEL_OFFSET 1
#define LABEL_LINE_SPACING 0

#define MAX_TEXT_WIDTH_STANDARD 135
#define MAX_TEXT_WIDTH_TIGHTER 80
#define MAX_TEXT_WIDTH_BESIDE 90
#define MAX_TEXT_WIDTH_BESIDE_TOP_TO_BOTTOM 150

/* special text height handling
 * each item has three text height variables:
 *  + text_height: actual height of the displayed (i.e. on-screen) PangoLayout.
 *  + text_height_for_layout: height used in icon grid layout algorithms.
 *       		      “sane amount” of text.
 *   “sane amount“ as of
 *      + hard-coded to three lines in text-below-icon mode.
 *      + unlimited in text-besides-icon mode (see VOODOO-TODO)
 *
 *  This layout height is used by grid layout algorithms, even
 *  though the actually displayed and/or requested text size may be larger
 *  and overlap adjacent icons, if an icon is selected.
 *
 *  + text_height_for_entire_text: height needed to display the entire PangoLayout,
 *    if it wasn't ellipsized.
 */

/* Private part of the CajaIconCanvasItem structure. */
struct CajaIconCanvasItemDetails
{
    /* The image, text, font. */
    double x, y;
    GdkPixbuf *pixbuf;
    GdkPixbuf *rendered_pixbuf;
    GList *emblem_pixbufs;
    char *editable_text;		/* Text that can be modified by a renaming function */
    char *additional_text;		/* Text that cannot be modifed, such as file size, etc. */
    GdkPoint *attach_points;
    int n_attach_points;

    /* Size of the text at current font. */
    int text_dx;
    int text_width;

    /* actual size required for rendering the text to display */
    int text_height;
    /* actual size that would be required for rendering the entire text if it wasn't ellipsized */
    int text_height_for_entire_text;
    /* actual size needed for rendering a “sane amount” of text */
    int text_height_for_layout;

    int editable_text_height;

    /* whether the entire text must always be visible. In that case,
     * text_height_for_layout will always be equal to text_height.
     * Used for the last line of a line-wise icon layout. */
    guint entire_text : 1;

    /* preview state */
    guint is_active : 1;

    /* Highlight state. */
    guint is_highlighted_for_selection : 1;
    guint is_highlighted_as_keyboard_focus: 1;
    guint is_highlighted_for_drop : 1;
    guint is_highlighted_for_clipboard : 1;
    guint show_stretch_handles : 1;
    guint is_prelit : 1;

    guint rendered_is_active : 1;
    guint rendered_is_highlighted_for_selection : 1;
    guint rendered_is_highlighted_for_drop : 1;
    guint rendered_is_highlighted_for_clipboard : 1;
    guint rendered_is_prelit : 1;
    guint rendered_is_focused : 1;

    guint is_renaming : 1;

    guint bounds_cached : 1;

    guint is_visible : 1;

    GdkRectangle embedded_text_rect;
    char *embedded_text;

    /* Cached PangoLayouts. Only used if the icon is visible */
    PangoLayout *editable_text_layout;
    PangoLayout *additional_text_layout;
    PangoLayout *embedded_text_layout;

    /* Cached rectangle in canvas coordinates */
    EelIRect canvas_rect;
    EelIRect text_rect;
    EelIRect emblem_rect;

    EelIRect bounds_cache;
    EelIRect bounds_cache_for_layout;
    EelIRect bounds_cache_for_entire_item;

    /* Accessibility bits */
    GailTextUtil *text_util;
};

/* Object argument IDs. */
enum
{
    PROP_0,
    PROP_EDITABLE_TEXT,
    PROP_ADDITIONAL_TEXT,
    PROP_HIGHLIGHTED_FOR_SELECTION,
    PROP_HIGHLIGHTED_AS_KEYBOARD_FOCUS,
    PROP_HIGHLIGHTED_FOR_DROP,
    PROP_HIGHLIGHTED_FOR_CLIPBOARD
};

typedef enum
{
    RIGHT_SIDE,
    BOTTOM_SIDE,
    LEFT_SIDE,
    TOP_SIDE
} RectangleSide;

typedef struct
{
    CajaIconCanvasItem *icon_item;
    EelIRect icon_rect;
    RectangleSide side;
    int position;
    int index;
    GList *emblem;
} EmblemLayout;

static int click_policy_auto_value;

static void caja_icon_canvas_item_text_interface_init (EelAccessibleTextIface *iface);
static GType caja_icon_canvas_item_accessible_factory_get_type (void);

G_DEFINE_TYPE_WITH_CODE (CajaIconCanvasItem, caja_icon_canvas_item, EEL_TYPE_CANVAS_ITEM,
                         G_IMPLEMENT_INTERFACE (EEL_TYPE_ACCESSIBLE_TEXT,
                                 caja_icon_canvas_item_text_interface_init));

/* private */
static void     draw_label_text                      (CajaIconCanvasItem        *item,
#if GTK_CHECK_VERSION(3,0,0)
    						      cairo_t                   *cr,
#else
    						      GdkDrawable               *drawable,
#endif
    						      gboolean                  create_mask,
    						      EelIRect                  icon_rect);
static void     measure_label_text                   (CajaIconCanvasItem        *item);
static void     get_icon_canvas_rectangle            (CajaIconCanvasItem        *item,
    						      EelIRect                  *rect);
static void     emblem_layout_reset                  (EmblemLayout              *layout,
    						      CajaIconCanvasItem        *icon_item,
    						      EelIRect                  icon_rect,
    						      gboolean			is_rtl);
static gboolean emblem_layout_next                   (EmblemLayout              *layout,
    						      GdkPixbuf                 **emblem_pixbuf,
    						      EelIRect                  *emblem_rect,
    						      gboolean			is_rtl);
static void     draw_pixbuf                          (GdkPixbuf                 *pixbuf,
#if GTK_CHECK_VERSION(3,0,0)
    						      cairo_t                   *cr,
#else
    						      GdkDrawable               *drawable,
#endif
    						      int                       x,
    						      int                       y);
static PangoLayout *get_label_layout                 (PangoLayout               **layout,
    						      CajaIconCanvasItem        *item,
    						      const char                *text);
#if !GTK_CHECK_VERSION(3,0,0)
static void     draw_label_layout                    (CajaIconCanvasItem        *item,
    						      GdkDrawable               *drawable,
    						      PangoLayout               *layout,
    						      gboolean                  highlight,
    						      GdkColor                  *label_color,
    						      int                       x,
    						      int                       y);
#endif
static gboolean hit_test_stretch_handle              (CajaIconCanvasItem        *item,
    						      EelIRect                  canvas_rect,
    						      GtkCornerType *corner);
static void      draw_embedded_text                  (CajaIconCanvasItem        *icon_item,
#if GTK_CHECK_VERSION(3,0,0)
    						      cairo_t                   *cr,
#else
    						      GdkDrawable               *drawable,
#endif
    						      int                       x,
    						      int                       y);

static void       caja_icon_canvas_item_ensure_bounds_up_to_date (CajaIconCanvasItem *icon_item);


/* Object initialization function for the icon item. */
static void
caja_icon_canvas_item_init (CajaIconCanvasItem *icon_item)
{
    static gboolean setup_auto_enums = FALSE;

    if (!setup_auto_enums)
    {
        eel_g_settings_add_auto_enum
             (caja_preferences,
             CAJA_PREFERENCES_CLICK_POLICY,
             &click_policy_auto_value);
        setup_auto_enums = TRUE;
    }

    icon_item->details = G_TYPE_INSTANCE_GET_PRIVATE ((icon_item), CAJA_TYPE_ICON_CANVAS_ITEM, CajaIconCanvasItemDetails);
    caja_icon_canvas_item_invalidate_label_size (icon_item);
}

static void
caja_icon_canvas_item_finalize (GObject *object)
{
    CajaIconCanvasItemDetails *details;

    g_assert (CAJA_IS_ICON_CANVAS_ITEM (object));

    details = CAJA_ICON_CANVAS_ITEM (object)->details;

    if (details->pixbuf != NULL)
    {
        g_object_unref (details->pixbuf);
    }

    if (details->text_util != NULL)
    {
        g_object_unref (details->text_util);
    }

    g_list_free_full (details->emblem_pixbufs, g_object_unref);
    g_free (details->editable_text);
    g_free (details->additional_text);
    g_free (details->attach_points);

    if (details->rendered_pixbuf != NULL)
    {
        g_object_unref (details->rendered_pixbuf);
    }

    if (details->editable_text_layout != NULL)
    {
        g_object_unref (details->editable_text_layout);
    }

    if (details->additional_text_layout != NULL)
    {
        g_object_unref (details->additional_text_layout);
    }

    if (details->embedded_text_layout != NULL)
    {
        g_object_unref (details->embedded_text_layout);
    }

    g_free (details->embedded_text);

    G_OBJECT_CLASS (caja_icon_canvas_item_parent_class)->finalize (object);
}

/* Currently we require pixbufs in this format (for hit testing).
 * Perhaps gdk-pixbuf will be changed so it can do the hit testing
 * and we won't have this requirement any more.
 */
static gboolean
pixbuf_is_acceptable (GdkPixbuf *pixbuf)
{
    return gdk_pixbuf_get_colorspace (pixbuf) == GDK_COLORSPACE_RGB
           && ((!gdk_pixbuf_get_has_alpha (pixbuf)
                && gdk_pixbuf_get_n_channels (pixbuf) == 3)
               || (gdk_pixbuf_get_has_alpha (pixbuf)
                   && gdk_pixbuf_get_n_channels (pixbuf) == 4))
           && gdk_pixbuf_get_bits_per_sample (pixbuf) == 8;
}

static void
caja_icon_canvas_item_invalidate_bounds_cache (CajaIconCanvasItem *item)
{
    item->details->bounds_cached = FALSE;
}

/* invalidate the text width and height cached in the item details. */
void
caja_icon_canvas_item_invalidate_label_size (CajaIconCanvasItem *item)
{
    if (item->details->editable_text_layout != NULL)
    {
        pango_layout_context_changed (item->details->editable_text_layout);
    }
    if (item->details->additional_text_layout != NULL)
    {
        pango_layout_context_changed (item->details->additional_text_layout);
    }
    if (item->details->embedded_text_layout != NULL)
    {
        pango_layout_context_changed (item->details->embedded_text_layout);
    }
    caja_icon_canvas_item_invalidate_bounds_cache (item);
    item->details->text_width = -1;
    item->details->text_height = -1;
    item->details->text_height_for_layout = -1;
    item->details->text_height_for_entire_text = -1;
    item->details->editable_text_height = -1;
}

/* Set property handler for the icon item. */
static void
caja_icon_canvas_item_set_property (GObject        *object,
                                    guint           property_id,
                                    const GValue   *value,
                                    GParamSpec     *pspec)
{
    CajaIconCanvasItem *item;
    CajaIconCanvasItemDetails *details;

    item = CAJA_ICON_CANVAS_ITEM (object);
    details = item->details;

    switch (property_id)
    {

    case PROP_EDITABLE_TEXT:
        if (g_strcmp0 (details->editable_text,
                        g_value_get_string (value)) == 0)
        {
            return;
        }

        g_free (details->editable_text);
        details->editable_text = g_strdup (g_value_get_string (value));
        if (details->text_util)
        {
            AtkObject *accessible;

            gail_text_util_text_setup (details->text_util,
                                       details->editable_text);
            accessible = atk_gobject_accessible_for_object (G_OBJECT (item));
            g_object_notify (G_OBJECT(accessible), "accessible-name");
        }

        caja_icon_canvas_item_invalidate_label_size (item);
        if (details->editable_text_layout)
        {
            g_object_unref (details->editable_text_layout);
            details->editable_text_layout = NULL;
        }
        break;

    case PROP_ADDITIONAL_TEXT:
        if (g_strcmp0 (details->additional_text,
                        g_value_get_string (value)) == 0)
        {
            return;
        }

        g_free (details->additional_text);
        details->additional_text = g_strdup (g_value_get_string (value));

        caja_icon_canvas_item_invalidate_label_size (item);
        if (details->additional_text_layout)
        {
            g_object_unref (details->additional_text_layout);
            details->additional_text_layout = NULL;
        }
        break;

    case PROP_HIGHLIGHTED_FOR_SELECTION:
        if (!details->is_highlighted_for_selection == !g_value_get_boolean (value))
        {
            return;
        }
        details->is_highlighted_for_selection = g_value_get_boolean (value);
        caja_icon_canvas_item_invalidate_label_size (item);
        break;

    case PROP_HIGHLIGHTED_AS_KEYBOARD_FOCUS:
        if (!details->is_highlighted_as_keyboard_focus == !g_value_get_boolean (value))
        {
            return;
        }
        details->is_highlighted_as_keyboard_focus = g_value_get_boolean (value);

        if (details->is_highlighted_as_keyboard_focus)
        {
            AtkObject *atk_object = atk_gobject_accessible_for_object (object);
            atk_focus_tracker_notify (atk_object);
        }
        break;

    case PROP_HIGHLIGHTED_FOR_DROP:
        if (!details->is_highlighted_for_drop == !g_value_get_boolean (value))
        {
            return;
        }
        details->is_highlighted_for_drop = g_value_get_boolean (value);
        break;

    case PROP_HIGHLIGHTED_FOR_CLIPBOARD:
        if (!details->is_highlighted_for_clipboard == !g_value_get_boolean (value))
        {
            return;
        }
        details->is_highlighted_for_clipboard = g_value_get_boolean (value);
        break;

    default:
        g_warning ("caja_icons_view_item_item_set_arg on unknown argument");
        return;
    }

    eel_canvas_item_request_update (EEL_CANVAS_ITEM (object));
}

/* Get property handler for the icon item */
static void
caja_icon_canvas_item_get_property (GObject        *object,
                                    guint           property_id,
                                    GValue         *value,
                                    GParamSpec     *pspec)
{
    CajaIconCanvasItemDetails *details;

    details = CAJA_ICON_CANVAS_ITEM (object)->details;

    switch (property_id)
    {

    case PROP_EDITABLE_TEXT:
        g_value_set_string (value, details->editable_text);
        break;

    case PROP_ADDITIONAL_TEXT:
        g_value_set_string (value, details->additional_text);
        break;

    case PROP_HIGHLIGHTED_FOR_SELECTION:
        g_value_set_boolean (value, details->is_highlighted_for_selection);
        break;

    case PROP_HIGHLIGHTED_AS_KEYBOARD_FOCUS:
        g_value_set_boolean (value, details->is_highlighted_as_keyboard_focus);
        break;

    case PROP_HIGHLIGHTED_FOR_DROP:
        g_value_set_boolean (value, details->is_highlighted_for_drop);
        break;

    case PROP_HIGHLIGHTED_FOR_CLIPBOARD:
        g_value_set_boolean (value, details->is_highlighted_for_clipboard);
        break;

    default:
        g_warning ("invalid property %d", property_id);
        break;
    }
}

#if GTK_CHECK_VERSION(3,0,0)
cairo_surface_t *
caja_icon_canvas_item_get_drag_surface (CajaIconCanvasItem *item)
#else
GdkPixmap *
caja_icon_canvas_item_get_image (CajaIconCanvasItem *item,
                                 GdkBitmap **mask,
                                 GdkColormap *colormap)
#endif
{
#if GTK_CHECK_VERSION(3,0,0)
    cairo_surface_t *surface;
#else
    GdkPixmap *pixmap;
    GdkPixbuf *pixbuf;
#endif
    EelCanvas *canvas;
    GdkScreen *screen;
    int width, height;
    int item_offset_x, item_offset_y;
    EelIRect icon_rect;
    EelIRect emblem_rect;
    GdkPixbuf *emblem_pixbuf;
    EmblemLayout emblem_layout;
    double item_x, item_y;
    gboolean is_rtl;
    cairo_t *cr;
#if GTK_CHECK_VERSION(3,0,0)
    GtkStyleContext *context;
#endif

    g_return_val_if_fail (CAJA_IS_ICON_CANVAS_ITEM (item), NULL);

    canvas = EEL_CANVAS_ITEM (item)->canvas;
#if GTK_CHECK_VERSION(3,0,0)
    screen = gtk_widget_get_screen (GTK_WIDGET (canvas));
    context = gtk_widget_get_style_context (GTK_WIDGET (canvas));

    gtk_style_context_save (context);
    gtk_style_context_add_class (context, "caja-canvas-item");
#else
    screen = gdk_colormap_get_screen (colormap);
#endif

    /* Assume we're updated so canvas item data is right */

    /* Calculate the offset from the top-left corner of the
       new image to the item position (where the pixmap is placed) */
    eel_canvas_world_to_window (canvas,
                                item->details->x, item->details->y,
                                &item_x, &item_y);

    item_offset_x = item_x - EEL_CANVAS_ITEM (item)->x1;
    item_offset_y = item_y - EEL_CANVAS_ITEM (item)->y1;

    /* Calculate the width of the item */
    width = EEL_CANVAS_ITEM (item)->x2 - EEL_CANVAS_ITEM (item)->x1;
    height = EEL_CANVAS_ITEM (item)->y2 - EEL_CANVAS_ITEM (item)->y1;

#if GTK_CHECK_VERSION(3,0,0)
    surface = gdk_window_create_similar_surface (gdk_screen_get_root_window (screen),
    						 CAIRO_CONTENT_COLOR_ALPHA,
    						 width, height);

    cr = cairo_create (surface);

    gtk_render_icon (context, cr, item->details->pixbuf,
                     item_offset_x, item_offset_y);
#else
    pixmap = gdk_pixmap_new (gdk_screen_get_root_window (screen),
                             width,	height,
                             gdk_visual_get_depth (gdk_colormap_get_visual (colormap)));
    gdk_drawable_set_colormap (pixmap, colormap);

    pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                             TRUE,
                             gdk_pixbuf_get_bits_per_sample (item->details->pixbuf),
                             width, height);
    gdk_pixbuf_fill (pixbuf, 0x00000000);

    gdk_pixbuf_composite (item->details->pixbuf, pixbuf,
                          item_offset_x, item_offset_y,
                          gdk_pixbuf_get_width (item->details->pixbuf),
                          gdk_pixbuf_get_height (item->details->pixbuf),
                          item_offset_x, item_offset_y, 1.0, 1.0,
                          GDK_INTERP_BILINEAR, 255);
#endif

    icon_rect.x0 = item_offset_x;
    icon_rect.y0 = item_offset_y;
    icon_rect.x1 = item_offset_x + gdk_pixbuf_get_width (item->details->pixbuf);
    icon_rect.y1 = item_offset_y + gdk_pixbuf_get_height (item->details->pixbuf);

    is_rtl = caja_icon_container_is_layout_rtl (CAJA_ICON_CONTAINER (canvas));

    emblem_layout_reset (&emblem_layout, item, icon_rect, is_rtl);
#if GTK_CHECK_VERSION(3,0,0)
    while (emblem_layout_next (&emblem_layout, &emblem_pixbuf, &emblem_rect, is_rtl))
    {
        gdk_cairo_set_source_pixbuf (cr, emblem_pixbuf, emblem_rect.x0, emblem_rect.y0);
        cairo_rectangle (cr, emblem_rect.x0, emblem_rect.y0,
                         gdk_pixbuf_get_width (emblem_pixbuf),
                         gdk_pixbuf_get_height (emblem_pixbuf));
        cairo_fill (cr);
    }

    draw_embedded_text (item, cr,
    			item_offset_x, item_offset_y);
    draw_label_text (item, cr, FALSE, icon_rect);
    cairo_destroy (cr);

    gtk_style_context_restore (context);

    return surface;
#else
    while (emblem_layout_next (&emblem_layout, &emblem_pixbuf, &emblem_rect, is_rtl))
    {
        gdk_pixbuf_composite (emblem_pixbuf, pixbuf,
                              emblem_rect.x0, emblem_rect.y0,
                              gdk_pixbuf_get_width (emblem_pixbuf),
                              gdk_pixbuf_get_height (emblem_pixbuf),
                              emblem_rect.x0, emblem_rect.y0,
                              1.0, 1.0,
                              GDK_INTERP_BILINEAR, 255);
    }

    /* draw pixbuf to mask and pixmap */
    cr = gdk_cairo_create (pixmap);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
    cairo_paint (cr);
    cairo_destroy (cr);

    *mask = gdk_pixmap_new (gdk_screen_get_root_window (screen),
                            width, height,
                            1);
    cr = gdk_cairo_create (*mask);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
    cairo_paint (cr);
    cairo_destroy (cr);

    draw_embedded_text (item, pixmap,
                        item_offset_x, item_offset_y);

    draw_label_text (item, pixmap, FALSE, icon_rect);
    draw_label_text (item, *mask, TRUE, icon_rect);

    g_object_unref (pixbuf);

    return pixmap;
#endif
}

void
caja_icon_canvas_item_set_image (CajaIconCanvasItem *item,
                                 GdkPixbuf *image)
{
    CajaIconCanvasItemDetails *details;

    g_return_if_fail (CAJA_IS_ICON_CANVAS_ITEM (item));
    g_return_if_fail (image == NULL || pixbuf_is_acceptable (image));

    details = item->details;
    if (details->pixbuf == image)
    {
        return;
    }

    if (image != NULL)
    {
        g_object_ref (image);
    }
    if (details->pixbuf != NULL)
    {
        g_object_unref (details->pixbuf);
    }
    if (details->rendered_pixbuf != NULL)
    {
        g_object_unref (details->rendered_pixbuf);
        details->rendered_pixbuf = NULL;
    }

    details->pixbuf = image;

    caja_icon_canvas_item_invalidate_bounds_cache (item);
    eel_canvas_item_request_update (EEL_CANVAS_ITEM (item));
}

void
caja_icon_canvas_item_set_emblems (CajaIconCanvasItem *item,
                                   GList *emblem_pixbufs)
{
    GList *p;

    g_return_if_fail (CAJA_IS_ICON_CANVAS_ITEM (item));

    g_assert (item->details->emblem_pixbufs != emblem_pixbufs || emblem_pixbufs == NULL);

    /* The case where the emblems are identical is fairly common,
     * so lets take the time to check for it.
     */
    if (eel_g_list_equal (item->details->emblem_pixbufs, emblem_pixbufs))
    {
        return;
    }

    /* Check if they are acceptable. */
    for (p = emblem_pixbufs; p != NULL; p = p->next)
    {
        g_return_if_fail (pixbuf_is_acceptable (p->data));
    }

    /* Take in the new list of emblems. */
    eel_g_object_list_ref (emblem_pixbufs);
    g_list_free_full (item->details->emblem_pixbufs, g_object_unref);
    item->details->emblem_pixbufs = g_list_copy (emblem_pixbufs);

    caja_icon_canvas_item_invalidate_bounds_cache (item);
    eel_canvas_item_request_update (EEL_CANVAS_ITEM (item));
}

void
caja_icon_canvas_item_set_attach_points (CajaIconCanvasItem *item,
        GdkPoint *attach_points,
        int n_attach_points)
{
    g_free (item->details->attach_points);
    item->details->attach_points = NULL;
    item->details->n_attach_points = 0;

    if (attach_points != NULL && n_attach_points != 0)
    {
        item->details->attach_points = g_memdup (attach_points, n_attach_points * sizeof (GdkPoint));
        item->details->n_attach_points = n_attach_points;
    }

    caja_icon_canvas_item_invalidate_bounds_cache (item);
}

void
caja_icon_canvas_item_set_embedded_text_rect (CajaIconCanvasItem       *item,
        const GdkRectangle           *text_rect)
{
    item->details->embedded_text_rect = *text_rect;

    caja_icon_canvas_item_invalidate_bounds_cache (item);
    eel_canvas_item_request_update (EEL_CANVAS_ITEM (item));
}

void
caja_icon_canvas_item_set_embedded_text (CajaIconCanvasItem       *item,
        const char                   *text)
{
    g_free (item->details->embedded_text);
    item->details->embedded_text = g_strdup (text);

    if (item->details->embedded_text_layout != NULL)
    {
        if (text != NULL)
        {
            pango_layout_set_text (item->details->embedded_text_layout, text, -1);
        }
        else
        {
            pango_layout_set_text (item->details->embedded_text_layout, "", -1);
        }
    }

    eel_canvas_item_request_update (EEL_CANVAS_ITEM (item));
}


/* Recomputes the bounding box of a icon canvas item.
 * This is a generic implementation that could be used for any canvas item
 * class, it has no assumptions about how the item is used.
 */
static void
recompute_bounding_box (CajaIconCanvasItem *icon_item,
                        double i2w_dx, double i2w_dy)
{
    /* The bounds stored in the item is the same as what get_bounds
     * returns, except it's in canvas coordinates instead of the item's
     * parent's coordinates.
     */

    EelCanvasItem *item;
    EelDPoint top_left, bottom_right;

    item = EEL_CANVAS_ITEM (icon_item);

    eel_canvas_item_get_bounds (item,
                                &top_left.x, &top_left.y,
                                &bottom_right.x, &bottom_right.y);

    top_left.x += i2w_dx;
    top_left.y += i2w_dy;
    bottom_right.x += i2w_dx;
    bottom_right.y += i2w_dy;
    eel_canvas_w2c_d (item->canvas,
                      top_left.x, top_left.y,
                      &item->x1, &item->y1);
    eel_canvas_w2c_d (item->canvas,
                      bottom_right.x, bottom_right.y,
                      &item->x2, &item->y2);
}

static EelIRect
compute_text_rectangle (const CajaIconCanvasItem *item,
                        EelIRect icon_rectangle,
                        gboolean canvas_coords,
                        CajaIconCanvasItemBoundsUsage usage)
{
    EelIRect text_rectangle;
    double pixels_per_unit;
    double text_width, text_height, text_height_for_layout, text_height_for_entire_text, real_text_height, text_dx;

    pixels_per_unit = EEL_CANVAS_ITEM (item)->canvas->pixels_per_unit;
    if (canvas_coords)
    {
        text_width = item->details->text_width;
        text_height = item->details->text_height;
        text_height_for_layout = item->details->text_height_for_layout;
        text_height_for_entire_text = item->details->text_height_for_entire_text;
        text_dx = item->details->text_dx;
    }
    else
    {
        text_width = item->details->text_width / pixels_per_unit;
        text_height = item->details->text_height / pixels_per_unit;
        text_height_for_layout = item->details->text_height_for_layout / pixels_per_unit;
        text_height_for_entire_text = item->details->text_height_for_entire_text / pixels_per_unit;
        text_dx = item->details->text_dx / pixels_per_unit;
    }

    if (CAJA_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas)->details->label_position == CAJA_ICON_LABEL_POSITION_BESIDE)
    {
        if (!caja_icon_container_is_layout_rtl (CAJA_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas)))
        {
            text_rectangle.x0 = icon_rectangle.x1;
            text_rectangle.x1 = text_rectangle.x0 + text_dx + text_width;
        }
        else
        {
            text_rectangle.x1 = icon_rectangle.x0;
            text_rectangle.x0 = text_rectangle.x1 - text_dx - text_width;
        }

        /* VOODOO-TODO */
#if 0
        if (for_layout)
        {
            /* in this case, we should be more smart and calculate the size according to the maximum
             * number of lines fitting next to the icon. However, this requires a more complex layout logic.
             * It would mean that when measuring the label, the icon dimensions must be known already,
             * and we
             *   1. start with an unlimited layout
             *   2. measure how many lines of this layout fit next to the icon
             *   3. limit the number of lines to the given number of fitting lines
             */
            real_text_height = VOODOO();
        }
        else
        {
#endif
            real_text_height = text_height_for_entire_text;
#if 0
        }
#endif

        text_rectangle.y0 = (icon_rectangle.y0 + icon_rectangle.y1) / 2- (int) real_text_height / 2;
        text_rectangle.y1 = text_rectangle.y0 + real_text_height;
    }
    else
    {
        text_rectangle.x0 = (icon_rectangle.x0 + icon_rectangle.x1) / 2 - (int) text_width / 2;
        text_rectangle.y0 = icon_rectangle.y1;
        text_rectangle.x1 = text_rectangle.x0 + text_width;

        if (usage == BOUNDS_USAGE_FOR_LAYOUT)
        {
            real_text_height = text_height_for_layout;
        }
        else if (usage == BOUNDS_USAGE_FOR_ENTIRE_ITEM)
        {
            real_text_height = text_height_for_entire_text;
        }
        else if (usage == BOUNDS_USAGE_FOR_DISPLAY)
        {
            real_text_height = text_height;
        }
        else
        {
            g_assert_not_reached ();
        }

        text_rectangle.y1 = text_rectangle.y0 + real_text_height + LABEL_OFFSET / pixels_per_unit;
    }

    return text_rectangle;
}

static EelIRect
get_current_canvas_bounds (EelCanvasItem *item)
{
    EelIRect bounds;

    g_assert (EEL_IS_CANVAS_ITEM (item));

    bounds.x0 = item->x1;
    bounds.y0 = item->y1;
    bounds.x1 = item->x2;
    bounds.y1 = item->y2;

    return bounds;
}

void
caja_icon_canvas_item_update_bounds (CajaIconCanvasItem *item,
                                     double i2w_dx, double i2w_dy)
{
    EelIRect before, after, emblem_rect;
    EmblemLayout emblem_layout;
    EelCanvasItem *canvas_item;
    GdkPixbuf *emblem_pixbuf;
    gboolean is_rtl;

    canvas_item = EEL_CANVAS_ITEM (item);

    /* Compute new bounds. */
    before = get_current_canvas_bounds (canvas_item);
    recompute_bounding_box (item, i2w_dx, i2w_dy);
    after = get_current_canvas_bounds (canvas_item);

    /* If the bounds didn't change, we are done. */
    if (eel_irect_equal (before, after))
    {
        return;
    }

    is_rtl = caja_icon_container_is_layout_rtl (CAJA_ICON_CONTAINER (canvas_item->canvas));

    /* Update canvas and text rect cache */
    get_icon_canvas_rectangle (item, &item->details->canvas_rect);
    item->details->text_rect = compute_text_rectangle (item, item->details->canvas_rect,
                               TRUE, BOUNDS_USAGE_FOR_DISPLAY);

    /* Update emblem rect cache */
    item->details->emblem_rect.x0 = 0;
    item->details->emblem_rect.x1 = 0;
    item->details->emblem_rect.y0 = 0;
    item->details->emblem_rect.y1 = 0;
    emblem_layout_reset (&emblem_layout, item, item->details->canvas_rect, is_rtl);
    while (emblem_layout_next (&emblem_layout, &emblem_pixbuf, &emblem_rect, is_rtl))
    {
        eel_irect_union (&item->details->emblem_rect, &item->details->emblem_rect, &emblem_rect);
    }

    /* queue a redraw. */
    eel_canvas_request_redraw (canvas_item->canvas,
                               before.x0, before.y0,
                               before.x1 + 1, before.y1 + 1);
}

/* Update handler for the icon canvas item. */
static void
caja_icon_canvas_item_update (EelCanvasItem *item,
                              double i2w_dx, double i2w_dy,
                              gint flags)
{
    caja_icon_canvas_item_update_bounds (CAJA_ICON_CANVAS_ITEM (item), i2w_dx, i2w_dy);

    eel_canvas_item_request_redraw (EEL_CANVAS_ITEM (item));

    EEL_CANVAS_ITEM_CLASS (caja_icon_canvas_item_parent_class)->update (item, i2w_dx, i2w_dy, flags);
}

/* Rendering */
static gboolean
in_single_click_mode (void)
{
    return click_policy_auto_value == CAJA_CLICK_POLICY_SINGLE;
}

#if !GTK_CHECK_VERSION(3,0,0)
/* Utility routine to create a rectangle with rounded corners.
 * This could possibly move to Eel as a general purpose routine.
 */
static void
make_round_rect (cairo_t *cr,
                 double x,
                 double y,
                 double width,
                 double height,
                 double radius)
{
    double cx, cy;

    width -= 2 * radius;
    height -= 2 * radius;

    cairo_move_to (cr, x + radius, y);

    cairo_rel_line_to (cr, width, 0.0);

    cairo_get_current_point (cr, &cx, &cy);
    cairo_arc (cr, cx, cy + radius, radius, 3.0 * G_PI_2, 0);

    cairo_rel_line_to (cr, 0.0, height);

    cairo_get_current_point (cr, &cx, &cy);
    cairo_arc (cr, cx - radius, cy, radius, 0, G_PI_2);

    cairo_rel_line_to (cr, - width, 0.0);

    cairo_get_current_point (cr, &cx, &cy);
    cairo_arc (cr, cx, cy - radius, radius, G_PI_2, G_PI);

    cairo_rel_line_to (cr, 0.0, -height);

    cairo_get_current_point (cr, &cx, &cy);
    cairo_arc (cr, cx + radius, cy, radius, G_PI, 3.0 * G_PI_2);

    cairo_close_path (cr);
}

static void
draw_frame (CajaIconCanvasItem *item,
            GdkDrawable *drawable,
            guint color,
            gboolean create_mask,
            int x,
            int y,
            int width,
            int height)
{
    cairo_t *cr = gdk_cairo_create (drawable);

    /* Set the rounded rect clip region. Magic rounding value taken
     * from old code.
     */
    make_round_rect (cr, x, y, width, height, 5);

    if (create_mask)
    {
        /* Dunno how to do this with cairo...
         * It used to threshold the rendering so that the
         * bitmask didn't show white where alpha < 0.5
         */
    }

    cairo_set_source_rgba (cr,
                           EEL_RGBA_COLOR_GET_R (color) / 255.0,
                           EEL_RGBA_COLOR_GET_G (color) / 255.0,
                           EEL_RGBA_COLOR_GET_B (color) / 255.0,
                           EEL_RGBA_COLOR_GET_A (color) / 255.0);

    /* Paint into drawable now that we have set up the color and opacity */
    cairo_fill (cr);

    cairo_destroy (cr);
}
#endif
/* Keep these for a bit while we work on performance of draw_or_measure_label_text. */
/*
  #define PERFORMANCE_TEST_DRAW_DISABLE
  #define PERFORMANCE_TEST_MEASURE_DISABLE
*/

/* This gets the size of the layout from the position of the layout.
 * This means that if the layout is right aligned we get the full width
 * of the layout, not just the width of the text snippet on the right side
 */
static void
layout_get_full_size (PangoLayout *layout,
                      int         *width,
                      int         *height,
                      int         *dx)
{
    PangoRectangle logical_rect;
    int the_width, total_width;

    pango_layout_get_extents (layout, NULL, &logical_rect);
    the_width = (logical_rect.width + PANGO_SCALE / 2) / PANGO_SCALE;
    total_width = (logical_rect.x + logical_rect.width + PANGO_SCALE / 2) / PANGO_SCALE;

    if (width != NULL)
    {
        *width = the_width;
    }

    if (height != NULL)
    {
        *height = (logical_rect.height + PANGO_SCALE / 2) / PANGO_SCALE;
    }

    if (dx != NULL)
    {
        *dx = total_width - the_width;
    }
}

static void
layout_get_size_for_layout (PangoLayout *layout,
                            int          max_layout_line_count,
                            int          height_for_entire_text,
                            int         *height_for_layout)
{
    PangoLayoutIter *iter;
    PangoRectangle logical_rect;
    int i;

    /* only use the first max_layout_line_count lines for the gridded auto layout */
    if (pango_layout_get_line_count (layout) <= max_layout_line_count)
    {
        *height_for_layout = height_for_entire_text;
    }
    else
    {
        *height_for_layout = 0;
        iter = pango_layout_get_iter (layout);
        /* VOODOO-TODO, determine number of lines based on the icon size for text besides icon.
         * cf. compute_text_rectangle() */
        for (i = 0; i < max_layout_line_count; i++)
        {
            pango_layout_iter_get_line_extents (iter, NULL, &logical_rect);
            *height_for_layout += (logical_rect.height + PANGO_SCALE / 2) / PANGO_SCALE;

            if (!pango_layout_iter_next_line (iter))
            {
                break;
            }

            *height_for_layout += pango_layout_get_spacing (layout);
        }
        pango_layout_iter_free (iter);
    }
}

#define IS_COMPACT_VIEW(container) \
        ((container->details->layout_mode == CAJA_ICON_LAYOUT_T_B_L_R || \
	  container->details->layout_mode == CAJA_ICON_LAYOUT_T_B_R_L) && \
	 container->details->label_position == CAJA_ICON_LABEL_POSITION_BESIDE)

#define TEXT_BACK_PADDING_X 4
#define TEXT_BACK_PADDING_Y 1

static void
prepare_pango_layout_width (CajaIconCanvasItem *item,
                            PangoLayout *layout)
{
    if (caja_icon_canvas_item_get_max_text_width (item) < 0)
    {
        pango_layout_set_width (layout, -1);
    }
    else
    {
        pango_layout_set_width (layout, floor (caja_icon_canvas_item_get_max_text_width (item)) * PANGO_SCALE);
        pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);
    }
}

static void
prepare_pango_layout_for_measure_entire_text (CajaIconCanvasItem *item,
        PangoLayout *layout)
{
    CajaIconContainer *container;

    prepare_pango_layout_width (item, layout);

    container = CAJA_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas);

    if (IS_COMPACT_VIEW (container))
    {
        pango_layout_set_height (layout, -1);
    }
    else
    {
        pango_layout_set_height (layout, G_MININT);
    }
}

static void
prepare_pango_layout_for_draw (CajaIconCanvasItem *item,
                               PangoLayout *layout)
{
    CajaIconCanvasItemDetails *details;
    CajaIconContainer *container;
    gboolean needs_highlight;

    prepare_pango_layout_width (item, layout);

    container = CAJA_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas);
    details = item->details;

    needs_highlight = details->is_highlighted_for_selection || details->is_highlighted_for_drop;

    if (IS_COMPACT_VIEW (container))
    {
        pango_layout_set_height (layout, -1);
    }
    else if (needs_highlight ||
             details->is_prelit ||
             details->is_highlighted_as_keyboard_focus ||
             details->entire_text ||
             container->details->label_position == CAJA_ICON_LABEL_POSITION_BESIDE)
    {
        /* VOODOO-TODO, cf. compute_text_rectangle() */
        pango_layout_set_height (layout, G_MININT);
    }
    else
    {
        /* TODO? we might save some resources, when the re-layout is not neccessary in case
         * the layout height already fits into max. layout lines. But pango should figure this
         * out itself (which it doesn't ATM).
         */
        pango_layout_set_height (layout,
                                 caja_icon_container_get_max_layout_lines_for_pango (container));
    }
}

static void
measure_label_text (CajaIconCanvasItem *item)
{
    CajaIconCanvasItemDetails *details;
    CajaIconContainer *container;
    gint editable_height, editable_height_for_layout, editable_height_for_entire_text, editable_width, editable_dx;
    gint additional_height, additional_width, additional_dx;
    PangoLayout *editable_layout;
    PangoLayout *additional_layout;
    gboolean have_editable, have_additional;

    /* check to see if the cached values are still valid; if so, there's
     * no work necessary
     */

    if (item->details->text_width >= 0 && item->details->text_height >= 0)
    {
        return;
    }

    details = item->details;

    have_editable = details->editable_text != NULL && details->editable_text[0] != '\0';
    have_additional = details->additional_text != NULL && details->additional_text[0] != '\0';

    /* No font or no text, then do no work. */
    if (!have_editable && !have_additional)
    {
        details->text_height = 0;
        details->text_height_for_layout = 0;
        details->text_height_for_entire_text = 0;
        details->text_width = 0;
        return;
    }

#ifdef PERFORMANCE_TEST_MEASURE_DISABLE
    /* fake out the width */
    details->text_width = 80;
    details->text_height = 20;
    details->text_height_for_layout = 20;
    details->text_height_for_entire_text = 20;
    return;
#endif

    editable_width = 0;
    editable_height = 0;
    editable_height_for_layout = 0;
    editable_height_for_entire_text = 0;
    editable_dx = 0;
    additional_width = 0;
    additional_height = 0;
    additional_dx = 0;

    container = CAJA_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas);
    editable_layout = NULL;
    additional_layout = NULL;

    if (have_editable)
    {
        /* first, measure required text height: editable_height_for_entire_text
         * then, measure text height applicable for layout: editable_height_for_layout
         * next, measure actually displayed height: editable_height
         */
        editable_layout = get_label_layout (&details->editable_text_layout, item, details->editable_text);

        prepare_pango_layout_for_measure_entire_text (item, editable_layout);
        layout_get_full_size (editable_layout,
                              NULL,
                              &editable_height_for_entire_text,
                              NULL);
        layout_get_size_for_layout (editable_layout,
                                    caja_icon_container_get_max_layout_lines (container),
                                    editable_height_for_entire_text,
                                    &editable_height_for_layout);

        prepare_pango_layout_for_draw (item, editable_layout);
        layout_get_full_size (editable_layout,
                              &editable_width,
                              &editable_height,
                              &editable_dx);
    }

    if (have_additional)
    {
        additional_layout = get_label_layout (&details->additional_text_layout, item, details->additional_text);
        prepare_pango_layout_for_draw (item, additional_layout);
        layout_get_full_size (additional_layout,
                              &additional_width, &additional_height, &additional_dx);
    }

    details->editable_text_height = editable_height;

    if (editable_width > additional_width)
    {
        details->text_width = editable_width;
        details->text_dx = editable_dx;
    }
    else
    {
        details->text_width = additional_width;
        details->text_dx = additional_dx;
    }

    if (have_additional)
    {
        details->text_height = editable_height + LABEL_LINE_SPACING + additional_height;
        details->text_height_for_layout = editable_height_for_layout + LABEL_LINE_SPACING + additional_height;
        details->text_height_for_entire_text = editable_height_for_entire_text + LABEL_LINE_SPACING + additional_height;
    }
    else
    {
        details->text_height = editable_height;
        details->text_height_for_layout = editable_height_for_layout;
        details->text_height_for_entire_text = editable_height_for_entire_text;
    }

    /* add some extra space for highlighting even when we don't highlight so things won't move */

    /* extra slop for nicer highlighting */
    details->text_height += TEXT_BACK_PADDING_Y*2;
    details->text_height_for_layout += TEXT_BACK_PADDING_Y*2;
    details->text_height_for_entire_text += TEXT_BACK_PADDING_Y*2;
    details->editable_text_height += TEXT_BACK_PADDING_Y*2;

    /* extra to make it look nicer */
    details->text_width += TEXT_BACK_PADDING_X*2;

    if (editable_layout)
    {
        g_object_unref (editable_layout);
    }

    if (additional_layout)
    {
        g_object_unref (additional_layout);
    }
}

static void
draw_label_text (CajaIconCanvasItem *item,
#if GTK_CHECK_VERSION(3,0,0)
                 cairo_t *cr,
                 gboolean create_mask,
                 EelIRect icon_rect)
{
    CajaIconCanvasItemDetails *details;
    CajaIconContainer *container;
    PangoLayout *editable_layout;
    PangoLayout *additional_layout;
    GtkStyleContext *context;
    GtkStateFlags state, base_state;
    gboolean have_editable, have_additional;
    gboolean needs_highlight, prelight_label, is_rtl_label_beside;
    EelIRect text_rect;
    int x;
    int max_text_width;
    gdouble frame_w, frame_h, frame_x, frame_y;
    gboolean draw_frame = TRUE;

#ifdef PERFORMANCE_TEST_DRAW_DISABLE
    return;
#endif

    details = item->details;

    measure_label_text (item);
    if (details->text_height == 0 ||
            details->text_width == 0)
    {
        return;
    }

    container = CAJA_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas);
    context = gtk_widget_get_style_context (GTK_WIDGET (container));

    text_rect = compute_text_rectangle (item, icon_rect, TRUE, BOUNDS_USAGE_FOR_DISPLAY);

    needs_highlight = details->is_highlighted_for_selection || details->is_highlighted_for_drop;
    is_rtl_label_beside = caja_icon_container_is_layout_rtl (container) &&
                          container->details->label_position == CAJA_ICON_LABEL_POSITION_BESIDE;

    editable_layout = NULL;
    additional_layout = NULL;

    have_editable = details->editable_text != NULL && details->editable_text[0] != '\0';
    have_additional = details->additional_text != NULL && details->additional_text[0] != '\0';
    g_assert (have_editable || have_additional);

    max_text_width = floor (caja_icon_canvas_item_get_max_text_width (item));

    base_state = gtk_widget_get_state_flags (GTK_WIDGET (container));
    base_state &= ~(GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_PRELIGHT);
    state = base_state;

    gtk_widget_style_get (GTK_WIDGET (container),
                          "activate_prelight_icon_label", &prelight_label,
                          NULL);

    /* if the icon is highlighted, do some set-up */
    if (needs_highlight &&
        !details->is_renaming) {
            state |= GTK_STATE_FLAG_SELECTED;

            frame_x = is_rtl_label_beside ? text_rect.x0 + item->details->text_dx : text_rect.x0;
            frame_y = text_rect.y0;
            frame_w = is_rtl_label_beside ? text_rect.x1 - text_rect.x0 - item->details->text_dx : text_rect.x1 - text_rect.x0;
            frame_h = text_rect.y1 - text_rect.y0;
    } else if (!needs_highlight && have_editable &&
               details->text_width > 0 && details->text_height > 0 &&
               prelight_label && item->details->is_prelit) {
            state |= GTK_STATE_FLAG_PRELIGHT;

            frame_x = text_rect.x0;
            frame_y = text_rect.y0;
            frame_w = text_rect.x1 - text_rect.x0;
            frame_h = text_rect.y1 - text_rect.y0;
    } else {
            draw_frame = FALSE;
    }

    if (draw_frame) {
            gtk_style_context_save (context);
            gtk_style_context_set_state (context, state);

            gtk_render_frame (context, cr,
                              frame_x, frame_y,
                              frame_w, frame_h);
            gtk_render_background (context, cr,
                                   frame_x, frame_y,
                                   frame_w, frame_h);

            gtk_style_context_restore (context);
    }

    if (container->details->label_position == CAJA_ICON_LABEL_POSITION_BESIDE)
    {
        x = text_rect.x0 + 2;
    }
    else
    {
        x = text_rect.x0 + ((text_rect.x1 - text_rect.x0) - max_text_width) / 2;
    }

    if (have_editable &&
        !details->is_renaming)
    {
        state = base_state;

        if (prelight_label && item->details->is_prelit) {
                state |= GTK_STATE_FLAG_PRELIGHT;
        }

        if (needs_highlight) {
                state |= GTK_STATE_FLAG_SELECTED;
        }

        editable_layout = get_label_layout (&item->details->editable_text_layout, item, item->details->editable_text);
        prepare_pango_layout_for_draw (item, editable_layout);

        gtk_style_context_save (context);
        gtk_style_context_set_state (context, state);

        gtk_render_layout (context, cr,
                           x, text_rect.y0 + TEXT_BACK_PADDING_Y,
                           editable_layout);

        gtk_style_context_restore (context);
    }

    if (have_additional &&
        !details->is_renaming)
    {
        state = base_state;

        if (needs_highlight) {
                state |= GTK_STATE_FLAG_SELECTED;
        }

        additional_layout = get_label_layout (&item->details->additional_text_layout, item, item->details->additional_text);
        prepare_pango_layout_for_draw (item, additional_layout);

        gtk_style_context_save (context);
        gtk_style_context_set_state (context, state);
        gtk_style_context_add_class (context, "dim-label");

        gtk_render_layout (context, cr,
                           x, text_rect.y0 + details->editable_text_height + LABEL_LINE_SPACING + TEXT_BACK_PADDING_Y,
                           additional_layout);

        gtk_style_context_restore (context);
    }

    if (!create_mask && item->details->is_highlighted_as_keyboard_focus)
    {
        if (needs_highlight) {
                state = GTK_STATE_FLAG_SELECTED;
        }

        gtk_style_context_save (context);
        gtk_style_context_set_state (context, state);

        gtk_render_focus (context,
                          cr,
                         text_rect.x0,
                         text_rect.y0,
                         text_rect.x1 - text_rect.x0,
                         text_rect.y1 - text_rect.y0);

        gtk_style_context_restore (context);
    }

    if (editable_layout != NULL)
    {
        g_object_unref (editable_layout);
    }

    if (additional_layout != NULL)
    {
        g_object_unref (additional_layout);
    }
}
#else
                 GdkDrawable *drawable,
                 gboolean create_mask,
                 EelIRect icon_rect)
{
    EelCanvasItem *canvas_item;
    CajaIconCanvasItemDetails *details;
    CajaIconContainer *container;
    PangoLayout *editable_layout;
    PangoLayout *additional_layout;
    GdkColor *label_color;
    gboolean have_editable, have_additional;
    gboolean needs_frame, needs_highlight, prelight_label, is_rtl_label_beside;
    EelIRect text_rect;
    int x;
    int max_text_width;

#ifdef PERFORMANCE_TEST_DRAW_DISABLE
    return;
#endif

    details = item->details;

    measure_label_text (item);
    if (details->text_height == 0 ||
            details->text_width == 0)
    {
        return;
    }

    container = CAJA_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas);
    canvas_item = EEL_CANVAS_ITEM (item);

    text_rect = compute_text_rectangle (item, icon_rect, TRUE, BOUNDS_USAGE_FOR_DISPLAY);

    needs_highlight = details->is_highlighted_for_selection || details->is_highlighted_for_drop;
    is_rtl_label_beside = caja_icon_container_is_layout_rtl (container) &&
                          container->details->label_position == CAJA_ICON_LABEL_POSITION_BESIDE;

    editable_layout = NULL;
    additional_layout = NULL;

    have_editable = details->editable_text != NULL && details->editable_text[0] != '\0';
    have_additional = details->additional_text != NULL && details->additional_text[0] != '\0';
    g_assert (have_editable || have_additional);

    max_text_width = floor (caja_icon_canvas_item_get_max_text_width (item));

    if (needs_highlight && !details->is_renaming)
    {
        draw_frame (item,
                    drawable,
                    gtk_widget_has_focus (GTK_WIDGET (container)) ? container->details->highlight_color_rgba : container->details->active_color_rgba,
                    create_mask,
                    is_rtl_label_beside ? text_rect.x0 + item->details->text_dx : text_rect.x0,
                    text_rect.y0,
                    is_rtl_label_beside ? text_rect.x1 - text_rect.x0 - item->details->text_dx : text_rect.x1 - text_rect.x0,
                    text_rect.y1 - text_rect.y0);
    }
    else if (!needs_highlight && !details->is_renaming &&
             (details->is_prelit ||
              details->is_highlighted_as_keyboard_focus))
    {
        /* clear the underlying icons, where the text or overlaps them. */
        gdk_window_clear_area (gtk_layout_get_bin_window (&EEL_CANVAS (container)->layout),
                               text_rect.x0,
                               text_rect.y0,
                               text_rect.x1 - text_rect.x0,
                               text_rect.y1 - text_rect.y0);
    }

    if (container->details->label_position == CAJA_ICON_LABEL_POSITION_BESIDE)
    {
        x = text_rect.x0 + 2;
    }
    else
    {
        x = text_rect.x0 + ((text_rect.x1 - text_rect.x0) - max_text_width) / 2;
    }

    if (have_editable)
    {
        editable_layout = get_label_layout (&item->details->editable_text_layout, item, item->details->editable_text);
        prepare_pango_layout_for_draw (item, editable_layout);

        gtk_widget_style_get (GTK_WIDGET (container),
                              "frame_text", &needs_frame,
                              "activate_prelight_icon_label", &prelight_label,
                              NULL);
        if (needs_frame && !needs_highlight && details->text_width > 0 && details->text_height > 0)
        {
            if (!(prelight_label && item->details->is_prelit))
            {
                draw_frame (item,
                            drawable,
                            container->details->normal_color_rgba,
                            create_mask,
                            text_rect.x0,
                            text_rect.y0,
                            text_rect.x1 - text_rect.x0,
                            text_rect.y1 - text_rect.y0);
            }
            else
            {
                draw_frame (item,
                            drawable,
                            container->details->prelight_color_rgba,
                            create_mask,
                            text_rect.x0,
                            text_rect.y0,
                            text_rect.x1 - text_rect.x0,
                            text_rect.y1 - text_rect.y0);
            }
        }

        caja_icon_container_get_label_color
             (CAJA_ICON_CONTAINER (canvas_item->canvas),
              &label_color, TRUE, needs_highlight,
              prelight_label & item->details->is_prelit);

        draw_label_layout (item,
                           drawable,
                           editable_layout, needs_highlight,
                           label_color,
                           x,
                           text_rect.y0 + TEXT_BACK_PADDING_Y);
    }

    if (have_additional)
    {
        additional_layout = get_label_layout (&item->details->additional_text_layout, item, item->details->additional_text);
        prepare_pango_layout_for_draw (item, additional_layout);

        caja_icon_container_get_label_color
             (CAJA_ICON_CONTAINER (canvas_item->canvas),
              &label_color, FALSE, needs_highlight,
              FALSE);

        draw_label_layout (item,
                           drawable,
                           additional_layout, needs_highlight,
                           label_color,
                           x,
                           text_rect.y0 + details->editable_text_height + LABEL_LINE_SPACING + TEXT_BACK_PADDING_Y);
    }

    if (!create_mask && item->details->is_highlighted_as_keyboard_focus)
    {
        gtk_paint_focus (gtk_widget_get_style (GTK_WIDGET (EEL_CANVAS_ITEM (item)->canvas)),
                         drawable,
                         needs_highlight ? GTK_STATE_SELECTED : GTK_STATE_NORMAL,
                         NULL,
                         GTK_WIDGET (EEL_CANVAS_ITEM (item)->canvas),
                         "icon-container",
                         text_rect.x0,
                         text_rect.y0,
                         text_rect.x1 - text_rect.x0,
                         text_rect.y1 - text_rect.y0);
    }

    if (editable_layout != NULL)
    {
        g_object_unref (editable_layout);
    }

    if (additional_layout != NULL)
    {
        g_object_unref (additional_layout);
    }
}
#endif

void
caja_icon_canvas_item_set_is_visible (CajaIconCanvasItem       *item,
                                      gboolean                      visible)
{
    if (item->details->is_visible == visible)
        return;

    item->details->is_visible = visible;

    if (!visible)
    {
        caja_icon_canvas_item_invalidate_label (item);
    }
}

void
caja_icon_canvas_item_invalidate_label (CajaIconCanvasItem     *item)
{
    caja_icon_canvas_item_invalidate_label_size (item);

    if (item->details->editable_text_layout)
    {
        g_object_unref (item->details->editable_text_layout);
        item->details->editable_text_layout = NULL;
    }

    if (item->details->additional_text_layout)
    {
        g_object_unref (item->details->additional_text_layout);
        item->details->additional_text_layout = NULL;
    }

    if (item->details->embedded_text_layout)
    {
        g_object_unref (item->details->embedded_text_layout);
        item->details->embedded_text_layout = NULL;
    }
}


static GdkPixbuf *
get_knob_pixbuf (void)
{
    GdkPixbuf *knob_pixbuf;
    char *knob_filename;

    knob_pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                            "stock-caja-knob",
                                            8, 0, NULL);
    if (!knob_pixbuf)
    {
        knob_filename = caja_pixmap_file ("knob.png");
        knob_pixbuf = gdk_pixbuf_new_from_file (knob_filename, NULL);
        g_free (knob_filename);
    }

    return knob_pixbuf;
}

static void
draw_stretch_handles (CajaIconCanvasItem *item,
#if GTK_CHECK_VERSION(3,0,0)
                      cairo_t *cr,
#else
                      GdkDrawable *drawable,
#endif
                      const EelIRect *rect)
{
    GtkWidget *widget;
    GdkPixbuf *knob_pixbuf;
    int knob_width, knob_height;
    double dash = { 2.0 };
#if GTK_CHECK_VERSION(3,0,0)
    GtkStyleContext *style;
    GdkRGBA color;
#endif

    if (!item->details->show_stretch_handles)
    {
        return;
    }

    widget = GTK_WIDGET (EEL_CANVAS_ITEM (item)->canvas);
#if GTK_CHECK_VERSION(3,0,0)
    style = gtk_widget_get_style_context (widget);
#endif

#if GTK_CHECK_VERSION(3,0,0)
    cairo_save (cr);
#else
    cairo_t *cr = gdk_cairo_create (drawable);
#endif
    knob_pixbuf = get_knob_pixbuf ();
    knob_width = gdk_pixbuf_get_width (knob_pixbuf);
    knob_height = gdk_pixbuf_get_height (knob_pixbuf);

    /* first draw the box */
#if GTK_CHECK_VERSION(3,0,0)
    gtk_style_context_get_color (style, GTK_STATE_FLAG_SELECTED, &color);
    gdk_cairo_set_source_rgba (cr, &color);
#else
    cairo_set_source_rgb (cr, 0, 0, 0);
#endif
    cairo_set_dash (cr, &dash, 1, 0);
    cairo_set_line_width (cr, 1.0);
    cairo_rectangle (cr,
             rect->x0 + 0.5,
             rect->y0 + 0.5,
             rect->x1 - rect->x0 - 1,
             rect->y1 - rect->y0 - 1);
    cairo_stroke (cr);

#if GTK_CHECK_VERSION(3,0,0)
    cairo_restore (cr);

    /* draw the stretch handles themselves */
    draw_pixbuf (knob_pixbuf, cr, rect->x0, rect->y0);
    draw_pixbuf (knob_pixbuf, cr, rect->x0, rect->y1 - knob_height);
    draw_pixbuf (knob_pixbuf, cr, rect->x1 - knob_width, rect->y0);
    draw_pixbuf (knob_pixbuf, cr, rect->x1 - knob_width, rect->y1 - knob_height);
#else
    cairo_destroy (cr);

    draw_pixbuf (knob_pixbuf, drawable, rect->x0, rect->y0);
    draw_pixbuf (knob_pixbuf, drawable, rect->x0, rect->y1 - knob_height);
    draw_pixbuf (knob_pixbuf, drawable, rect->x1 - knob_width, rect->y0);
    draw_pixbuf (knob_pixbuf, drawable, rect->x1 - knob_width, rect->y1 - knob_height);
#endif

    g_object_unref (knob_pixbuf);
}

static void
emblem_layout_reset (EmblemLayout *layout, CajaIconCanvasItem *icon_item, EelIRect icon_rect, gboolean is_rtl)
{
    layout->icon_item = icon_item;
    layout->icon_rect = icon_rect;
    layout->side = is_rtl ? LEFT_SIDE : RIGHT_SIDE;
    layout->position = 0;
    layout->index = 0;
    layout->emblem = icon_item->details->emblem_pixbufs;
}

static gboolean
emblem_layout_next (EmblemLayout *layout,
                    GdkPixbuf **emblem_pixbuf,
                    EelIRect *emblem_rect,
                    gboolean is_rtl)
{
    GdkPixbuf *pixbuf;
    int width, height, x, y;
    GdkPoint *attach_points;

    /* Check if we have layed out all of the pixbufs. */
    if (layout->emblem == NULL)
    {
        return FALSE;
    }

    /* Get the pixbuf. */
    pixbuf = layout->emblem->data;
    width = gdk_pixbuf_get_width (pixbuf);
    height = gdk_pixbuf_get_height (pixbuf);


    /* Advance to the next emblem. */
    layout->emblem = layout->emblem->next;

    attach_points = layout->icon_item->details->attach_points;
    if (attach_points != NULL)
    {
        if (layout->index >= layout->icon_item->details->n_attach_points)
        {
            return FALSE;
        }

        x = layout->icon_rect.x0 + attach_points[layout->index].x;
        y = layout->icon_rect.y0 + attach_points[layout->index].y;

        layout->index += 1;

        /* Return the rectangle and pixbuf. */
        *emblem_pixbuf = pixbuf;
        emblem_rect->x0 = x - width / 2;
        emblem_rect->y0 = y - height / 2;
        emblem_rect->x1 = emblem_rect->x0 + width;
        emblem_rect->y1 = emblem_rect->y0 + height;

        return TRUE;

    }

    for (;;)
    {

        /* Find the side to lay out along. */
        switch (layout->side)
        {
        case RIGHT_SIDE:
            x = layout->icon_rect.x1;
            y = is_rtl ? layout->icon_rect.y1 : layout->icon_rect.y0;
            break;
        case BOTTOM_SIDE:
            x = is_rtl ? layout->icon_rect.x0 : layout->icon_rect.x1;
            y = layout->icon_rect.y1;
            break;
        case LEFT_SIDE:
            x = layout->icon_rect.x0;
            y = is_rtl ? layout->icon_rect.y0 : layout->icon_rect.y1;
            break;
        case TOP_SIDE:
            x = is_rtl ? layout->icon_rect.x1 : layout->icon_rect.x0;
            y = layout->icon_rect.y0;
            break;
        default:
            g_assert_not_reached ();
            x = 0;
            y = 0;
            break;
        }
        if (layout->position != 0)
        {
            switch (layout->side)
            {
            case RIGHT_SIDE:
                y += (is_rtl ? -1 : 1) * (layout->position + height / 2);
                break;
            case BOTTOM_SIDE:
                x += (is_rtl ? 1 : -1 ) * (layout->position + width / 2);
                break;
            case LEFT_SIDE:
                y += (is_rtl ? 1 : -1) * (layout->position + height / 2);
                break;
            case TOP_SIDE:
                x += (is_rtl ? -1 : 1) * (layout->position + width / 2);
                break;
            }
        }

        /* Check to see if emblem fits in current side. */
        if (x >= layout->icon_rect.x0 && x <= layout->icon_rect.x1
                && y >= layout->icon_rect.y0 && y <= layout->icon_rect.y1)
        {

            /* It fits. */

            /* Advance along the side. */
            switch (layout->side)
            {
            case RIGHT_SIDE:
            case LEFT_SIDE:
                layout->position += height + EMBLEM_SPACING;
                break;
            case BOTTOM_SIDE:
            case TOP_SIDE:
                layout->position += width + EMBLEM_SPACING;
                break;
            }

            /* Return the rectangle and pixbuf. */
            *emblem_pixbuf = pixbuf;
            emblem_rect->x0 = x - width / 2;
            emblem_rect->y0 = y - height / 2;
            emblem_rect->x1 = emblem_rect->x0 + width;
            emblem_rect->y1 = emblem_rect->y0 + height;

            return TRUE;
        }

        /* It doesn't fit, so move to the next side. */
        switch (layout->side)
        {
        case RIGHT_SIDE:
            layout->side = is_rtl ? TOP_SIDE : BOTTOM_SIDE;
            break;
        case BOTTOM_SIDE:
            layout->side = is_rtl ? RIGHT_SIDE : LEFT_SIDE;
            break;
        case LEFT_SIDE:
            layout->side = is_rtl ? BOTTOM_SIDE : TOP_SIDE;
            break;
        case TOP_SIDE:
        default:
            return FALSE;
        }
        layout->position = 0;
    }
}

static void
#if GTK_CHECK_VERSION(3,0,0)
draw_pixbuf (GdkPixbuf *pixbuf,
             cairo_t *cr,
             int x, int y)
{
    cairo_save (cr);
    gdk_cairo_set_source_pixbuf (cr, pixbuf, x, y);
    cairo_paint (cr);
    cairo_restore (cr);
}
#else
draw_pixbuf (GdkPixbuf *pixbuf, GdkDrawable *drawable, int x, int y)
{
    cairo_t *cr = gdk_cairo_create (drawable);
    gdk_cairo_set_source_pixbuf (cr, pixbuf, x, y);
    cairo_paint (cr);
    cairo_destroy (cr);
}
#endif

/* shared code to highlight or dim the passed-in pixbuf */
static GdkPixbuf *
real_map_pixbuf (CajaIconCanvasItem *icon_item)
{
    EelCanvas *canvas;
    char *audio_filename;
    CajaIconContainer *container;
    GdkPixbuf *temp_pixbuf, *old_pixbuf, *audio_pixbuf;
    int emblem_size;
#if GTK_CHECK_VERSION(3,0,0)
    GtkStyleContext *style;
    GdkRGBA color;
#else
    guint render_mode, saturation, brightness, lighten;
#endif

    temp_pixbuf = icon_item->details->pixbuf;
    canvas = EEL_CANVAS_ITEM(icon_item)->canvas;
    container = CAJA_ICON_CONTAINER (canvas);

    g_object_ref (temp_pixbuf);

    if (icon_item->details->is_prelit ||
            icon_item->details->is_highlighted_for_clipboard)
    {
        old_pixbuf = temp_pixbuf;

#if GTK_CHECK_VERSION(3,0,0)
        temp_pixbuf = eel_create_spotlight_pixbuf (temp_pixbuf);
        g_object_unref (old_pixbuf);
#else
        gtk_widget_style_get (GTK_WIDGET (container),
                              "prelight_icon_render_mode", &render_mode,
                              "prelight_icon_saturation", &saturation,
                              "prelight_icon_brightness", &brightness,
                              "prelight_icon_lighten", &lighten,
                              NULL);

        if (render_mode > 0 || saturation < 255 || brightness < 255)
        {
            temp_pixbuf = eel_gdk_pixbuf_render (temp_pixbuf,
                                                 render_mode,
                                                 saturation,
                                                 brightness,
                                                 lighten,
                                                 container->details->prelight_icon_color_rgba);
            g_object_unref (old_pixbuf);
        }
#endif

        /* FIXME bugzilla.gnome.org 42471: This hard-wired image is inappropriate to
         * this level of code, which shouldn't know that the
         * preview is audio, nor should it have an icon
         * hard-wired in.
         */

        /* if the icon is currently being previewed, superimpose an image to indicate that */
        /* audio is the only kind of previewing right now, so this code isn't as general as it could be */
        if (icon_item->details->is_active)
        {
            emblem_size = caja_icon_get_emblem_size_for_icon_size (gdk_pixbuf_get_width (temp_pixbuf));
            /* Load the audio symbol. */
            audio_filename = caja_pixmap_file ("audio.svg");
            if (audio_filename != NULL)
            {
                audio_pixbuf = gdk_pixbuf_new_from_file_at_scale (audio_filename,
                               emblem_size, emblem_size,
                               TRUE,
                               NULL);
            }
            else
            {
                audio_pixbuf = NULL;
            }

            /* Composite it onto the icon. */
            if (audio_pixbuf != NULL)
            {
                gdk_pixbuf_composite
                (audio_pixbuf,
                 temp_pixbuf,
                 0, 0,
                 gdk_pixbuf_get_width (audio_pixbuf),
                 gdk_pixbuf_get_height (audio_pixbuf),
                 0, 0,
                 1.0, 1.0,
                 GDK_INTERP_BILINEAR, 0xFF);

                g_object_unref (audio_pixbuf);
            }

            g_free (audio_filename);
        }
    }

    if (icon_item->details->is_highlighted_for_selection
            || icon_item->details->is_highlighted_for_drop)
    {
#if GTK_CHECK_VERSION(3,0,0)
        style = gtk_widget_get_style_context (GTK_WIDGET (canvas));

        if (gtk_widget_has_focus (GTK_WIDGET (canvas))) {
                gtk_style_context_get_background_color (style, GTK_STATE_FLAG_SELECTED, &color);
        } else {
                gtk_style_context_get_background_color (style, GTK_STATE_FLAG_ACTIVE, &color);
        }

        old_pixbuf = temp_pixbuf;
        temp_pixbuf = eel_create_colorized_pixbuf (temp_pixbuf, &color);
#else
        guint color;

        old_pixbuf = temp_pixbuf;

        color =  gtk_widget_has_focus (GTK_WIDGET (canvas)) ? CAJA_ICON_CONTAINER (canvas)->details->highlight_color_rgba : CAJA_ICON_CONTAINER (canvas)->details->active_color_rgba;

        temp_pixbuf = eel_create_colorized_pixbuf (temp_pixbuf,
                      EEL_RGBA_COLOR_GET_R (color),
                      EEL_RGBA_COLOR_GET_G (color),
                      EEL_RGBA_COLOR_GET_B (color));
#endif

        g_object_unref (old_pixbuf);
    }

#if !GTK_CHECK_VERSION(3,0,0)
    if (!icon_item->details->is_active
            && !icon_item->details->is_prelit
            && !icon_item->details->is_highlighted_for_selection
            && !icon_item->details->is_highlighted_for_drop)
    {
        old_pixbuf = temp_pixbuf;

        gtk_widget_style_get (GTK_WIDGET (container),
                              "normal_icon_render_mode", &render_mode,
                              "normal_icon_saturation", &saturation,
                              "normal_icon_brightness", &brightness,
                              "normal_icon_lighten", &lighten,
                              NULL);
        if (render_mode > 0 || saturation < 255 || brightness < 255)
        {
            /* if theme requests colorization */
            temp_pixbuf = eel_gdk_pixbuf_render (temp_pixbuf,
                                                 render_mode,
                                                 saturation,
                                                 brightness,
                                                 lighten,
                                                 container->details->normal_icon_color_rgba);
            g_object_unref (old_pixbuf);
        }
    }
#endif

    return temp_pixbuf;
}

static GdkPixbuf *
map_pixbuf (CajaIconCanvasItem *icon_item)
{
    if (!(icon_item->details->rendered_pixbuf != NULL
            && icon_item->details->rendered_is_active == icon_item->details->is_active
            && icon_item->details->rendered_is_prelit == icon_item->details->is_prelit
            && icon_item->details->rendered_is_highlighted_for_selection == icon_item->details->is_highlighted_for_selection
            && icon_item->details->rendered_is_highlighted_for_drop == icon_item->details->is_highlighted_for_drop
            && icon_item->details->rendered_is_highlighted_for_clipboard == icon_item->details->is_highlighted_for_clipboard
            && (icon_item->details->is_highlighted_for_selection && icon_item->details->rendered_is_focused == gtk_widget_has_focus (GTK_WIDGET (EEL_CANVAS_ITEM (icon_item)->canvas)))))
    {
        if (icon_item->details->rendered_pixbuf != NULL)
        {
            g_object_unref (icon_item->details->rendered_pixbuf);
        }
        icon_item->details->rendered_pixbuf = real_map_pixbuf (icon_item);
        icon_item->details->rendered_is_active = icon_item->details->is_active;
        icon_item->details->rendered_is_prelit = icon_item->details->is_prelit;
        icon_item->details->rendered_is_highlighted_for_selection = icon_item->details->is_highlighted_for_selection;
        icon_item->details->rendered_is_highlighted_for_drop = icon_item->details->is_highlighted_for_drop;
        icon_item->details->rendered_is_highlighted_for_clipboard = icon_item->details->is_highlighted_for_clipboard;
        icon_item->details->rendered_is_focused = gtk_widget_has_focus (GTK_WIDGET (EEL_CANVAS_ITEM (icon_item)->canvas));
    }

    g_object_ref (icon_item->details->rendered_pixbuf);

    return icon_item->details->rendered_pixbuf;
}

static void
draw_embedded_text (CajaIconCanvasItem *item,
#if GTK_CHECK_VERSION(3,0,0)
                    cairo_t *cr,
#else
                    GdkDrawable *drawable,
#endif
                    int x, int y)
{
    PangoLayout *layout;
    PangoContext *context;
    PangoFontDescription *desc;
#if GTK_CHECK_VERSION(3,0,0)
    GtkWidget *widget;
    GtkStyleContext *style_context;
#endif

    if (item->details->embedded_text == NULL ||
            item->details->embedded_text_rect.width == 0 ||
            item->details->embedded_text_rect.height == 0)
    {
        return;
    }

#if GTK_CHECK_VERSION(3,0,0)
    widget = GTK_WIDGET (EEL_CANVAS_ITEM (item)->canvas);
#endif

    if (item->details->embedded_text_layout != NULL)
    {
        layout = g_object_ref (item->details->embedded_text_layout);
    }
    else
    {
#if GTK_CHECK_VERSION(3,0,0)
        context = gtk_widget_get_pango_context (widget);
#else
        context = gtk_widget_get_pango_context (GTK_WIDGET (EEL_CANVAS_ITEM (item)->canvas));
#endif
        layout = pango_layout_new (context);
        pango_layout_set_text (layout, item->details->embedded_text, -1);

        desc = pango_font_description_from_string ("monospace 6");
        pango_layout_set_font_description (layout, desc);
        pango_font_description_free (desc);

        if (item->details->is_visible)
        {
            item->details->embedded_text_layout = g_object_ref (layout);
        }
    }

#if GTK_CHECK_VERSION(3,0,0)
    style_context = gtk_widget_get_style_context (widget);
    gtk_style_context_save (style_context);
    gtk_style_context_add_class (style_context, "icon-embedded-text");

    cairo_save (cr);

    cairo_rectangle (cr,
                     x + item->details->embedded_text_rect.x,
                     y + item->details->embedded_text_rect.y,
                     item->details->embedded_text_rect.width,
                     item->details->embedded_text_rect.height);
    cairo_clip (cr);

    gtk_render_layout (style_context, cr,
                       x + item->details->embedded_text_rect.x,
                       y + item->details->embedded_text_rect.y,
                       layout);

    gtk_style_context_restore (style_context);
    cairo_restore (cr);
#else
    cairo_t *cr = gdk_cairo_create (drawable);

    cairo_rectangle (cr,
                     x + item->details->embedded_text_rect.x,
                     y + item->details->embedded_text_rect.y,
                     item->details->embedded_text_rect.width,
                     item->details->embedded_text_rect.height);
    cairo_clip (cr);

    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_move_to (cr, 
    	           x + item->details->embedded_text_rect.x,
    	           y + item->details->embedded_text_rect.y);
    pango_cairo_show_layout (cr, layout);

    cairo_destroy (cr);
#endif
}

/* Draw the icon item for non-anti-aliased mode. */
static void
#if GTK_CHECK_VERSION(3,0,0)
caja_icon_canvas_item_draw (EelCanvasItem *item,
                            cairo_t *cr,
                            cairo_region_t *region)
{
    CajaIconContainer *container;
#else
caja_icon_canvas_item_draw (EelCanvasItem *item, GdkDrawable *drawable,
                            GdkEventExpose *expose)
{
#endif
    CajaIconCanvasItem *icon_item;
    CajaIconCanvasItemDetails *details;
    EelIRect icon_rect, emblem_rect;
    EmblemLayout emblem_layout;
    GdkPixbuf *emblem_pixbuf, *temp_pixbuf;
#if GTK_CHECK_VERSION(3,0,0)
    GtkStyleContext *context;

    container = CAJA_ICON_CONTAINER (item->canvas);
    gboolean is_rtl;
#else
    GdkRectangle pixbuf_rect;
    gboolean is_rtl;

#endif
    icon_item = CAJA_ICON_CANVAS_ITEM (item);
    details = icon_item->details;

    /* Draw the pixbuf. */
    if (details->pixbuf == NULL)
    {
        return;
    }

#if GTK_CHECK_VERSION(3,0,0)
    context = gtk_widget_get_style_context (GTK_WIDGET (container));
    gtk_style_context_save (context);
    gtk_style_context_add_class (context, "caja-canvas-item");

#endif
    icon_rect = icon_item->details->canvas_rect;
#if GTK_CHECK_VERSION(3,0,0)
    temp_pixbuf = map_pixbuf (icon_item);

    gtk_render_icon (context, cr,
                     temp_pixbuf,
                     icon_rect.x0, icon_rect.y0);
    g_object_unref (temp_pixbuf);

    draw_embedded_text (icon_item, cr, icon_rect.x0, icon_rect.y0);
#else
    /* if the pre-lit or selection flag is set, make a pre-lit or darkened pixbuf and draw that instead */
    /* and colorize normal pixbuf if rc wants that */
    temp_pixbuf = map_pixbuf (icon_item);
    pixbuf_rect.x = icon_rect.x0;
    pixbuf_rect.y = icon_rect.y0;
    pixbuf_rect.width = gdk_pixbuf_get_width (temp_pixbuf);
    pixbuf_rect.height = gdk_pixbuf_get_height (temp_pixbuf);

    cairo_t *cr = gdk_cairo_create (drawable);
    gdk_cairo_rectangle (cr, &expose->area);
    cairo_clip (cr);
    gdk_cairo_set_source_pixbuf (cr, temp_pixbuf, pixbuf_rect.x, pixbuf_rect.y);
    gdk_cairo_rectangle (cr, &pixbuf_rect);
    cairo_fill (cr);
    cairo_destroy (cr);
    g_object_unref (temp_pixbuf);

    draw_embedded_text (icon_item, drawable, icon_rect.x0, icon_rect.y0);
#endif

    is_rtl = caja_icon_container_is_layout_rtl (CAJA_ICON_CONTAINER (item->canvas));

    /* Draw the emblem pixbufs. */
    emblem_layout_reset (&emblem_layout, icon_item, icon_rect, is_rtl);
    while (emblem_layout_next (&emblem_layout, &emblem_pixbuf, &emblem_rect, is_rtl))
    {
#if GTK_CHECK_VERSION(3,0,0)
        draw_pixbuf (emblem_pixbuf, cr, emblem_rect.x0, emblem_rect.y0);
    }

    /* Draw stretching handles (if necessary). */
    draw_stretch_handles (icon_item, cr, &icon_rect);

    /* Draw the label text. */
    draw_label_text (icon_item, cr, FALSE, icon_rect);

    gtk_style_context_restore (context);
#else
        draw_pixbuf (emblem_pixbuf, drawable, emblem_rect.x0, emblem_rect.y0);
    }

    draw_stretch_handles (icon_item, drawable, &icon_rect);
    draw_label_text (icon_item, drawable, FALSE, icon_rect);
#endif
}

#define ZERO_WIDTH_SPACE "\xE2\x80\x8B"

#define ZERO_OR_THREE_DIGITS(p) \
	(!g_ascii_isdigit (*p) || \
	 (g_ascii_isdigit (*(p+1)) && \
	  g_ascii_isdigit (*(p+2))))


static PangoLayout *
create_label_layout (CajaIconCanvasItem *item,
                     const char *text)
{
    PangoLayout *layout;
    PangoContext *context;
    PangoFontDescription *desc;
    CajaIconContainer *container;
    EelCanvasItem *canvas_item;
    GString *str;
    char *zeroified_text;
    const char *p;

    canvas_item = EEL_CANVAS_ITEM (item);

    container = CAJA_ICON_CONTAINER (canvas_item->canvas);
    context = gtk_widget_get_pango_context (GTK_WIDGET (canvas_item->canvas));
    layout = pango_layout_new (context);

    zeroified_text = NULL;

    if (text != NULL)
    {
        str = g_string_new (NULL);

        for (p = text; *p != '\0'; p++)
        {
            str = g_string_append_c (str, *p);

            if (*p == '_' || *p == '-' || (*p == '.' && ZERO_OR_THREE_DIGITS (p+1)))
            {
                /* Ensure that we allow to break after '_' or '.' characters,
                 * if they are not likely to be part of a version information, to
                 * not break wrapping of foobar-0.0.1.
                 * Wrap before IPs and long numbers, though. */
                str = g_string_append (str, ZERO_WIDTH_SPACE);
            }
        }

        zeroified_text = g_string_free (str, FALSE);
    }

    pango_layout_set_text (layout, zeroified_text, -1);
    pango_layout_set_auto_dir (layout, FALSE);

    if (container->details->label_position == CAJA_ICON_LABEL_POSITION_BESIDE)
    {
        if (!caja_icon_container_is_layout_rtl (container))
        {
            pango_layout_set_alignment (layout, PANGO_ALIGN_LEFT);
        }
        else
        {
            pango_layout_set_alignment (layout, PANGO_ALIGN_RIGHT);
        }
    }
    else
    {
        pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
    }

    pango_layout_set_spacing (layout, LABEL_LINE_SPACING);
    pango_layout_set_wrap (layout, PANGO_WRAP_WORD_CHAR);

    /* Create a font description */
    if (container->details->font)
    {
        desc = pango_font_description_from_string (container->details->font);
    }
    else
    {
        desc = pango_font_description_copy (pango_context_get_font_description (context));
        pango_font_description_set_size (desc,
                                         pango_font_description_get_size (desc) +
                                         container->details->font_size_table [container->details->zoom_level]);
    }
    pango_layout_set_font_description (layout, desc);
    pango_font_description_free (desc);
    g_free (zeroified_text);

    return layout;
}

static PangoLayout *
get_label_layout (PangoLayout **layout_cache,
                  CajaIconCanvasItem *item,
                  const char *text)
{
    PangoLayout *layout;

    if (*layout_cache != NULL)
    {
        return g_object_ref (*layout_cache);
    }

    layout = create_label_layout (item, text);

    if (item->details->is_visible)
    {
        *layout_cache = g_object_ref (layout);
    }

    return layout;
}

#if !GTK_CHECK_VERSION(3,0,0)
static void
draw_label_layout (CajaIconCanvasItem *item,
                   GdkDrawable *drawable,
                   PangoLayout *layout,
                   gboolean highlight,
                   GdkColor *label_color,
                   int x,
                   int y)
{
    g_return_if_fail (drawable != NULL);

    if (item->details->is_renaming)
    {
        return;
    }

    if (!highlight && (CAJA_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas)->details->use_drop_shadows))
    {
        /* draw a drop shadow */
        eel_gdk_draw_layout_with_drop_shadow (drawable,
                                              label_color,
                                              &gtk_widget_get_style (GTK_WIDGET (EEL_CANVAS_ITEM (item)->canvas))->black,
                                              x, y,
                                              layout);
    }
    else
    {
        cairo_t *cr = gdk_cairo_create (drawable);
        gdk_cairo_set_source_color (cr, label_color);
        cairo_move_to (cr, x, y);
        pango_cairo_show_layout (cr, layout);
        cairo_destroy (cr);
    }
}
#endif

/* handle events */

static int
caja_icon_canvas_item_event (EelCanvasItem *item, GdkEvent *event)
{
    CajaIconCanvasItem *icon_item;
    GdkCursor *cursor;

    icon_item = CAJA_ICON_CANVAS_ITEM (item);

    switch (event->type)
    {
    case GDK_ENTER_NOTIFY:
        if (!icon_item->details->is_prelit)
        {
            icon_item->details->is_prelit = TRUE;
            caja_icon_canvas_item_invalidate_label_size (icon_item);
            eel_canvas_item_request_update (item);
            eel_canvas_item_send_behind (item,
                                         CAJA_ICON_CONTAINER (item->canvas)->details->rubberband_info.selection_rectangle);

            /* show a hand cursor */
            if (in_single_click_mode ())
            {
                cursor = gdk_cursor_new_for_display (gdk_display_get_default(),
                                                     GDK_HAND2);
                gdk_window_set_cursor (((GdkEventAny *)event)->window, cursor);
#if GTK_CHECK_VERSION(3,0,0)
                g_object_unref (cursor);
#else
                gdk_cursor_unref (cursor);
#endif
            }

            /* FIXME bugzilla.gnome.org 42473:
             * We should emit our own signal here,
             * not one from the container; it could hook
             * up to that signal and emit one of its
             * own. Doing it this way hard-codes what
             * "user_data" is. Also, the two signals
             * should be separate. The "unpreview" signal
             * does not have a return value.
             */
            icon_item->details->is_active = caja_icon_container_emit_preview_signal
                                            (CAJA_ICON_CONTAINER (item->canvas),
                                             CAJA_ICON_CANVAS_ITEM (item)->user_data,
                                             TRUE);
        }
        return TRUE;

    case GDK_LEAVE_NOTIFY:
        if (icon_item->details->is_prelit
                || icon_item->details->is_highlighted_for_drop)
        {
            /* When leaving, turn of the prelight state and the
             * higlighted for drop. The latter gets turned on
             * by the drag&drop motion callback.
             */
            /* FIXME bugzilla.gnome.org 42473:
             * We should emit our own signal here,
             * not one from the containe; it could hook up
             * to that signal and emit one of its
             * ownr. Doing it this way hard-codes what
             * "user_data" is. Also, the two signals
             * should be separate. The "unpreview" signal
             * does not have a return value.
             */
            caja_icon_container_emit_preview_signal
            (CAJA_ICON_CONTAINER (item->canvas),
             CAJA_ICON_CANVAS_ITEM (item)->user_data,
             FALSE);
            icon_item->details->is_prelit = FALSE;
            icon_item->details->is_active = 0;
            icon_item->details->is_highlighted_for_drop = FALSE;
            caja_icon_canvas_item_invalidate_label_size (icon_item);
            eel_canvas_item_request_update (item);

            /* show default cursor */
            gdk_window_set_cursor (((GdkEventAny *)event)->window, NULL);
        }
        return TRUE;

    default:
        /* Don't eat up other events; icon container might use them. */
        return FALSE;
    }
}

static gboolean
hit_test_pixbuf (GdkPixbuf *pixbuf, EelIRect pixbuf_location, EelIRect probe_rect)
{
    EelIRect relative_rect, pixbuf_rect;
    int x, y;
    guint8 *pixel;

    /* You can get here without a pixbuf in some strange cases. */
    if (pixbuf == NULL)
    {
        return FALSE;
    }

    /* Check to see if it's within the rectangle at all. */
    relative_rect.x0 = probe_rect.x0 - pixbuf_location.x0;
    relative_rect.y0 = probe_rect.y0 - pixbuf_location.y0;
    relative_rect.x1 = probe_rect.x1 - pixbuf_location.x0;
    relative_rect.y1 = probe_rect.y1 - pixbuf_location.y0;
    pixbuf_rect.x0 = 0;
    pixbuf_rect.y0 = 0;
    pixbuf_rect.x1 = gdk_pixbuf_get_width (pixbuf);
    pixbuf_rect.y1 = gdk_pixbuf_get_height (pixbuf);
    eel_irect_intersect (&relative_rect, &relative_rect, &pixbuf_rect);
    if (eel_irect_is_empty (&relative_rect))
    {
        return FALSE;
    }

    /* If there's no alpha channel, it's opaque and we have a hit. */
    if (!gdk_pixbuf_get_has_alpha (pixbuf))
    {
        return TRUE;
    }
    g_assert (gdk_pixbuf_get_n_channels (pixbuf) == 4);

    /* Check the alpha channel of the pixel to see if we have a hit. */
    for (x = relative_rect.x0; x < relative_rect.x1; x++)
    {
        for (y = relative_rect.y0; y < relative_rect.y1; y++)
        {
            pixel = gdk_pixbuf_get_pixels (pixbuf)
                    + y * gdk_pixbuf_get_rowstride (pixbuf)
                    + x * 4;
            if (pixel[3] > 1)
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

static gboolean
hit_test (CajaIconCanvasItem *icon_item, EelIRect canvas_rect)
{
    CajaIconCanvasItemDetails *details;
    EelIRect emblem_rect;
    EmblemLayout emblem_layout;
    GdkPixbuf *emblem_pixbuf;
    gboolean is_rtl;

    details = icon_item->details;

    /* Quick check to see if the rect hits the icon, text or emblems at all. */
    if (!eel_irect_hits_irect (icon_item->details->canvas_rect, canvas_rect)
            && (!eel_irect_hits_irect (details->text_rect, canvas_rect))
            && (!eel_irect_hits_irect (details->emblem_rect, canvas_rect)))
    {
        return FALSE;
    }

    /* Check for hits in the stretch handles. */
    if (hit_test_stretch_handle (icon_item, canvas_rect, NULL))
    {
        return TRUE;
    }

    /* Check for hit in the icon. */
    if (eel_irect_hits_irect (icon_item->details->canvas_rect, canvas_rect))
    {
        return TRUE;
    }

    /* Check for hit in the text. */
    if (eel_irect_hits_irect (details->text_rect, canvas_rect)
            && !icon_item->details->is_renaming)
    {
        return TRUE;
    }

    is_rtl = caja_icon_container_is_layout_rtl (CAJA_ICON_CONTAINER (EEL_CANVAS_ITEM (icon_item)->canvas));

    /* Check for hit in the emblem pixbufs. */
    emblem_layout_reset (&emblem_layout, icon_item, icon_item->details->canvas_rect, is_rtl);
    while (emblem_layout_next (&emblem_layout, &emblem_pixbuf, &emblem_rect, is_rtl))
    {
        if (hit_test_pixbuf (emblem_pixbuf, emblem_rect, canvas_rect))
        {
            return TRUE;
        }
    }

    return FALSE;
}

/* Point handler for the icon canvas item. */
static double
caja_icon_canvas_item_point (EelCanvasItem *item, double x, double y, int cx, int cy,
                             EelCanvasItem **actual_item)
{
    EelIRect canvas_rect;

    *actual_item = item;
    canvas_rect.x0 = cx;
    canvas_rect.y0 = cy;
    canvas_rect.x1 = cx + 1;
    canvas_rect.y1 = cy + 1;
    if (hit_test (CAJA_ICON_CANVAS_ITEM (item), canvas_rect))
    {
        return 0.0;
    }
    else
    {
        /* This value means not hit.
         * It's kind of arbitrary. Can we do better?
         */
        return item->canvas->pixels_per_unit * 2 + 10;
    }
}

static void
caja_icon_canvas_item_translate (EelCanvasItem *item, double dx, double dy)
{
    CajaIconCanvasItem *icon_item;
    CajaIconCanvasItemDetails *details;

    icon_item = CAJA_ICON_CANVAS_ITEM (item);
    details = icon_item->details;

    details->x += dx;
    details->y += dy;
}

void
caja_icon_canvas_item_get_bounds_for_layout (CajaIconCanvasItem *icon_item,
        double *x1, double *y1, double *x2, double *y2)
{
    CajaIconCanvasItemDetails *details;
    EelIRect *total_rect;

    details = icon_item->details;

    caja_icon_canvas_item_ensure_bounds_up_to_date (icon_item);
    g_assert (details->bounds_cached);

    total_rect = &details->bounds_cache_for_layout;

    /* Return the result. */
    if (x1 != NULL)
    {
        *x1 = (int)details->x + total_rect->x0;
    }
    if (y1 != NULL)
    {
        *y1 = (int)details->y + total_rect->y0;
    }
    if (x2 != NULL)
    {
        *x2 = (int)details->x + total_rect->x1 + 1;
    }
    if (y2 != NULL)
    {
        *y2 = (int)details->y + total_rect->y1 + 1;
    }
}

void
caja_icon_canvas_item_get_bounds_for_entire_item (CajaIconCanvasItem *icon_item,
        double *x1, double *y1, double *x2, double *y2)
{
    CajaIconCanvasItemDetails *details;
    EelIRect *total_rect;

    details = icon_item->details;

    caja_icon_canvas_item_ensure_bounds_up_to_date (icon_item);
    g_assert (details->bounds_cached);

    total_rect = &details->bounds_cache_for_entire_item;

    /* Return the result. */
    if (x1 != NULL)
    {
        *x1 = (int)details->x + total_rect->x0;
    }
    if (y1 != NULL)
    {
        *y1 = (int)details->y + total_rect->y0;
    }
    if (x2 != NULL)
    {
        *x2 = (int)details->x + total_rect->x1 + 1;
    }
    if (y2 != NULL)
    {
        *y2 = (int)details->y + total_rect->y1 + 1;
    }
}

/* Bounds handler for the icon canvas item. */
static void
caja_icon_canvas_item_bounds (EelCanvasItem *item,
                              double *x1, double *y1, double *x2, double *y2)
{
    CajaIconCanvasItem *icon_item;
    CajaIconCanvasItemDetails *details;
    EelIRect *total_rect;

    icon_item = CAJA_ICON_CANVAS_ITEM (item);
    details = icon_item->details;

    g_assert (x1 != NULL);
    g_assert (y1 != NULL);
    g_assert (x2 != NULL);
    g_assert (y2 != NULL);

    caja_icon_canvas_item_ensure_bounds_up_to_date (icon_item);
    g_assert (details->bounds_cached);

    total_rect = &details->bounds_cache;

    /* Return the result. */
    *x1 = (int)details->x + total_rect->x0;
    *y1 = (int)details->y + total_rect->y0;
    *x2 = (int)details->x + total_rect->x1 + 1;
    *y2 = (int)details->y + total_rect->y1 + 1;
}

static void
caja_icon_canvas_item_ensure_bounds_up_to_date (CajaIconCanvasItem *icon_item)
{
    CajaIconCanvasItemDetails *details;
    EelIRect icon_rect, emblem_rect, icon_rect_raw;
    EelIRect text_rect, text_rect_for_layout, text_rect_for_entire_text;
    EelIRect total_rect, total_rect_for_layout, total_rect_for_entire_text;
    EelCanvasItem *item;
    double pixels_per_unit;
    EmblemLayout emblem_layout;
    GdkPixbuf *emblem_pixbuf;
    gboolean is_rtl;

    details = icon_item->details;
    item = EEL_CANVAS_ITEM (icon_item);

    if (!details->bounds_cached)
    {
        measure_label_text (icon_item);

        pixels_per_unit = EEL_CANVAS_ITEM (item)->canvas->pixels_per_unit;

        /* Compute raw and scaled icon rectangle. */
        icon_rect.x0 = 0;
        icon_rect.y0 = 0;
        icon_rect_raw.x0 = 0;
        icon_rect_raw.y0 = 0;
        if (details->pixbuf == NULL)
        {
            icon_rect.x1 = icon_rect.x0;
            icon_rect.y1 = icon_rect.y0;
            icon_rect_raw.x1 = icon_rect_raw.x0;
            icon_rect_raw.y1 = icon_rect_raw.y0;
        }
        else
        {
            icon_rect_raw.x1 = icon_rect_raw.x0 + gdk_pixbuf_get_width (details->pixbuf);
            icon_rect_raw.y1 = icon_rect_raw.y0 + gdk_pixbuf_get_height (details->pixbuf);
            icon_rect.x1 = icon_rect_raw.x1 / pixels_per_unit;
            icon_rect.y1 = icon_rect_raw.y1 / pixels_per_unit;
        }

        /* Compute text rectangle. */
        text_rect = compute_text_rectangle (icon_item, icon_rect, FALSE, BOUNDS_USAGE_FOR_DISPLAY);
        text_rect_for_layout = compute_text_rectangle (icon_item, icon_rect, FALSE, BOUNDS_USAGE_FOR_LAYOUT);
        text_rect_for_entire_text = compute_text_rectangle (icon_item, icon_rect, FALSE, BOUNDS_USAGE_FOR_ENTIRE_ITEM);

        is_rtl = caja_icon_container_is_layout_rtl (CAJA_ICON_CONTAINER (item->canvas));

        /* Compute total rectangle, adding in emblem rectangles. */
        eel_irect_union (&total_rect, &icon_rect, &text_rect);
        eel_irect_union (&total_rect_for_layout, &icon_rect, &text_rect_for_layout);
        eel_irect_union (&total_rect_for_entire_text, &icon_rect, &text_rect_for_entire_text);
        emblem_layout_reset (&emblem_layout, icon_item, icon_rect_raw, is_rtl);
        while (emblem_layout_next (&emblem_layout, &emblem_pixbuf, &emblem_rect, is_rtl))
        {
            emblem_rect.x0 = floor (emblem_rect.x0 / pixels_per_unit);
            emblem_rect.y0 = floor (emblem_rect.y0 / pixels_per_unit);
            emblem_rect.x1 = ceil (emblem_rect.x1 / pixels_per_unit);
            emblem_rect.y1 = ceil (emblem_rect.y1 / pixels_per_unit);

            eel_irect_union (&total_rect, &total_rect, &emblem_rect);
            eel_irect_union (&total_rect_for_layout, &total_rect_for_layout, &emblem_rect);
            eel_irect_union (&total_rect_for_entire_text, &total_rect_for_entire_text, &emblem_rect);
        }

        details->bounds_cache = total_rect;
        details->bounds_cache_for_layout = total_rect_for_layout;
        details->bounds_cache_for_entire_item = total_rect_for_entire_text;
        details->bounds_cached = TRUE;
    }
}

/* Get the rectangle of the icon only, in world coordinates. */
EelDRect
caja_icon_canvas_item_get_icon_rectangle (const CajaIconCanvasItem *item)
{
    EelDRect rectangle;
    double pixels_per_unit;
    GdkPixbuf *pixbuf;

    g_return_val_if_fail (CAJA_IS_ICON_CANVAS_ITEM (item), eel_drect_empty);

    rectangle.x0 = item->details->x;
    rectangle.y0 = item->details->y;

    pixbuf = item->details->pixbuf;

    pixels_per_unit = EEL_CANVAS_ITEM (item)->canvas->pixels_per_unit;
    rectangle.x1 = rectangle.x0 + (pixbuf == NULL ? 0 : gdk_pixbuf_get_width (pixbuf)) / pixels_per_unit;
    rectangle.y1 = rectangle.y0 + (pixbuf == NULL ? 0 : gdk_pixbuf_get_height (pixbuf)) / pixels_per_unit;

    eel_canvas_item_i2w (EEL_CANVAS_ITEM (item),
                         &rectangle.x0,
                         &rectangle.y0);
    eel_canvas_item_i2w (EEL_CANVAS_ITEM (item),
                         &rectangle.x1,
                         &rectangle.y1);

    return rectangle;
}

EelDRect
caja_icon_canvas_item_get_text_rectangle (CajaIconCanvasItem *item,
        gboolean for_layout)
{
    /* FIXME */
    EelIRect icon_rectangle;
    EelIRect text_rectangle;
    EelDRect ret;
    double pixels_per_unit;
    GdkPixbuf *pixbuf;

    g_return_val_if_fail (CAJA_IS_ICON_CANVAS_ITEM (item), eel_drect_empty);

    icon_rectangle.x0 = item->details->x;
    icon_rectangle.y0 = item->details->y;

    pixbuf = item->details->pixbuf;

    pixels_per_unit = EEL_CANVAS_ITEM (item)->canvas->pixels_per_unit;
    icon_rectangle.x1 = icon_rectangle.x0 + (pixbuf == NULL ? 0 : gdk_pixbuf_get_width (pixbuf)) / pixels_per_unit;
    icon_rectangle.y1 = icon_rectangle.y0 + (pixbuf == NULL ? 0 : gdk_pixbuf_get_height (pixbuf)) / pixels_per_unit;

    measure_label_text (item);

    text_rectangle = compute_text_rectangle (item, icon_rectangle, FALSE,
                     for_layout ? BOUNDS_USAGE_FOR_LAYOUT : BOUNDS_USAGE_FOR_DISPLAY);

    ret.x0 = text_rectangle.x0;
    ret.y0 = text_rectangle.y0;
    ret.x1 = text_rectangle.x1;
    ret.y1 = text_rectangle.y1;

    eel_canvas_item_i2w (EEL_CANVAS_ITEM (item),
                         &ret.x0,
                         &ret.y0);
    eel_canvas_item_i2w (EEL_CANVAS_ITEM (item),
                         &ret.x1,
                         &ret.y1);

    return ret;
}


/* Get the rectangle of the icon only, in canvas coordinates. */
static void
get_icon_canvas_rectangle (CajaIconCanvasItem *item,
                           EelIRect *rect)
{
    GdkPixbuf *pixbuf;

    g_assert (CAJA_IS_ICON_CANVAS_ITEM (item));
    g_assert (rect != NULL);

    eel_canvas_w2c (EEL_CANVAS_ITEM (item)->canvas,
                    item->details->x,
                    item->details->y,
                    &rect->x0,
                    &rect->y0);

    pixbuf = item->details->pixbuf;

    rect->x1 = rect->x0 + (pixbuf == NULL ? 0 : gdk_pixbuf_get_width (pixbuf));
    rect->y1 = rect->y0 + (pixbuf == NULL ? 0 : gdk_pixbuf_get_height (pixbuf));
}

void
caja_icon_canvas_item_set_show_stretch_handles (CajaIconCanvasItem *item,
        gboolean show_stretch_handles)
{
    g_return_if_fail (CAJA_IS_ICON_CANVAS_ITEM (item));
    g_return_if_fail (show_stretch_handles == FALSE || show_stretch_handles == TRUE);

    if (!item->details->show_stretch_handles == !show_stretch_handles)
    {
        return;
    }

    item->details->show_stretch_handles = show_stretch_handles;
    eel_canvas_item_request_update (EEL_CANVAS_ITEM (item));
}

/* Check if one of the stretch handles was hit. */
static gboolean
hit_test_stretch_handle (CajaIconCanvasItem *item,
                         EelIRect probe_canvas_rect,
                         GtkCornerType *corner)
{
    EelIRect icon_rect;
    GdkPixbuf *knob_pixbuf;
    int knob_width, knob_height;
    int hit_corner;

    g_assert (CAJA_IS_ICON_CANVAS_ITEM (item));

    /* Make sure there are handles to hit. */
    if (!item->details->show_stretch_handles)
    {
        return FALSE;
    }

    /* Quick check to see if the rect hits the icon at all. */
    icon_rect = item->details->canvas_rect;
    if (!eel_irect_hits_irect (probe_canvas_rect, icon_rect))
    {
        return FALSE;
    }

    knob_pixbuf = get_knob_pixbuf ();
    knob_width = gdk_pixbuf_get_width (knob_pixbuf);
    knob_height = gdk_pixbuf_get_height (knob_pixbuf);
    g_object_unref (knob_pixbuf);

    /* Check for hits in the stretch handles. */
    hit_corner = -1;
    if (probe_canvas_rect.x0 < icon_rect.x0 + knob_width)
    {
        if (probe_canvas_rect.y0 < icon_rect.y0 + knob_height)
            hit_corner = GTK_CORNER_TOP_LEFT;
        else if (probe_canvas_rect.y1 >= icon_rect.y1 - knob_height)
            hit_corner = GTK_CORNER_BOTTOM_LEFT;
    }
    else if (probe_canvas_rect.x1 >= icon_rect.x1 - knob_width)
    {
        if (probe_canvas_rect.y0 < icon_rect.y0 + knob_height)
            hit_corner = GTK_CORNER_TOP_RIGHT;
        else if (probe_canvas_rect.y1 >= icon_rect.y1 - knob_height)
            hit_corner = GTK_CORNER_BOTTOM_RIGHT;
    }
    if (corner)
        *corner = hit_corner;

    return hit_corner != -1;
}

gboolean
caja_icon_canvas_item_hit_test_stretch_handles (CajaIconCanvasItem *item,
        EelDPoint world_point,
        GtkCornerType *corner)
{
    EelIRect canvas_rect;

    g_return_val_if_fail (CAJA_IS_ICON_CANVAS_ITEM (item), FALSE);

    eel_canvas_w2c (EEL_CANVAS_ITEM (item)->canvas,
                    world_point.x,
                    world_point.y,
                    &canvas_rect.x0,
                    &canvas_rect.y0);
    canvas_rect.x1 = canvas_rect.x0 + 1;
    canvas_rect.y1 = canvas_rect.y0 + 1;
    return hit_test_stretch_handle (item, canvas_rect, corner);
}

/* caja_icon_canvas_item_hit_test_rectangle
 *
 * Check and see if there is an intersection between the item and the
 * canvas rect.
 */
gboolean
caja_icon_canvas_item_hit_test_rectangle (CajaIconCanvasItem *item, EelIRect canvas_rect)
{
    g_return_val_if_fail (CAJA_IS_ICON_CANVAS_ITEM (item), FALSE);

    return hit_test (item, canvas_rect);
}

const char *
caja_icon_canvas_item_get_editable_text (CajaIconCanvasItem *icon_item)
{
    g_return_val_if_fail (CAJA_IS_ICON_CANVAS_ITEM (icon_item), NULL);

    return icon_item->details->editable_text;
}

void
caja_icon_canvas_item_set_renaming (CajaIconCanvasItem *item, gboolean state)
{
    g_return_if_fail (CAJA_IS_ICON_CANVAS_ITEM (item));
    g_return_if_fail (state == FALSE || state == TRUE);

    if (!item->details->is_renaming == !state)
    {
        return;
    }

    item->details->is_renaming = state;
    eel_canvas_item_request_update (EEL_CANVAS_ITEM (item));
}

double
caja_icon_canvas_item_get_max_text_width (CajaIconCanvasItem *item)
{
    EelCanvasItem *canvas_item;
    CajaIconContainer *container;

    canvas_item = EEL_CANVAS_ITEM (item);
    container = CAJA_ICON_CONTAINER (canvas_item->canvas);

    if (caja_icon_container_is_tighter_layout (container))
    {
        return MAX_TEXT_WIDTH_TIGHTER * canvas_item->canvas->pixels_per_unit;
    }
    else
    {

        if (container->details->label_position == CAJA_ICON_LABEL_POSITION_BESIDE)
        {
            if (container->details->layout_mode == CAJA_ICON_LAYOUT_T_B_L_R ||
                    container->details->layout_mode == CAJA_ICON_LAYOUT_T_B_R_L)
            {
                if (container->details->all_columns_same_width)
                {
                    return MAX_TEXT_WIDTH_BESIDE_TOP_TO_BOTTOM * canvas_item->canvas->pixels_per_unit;
                }
                else
                {
                    return -1;
                }
            }
            else
            {
                return MAX_TEXT_WIDTH_BESIDE * canvas_item->canvas->pixels_per_unit;
            }
        }
        else
        {
            return MAX_TEXT_WIDTH_STANDARD * canvas_item->canvas->pixels_per_unit;
        }


    }

}

void
caja_icon_canvas_item_set_entire_text (CajaIconCanvasItem       *item,
					   gboolean                      entire_text)
{
	if (item->details->entire_text != entire_text) {
		item->details->entire_text = entire_text;

		caja_icon_canvas_item_invalidate_label_size (item);
		eel_canvas_item_request_update (EEL_CANVAS_ITEM (item));
	}
}

/* Class initialization function for the icon canvas item. */
static void
caja_icon_canvas_item_class_init (CajaIconCanvasItemClass *class)
{
	GObjectClass *object_class;
	EelCanvasItemClass *item_class;

	object_class = G_OBJECT_CLASS (class);
	item_class = EEL_CANVAS_ITEM_CLASS (class);

	object_class->finalize = caja_icon_canvas_item_finalize;
	object_class->set_property = caja_icon_canvas_item_set_property;
	object_class->get_property = caja_icon_canvas_item_get_property;

        g_object_class_install_property (
		object_class,
		PROP_EDITABLE_TEXT,
		g_param_spec_string ("editable_text",
				     "editable text",
				     "the editable label",
				     "", G_PARAM_READWRITE));

        g_object_class_install_property (
		object_class,
		PROP_ADDITIONAL_TEXT,
		g_param_spec_string ("additional_text",
				     "additional text",
				     "some more text",
				     "", G_PARAM_READWRITE));

        g_object_class_install_property (
		object_class,
		PROP_HIGHLIGHTED_FOR_SELECTION,
		g_param_spec_boolean ("highlighted_for_selection",
				      "highlighted for selection",
				      "whether we are highlighted for a selection",
				      FALSE, G_PARAM_READWRITE)); 

        g_object_class_install_property (
		object_class,
		PROP_HIGHLIGHTED_AS_KEYBOARD_FOCUS,
		g_param_spec_boolean ("highlighted_as_keyboard_focus",
				      "highlighted as keyboard focus",
				      "whether we are highlighted to render keyboard focus",
				      FALSE, G_PARAM_READWRITE)); 


        g_object_class_install_property (
		object_class,
		PROP_HIGHLIGHTED_FOR_DROP,
		g_param_spec_boolean ("highlighted_for_drop",
				      "highlighted for drop",
				      "whether we are highlighted for a D&D drop",
				      FALSE, G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_HIGHLIGHTED_FOR_CLIPBOARD,
		g_param_spec_boolean ("highlighted_for_clipboard",
				      "highlighted for clipboard",
				      "whether we are highlighted for a clipboard paste (after we have been cut)",
 				      FALSE, G_PARAM_READWRITE));

	item_class->update = caja_icon_canvas_item_update;
	item_class->draw = caja_icon_canvas_item_draw;
	item_class->point = caja_icon_canvas_item_point;
	item_class->translate = caja_icon_canvas_item_translate;
	item_class->bounds = caja_icon_canvas_item_bounds;
	item_class->event = caja_icon_canvas_item_event;

	atk_registry_set_factory_type (atk_get_default_registry (),
				       CAJA_TYPE_ICON_CANVAS_ITEM,
				       caja_icon_canvas_item_accessible_factory_get_type ());

	g_type_class_add_private (class, sizeof (CajaIconCanvasItemDetails));
}

static GailTextUtil *
caja_icon_canvas_item_get_text (GObject *text)
{
	return CAJA_ICON_CANVAS_ITEM (text)->details->text_util;
}

static void
caja_icon_canvas_item_text_interface_init (EelAccessibleTextIface *iface)
{
	iface->get_text = caja_icon_canvas_item_get_text;
}

/* ============================= a11y interfaces =========================== */

static const char *caja_icon_canvas_item_accessible_action_names[] = {
        "open",
        "menu",
        NULL
};

static const char *caja_icon_canvas_item_accessible_action_descriptions[] = {
        "Open item",
        "Popup context menu",
        NULL
};

enum {
	ACTION_OPEN,
	ACTION_MENU,
	LAST_ACTION
};

typedef struct {
        char *action_descriptions[LAST_ACTION];
	char *image_description;
	char *description;
} CajaIconCanvasItemAccessiblePrivate;

typedef struct {
	CajaIconCanvasItem *item;
	gint action_number;
} CajaIconCanvasItemAccessibleActionContext;

static GType caja_icon_canvas_item_accessible_get_type (void);

#define GET_PRIV(o) G_TYPE_INSTANCE_GET_PRIVATE(o, caja_icon_canvas_item_accessible_get_type (), CajaIconCanvasItemAccessiblePrivate);

/* accessible AtkAction interface */

static gboolean
caja_icon_canvas_item_accessible_idle_do_action (gpointer data)
{
    CajaIconCanvasItem *item;
    CajaIconCanvasItemAccessibleActionContext *ctx;
    CajaIcon *icon;
    CajaIconContainer *container;
    GList* selection;
    GList file_list;
    GdkEventButton button_event = { 0 };
    gint action_number;

    container = CAJA_ICON_CONTAINER (data);
    container->details->a11y_item_action_idle_handler = 0;
    while (!g_queue_is_empty (container->details->a11y_item_action_queue))
    {
        ctx = g_queue_pop_head (container->details->a11y_item_action_queue);
        action_number = ctx->action_number;
        item = ctx->item;
        g_free (ctx);
        icon = item->user_data;

        switch (action_number)
        {
        case ACTION_OPEN:
            file_list.data = icon->data;
            file_list.next = NULL;
            file_list.prev = NULL;
            g_signal_emit_by_name (container, "activate", &file_list);
            break;
        case ACTION_MENU:
            selection = caja_icon_container_get_selection (container);
            if (selection == NULL ||
                    g_list_length (selection) != 1 ||
                    selection->data != icon->data)
            {
                g_list_free (selection);
                return FALSE;
            }
            g_list_free (selection);
            g_signal_emit_by_name (container, "context_click_selection", &button_event);
            break;
        default :
            g_assert_not_reached ();
            break;
        }
    }
    return FALSE;
}

static gboolean
caja_icon_canvas_item_accessible_do_action (AtkAction *accessible, int i)
{
    CajaIconCanvasItem *item;
    CajaIconCanvasItemAccessibleActionContext *ctx;
    CajaIconContainer *container;

    g_assert (i < LAST_ACTION);

    item = CAJA_ICON_CANVAS_ITEM (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (accessible)));
    if (!item)
    {
        return FALSE;
    }
    container = CAJA_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas);
    switch (i)
    {
    case ACTION_OPEN:
    case ACTION_MENU:
        if (container->details->a11y_item_action_queue == NULL)
        {
            container->details->a11y_item_action_queue = g_queue_new ();
        }
        ctx = g_new (CajaIconCanvasItemAccessibleActionContext, 1);
        ctx->action_number = i;
        ctx->item = item;
        g_queue_push_head (container->details->a11y_item_action_queue, ctx);
        if (container->details->a11y_item_action_idle_handler == 0)
        {
            container->details->a11y_item_action_idle_handler = g_idle_add (caja_icon_canvas_item_accessible_idle_do_action, container);
        }
        break;
    default :
        g_warning ("Invalid action passed to CajaIconCanvasItemAccessible::do_action");
        return FALSE;
    }

    return TRUE;
}

static int
caja_icon_canvas_item_accessible_get_n_actions (AtkAction *accessible)
{
    return LAST_ACTION;
}

static const char *
caja_icon_canvas_item_accessible_action_get_description (AtkAction *accessible,
        int i)
{
    CajaIconCanvasItemAccessiblePrivate *priv;

    g_assert (i < LAST_ACTION);

    priv = GET_PRIV (accessible);
    if (priv->action_descriptions[i])
    {
        return priv->action_descriptions[i];
    }
    else
    {
        return caja_icon_canvas_item_accessible_action_descriptions[i];
    }
}

static const char *
caja_icon_canvas_item_accessible_action_get_name (AtkAction *accessible, int i)
{
    g_assert (i < LAST_ACTION);

    return caja_icon_canvas_item_accessible_action_names[i];
}

static const char *
caja_icon_canvas_item_accessible_action_get_keybinding (AtkAction *accessible,
        int i)
{
    g_assert (i < LAST_ACTION);

    return NULL;
}

static gboolean
caja_icon_canvas_item_accessible_action_set_description (AtkAction *accessible,
        int i,
        const char *description)
{
    CajaIconCanvasItemAccessiblePrivate *priv;

    g_assert (i < LAST_ACTION);

    priv = GET_PRIV (accessible);

    if (priv->action_descriptions[i])
    {
        g_free (priv->action_descriptions[i]);
    }
    priv->action_descriptions[i] = g_strdup (description);

    return TRUE;
}

static void
caja_icon_canvas_item_accessible_action_interface_init (AtkActionIface *iface)
{
    iface->do_action = caja_icon_canvas_item_accessible_do_action;
    iface->get_n_actions = caja_icon_canvas_item_accessible_get_n_actions;
    iface->get_description = caja_icon_canvas_item_accessible_action_get_description;
    iface->get_keybinding = caja_icon_canvas_item_accessible_action_get_keybinding;
    iface->get_name = caja_icon_canvas_item_accessible_action_get_name;
    iface->set_description = caja_icon_canvas_item_accessible_action_set_description;
}

static const gchar* caja_icon_canvas_item_accessible_get_name(AtkObject* accessible)
{
    CajaIconCanvasItem* item;

    if (accessible->name)
    {
        return accessible->name;
    }

    item = CAJA_ICON_CANVAS_ITEM (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (accessible)));

    if (!item)
    {
        return NULL;
    }

    return item->details->editable_text;
}

static const gchar* caja_icon_canvas_item_accessible_get_description(AtkObject* accessible)
{
    CajaIconCanvasItem* item;

    item = CAJA_ICON_CANVAS_ITEM (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (accessible)));

    if (!item)
    {
        return NULL;
    }

    return item->details->additional_text;
}

static AtkObject *
caja_icon_canvas_item_accessible_get_parent (AtkObject *accessible)
{
    CajaIconCanvasItem *item;

    item = CAJA_ICON_CANVAS_ITEM (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (accessible)));
    if (!item)
    {
        return NULL;
    }

    return gtk_widget_get_accessible (GTK_WIDGET (EEL_CANVAS_ITEM (item)->canvas));
}

static int
caja_icon_canvas_item_accessible_get_index_in_parent (AtkObject *accessible)
{
    CajaIconCanvasItem *item;
    CajaIconContainer *container;
    GList *l;
    CajaIcon *icon;
    int i;

    item = CAJA_ICON_CANVAS_ITEM (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (accessible)));
    if (!item)
    {
        return -1;
    }

    container = CAJA_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas);

    l = container->details->icons;
    i = 0;
    while (l)
    {
        icon = l->data;

        if (icon->item == item)
        {
            return i;
        }

        i++;
        l = l->next;
    }

    return -1;
}


static const gchar* caja_icon_canvas_item_accessible_get_image_description(AtkImage* image)
{
    CajaIconCanvasItemAccessiblePrivate* priv;
    CajaIconCanvasItem* item;
    CajaIcon* icon;
    CajaIconContainer* container;
    char* description;

    priv = GET_PRIV (image);

    if (priv->image_description)
    {
        return priv->image_description;
    }
    else
    {
        item = CAJA_ICON_CANVAS_ITEM (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (image)));

        if (item == NULL)
        {
            return NULL;
        }

        icon = item->user_data;
        container = CAJA_ICON_CONTAINER(EEL_CANVAS_ITEM(item)->canvas);
        description = caja_icon_container_get_icon_description(container, icon->data);
        g_free(priv->description);
        priv->description = description;

        return priv->description;
    }
}

static void
caja_icon_canvas_item_accessible_get_image_size
(AtkImage *image,
 gint     *width,
 gint     *height)
{
    CajaIconCanvasItem *item;

    item = CAJA_ICON_CANVAS_ITEM (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (image)));

    if (!item || !item->details->pixbuf)
    {
        *width = *height = 0;
    }
    else
    {
        *width = gdk_pixbuf_get_width (item->details->pixbuf);
        *height = gdk_pixbuf_get_height (item->details->pixbuf);
    }
}

static void
caja_icon_canvas_item_accessible_get_image_position
(AtkImage		 *image,
 gint                    *x,
 gint	                 *y,
 AtkCoordType	         coord_type)
{
    CajaIconCanvasItem *item;
    gint x_offset, y_offset, itmp;

    item = CAJA_ICON_CANVAS_ITEM (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (image)));
    if (!item)
    {
        return;
    }
    if (!item->details->canvas_rect.x0 && !item->details->canvas_rect.x1)
    {
        return;
    }
    else
    {
        x_offset = 0;
        y_offset = 0;
        if (item->details->text_width)
        {
            itmp = item->details->canvas_rect.x0 -
                   item->details->text_rect.x0;
            if (itmp > x_offset)
            {
                x_offset = itmp;
            }
            itmp = item->details->canvas_rect.y0 -
                   item->details->text_rect.y0;
            if (itmp > y_offset)
            {
                y_offset = itmp;
            }
        }
        if (item->details->emblem_pixbufs)
        {
            itmp = item->details->canvas_rect.x0 -
                   item->details->emblem_rect.x0;
            if (itmp > x_offset)
            {
                x_offset = itmp;
            }
            itmp = item->details->canvas_rect.y0 -
                   item->details->emblem_rect.y0;
            if (itmp > y_offset)
            {
                y_offset = itmp;
            }
        }
    }
    atk_component_get_position (ATK_COMPONENT (image), x, y, coord_type);
    *x += x_offset;
    *y += y_offset;
}

static gboolean
caja_icon_canvas_item_accessible_set_image_description
(AtkImage    *image,
 const gchar *description)
{
    CajaIconCanvasItemAccessiblePrivate *priv;

    priv = GET_PRIV (image);

    g_free (priv->image_description);
    priv->image_description = g_strdup (description);

    return TRUE;
}

static void
caja_icon_canvas_item_accessible_image_interface_init (AtkImageIface *iface)
{
    iface->get_image_description = caja_icon_canvas_item_accessible_get_image_description;
    iface->set_image_description = caja_icon_canvas_item_accessible_set_image_description;
    iface->get_image_size        = caja_icon_canvas_item_accessible_get_image_size;
    iface->get_image_position    = caja_icon_canvas_item_accessible_get_image_position;
}

static gint
caja_icon_canvas_item_accessible_get_offset_at_point (AtkText	 *text,
        gint           x,
        gint           y,
        AtkCoordType coords)
{
    gint real_x, real_y, real_width, real_height;
    CajaIconCanvasItem *item;
    gint editable_height;
    gint offset = 0;
    gint index;
    PangoLayout *layout, *editable_layout, *additional_layout;
    PangoRectangle rect0;
    char *icon_text;
    gboolean have_editable;
    gboolean have_additional;
    gint text_offset;

    atk_component_get_extents (ATK_COMPONENT (text), &real_x, &real_y,
                               &real_width, &real_height, coords);

    x -= real_x;
    y -= real_y;

    item = CAJA_ICON_CANVAS_ITEM (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text)));

    if (item->details->pixbuf)
    {
        y -= gdk_pixbuf_get_height (item->details->pixbuf);
    }
    have_editable = item->details->editable_text != NULL &&
                    item->details->editable_text[0] != '\0';
    have_additional = item->details->additional_text != NULL &&item->details->additional_text[0] != '\0';

    editable_layout = NULL;
    additional_layout = NULL;
    if (have_editable)
    {
        editable_layout = get_label_layout (&item->details->editable_text_layout, item, item->details->editable_text);
        prepare_pango_layout_for_draw (item, editable_layout);
        pango_layout_get_pixel_size (editable_layout, NULL, &editable_height);
        if (y >= editable_height &&
                have_additional)
        {
            prepare_pango_layout_for_draw (item, editable_layout);
            additional_layout = get_label_layout (&item->details->additional_text_layout, item, item->details->additional_text);
            layout = additional_layout;
            icon_text = item->details->additional_text;
            y -= editable_height + LABEL_LINE_SPACING;
        }
        else
        {
            layout = editable_layout;
            icon_text = item->details->editable_text;
        }
    }
    else if (have_additional)
    {
        additional_layout = get_label_layout (&item->details->additional_text_layout, item, item->details->additional_text);
        prepare_pango_layout_for_draw (item, additional_layout);
        layout = additional_layout;
        icon_text = item->details->additional_text;
    }
    else
    {
        return 0;
    }

    text_offset = 0;
    if (have_editable)
    {
        pango_layout_index_to_pos (editable_layout, 0, &rect0);
        text_offset = PANGO_PIXELS (rect0.x);
    }
    if (have_additional)
    {
        gint itmp;

        pango_layout_index_to_pos (additional_layout, 0, &rect0);
        itmp = PANGO_PIXELS (rect0.x);
        if (itmp < text_offset)
        {
            text_offset = itmp;
        }
    }
    pango_layout_index_to_pos (layout, 0, &rect0);
    x += text_offset;
    if (!pango_layout_xy_to_index (layout,
                                   x * PANGO_SCALE,
                                   y * PANGO_SCALE,
                                   &index, NULL))
    {
        if (x < 0 || y < 0)
        {
            index = 0;
        }
        else
        {
            index = -1;
        }
    }
    if (index == -1)
    {
        offset = g_utf8_strlen (icon_text, -1);
    }
    else
    {
        offset = g_utf8_pointer_to_offset (icon_text, icon_text + index);
    }
    if (layout == additional_layout)
    {
        offset += g_utf8_strlen (item->details->editable_text, -1);
    }

    if (editable_layout != NULL)
    {
        g_object_unref (editable_layout);
    }

    if (additional_layout != NULL)
    {
        g_object_unref (additional_layout);
    }

    return offset;
}

static void
caja_icon_canvas_item_accessible_get_character_extents (AtkText	   *text,
        gint	   offset,
        gint	   *x,
        gint	   *y,
        gint	   *width,
        gint	   *height,
        AtkCoordType coords)
{
    gint pos_x, pos_y;
    gint len, byte_offset;
    gint editable_height;
    gchar *icon_text;
    CajaIconCanvasItem *item;
    PangoLayout *layout, *editable_layout, *additional_layout;
    PangoRectangle rect;
    PangoRectangle rect0;
    gboolean have_editable;
    gint text_offset;

    atk_component_get_position (ATK_COMPONENT (text), &pos_x, &pos_y, coords);
    item = CAJA_ICON_CANVAS_ITEM (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text)));

    if (item->details->pixbuf)
    {
        pos_y += gdk_pixbuf_get_height (item->details->pixbuf);
    }

    have_editable = item->details->editable_text != NULL &&
                    item->details->editable_text[0] != '\0';
    if (have_editable)
    {
        len = g_utf8_strlen (item->details->editable_text, -1);
    }
    else
    {
        len = 0;
    }

    editable_layout = get_label_layout (&item->details->editable_text_layout, item, item->details->editable_text);
    additional_layout = get_label_layout (&item->details->additional_text_layout, item, item->details->additional_text);

    if (offset < len)
    {
        icon_text = item->details->editable_text;
        layout = editable_layout;
    }
    else
    {
        offset -= len;
        icon_text = item->details->additional_text;
        layout = additional_layout;
        pos_y += LABEL_LINE_SPACING;
        if (have_editable)
        {
            pango_layout_get_pixel_size (editable_layout, NULL, &editable_height);
            pos_y += editable_height;
        }
    }
    byte_offset = g_utf8_offset_to_pointer (icon_text, offset) - icon_text;
    pango_layout_index_to_pos (layout, byte_offset, &rect);
    text_offset = 0;
    if (have_editable)
    {
        pango_layout_index_to_pos (editable_layout, 0, &rect0);
        text_offset = PANGO_PIXELS (rect0.x);
    }
    if (item->details->additional_text != NULL &&
            item->details->additional_text[0] != '\0')
    {
        gint itmp;

        pango_layout_index_to_pos (additional_layout, 0, &rect0);
        itmp = PANGO_PIXELS (rect0.x);
        if (itmp < text_offset)
        {
            text_offset = itmp;
        }
    }

    g_object_unref (editable_layout);
    g_object_unref (additional_layout);

    *x = pos_x + PANGO_PIXELS (rect.x) - text_offset;
    *y = pos_y + PANGO_PIXELS (rect.y);
    *width = PANGO_PIXELS (rect.width);
    *height = PANGO_PIXELS (rect.height);
}

static void
caja_icon_canvas_item_accessible_text_interface_init (AtkTextIface *iface)
{
    iface->get_text                = eel_accessibility_text_get_text;
    iface->get_character_at_offset = eel_accessibility_text_get_character_at_offset;
    iface->get_text_before_offset  = eel_accessibility_text_get_text_before_offset;
    iface->get_text_at_offset      = eel_accessibility_text_get_text_at_offset;
    iface->get_text_after_offset   = eel_accessibility_text_get_text_after_offset;
    iface->get_character_count     = eel_accessibility_text_get_character_count;
    iface->get_character_extents   = caja_icon_canvas_item_accessible_get_character_extents;
    iface->get_offset_at_point     = caja_icon_canvas_item_accessible_get_offset_at_point;
}

#if GTK_CHECK_VERSION(3, 0, 0)
typedef struct {
	EelCanvasItemAccessible parent;
} CajaIconCanvasItemAccessible;

typedef struct {
	EelCanvasItemAccessibleClass parent_class;
} CajaIconCanvasItemAccessibleClass;

G_DEFINE_TYPE_WITH_CODE (CajaIconCanvasItemAccessible,
			 caja_icon_canvas_item_accessible,
			 eel_canvas_item_accessible_get_type (),
			 G_IMPLEMENT_INTERFACE (ATK_TYPE_IMAGE,
						caja_icon_canvas_item_accessible_image_interface_init)
			 G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT,
						caja_icon_canvas_item_accessible_text_interface_init)
			 G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION,
						caja_icon_canvas_item_accessible_action_interface_init));
#else
/* dummy typedef */
typedef AtkObject CajaIconCanvasItemAccessible;
typedef AtkObjectClass CajaIconCanvasItemAccessibleClass;

static void caja_icon_canvas_item_accessible_class_init (CajaIconCanvasItemAccessibleClass *klass);
static gpointer caja_icon_canvas_item_accessible_parent_class = NULL;

static GType
caja_icon_canvas_item_accessible_get_type (void)
{
    static GType type = 0;

    if (!type)
    {
        static GInterfaceInfo atk_image_info =
        {
            (GInterfaceInitFunc) caja_icon_canvas_item_accessible_image_interface_init,
            (GInterfaceFinalizeFunc) NULL,
            NULL
        };

        static GInterfaceInfo atk_text_info =
        {
            (GInterfaceInitFunc) caja_icon_canvas_item_accessible_text_interface_init,
            (GInterfaceFinalizeFunc) NULL,
            NULL
        };

        static GInterfaceInfo atk_action_info =
        {
            (GInterfaceInitFunc) caja_icon_canvas_item_accessible_action_interface_init,
            (GInterfaceFinalizeFunc) NULL,
            NULL
        };

        type = eel_accessibility_create_derived_type
               ("CajaIconCanvasItemAccessible",
                EEL_TYPE_CANVAS_ITEM,
                caja_icon_canvas_item_accessible_class_init);

        g_type_add_interface_static (type, ATK_TYPE_IMAGE,
                                     &atk_image_info);
        g_type_add_interface_static (type, ATK_TYPE_TEXT,
                                     &atk_text_info);
        g_type_add_interface_static (type, ATK_TYPE_ACTION,
                                     &atk_action_info);
    }

    return type;
}
#endif

static AtkStateSet*
caja_icon_canvas_item_accessible_ref_state_set (AtkObject *accessible)
{
	AtkStateSet *state_set;
	CajaIconCanvasItem *item;
	CajaIconContainer *container;
	CajaIcon *icon;
	GList *l;
	gboolean one_item_selected;

	state_set = ATK_OBJECT_CLASS (caja_icon_canvas_item_accessible_parent_class)->ref_state_set (accessible);

	item = CAJA_ICON_CANVAS_ITEM (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (accessible)));
	if (!item) {
		atk_state_set_add_state (state_set, ATK_STATE_DEFUNCT);
		return state_set;
	}
	container = CAJA_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas);
	if (item->details->is_highlighted_as_keyboard_focus) {
		atk_state_set_add_state (state_set, ATK_STATE_FOCUSED);
	} else if (!container->details->keyboard_focus) {

		one_item_selected = FALSE;
		l = container->details->icons;
		while (l) {
			icon = l->data;
		
			if (icon->item == item) {
				if (icon->is_selected) {
					one_item_selected = TRUE;
				} else {
					break;
				}
			} else if (icon->is_selected) {
				one_item_selected = FALSE;
				break;
			}

			l = l->next;
		}

		if (one_item_selected) {
			atk_state_set_add_state (state_set, ATK_STATE_FOCUSED);
		}
	}

	return state_set;
}

static void
caja_icon_canvas_item_accessible_finalize (GObject *object)
{
	CajaIconCanvasItemAccessiblePrivate *priv;
	int i;

	priv = GET_PRIV (object);

	for (i = 0; i < LAST_ACTION; i++) {
		g_free (priv->action_descriptions[i]);
	}
	g_free (priv->image_description);
	g_free (priv->description);

        G_OBJECT_CLASS (caja_icon_canvas_item_accessible_parent_class)->finalize (object);
}

static void
caja_icon_canvas_item_accessible_initialize (AtkObject *accessible,
						 gpointer widget)
{
	ATK_OBJECT_CLASS (caja_icon_canvas_item_accessible_parent_class)->initialize (accessible, widget);

	atk_object_set_role (accessible, ATK_ROLE_ICON);
}

static void
caja_icon_canvas_item_accessible_class_init (CajaIconCanvasItemAccessibleClass *klass)
{
	AtkObjectClass *aclass = ATK_OBJECT_CLASS (klass);
	GObjectClass *oclass = G_OBJECT_CLASS (klass);

#if ! GTK_CHECK_VERSION(3, 0, 0)
	caja_icon_canvas_item_accessible_parent_class = g_type_class_peek_parent (klass);
#endif

	oclass->finalize = caja_icon_canvas_item_accessible_finalize;

	aclass->initialize = caja_icon_canvas_item_accessible_initialize;

	aclass->get_name = caja_icon_canvas_item_accessible_get_name;
	aclass->get_description = caja_icon_canvas_item_accessible_get_description;
	aclass->get_parent = caja_icon_canvas_item_accessible_get_parent;
	aclass->get_index_in_parent = caja_icon_canvas_item_accessible_get_index_in_parent;
	aclass->ref_state_set = caja_icon_canvas_item_accessible_ref_state_set;

	g_type_class_add_private (klass, sizeof (CajaIconCanvasItemAccessiblePrivate));
}

#if GTK_CHECK_VERSION(3, 0, 0)
static void
caja_icon_canvas_item_accessible_init (CajaIconCanvasItemAccessible *self)
{
}
#endif

/* dummy typedef */
typedef AtkObjectFactory      CajaIconCanvasItemAccessibleFactory;
typedef AtkObjectFactoryClass CajaIconCanvasItemAccessibleFactoryClass;

G_DEFINE_TYPE (CajaIconCanvasItemAccessibleFactory, caja_icon_canvas_item_accessible_factory,
	       ATK_TYPE_OBJECT_FACTORY);


static AtkObject *
caja_icon_canvas_item_accessible_factory_create_accessible (GObject *for_object)
{
    AtkObject *accessible;
    CajaIconCanvasItem *item;
    GString *item_text;

    item = CAJA_ICON_CANVAS_ITEM (for_object);
    g_assert (item != NULL);

    item_text = g_string_new (NULL);
    if (item->details->editable_text)
    {
        g_string_append (item_text, item->details->editable_text);
    }
    if (item->details->additional_text)
    {
        g_string_append (item_text, item->details->additional_text);
    }
    item->details->text_util = gail_text_util_new ();
    gail_text_util_text_setup (item->details->text_util,
                               item_text->str);
    g_string_free (item_text, TRUE);

    accessible = g_object_new (caja_icon_canvas_item_accessible_get_type (), NULL);
    atk_object_initialize (accessible, for_object);
    return accessible;
}

static GType
caja_icon_canvas_item_accessible_factory_get_accessible_type (void)
{
    return caja_icon_canvas_item_accessible_get_type ();
}

static void
caja_icon_canvas_item_accessible_factory_init (CajaIconCanvasItemAccessibleFactory *self)
{
}

static void
caja_icon_canvas_item_accessible_factory_class_init (CajaIconCanvasItemAccessibleFactoryClass *klass)
{
	klass->create_accessible = caja_icon_canvas_item_accessible_factory_create_accessible;
	klass->get_accessible_type = caja_icon_canvas_item_accessible_factory_get_accessible_type;
}

