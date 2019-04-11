/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Red Hat, Inc.
 * Copyright (C) 2006 Paolo Borelli <pborelli@katamail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors: David Zeuthen <davidz@redhat.com>
 *          Paolo Borelli <pborelli@katamail.com>
 *
 */

#include <config.h>
#include <string.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <libcaja-private/caja-autorun.h>
#include <libcaja-private/caja-icon-info.h>

#include "caja-x-content-bar.h"

struct _CajaXContentBarPrivate
{
    GtkWidget *label;
    GtkWidget *button;

    char *x_content_type;
    GMount *mount;
};

enum
{
    PROP_0,
    PROP_MOUNT,
    PROP_X_CONTENT_TYPE,
};

G_DEFINE_TYPE_WITH_PRIVATE (CajaXContentBar, caja_x_content_bar, GTK_TYPE_BOX)

void
caja_x_content_bar_set_x_content_type (CajaXContentBar *bar, const char *x_content_type)
{
    char *message;
    char *description;
    GAppInfo *default_app;

    g_free (bar->priv->x_content_type);
    bar->priv->x_content_type = g_strdup (x_content_type);

    description = g_content_type_get_description (x_content_type);

    /* Customize greeting for well-known x-content types */
    if (strcmp (x_content_type, "x-content/audio-cdda") == 0)
    {
        message = g_strdup (_("These files are on an Audio CD."));
    }
    else if (strcmp (x_content_type, "x-content/audio-dvd") == 0)
    {
        message = g_strdup (_("These files are on an Audio DVD."));
    }
    else if (strcmp (x_content_type, "x-content/video-dvd") == 0)
    {
        message = g_strdup (_("These files are on a Video DVD."));
    }
    else if (strcmp (x_content_type, "x-content/video-vcd") == 0)
    {
        message = g_strdup (_("These files are on a Video CD."));
    }
    else if (strcmp (x_content_type, "x-content/video-svcd") == 0)
    {
        message = g_strdup (_("These files are on a Super Video CD."));
    }
    else if (strcmp (x_content_type, "x-content/image-photocd") == 0)
    {
        message = g_strdup (_("These files are on a Photo CD."));
    }
    else if (strcmp (x_content_type, "x-content/image-picturecd") == 0)
    {
        message = g_strdup (_("These files are on a Picture CD."));
    }
    else if (strcmp (x_content_type, "x-content/image-dcf") == 0)
    {
        message = g_strdup (_("The media contains digital photos."));
    }
    else if (strcmp (x_content_type, "x-content/audio-player") == 0)
    {
        message = g_strdup (_("These files are on a digital audio player."));
    }
    else if (strcmp (x_content_type, "x-content/software") == 0)
    {
        message = g_strdup (_("The media contains software."));
    }
    else
    {
        /* fallback to generic greeting */
        message = g_strdup_printf (_("The media has been detected as \"%s\"."), description);
    }


    gtk_label_set_text (GTK_LABEL (bar->priv->label), message);
    gtk_widget_show (bar->priv->label);

    /* TODO: We really need a GtkBrowserBackButton-ish widget here.. until then, we only
     *       show the default application. */

    default_app = g_app_info_get_default_for_type (x_content_type, FALSE);
    if (default_app != NULL)
    {
        char *button_text;
        const char *name;
        GIcon *icon;
        GtkWidget *image;

        icon = g_app_info_get_icon (default_app);
        if (icon != NULL)
        {
            GdkPixbuf *pixbuf;
            int icon_size, icon_scale;
            CajaIconInfo *icon_info;
            icon_size = caja_get_icon_size_for_stock_size (GTK_ICON_SIZE_BUTTON);
            icon_scale = gtk_widget_get_scale_factor (GTK_WIDGET (bar));
            icon_info = caja_icon_info_lookup (icon, icon_size, icon_scale);
            pixbuf = caja_icon_info_get_pixbuf_at_size (icon_info, icon_size);
            image = gtk_image_new_from_pixbuf (pixbuf);
            g_object_unref (pixbuf);
            g_object_unref (icon_info);
        }
        else
        {
            image = NULL;
        }

        name = g_app_info_get_display_name (default_app);
        button_text = g_strdup_printf (_("Open %s"), name);

        gtk_button_set_image (GTK_BUTTON (bar->priv->button), image);
        gtk_button_set_label (GTK_BUTTON (bar->priv->button), button_text);
        gtk_widget_show (bar->priv->button);
        g_free (button_text);
        g_object_unref (default_app);
    }
    else
    {
        gtk_widget_hide (bar->priv->button);
    }

    g_free (message);
    g_free (description);
}

const char *
caja_x_content_bar_get_x_content_type (CajaXContentBar *bar)
{
    return bar->priv->x_content_type;
}

GMount *
caja_x_content_bar_get_mount (CajaXContentBar *bar)
{
    return bar->priv->mount != NULL ? g_object_ref (bar->priv->mount) : NULL;
}

void
caja_x_content_bar_set_mount (CajaXContentBar *bar, GMount *mount)
{
    if (bar->priv->mount != NULL)
    {
        g_object_unref (bar->priv->mount);
    }
    bar->priv->mount = mount != NULL ? g_object_ref (mount) : NULL;
}


static void
caja_x_content_bar_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
    CajaXContentBar *bar;

    bar = CAJA_X_CONTENT_BAR (object);

    switch (prop_id)
    {
    case PROP_MOUNT:
        caja_x_content_bar_set_mount (bar, G_MOUNT (g_value_get_object (value)));
        break;
    case PROP_X_CONTENT_TYPE:
        caja_x_content_bar_set_x_content_type (bar, g_value_get_string (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
caja_x_content_bar_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
    CajaXContentBar *bar;

    bar = CAJA_X_CONTENT_BAR (object);

    switch (prop_id)
    {
    case PROP_MOUNT:
        g_value_set_object (value, bar->priv->mount);
        break;
    case PROP_X_CONTENT_TYPE:
        g_value_set_string (value, bar->priv->x_content_type);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
caja_x_content_bar_finalize (GObject *object)
{
    CajaXContentBar *bar = CAJA_X_CONTENT_BAR (object);

    g_free (bar->priv->x_content_type);
    if (bar->priv->mount != NULL)
        g_object_unref (bar->priv->mount);

    G_OBJECT_CLASS (caja_x_content_bar_parent_class)->finalize (object);
}

static void
caja_x_content_bar_class_init (CajaXContentBarClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->get_property = caja_x_content_bar_get_property;
    object_class->set_property = caja_x_content_bar_set_property;
    object_class->finalize = caja_x_content_bar_finalize;

    g_object_class_install_property (object_class,
                                     PROP_MOUNT,
                                     g_param_spec_object (
                                         "mount",
                                         "The GMount to run programs for",
                                         "The GMount to run programs for",
                                         G_TYPE_MOUNT,
                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (object_class,
                                     PROP_X_CONTENT_TYPE,
                                     g_param_spec_string (
                                         "x-content-type",
                                         "The x-content type for the cluebar",
                                         "The x-content type for the cluebar",
                                         NULL,
                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void
button_clicked_callback (GtkWidget *button, CajaXContentBar *bar)
{
    GAppInfo *default_app;

    if (bar->priv->x_content_type == NULL ||
            bar->priv->mount == NULL)
        return;

    default_app = g_app_info_get_default_for_type (bar->priv->x_content_type, FALSE);
    if (default_app != NULL)
    {
        caja_autorun_launch_for_mount (bar->priv->mount, default_app);
        g_object_unref (default_app);
    }
}

static void
caja_x_content_bar_init (CajaXContentBar *bar)
{
    GtkWidget *hbox;

    bar->priv = caja_x_content_bar_get_instance_private (bar);

    hbox = GTK_WIDGET (bar);

    bar->priv->label = gtk_label_new (NULL);
    gtk_label_set_ellipsize (GTK_LABEL (bar->priv->label), PANGO_ELLIPSIZE_END);

    gtk_orientable_set_orientation (GTK_ORIENTABLE (bar), GTK_ORIENTATION_HORIZONTAL);

    gtk_label_set_xalign (GTK_LABEL (bar->priv->label), 0.0);
    gtk_box_pack_start (GTK_BOX (bar), bar->priv->label, TRUE, TRUE, 0);

    bar->priv->button = gtk_button_new ();
    gtk_box_pack_end (GTK_BOX (hbox), bar->priv->button, FALSE, FALSE, 0);

    g_signal_connect (bar->priv->button,
                      "clicked",
                      G_CALLBACK (button_clicked_callback),
                      bar);
}

GtkWidget *
caja_x_content_bar_new (GMount *mount,
                        const char *x_content_type)
{
    GObject *bar;

    bar = g_object_new (CAJA_TYPE_X_CONTENT_BAR,
                        "mount", mount,
                        "x-content-type", x_content_type,
                        NULL);

    return GTK_WIDGET (bar);
}
