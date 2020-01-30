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
#include <gtk/gtk.h>
#include <cairo-xlib.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <math.h>
#include <stdio.h>

#include "eel-background.h"
#include "eel-gdk-extensions.h"
#include "eel-glib-extensions.h"
#include "eel-lib-self-check-functions.h"
#include "eel-canvas.h"

enum
{
    APPEARANCE_CHANGED,
    SETTINGS_CHANGED,
    RESET,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct EelBackgroundPrivate
{
    GtkWidget *widget;
    GtkWidget *front_widget;
    MateBG *bg;
    char *color;

    /* Realized data: */
    cairo_surface_t *bg_surface;
    gboolean unset_root_surface;
    MateBGCrossfade *fade;
    int bg_entire_width;
    int bg_entire_height;
    GdkRGBA default_color;
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

static GList *desktop_bg_objects = NULL;

G_DEFINE_TYPE_WITH_PRIVATE (EelBackground, eel_background, G_TYPE_OBJECT)

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

    if (self->details->is_desktop)
    {
        desktop_bg_objects = g_list_remove (desktop_bg_objects,
                                            G_OBJECT (self));
    }

    G_OBJECT_CLASS (eel_background_parent_class)->finalize (object);
}

static void
eel_background_unrealize (EelBackground *self)
{
    free_background_surface (self);

    self->details->bg_entire_width = 0;
    self->details->bg_entire_height = 0;
    self->details->default_color.red = 1.0;
    self->details->default_color.green = 1.0;
    self->details->default_color.blue = 1.0;
    self->details->default_color.alpha = 1.0;
}

#define CLAMP_COLOR(v) (CLAMP ((v), 0, 1))
#define SATURATE(v) ((1.0 - saturation) * intensity + saturation * (v))

static void
make_color_inactive (EelBackground *self,
                     GdkRGBA       *color)
{
    if (!self->details->is_active) {
        double intensity, saturation;

        saturation = 0.7;
        intensity = color->red * 0.30 + color->green * 0.59 + color->blue * 0.11;
        color->red = SATURATE (color->red);
        color->green = SATURATE (color->green);
        color->blue = SATURATE (color->blue);

        if (intensity > 0.5)
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

gchar *
eel_bg_get_desktop_color (EelBackground *self)
{
    MateBGColorType type;
    GdkRGBA    primary, secondary;
    char      *start_color, *color_spec;
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

    start_color = eel_gdk_rgb_to_color_spec (eel_gdk_rgba_to_rgb (&primary));

    if (use_gradient)
    {
        char *end_color;

        end_color  = eel_gdk_rgb_to_color_spec (eel_gdk_rgba_to_rgb (&secondary));
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
    GdkRGBA c;

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
        eel_gdk_rgba_parse_with_white_default (&c, self->details->color);
        make_color_inactive (self, &c);
        mate_bg_set_color (self->details->bg, MATE_BG_COLOR_SOLID, &c, NULL);
    }
    else
    {
        GdkRGBA c1, c2;
        char *spec;

        spec = eel_gradient_get_start_color_spec (self->details->color);
        eel_gdk_rgba_parse_with_white_default (&c1, spec);
        make_color_inactive (self, &c1);
        g_free (spec);

        spec = eel_gradient_get_end_color_spec (self->details->color);
        eel_gdk_rgba_parse_with_white_default (&c2, spec);
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
        gint scale = gtk_widget_get_scale_factor (self->details->widget);
        *width = WidthOfScreen (gdk_x11_screen_get_xscreen (screen)) / scale;
        *height = HeightOfScreen (gdk_x11_screen_get_xscreen (screen)) / scale;
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
    int width, height;
    GdkWindow *window;
    GtkStyleContext *style;
    GdkRGBA *c;

    /* Set the default color */
    style = gtk_widget_get_style_context (self->details->widget);
    gtk_style_context_save (style);
    gtk_style_context_set_state (style, GTK_STATE_FLAG_NORMAL);
    if (self->details->use_base) {
        gtk_style_context_add_class (style, GTK_STYLE_CLASS_VIEW);
    }

    gtk_style_context_get (style, gtk_style_context_get_state (style),
                           GTK_STYLE_PROPERTY_BACKGROUND_COLOR,
                           &c, NULL);
    self->details->default_color = *c;
    gdk_rgba_free (c);

    gtk_style_context_restore (style);

    /* If the window size is the same as last time, don't update */
    drawable_get_adjusted_size (self, &width, &height);
    if (width == self->details->bg_entire_width &&
        height == self->details->bg_entire_height)
    {
        return FALSE;
    }

    free_background_surface (self);

    set_image_properties (self);

    window = gtk_widget_get_window (self->details->widget);
    self->details->bg_surface = mate_bg_create_surface (self->details->bg,
        						window, width, height,
        						self->details->is_desktop);
    self->details->unset_root_surface = self->details->is_desktop;

    self->details->bg_entire_width = width;
    self->details->bg_entire_height = height;

    return TRUE;
}

void
eel_background_draw (GtkWidget *widget,
                     cairo_t   *cr)
{
    EelBackground *self = eel_get_widget_background (widget);
    GdkRGBA color;
    int width, height;

    if (self->details->fade != NULL &&
        mate_bg_crossfade_is_started (self->details->fade))
    {
        return;
    }

    drawable_get_adjusted_size (self, &width, &height);

    eel_background_ensure_realized (self);
    color = self->details->default_color;
    make_color_inactive (self, &color);

    cairo_save (cr);

    if (self->details->bg_surface != NULL)
    {
        cairo_set_source_surface (cr, self->details->bg_surface, 0, 0);
        cairo_pattern_set_extend (cairo_get_source (cr), CAIRO_EXTEND_REPEAT);
    } else {
        gdk_cairo_set_source_rgba (cr, &color);
    }

    cairo_rectangle (cr, 0, 0, width, height);
    cairo_fill (cr);

    cairo_restore (cr);
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
    GSettings *mate_background_preferences;
    GtkWidget *widget = self->details->widget;
    gboolean do_fade;

    if (!self->details->is_desktop || widget == NULL || !gtk_widget_get_realized (widget)) {
        return;
    }

    mate_background_preferences = g_settings_new ("org.mate.background");
    do_fade = g_settings_get_boolean (mate_background_preferences,
                                      MATE_BG_KEY_BACKGROUND_FADE);
    g_object_unref (mate_background_preferences);

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
                 GtkWidget       *widget,
                 cairo_surface_t *surface)
{
    if (self->details->fade == NULL ||
        !mate_bg_crossfade_set_end_surface (self->details->fade, surface))
    {
        return FALSE;
    }

    if (!mate_bg_crossfade_is_started (self->details->fade))
    {
        mate_bg_crossfade_start_widget (self->details->fade, widget);

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
    GtkWidget *widget = self->details->widget;

    gboolean in_fade = FALSE;

    if (!gtk_widget_get_realized (widget))
        return;

    eel_background_ensure_realized (self);

    if (self->details->bg_surface == NULL)
        return;

    gtk_widget_queue_draw (widget);

    if (self->details->fade != NULL)
        in_fade = fade_to_surface (self, widget, self->details->bg_surface);

    if (!in_fade)
    {
        GdkWindow *window;

        if (EEL_IS_CANVAS (widget))
        {
            window = gtk_layout_get_bin_window (GTK_LAYOUT (widget));
        }
        else
        {
            window = gtk_widget_get_window (widget);
        }

        if (self->details->is_desktop)
        {
            set_root_surface (self, window, gtk_widget_get_screen (widget));
        }
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
widget_style_updated_cb (GtkWidget *widget,
                         gpointer   user_data)
{
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
    self->details->front_widget = NULL;
}

/* Gets the background attached to a widget.

   If the widget doesn't already have a EelBackground object,
   this will create one. To change the widget's background, you can
   just call eel_background methods on the widget.

   You need to call eel_background_draw() from your draw event handler to
   draw the background.

   Later, we might want a call to find out if we already have a background,
   or a way to share the same background among multiple widgets; both would
   be straightforward.
*/
EelBackground *
eel_get_widget_background (GtkWidget *widget)
{
    EelBackground *self;
    gpointer data;
    GList *l;

    g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

    /* Check for an existing background. */
    data = g_object_get_data (G_OBJECT (widget), "eel_background");
    if (data != NULL)
    {
        g_assert (EEL_IS_BACKGROUND (data));
        return data;
    }

    /* Check for an existing desktop window background. */
    for (l = desktop_bg_objects; l != NULL; l = l->next)
    {
        g_assert (EEL_IS_BACKGROUND (l->data));
        self = EEL_BACKGROUND (l->data);
        if (widget == self->details->widget)
        {
            return self;
        }
    }

    self = eel_background_new ();
    self->details->widget = widget;
    self->details->front_widget = widget;

    /* Store the background in the widget's data. */
    g_object_set_data_full (G_OBJECT (widget), "eel_background",
                            self, g_object_unref);

    g_signal_connect_object (widget, "destroy",
                             G_CALLBACK (on_widget_destroyed), self, 0);
    g_signal_connect_object (widget, "realize",
                             G_CALLBACK (widget_realize_cb), self, 0);
    g_signal_connect_object (widget, "unrealize",
                             G_CALLBACK (widget_unrealize_cb), self, 0);

    g_signal_connect_object (widget, "style-updated",
                             G_CALLBACK (widget_style_updated_cb), self, 0);

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
}

static void
eel_background_init (EelBackground *self)
{
    self->details = eel_background_get_instance_private(self);

    self->details->bg = mate_bg_new ();
    self->details->default_color.red = 1.0;
    self->details->default_color.green = 1.0;
    self->details->default_color.blue = 1.0;
    self->details->default_color.alpha = 1.0;
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
                            gboolean       is_desktop)
{
    self->details->is_desktop = is_desktop;

    if (is_desktop)
    {
        self->details->widget =
          gtk_widget_get_toplevel (self->details->front_widget);

        desktop_bg_objects = g_list_prepend (desktop_bg_objects,
                                             G_OBJECT (self));

        if (gtk_widget_get_realized (self->details->widget))
        {
            widget_realized_setup (self->details->widget, self);
        }
    }
    else
    {
        desktop_bg_objects = g_list_remove (desktop_bg_objects,
                                            G_OBJECT (self));
        self->details->widget = self->details->front_widget;
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
        gtk_widget_queue_draw (self->details->widget);
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
    GdkDisplay *display = gdk_screen_get_display (screen);
    gdk_monitor_get_geometry (gdk_display_get_monitor (display, 0), &rect);

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
    gchar *filename;

    if (image_uri != NULL) {
        filename = g_filename_from_uri (image_uri, NULL, NULL);
    } else {
        filename = g_strdup ("");    /* GSettings expects a string, not NULL */
    }

    mate_bg_set_filename (self->details->bg, filename);
    g_free (filename);

    if (emit_signal)
        g_signal_emit (self, signals[SETTINGS_CHANGED], 0, GDK_ACTION_COPY);

    set_image_properties (self);
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
    char *keyfile = g_settings_get_string (settings, MATE_BG_KEY_PICTURE_FILENAME);

    if (!g_file_test (keyfile, G_FILE_TEST_EXISTS) && (*keyfile != '\0'))
    {
        *keyfile = '\0';
        g_settings_set_string (settings, MATE_BG_KEY_PICTURE_FILENAME, keyfile);
    }

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

    /* If a custom background color isn't set, get the GtkStyleContext's bg color. */

    if (!self->details->color)
    {

        GtkStyleContext *style = gtk_widget_get_style_context (widget);
        GdkRGBA bg;
        GdkRGBA *c;

        gtk_style_context_get (style, GTK_STATE_FLAG_NORMAL,
                               GTK_STYLE_PROPERTY_BACKGROUND_COLOR,
                               &c, NULL);
        bg = *c;
        gdk_rgba_free (c);

        gradient_spec = gdk_rgba_to_string (&bg);

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
