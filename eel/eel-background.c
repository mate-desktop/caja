/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   eel-background.c: Object for the background of a widget.

   Copyright (C) 2000 Eazel, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Author: Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include "eel-background.h"
#include "eel-gdk-extensions.h"
#include "eel-glib-extensions.h"
#include "eel-lib-self-check-functions.h"
#include <gtk/gtk.h>
#include <eel/eel-canvas.h>
#include <eel/eel-canvas-util.h>
#if GTK_CHECK_VERSION (3, 0, 0)
# include <cairo-xlib.h>
#endif
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <math.h>
#include <stdio.h>
#define MATE_DESKTOP_USE_UNSTABLE_API
#include <libmateui/mate-bg.h>

#if !GTK_CHECK_VERSION(3, 0, 0)
#define cairo_surface_t         GdkPixmap
#define cairo_surface_reference g_object_ref
#define cairo_surface_destroy   g_object_unref
#define cairo_xlib_surface_get_display  GDK_PIXMAP_XDISPLAY
#define cairo_xlib_surface_get_drawable GDK_PIXMAP_XID
#define cairo_set_source_surface gdk_cairo_set_source_pixmap
#define mate_bg_create_surface              mate_bg_create_pixmap
#define mate_bg_set_surface_as_root         mate_bg_set_pixmap_as_root
#define mate_bg_get_surface_from_root       mate_bg_get_pixmap_from_root
#define mate_bg_crossfade_set_start_surface mate_bg_crossfade_set_start_pixmap
#define mate_bg_crossfade_set_end_surface   mate_bg_crossfade_set_end_pixmap
#endif

static void set_image_properties (EelBackground *background);

static void init_fade (EelBackground *background, GtkWidget *widget);
static void free_fade (EelBackground *background);

G_DEFINE_TYPE (EelBackground, eel_background, G_TYPE_OBJECT);

enum
{
    APPEARANCE_CHANGED,
    SETTINGS_CHANGED,
    RESET,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct EelBackgroundDetails
{
    char *color;

    MateBG *bg;
    GtkWidget *widget;

    /* Realized data: */
    cairo_surface_t *background_surface;
    gboolean background_surface_is_unset_root_surface;
    MateBGCrossfade *fade;
    int background_entire_width;
    int background_entire_height;
    GdkColor default_color;

    gboolean use_base;

    /* Is this background attached to desktop window */
    gboolean is_desktop;
    /* Desktop screen size watcher */
    gulong screen_size_handler;
    /* Desktop monitors configuration watcher */
    gulong screen_monitors_handler;
    /* Can we use common surface for root window and desktop window */
    gboolean use_common_surface;
    guint change_idle_id;

    /* activity status */
    gboolean is_active;
};

static void
on_bg_changed (MateBG *bg, EelBackground *background)
{
    init_fade (background, background->details->widget);
    g_signal_emit (G_OBJECT (background),
                   signals[APPEARANCE_CHANGED], 0);
}

static void
on_bg_transitioned (MateBG *bg, EelBackground *background)
{
    free_fade (background);
    g_signal_emit (G_OBJECT (background),
                   signals[APPEARANCE_CHANGED], 0);
}

static void
eel_background_init (EelBackground *background)
{
    background->details =
    		G_TYPE_INSTANCE_GET_PRIVATE (background,
    					     EEL_TYPE_BACKGROUND,
    					     EelBackgroundDetails);

    background->details->default_color.red = 0xffff;
    background->details->default_color.green = 0xffff;
    background->details->default_color.blue = 0xffff;
    background->details->bg = mate_bg_new ();
    background->details->is_active = TRUE;

    g_signal_connect (background->details->bg, "changed",
                      G_CALLBACK (on_bg_changed), background);
    g_signal_connect (background->details->bg, "transitioned",
                      G_CALLBACK (on_bg_transitioned), background);

}

/* The safe way to clear an image from a background is:
 * 		eel_background_set_image_uri (NULL);
 * This fn is a private utility - it does NOT clear
 * the details->bg_uri setting.
 */
static void
eel_background_remove_current_image (EelBackground *background)
{
    if (background->details->bg != NULL)
    {
        g_object_unref (G_OBJECT (background->details->bg));
        background->details->bg = NULL;
    }
}

static void
free_fade (EelBackground *background)
{
    if (background->details->fade != NULL)
    {
        g_object_unref (background->details->fade);
        background->details->fade = NULL;
    }
}

static void
free_background_surface (EelBackground *background)
{
    cairo_surface_t *surface;

    surface = background->details->background_surface;
    if (surface != NULL)
    {
        /* If we created a root surface and didn't set it as background
           it will live forever, so we need to kill it manually.
           If set as root background it will be killed next time the
           background is changed. */
        if (background->details->background_surface_is_unset_root_surface)
        {
            XKillClient (cairo_xlib_surface_get_display (surface),
                         cairo_xlib_surface_get_drawable (surface));
        }
        cairo_surface_destroy (surface);
        background->details->background_surface = NULL;
    }
}


static EelBackgroundImagePlacement
placement_mate_to_eel (MateBGPlacement p)
{
    switch (p)
    {
    case MATE_BG_PLACEMENT_CENTERED:
        return EEL_BACKGROUND_CENTERED;
    case MATE_BG_PLACEMENT_FILL_SCREEN:
        return EEL_BACKGROUND_SCALED;
    case MATE_BG_PLACEMENT_SCALED:
        return EEL_BACKGROUND_SCALED_ASPECT;
    case MATE_BG_PLACEMENT_ZOOMED:
        return EEL_BACKGROUND_ZOOM;
    case MATE_BG_PLACEMENT_TILED:
        return EEL_BACKGROUND_TILED;
    case MATE_BG_PLACEMENT_SPANNED:
        return EEL_BACKGROUND_SPANNED;
    }

    return EEL_BACKGROUND_TILED;
}

static MateBGPlacement
placement_eel_to_mate (EelBackgroundImagePlacement p)
{
    switch (p)
    {
    case EEL_BACKGROUND_CENTERED:
        return MATE_BG_PLACEMENT_CENTERED;
    case EEL_BACKGROUND_SCALED:
        return MATE_BG_PLACEMENT_FILL_SCREEN;
    case EEL_BACKGROUND_SCALED_ASPECT:
        return MATE_BG_PLACEMENT_SCALED;
    case EEL_BACKGROUND_ZOOM:
        return MATE_BG_PLACEMENT_ZOOMED;
    case EEL_BACKGROUND_TILED:
        return MATE_BG_PLACEMENT_TILED;
    case EEL_BACKGROUND_SPANNED:
        return MATE_BG_PLACEMENT_SPANNED;
    }

    return MATE_BG_PLACEMENT_TILED;
}

EelBackgroundImagePlacement
eel_background_get_image_placement (EelBackground *background)
{
    g_return_val_if_fail (EEL_IS_BACKGROUND (background), EEL_BACKGROUND_TILED);

    return placement_mate_to_eel (mate_bg_get_placement (background->details->bg));
}

void
eel_background_set_image_placement (EelBackground              *background,
                                    EelBackgroundImagePlacement new_placement)
{
    g_return_if_fail (EEL_IS_BACKGROUND (background));

    mate_bg_set_placement (background->details->bg,
                           placement_eel_to_mate (new_placement));
}


static void
eel_background_finalize (GObject *object)
{
    EelBackground *background;

    background = EEL_BACKGROUND (object);

    g_free (background->details->color);
    eel_background_remove_current_image (background);

    free_background_surface (background);

    free_fade (background);

    G_OBJECT_CLASS (eel_background_parent_class)->finalize (object);
}

static void
eel_background_class_init (EelBackgroundClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    signals[APPEARANCE_CHANGED] =
        g_signal_new ("appearance_changed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
                      G_STRUCT_OFFSET (EelBackgroundClass,
                                       appearance_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);
    signals[SETTINGS_CHANGED] =
        g_signal_new ("settings_changed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
                      G_STRUCT_OFFSET (EelBackgroundClass,
                                       settings_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__INT,
                      G_TYPE_NONE,
                      1, G_TYPE_INT);
    signals[RESET] =
        g_signal_new ("reset",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
                      G_STRUCT_OFFSET (EelBackgroundClass,
                                       reset),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);

    object_class->finalize = eel_background_finalize;

    g_type_class_add_private (klass, sizeof (EelBackgroundDetails));
}

EelBackground *
eel_background_new (void)
{
    return EEL_BACKGROUND (g_object_new (EEL_TYPE_BACKGROUND, NULL));
}

static void
eel_background_unrealize (EelBackground *background)
{
    free_background_surface (background);

    background->details->background_entire_width = 0;
    background->details->background_entire_height = 0;
    background->details->default_color.red = 0xffff;
    background->details->default_color.green = 0xffff;
    background->details->default_color.blue = 0xffff;
}

static void
drawable_get_adjusted_size (EelBackground *background,
                            GdkWindow     *window,
                            int		  *width,
                            int	          *height)
{
    *width = gdk_window_get_width(GDK_WINDOW(window));
    *height = gdk_window_get_height(GDK_WINDOW(window));

    if (background->details->is_desktop)
    {
        GdkScreen *screen;
        screen = gdk_window_get_screen (window);
        *width = gdk_screen_get_width (screen);
        *height = gdk_screen_get_height (screen);
    }
}

static gboolean
eel_background_ensure_realized (EelBackground *background, GdkWindow *window)
{
    gpointer data;
    GtkWidget *widget;
    GtkStyle *style;
    gboolean changed;
    int entire_width;
    int entire_height;

    drawable_get_adjusted_size (background, window, &entire_width, &entire_height);

    /* Set the default color */

    /* Get the widget to which the window belongs and its style as well */
    gdk_window_get_user_data (window, &data);
    widget = GTK_WIDGET (data);
    if (widget != NULL)
    {
        style = gtk_widget_get_style (widget);
        if (background->details->use_base)
        {
            background->details->default_color = style->base[GTK_STATE_NORMAL];
        }
        else
        {
            background->details->default_color = style->bg[GTK_STATE_NORMAL];
        }

    }

    /* If the window size is the same as last time, don't update */
    if (entire_width == background->details->background_entire_width &&
            entire_height == background->details->background_entire_height)
    {
        return FALSE;
    }

    free_background_surface (background);

    changed = FALSE;

    set_image_properties (background);

    background->details->background_surface = mate_bg_create_surface (background->details->bg,
        							      window,
        							      entire_width, entire_height,
        							      background->details->is_desktop);
    background->details->background_surface_is_unset_root_surface = background->details->is_desktop;

    /* We got the surface and everything, so we don't care about a change
       that is pending (unless things actually change after this time) */
    g_object_set_data (G_OBJECT (background->details->bg),
                       "ignore-pending-change", GINT_TO_POINTER (TRUE));
    changed = TRUE;


    background->details->background_entire_width = entire_width;
    background->details->background_entire_height = entire_height;

    return changed;
}

#define CLAMP_COLOR(v) (t = (v), CLAMP (t, 0, G_MAXUSHORT))
#define SATURATE(v) ((1.0 - saturation) * intensity + saturation * (v))

static void
make_color_inactive (EelBackground *background, GdkColor *color)
{
    double intensity, saturation;
    gushort t;

    if (!background->details->is_active)
    {
        saturation = 0.7;
        intensity = color->red * 0.30 + color->green * 0.59 + color->blue * 0.11;
        color->red = SATURATE (color->red);
        color->green = SATURATE (color->green);
        color->blue = SATURATE (color->blue);

        if (intensity > G_MAXUSHORT / 2)
        {
            color->red *= 0.9;
            color->green *= 0.9;
            color->blue *= 0.9;
        }
        else
        {
            color->red *= 1.25;
            color->green *= 1.25;
            color->blue *= 1.25;
        }

        color->red = CLAMP_COLOR (color->red);
        color->green = CLAMP_COLOR (color->green);
        color->blue = CLAMP_COLOR (color->blue);
    }
}

void
#if GTK_CHECK_VERSION (3, 0, 0)
eel_background_draw (GtkWidget *widget,
                     cairo_t   *cr)
#else
eel_background_expose (GtkWidget      *widget,
                       GdkEventExpose *event)
#endif
{
    int window_width;
    int window_height;
    GdkWindow *widget_window;

    widget_window = gtk_widget_get_window (widget);

#if !GTK_CHECK_VERSION (3, 0, 0)
    g_return_if_fail (event->window == widget_window);
#endif

    EelBackground *background = eel_get_widget_background (widget);

    drawable_get_adjusted_size (background, widget_window, &window_width, &window_height);

    eel_background_ensure_realized (background, widget_window);
    make_color_inactive (background, &background->details->default_color);

#if GTK_CHECK_VERSION(3,0,0)
    cairo_save (cr);
#else
    cairo_t *cr = gdk_cairo_create (widget_window);
#endif

    if (background->details->background_surface != NULL) {
        cairo_set_source_surface (cr, background->details->background_surface, 0, 0);
        cairo_pattern_set_extend (cairo_get_source (cr), CAIRO_EXTEND_REPEAT);
    } else {
        gdk_cairo_set_source_color (cr, &background->details->default_color);
    }

#if !GTK_CHECK_VERSION (3, 0, 0)
    gdk_cairo_rectangle (cr, &event->area);
    cairo_clip (cr);
#endif

    cairo_rectangle (cr, 0, 0, window_width, window_height);
    cairo_fill (cr);

#if GTK_CHECK_VERSION(3,0,0)
    cairo_restore (cr);
#else
    cairo_destroy (cr);
#endif
}

static void
set_image_properties (EelBackground *background)
{
    GdkColor c;
    if (!background->details->color)
    {
        c = background->details->default_color;
        make_color_inactive (background, &c);
        mate_bg_set_color (background->details->bg, MATE_BG_COLOR_SOLID,
                           &c, NULL);
    }
    else if (!eel_gradient_is_gradient (background->details->color))
    {
        eel_gdk_color_parse_with_white_default (background->details->color, &c);
        make_color_inactive (background, &c);
        mate_bg_set_color (background->details->bg, MATE_BG_COLOR_SOLID, &c, NULL);
    }
    else
    {
        GdkColor c1;
        GdkColor c2;
        char *spec;

        spec = eel_gradient_get_start_color_spec (background->details->color);
        eel_gdk_color_parse_with_white_default (spec, &c1);
        make_color_inactive (background, &c1);
        g_free (spec);

        spec = eel_gradient_get_end_color_spec (background->details->color);
        eel_gdk_color_parse_with_white_default (spec, &c2);
        make_color_inactive (background, &c2);
        g_free (spec);

        if (eel_gradient_is_horizontal (background->details->color))
            mate_bg_set_color (background->details->bg, MATE_BG_COLOR_H_GRADIENT, &c1, &c2);
        else
            mate_bg_set_color (background->details->bg, MATE_BG_COLOR_V_GRADIENT, &c1, &c2);

    }
}

char *
eel_background_get_color (EelBackground *background)
{
    g_return_val_if_fail (EEL_IS_BACKGROUND (background), NULL);

    return g_strdup (background->details->color);
}

char *
eel_background_get_image_uri (EelBackground *background)
{
    const char *filename;

    g_return_val_if_fail (EEL_IS_BACKGROUND (background), NULL);

    filename = mate_bg_get_filename (background->details->bg);
    if (filename)
    {
        return g_filename_to_uri (filename, NULL, NULL);
    }
    return NULL;
}

/* Use style->base as the default color instead of bg */
void
eel_background_set_use_base (EelBackground *background,
                             gboolean use_base)
{
    background->details->use_base = use_base;
}

void
eel_background_set_color (EelBackground *background,
                          const char *color)
{
    if (g_strcmp0 (background->details->color, color) != 0)
    {
        g_free (background->details->color);
        background->details->color = g_strdup (color);

        set_image_properties (background);
    }
}

static gboolean
eel_background_set_image_uri_helper (EelBackground *background,
                                     const char *image_uri,
                                     gboolean emit_signal)
{
    char *filename;

    if (image_uri != NULL)
    {
        filename = g_filename_from_uri (image_uri, NULL, NULL);
    }
    else
    {
        filename = NULL;
    }

    mate_bg_set_filename (background->details->bg, filename);

    if (emit_signal)
    {
        g_signal_emit (background, signals[SETTINGS_CHANGED], 0, GDK_ACTION_COPY);
    }

    set_image_properties (background);

    g_free (filename);

    return TRUE;
}

void
eel_background_set_image_uri (EelBackground *background, const char *image_uri)
{


    eel_background_set_image_uri_helper (background, image_uri, TRUE);
}

/* Use this fn to set both the image and color and avoid flash. The color isn't
 * changed till after the image is done loading, that way if an update occurs
 * before then, it will use the old color and image.
 */
static void
eel_background_set_image_uri_and_color (EelBackground *background, GdkDragAction action,
                                        const char *image_uri, const char *color)
{
    eel_background_set_image_uri_helper (background, image_uri, FALSE);
    eel_background_set_color (background, color);

    /* We always emit, even if the color didn't change, because the image change
     * relies on us doing it here.
     */

    g_signal_emit (background, signals[SETTINGS_CHANGED], 0, action);
}

void
eel_background_receive_dropped_background_image (EelBackground *background,
        GdkDragAction action,
        const char *image_uri)
{
    /* Currently, we only support tiled images. So we set the placement.
     * We rely on eel_background_set_image_uri_and_color to emit
     * the SETTINGS_CHANGED & APPEARANCE_CHANGE signals.
     */
    eel_background_set_image_placement (background, EEL_BACKGROUND_TILED);

    eel_background_set_image_uri_and_color (background, action, image_uri, NULL);
}

/**
 * eel_background_is_set:
 *
 * Check whether the background's color or image has been set.
 */
gboolean
eel_background_is_set (EelBackground *background)
{
    g_assert (EEL_IS_BACKGROUND (background));

    return background->details->color != NULL
           || mate_bg_get_filename (background->details->bg) != NULL;
}

/**
 * eel_background_reset:
 *
 * Emit the reset signal to forget any color or image that has been
 * set previously.
 */
void
eel_background_reset (EelBackground *background)
{
    g_return_if_fail (EEL_IS_BACKGROUND (background));

    g_signal_emit (background, signals[RESET], 0);
}

static void
set_root_surface (EelBackground *background,
                  GdkWindow     *window)
{
    GdkScreen *screen;

    eel_background_ensure_realized (background, window);
    screen = gdk_window_get_screen (window);

    if (background->details->use_common_surface)
    {
        background->details->background_surface_is_unset_root_surface = FALSE;
    }
    else
    {
        background->details->background_surface =
            mate_bg_create_surface (background->details->bg, window,
        			    gdk_screen_get_width (screen), gdk_screen_get_height (screen), TRUE);
    }

    mate_bg_set_surface_as_root (screen, background->details->background_surface);
}

static void
on_fade_finished (MateBGCrossfade *fade,
		  GdkWindow       *window,
		  EelBackground   *background)
{
    set_root_surface (background, window);
}

static gboolean
fade_to_surface (EelBackground   *background,
                 GdkWindow       *window,
                 cairo_surface_t *surface)
{
    if (background->details->fade == NULL)
    {
        return FALSE;
    }

    if (!mate_bg_crossfade_set_end_surface (background->details->fade,
                                            surface))
    {
        return FALSE;
    }

    if (!mate_bg_crossfade_is_started (background->details->fade))
    {
        mate_bg_crossfade_start (background->details->fade, window);
        if (background->details->is_desktop)
        {
            g_signal_connect (background->details->fade,
                              "finished",
                              G_CALLBACK (on_fade_finished), background);
        }
    }

    return mate_bg_crossfade_is_started (background->details->fade);
}


static void
eel_background_set_up_widget (EelBackground *background, GtkWidget *widget)
{
    GdkWindow *window;
    GdkWindow *widget_window;
    gboolean in_fade;

    if (!gtk_widget_get_realized (widget))
    {
        return;
    }

    widget_window = gtk_widget_get_window (widget);

    eel_background_ensure_realized (background, widget_window);
    make_color_inactive (background, &background->details->default_color);

    if (EEL_IS_CANVAS (widget))
    {
        window = gtk_layout_get_bin_window (GTK_LAYOUT (widget));
    }
    else
    {
        window = widget_window;
    }

    if (background->details->fade != NULL)
    {
        in_fade = fade_to_surface (background, window,
                                   background->details->background_surface);
    }
    else
    {
        in_fade = FALSE;
    }

    if (!in_fade)
    {
#if GTK_CHECK_VERSION (3, 0, 0)
        cairo_pattern_t *pattern;
        pattern = cairo_pattern_create_for_surface (background->details->background_surface);
        gdk_window_set_background_pattern (window, pattern);
        cairo_pattern_destroy (pattern);
#endif
        if (!background->details->is_desktop) {
            gdk_window_set_background (window, &background->details->default_color);
        }
#if !GTK_CHECK_VERSION (3, 0, 0)
        gdk_window_set_back_pixmap (window, background->details->background_surface, FALSE);
#endif
    }

    if (background->details->is_desktop && !in_fade)
    {
        set_root_surface (background, window);
    }
}

static gboolean
on_background_changed (EelBackground *background)
{
    if (background->details->change_idle_id == 0)
    {
        return FALSE;
    }

    background->details->change_idle_id = 0;

    eel_background_unrealize (background);
    eel_background_set_up_widget (background, background->details->widget);

    gtk_widget_queue_draw (background->details->widget);

    return FALSE;
}

static void
init_fade (EelBackground *background, GtkWidget *widget)
{
    if (widget == NULL || !gtk_widget_get_realized (widget))
        return;

    if (!background->details->is_desktop)
    {
        return;
    }

    if (background->details->fade == NULL)
    {
        GdkWindow *window;
        int old_width, old_height, width, height;

        /* If this was the result of a screen size change,
         * we don't want to crossfade
         */
        window = gtk_widget_get_window (widget);
        old_width = gdk_window_get_width(GDK_WINDOW(window));
        old_height = gdk_window_get_height(GDK_WINDOW(window));
        drawable_get_adjusted_size (background, window,
                                    &width, &height);
        if (old_width == width && old_height == height)
        {
            background->details->fade = mate_bg_crossfade_new (width, height);
            g_signal_connect_swapped (background->details->fade,
                                      "finished",
                                      G_CALLBACK (free_fade),
                                      background);
        }
    }

    if (background->details->fade != NULL && !mate_bg_crossfade_is_started (background->details->fade))
    {
        cairo_surface_t *start_surface;

        if (background->details->background_surface == NULL)
        {
            start_surface = mate_bg_get_surface_from_root (gtk_widget_get_screen (widget));
        }
        else
        {
            start_surface = cairo_surface_reference (background->details->background_surface);
        }
        mate_bg_crossfade_set_start_surface (background->details->fade,
                                            start_surface);
        cairo_surface_destroy (start_surface);
    }
}

static void
eel_widget_queue_background_change (GtkWidget *widget)
{
    EelBackground *background;

    background = eel_get_widget_background (widget);

    if (background->details->change_idle_id > 0)
    {
        return;
    }

    background->details->change_idle_id = g_idle_add ((GSourceFunc) on_background_changed, background);
}

/* Callback used when the style of a widget changes.  We have to regenerate its
 * EelBackgroundStyle so that it will match the chosen GTK+ theme.
 */
static void
widget_style_set_cb (GtkWidget *widget, GtkStyle *previous_style, gpointer data)
{
    EelBackground *background;

    background = EEL_BACKGROUND (data);

    if (previous_style != NULL)
    {
        eel_widget_queue_background_change (widget);
    }
}

static void
screen_size_changed (GdkScreen *screen, EelBackground *background)
{
    g_signal_emit (background, signals[APPEARANCE_CHANGED], 0);
}

static void
widget_realized_setup (GtkWidget *widget, gpointer data)
{
    EelBackground *background;

    background = EEL_BACKGROUND (data);

    if (background->details->is_desktop)
    {
        GdkWindow *root_window;
        GdkScreen *screen;

        screen = gtk_widget_get_screen (widget);

        if (background->details->screen_size_handler > 0)
        {
            g_signal_handler_disconnect (screen,
                                         background->details->screen_size_handler);
        }

        background->details->screen_size_handler =
            g_signal_connect (screen, "size_changed",
                              G_CALLBACK (screen_size_changed), background);
        if (background->details->screen_monitors_handler > 0)
        {
            g_signal_handler_disconnect (screen,
                                         background->details->screen_monitors_handler);
        }
        background->details->screen_monitors_handler =
            g_signal_connect (screen, "monitors-changed",
                              G_CALLBACK (screen_size_changed), background);

        root_window = gdk_screen_get_root_window(screen);

        if (gdk_window_get_visual(root_window) == gtk_widget_get_visual(widget))
        {
            background->details->use_common_surface = TRUE;
        }
        else
        {
            background->details->use_common_surface = FALSE;
        }

        init_fade (background, widget);
    }
}

static void
widget_realize_cb (GtkWidget *widget, gpointer data)
{
    EelBackground *background;

    background = EEL_BACKGROUND (data);

    widget_realized_setup (widget, data);

    eel_background_set_up_widget (background, widget);
}

static void
widget_unrealize_cb (GtkWidget *widget, gpointer data)
{
    EelBackground *background;

    background = EEL_BACKGROUND (data);

    if (background->details->screen_size_handler > 0)
    {
        g_signal_handler_disconnect (gtk_widget_get_screen (GTK_WIDGET (widget)),
                                     background->details->screen_size_handler);
        background->details->screen_size_handler = 0;
    }
    if (background->details->screen_monitors_handler > 0)
    {
        g_signal_handler_disconnect (gtk_widget_get_screen (GTK_WIDGET (widget)),
                                     background->details->screen_monitors_handler);
        background->details->screen_monitors_handler = 0;
    }
    background->details->use_common_surface = FALSE;
}

void
eel_background_set_desktop (EelBackground *background, GtkWidget *widget, gboolean is_desktop)
{
    background->details->is_desktop = is_desktop;

    if (gtk_widget_get_realized (widget) && background->details->is_desktop)
    {
        widget_realized_setup (widget, background);
    }

}

gboolean
eel_background_is_desktop (EelBackground *background)
{
    return background->details->is_desktop;
}

static void
on_widget_destroyed (GtkWidget *widget, EelBackground *background)
{
    if (background->details->change_idle_id != 0)
    {
        g_source_remove (background->details->change_idle_id);
        background->details->change_idle_id = 0;
    }

    background->details->widget = NULL;
}

/* Gets the background attached to a widget.

   If the widget doesn't already have a EelBackground object,
   this will create one. To change the widget's background, you can
   just call eel_background methods on the widget.

   If the widget is a canvas, nothing more needs to be done.  For
   normal widgets, you need to call eel_background_expose() from your
   expose handler to draw the background.

   Later, we might want a call to find out if we already have a background,
   or a way to share the same background among multiple widgets; both would
   be straightforward.
*/
EelBackground *
eel_get_widget_background (GtkWidget *widget)
{
    gpointer data;
    EelBackground *background;

    g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

    /* Check for an existing background. */
    data = g_object_get_data (G_OBJECT (widget), "eel_background");
    if (data != NULL)
    {
        g_assert (EEL_IS_BACKGROUND (data));
        return data;
    }

    /* Store the background in the widget's data. */
    background = eel_background_new ();
    g_object_set_data_full (G_OBJECT (widget), "eel_background",
                            background, g_object_unref);
    background->details->widget = widget;
    g_signal_connect_object (widget, "destroy", G_CALLBACK (on_widget_destroyed), background, 0);

    /* Arrange to get the signal whenever the background changes. */
    g_signal_connect_object (background, "appearance_changed",
                             G_CALLBACK (eel_widget_queue_background_change), widget, G_CONNECT_SWAPPED);
    eel_widget_queue_background_change (widget);

    g_signal_connect_object (widget, "style_set",
                             G_CALLBACK (widget_style_set_cb),
                             background,
                             0);
    g_signal_connect_object (widget, "realize",
                             G_CALLBACK (widget_realize_cb),
                             background,
                             0);
    g_signal_connect_object (widget, "unrealize",
                             G_CALLBACK (widget_unrealize_cb),
                             background,
                             0);

    return background;
}

/* determine if a background is darker or lighter than average, to help clients know what
   colors to draw on top with */
gboolean
eel_background_is_dark (EelBackground *background)
{
    GdkScreen *screen;
    GdkRectangle rect;

    /* only check for the background on the 0th monitor */
    screen = gdk_screen_get_default ();
    gdk_screen_get_monitor_geometry (screen, 0, &rect);

    return mate_bg_is_dark (background->details->bg, rect.width, rect.height);
}

/* handle dropped colors */
void
eel_background_receive_dropped_color (EelBackground *background,
                                      GtkWidget *widget,
                                      GdkDragAction action,
                                      int drop_location_x,
                                      int drop_location_y,
                                      const GtkSelectionData *selection_data)
{
    guint16 *channels;
    char *color_spec, *gradient_spec;
    char *new_gradient_spec;
    int left_border, right_border, top_border, bottom_border;
    GtkAllocation allocation;

    g_return_if_fail (EEL_IS_BACKGROUND (background));
    g_return_if_fail (GTK_IS_WIDGET (widget));
    g_return_if_fail (selection_data != NULL);

    /* Convert the selection data into a color spec. */
    if (gtk_selection_data_get_length ((GtkSelectionData *) selection_data) != 8 ||
            gtk_selection_data_get_format ((GtkSelectionData *) selection_data) != 16)
    {
        g_warning ("received invalid color data");
        return;
    }
    channels = (guint16 *) gtk_selection_data_get_data ((GtkSelectionData *) selection_data);
    color_spec = g_strdup_printf ("#%02X%02X%02X",
                                  channels[0] >> 8,
                                  channels[1] >> 8,
                                  channels[2] >> 8);

    /* Figure out if the color was dropped close enough to an edge to create a gradient.
       For the moment, this is hard-wired, but later the widget will have to have some
       say in where the borders are.
    */
    gtk_widget_get_allocation (widget, &allocation);
    left_border = 32;
    right_border = allocation.width - 32;
    top_border = 32;
    bottom_border = allocation.height - 32;

    /* If a custom background color isn't set, get the GtkStyle's bg color. */
    if (!background->details->color)
    {
        gradient_spec = gdk_color_to_string (&gtk_widget_get_style (widget)->bg[GTK_STATE_NORMAL]);
    }
    else
    {
        gradient_spec = background->details->color;
    }

    if (drop_location_x < left_border && drop_location_x <= right_border)
    {
        new_gradient_spec = eel_gradient_set_left_color_spec (gradient_spec, color_spec);
    }
    else if (drop_location_x >= left_border && drop_location_x > right_border)
    {
        new_gradient_spec = eel_gradient_set_right_color_spec (gradient_spec, color_spec);
    }
    else if (drop_location_y < top_border && drop_location_y <= bottom_border)
    {
        new_gradient_spec = eel_gradient_set_top_color_spec (gradient_spec, color_spec);
    }
    else if (drop_location_y >= top_border && drop_location_y > bottom_border)
    {
        new_gradient_spec = eel_gradient_set_bottom_color_spec (gradient_spec, color_spec);
    }
    else
    {
        new_gradient_spec = g_strdup (color_spec);
    }

    g_free (color_spec);
    g_free (gradient_spec);

    eel_background_set_image_uri_and_color (background, action, NULL, new_gradient_spec);

    g_free (new_gradient_spec);
}

void
eel_background_save_to_settings (EelBackground *background)
{
    if (background->details->bg)
        mate_bg_save_to_preferences (background->details->bg);
}

void
eel_background_set_active (EelBackground *background,
                           gboolean is_active)
{
    if (background->details->is_active != is_active)
    {
        background->details->is_active = is_active;
        set_image_properties (background);
    }
}

/* self check code */

#if !defined (EEL_OMIT_SELF_CHECK)

void
eel_self_check_background (void)
{
    EelBackground *background;

    background = eel_background_new ();

    eel_background_set_color (background, NULL);
    eel_background_set_color (background, "");
    eel_background_set_color (background, "red");
    eel_background_set_color (background, "red-blue");
    eel_background_set_color (background, "red-blue:h");

    g_object_ref_sink (background);
    g_object_unref (background);
}

#endif
