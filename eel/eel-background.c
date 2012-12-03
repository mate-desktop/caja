/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   eel-background.c: Object for the background of a widget.

   Copyright (C) 2000 Eazel, Inc.
   Copyright (C) 2012 Jasmine Hassan <jasmine.aura@gmail.com>

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

   Authors: Darin Adler <darin@eazel.com>
            Jasmine Hassan <jasmine.aura@gmail.com>
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
#include <libcaja-private/caja-global-preferences.h>

#if !GTK_CHECK_VERSION(3, 0, 0)
#define cairo_surface_t         GdkPixmap
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
    GtkWidget *widget;
    MateBG *bg;
    char *color;

    /* Realized data: */
    cairo_surface_t *bg_surface;
    gboolean unset_root_surface;
    MateBGCrossfade *fade;
    int bg_entire_width;
    int bg_entire_height;
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
free_fade (EelBackground *self)
{
    if (self->details->fade != NULL)
    {
        g_object_unref (self->details->fade);
        self->details->fade = NULL;
    }
}

static void
free_background_surface (EelBackground *self)
{
    cairo_surface_t *surface;

    surface = self->details->bg_surface;
    if (surface != NULL)
    {
        /* If we created a root surface and didn't set it as background
           it will live forever, so we need to kill it manually.
           If set as root background it will be killed next time the
           background is changed. */
        if (self->details->unset_root_surface)
        {
            XKillClient (cairo_xlib_surface_get_display (surface),
                         cairo_xlib_surface_get_drawable (surface));
        }
        cairo_surface_destroy (surface);
        self->details->bg_surface = NULL;
    }
}

static void
eel_background_finalize (GObject *object)
{
    EelBackground *self = EEL_BACKGROUND (object);

    g_free (self->details->color);

    if (self->details->bg != NULL) {
        g_object_unref (G_OBJECT (self->details->bg));
        self->details->bg = NULL;
    }

    free_background_surface (self);

    free_fade (self);

    G_OBJECT_CLASS (eel_background_parent_class)->finalize (object);
}

static void
eel_background_unrealize (EelBackground *self)
{
    free_background_surface (self);

    self->details->bg_entire_width = 0;
    self->details->bg_entire_height = 0;
    self->details->default_color.red = 0xffff;
    self->details->default_color.green = 0xffff;
    self->details->default_color.blue = 0xffff;
}

#define CLAMP_COLOR(v) (t = (v), CLAMP (t, 0, G_MAXUSHORT))
#define SATURATE(v) ((1.0 - saturation) * intensity + saturation * (v))

static void
make_color_inactive (EelBackground *self,
		     GdkColor      *color)
{
    double intensity, saturation;
    gushort t;

    if (self->details->is_active) {
        return;
    }

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

gchar *
eel_bg_get_desktop_color (EelBackground *self)
{
    MateBGColorType type;
    GdkColor   primary, secondary;
    char      *start_color, *end_color, *color_spec;
    gboolean   use_gradient = TRUE;
    gboolean   is_horizontal = FALSE;

    mate_bg_get_color (self->details->bg, &type, &primary, &secondary);

    if (type == MATE_BG_COLOR_V_GRADIENT)
    {
        is_horizontal = FALSE;
    }
    else if (type == MATE_BG_COLOR_H_GRADIENT)
    {
        is_horizontal = TRUE;
    }
    else	/* implicit (type == MATE_BG_COLOR_SOLID) */
    {
        use_gradient = FALSE;
    }

    start_color = eel_gdk_rgb_to_color_spec (eel_gdk_color_to_rgb (&primary));

    if (use_gradient)
    {
        end_color  = eel_gdk_rgb_to_color_spec (eel_gdk_color_to_rgb (&secondary));
        color_spec = eel_gradient_new (start_color, end_color, is_horizontal);
        g_free (end_color);
    }
    else
    {
        color_spec = g_strdup (start_color);
    }
    g_free (start_color);

    return color_spec;
}

static void
set_image_properties (EelBackground *self)
{
    GdkColor c;

    if (self->details->is_desktop && !self->details->color)
        self->details->color = eel_bg_get_desktop_color (self);

    if (!self->details->color)
    {
        c = self->details->default_color;
        make_color_inactive (self, &c);
        mate_bg_set_color (self->details->bg, MATE_BG_COLOR_SOLID, &c, NULL);
    }
    else if (!eel_gradient_is_gradient (self->details->color))
    {
        eel_gdk_color_parse_with_white_default (self->details->color, &c);
        make_color_inactive (self, &c);
        mate_bg_set_color (self->details->bg, MATE_BG_COLOR_SOLID, &c, NULL);
    }
    else
    {
        GdkColor c1, c2;
        char *spec;

        spec = eel_gradient_get_start_color_spec (self->details->color);
        eel_gdk_color_parse_with_white_default (spec, &c1);
        make_color_inactive (self, &c1);
        g_free (spec);

        spec = eel_gradient_get_end_color_spec (self->details->color);
        eel_gdk_color_parse_with_white_default (spec, &c2);
        make_color_inactive (self, &c2);
        g_free (spec);

        if (eel_gradient_is_horizontal (self->details->color)) {
            mate_bg_set_color (self->details->bg, MATE_BG_COLOR_H_GRADIENT, &c1, &c2);
        } else {
            mate_bg_set_color (self->details->bg, MATE_BG_COLOR_V_GRADIENT, &c1, &c2);
        }
    }
}

gchar *
eel_background_get_color (EelBackground *self)
{
    g_return_val_if_fail (EEL_IS_BACKGROUND (self), NULL);

    return g_strdup (self->details->color);
}

/* @color: color or gradient to set */
void
eel_background_set_color (EelBackground *self,
                          const gchar   *color)
{
    if (g_strcmp0 (self->details->color, color) != 0)
    {
        g_free (self->details->color);
        self->details->color = g_strdup (color);

        set_image_properties (self);
    }
}

/* Use style->base as the default color instead of bg */
void
eel_background_set_use_base (EelBackground *self,
                             gboolean       use_base)
{
    self->details->use_base = use_base;
}

static void
drawable_get_adjusted_size (EelBackground *self,
                            int		  *width,
                            int	          *height)
{
    if (self->details->is_desktop)
    {
        GdkScreen *screen = gtk_widget_get_screen (self->details->widget);
        *width = gdk_screen_get_width (screen);
        *height = gdk_screen_get_height (screen);
    }
    else
    {
        GdkWindow *window = gtk_widget_get_window (self->details->widget);
        *width = gdk_window_get_width (window);
        *height = gdk_window_get_height (window);
    }
}

static gboolean
eel_background_ensure_realized (EelBackground *self)
{
    GtkStyle *style;
    int width, height;
    GdkWindow *window;

    /* Set the default color */
    style = gtk_widget_get_style (self->details->widget);
    if (self->details->use_base) {
       self->details->default_color = style->base[GTK_STATE_NORMAL];
    } else {
        self->details->default_color = style->bg[GTK_STATE_NORMAL];
    }

    /* If the window size is the same as last time, don't update */
    drawable_get_adjusted_size (self, &width, &height);
    if (width == self->details->bg_entire_width &&
        height == self->details->bg_entire_height)
    {
        return FALSE;
    }

    free_background_surface (self);

    /* Calls mate_bg_set_color, which sets "ignore-pending-change" to false,
       and queues emission of changed signal if it's still false */
    set_image_properties (self);

    window = gtk_widget_get_window (self->details->widget);
    self->details->bg_surface = mate_bg_create_surface (self->details->bg,
        						window, width, height,
        						self->details->is_desktop);
    self->details->unset_root_surface = self->details->is_desktop;

    /* We got the surface and everything, so we don't care about a change
       that is pending (unless things actually change after this time) */
    g_object_set_data (G_OBJECT (self->details->bg),
                       "ignore-pending-change",
                       GINT_TO_POINTER (TRUE));

    self->details->bg_entire_width = width;
    self->details->bg_entire_height = height;

    return TRUE;
}

void
eel_background_draw (GtkWidget *widget,
#  if GTK_CHECK_VERSION (3, 0, 0)
                     cairo_t   *cr)
{
#  else
                     GdkEventExpose *event)
{
#  endif
    int width, height;
    GdkWindow *window = gtk_widget_get_window (widget);

#  if !GTK_CHECK_VERSION (3, 0, 0)
    if (event->window != window)
        return;
#  endif

    EelBackground *self = eel_get_widget_background (widget);

    drawable_get_adjusted_size (self, &width, &height);

    eel_background_ensure_realized (self);
    make_color_inactive (self, &self->details->default_color);

#  if GTK_CHECK_VERSION(3,0,0)
    cairo_save (cr);
#  else
    cairo_t *cr = gdk_cairo_create (window);
#  endif

    if (self->details->bg_surface != NULL) {
        cairo_set_source_surface (cr, self->details->bg_surface, 0, 0);
        cairo_pattern_set_extend (cairo_get_source (cr), CAIRO_EXTEND_REPEAT);
    } else {
        gdk_cairo_set_source_color (cr, &self->details->default_color);
    }

#  if !GTK_CHECK_VERSION (3, 0, 0)
    gdk_cairo_rectangle (cr, &event->area);
    cairo_clip (cr);
#  endif

    cairo_rectangle (cr, 0, 0, width, height);
    cairo_fill (cr);

#  if GTK_CHECK_VERSION(3,0,0)
    cairo_restore (cr);
#  else
    cairo_destroy (cr);
#  endif
}

static void
set_root_surface (EelBackground *self,
                  GdkWindow     *window,
                  GdkScreen     *screen)
{
    eel_background_ensure_realized (self);

    if (self->details->use_common_surface) {
        self->details->unset_root_surface = FALSE;
    } else {
        int width, height;
        drawable_get_adjusted_size (self, &width, &height);
        self->details->bg_surface = mate_bg_create_surface (self->details->bg, window,
        						    width, height, TRUE);
    }

    if (self->details->bg_surface != NULL)
        mate_bg_set_surface_as_root (screen, self->details->bg_surface);
}

static void
init_fade (EelBackground *self)
{
    GtkWidget *widget = self->details->widget;
    gboolean do_fade;

    if (!self->details->is_desktop || widget == NULL || !gtk_widget_get_realized (widget)) {
        return;
    }

    do_fade = g_settings_get_boolean (mate_background_preferences,
                                      MATE_BG_KEY_BACKGROUND_FADE);
    if (!do_fade) {
    	return;
    }

    if (self->details->fade == NULL)
    {
        int width, height;

        /* If this was the result of a screen size change,
         * we don't want to crossfade
         */
        drawable_get_adjusted_size (self, &width, &height);
        if (width == self->details->bg_entire_width &&
            height == self->details->bg_entire_height)
        {
            self->details->fade = mate_bg_crossfade_new (width, height);
            g_signal_connect_swapped (self->details->fade,
                                      "finished",
                                      G_CALLBACK (free_fade),
                                      self);
        }
    }

    if (self->details->fade != NULL && !mate_bg_crossfade_is_started (self->details->fade))
    {
        if (self->details->bg_surface == NULL)
        {
            cairo_surface_t *start_surface;
            start_surface = mate_bg_get_surface_from_root (gtk_widget_get_screen (widget));
            mate_bg_crossfade_set_start_surface (self->details->fade, start_surface);
            cairo_surface_destroy (start_surface);
        }
        else
        {
            mate_bg_crossfade_set_start_surface (self->details->fade, self->details->bg_surface);
        }
    }
}

static void
on_fade_finished (MateBGCrossfade *fade,
                  GdkWindow       *window,
		  gpointer         user_data)
{
    EelBackground *self = EEL_BACKGROUND (user_data);

    set_root_surface (self, window, gdk_window_get_screen (window));
}

static gboolean
fade_to_surface (EelBackground   *self,
                 GdkWindow       *window,
                 cairo_surface_t *surface)
{
    if (self->details->fade == NULL ||
        !mate_bg_crossfade_set_end_surface (self->details->fade, surface))
    {
        return FALSE;
    }

    if (!mate_bg_crossfade_is_started (self->details->fade))
    {
        mate_bg_crossfade_start (self->details->fade, window);
        if (self->details->is_desktop)
        {
            g_signal_connect (self->details->fade,
                              "finished",
                              G_CALLBACK (on_fade_finished), self);
        }
    }

    return mate_bg_crossfade_is_started (self->details->fade);
}

static void
eel_background_set_up_widget (EelBackground *self)
{
    GdkWindow *window;
    GtkWidget *widget = self->details->widget;
    gboolean in_fade = FALSE;

    if (!gtk_widget_get_realized (widget))
        return;

    eel_background_ensure_realized (self);
    make_color_inactive (self, &self->details->default_color);

    if (self->details->bg_surface == NULL)
        return;

    if (EEL_IS_CANVAS (widget)) {
        window = gtk_layout_get_bin_window (GTK_LAYOUT (widget));
    } else {
        window = gtk_widget_get_window (widget);
    }

    if (self->details->fade != NULL)
        in_fade = fade_to_surface (self, window, self->details->bg_surface);

    if (!in_fade)
    {
#  if GTK_CHECK_VERSION (3, 0, 0)
        cairo_pattern_t *pattern;
        pattern = cairo_pattern_create_for_surface (self->details->bg_surface);
        gdk_window_set_background_pattern (window, pattern);
        cairo_pattern_destroy (pattern);
#  endif

        if (self->details->is_desktop)
        {
#  if !GTK_CHECK_VERSION (3, 0, 0)
            gdk_window_set_back_pixmap (window, self->details->bg_surface, FALSE);
#  endif
            set_root_surface (self, window, gtk_widget_get_screen (widget));
        }
        else
        {
            gdk_window_set_background (window, &self->details->default_color);
#  if !GTK_CHECK_VERSION (3, 0, 0)
            gdk_window_set_back_pixmap (window, self->details->bg_surface, FALSE);
#  endif
        }

        gdk_window_invalidate_rect (window, NULL, TRUE);
    }
}

static gboolean
background_changed_cb (EelBackground *self)
{
    if (self->details->change_idle_id == 0) {
        return FALSE;
    }
    self->details->change_idle_id = 0;

    eel_background_unrealize (self);
    eel_background_set_up_widget (self);

    gtk_widget_queue_draw (self->details->widget);

    return FALSE;
}

static void
widget_queue_background_change (GtkWidget *widget,
                                gpointer   user_data)
{
    EelBackground *self = EEL_BACKGROUND (user_data);

    if (self->details->change_idle_id != 0)
    {
        g_source_remove (self->details->change_idle_id);
    }

    self->details->change_idle_id =
          g_idle_add ((GSourceFunc) background_changed_cb, self);
}

/* Callback used when the style of a widget changes.  We have to regenerate its
 * EelBackgroundStyle so that it will match the chosen GTK+ theme.
 */
static void
widget_style_set_cb (GtkWidget *widget,
                     GtkStyle  *previous_style,
                     gpointer   user_data)
{
    if (previous_style != NULL)
        widget_queue_background_change (widget, user_data);
}

static void
eel_background_changed (MateBG *bg,
                        gpointer user_data)
{
    EelBackground *self = EEL_BACKGROUND (user_data);

    init_fade (self);
    g_signal_emit (G_OBJECT (self), signals[APPEARANCE_CHANGED], 0);
}

static void
eel_background_transitioned (MateBG *bg, gpointer user_data)
{
    EelBackground *self = EEL_BACKGROUND (user_data);

    free_fade (self);
    g_signal_emit (G_OBJECT (self), signals[APPEARANCE_CHANGED], 0);
}

static void
screen_size_changed (GdkScreen *screen, EelBackground *background)
{
    int w, h;

    drawable_get_adjusted_size (background, &w, &h);
    if (w != background->details->bg_entire_width ||
        h != background->details->bg_entire_height)
    {
        g_signal_emit (background, signals[APPEARANCE_CHANGED], 0);
    }
}

static void
widget_realized_setup (GtkWidget     *widget,
                       EelBackground *self)
{
    if (!self->details->is_desktop) {
        return;
    }

    GdkScreen *screen = gtk_widget_get_screen (widget);
    GdkWindow *window = gdk_screen_get_root_window (screen);

    if (self->details->screen_size_handler > 0)
    {
        g_signal_handler_disconnect (screen, self->details->screen_size_handler);
    }
    self->details->screen_size_handler =
        g_signal_connect (screen, "size_changed", G_CALLBACK (screen_size_changed), self);

    if (self->details->screen_monitors_handler > 0)
    {
        g_signal_handler_disconnect (screen, self->details->screen_monitors_handler);
    }
    self->details->screen_monitors_handler =
        g_signal_connect (screen, "monitors-changed", G_CALLBACK (screen_size_changed), self);

    self->details->use_common_surface =
        (gdk_window_get_visual (window) == gtk_widget_get_visual (widget)) ? TRUE : FALSE;

    init_fade (self);
}

static void
widget_realize_cb (GtkWidget *widget,
                   gpointer   user_data)
{
    EelBackground *self = EEL_BACKGROUND (user_data);

    widget_realized_setup (widget, self);

    eel_background_set_up_widget (self);    
}

static void
widget_unrealize_cb (GtkWidget *widget,
                     gpointer   user_data)
{
    EelBackground *self = EEL_BACKGROUND (user_data);

    if (self->details->screen_size_handler > 0) {
        g_signal_handler_disconnect (gtk_widget_get_screen (GTK_WIDGET (widget)),
                                     self->details->screen_size_handler);
        self->details->screen_size_handler = 0;
    }

    if (self->details->screen_monitors_handler > 0) {
        g_signal_handler_disconnect (gtk_widget_get_screen (GTK_WIDGET (widget)),
                                     self->details->screen_monitors_handler);
        self->details->screen_monitors_handler = 0;
    }

    self->details->use_common_surface = FALSE;
}

static void
on_widget_destroyed (GtkWidget *widget,
                     gpointer   user_data)
{
    EelBackground *self = EEL_BACKGROUND (user_data);

    if (self->details->change_idle_id != 0) {
        g_source_remove (self->details->change_idle_id);
        self->details->change_idle_id = 0;
    }

    free_fade (self);
    self->details->widget = NULL;
}

/* Gets the background attached to a widget.

   If the widget doesn't already have a EelBackground object,
   this will create one. To change the widget's background, you can
   just call eel_background methods on the widget.

   If the widget is a canvas, nothing more needs to be done.  For
   normal widgets, you need to call eel_background_draw() from your
   draw/expose handler to draw the background.

   Later, we might want a call to find out if we already have a background,
   or a way to share the same background among multiple widgets; both would
   be straightforward.
*/
EelBackground *
eel_get_widget_background (GtkWidget *widget)
{

    g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

    /* Check for an existing background. */
    gpointer data = g_object_get_data (G_OBJECT (widget), "eel_background");
    if (data != NULL)
    {
        g_assert (EEL_IS_BACKGROUND (data));
        return data;
    }

    /* Store the background in the widget's data. */
    EelBackground *self = eel_background_new ();
    g_object_set_data_full (G_OBJECT (widget), "eel_background",
                            self, g_object_unref);
    self->details->widget = widget;

    g_signal_connect_object (widget, "destroy",
                             G_CALLBACK (on_widget_destroyed), self, 0);
    g_signal_connect_object (widget, "realize",
                             G_CALLBACK (widget_realize_cb), self, 0);
    g_signal_connect_object (widget, "unrealize",
                             G_CALLBACK (widget_unrealize_cb), self, 0);

    g_signal_connect_object (widget, "style_set",
                             G_CALLBACK (widget_style_set_cb), self, 0);

    /* Arrange to get the signal whenever the background changes. */
    g_signal_connect_object (self, "appearance_changed",
                             G_CALLBACK (widget_queue_background_change),
                             widget, G_CONNECT_SWAPPED);
    widget_queue_background_change (widget, self);

    return self;
}

static void
eel_background_class_init (EelBackgroundClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    signals[APPEARANCE_CHANGED] =
        g_signal_new ("appearance_changed", G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
                      G_STRUCT_OFFSET (EelBackgroundClass, appearance_changed),
                      NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    signals[SETTINGS_CHANGED] =
        g_signal_new ("settings_changed", G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
                      G_STRUCT_OFFSET (EelBackgroundClass, settings_changed),
                      NULL, NULL, g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

    signals[RESET] =
        g_signal_new ("reset", G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
                      G_STRUCT_OFFSET (EelBackgroundClass, reset),
                      NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    object_class->finalize = eel_background_finalize;

    g_type_class_add_private (klass, sizeof (EelBackgroundDetails));
}

static void
eel_background_init (EelBackground *self)
{
    self->details =
          G_TYPE_INSTANCE_GET_PRIVATE (self,
        			       EEL_TYPE_BACKGROUND,
        			       EelBackgroundDetails);

    self->details->bg = mate_bg_new ();
    self->details->default_color.red = 0xffff;
    self->details->default_color.green = 0xffff;
    self->details->default_color.blue = 0xffff;
    self->details->is_active = TRUE;

    g_signal_connect (self->details->bg, "changed",
                      G_CALLBACK (eel_background_changed), self);
    g_signal_connect (self->details->bg, "transitioned",
                      G_CALLBACK (eel_background_transitioned), self);

}

/**
 * eel_background_is_set:
 *
 * Check whether the background's color or image has been set.
 */
gboolean
eel_background_is_set (EelBackground *self)
{
    g_assert (EEL_IS_BACKGROUND (self));

    return self->details->color != NULL ||
           mate_bg_get_filename (self->details->bg) != NULL;
}

/**
 * eel_background_reset:
 *
 * Emit the reset signal to forget any color or image that has been
 * set previously.
 */
void
eel_background_reset (EelBackground *self)
{
    g_return_if_fail (EEL_IS_BACKGROUND (self));

    g_signal_emit (self, signals[RESET], 0);
}

void
eel_background_set_desktop (EelBackground *self,
                            GtkWidget     *widget,
                            gboolean       is_desktop)
{
    self->details->is_desktop = is_desktop;

    if (gtk_widget_get_realized (widget) && is_desktop)
    {
        widget_realized_setup (widget, self);
    }
}

gboolean
eel_background_is_desktop (EelBackground *self)
{
    return self->details->is_desktop;
}

void
eel_background_set_active (EelBackground *self,
                           gboolean is_active)
{
    if (self->details->is_active != is_active)
    {
        self->details->is_active = is_active;
        set_image_properties (self);
    }
}

/* determine if a background is darker or lighter than average, to help clients know what
   colors to draw on top with */
gboolean
eel_background_is_dark (EelBackground *self)
{
    GdkRectangle rect;

    /* only check for the background on the 0th monitor */
    GdkScreen *screen = gdk_screen_get_default ();
    gdk_screen_get_monitor_geometry (screen, 0, &rect);

    return mate_bg_is_dark (self->details->bg, rect.width, rect.height);
}

gchar *
eel_background_get_image_uri (EelBackground *self)
{
    g_return_val_if_fail (EEL_IS_BACKGROUND (self), NULL);

    const gchar *filename = mate_bg_get_filename (self->details->bg);

    if (filename) {
        return g_filename_to_uri (filename, NULL, NULL);
    }
    return NULL;
}

static void
eel_bg_set_image_uri_helper (EelBackground *self,
                             const gchar   *image_uri,
                             gboolean       emit_signal)
{
    gchar *filename = g_strdup ("");	/* GSettings expects a string, not NULL */

    if (image_uri != NULL)
        filename = g_filename_from_uri (image_uri, NULL, NULL);

    mate_bg_set_filename (self->details->bg, filename);

    if (emit_signal)
        g_signal_emit (self, signals[SETTINGS_CHANGED], 0, GDK_ACTION_COPY);

    set_image_properties (self);

    g_free (filename);
}

/* Use this function to set an image only (no color).
 * Also, to unset an image, use: eel_bg_set_image_uri (background, NULL)
 */
void
eel_background_set_image_uri (EelBackground *self,
                      const gchar   *image_uri)
{
    eel_bg_set_image_uri_helper (self, image_uri, TRUE);
}

/* Use this fn to set both the image and color and avoid flash. The color isn't
 * changed till after the image is done loading, that way if an update occurs
 * before then, it will use the old color and image.
 */
static void
eel_bg_set_image_uri_and_color (EelBackground *self,
                                GdkDragAction  action,
                                const gchar   *image_uri,
                                const gchar   *color)
{
    if (self->details->is_desktop &&
        !mate_bg_get_draw_background (self->details->bg))
    {
        mate_bg_set_draw_background (self->details->bg, TRUE);
    }

    eel_bg_set_image_uri_helper (self, image_uri, FALSE);
    eel_background_set_color (self, color);

    /* We always emit, even if the color didn't change, because the image change
       relies on us doing it here. */
    g_signal_emit (self, signals[SETTINGS_CHANGED], 0, action);
}

void
eel_bg_set_placement (EelBackground   *self,
		      MateBGPlacement  placement)
{
    if (self->details->bg)
        mate_bg_set_placement (self->details->bg,
        		       placement);
}

void
eel_bg_save_to_gsettings (EelBackground *self,
			  GSettings     *settings)
{
    if (self->details->bg)
        mate_bg_save_to_gsettings (self->details->bg,
        			   settings);
}

void
eel_bg_load_from_gsettings (EelBackground *self,
			    GSettings     *settings)
{
    if (self->details->bg)
        mate_bg_load_from_gsettings (self->details->bg,
        			     settings);
}

void
eel_bg_load_from_system_gsettings (EelBackground *self,
				   GSettings     *settings,
				   gboolean       apply)
{
    if (self->details->bg)
        mate_bg_load_from_system_gsettings (self->details->bg,
        				    settings,
        				    apply);
}

/* handle dropped images */
void
eel_background_set_dropped_image (EelBackground *self,
                                  GdkDragAction  action,
                                  const gchar   *image_uri)
{
    /* Currently, we only support tiled images. So we set the placement. */
    mate_bg_set_placement (self->details->bg, MATE_BG_PLACEMENT_TILED);

    eel_bg_set_image_uri_and_color (self, action, image_uri, NULL);
}

/* handle dropped colors */
void
eel_background_set_dropped_color (EelBackground *self,
                                  GtkWidget     *widget,
                                  GdkDragAction  action,
                                  int            drop_location_x,
                                  int            drop_location_y,
                                  const GtkSelectionData *selection_data)
{
    guint16 *channels;
    char *color_spec, *gradient_spec;
    char *new_gradient_spec;
    int left_border, right_border, top_border, bottom_border;
    GtkAllocation allocation;

    g_return_if_fail (EEL_IS_BACKGROUND (self));
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
    if (!self->details->color)
    {
        gradient_spec = gdk_color_to_string (&gtk_widget_get_style (widget)->bg[GTK_STATE_NORMAL]);
    }
    else
    {
        gradient_spec = g_strdup (self->details->color);
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

    eel_bg_set_image_uri_and_color (self, action, NULL, new_gradient_spec);

    g_free (new_gradient_spec);
}

EelBackground *
eel_background_new (void)
{
    return EEL_BACKGROUND (g_object_new (EEL_TYPE_BACKGROUND, NULL));
}


/*
 * self check code
 */

#if !defined (EEL_OMIT_SELF_CHECK)

void
eel_self_check_background (void)
{
    EelBackground *self = eel_background_new ();

    eel_background_set_color (self, NULL);
    eel_background_set_color (self, "");
    eel_background_set_color (self, "red");
    eel_background_set_color (self, "red-blue");
    eel_background_set_color (self, "red-blue:h");

    g_object_ref_sink (self);
    g_object_unref (self);
}

#endif
