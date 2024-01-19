
/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-desktop-wayland-bg-dialog.c background changing dialog for wayland

   Copyright (C) 2024 Luke <lukefromdc@hushmail.com>

   The Mate Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Mate Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Mate Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Author: <lukefromdc@hushmail.com>
*/

#include <config.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xatom.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-vfs-extensions.h>

#include <libcaja-private/caja-directory-background.h>
#include <libcaja-private/caja-global-preferences.h>

#include "fm-desktop-icon-view.h"
#include "fm-desktop-wayland-bg-dialog.h"


#ifdef HAVE_WAYLAND

GSettings   *background_settings;

static void
update_preview (gboolean starting, GtkWidget *box, gchar *filename,
               const gchar *shading_type, gchar *primary_color_str, gchar  *secondary_color_str)
{
    static GtkWidget *preview_image;
    static GtkCssProvider *provider;
    gchar *css;
    GString *string;
    static GdkRectangle geometry = {0};

    /* setup the preview only once*/
    if (starting == TRUE)
    {
        static GtkWidget *preview;

        /*Get the size and shape of the desktop*/
        GdkDisplay *display = gdk_screen_get_display (gdk_screen_get_default());
        GdkMonitor *monitor = gdk_display_get_monitor (display, 0);
        gdk_monitor_get_geometry (monitor, &geometry);

        preview = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_size_request (preview, geometry.width / 5, geometry.height / 5);
        gtk_widget_set_name (GTK_WIDGET (preview), "caja-wayland-bg-preview");

        preview_image = gtk_image_new ();
        provider = gtk_css_provider_new ();
        gtk_style_context_add_provider (gtk_widget_get_style_context (preview),
                                        GTK_STYLE_PROVIDER (provider),
                                        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        gtk_box_pack_start (GTK_BOX (preview), preview_image, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (box), preview, FALSE, FALSE, 0);
    }

    /* No image filename means we are previewing a color or gradient background*/
    if ((!filename) || (strcmp (filename, "") == 0) || (strcmp (filename, " ") == 0))
    {
        if (GTK_IS_IMAGE(preview_image))
            gtk_image_clear (GTK_IMAGE(preview_image));

        /*Build a color preview using a cssprovider due to requirement to handle RBGA values*/
        string = g_string_new(NULL);
        g_string_append (string, "#caja-wayland-bg-preview {");

        if (strcmp (shading_type, "vertical-gradient") == 0)
            g_string_append (string, "background-image: linear-gradient(to bottom,");

        else if  (strcmp (shading_type, "horizontal-gradient") == 0)
            g_string_append (string, "background-image: linear-gradient(to right,");

        else
        {
            g_string_append (string, "background-color:");
            g_string_append (string, primary_color_str);
        }

        if ((strcmp (shading_type, "vertical-gradient") == 0) ||
           (strcmp (shading_type, "horizontal-gradient") == 0))
        {
            g_string_append (string, primary_color_str);
            g_string_append (string, ",");
            g_string_append (string, secondary_color_str);
            g_string_append (string, ");");
        }
            g_string_append (string, "}");

        css = g_string_free (string, FALSE);

        gtk_css_provider_load_from_data (provider, css, -1, NULL);

        g_free (css);
    }
    else
    /*Preview a background image*/
    {
        GdkPixbuf  *pixbuf;

        pixbuf = gdk_pixbuf_new_from_file_at_scale (filename, geometry.width / 5,
                                                    geometry.height / 5, TRUE, NULL);

        if (GTK_IS_IMAGE(preview_image))
            gtk_image_set_from_pixbuf (GTK_IMAGE (preview_image), pixbuf);

        /*Clear the color preview*/
        string = g_string_new (NULL);
        g_string_append (string, "#caja-wayland-bg-preview {");

        g_string_append (string, "background-image: none;");
        g_string_append (string, "background-color: transparent;");
        g_string_append (string, "}");

        css = g_string_free (string, FALSE);
        gtk_css_provider_load_from_data (provider, css, -1, NULL);

        g_free (css);
        g_object_unref (pixbuf);
    }
}

static void
update_primary_color (GtkWidget *colorbutton1)
{
    gchar *shading_type, *primary_color_str, *secondary_color_str;
    GdkRGBA color1;

    shading_type = g_settings_get_string (background_settings,
                                          "color-shading-type");

    secondary_color_str = g_settings_get_string (background_settings,
                                               "secondary-color");

    gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (colorbutton1), &color1);
    primary_color_str = gdk_rgba_to_string (&color1);

    update_preview (FALSE, NULL, "", shading_type,
                    primary_color_str, secondary_color_str);

    g_settings_set_string (background_settings,
                           "primary-color", primary_color_str);

    g_settings_set_string (background_settings,
                       "picture-filename", "");

    g_free (shading_type);
    g_free (primary_color_str);
    g_free (secondary_color_str);
}

static void
update_secondary_color (GtkWidget *colorbutton2)
{
    gchar *shading_type, *primary_color_str, *secondary_color_str;
    GdkRGBA color2;

    shading_type = g_settings_get_string (background_settings,
                                          "color-shading-type");

    primary_color_str = g_settings_get_string (background_settings,
                                               "primary-color");

    gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (colorbutton2), &color2);
    secondary_color_str = gdk_rgba_to_string (&color2);

    g_settings_set_string (background_settings,
                           "secondary-color", secondary_color_str);

    g_settings_set_string (background_settings,
                       "picture-filename", "");

    update_preview (FALSE, NULL, "", shading_type,
                    primary_color_str, secondary_color_str);

    g_free (shading_type);
    g_free (primary_color_str);
    g_free (secondary_color_str);
}

static void
update_color_background_options (GtkWidget *colorbox)
{
    gchar *primary_color_str, *secondary_color_str;
    const gchar *shading_type;

    primary_color_str = g_settings_get_string (background_settings,
                                               "primary-color");

    secondary_color_str = g_settings_get_string (background_settings,
                                               "secondary-color");

    shading_type = gtk_combo_box_get_active_id (GTK_COMBO_BOX (colorbox));
    /*write to gsettings*/
    g_settings_set_string (background_settings,
                           "color-shading-type", shading_type);
    g_settings_set_string (background_settings,
                       "picture-filename", "");

    update_preview (FALSE, NULL, "", shading_type,
                    primary_color_str, secondary_color_str);

    g_free (primary_color_str);
    g_free (secondary_color_str);
}

static void
update_image_background_options(GtkWidget *stylebox)
{
    const gchar *options;
    options = gtk_combo_box_get_active_id (GTK_COMBO_BOX(stylebox));

    /*write to gsettings*/
    g_settings_set_string (background_settings,
                           "picture-options", options);

    /*Only the image changes here, we are not thumbnailing image options yet*/
}

static void
update_background_image (GtkWidget *filebutton)
{
    gchar *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filebutton));
    if (strcmp (filename, " ") == 0)
        filename = "";

    /*write to gsettings*/
    g_settings_set_string (background_settings,
                       "picture-filename", filename);

    update_preview (FALSE, NULL, filename, NULL, NULL, NULL);
    g_free (filename);
}

void
wayland_bg_dialog_new (void)
{
    GtkWidget *dialog, *box, *vbox1, *vbox2, *hbox1, *hbox2;
    GtkWidget *close_button, *colorlabel, *stylelabel;
    GtkWidget *filelabel, *stylebox;
    GtkWidget *colorbox, *colorbutton1, *colorbutton2;
    GtkWidget *filebutton;
    GdkRGBA    color1, color2;
    gchar *filename, *options;
    gchar *shading_type, *primary_color_str, *secondary_color_str;

    background_settings = g_settings_new ("org.mate.background");

    filename = g_settings_get_string (background_settings,
                                      "picture-filename");

    options = g_settings_get_string (background_settings,
                                     "picture-options");

    primary_color_str = g_settings_get_string (background_settings,
                                               "primary-color");

    secondary_color_str = g_settings_get_string (background_settings,
                                                 "secondary-color");

    shading_type = g_settings_get_string (background_settings,
                                          "color-shading-type");

    dialog = gtk_dialog_new ();
    gtk_window_set_title (GTK_WINDOW (dialog),
                         _("Desktop Background Preferences"));

    gtk_window_set_transient_for (GTK_WINDOW (dialog), NULL);

    /*Image Style Combobox*/
    stylebox = gtk_combo_box_text_new ();

    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (stylebox),
                   "wallpaper", "Tile" );

    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (stylebox),
                   "zoom", "Zoom" );

    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (stylebox),
                   "centered", "Center");

    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (stylebox),
                   "scaled", "Scale");

    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (stylebox),
                   "stretched", "Stretch");

    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (stylebox),
                   "spanned", "Span");

    gtk_combo_box_set_active_id (GTK_COMBO_BOX (stylebox), options);

    g_signal_connect (stylebox, "changed",
              G_CALLBACK (update_image_background_options), stylebox);

    gtk_widget_set_tooltip_text (stylebox, "Image Aspect Ratio and Size. \n"
                                "Changes applied immediately");

    /*Color Combobox*/
    colorlabel = gtk_label_new ("Colors:");
    colorbox =  gtk_combo_box_text_new ();

    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (colorbox),
                               "solid", "Solid color" );

    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (colorbox),
                               "horizontal-gradient", "Horizontal gradient" );

    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (colorbox),
                               "vertical-gradient", "Vertical gradient");

    gtk_combo_box_set_active_id (GTK_COMBO_BOX (colorbox), shading_type);

    g_signal_connect (colorbox, "changed",
                      G_CALLBACK (update_color_background_options), colorbox);

    gtk_widget_set_tooltip_text (colorbox, "Use gradient or solid color. \n"
                                 "Changes applied immediately");

    colorbutton1 = gtk_color_button_new ();
    colorbutton2 = gtk_color_button_new ();
    gtk_widget_set_tooltip_text (colorbutton1, "Color for gradient top/left or solid color \n"
                                 "Applies on selecting any color");

    gtk_widget_set_tooltip_text (colorbutton2, "Color for gradient bottom/right\n"
                                "Applies on selecting any color");


    if (!(gdk_rgba_parse (&color1, primary_color_str)))
        gdk_rgba_parse (&color1, "rgb(88,145,188)");

    if (!(gdk_rgba_parse (&color2, secondary_color_str)))
        gdk_rgba_parse (&color2, "rgb(60,143,37)");

    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (colorbutton1), &color1);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (colorbutton2), &color2);

    g_signal_connect (colorbutton1, "color-set",
              G_CALLBACK (update_primary_color), colorbutton1);

    g_signal_connect (colorbutton2, "color-set",
              G_CALLBACK (update_secondary_color), colorbutton1);

    /*file chooser and it's label for the color bg case*/
    filelabel  = gtk_label_new ("Image:");
    stylelabel  = gtk_label_new ("Style:");
    filebutton = gtk_file_chooser_button_new (_("Select a file"),
                    GTK_FILE_CHOOSER_ACTION_OPEN);

    gtk_widget_set_tooltip_text (filebutton, "Image for desktop background. \n"
                                "Applies on opening image");

    gtk_file_chooser_button_set_width_chars (GTK_FILE_CHOOSER_BUTTON (filebutton), 16);

    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (filebutton),
                       filename);

    gtk_file_chooser_button_set_title (GTK_FILE_CHOOSER_BUTTON (filebutton),
                       "Select a File");

    /* If the last background was an image show the user the default background directory*/
    if ((!filename) || (strcmp (filename, "") == 0) || (strcmp (filename, " ") == 0))
    {
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (filebutton), "/usr/share/backgrounds/");
    }

    g_signal_connect (filebutton, "file-set",
              G_CALLBACK (update_background_image), NULL);

    /*Apply and Cancel buttons */
    close_button = gtk_button_new_with_mnemonic (_("_Close"));

    gtk_button_set_image (GTK_BUTTON (close_button),
                  gtk_image_new_from_icon_name ("gtk-cancel", GTK_ICON_SIZE_BUTTON));

    gtk_button_set_use_underline (GTK_BUTTON (close_button), TRUE);
    gtk_widget_set_can_default (close_button, TRUE);

    /*Prepare the boxes to pack all this into*/
    box = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    vbox1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
    vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
    hbox1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

    /*Pack the filechooser and image options*/
    gtk_box_pack_start (GTK_BOX (hbox1), filelabel, FALSE, FALSE, 7);
    gtk_box_pack_start (GTK_BOX (hbox1), filebutton, FALSE, FALSE, 3);
    gtk_box_pack_end (GTK_BOX (hbox1), stylebox, FALSE, FALSE, 5);
    gtk_box_pack_end (GTK_BOX (hbox1), stylelabel, FALSE, FALSE, 2);

    /*Pack the colorpickers and color options*/

    gtk_box_pack_start (GTK_BOX (hbox2), colorlabel, FALSE, FALSE, 7);
    gtk_box_pack_start (GTK_BOX (hbox2), colorbox, FALSE, FALSE, 5);
    gtk_box_pack_end (GTK_BOX (hbox2), colorbutton2, FALSE, FALSE, 5);
    gtk_box_pack_end (GTK_BOX (hbox2), colorbutton1, FALSE, FALSE, 2);

    /*Get the preview and pack it*/
    update_preview (TRUE, vbox2, filename, shading_type,
                    primary_color_str, secondary_color_str);

    gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

    /*Pack the other boxes into the final vertical box*/
    gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 20);
    gtk_box_pack_start (GTK_BOX (vbox1), hbox2, FALSE, FALSE, 5);

    /*Pack the final vertical box into the content area*/
    gtk_box_pack_start (GTK_BOX (box), vbox1, FALSE, FALSE, 0);

    /*Pack the close action area*/

    gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
                      close_button,
                      GTK_RESPONSE_APPLY);

    gtk_widget_show_all (dialog);

    /*Run the dialog*/
    gtk_dialog_run (GTK_DIALOG (dialog));

    /*cleanup*/
    g_free(filename);
    g_free(options);
    g_free(shading_type);
    g_free(primary_color_str);
    g_free(secondary_color_str);

    g_signal_handlers_disconnect_by_func (stylebox, update_image_background_options, stylebox);
    g_signal_handlers_disconnect_by_func (colorbox, update_color_background_options, colorbox);
    g_signal_handlers_disconnect_by_func (colorbutton1, update_primary_color, colorbutton1);
    g_signal_handlers_disconnect_by_func (colorbutton2, update_primary_color, colorbutton2);
    g_signal_handlers_disconnect_by_func (filebutton, update_background_image, NULL);

    g_object_unref (background_settings);
    gtk_widget_destroy (dialog);
}
#endif

