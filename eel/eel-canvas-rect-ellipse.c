/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 1997, 1998, 1999, 2000 Free Software Foundation
 * All rights reserved.
 *
 * This file is part of the Mate Library.
 *
 * The Mate Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Mate Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Mate Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
/*
  @NOTATION@
 */
/* Rectangle and ellipse item types for EelCanvas widget
 *
 * EelCanvas is basically a port of the Tk toolkit's most excellent canvas widget.  Tk is
 * copyrighted by the Regents of the University of California, Sun Microsystems, and other parties.
 *
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <config.h>
#include <math.h>
#include "eel-canvas-rect-ellipse.h"
#include "eel-canvas-util.h"
#include <string.h>

#ifdef HAVE_RENDER
#include <gdk/gdkx.h>
#include <X11/extensions/Xrender.h>
#endif

/* Base class for rectangle and ellipse item types */

#define noVERBOSE

enum
{
    PROP_0,
    PROP_X1,
    PROP_Y1,
    PROP_X2,
    PROP_Y2,
#if !GTK_CHECK_VERSION(3,0,0)
    PROP_FILL_COLOR,
    PROP_FILL_COLOR_GDK,
#endif
    PROP_FILL_COLOR_RGBA,
#if !GTK_CHECK_VERSION(3,0,0)
    PROP_OUTLINE_COLOR,
    PROP_OUTLINE_COLOR_GDK,
#endif
    PROP_OUTLINE_COLOR_RGBA,
	PROP_OUTLINE_STIPPLING,
    PROP_WIDTH_PIXELS,
    PROP_WIDTH_UNITS
};


static void eel_canvas_re_class_init (EelCanvasREClass *klass);
static void eel_canvas_re_init       (EelCanvasRE      *re);
static void eel_canvas_re_set_property (GObject              *object,
                                        guint                 param_id,
                                        const GValue         *value,
                                        GParamSpec           *pspec);
static void eel_canvas_re_get_property (GObject              *object,
                                        guint                 param_id,
                                        GValue               *value,
                                        GParamSpec           *pspec);

static void eel_canvas_re_update_shared (EelCanvasItem *item,
        double i2w_dx, double i2w_dy, int flags);
static void eel_canvas_re_realize     (EelCanvasItem *item);
static void eel_canvas_re_unrealize   (EelCanvasItem *item);
static void eel_canvas_re_bounds      (EelCanvasItem *item, double *x1, double *y1, double *x2, double *y2);
static void eel_canvas_re_translate   (EelCanvasItem *item, double dx, double dy);
static void eel_canvas_rect_update      (EelCanvasItem *item, double i2w_dx, double i2w_dy, int flags);
static void eel_canvas_ellipse_update      (EelCanvasItem *item, double i2w_dx, double i2w_dy, int flags);

typedef struct
{
    /*< public >*/
    int x0, y0, x1, y1;
}  Rect;

static Rect make_rect (int x0, int y0, int x1, int y1);
static void  diff_rects (Rect r1, Rect r2, int *count, Rect result[4]);

static EelCanvasItemClass *re_parent_class;
static EelCanvasREClass *rect_parent_class;


GType
eel_canvas_re_get_type (void)
{
    static GType re_type = 0;

    if (!re_type)
    {
        GTypeInfo re_info =
        {
            sizeof (EelCanvasREClass),
            (GBaseInitFunc) NULL,
            (GBaseFinalizeFunc) NULL,
            (GClassInitFunc) eel_canvas_re_class_init,
            NULL,           /* class_finalize */
            NULL,           /* class_data */
            sizeof (EelCanvasRE),
            0,              /* n_preallocs */
            (GInstanceInitFunc) eel_canvas_re_init
        };

        re_type = g_type_register_static (eel_canvas_item_get_type (),
                                          "EelCanvasRE",
                                          &re_info,
                                          0);
    }

    return re_type;
}

static void
eel_canvas_re_class_init (EelCanvasREClass *klass)
{
    GObjectClass *gobject_class;
    EelCanvasItemClass *item_class;

    gobject_class = (GObjectClass *) klass;
    item_class = (EelCanvasItemClass *) klass;

    re_parent_class = g_type_class_peek_parent (klass);

    gobject_class->set_property = eel_canvas_re_set_property;
    gobject_class->get_property = eel_canvas_re_get_property;

    g_object_class_install_property
    (gobject_class,
     PROP_X1,
     g_param_spec_double ("x1", NULL, NULL,
                          -G_MAXDOUBLE, G_MAXDOUBLE, 0,
                          G_PARAM_READWRITE));
    g_object_class_install_property
    (gobject_class,
     PROP_Y1,
     g_param_spec_double ("y1", NULL, NULL,
                          -G_MAXDOUBLE, G_MAXDOUBLE, 0,
                          G_PARAM_READWRITE));
    g_object_class_install_property
    (gobject_class,
     PROP_X2,
     g_param_spec_double ("x2", NULL, NULL,
                          -G_MAXDOUBLE, G_MAXDOUBLE, 0,
                          G_PARAM_READWRITE));
    g_object_class_install_property
    (gobject_class,
     PROP_Y2,
     g_param_spec_double ("y2", NULL, NULL,
                          -G_MAXDOUBLE, G_MAXDOUBLE, 0,
                          G_PARAM_READWRITE));
    g_object_class_install_property
    (gobject_class,
#if GTK_CHECK_VERSION(3,0,0)
     PROP_FILL_COLOR_RGBA,
     g_param_spec_boxed ("fill-color-rgba", NULL, NULL,
                         GDK_TYPE_RGBA,
#else
     PROP_FILL_COLOR,
     g_param_spec_string ("fill-color", NULL, NULL,
                          NULL,
                          G_PARAM_READWRITE));
    g_object_class_install_property
    (gobject_class,
     PROP_FILL_COLOR_GDK,
     g_param_spec_boxed ("fill-color-gdk", NULL, NULL,
                         GDK_TYPE_COLOR,
                         G_PARAM_READWRITE));
    g_object_class_install_property
    (gobject_class,
     PROP_FILL_COLOR_RGBA,
     g_param_spec_uint ("fill-color-rgba", NULL, NULL,
                        0, G_MAXUINT, 0,
                        G_PARAM_READWRITE));
    g_object_class_install_property
    (gobject_class,
     PROP_OUTLINE_COLOR,
     g_param_spec_string ("outline-color", NULL, NULL,
                          NULL,
                          G_PARAM_READWRITE));
    g_object_class_install_property
    (gobject_class,
     PROP_OUTLINE_COLOR_GDK,
     g_param_spec_boxed ("outline-color-gdk", NULL, NULL,
                         GDK_TYPE_COLOR,
#endif
                         G_PARAM_READWRITE));
    g_object_class_install_property
    (gobject_class,
     PROP_OUTLINE_COLOR_RGBA,
#if GTK_CHECK_VERSION(3,0,0)
     g_param_spec_boxed ("outline-color-rgba", NULL, NULL,
                         GDK_TYPE_RGBA,
                         G_PARAM_READWRITE));
#else
     g_param_spec_uint ("outline-color-rgba", NULL, NULL,
                        0, G_MAXUINT, 0,
                        G_PARAM_READWRITE));
#endif
	g_object_class_install_property
		(gobject_class,
		 PROP_OUTLINE_STIPPLING,
		 g_param_spec_boolean ("outline-stippling", NULL, NULL,
				       FALSE, G_PARAM_READWRITE));
    g_object_class_install_property
    (gobject_class,
     PROP_WIDTH_PIXELS,
     g_param_spec_uint ("width-pixels", NULL, NULL,
                        0, G_MAXUINT, 0,
                        G_PARAM_READWRITE));
    g_object_class_install_property
    (gobject_class,
     PROP_WIDTH_UNITS,
     g_param_spec_double ("width-units", NULL, NULL,
                          0.0, G_MAXDOUBLE, 0.0,
                          G_PARAM_READWRITE));

    item_class->realize = eel_canvas_re_realize;
    item_class->unrealize = eel_canvas_re_unrealize;
    item_class->translate = eel_canvas_re_translate;
    item_class->bounds = eel_canvas_re_bounds;
}

static void
eel_canvas_re_init (EelCanvasRE *re)
{
    re->x1 = 0.0;
    re->y1 = 0.0;
    re->x2 = 0.0;
    re->y2 = 0.0;
    re->width = 0.0;
}

static void get_bounds (EelCanvasRE *re, double *px1, double *py1, double *px2, double *py2)
{
    EelCanvasItem *item;
    double x1, y1, x2, y2;
    int cx1, cy1, cx2, cy2;
    double hwidth;

#ifdef VERBOSE
    g_print ("re get_bounds\n");
#endif
    item = EEL_CANVAS_ITEM (re);

    if (re->width_pixels)
        hwidth = (re->width / item->canvas->pixels_per_unit) / 2.0;
    else
        hwidth = re->width / 2.0;

    x1 = re->x1;
    y1 = re->y1;
    x2 = re->x2;
    y2 = re->y2;

    eel_canvas_item_i2w (item, &x1, &y1);
    eel_canvas_item_i2w (item, &x2, &y2);
    eel_canvas_w2c (item->canvas, x1 - hwidth, y1 - hwidth, &cx1, &cy1);
    eel_canvas_w2c (item->canvas, x2 + hwidth, y2 + hwidth, &cx2, &cy2);
    *px1 = cx1;
    *py1 = cy1;
    *px2 = cx2;
    *py2 = cy2;

    /* Some safety fudging */

    *px1 -= 2;
    *py1 -= 2;
    *px2 += 2;
    *py2 += 2;
}

static void
eel_canvas_re_set_fill (EelCanvasRE *re, gboolean fill_set)
{
    if (re->fill_set != fill_set)
    {
        re->fill_set = fill_set;
        eel_canvas_item_request_update (EEL_CANVAS_ITEM (re));
    }
}

static void
eel_canvas_re_set_outline (EelCanvasRE *re, gboolean outline_set)
{
    if (re->outline_set != outline_set)
    {
        re->outline_set = outline_set;
        eel_canvas_item_request_update (EEL_CANVAS_ITEM (re));
    }
}

static void
eel_canvas_re_set_property (GObject              *object,
                            guint                 param_id,
                            const GValue         *value,
                            GParamSpec           *pspec)
{
    EelCanvasItem *item;
    EelCanvasRE *re;
#if !GTK_CHECK_VERSION(3,0,0)
    GdkColor color = { 0, 0, 0, 0, };
    GdkColor *pcolor;
#endif

    g_return_if_fail (object != NULL);
    g_return_if_fail (EEL_IS_CANVAS_RE (object));

    item = EEL_CANVAS_ITEM (object);
    re = EEL_CANVAS_RE (object);

    switch (param_id)
    {
    case PROP_X1:
        re->x1 = g_value_get_double (value);

        eel_canvas_item_request_update (item);
        break;

    case PROP_Y1:
        re->y1 = g_value_get_double (value);

        eel_canvas_item_request_update (item);
        break;

    case PROP_X2:
        re->x2 = g_value_get_double (value);

        eel_canvas_item_request_update (item);
        break;

    case PROP_Y2:
        re->y2 = g_value_get_double (value);

        eel_canvas_item_request_update (item);
        break;

#if GTK_CHECK_VERSION(3,0,0)
    case PROP_FILL_COLOR_RGBA: {
            GdkRGBA *color;

            color = g_value_get_boxed (value);

            eel_canvas_re_set_fill (re, color != NULL);

            if (color != NULL) {
                    re->fill_color = *color;
        }

        eel_canvas_item_request_redraw (item);
        break;
    }

    case PROP_OUTLINE_COLOR_RGBA: {
            GdkRGBA *color;

            color = g_value_get_boxed (value);

            eel_canvas_re_set_outline (re, color != NULL);

            if (color != NULL) {
                    re->outline_color = *color;
        }

        eel_canvas_item_request_redraw (item);
        break;
    }
#else
    case PROP_FILL_COLOR:
    case PROP_FILL_COLOR_GDK:
    case PROP_FILL_COLOR_RGBA:
        switch (param_id)
        {
        case PROP_FILL_COLOR:
            if (g_value_get_string (value) &&
                    gdk_color_parse (g_value_get_string (value), &color))
                eel_canvas_re_set_fill (re, TRUE);
            else
                eel_canvas_re_set_fill (re, FALSE);

            re->fill_color = ((color.red & 0xff00) << 16 |
                              (color.green & 0xff00) << 8 |
                              (color.blue & 0xff00) |
                              0xff);
            break;

        case PROP_FILL_COLOR_GDK:
            pcolor = g_value_get_boxed (value);
            eel_canvas_re_set_fill (re, pcolor != NULL);

            if (pcolor)
            {
                color = *pcolor;
            }

            re->fill_color = ((color.red & 0xff00) << 16 |
                              (color.green & 0xff00) << 8 |
                              (color.blue & 0xff00) |
                              0xff);
            break;

        case PROP_FILL_COLOR_RGBA:
            eel_canvas_re_set_fill (re, TRUE);
            re->fill_color = g_value_get_uint (value);
            break;
        }
#ifdef VERBOSE
        g_print ("re fill color = %08x\n", re->fill_color);
#endif
        eel_canvas_item_request_redraw (item);
        break;

    case PROP_OUTLINE_COLOR:
    case PROP_OUTLINE_COLOR_GDK:
    case PROP_OUTLINE_COLOR_RGBA:
        switch (param_id)
        {
        case PROP_OUTLINE_COLOR:
            if (g_value_get_string (value) &&
                    gdk_color_parse (g_value_get_string (value), &color))
                eel_canvas_re_set_outline (re, TRUE);
            else
                eel_canvas_re_set_outline (re, FALSE);

            re->outline_color = ((color.red & 0xff00) << 16 |
                                 (color.green & 0xff00) << 8 |
                                 (color.blue & 0xff00) |
                                 0xff);
            break;

        case PROP_OUTLINE_COLOR_GDK:
            pcolor = g_value_get_boxed (value);
            eel_canvas_re_set_outline (re, pcolor != NULL);

            if (pcolor)
            {
                color = *pcolor;
            }

            re->outline_color = ((color.red & 0xff00) << 16 |
                                 (color.green & 0xff00) << 8 |
                                 (color.blue & 0xff00) |
                                 0xff);
            break;

        case PROP_OUTLINE_COLOR_RGBA:
            eel_canvas_re_set_outline (re, TRUE);
            re->outline_color = g_value_get_uint (value);
            break;
        }
#ifdef VERBOSE
        g_print ("re outline color %x %x %x\n", color.red, color.green, color.blue);
#endif
        eel_canvas_item_request_redraw (item);
        break;
#endif

	case PROP_OUTLINE_STIPPLING:
		re->outline_stippling = g_value_get_boolean (value);

		eel_canvas_item_request_redraw (item);
		break;

    case PROP_WIDTH_PIXELS:
        re->width = g_value_get_uint (value);
        re->width_pixels = TRUE;

        eel_canvas_item_request_update (item);
        break;

    case PROP_WIDTH_UNITS:
        re->width = fabs (g_value_get_double (value));
        re->width_pixels = FALSE;

        eel_canvas_item_request_update (item);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

#if !GTK_CHECK_VERSION(3,0,0)
/* Allocates a GdkColor structure filled with the specified pixel, and puts it into the specified
 * value for returning it in the get_property method.
 */
static void
get_color_value (EelCanvasRE *re, gulong pixel, GValue *value)
{
    GdkColor color;

	color.red   = (pixel >> 16) & 0xFF;
	color.green = (pixel >>  8) & 0xFF;
	color.blue  =  pixel        & 0xFF;
	color.red   |= color.red   << 8;
	color.green |= color.green << 8;
	color.blue  |= color.blue  << 8;

    g_value_set_boxed (value, &color);
}
#endif

static void
eel_canvas_re_get_property (GObject              *object,
                            guint                 param_id,
                            GValue               *value,
                            GParamSpec           *pspec)
{
    EelCanvasRE *re;

    g_return_if_fail (object != NULL);
    g_return_if_fail (EEL_IS_CANVAS_RE (object));

    re = EEL_CANVAS_RE (object);

    switch (param_id)
    {
    case PROP_X1:
        g_value_set_double (value,  re->x1);
        break;

    case PROP_Y1:
        g_value_set_double (value,  re->y1);
        break;

    case PROP_X2:
        g_value_set_double (value,  re->x2);
        break;

    case PROP_Y2:
        g_value_set_double (value,  re->y2);
        break;

#if GTK_CHECK_VERSION(3,0,0)
    case PROP_FILL_COLOR_RGBA:
        g_value_set_boxed (value,  &re->fill_color);
        break;

    case PROP_OUTLINE_COLOR_RGBA:
        g_value_set_boxed (value,  &re->outline_color);
#else
    case PROP_FILL_COLOR_GDK:
		get_color_value (re, re->fill_color, value);
        break;

    case PROP_OUTLINE_COLOR_GDK:
		get_color_value (re, re->outline_color, value);
        break;

    case PROP_FILL_COLOR_RGBA:
        g_value_set_uint (value,  re->fill_color);
        break;

    case PROP_OUTLINE_COLOR_RGBA:
        g_value_set_uint (value,  re->outline_color);
#endif
        break;

	case PROP_OUTLINE_STIPPLING:
		g_value_set_boolean (value, re->outline_stippling);
		break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
eel_canvas_re_update_shared (EelCanvasItem *item, double i2w_dx, double i2w_dy, int flags)
{
#ifdef VERBOSE
    g_print ("eel_canvas_re_update_shared\n");
#endif
    if (re_parent_class->update)
        (* re_parent_class->update) (item, i2w_dx, i2w_dy, flags);
}

static void
eel_canvas_re_realize (EelCanvasItem *item)
{
#ifdef VERBOSE
    g_print ("eel_canvas_re_realize\n");
#endif
    if (re_parent_class->realize)
        (* re_parent_class->realize) (item);
}

static void
eel_canvas_re_unrealize (EelCanvasItem *item)
{
    if (re_parent_class->unrealize)
        (* re_parent_class->unrealize) (item);
}

static void
eel_canvas_re_translate (EelCanvasItem *item, double dx, double dy)
{
    EelCanvasRE *re;

#ifdef VERBOSE
    g_print ("eel_canvas_re_translate\n");
#endif
    re = EEL_CANVAS_RE (item);

    re->x1 += dx;
    re->y1 += dy;
    re->x2 += dx;
    re->y2 += dy;
}


static void
eel_canvas_re_bounds (EelCanvasItem *item, double *x1, double *y1, double *x2, double *y2)
{
    EelCanvasRE *re;
    double hwidth;

#ifdef VERBOSE
    g_print ("eel_canvas_re_bounds\n");
#endif
    re = EEL_CANVAS_RE (item);

    if (re->width_pixels)
        hwidth = (re->width / item->canvas->pixels_per_unit) / 2.0;
    else
        hwidth = re->width / 2.0;

    *x1 = re->x1 - hwidth;
    *y1 = re->y1 - hwidth;
    *x2 = re->x2 + hwidth;
    *y2 = re->y2 + hwidth;
}

/* Rectangle item */


static void eel_canvas_rect_class_init (EelCanvasRectClass *klass);
static void eel_canvas_rect_init (EelCanvasRect *rect);
static void eel_canvas_rect_finalize (GObject *object);
static void eel_canvas_rect_realize  (EelCanvasItem *item);

#if GTK_CHECK_VERSION(3,0,0)
static void   eel_canvas_rect_draw   (EelCanvasItem *item, cairo_t *cr, cairo_region_t *region);
#else
static void   eel_canvas_rect_draw   (EelCanvasItem *item, GdkDrawable *drawable, GdkEventExpose *expose);
#endif
static double eel_canvas_rect_point  (EelCanvasItem *item, double x, double y, int cx, int cy,
                                      EelCanvasItem **actual_item);

struct _EelCanvasRectPrivate
{
    Rect last_update_rect;
    Rect last_outline_update_rect;
    int last_outline_update_width;

#ifdef HAVE_RENDER
    gboolean use_render;
    XRenderPictFormat *format;
#endif
};

GType
eel_canvas_rect_get_type (void)
{
    static GType rect_type = 0;

    if (!rect_type)
    {
        GTypeInfo rect_info =
        {
            sizeof (EelCanvasRectClass),
            (GBaseInitFunc) NULL,
            (GBaseFinalizeFunc) NULL,
            (GClassInitFunc) eel_canvas_rect_class_init,
            NULL,           /* class_finalize */
            NULL,           /* class_data */
            sizeof (EelCanvasRect),
            0,              /* n_preallocs */
            (GInstanceInitFunc) eel_canvas_rect_init
        };

        rect_type = g_type_register_static (eel_canvas_re_get_type (),
                                            "EelCanvasRect",
                                            &rect_info,
                                            0);
    }

    return rect_type;
}

static void
eel_canvas_rect_class_init (EelCanvasRectClass *klass)
{
    EelCanvasItemClass *item_class;

    rect_parent_class = g_type_class_peek_parent (klass);

    item_class = (EelCanvasItemClass *) klass;

    item_class->draw = eel_canvas_rect_draw;
    item_class->point = eel_canvas_rect_point;
    item_class->update = eel_canvas_rect_update;
    item_class->realize = eel_canvas_rect_realize;

    G_OBJECT_CLASS (klass)->finalize = eel_canvas_rect_finalize;

}

static void
eel_canvas_rect_init (EelCanvasRect *rect)
{
    rect->priv = g_new0 (EelCanvasRectPrivate, 1);
}

static void
eel_canvas_rect_finalize (GObject *object)
{
    EelCanvasRect *rect = EEL_CANVAS_RECT (object);

    if (rect->priv)
    {
        g_free (rect->priv);
    }

    G_OBJECT_CLASS (rect_parent_class)->finalize (object);
}

static void
eel_canvas_rect_realize  (EelCanvasItem *item)
{
#ifdef HAVE_RENDER
    EelCanvasRectPrivate *priv;
    int event_base, error_base;
    Display *dpy;

    priv = EEL_CANVAS_RECT (item)->priv;

    dpy = GDK_WINDOW_XDISPLAY (gtk_widget_get_window (GTK_WIDGET (item->canvas)));
    priv->use_render = XRenderQueryExtension (dpy, &event_base, &error_base);

    if (priv->use_render)
    {
        GdkVisual *gdk_visual;
        Visual *visual;

        gdk_visual = gtk_widget_get_visual (GTK_WIDGET (item->canvas));
        visual = gdk_x11_visual_get_xvisual (gdk_visual);

        priv->format = XRenderFindVisualFormat (dpy, visual);
    }
#endif

    if (EEL_CANVAS_ITEM_CLASS (rect_parent_class)->realize)
    {
        (* EEL_CANVAS_ITEM_CLASS (rect_parent_class)->realize) (item);
    }
}


static void
eel_canvas_set_source_color (cairo_t *cr,
#if GTK_CHECK_VERSION(3,0,0)
			     GdkRGBA *rgba)
{
	gdk_cairo_set_source_rgba (cr, rgba);
#else
			     guint rgba)
{
	cairo_set_source_rgba (cr,
			       ((rgba >> 24) & 0xff) / 255.,
			       ((rgba >> 16) & 0xff) / 255.,
			       ((rgba >>  8) & 0xff) / 255.,
			       ((rgba >>  0) & 0xff) / 255.);
#endif
}

#define DASH_ON 0.8
#define DASH_OFF 1.7
static void
#if GTK_CHECK_VERSION(3,0,0)
eel_canvas_rect_draw (EelCanvasItem *item, cairo_t *cr, cairo_region_t *region)
#else
eel_canvas_rect_draw (EelCanvasItem *item, GdkDrawable *drawable, GdkEventExpose *expose)
#endif
{
    EelCanvasRE *re;
    double x1, y1, x2, y2;
    int cx1, cy1, cx2, cy2;
    double i2w_dx, i2w_dy;

    re = EEL_CANVAS_RE (item);

    /* Get canvas pixel coordinates */
    i2w_dx = 0.0;
    i2w_dy = 0.0;
    eel_canvas_item_i2w (item, &i2w_dx, &i2w_dy);

    x1 = re->x1 + i2w_dx;
    y1 = re->y1 + i2w_dy;
    x2 = re->x2 + i2w_dx;
    y2 = re->y2 + i2w_dy;

    eel_canvas_w2c (item->canvas, x1, y1, &cx1, &cy1);
    eel_canvas_w2c (item->canvas, x2, y2, &cx2, &cy2);

    if (cx2 <= cx1 || cy2 <= cy1 ) {
        return;
    }

#if GTK_CHECK_VERSION(3,0,0)
    cairo_save (cr);
#else
    cairo_t *cr = gdk_cairo_create (drawable);
    gdk_cairo_region (cr, expose->region);
    cairo_clip (cr);
#endif

    if (re->fill_set)
    {
#if GTK_CHECK_VERSION(3,0,0)
        eel_canvas_set_source_color (cr, &re->fill_color);
#else
        eel_canvas_set_source_color (cr, re->fill_color);
#endif
        cairo_rectangle (cr,
                         cx1, cy1,
                         cx2 - cx1 + 1,
                         cy2 - cy1 + 1);
        cairo_fill (cr);
    }

    if (re->outline_set)
    {
#if GTK_CHECK_VERSION(3,0,0)
        eel_canvas_set_source_color (cr, &re->outline_color);
#else
        eel_canvas_set_source_color (cr, re->outline_color);
#endif
        if (re->width_pixels) {
            cairo_set_line_width (cr, (int) re->width);
        } else {
            cairo_set_line_width (cr, (int) (re->width * re->item.canvas->pixels_per_unit + 0.5));
        }

		if (re->outline_stippling) {
			double dash[2] = { DASH_ON, DASH_OFF };

			cairo_set_dash (cr, dash, G_N_ELEMENTS (dash), 0);
		}

        cairo_rectangle (cr,
            	 cx1 + 0.5, cy1 + 0.5,
            	 cx2 - cx1,
            	 cy2 - cy1);
        cairo_stroke (cr);
    }

#if GTK_CHECK_VERSION(3,0,0)
    cairo_restore (cr);
#else
    cairo_destroy (cr);
#endif
}

static double
eel_canvas_rect_point (EelCanvasItem *item, double x, double y, int cx, int cy, EelCanvasItem **actual_item)
{
    EelCanvasRE *re;
    double x1, y1, x2, y2;
    double hwidth;
    double dx, dy;
    double tmp;

#ifdef VERBOSE
    g_print ("eel_canvas_rect_point\n");
#endif
    re = EEL_CANVAS_RE (item);

    *actual_item = item;

    /* Find the bounds for the rectangle plus its outline width */

    x1 = re->x1;
    y1 = re->y1;
    x2 = re->x2;
    y2 = re->y2;

    if (re->outline_set)
    {
        if (re->width_pixels)
            hwidth = (re->width / item->canvas->pixels_per_unit) / 2.0;
        else
            hwidth = re->width / 2.0;

        x1 -= hwidth;
        y1 -= hwidth;
        x2 += hwidth;
        y2 += hwidth;
    }
    else
        hwidth = 0.0;

    /* Is point inside rectangle (which can be hollow if it has no fill set)? */

    if ((x >= x1) && (y >= y1) && (x <= x2) && (y <= y2))
    {
        if (re->fill_set || !re->outline_set)
            return 0.0;

        dx = x - x1;
        tmp = x2 - x;
        if (tmp < dx)
            dx = tmp;

        dy = y - y1;
        tmp = y2 - y;
        if (tmp < dy)
            dy = tmp;

        if (dy < dx)
            dx = dy;

        dx -= 2.0 * hwidth;

        if (dx < 0.0)
            return 0.0;
        else
            return dx;
    }

    /* Point is outside rectangle */

    if (x < x1)
        dx = x1 - x;
    else if (x > x2)
        dx = x - x2;
    else
        dx = 0.0;

    if (y < y1)
        dy = y1 - y;
    else if (y > y2)
        dy = y - y2;
    else
        dy = 0.0;

    return sqrt (dx * dx + dy * dy);
}

static void
request_redraw_borders (EelCanvas *canvas,
                        Rect     *update_rect,
                        int     width)
{
    eel_canvas_request_redraw (canvas,
                               update_rect->x0, update_rect->y0,
                               update_rect->x1, update_rect->y0 + width);
    eel_canvas_request_redraw (canvas,
                               update_rect->x0, update_rect->y1-width,
                               update_rect->x1, update_rect->y1);
    eel_canvas_request_redraw (canvas,
                               update_rect->x0,       update_rect->y0,
                               update_rect->x0+width, update_rect->y1);
    eel_canvas_request_redraw (canvas,
                               update_rect->x1-width, update_rect->y0,
                               update_rect->x1,       update_rect->y1);
}


static void
eel_canvas_rect_update (EelCanvasItem *item, double i2w_dx, double i2w_dy, gint flags)
{
    EelCanvasRE *re;
    double x1, y1, x2, y2;
    int cx1, cy1, cx2, cy2;
    int repaint_rects_count, i;
    int width_pixels;
    int width_lt, width_rb;
    Rect update_rect, repaint_rects[4];
    EelCanvasRectPrivate *priv;

    eel_canvas_re_update_shared (item, i2w_dx, i2w_dy, flags);

    re = EEL_CANVAS_RE (item);
    priv = EEL_CANVAS_RECT (item)->priv;

    x1 = re->x1 + i2w_dx;
    y1 = re->y1 + i2w_dy;
    x2 = re->x2 + i2w_dx;
    y2 = re->y2 + i2w_dy;

    eel_canvas_w2c (item->canvas, x1, y1, &cx1, &cy1);
    eel_canvas_w2c (item->canvas, x2, y2, &cx2, &cy2);

    update_rect = make_rect (cx1, cy1, cx2+1, cy2+1);
#if 0
    eel_canvas_request_redraw (item->canvas,
                               update_rect.x0, update_rect.y0,
                               update_rect.x1, update_rect.y1);
    eel_canvas_request_redraw (item->canvas,
                               priv->last_update_rect.x0, priv->last_update_rect.y0,
                               priv->last_update_rect.x1, priv->last_update_rect.y1);
#else
    diff_rects (update_rect, priv->last_update_rect,
                &repaint_rects_count, repaint_rects);
    for (i = 0; i < repaint_rects_count; i++)
    {
        eel_canvas_request_redraw (item->canvas,
                                   repaint_rects[i].x0, repaint_rects[i].y0,
                                   repaint_rects[i].x1, repaint_rects[i].y1);
    }
#endif
    priv->last_update_rect = update_rect;

    if (re->outline_set)
    {
        /* Outline and bounding box */
        if (re->width_pixels)
            width_pixels = (int) re->width;
        else
            width_pixels = (int) floor (re->width * re->item.canvas->pixels_per_unit + 0.5);

        width_lt = width_pixels / 2;
        width_rb = (width_pixels + 1) / 2;

        cx1 -= width_lt;
        cy1 -= width_lt;
        cx2 += width_rb;
        cy2 += width_rb;

        update_rect = make_rect (cx1, cy1, cx2, cy2);
        request_redraw_borders (item->canvas, &update_rect,
                                (width_lt + width_rb));
        request_redraw_borders (item->canvas, &priv->last_outline_update_rect,
                                priv->last_outline_update_width);
        priv->last_outline_update_rect = update_rect;
        priv->last_outline_update_width = width_lt + width_rb;

        item->x1 = cx1;
        item->y1 = cy1;
        item->x2 = cx2+1;
        item->y2 = cy2+1;
    }
    else
    {
        item->x1 = cx1;
        item->y1 = cy1;
        item->x2 = cx2+1;
        item->y2 = cy2+1;
    }
}

/* Ellipse item */


static void eel_canvas_ellipse_class_init (EelCanvasEllipseClass *klass);

#if GTK_CHECK_VERSION(3,0,0)
static void   eel_canvas_ellipse_draw   (EelCanvasItem *item, cairo_t *cr, cairo_region_t *region);
#else
static void   eel_canvas_ellipse_draw   (EelCanvasItem *item, GdkDrawable *drawable, GdkEventExpose *expose);
#endif
static double eel_canvas_ellipse_point  (EelCanvasItem *item, double x, double y, int cx, int cy,
        EelCanvasItem **actual_item);


GType
eel_canvas_ellipse_get_type (void)
{
    static GType ellipse_type = 0;

    if (!ellipse_type)
    {
        GTypeInfo ellipse_info =
        {
            sizeof (EelCanvasEllipseClass),
            (GBaseInitFunc) NULL,
            (GBaseFinalizeFunc) NULL,
            (GClassInitFunc) eel_canvas_ellipse_class_init,
            NULL,           /* class_finalize */
            NULL,           /* class_data */
            sizeof (EelCanvasEllipse),
            0,              /* n_preallocs */
            (GInstanceInitFunc) NULL

        };

        ellipse_type = g_type_register_static (eel_canvas_re_get_type (),
                                               "EelCanvasEllipse",
                                               &ellipse_info,
                                               0);
    }

    return ellipse_type;
}

static void
eel_canvas_ellipse_class_init (EelCanvasEllipseClass *klass)
{
    EelCanvasItemClass *item_class;

    item_class = (EelCanvasItemClass *) klass;

    item_class->draw = eel_canvas_ellipse_draw;
    item_class->point = eel_canvas_ellipse_point;
    item_class->update = eel_canvas_ellipse_update;
}

static void
#if GTK_CHECK_VERSION(3,0,0)
eel_canvas_ellipse_draw (EelCanvasItem *item, cairo_t *cr, cairo_region_t *region)
#else
eel_canvas_ellipse_draw (EelCanvasItem *item, GdkDrawable *drawable, GdkEventExpose *expose)
#endif
{
    EelCanvasRE *re;
    int x1, y1, x2, y2;
    double i2w_dx, i2w_dy;

    re = EEL_CANVAS_RE (item);

    /* Get canvas pixel coordinates */

    i2w_dx = 0.0;
    i2w_dy = 0.0;
    eel_canvas_item_i2w (item, &i2w_dx, &i2w_dy);

    eel_canvas_w2c (item->canvas,
                    re->x1 + i2w_dx,
                    re->y1 + i2w_dy,
                    &x1, &y1);
    eel_canvas_w2c (item->canvas,
                    re->x2 + i2w_dx,
                    re->y2 + i2w_dy,
                    &x2, &y2);

#if GTK_CHECK_VERSION(3,0,0)
    cairo_save (cr);
#else
    cairo_t *cr = gdk_cairo_create (drawable);
    gdk_cairo_region (cr, expose->region);
    cairo_clip (cr);
#endif

    cairo_save (cr);
    cairo_translate (cr, (x1 + x2) / 2., (y1 + y2) / 2.);
    cairo_scale (cr, (x2 - x1), (y2 - y1));
    cairo_arc (cr, 0, 0, 1, 0, 2 * G_PI);
    cairo_restore (cr);

    if (re->fill_set)
    {
#if GTK_CHECK_VERSION(3,0,0)
    	eel_canvas_set_source_color (cr, &re->fill_color);
#else
    	eel_canvas_set_source_color (cr, re->fill_color);
#endif
    	cairo_fill_preserve (cr);
    }

    if (re->outline_set)
    {
#if GTK_CHECK_VERSION(3,0,0)
        eel_canvas_set_source_color (cr, &re->outline_color);
#else
        eel_canvas_set_source_color (cr, re->outline_color);
#endif
        if (re->width_pixels) {
            cairo_set_line_width (cr, (int) re->width);
        } else {
            cairo_set_line_width (cr, (int) (re->width * re->item.canvas->pixels_per_unit + 0.5));
        }

        cairo_stroke_preserve (cr);
    }

#if GTK_CHECK_VERSION(3,0,0)
    cairo_restore (cr);
#else
    cairo_destroy (cr);
#endif
}

static double
eel_canvas_ellipse_point (EelCanvasItem *item, double x, double y, int cx, int cy, EelCanvasItem **actual_item)
{
    EelCanvasRE *re;
    double dx, dy;
    double scaled_dist;
    double outline_dist;
    double center_dist;
    double width;
    double a, b;
    double diamx, diamy;

    re = EEL_CANVAS_RE (item);

    *actual_item = item;

    if (re->outline_set)
    {
        if (re->width_pixels)
            width = re->width / item->canvas->pixels_per_unit;
        else
            width = re->width;
    }
    else
        width = 0.0;

    /* Compute the distance between the center of the ellipse and the point, with the ellipse
     * considered as being scaled to a circle.
     */

    dx = x - (re->x1 + re->x2) / 2.0;
    dy = y - (re->y1 + re->y2) / 2.0;
    center_dist = sqrt (dx * dx + dy * dy);

    a = dx / ((re->x2 + width - re->x1) / 2.0);
    b = dy / ((re->y2 + width - re->y1) / 2.0);
    scaled_dist = sqrt (a * a + b * b);

    /* If the scaled distance is greater than 1, then we are outside.  Compute the distance from
     * the point to the edge of the circle, then scale back to the original un-scaled coordinate
     * system.
     */

    if (scaled_dist > 1.0)
        return (center_dist / scaled_dist) * (scaled_dist - 1.0);

    /* We are inside the outer edge of the ellipse.  If it is filled, then we are "inside".
     * Otherwise, do the same computation as above, but also check whether we are inside the
     * outline.
     */

    if (re->fill_set)
        return 0.0;

    if (scaled_dist > EEL_CANVAS_EPSILON)
        outline_dist = (center_dist / scaled_dist) * (1.0 - scaled_dist) - width;
    else
    {
        /* Handle very small distance */

        diamx = re->x2 - re->x1;
        diamy = re->y2 - re->y1;

        if (diamx < diamy)
            outline_dist = (diamx - width) / 2.0;
        else
            outline_dist = (diamy - width) / 2.0;
    }

    if (outline_dist < 0.0)
        return 0.0;

    return outline_dist;
}

static void
eel_canvas_ellipse_update (EelCanvasItem *item, double i2w_dx, double i2w_dy, gint flags)
{
    EelCanvasRE *re;
    double x0, y0, x1, y1;

#ifdef VERBOSE
    g_print ("eel_canvas_sllipse_update item %x\n", item);
#endif

    eel_canvas_re_update_shared (item, i2w_dx, i2w_dy, flags);
    re = EEL_CANVAS_RE (item);

    get_bounds (re, &x0, &y0, &x1, &y1);
    eel_canvas_update_bbox (item, x0, y0, x1, y1);
}

static int
rect_empty (const Rect *src)
{
    return (src->x1 <= src->x0 || src->y1 <= src->y0);
}

static Rect
make_rect (int x0, int y0, int x1, int y1)
{
    Rect r;

    r.x0 = x0;
    r.y0 = y0;
    r.x1 = x1;
    r.y1 = y1;
    return r;
}

static gboolean
rects_intersect (Rect r1, Rect r2)
{
    if (r1.x0 >= r2.x1)
    {
        return FALSE;
    }
    if (r2.x0 >= r1.x1)
    {
        return FALSE;
    }
    if (r1.y0 >= r2.y1)
    {
        return FALSE;
    }
    if (r2.y0 >= r1.y1)
    {
        return FALSE;
    }
    return TRUE;
}

static void
diff_rects_guts (Rect ra, Rect rb, int *count, Rect result[4])
{
    if (ra.x0 < rb.x0)
    {
        result[(*count)++] = make_rect (ra.x0, ra.y0, rb.x0, ra.y1);
    }
    if (ra.y0 < rb.y0)
    {
        result[(*count)++] = make_rect (ra.x0, ra.y0, ra.x1, rb.y0);
    }
    if (ra.x1 < rb.x1)
    {
        result[(*count)++] = make_rect (ra.x1, rb.y0, rb.x1, rb.y1);
    }
    if (ra.y1 < rb.y1)
    {
        result[(*count)++] = make_rect (rb.x0, ra.y1, rb.x1, rb.y1);
    }
}

static void
diff_rects (Rect r1, Rect r2, int *count, Rect result[4])
{
    g_assert (count != NULL);
    g_assert (result != NULL);

    *count = 0;

    if (rects_intersect (r1, r2))
    {
        diff_rects_guts (r1, r2, count, result);
        diff_rects_guts (r2, r1, count, result);
    }
    else
    {
        if (!rect_empty (&r1))
        {
            result[(*count)++] = r1;
        }
        if (!rect_empty (&r2))
        {
            result[(*count)++] = r2;
        }
    }
}
