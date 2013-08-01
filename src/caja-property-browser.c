/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Caja is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Caja is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 */

/* This is the implementation of the property browser window, which
 * gives the user access to an extensible palette of properties which
 * can be dropped on various elements of the user interface to
 * customize them
 */

#include <config.h>
#include <math.h>
#include "caja-property-browser.h"

#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-image-table.h>
#include <eel/eel-labeled-image.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-xml-extensions.h>
#include <libxml/parser.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <libcaja-private/caja-customization-data.h>
#include <libcaja-private/caja-directory.h>
#include <libcaja-private/caja-emblem-utils.h>
#include <libcaja-private/caja-file-utilities.h>
#include <libcaja-private/caja-file.h>
#include <libcaja-private/caja-global-preferences.h>
#include <libcaja-private/caja-metadata.h>
#include <libcaja-private/caja-signaller.h>
#include <atk/atkrelationset.h>

#include "glibcompat.h" /* for g_list_free_full */

/* property types */

typedef enum
{
    CAJA_PROPERTY_NONE,
    CAJA_PROPERTY_PATTERN,
    CAJA_PROPERTY_COLOR,
    CAJA_PROPERTY_EMBLEM
} CajaPropertyType;

struct CajaPropertyBrowserDetails
{
    GtkHBox *container;

    GtkWidget *content_container;
    GtkWidget *content_frame;
    GtkWidget *content_table;

    GtkWidget *category_container;
    GtkWidget *category_box;

    GtkWidget *title_box;
    GtkWidget *title_label;
    GtkWidget *help_label;

    GtkWidget *bottom_box;

    GtkWidget *add_button;
    GtkWidget *add_button_image;
    GtkWidget *remove_button;
    GtkWidget *remove_button_image;

    GtkWidget *patterns_dialog;
    GtkWidget *colors_dialog;
    GtkWidget *emblems_dialog;

    GtkWidget *keyword;
    GtkWidget *emblem_image;
    GtkWidget *image_button;

    GtkWidget *color_picker;
    GtkWidget *color_name;

    GList *keywords;

    char *path;
    char *category;
    char *dragged_file;
    char *drag_type;
    char *image_path;
    char *filename;

    CajaPropertyType category_type;

    int category_position;

    GdkPixbuf *property_chit;

    gboolean remove_mode;
    gboolean keep_around;
    gboolean has_local;
};

static void     caja_property_browser_update_contents       (CajaPropertyBrowser       *property_browser);
static void     caja_property_browser_set_category          (CajaPropertyBrowser       *property_browser,
        const char                    *new_category);
static void     caja_property_browser_set_dragged_file      (CajaPropertyBrowser       *property_browser,
        const char                    *dragged_file_name);
static void     caja_property_browser_set_drag_type         (CajaPropertyBrowser       *property_browser,
        const char                    *new_drag_type);
static void     add_new_button_callback                         (GtkWidget                     *widget,
        CajaPropertyBrowser       *property_browser);
static void     cancel_remove_mode                              (CajaPropertyBrowser       *property_browser);
static void     done_button_callback                            (GtkWidget                     *widget,
        GtkWidget                     *property_browser);
static void     help_button_callback                            (GtkWidget                     *widget,
        GtkWidget                     *property_browser);
static void     remove_button_callback                          (GtkWidget                     *widget,
        CajaPropertyBrowser       *property_browser);
static gboolean caja_property_browser_delete_event_callback (GtkWidget                     *widget,
        GdkEvent                      *event,
        gpointer                       user_data);
static void     caja_property_browser_hide_callback         (GtkWidget                     *widget,
        gpointer                       user_data);
static void     caja_property_browser_drag_end              (GtkWidget                     *widget,
        GdkDragContext                *context);
static void     caja_property_browser_drag_begin            (GtkWidget                     *widget,
        GdkDragContext                *context);
static void     caja_property_browser_drag_data_get         (GtkWidget                     *widget,
        GdkDragContext                *context,
        GtkSelectionData              *selection_data,
        guint                          info,
        guint32                        time);
static void     emit_emblems_changed_signal                     (void);
static void     emblems_changed_callback                        (GObject                       *signaller,
        CajaPropertyBrowser       *property_browser);

/* misc utilities */
static void     element_clicked_callback                        (GtkWidget                     *image_table,
        GtkWidget                     *child,
        const EelImageTableEvent *event,
        gpointer                       callback_data);

static GdkPixbuf * make_drag_image                              (CajaPropertyBrowser       *property_browser,
        const char                    *file_name);
static GdkPixbuf * make_color_drag_image                        (CajaPropertyBrowser       *property_browser,
        const char                    *color_spec,
        gboolean                       trim_edges);


#define BROWSER_CATEGORIES_FILE_NAME "browser.xml"

#define PROPERTY_BROWSER_WIDTH 540
#define PROPERTY_BROWSER_HEIGHT 340
#define MAX_EMBLEM_HEIGHT 52
#define STANDARD_BUTTON_IMAGE_HEIGHT 42

#define MAX_ICON_WIDTH 63
#define MAX_ICON_HEIGHT 63
#define COLOR_SQUARE_SIZE 48

#define LABELED_IMAGE_SPACING 2
#define IMAGE_TABLE_X_SPACING 6
#define IMAGE_TABLE_Y_SPACING 4

#define ERASE_OBJECT_NAME "erase.png"

enum
{
    PROPERTY_TYPE
};

static GtkTargetEntry drag_types[] =
{
    { "text/uri-list",  0, PROPERTY_TYPE }
};


G_DEFINE_TYPE (CajaPropertyBrowser, caja_property_browser, GTK_TYPE_WINDOW)


/* Destroy the three dialogs for adding patterns/colors/emblems if any of them
   exist. */
static void
caja_property_browser_destroy_dialogs (CajaPropertyBrowser *property_browser)
{
    if (property_browser->details->patterns_dialog)
    {
        gtk_widget_destroy (property_browser->details->patterns_dialog);
        property_browser->details->patterns_dialog = NULL;
    }
    if (property_browser->details->colors_dialog)
    {
        gtk_widget_destroy (property_browser->details->colors_dialog);
        property_browser->details->colors_dialog = NULL;
    }
    if (property_browser->details->emblems_dialog)
    {
        gtk_widget_destroy (property_browser->details->emblems_dialog);
        property_browser->details->emblems_dialog = NULL;
    }
}

static void
caja_property_browser_dispose (GObject *object)
{
    CajaPropertyBrowser *property_browser;

    property_browser = CAJA_PROPERTY_BROWSER (object);

    caja_property_browser_destroy_dialogs (property_browser);

    g_free (property_browser->details->path);
    g_free (property_browser->details->category);
    g_free (property_browser->details->dragged_file);
    g_free (property_browser->details->drag_type);

    g_list_free_full (property_browser->details->keywords, g_free);

    if (property_browser->details->property_chit)
    {
        g_object_unref (property_browser->details->property_chit);
    }

    G_OBJECT_CLASS (caja_property_browser_parent_class)->dispose (object);
}

/* initializing the class object by installing the operations we override */
static void
caja_property_browser_class_init (CajaPropertyBrowserClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    G_OBJECT_CLASS (klass)->dispose = caja_property_browser_dispose;
    widget_class->drag_begin = caja_property_browser_drag_begin;
    widget_class->drag_data_get  = caja_property_browser_drag_data_get;
    widget_class->drag_end  = caja_property_browser_drag_end;

    g_type_class_add_private (klass, sizeof (CajaPropertyBrowserDetails));
}

/* initialize the instance's fields, create the necessary subviews, etc. */

static void
caja_property_browser_init (CajaPropertyBrowser *property_browser)
{
    GtkWidget *widget, *temp_box, *temp_hbox, *temp_frame, *vbox;
    GtkWidget *temp_button;
    GtkWidget *viewport;
    PangoAttrList *attrs;
    char *temp_str;

    widget = GTK_WIDGET (property_browser);

    property_browser->details = G_TYPE_INSTANCE_GET_PRIVATE (property_browser,
    							     CAJA_TYPE_PROPERTY_BROWSER,
    							     CajaPropertyBrowserDetails);

    property_browser->details->category = g_strdup ("patterns");
    property_browser->details->category_type = CAJA_PROPERTY_PATTERN;

    /* load the chit frame */
    temp_str = caja_pixmap_file ("chit_frame.png");
    if (temp_str != NULL)
    {
        property_browser->details->property_chit = gdk_pixbuf_new_from_file (temp_str, NULL);
    }
    g_free (temp_str);

    /* set the initial size of the property browser */
    gtk_window_set_default_size (GTK_WINDOW (property_browser),
                                 PROPERTY_BROWSER_WIDTH,
                                 PROPERTY_BROWSER_HEIGHT);

    /* set the title and standard close accelerator */
    gtk_window_set_title (GTK_WINDOW (widget), _("Backgrounds and Emblems"));
    gtk_window_set_wmclass (GTK_WINDOW (widget), "property_browser", "Caja");
    gtk_window_set_type_hint (GTK_WINDOW (widget), GDK_WINDOW_TYPE_HINT_DIALOG);

    /* create the main vbox. */
    vbox = gtk_vbox_new (FALSE, 12);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
    gtk_widget_show (vbox);
    gtk_container_add (GTK_CONTAINER (property_browser), vbox);

    /* create the container box */
    property_browser->details->container = GTK_HBOX (gtk_hbox_new (FALSE, 6));
    gtk_widget_show (GTK_WIDGET (property_browser->details->container));
    gtk_box_pack_start (GTK_BOX (vbox),
                        GTK_WIDGET (property_browser->details->container),
                        TRUE, TRUE, 0);

    /* make the category container */
    property_browser->details->category_container = gtk_scrolled_window_new (NULL, NULL);
    property_browser->details->category_position = -1;

    viewport = gtk_viewport_new (NULL, NULL);
    gtk_widget_show (viewport);
    gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewport), GTK_SHADOW_NONE);

    gtk_box_pack_start (GTK_BOX (property_browser->details->container),
                        property_browser->details->category_container, FALSE, FALSE, 0);
    gtk_widget_show (property_browser->details->category_container);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (property_browser->details->category_container),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    /* allocate a table to hold the category selector */
    property_browser->details->category_box = gtk_vbox_new (FALSE, 6);
    gtk_container_add(GTK_CONTAINER(viewport), property_browser->details->category_box);
    gtk_container_add (GTK_CONTAINER (property_browser->details->category_container), viewport);
    gtk_widget_show (GTK_WIDGET (property_browser->details->category_box));

    /* make the content container vbox */
    property_browser->details->content_container = gtk_vbox_new (FALSE, 6);
    gtk_widget_show (property_browser->details->content_container);
    gtk_box_pack_start (GTK_BOX (property_browser->details->container),
                        property_browser->details->content_container,
                        TRUE, TRUE, 0);

    /* create the title box */
    property_browser->details->title_box = gtk_event_box_new();

    gtk_widget_show(property_browser->details->title_box);
    gtk_box_pack_start (GTK_BOX(property_browser->details->content_container),
                        property_browser->details->title_box,
                        FALSE, FALSE, 0);

    temp_frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(temp_frame), GTK_SHADOW_NONE);
    gtk_widget_show(temp_frame);
    gtk_container_add(GTK_CONTAINER(property_browser->details->title_box), temp_frame);

    temp_hbox = gtk_hbox_new(FALSE, 0);
    gtk_widget_show(temp_hbox);

    gtk_container_add(GTK_CONTAINER(temp_frame), temp_hbox);

    /* add the title label */
    attrs = pango_attr_list_new ();
    pango_attr_list_insert (attrs, pango_attr_scale_new (PANGO_SCALE_X_LARGE));
    pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
    property_browser->details->title_label = gtk_label_new ("");
    gtk_label_set_attributes (GTK_LABEL (property_browser->details->title_label), attrs);
    pango_attr_list_unref (attrs);

    gtk_widget_show(property_browser->details->title_label);
    gtk_box_pack_start (GTK_BOX(temp_hbox), property_browser->details->title_label, FALSE, FALSE, 0);

    /* add the help label */
    property_browser->details->help_label = gtk_label_new  ("");
    gtk_widget_show(property_browser->details->help_label);
    gtk_box_pack_end (GTK_BOX (temp_hbox), property_browser->details->help_label, FALSE, FALSE, 0);

    /* add the bottom box to hold the command buttons */
    temp_box = gtk_event_box_new();
    gtk_widget_show(temp_box);

    property_browser->details->bottom_box = gtk_hbox_new (FALSE, 6);
    gtk_widget_show (property_browser->details->bottom_box);

    gtk_box_pack_end (GTK_BOX (vbox), temp_box, FALSE, FALSE, 0);
    gtk_container_add (GTK_CONTAINER (temp_box), property_browser->details->bottom_box);

    /* create the "help" button */
    temp_button = gtk_button_new_from_stock (GTK_STOCK_HELP);

    gtk_widget_show (temp_button);
    gtk_box_pack_start (GTK_BOX (property_browser->details->bottom_box), temp_button, FALSE, FALSE, 0);
    g_signal_connect_object (temp_button, "clicked", G_CALLBACK (help_button_callback), property_browser, 0);

    /* create the "done" button */
    temp_button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
    gtk_widget_set_can_default (temp_button, TRUE);

    gtk_widget_show (temp_button);
    gtk_box_pack_end (GTK_BOX (property_browser->details->bottom_box), temp_button, FALSE, FALSE, 0);
    gtk_widget_grab_default (temp_button);
    gtk_widget_grab_focus (temp_button);
    g_signal_connect_object (temp_button, "clicked", G_CALLBACK (done_button_callback), property_browser, 0);

    /* create the "remove" button */
    property_browser->details->remove_button = gtk_button_new_with_mnemonic (_("_Remove..."));

    property_browser->details->remove_button_image = gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image (GTK_BUTTON (property_browser->details->remove_button),
                          property_browser->details->remove_button_image);
    gtk_widget_show_all (property_browser->details->remove_button);

    gtk_box_pack_end (GTK_BOX (property_browser->details->bottom_box),
                      property_browser->details->remove_button, FALSE, FALSE, 0);

    g_signal_connect_object (property_browser->details->remove_button, "clicked",
                             G_CALLBACK (remove_button_callback), property_browser, 0);

    /* now create the "add new" button */
    property_browser->details->add_button = gtk_button_new_with_mnemonic (_("Add new..."));

    property_browser->details->add_button_image = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image (GTK_BUTTON (property_browser->details->add_button),
                          property_browser->details->add_button_image);
    gtk_widget_show_all (property_browser->details->add_button);

    gtk_box_pack_end (GTK_BOX(property_browser->details->bottom_box),
                      property_browser->details->add_button, FALSE, FALSE, 0);

    g_signal_connect_object (property_browser->details->add_button, "clicked",
                             G_CALLBACK (add_new_button_callback), property_browser, 0);


    /* now create the actual content, with the category pane and the content frame */

    /* the actual contents are created when necessary */
    property_browser->details->content_frame = NULL;

    g_signal_connect (property_browser, "delete_event",
                      G_CALLBACK (caja_property_browser_delete_event_callback), NULL);
    g_signal_connect (property_browser, "hide",
                      G_CALLBACK (caja_property_browser_hide_callback), NULL);

    g_signal_connect_object (caja_signaller_get_current (),
                             "emblems_changed",
                             G_CALLBACK (emblems_changed_callback), property_browser, 0);

    /* initially, display the top level */
    caja_property_browser_set_path(property_browser, BROWSER_CATEGORIES_FILE_NAME);
}

/* create a new instance */
CajaPropertyBrowser *
caja_property_browser_new (GdkScreen *screen)
{
    CajaPropertyBrowser *browser;

    browser = CAJA_PROPERTY_BROWSER
              (gtk_widget_new (caja_property_browser_get_type (), NULL));

    gtk_window_set_screen (GTK_WINDOW (browser), screen);
    gtk_widget_show (GTK_WIDGET(browser));

    return browser;
}

/* show the main property browser */

void
caja_property_browser_show (GdkScreen *screen)
{
    static GtkWindow *browser = NULL;

    if (browser == NULL)
    {
        browser = GTK_WINDOW (caja_property_browser_new (screen));
        g_object_add_weak_pointer (G_OBJECT (browser),
                                   (gpointer *) &browser);
    }
    else
    {
        gtk_window_set_screen (browser, screen);
        gtk_window_present (browser);
    }
}

static gboolean
caja_property_browser_delete_event_callback (GtkWidget *widget,
        GdkEvent  *event,
        gpointer   user_data)
{
    /* Hide but don't destroy */
    gtk_widget_hide(widget);
    return TRUE;
}

static void
caja_property_browser_hide_callback (GtkWidget *widget,
                                     gpointer   user_data)
{
    CajaPropertyBrowser *property_browser;

    property_browser = CAJA_PROPERTY_BROWSER (widget);

    cancel_remove_mode (property_browser);

    /* Destroy the 3 dialogs to add new patterns/colors/emblems. */
    caja_property_browser_destroy_dialogs (property_browser);
}

/* remember the name of the dragged file */
static void
caja_property_browser_set_dragged_file (CajaPropertyBrowser *property_browser,
                                        const char *dragged_file_name)
{
    g_free (property_browser->details->dragged_file);
    property_browser->details->dragged_file = g_strdup (dragged_file_name);
}

/* remember the drag type */
static void
caja_property_browser_set_drag_type (CajaPropertyBrowser *property_browser,
                                     const char *new_drag_type)
{
    g_free (property_browser->details->drag_type);
    property_browser->details->drag_type = g_strdup (new_drag_type);
}

static void
caja_property_browser_drag_begin (GtkWidget *widget,
                                  GdkDragContext *context)
{
    CajaPropertyBrowser *property_browser;
    GtkWidget *child;
    GdkPixbuf *pixbuf;
    int x_delta, y_delta;
    char *element_name;

    property_browser = CAJA_PROPERTY_BROWSER (widget);

    child = g_object_steal_data (G_OBJECT (property_browser), "dragged-image");
    g_return_if_fail (child != NULL);

    element_name = g_object_get_data (G_OBJECT (child), "property-name");
    g_return_if_fail (child != NULL);

    /* compute the offsets for dragging */
    if (strcmp (drag_types[0].target, "application/x-color") != 0)
    {
        /* it's not a color, so, for now, it must be an image */
        /* fiddle with the category to handle the "reset" case properly */
        char * save_category = property_browser->details->category;
        if (g_strcmp0 (property_browser->details->category, "colors") == 0)
        {
            property_browser->details->category = "patterns";
        }
        pixbuf = make_drag_image (property_browser, element_name);
        property_browser->details->category = save_category;
    }
    else
    {
        pixbuf = make_color_drag_image (property_browser, element_name, TRUE);
    }

    /* set the pixmap and mask for dragging */
    if (pixbuf != NULL)
    {
        x_delta = gdk_pixbuf_get_width (pixbuf) / 2;
        y_delta = gdk_pixbuf_get_height (pixbuf) / 2;

        gtk_drag_set_icon_pixbuf
        (context,
         pixbuf,
         x_delta, y_delta);
        g_object_unref (pixbuf);
    }

}


/* drag and drop data get handler */

static void
caja_property_browser_drag_data_get (GtkWidget *widget,
                                     GdkDragContext *context,
                                     GtkSelectionData *selection_data,
                                     guint info,
                                     guint32 time)
{
    char  *image_file_name, *image_file_uri;
    gboolean is_reset;
    CajaPropertyBrowser *property_browser = CAJA_PROPERTY_BROWSER(widget);
    GdkAtom target;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (context != NULL);

    target = gtk_selection_data_get_target (selection_data);

    switch (info)
    {
    case PROPERTY_TYPE:
        /* formulate the drag data based on the drag type.  Eventually, we will
           probably select the behavior from properties in the category xml definition,
           but for now we hardwire it to the drag_type */

        is_reset = FALSE;
        if (strcmp (property_browser->details->drag_type,
                    "property/keyword") == 0)
        {
            char *keyword_str = eel_filename_strip_extension (property_browser->details->dragged_file);
            gtk_selection_data_set (selection_data, target, 8, keyword_str, strlen (keyword_str));
            g_free (keyword_str);
            return;
        }
        else if (strcmp (property_browser->details->drag_type,
                         "application/x-color") == 0)
        {
            GdkColor color;
            guint16 colorArray[4];

            /* handle the "reset" case as an image */
            if (g_strcmp0 (property_browser->details->dragged_file, RESET_IMAGE_NAME) != 0)
            {
                gdk_color_parse (property_browser->details->dragged_file, &color);

                colorArray[0] = color.red;
                colorArray[1] = color.green;
                colorArray[2] = color.blue;
                colorArray[3] = 0xffff;

                gtk_selection_data_set(selection_data,
                                       target, 16, (const char *) &colorArray[0], 8);
                return;
            }
            else
            {
                is_reset = TRUE;
            }

        }

        image_file_name = g_strdup_printf ("%s/%s/%s",
                                           CAJA_DATADIR,
                                           is_reset ? "patterns" : property_browser->details->category,
                                           property_browser->details->dragged_file);

        if (!g_file_test (image_file_name, G_FILE_TEST_EXISTS))
        {
            char *user_directory;
            g_free (image_file_name);

            user_directory = caja_get_user_directory ();
            image_file_name = g_strdup_printf ("%s/%s/%s",
                                               user_directory,
                                               property_browser->details->category,
                                               property_browser->details->dragged_file);

            g_free (user_directory);
        }

        image_file_uri = g_filename_to_uri (image_file_name, NULL, NULL);
        gtk_selection_data_set (selection_data, target, 8, image_file_uri, strlen (image_file_uri));
        g_free (image_file_name);
        g_free (image_file_uri);

        break;
    default:
        g_assert_not_reached ();
    }
}

/* drag and drop end handler, where we destroy ourselves, since the transaction is complete */

static void
caja_property_browser_drag_end (GtkWidget *widget, GdkDragContext *context)
{
    CajaPropertyBrowser *property_browser = CAJA_PROPERTY_BROWSER(widget);
    if (!property_browser->details->keep_around)
    {
        gtk_widget_hide (GTK_WIDGET (widget));
    }
}

/* utility routine to check if the passed-in uri is an image file */
static gboolean
ensure_file_is_image (GFile *file)
{
    GFileInfo *info;
    const char *mime_type;
    gboolean ret;

    info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE, 0, NULL, NULL);
    if (info == NULL)
    {
        return FALSE;
    }

    mime_type = g_file_info_get_content_type (info);
    if (mime_type == NULL)
    {
        return FALSE;
    }

    ret = (g_content_type_is_a (mime_type, "image/*") &&
           !g_content_type_equals (mime_type, "image/svg") &&
           !g_content_type_equals (mime_type, "image/svg+xml"));

    g_object_unref (info);

    return ret;
}

/* create the appropriate pixbuf for the passed in file */

static GdkPixbuf *
make_drag_image (CajaPropertyBrowser *property_browser, const char* file_name)
{
    GdkPixbuf *pixbuf, *orig_pixbuf;
    char *image_file_name;
    char *icon_name;
    gboolean is_reset;
    CajaIconInfo *info;

    if (property_browser->details->category_type == CAJA_PROPERTY_EMBLEM)
    {
        if (strcmp (file_name, "erase") == 0)
        {
            pixbuf = NULL;

            image_file_name = caja_pixmap_file (ERASE_OBJECT_NAME);
            if (image_file_name != NULL)
            {
                pixbuf = gdk_pixbuf_new_from_file (image_file_name, NULL);
            }
            g_free (image_file_name);
        }
        else
        {
            icon_name = caja_emblem_get_icon_name_from_keyword (file_name);
            info = caja_icon_info_lookup_from_name (icon_name, CAJA_ICON_SIZE_STANDARD);
            pixbuf = caja_icon_info_get_pixbuf_at_size (info, CAJA_ICON_SIZE_STANDARD);
            g_object_unref (info);
            g_free (icon_name);
        }
        return pixbuf;
    }

    image_file_name = g_strdup_printf ("%s/%s/%s",
                                       CAJA_DATADIR,
                                       property_browser->details->category,
                                       file_name);

    if (!g_file_test (image_file_name, G_FILE_TEST_EXISTS))
    {
        char *user_directory;
        g_free (image_file_name);

        user_directory = caja_get_user_directory ();

        image_file_name = g_strdup_printf ("%s/%s/%s",
                                           user_directory,
                                           property_browser->details->category,
                                           file_name);

        g_free (user_directory);
    }

    orig_pixbuf = gdk_pixbuf_new_from_file_at_scale (image_file_name,
                  MAX_ICON_WIDTH, MAX_ICON_HEIGHT,
                  TRUE,
                  NULL);

    g_free (image_file_name);

    if (orig_pixbuf == NULL)
    {
        return NULL;
    }

    is_reset = g_strcmp0 (file_name, RESET_IMAGE_NAME) == 0;

    if (strcmp (property_browser->details->category, "patterns") == 0 &&
            property_browser->details->property_chit != NULL)
    {
        pixbuf = caja_customization_make_pattern_chit (orig_pixbuf, property_browser->details->property_chit, TRUE, is_reset);
    }
    else
    {
        pixbuf = eel_gdk_pixbuf_scale_down_to_fit (orig_pixbuf, MAX_ICON_WIDTH, MAX_ICON_HEIGHT);
    }

    g_object_unref (orig_pixbuf);

    return pixbuf;
}


/* create a pixbuf and fill it with a color */

static GdkPixbuf*
make_color_drag_image (CajaPropertyBrowser *property_browser, const char *color_spec, gboolean trim_edges)
{
    GdkPixbuf *color_square;
    GdkPixbuf *ret;
    int row, col, stride;
    char *pixels, *row_pixels;
    GdkColor color;

    color_square = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, COLOR_SQUARE_SIZE, COLOR_SQUARE_SIZE);

    gdk_color_parse (color_spec, &color);
    color.red >>= 8;
    color.green >>= 8;
    color.blue >>= 8;

    pixels = gdk_pixbuf_get_pixels (color_square);
    stride = gdk_pixbuf_get_rowstride (color_square);

    /* loop through and set each pixel */
    for (row = 0; row < COLOR_SQUARE_SIZE; row++)
    {
        row_pixels =  (pixels + (row * stride));
        for (col = 0; col < COLOR_SQUARE_SIZE; col++)
        {
            *row_pixels++ = color.red;
            *row_pixels++ = color.green;
            *row_pixels++ = color.blue;
            *row_pixels++ = 255;
        }
    }

    g_assert (color_square != NULL);

    if (property_browser->details->property_chit != NULL)
    {
        ret = caja_customization_make_pattern_chit (color_square,
                property_browser->details->property_chit,
                trim_edges, FALSE);
        g_object_unref (color_square);
    }
    else
    {
        ret = color_square;
    }

    return ret;
}

/* this callback handles button presses on the category widget. It maintains the active state */

static void
category_toggled_callback (GtkWidget *widget, char *category_name)
{
    CajaPropertyBrowser *property_browser;

    property_browser = CAJA_PROPERTY_BROWSER (g_object_get_data (G_OBJECT (widget), "user_data"));

    /* exit remove mode when the user switches categories, since there might be nothing to remove
       in the new category */
    property_browser->details->remove_mode = FALSE;

    caja_property_browser_set_category (property_browser, category_name);
}

static xmlDocPtr
read_browser_xml (CajaPropertyBrowser *property_browser)
{
    char *path;
    xmlDocPtr document;

    path = caja_get_data_file_path (property_browser->details->path);
    if (path == NULL)
    {
        return NULL;
    }
    document = xmlParseFile (path);
    g_free (path);
    return document;
}

static void
write_browser_xml (CajaPropertyBrowser *property_browser,
                   xmlDocPtr document)
{
    char *user_directory, *path;

    user_directory = caja_get_user_directory ();
    path = g_build_filename (user_directory, property_browser->details->path, NULL);
    g_free (user_directory);
    xmlSaveFile (path, document);
    g_free (path);
}

static xmlNodePtr
get_color_category (xmlDocPtr document)
{
    return eel_xml_get_root_child_by_name_and_property (document, "category", "name", "colors");
}

/* routines to remove specific category types.  First, handle colors */

static void
remove_color (CajaPropertyBrowser *property_browser, const char* color_name)
{
    /* load the local xml file to remove the color */
    xmlDocPtr document;
    xmlNodePtr cur_node, color_node;
    gboolean match;
    char *name;

    document = read_browser_xml (property_browser);
    if (document == NULL)
    {
        return;
    }

    /* find the colors category */
    cur_node = get_color_category (document);
    if (cur_node != NULL)
    {
        /* loop through the colors to find one that matches */
        for (color_node = eel_xml_get_children (cur_node);
                color_node != NULL;
                color_node = color_node->next)
        {

            if (color_node->type != XML_ELEMENT_NODE)
            {
                continue;
            }

            name = xmlGetProp (color_node, "name");
            match = name != NULL
                    && strcmp (name, color_name) == 0;
            xmlFree (name);

            if (match)
            {
                xmlUnlinkNode (color_node);
                xmlFreeNode (color_node);
                write_browser_xml (property_browser, document);
                break;
            }
        }
    }

    xmlFreeDoc (document);
}

/* remove the pattern matching the passed in name */

static void
remove_pattern(CajaPropertyBrowser *property_browser, const char* pattern_name)
{
    char *pattern_path;
    char *user_directory;

    user_directory = caja_get_user_directory ();

    /* build the pathname of the pattern */
    pattern_path = g_build_filename (user_directory,
                                     "patterns",
                                     pattern_name,
                                     NULL);
    g_free (user_directory);

    /* delete the pattern from the pattern directory */
    if (g_unlink (pattern_path) != 0)
    {
        char *message = g_strdup_printf (_("Sorry, but pattern %s could not be deleted."), pattern_name);
        char *detail = _("Check that you have permission to delete the pattern.");
        eel_show_error_dialog (message, detail, GTK_WINDOW (property_browser));
        g_free (message);
    }

    g_free (pattern_path);
}

/* remove the emblem matching the passed in name */

static void
remove_emblem (CajaPropertyBrowser *property_browser, const char* emblem_name)
{
    /* delete the emblem from the emblem directory */
    if (caja_emblem_remove_emblem (emblem_name) == FALSE)
    {
        char *message = g_strdup_printf (_("Sorry, but emblem %s could not be deleted."), emblem_name);
        char *detail = _("Check that you have permission to delete the emblem.");
        eel_show_error_dialog (message, detail, GTK_WINDOW (property_browser));
        g_free (message);
    }
    else
    {
        emit_emblems_changed_signal ();
    }
}

/* handle removing the passed in element */

static void
caja_property_browser_remove_element (CajaPropertyBrowser *property_browser, EelLabeledImage *child)
{
    const char *element_name;
    char *color_name;

    element_name = g_object_get_data (G_OBJECT (child), "property-name");

    /* lookup category and get mode, then case out and handle the modes */
    switch (property_browser->details->category_type)
    {
    case CAJA_PROPERTY_PATTERN:
        remove_pattern (property_browser, element_name);
        break;
    case CAJA_PROPERTY_COLOR:
        color_name = eel_labeled_image_get_text (child);
        remove_color (property_browser, color_name);
        g_free (color_name);
        break;
    case CAJA_PROPERTY_EMBLEM:
        remove_emblem (property_browser, element_name);
        break;
    default:
        break;
    }
}

static void
update_preview_cb (GtkFileChooser *fc,
                   GtkImage *preview)
{
    char *filename;
    GdkPixbuf *pixbuf;

    filename = gtk_file_chooser_get_preview_filename (fc);
    if (filename)
    {
        pixbuf = gdk_pixbuf_new_from_file (filename, NULL);

        gtk_file_chooser_set_preview_widget_active (fc, pixbuf != NULL);

        if (pixbuf)
        {
            gtk_image_set_from_pixbuf (preview, pixbuf);
            g_object_unref (pixbuf);
        }

        g_free (filename);
    }
}

static void
icon_button_clicked_cb (GtkButton *b,
                        CajaPropertyBrowser *browser)
{
    GtkWidget *dialog;
    GtkFileFilter *filter;
    GtkWidget *preview;
    int res;

    dialog = gtk_file_chooser_dialog_new (_("Select an Image File for the New Emblem"),
                                          GTK_WINDOW (browser),
                                          GTK_FILE_CHOOSER_ACTION_OPEN,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                          NULL);
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
                                         DATADIR "/pixmaps");
    filter = gtk_file_filter_new ();
    gtk_file_filter_add_pixbuf_formats (filter);
    gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);

    preview = gtk_image_new ();
    gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (dialog),
                                         preview);
    g_signal_connect (dialog, "update-preview",
                      G_CALLBACK (update_preview_cb), preview);

    res = gtk_dialog_run (GTK_DIALOG (dialog));

    if (res == GTK_RESPONSE_ACCEPT)
    {
        /* update the image */
        g_free (browser->details->filename);
        browser->details->filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        gtk_image_set_from_file (GTK_IMAGE (browser->details->image_button), browser->details->filename);
    }

    gtk_widget_destroy (dialog);
}

/* here's where we create the emblem dialog */
static GtkWidget*
caja_emblem_dialog_new (CajaPropertyBrowser *property_browser)
{
    GtkWidget *widget;
    GtkWidget *button;
    GtkWidget *dialog;
    GtkWidget *label;
    GtkWidget *table = gtk_table_new(2, 2, FALSE);

    dialog = gtk_dialog_new_with_buttons (_("Create a New Emblem"),
                                          GTK_WINDOW (property_browser), 0,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_OK, GTK_RESPONSE_OK,
                                          NULL);

    /* install the table in the dialog */
    gtk_container_set_border_width (GTK_CONTAINER (table), 5);
    gtk_table_set_row_spacings (GTK_TABLE (table), 6);
    gtk_table_set_col_spacings (GTK_TABLE (table), 12);
    gtk_widget_show (table);

    gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), 2);
    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
    gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), table, TRUE, TRUE, 0);
    gtk_dialog_set_default_response (GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    /* make the keyword label and field */

    widget = gtk_label_new_with_mnemonic(_("_Keyword:"));
    gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
    gtk_widget_show(widget);
    gtk_table_attach(GTK_TABLE(table), widget, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);

    property_browser->details->keyword = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (property_browser->details->keyword), TRUE);
    gtk_entry_set_max_length (GTK_ENTRY (property_browser->details->keyword), 24);
    gtk_widget_show(property_browser->details->keyword);
    gtk_table_attach(GTK_TABLE(table), property_browser->details->keyword, 1, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
    gtk_widget_grab_focus(property_browser->details->keyword);
    gtk_label_set_mnemonic_widget (GTK_LABEL (widget),
                                   GTK_WIDGET (property_browser->details->keyword));

    /* default image is the generic emblem */
    g_free (property_browser->details->image_path);
    property_browser->details->image_path = g_build_filename (CAJA_PIXMAPDIR, "emblems.png", NULL);

    /* set up a file chooser to pick the image file */
    label = gtk_label_new_with_mnemonic (_("_Image:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);

    widget = gtk_hbox_new (FALSE, 0);
    gtk_widget_show (widget);

    button = gtk_button_new ();
    property_browser->details->image_button = gtk_image_new_from_file (property_browser->details->image_path);
    gtk_button_set_image (GTK_BUTTON (button), property_browser->details->image_button);
    g_signal_connect (button, "clicked", G_CALLBACK (icon_button_clicked_cb),
                      property_browser);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), button);

    gtk_widget_show (button);
    gtk_table_attach (GTK_TABLE (table), widget, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
    gtk_box_pack_start (GTK_BOX (widget), button, FALSE, FALSE, 0);

    return dialog;
}

/* create the color selection dialog */

static GtkWidget*
caja_color_selection_dialog_new (CajaPropertyBrowser *property_browser)
{
    GtkWidget *widget;
    GtkWidget *dialog;
    GtkWidget *table = gtk_table_new(2, 2, FALSE);

    dialog = gtk_dialog_new_with_buttons (_("Create a New Color:"),
                                          GTK_WINDOW (property_browser), 0,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_OK, GTK_RESPONSE_OK,
                                          NULL);

    /* install the table in the dialog */

    gtk_widget_show (table);
    gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), table, TRUE, TRUE, 0);
    gtk_dialog_set_default_response (GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    /* make the name label and field */

    widget = gtk_label_new_with_mnemonic(_("Color _name:"));
    gtk_widget_show(widget);
    gtk_table_attach(GTK_TABLE(table), widget, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);

    property_browser->details->color_name = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (property_browser->details->color_name), TRUE);
    gtk_entry_set_max_length (GTK_ENTRY (property_browser->details->color_name), 24);
    gtk_widget_grab_focus (property_browser->details->color_name);
    gtk_label_set_mnemonic_widget (GTK_LABEL (widget), property_browser->details->color_name);
    gtk_widget_show(property_browser->details->color_name);
    gtk_table_attach(GTK_TABLE(table), property_browser->details->color_name, 1, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
    gtk_widget_grab_focus(property_browser->details->color_name);

    /* default image is the generic emblem */
    g_free(property_browser->details->image_path);

    widget = gtk_label_new_with_mnemonic(_("Color _value:"));
    gtk_widget_show(widget);
    gtk_table_attach(GTK_TABLE(table), widget, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);

    property_browser->details->color_picker = gtk_color_button_new ();
    gtk_widget_show (property_browser->details->color_picker);
    gtk_label_set_mnemonic_widget (GTK_LABEL (widget), property_browser->details->color_picker);

    gtk_table_attach(GTK_TABLE(table), property_browser->details->color_picker, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);

    return dialog;
}

/* add the newly selected file to the browser images */
static void
add_pattern_to_browser (GtkDialog *dialog, gint response_id, gpointer *data)
{
    char *directory_path, *destination_name;
    char *basename;
    char *user_directory;
    GFile *dest, *selected;

    CajaPropertyBrowser *property_browser = CAJA_PROPERTY_BROWSER (data);

    if (response_id != GTK_RESPONSE_ACCEPT)
    {
        gtk_widget_hide (GTK_WIDGET (dialog));
        return;
    }

    selected = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));

    /* don't allow the user to change the reset image */
    basename = g_file_get_basename (selected);
    if (basename && g_strcmp0 (basename, RESET_IMAGE_NAME) == 0)
    {
        eel_show_error_dialog (_("Sorry, but you cannot replace the reset image."),
                               _("Reset is a special image that cannot be deleted."),
                               NULL);
        g_object_unref (selected);
        g_free (basename);
        return;
    }


    user_directory = caja_get_user_directory ();

    /* copy the image file to the patterns directory */
    directory_path = g_build_filename (user_directory, "patterns", NULL);
    g_free (user_directory);
    destination_name = g_build_filename (directory_path, basename, NULL);

    /* make the directory if it doesn't exist */
    if (!g_file_test (directory_path, G_FILE_TEST_EXISTS))
    {
        g_mkdir_with_parents (directory_path, 0775);
    }

    dest = g_file_new_for_path (destination_name);

    g_free (destination_name);
    g_free (directory_path);

    if (!g_file_copy (selected, dest,
                      0,
                      NULL, NULL, NULL, NULL))
    {
        char *message = g_strdup_printf (_("Sorry, but the pattern %s could not be installed."), basename);
        eel_show_error_dialog (message, NULL, GTK_WINDOW (property_browser));
        g_free (message);
    }
    g_object_unref (selected);
    g_object_unref (dest);
    g_free (basename);


    /* update the property browser's contents to show the new one */
    caja_property_browser_update_contents (property_browser);

    gtk_widget_hide (GTK_WIDGET (dialog));
}

/* here's where we initiate adding a new pattern by putting up a file selector */

static void
add_new_pattern (CajaPropertyBrowser *property_browser)
{
    GtkWidget *dialog;
    GtkFileFilter *filter;
    GtkWidget *preview;

    if (property_browser->details->patterns_dialog)
    {
        gtk_window_present (GTK_WINDOW (property_browser->details->patterns_dialog));
    }
    else
    {
        property_browser->details->patterns_dialog = dialog =
                    gtk_file_chooser_dialog_new (_("Select an Image File to Add as a Pattern"),
                            GTK_WINDOW (property_browser),
                            GTK_FILE_CHOOSER_ACTION_OPEN,
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                            NULL);
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
                                             DATADIR "/caja/patterns/");
        filter = gtk_file_filter_new ();
        gtk_file_filter_add_pixbuf_formats (filter);
        gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);

        gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), FALSE);

        preview = gtk_image_new ();
        gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (dialog),
                                             preview);
        g_signal_connect (dialog, "update-preview",
                          G_CALLBACK (update_preview_cb), preview);

        g_signal_connect (dialog, "response",
                          G_CALLBACK (add_pattern_to_browser),
                          property_browser);

        gtk_widget_show (GTK_WIDGET (dialog));

        if (property_browser->details->patterns_dialog)
            eel_add_weak_pointer (&property_browser->details->patterns_dialog);
    }
}

/* here's where we add the passed in color to the file that defines the colors */

static void
add_color_to_file (CajaPropertyBrowser *property_browser, const char *color_spec, const char *color_name)
{
    xmlNodePtr cur_node, new_color_node, children_node;
    xmlDocPtr document;
    xmlChar *child_color_name;
    gboolean color_name_exists = FALSE;

    document = read_browser_xml (property_browser);
    if (document == NULL)
    {
        return;
    }

    /* find the colors category */
    cur_node = get_color_category (document);
    if (cur_node != NULL)
    {
        /* check if theres already a color whith that name */
        children_node = cur_node->xmlChildrenNode;
        while (children_node != NULL)
        {
            child_color_name = xmlGetProp (children_node, "name");
            if (xmlStrcmp (color_name, child_color_name) == 0)
            {
                color_name_exists = TRUE;
                xmlFree (child_color_name);
                break;
            }
            xmlFree (child_color_name);

            children_node = children_node->next;
        }

        /* add a new color node */
        if (!color_name_exists)
        {
            new_color_node = xmlNewChild (cur_node, NULL, "color", NULL);
            xmlNodeSetContent (new_color_node, color_spec);
            xmlSetProp (new_color_node, "local", "1");
            xmlSetProp (new_color_node, "name", color_name);

            write_browser_xml (property_browser, document);
        }
        else
        {
            eel_show_error_dialog (_("The color cannot be installed."),
                                   _("Sorry, but you must specify an unused color name for the new color."),
                                   GTK_WINDOW (property_browser));
        }
    }

    xmlFreeDoc (document);
}

/* handle the OK button being pushed on the color selection dialog */
static void
add_color_to_browser (GtkWidget *widget, gint which_button, gpointer *data)
{
    char * color_spec;
    const char *color_name;
    char *stripped_color_name;

    CajaPropertyBrowser *property_browser = CAJA_PROPERTY_BROWSER (data);

    if (which_button == GTK_RESPONSE_OK)
    {
        GdkColor color;

        gtk_color_button_get_color (GTK_COLOR_BUTTON (property_browser->details->color_picker), &color);
        color_spec = gdk_color_to_string (&color);

        color_name = gtk_entry_get_text (GTK_ENTRY (property_browser->details->color_name));
        stripped_color_name = g_strstrip (g_strdup (color_name));
        if (strlen (stripped_color_name) == 0)
        {
            eel_show_error_dialog (_("The color cannot be installed."),
                                   _("Sorry, but you must specify a non-blank name for the new color."),
                                   GTK_WINDOW (property_browser));

        }
        else
        {
            add_color_to_file (property_browser, color_spec, stripped_color_name);
            caja_property_browser_update_contents(property_browser);
        }
        g_free (stripped_color_name);
        g_free (color_spec);
    }

    gtk_widget_destroy(property_browser->details->colors_dialog);
    property_browser->details->colors_dialog = NULL;
}

/* create the color selection dialog, pre-set with the color that was just selected */
static void
show_color_selection_window (GtkWidget *widget, gpointer *data)
{
    GdkColor color;
    CajaPropertyBrowser *property_browser = CAJA_PROPERTY_BROWSER(data);

    gtk_color_selection_get_current_color (GTK_COLOR_SELECTION
                                           (gtk_color_selection_dialog_get_color_selection (GTK_COLOR_SELECTION_DIALOG (property_browser->details->colors_dialog))),
                                           &color);
    gtk_widget_destroy (property_browser->details->colors_dialog);

    /* allocate a new color selection dialog */
    property_browser->details->colors_dialog = caja_color_selection_dialog_new (property_browser);

    /* set the color to the one picked by the selector */
    gtk_color_button_set_color (GTK_COLOR_BUTTON (property_browser->details->color_picker), &color);

    /* connect the signals to the new dialog */

    eel_add_weak_pointer (&property_browser->details->colors_dialog);

    g_signal_connect_object (property_browser->details->colors_dialog, "response",
                             G_CALLBACK (add_color_to_browser), property_browser, 0);
    gtk_window_set_position (GTK_WINDOW (property_browser->details->colors_dialog), GTK_WIN_POS_MOUSE);
    gtk_widget_show (GTK_WIDGET(property_browser->details->colors_dialog));
}


/* here's the routine to add a new color, by putting up a color selector */

static void
add_new_color (CajaPropertyBrowser *property_browser)
{
    if (property_browser->details->colors_dialog)
    {
        gtk_window_present (GTK_WINDOW (property_browser->details->colors_dialog));
    }
    else
    {
        GtkColorSelectionDialog *color_dialog;
        GtkWidget *ok_button, *cancel_button, *help_button;

        property_browser->details->colors_dialog = gtk_color_selection_dialog_new (_("Select a Color to Add"));
        color_dialog = GTK_COLOR_SELECTION_DIALOG (property_browser->details->colors_dialog);

        eel_add_weak_pointer (&property_browser->details->colors_dialog);

        g_object_get (color_dialog, "ok-button", &ok_button,
                      "cancel-button", &cancel_button,
                      "help-button", &help_button, NULL);

        g_signal_connect_object (ok_button, "clicked",
                                 G_CALLBACK (show_color_selection_window), property_browser, 0);
        g_signal_connect_object (cancel_button, "clicked",
                                 G_CALLBACK (gtk_widget_destroy), color_dialog, G_CONNECT_SWAPPED);
        gtk_widget_hide (help_button);

        gtk_window_set_position (GTK_WINDOW (color_dialog), GTK_WIN_POS_MOUSE);
        gtk_widget_show (GTK_WIDGET(color_dialog));
    }
}

/* here's where we handle clicks in the emblem dialog buttons */
static void
emblem_dialog_clicked (GtkWidget *dialog, int which_button, CajaPropertyBrowser *property_browser)
{
    const char *new_keyword;
    char *stripped_keyword;
    char *emblem_path;
    GFile *emblem_file;
    GdkPixbuf *pixbuf;

    if (which_button == GTK_RESPONSE_OK)
    {

        /* update the image path from the file entry */
        if (property_browser->details->filename)
        {
            emblem_path = property_browser->details->filename;
            emblem_file = g_file_new_for_path (emblem_path);
            if (ensure_file_is_image (emblem_file))
            {
                g_free (property_browser->details->image_path);
                property_browser->details->image_path = emblem_path;
            }
            else
            {
                char *message = g_strdup_printf
                                (_("Sorry, but \"%s\" is not a usable image file."), emblem_path);
                eel_show_error_dialog (_("The file is not an image."), message, GTK_WINDOW (property_browser));
                g_free (message);
                g_free (emblem_path);
                emblem_path = NULL;
                g_object_unref (emblem_file);
                return;
            }
            g_object_unref (emblem_file);
        }

        emblem_file = g_file_new_for_path (property_browser->details->image_path);
        pixbuf = caja_emblem_load_pixbuf_for_emblem (emblem_file);
        g_object_unref (emblem_file);

        if (pixbuf == NULL)
        {
            char *message = g_strdup_printf
                            (_("Sorry, but \"%s\" is not a usable image file."), property_browser->details->image_path);
            eel_show_error_dialog (_("The file is not an image."), message, GTK_WINDOW (property_browser));
            g_free (message);
        }

        new_keyword = gtk_entry_get_text(GTK_ENTRY(property_browser->details->keyword));
        if (new_keyword == NULL)
        {
            stripped_keyword = NULL;
        }
        else
        {
            stripped_keyword = g_strstrip (g_strdup (new_keyword));
        }


        caja_emblem_install_custom_emblem (pixbuf,
                                           stripped_keyword,
                                           stripped_keyword,
                                           GTK_WINDOW (property_browser));
        if (pixbuf != NULL)
            g_object_unref (pixbuf);

        caja_emblem_refresh_list ();

        emit_emblems_changed_signal ();

        g_free (stripped_keyword);
    }

    gtk_widget_destroy (dialog);

    property_browser->details->keyword = NULL;
    property_browser->details->emblem_image = NULL;
    property_browser->details->filename = NULL;
}

/* here's the routine to add a new emblem, by putting up an emblem dialog */

static void
add_new_emblem (CajaPropertyBrowser *property_browser)
{
    if (property_browser->details->emblems_dialog)
    {
        gtk_window_present (GTK_WINDOW (property_browser->details->emblems_dialog));
    }
    else
    {
        property_browser->details->emblems_dialog = caja_emblem_dialog_new (property_browser);

        eel_add_weak_pointer (&property_browser->details->emblems_dialog);

        g_signal_connect_object (property_browser->details->emblems_dialog, "response",
                                 G_CALLBACK (emblem_dialog_clicked), property_browser, 0);
        gtk_window_set_position (GTK_WINDOW (property_browser->details->emblems_dialog), GTK_WIN_POS_MOUSE);
        gtk_widget_show (GTK_WIDGET(property_browser->details->emblems_dialog));
    }
}

/* cancelremove mode */
static void
cancel_remove_mode (CajaPropertyBrowser *property_browser)
{
    if (property_browser->details->remove_mode)
    {
        property_browser->details->remove_mode = FALSE;
        caja_property_browser_update_contents(property_browser);
        gtk_widget_show (property_browser->details->help_label);
    }
}

/* handle the add_new button */

static void
add_new_button_callback(GtkWidget *widget, CajaPropertyBrowser *property_browser)
{
    /* handle remove mode, where we act as a cancel button */
    if (property_browser->details->remove_mode)
    {
        cancel_remove_mode (property_browser);
        return;
    }

    switch (property_browser->details->category_type)
    {
    case CAJA_PROPERTY_PATTERN:
        add_new_pattern (property_browser);
        break;
    case CAJA_PROPERTY_COLOR:
        add_new_color (property_browser);
        break;
    case CAJA_PROPERTY_EMBLEM:
        add_new_emblem (property_browser);
        break;
    default:
        break;
    }
}

/* handle the "done" button */
static void
done_button_callback (GtkWidget *widget, GtkWidget *property_browser)
{
    cancel_remove_mode (CAJA_PROPERTY_BROWSER (property_browser));
    gtk_widget_hide (property_browser);
}

/* handle the "help" button */
static void
help_button_callback (GtkWidget *widget, GtkWidget *property_browser)
{
    GError *error = NULL;
    GtkWidget *dialog;

    gtk_show_uri (gtk_widget_get_screen (property_browser),
                  "help:user-guide#goscaja-50",
                  gtk_get_current_event_time (), &error);

    if (error)
    {
        dialog = gtk_message_dialog_new (GTK_WINDOW (property_browser),
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_OK,
                                         _("There was an error displaying help: \n%s"),
                                         error->message);

        g_signal_connect (G_OBJECT (dialog),
                          "response", G_CALLBACK (gtk_widget_destroy),
                          NULL);
        gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
        gtk_widget_show (dialog);
        g_error_free (error);
    }
}

/* handle the "remove" button */
static void
remove_button_callback(GtkWidget *widget, CajaPropertyBrowser *property_browser)
{
    if (property_browser->details->remove_mode)
    {
        return;
    }

    property_browser->details->remove_mode = TRUE;
    gtk_widget_hide (property_browser->details->help_label);
    caja_property_browser_update_contents(property_browser);
}

/* this callback handles clicks on the image or color based content content elements */

static void
element_clicked_callback (GtkWidget *image_table,
                          GtkWidget *child,
                          const EelImageTableEvent *event,
                          gpointer callback_data)
{
    CajaPropertyBrowser *property_browser;
    GtkTargetList *target_list;
    GdkDragContext *context;
    const char *element_name;
    GdkDragAction action;

    g_return_if_fail (EEL_IS_IMAGE_TABLE (image_table));
    g_return_if_fail (EEL_IS_LABELED_IMAGE (child));
    g_return_if_fail (event != NULL);
    g_return_if_fail (CAJA_IS_PROPERTY_BROWSER (callback_data));
    g_return_if_fail (g_object_get_data (G_OBJECT (child), "property-name") != NULL);

    element_name = g_object_get_data (G_OBJECT (child), "property-name");
    property_browser = CAJA_PROPERTY_BROWSER (callback_data);

    /* handle remove mode by removing the element */
    if (property_browser->details->remove_mode)
    {
        caja_property_browser_remove_element (property_browser, EEL_LABELED_IMAGE (child));
        property_browser->details->remove_mode = FALSE;
        caja_property_browser_update_contents (property_browser);
        gtk_widget_show (property_browser->details->help_label);
        return;
    }

    /* set up the drag and drop type corresponding to the category */
    drag_types[0].target = property_browser->details->drag_type;

    /* treat the reset property in the colors section specially */
    if (g_strcmp0 (element_name, RESET_IMAGE_NAME) == 0)
    {
        drag_types[0].target = "x-special/mate-reset-background";
    }

    target_list = gtk_target_list_new (drag_types, G_N_ELEMENTS (drag_types));
    caja_property_browser_set_dragged_file(property_browser, element_name);
    action = event->button == 3 ? GDK_ACTION_ASK : GDK_ACTION_MOVE | GDK_ACTION_COPY;

    g_object_set_data (G_OBJECT (property_browser), "dragged-image", child);

    context = gtk_drag_begin (GTK_WIDGET (property_browser),
                              target_list,
                              GDK_ACTION_ASK | GDK_ACTION_MOVE | GDK_ACTION_COPY,
                              event->button,
                              event->event);
    gtk_target_list_unref (target_list);

    /* optionally (if the shift key is down) hide the property browser - it will later be destroyed when the drag ends */
    property_browser->details->keep_around = (event->state & GDK_SHIFT_MASK) == 0;
    if (! property_browser->details->keep_around)
    {
        gtk_widget_hide (GTK_WIDGET (property_browser));
    }
}

static void
labeled_image_configure (EelLabeledImage *labeled_image)
{
    g_return_if_fail (EEL_IS_LABELED_IMAGE (labeled_image));

    eel_labeled_image_set_spacing (labeled_image, LABELED_IMAGE_SPACING);
}

/* Make a color tile for a property */
static GtkWidget *
labeled_image_new (const char *text,
                   GdkPixbuf *pixbuf,
                   const char *property_name,
                   double scale_factor)
{
    GtkWidget *labeled_image;

    labeled_image = eel_labeled_image_new (text, pixbuf);
    labeled_image_configure (EEL_LABELED_IMAGE (labeled_image));

    if (property_name != NULL)
    {
        g_object_set_data_full (G_OBJECT (labeled_image),
                                "property-name",
                                g_strdup (property_name),
                                g_free);
    }

    return labeled_image;
}

static void
make_properties_from_directories (CajaPropertyBrowser *property_browser)
{
    CajaCustomizationData *customization_data;
    char *object_name;
    char *object_label;
    GdkPixbuf *object_pixbuf;
    EelImageTable *image_table;
    GtkWidget *reset_object = NULL;
    GList *icons, *l;
    char *icon_name;
    char *keyword;
    GtkWidget *property_image;
    GtkWidget *blank;
    guint num_images;
    char *path;
    CajaIconInfo *info;

    g_return_if_fail (CAJA_IS_PROPERTY_BROWSER (property_browser));
    g_return_if_fail (EEL_IS_IMAGE_TABLE (property_browser->details->content_table));

    image_table = EEL_IMAGE_TABLE (property_browser->details->content_table);

    if (property_browser->details->category_type == CAJA_PROPERTY_EMBLEM)
    {
        g_list_free_full (property_browser->details->keywords, g_free);
        property_browser->details->keywords = NULL;

        icons = caja_emblem_list_available ();

        property_browser->details->has_local = FALSE;
        l = icons;
        while (l != NULL)
        {
            icon_name = (char *)l->data;
            l = l->next;

            if (!caja_emblem_should_show_in_list (icon_name))
            {
                continue;
            }

            object_name = caja_emblem_get_keyword_from_icon_name (icon_name);
            if (caja_emblem_can_remove_emblem (object_name))
            {
                property_browser->details->has_local = TRUE;
            }
            else if (property_browser->details->remove_mode)
            {
                g_free (object_name);
                continue;
            }
            info = caja_icon_info_lookup_from_name (icon_name, CAJA_ICON_SIZE_STANDARD);
            object_pixbuf = caja_icon_info_get_pixbuf_at_size (info, CAJA_ICON_SIZE_STANDARD);
            object_label = g_strdup (caja_icon_info_get_display_name (info));
            g_object_unref (info);

            if (object_label == NULL)
            {
                object_label = g_strdup (object_name);
            }

            property_image = labeled_image_new (object_label, object_pixbuf, object_name, PANGO_SCALE_LARGE);
            eel_labeled_image_set_fixed_image_height (EEL_LABELED_IMAGE (property_image), MAX_EMBLEM_HEIGHT);

            keyword = eel_filename_strip_extension (object_name);
            property_browser->details->keywords = g_list_prepend (property_browser->details->keywords,
                                                  keyword);

            gtk_container_add (GTK_CONTAINER (image_table), property_image);
            gtk_widget_show (property_image);

            g_free (object_name);
            g_free (object_label);
            if (object_pixbuf != NULL)
            {
                g_object_unref (object_pixbuf);
            }
        }
        g_list_free_full (icons, g_free);
    }
    else
    {
        customization_data = caja_customization_data_new (property_browser->details->category,
                             !property_browser->details->remove_mode,
                             MAX_ICON_WIDTH,
                             MAX_ICON_HEIGHT);
        if (customization_data == NULL)
        {
            return;
        }

        /* interate through the set of objects and display each */
        while (caja_customization_data_get_next_element_for_display (customization_data,
                &object_name,
                &object_pixbuf,
                &object_label))
        {

            property_image = labeled_image_new (object_label, object_pixbuf, object_name, PANGO_SCALE_LARGE);

            gtk_container_add (GTK_CONTAINER (image_table), property_image);
            gtk_widget_show (property_image);

            /* Keep track of ERASE objects to place them prominently later */
            if (property_browser->details->category_type == CAJA_PROPERTY_PATTERN
                    && !g_strcmp0 (object_name, RESET_IMAGE_NAME))
            {
                g_assert (reset_object == NULL);
                reset_object = property_image;
            }

            gtk_widget_show (property_image);

            g_free (object_name);
            g_free (object_label);
            if (object_pixbuf != NULL)
            {
                g_object_unref (object_pixbuf);
            }
        }

        property_browser->details->has_local = caja_customization_data_private_data_was_displayed (customization_data);
        caja_customization_data_destroy (customization_data);
    }

    /*
     * We place ERASE objects (for emblems) at the end with a blank in between.
     */
    if (property_browser->details->category_type == CAJA_PROPERTY_EMBLEM)
    {
        blank = eel_image_table_add_empty_image (image_table);
        labeled_image_configure (EEL_LABELED_IMAGE (blank));


        num_images = eel_wrap_table_get_num_children (EEL_WRAP_TABLE (image_table));
        g_assert (num_images > 0);
        eel_wrap_table_reorder_child (EEL_WRAP_TABLE (image_table),
                                      blank,
                                      num_images - 1);

        gtk_widget_show (blank);

        object_pixbuf = NULL;

        path = caja_pixmap_file (ERASE_OBJECT_NAME);
        if (path != NULL)
        {
            object_pixbuf = gdk_pixbuf_new_from_file (path, NULL);
        }
        g_free (path);
        property_image = labeled_image_new (_("Erase"), object_pixbuf, "erase", PANGO_SCALE_LARGE);
        eel_labeled_image_set_fixed_image_height (EEL_LABELED_IMAGE (property_image), MAX_EMBLEM_HEIGHT);

        gtk_container_add (GTK_CONTAINER (image_table), property_image);
        gtk_widget_show (property_image);

        eel_wrap_table_reorder_child (EEL_WRAP_TABLE (image_table),
                                      property_image, -1);

        if (object_pixbuf != NULL)
        {
            g_object_unref (object_pixbuf);
        }
    }

    /*
     * We place RESET objects (for colors and patterns) at the beginning.
     */
    if (reset_object != NULL)
    {
        g_assert (EEL_IS_LABELED_IMAGE (reset_object));
        eel_wrap_table_reorder_child (EEL_WRAP_TABLE (image_table),
                                      reset_object,
                                      0);
    }

}

/* utility routine to add a reset property in the first position */
static void
add_reset_property (CajaPropertyBrowser *property_browser)
{
    char *reset_image_file_name;
    GtkWidget *reset_image;
    GdkPixbuf *reset_pixbuf, *reset_chit;

    reset_chit = NULL;

    reset_image_file_name = g_strdup_printf ("%s/%s/%s", CAJA_DATADIR, "patterns", RESET_IMAGE_NAME);
    reset_pixbuf = gdk_pixbuf_new_from_file (reset_image_file_name, NULL);
    if (reset_pixbuf != NULL && property_browser->details->property_chit != NULL)
    {
        reset_chit = caja_customization_make_pattern_chit (reset_pixbuf, property_browser->details->property_chit, FALSE, TRUE);
    }

    g_free (reset_image_file_name);

    reset_image = labeled_image_new (_("Reset"), reset_chit != NULL ? reset_chit : reset_pixbuf, RESET_IMAGE_NAME, PANGO_SCALE_MEDIUM);
    gtk_container_add (GTK_CONTAINER (property_browser->details->content_table), reset_image);
    eel_wrap_table_reorder_child (EEL_WRAP_TABLE (property_browser->details->content_table),
                                  reset_image,
                                  0);
    gtk_widget_show (reset_image);

    if (reset_pixbuf != NULL)
    {
        g_object_unref (reset_pixbuf);
    }

    if (reset_chit != NULL)
    {
        g_object_unref (reset_chit);
    }
}

/* generate properties from the children of the passed in node */
/* for now, we just handle color nodes */

static void
make_properties_from_xml_node (CajaPropertyBrowser *property_browser,
                               xmlNodePtr node)
{
    xmlNodePtr child_node;
    GdkPixbuf *pixbuf;
    GtkWidget *new_property;
    char *deleted, *local, *color, *name;

    gboolean local_only = property_browser->details->remove_mode;

    /* add a reset property in the first slot */
    if (!property_browser->details->remove_mode)
    {
        add_reset_property (property_browser);
    }

    property_browser->details->has_local = FALSE;

    for (child_node = eel_xml_get_children (node);
            child_node != NULL;
            child_node = child_node->next)
    {

        if (child_node->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        /* We used to mark colors that were removed with the "deleted" attribute.
         * To prevent these colors from suddenly showing up now, this legacy remains.
         */
        deleted = xmlGetProp (child_node, "deleted");
        local = xmlGetProp (child_node, "local");

        if (deleted == NULL && (!local_only || local != NULL))
        {
            if (local != NULL)
            {
                property_browser->details->has_local = TRUE;
            }

            color = xmlNodeGetContent (child_node);
            name = eel_xml_get_property_translated (child_node, "name");

            /* make the image from the color spec */
            pixbuf = make_color_drag_image (property_browser, color, FALSE);

            /* make the tile from the pixmap and name */
            new_property = labeled_image_new (name, pixbuf, color, PANGO_SCALE_LARGE);

            gtk_container_add (GTK_CONTAINER (property_browser->details->content_table), new_property);
            gtk_widget_show (new_property);

            g_object_unref (pixbuf);
            xmlFree (color);
            xmlFree (name);
        }

        xmlFree (local);
        xmlFree (deleted);
    }
}

/* make_category generates widgets corresponding all of the objects in the passed in directory */
static void
make_category(CajaPropertyBrowser *property_browser, const char* path, const char* mode, xmlNodePtr node, const char *description)
{

    /* set up the description in the help label */
    gtk_label_set_text (GTK_LABEL (property_browser->details->help_label), description);

    /* case out on the mode */
    if (strcmp (mode, "directory") == 0)
        make_properties_from_directories (property_browser);
    else if (strcmp (mode, "inline") == 0)
        make_properties_from_xml_node (property_browser, node);

}

/* Create a category button */
static GtkWidget *
property_browser_category_button_new (const char *display_name,
                                      const char *image)
{
    GtkWidget *button;
    char *file_name;

    g_return_val_if_fail (display_name != NULL, NULL);
    g_return_val_if_fail (image != NULL, NULL);

    file_name = caja_pixmap_file (image);
    if (file_name != NULL)
    {
        button = eel_labeled_image_radio_button_new_from_file_name (display_name, file_name);
    }
    else
    {
        button = eel_labeled_image_radio_button_new (display_name, NULL);
    }

    gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);

    /* We also want all of the buttons to be the same height */
    eel_labeled_image_set_fixed_image_height (EEL_LABELED_IMAGE (gtk_bin_get_child (GTK_BIN (button))), STANDARD_BUTTON_IMAGE_HEIGHT);

    g_free (file_name);

    return button;
}

/* this is a utility routine to generate a category link widget and install it in the browser */
static void
make_category_link (CajaPropertyBrowser *property_browser,
                    const char *name,
                    const char *display_name,
                    const char *image,
                    GtkRadioButton **group)
{
    GtkWidget *button;

    g_return_if_fail (name != NULL);
    g_return_if_fail (image != NULL);
    g_return_if_fail (display_name != NULL);
    g_return_if_fail (CAJA_IS_PROPERTY_BROWSER (property_browser));

    button = property_browser_category_button_new (display_name, image);
    gtk_widget_show (button);

    if (*group)
    {
        gtk_radio_button_set_group (GTK_RADIO_BUTTON (button),
                                    gtk_radio_button_get_group (*group));
    }
    else
    {
        *group = GTK_RADIO_BUTTON (button);
    }

    /* if the button represents the current category, highlight it */
    if (property_browser->details->category &&
            strcmp (property_browser->details->category, name) == 0)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

    /* Place it in the category box */
    gtk_box_pack_start (GTK_BOX (property_browser->details->category_box),
                        button, FALSE, FALSE, 0);

    property_browser->details->category_position += 1;

    /* add a signal to handle clicks */
    g_object_set_data (G_OBJECT(button), "user_data", property_browser);
    g_signal_connect_data
    (button, "toggled",
     G_CALLBACK (category_toggled_callback),
     g_strdup (name), (GClosureNotify) g_free, 0);
}

/* update_contents populates the property browser with information specified by the path and other state variables */
void
caja_property_browser_update_contents (CajaPropertyBrowser *property_browser)
{
    xmlNodePtr cur_node;
    xmlDocPtr document;
    GtkWidget *viewport;
    GtkRadioButton *group;
    gboolean got_categories;
    char *name, *image, *type, *description, *display_name, *path, *mode;
    const char *text;

    /* load the xml document corresponding to the path and selection */
    document = read_browser_xml (property_browser);
    if (document == NULL)
    {
        return;
    }

    /* remove the existing content box, if any, and allocate a new one */
    if (property_browser->details->content_frame)
    {
        gtk_widget_destroy(property_browser->details->content_frame);
    }

    /* allocate a new container, with a scrollwindow and viewport */
    property_browser->details->content_frame = gtk_scrolled_window_new (NULL, NULL);
    viewport = gtk_viewport_new (NULL, NULL);
    gtk_widget_show(viewport);
    gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewport), GTK_SHADOW_IN);
    gtk_container_add (GTK_CONTAINER (property_browser->details->content_container), property_browser->details->content_frame);
    gtk_widget_show (property_browser->details->content_frame);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (property_browser->details->content_frame),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    /* allocate a table to hold the content widgets */
    property_browser->details->content_table = eel_image_table_new (TRUE);
    eel_wrap_table_set_x_spacing (EEL_WRAP_TABLE (property_browser->details->content_table),
                                  IMAGE_TABLE_X_SPACING);
    eel_wrap_table_set_y_spacing (EEL_WRAP_TABLE (property_browser->details->content_table),
                                  IMAGE_TABLE_Y_SPACING);

    g_signal_connect_object (property_browser->details->content_table, "child_pressed",
                             G_CALLBACK (element_clicked_callback), property_browser, 0);

    gtk_container_add(GTK_CONTAINER(viewport), property_browser->details->content_table);
    gtk_container_add (GTK_CONTAINER (property_browser->details->content_frame), viewport);
    gtk_widget_show (GTK_WIDGET (property_browser->details->content_table));

    /* iterate through the xml file to generate the widgets */
    got_categories = property_browser->details->category_position >= 0;
    if (!got_categories)
    {
        property_browser->details->category_position = 0;
    }

    group = NULL;
    for (cur_node = eel_xml_get_children (xmlDocGetRootElement (document));
            cur_node != NULL;
            cur_node = cur_node->next)
    {

        if (cur_node->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp (cur_node->name, "category") == 0)
        {
            name = xmlGetProp (cur_node, "name");

            if (property_browser->details->category != NULL
                    && strcmp (property_browser->details->category, name) == 0)
            {
                path = xmlGetProp (cur_node, "path");
                mode = xmlGetProp (cur_node, "mode");
                description = eel_xml_get_property_translated (cur_node, "description");
                type = xmlGetProp (cur_node, "type");

                make_category (property_browser,
                               path,
                               mode,
                               cur_node,
                               description);
                caja_property_browser_set_drag_type (property_browser, type);

                xmlFree (path);
                xmlFree (mode);
                xmlFree (description);
                xmlFree (type);
            }

            if (!got_categories)
            {
                display_name = eel_xml_get_property_translated (cur_node, "display_name");
                image = xmlGetProp (cur_node, "image");

                make_category_link (property_browser,
                                    name,
                                    display_name,
                                    image,
                                    &group);

                xmlFree (display_name);
                xmlFree (image);
            }

            xmlFree (name);
        }
    }

    /* release the  xml document and we're done */
    xmlFreeDoc (document);

    /* update the title and button */

    if (property_browser->details->category == NULL)
    {
        gtk_label_set_text (GTK_LABEL (property_browser->details->title_label), _("Select a Category:"));
        gtk_widget_hide(property_browser->details->add_button);
        gtk_widget_hide(property_browser->details->remove_button);

    }
    else
    {
        char *label_text;
        char *stock_id;
        if (property_browser->details->remove_mode)
        {
            stock_id = GTK_STOCK_CANCEL;
            text = _("C_ancel Remove");
        }
        else
        {
            stock_id = GTK_STOCK_ADD;
            /* FIXME: Using spaces to add padding is not good design. */
            switch (property_browser->details->category_type)
            {
            case CAJA_PROPERTY_PATTERN:
                text = _("_Add a New Pattern...");
                break;
            case CAJA_PROPERTY_COLOR:
                text = _("_Add a New Color...");
                break;
            case CAJA_PROPERTY_EMBLEM:
                text = _("_Add a New Emblem...");
                break;
            default:
                text = NULL;
                break;
            }
        }

        /* enable the "add new" button and update it's name and icon */
        gtk_image_set_from_stock (GTK_IMAGE(property_browser->details->add_button_image), stock_id,
                                  GTK_ICON_SIZE_BUTTON);

        if (text != NULL)
        {
            gtk_button_set_label (GTK_BUTTON (property_browser->details->add_button), text);

        }
        gtk_widget_show (property_browser->details->add_button);


        if (property_browser->details->remove_mode)
        {

            switch (property_browser->details->category_type)
            {
            case CAJA_PROPERTY_PATTERN:
                label_text = g_strdup (_("Click on a pattern to remove it"));
                break;
            case CAJA_PROPERTY_COLOR:
                label_text = g_strdup (_("Click on a color to remove it"));
                break;
            case CAJA_PROPERTY_EMBLEM:
                label_text = g_strdup (_("Click on an emblem to remove it"));
                break;
            default:
                label_text = NULL;
                break;
            }
        }
        else
        {
            switch (property_browser->details->category_type)
            {
            case CAJA_PROPERTY_PATTERN:
                label_text = g_strdup (_("Patterns:"));
                break;
            case CAJA_PROPERTY_COLOR:
                label_text = g_strdup (_("Colors:"));
                break;
            case CAJA_PROPERTY_EMBLEM:
                label_text = g_strdup (_("Emblems:"));
                break;
            default:
                label_text = NULL;
                break;
            }
        }

        if (label_text)
        {
            gtk_label_set_text_with_mnemonic
            (GTK_LABEL (property_browser->details->title_label), label_text);
        }
        g_free(label_text);

        /* enable the remove button (if necessary) and update its name */

        /* case out instead of substituting to provide flexibilty for other languages */
        /* FIXME: Using spaces to add padding is not good design. */
        switch (property_browser->details->category_type)
        {
        case CAJA_PROPERTY_PATTERN:
            text = _("_Remove a Pattern...");
            break;
        case CAJA_PROPERTY_COLOR:
            text = _("_Remove a Color...");
            break;
        case CAJA_PROPERTY_EMBLEM:
            text = _("_Remove an Emblem...");
            break;
        default:
            text = NULL;
            break;
        }

        if (property_browser->details->remove_mode
                || !property_browser->details->has_local)
            gtk_widget_hide(property_browser->details->remove_button);
        else
            gtk_widget_show(property_browser->details->remove_button);
        if (text != NULL)
        {
            gtk_button_set_label (GTK_BUTTON (property_browser->details->remove_button), text);
        }
    }
}

/* set the category and regenerate contents as necessary */

static void
caja_property_browser_set_category (CajaPropertyBrowser *property_browser,
                                    const char *new_category)
{
    /* there's nothing to do if the category is the same as the current one */
    if (g_strcmp0 (property_browser->details->category, new_category) == 0)
    {
        return;
    }

    g_free (property_browser->details->category);
    property_browser->details->category = g_strdup (new_category);

    /* set up the property type enum */
    if (g_strcmp0 (new_category, "patterns") == 0)
    {
        property_browser->details->category_type = CAJA_PROPERTY_PATTERN;
    }
    else if (g_strcmp0 (new_category, "colors") == 0)
    {
        property_browser->details->category_type = CAJA_PROPERTY_COLOR;
    }
    else if (g_strcmp0 (new_category, "emblems") == 0)
    {
        property_browser->details->category_type = CAJA_PROPERTY_EMBLEM;
    }
    else
    {
        property_browser->details->category_type = CAJA_PROPERTY_NONE;
    }

    /* populate the per-uri box with the info */
    caja_property_browser_update_contents (property_browser);
}


/* here is the routine that populates the property browser with the appropriate information
   when the path changes */

void
caja_property_browser_set_path (CajaPropertyBrowser *property_browser,
                                const char *new_path)
{
    /* there's nothing to do if the uri is the same as the current one */
    if (g_strcmp0 (property_browser->details->path, new_path) == 0)
    {
        return;
    }

    g_free (property_browser->details->path);
    property_browser->details->path = g_strdup (new_path);

    /* populate the per-uri box with the info */
    caja_property_browser_update_contents (property_browser);
}

static void
emblems_changed_callback (GObject *signaller,
                          CajaPropertyBrowser *property_browser)
{
    caja_property_browser_update_contents (property_browser);
}


static void
emit_emblems_changed_signal (void)
{
    g_signal_emit_by_name (caja_signaller_get_current (), "emblems_changed");
}
