/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 1999, 2000, 2001 Eazel, Inc.
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
 *
 */

#include <config.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <eel/eel-background.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>

#include <libcaja-private/caja-dnd.h>
#include <libcaja-private/caja-directory.h>
#include <libcaja-private/caja-file-dnd.h>
#include <libcaja-private/caja-file.h>
#include <libcaja-private/caja-global-preferences.h>
#include <libcaja-private/caja-keep-last-vertical-box.h>
#include <libcaja-private/caja-metadata.h>
#include <libcaja-private/caja-mime-actions.h>
#include <libcaja-private/caja-program-choosing.h>
#include <libcaja-private/caja-sidebar-provider.h>
#include <libcaja-private/caja-module.h>

#include "caja-information-panel.h"
#include "caja-sidebar-title.h"

struct _CajaInformationPanelPrivate
{
    GtkWidget *container;
    CajaWindowInfo *window;
    CajaSidebarTitle *title;
    GtkWidget *button_box_centerer;
    GtkWidget *button_box;
    gboolean has_buttons;
    CajaFile *file;
    guint file_changed_connection;
    gboolean background_connected;

    char *default_background_color;
    char *default_background_image;
    char *current_background_color;
    char *current_background_image;
};

/* button assignments */
#define CONTEXTUAL_MENU_BUTTON 3

static gboolean caja_information_panel_press_event           (GtkWidget                    *widget,
        GdkEventButton               *event);
static void     caja_information_panel_finalize              (GObject                      *object);
static void     caja_information_panel_drag_data_received    (GtkWidget                    *widget,
        GdkDragContext               *context,
        int                           x,
        int                           y,
        GtkSelectionData             *selection_data,
        guint                         info,
        guint                         time);
static void     caja_information_panel_read_defaults         (CajaInformationPanel     *information_panel);
static void     caja_information_panel_style_updated         (GtkWidget                    *widget);
static void     caja_information_panel_theme_changed         (GSettings   *settings,
                                                              const gchar *key,
                                                              gpointer     user_data);
static void     caja_information_panel_update_appearance     (CajaInformationPanel     *information_panel);
static void     caja_information_panel_update_buttons        (CajaInformationPanel     *information_panel);
static void     background_metadata_changed_callback             (CajaInformationPanel     *information_panel);
static void     caja_information_panel_iface_init            (CajaSidebarIface         *iface);
static void     sidebar_provider_iface_init                      (CajaSidebarProviderIface *iface);
static GType    caja_information_panel_provider_get_type     (void);

enum
{
    LOCATION_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/* drag and drop definitions */

enum
{
    TARGET_URI_LIST,
    TARGET_COLOR,
    TARGET_BGIMAGE,
    TARGET_KEYWORD,
    TARGET_BACKGROUND_RESET,
    TARGET_MATE_URI_LIST
};

static const GtkTargetEntry target_table[] =
{
    { "text/uri-list",  0, TARGET_URI_LIST },
    { "application/x-color", 0, TARGET_COLOR },
    { "property/bgimage", 0, TARGET_BGIMAGE },
    { "property/keyword", 0, TARGET_KEYWORD },
    { "x-special/mate-reset-background", 0, TARGET_BACKGROUND_RESET },
    { "x-special/mate-icon-list",  0, TARGET_MATE_URI_LIST }
};

typedef enum
{
    NO_PART,
    BACKGROUND_PART,
    ICON_PART
} InformationPanelPart;

typedef struct
{
    GObject parent;
} CajaInformationPanelProvider;

typedef struct
{
    GObjectClass parent;
} CajaInformationPanelProviderClass;


G_DEFINE_TYPE_WITH_CODE (CajaInformationPanel, caja_information_panel, EEL_TYPE_BACKGROUND_BOX,
                         G_ADD_PRIVATE (CajaInformationPanel)
                         G_IMPLEMENT_INTERFACE (CAJA_TYPE_SIDEBAR,
                                 caja_information_panel_iface_init));

G_DEFINE_TYPE_WITH_CODE (CajaInformationPanelProvider, caja_information_panel_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CAJA_TYPE_SIDEBAR_PROVIDER,
                                 sidebar_provider_iface_init));


static const char *
caja_information_panel_get_sidebar_id (CajaSidebar *sidebar)
{
    return CAJA_INFORMATION_PANEL_ID;
}

static char *
caja_information_panel_get_tab_label (CajaSidebar *sidebar)
{
    return g_strdup (_("Information"));
}

static char *
caja_information_panel_get_tab_tooltip (CajaSidebar *sidebar)
{
    return g_strdup (_("Show Information"));
}

static GdkPixbuf *
caja_information_panel_get_tab_icon (CajaSidebar *sidebar)
{
    return NULL;
}

static void
caja_information_panel_is_visible_changed (CajaSidebar *sidebar,
        gboolean         is_visible)
{
    /* Do nothing */
}

static void
caja_information_panel_iface_init (CajaSidebarIface *iface)
{
    iface->get_sidebar_id = caja_information_panel_get_sidebar_id;
    iface->get_tab_label = caja_information_panel_get_tab_label;
    iface->get_tab_tooltip = caja_information_panel_get_tab_tooltip;
    iface->get_tab_icon = caja_information_panel_get_tab_icon;
    iface->is_visible_changed = caja_information_panel_is_visible_changed;
}

/* initializing the class object by installing the operations we override */
static void
caja_information_panel_class_init (CajaInformationPanelClass *klass)
{
    GtkWidgetClass *widget_class;
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (klass);
    widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->finalize = caja_information_panel_finalize;

    widget_class->drag_data_received  = caja_information_panel_drag_data_received;
    widget_class->button_press_event  = caja_information_panel_press_event;
    widget_class->style_updated = caja_information_panel_style_updated;

    /* add the "location changed" signal */
    signals[LOCATION_CHANGED] = g_signal_new
                                ("location_changed",
                                 G_TYPE_FROM_CLASS (klass),
                                 G_SIGNAL_RUN_LAST,
                                 G_STRUCT_OFFSET (CajaInformationPanelClass,
                                         location_changed),
                                 NULL, NULL,
                                 g_cclosure_marshal_VOID__STRING,
                                 G_TYPE_NONE, 1, G_TYPE_STRING);
}

/* utility routine to allocate the box the holds the command buttons */
static void
make_button_box (CajaInformationPanel *information_panel)
{
    information_panel->details->button_box_centerer = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

    gtk_box_pack_start (GTK_BOX (information_panel->details->container),
                        information_panel->details->button_box_centerer, TRUE, TRUE, 0);

    information_panel->details->button_box = caja_keep_last_vertical_box_new (4);
    gtk_container_set_border_width (GTK_CONTAINER (information_panel->details->button_box), 8);
    gtk_widget_show (information_panel->details->button_box);
    gtk_box_pack_start (GTK_BOX (information_panel->details->button_box_centerer),
                        information_panel->details->button_box,
                        TRUE, TRUE, 0);
    information_panel->details->has_buttons = FALSE;
}

/* initialize the instance's fields, create the necessary subviews, etc. */

static void
caja_information_panel_init (CajaInformationPanel *information_panel)
{
    information_panel->details = caja_information_panel_get_instance_private (information_panel);

    /* load the default background */
    caja_information_panel_read_defaults (information_panel);

    /* enable mouse tracking */
    gtk_widget_add_events (GTK_WIDGET (information_panel), GDK_POINTER_MOTION_MASK);

    /* create the container box */
    information_panel->details->container = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width (GTK_CONTAINER (information_panel->details->container), 0);
    gtk_widget_show (information_panel->details->container);
    gtk_container_add (GTK_CONTAINER (information_panel),
                       information_panel->details->container);

    /* allocate and install the index title widget */
    information_panel->details->title = CAJA_SIDEBAR_TITLE (caja_sidebar_title_new ());
    gtk_widget_show (GTK_WIDGET (information_panel->details->title));
    gtk_box_pack_start (GTK_BOX (information_panel->details->container),
                        GTK_WIDGET (information_panel->details->title),
                        FALSE, FALSE, 8);

    /* allocate and install the command button container */
    make_button_box (information_panel);

    /* add a callback for when the theme changes */
    g_signal_connect (caja_preferences,
              "changed::" CAJA_PREFERENCES_SIDE_PANE_BACKGROUND_SET,
              G_CALLBACK(caja_information_panel_theme_changed),
              information_panel);
    g_signal_connect (caja_preferences,
              "changed::" CAJA_PREFERENCES_SIDE_PANE_BACKGROUND_COLOR,
              G_CALLBACK(caja_information_panel_theme_changed),
              information_panel);
    g_signal_connect (caja_preferences,
              "changed::" CAJA_PREFERENCES_SIDE_PANE_BACKGROUND_URI,
              G_CALLBACK(caja_information_panel_theme_changed),
              information_panel);

    /* prepare ourselves to receive dropped objects */
    gtk_drag_dest_set (GTK_WIDGET (information_panel),
                       GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP,
                       target_table, G_N_ELEMENTS (target_table),
                       GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_ASK);
}

static void
caja_information_panel_finalize (GObject *object)
{
    CajaInformationPanel *information_panel;

    information_panel = CAJA_INFORMATION_PANEL (object);

    if (information_panel->details->file != NULL)
    {
        caja_file_monitor_remove (information_panel->details->file, information_panel);
        caja_file_unref (information_panel->details->file);
    }

    g_free (information_panel->details->default_background_color);
    g_free (information_panel->details->default_background_image);
    g_free (information_panel->details->current_background_color);
    g_free (information_panel->details->current_background_image);

    g_signal_handlers_disconnect_by_func (caja_preferences,
                                          caja_information_panel_theme_changed,
                                          information_panel);

    G_OBJECT_CLASS (caja_information_panel_parent_class)->finalize (object);
}

/* callback to handle resetting the background */
static void
reset_background_callback (GtkWidget *menu_item, GtkWidget *information_panel)
{
    EelBackground *background;

    background = eel_get_widget_background (information_panel);
    if (background != NULL)
    {
        eel_background_reset (background);
    }
}

static gboolean
information_panel_has_background (CajaInformationPanel *information_panel)
{
    EelBackground *background;
    gboolean has_background;
    char *color;
    char *image;

    background = eel_get_widget_background (GTK_WIDGET(information_panel));

    color = eel_background_get_color (background);
    image = eel_background_get_image_uri (background);

    has_background = (color || image);

    return has_background;
}

/* create the context menu */
static GtkWidget *
caja_information_panel_create_context_menu (CajaInformationPanel *information_panel)
{
    GtkWidget *menu, *menu_item;

    menu = gtk_menu_new ();
    gtk_menu_set_screen (GTK_MENU (menu),
                         gtk_widget_get_screen (GTK_WIDGET (information_panel)));

    /* add the reset background item, possibly disabled */
    menu_item = gtk_menu_item_new_with_mnemonic (_("Use _Default Background"));
    gtk_widget_show (menu_item);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
    gtk_widget_set_sensitive (menu_item, information_panel_has_background (information_panel));
    g_signal_connect_object (menu_item, "activate",
                             G_CALLBACK (reset_background_callback), information_panel, 0);

    return menu;
}

/* set up the default backgrounds and images */
static void
caja_information_panel_read_defaults (CajaInformationPanel *information_panel)
{
    gboolean background_set;
    char *background_color, *background_image;

    background_set = g_settings_get_boolean (caja_preferences, CAJA_PREFERENCES_SIDE_PANE_BACKGROUND_SET);

    background_color = NULL;
    background_image = NULL;
    if (background_set)
    {
        background_color = g_settings_get_string (caja_preferences, CAJA_PREFERENCES_SIDE_PANE_BACKGROUND_COLOR);
        background_image = g_settings_get_string (caja_preferences, CAJA_PREFERENCES_SIDE_PANE_BACKGROUND_URI);
    }

    g_free (information_panel->details->default_background_color);
    information_panel->details->default_background_color = NULL;
    g_free (information_panel->details->default_background_image);
    information_panel->details->default_background_image = NULL;

    if (background_color && strlen (background_color))
    {
        information_panel->details->default_background_color = g_strdup (background_color);
    }

    /* set up the default background image */

    if (background_image && strlen (background_image))
    {
        information_panel->details->default_background_image = g_strdup (background_image);
    }

    g_free (background_color);
    g_free (background_image);
}

/* handler for handling theme changes */

static void
caja_information_panel_theme_changed (GSettings   *settings,
                                      const gchar *key,
                                      gpointer user_data)
{
    CajaInformationPanel *information_panel;

    information_panel = CAJA_INFORMATION_PANEL (user_data);
    caja_information_panel_read_defaults (information_panel);
    caja_information_panel_update_appearance (information_panel);
    gtk_widget_queue_draw (GTK_WIDGET (information_panel)) ;
}

/* hit testing */

static InformationPanelPart
hit_test (CajaInformationPanel *information_panel,
          int x, int y)
{
    GtkAllocation *allocation;
    gboolean bg_hit;

    if (caja_sidebar_title_hit_test_icon (information_panel->details->title, x, y))
    {
        return ICON_PART;
    }

    allocation = g_new0 (GtkAllocation, 1);
    gtk_widget_get_allocation (GTK_WIDGET (information_panel), allocation);

    bg_hit = allocation != NULL
             && x >= allocation->x && y >= allocation->y
             && x < allocation->x + allocation->width
             && y < allocation->y + allocation->height;
    g_free (allocation);

    if (bg_hit)
    {
        return BACKGROUND_PART;
    }

    return NO_PART;
}

/* utility to test if a uri refers to a local image */
static gboolean
uri_is_local_image (const char *uri)
{
    GdkPixbuf *pixbuf;
    char *image_path;

    image_path = g_filename_from_uri (uri, NULL, NULL);
    if (image_path == NULL)
    {
        return FALSE;
    }

    pixbuf = gdk_pixbuf_new_from_file (image_path, NULL);
    g_free (image_path);

    if (pixbuf == NULL)
    {
        return FALSE;
    }
    g_object_unref (pixbuf);
    return TRUE;
}

static void
receive_dropped_uri_list (CajaInformationPanel *information_panel,
                          GdkDragAction action,
                          int x, int y,
                          GtkSelectionData *selection_data)
{
    char **uris;
    gboolean exactly_one;
    GtkWindow *window;

    uris = g_uri_list_extract_uris ((gchar *) gtk_selection_data_get_data (selection_data));
    exactly_one = uris[0] != NULL && (uris[1] == NULL || uris[1][0] == '\0');
    window = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (information_panel)));

    switch (hit_test (information_panel, x, y))
    {
    case NO_PART:
    case BACKGROUND_PART:
        /* FIXME bugzilla.gnome.org 42507: Does this work for all images, or only background images?
         * Other views handle background images differently from other URIs.
         */
        if (exactly_one && uri_is_local_image (uris[0]))
        {
            if (action == GDK_ACTION_ASK)
            {
                action = caja_drag_drop_background_ask (GTK_WIDGET (information_panel),
                             CAJA_DND_ACTION_SET_AS_BACKGROUND | CAJA_DND_ACTION_SET_AS_GLOBAL_BACKGROUND);
            }

            if (action > 0)
            {
                EelBackground *background;

                background = eel_get_widget_background (GTK_WIDGET (information_panel));
                eel_background_set_dropped_image (background, action, uris[0]);
            }
        }
        else if (exactly_one)
        {
            g_signal_emit (information_panel, signals[LOCATION_CHANGED], 0, uris[0]);
        }
        break;
    case ICON_PART:
        /* handle images dropped on the logo specially */

        if (!exactly_one)
        {
            eel_show_error_dialog (
                _("You cannot assign more than one custom icon at a time."),
                _("Please drag just one image to set a custom icon."),
                window);
            break;
        }

        if (uri_is_local_image (uris[0]))
        {
            if (information_panel->details->file != NULL)
            {
                caja_file_set_metadata (information_panel->details->file,
                                        CAJA_METADATA_KEY_CUSTOM_ICON,
                                        NULL,
                                        uris[0]);
                caja_file_set_metadata (information_panel->details->file,
                                        CAJA_METADATA_KEY_ICON_SCALE,
                                        NULL,
                                        NULL);
            }
        }
        else
        {
            GFile *f;

            f = g_file_new_for_uri (uris[0]);
            if (!g_file_is_native (f))
            {
                eel_show_error_dialog (
                    _("The file that you dropped is not local."),
                    _("You can only use local images as custom icons."),
                    window);

            }
            else
            {
                eel_show_error_dialog (
                    _("The file that you dropped is not an image."),
                    _("You can only use images as custom icons."),
                    window);
            }
            g_object_unref (f);
        }
        break;
    }

    g_strfreev (uris);
}

static void
receive_dropped_color (CajaInformationPanel *information_panel,
                       GdkDragAction action,
                       int x, int y,
                       GtkSelectionData *selection_data)
{
    guint16 *channels;
    char color_spec[8];

    if (gtk_selection_data_get_length (selection_data) != 8 ||
            gtk_selection_data_get_format (selection_data) != 16)
    {
        g_warning ("received invalid color data");
        return;
    }

    channels = (guint16 *) gtk_selection_data_get_data (selection_data);
    g_snprintf (color_spec, sizeof (color_spec),
                "#%02X%02X%02X", channels[0] >> 8, channels[1] >> 8, channels[2] >> 8);

    switch (hit_test (information_panel, x, y))
    {
    case NO_PART:
        g_warning ("dropped color, but not on any part of information_panel");
        break;
    case ICON_PART:
    case BACKGROUND_PART:
        if (action == GDK_ACTION_ASK)
        {
            action = caja_drag_drop_background_ask (GTK_WIDGET (information_panel),
                         CAJA_DND_ACTION_SET_AS_BACKGROUND | CAJA_DND_ACTION_SET_AS_GLOBAL_BACKGROUND);
        }

        if (action > 0)
        {
            EelBackground *background;

            background = eel_get_widget_background (GTK_WIDGET (information_panel));
            eel_background_set_dropped_color (background, GTK_WIDGET (information_panel),
                                              action, x, y, selection_data);
        }

        break;
    }
}

/* handle receiving a dropped keyword */

static void
receive_dropped_keyword (CajaInformationPanel *information_panel,
                         int x, int y,
                         GtkSelectionData *selection_data)
{
    caja_drag_file_receive_dropped_keyword (information_panel->details->file,
                                            gtk_selection_data_get_data (selection_data));

    /* regenerate the display */
    caja_information_panel_update_appearance (information_panel);
}

static void
caja_information_panel_drag_data_received (GtkWidget *widget, GdkDragContext *context,
        int x, int y,
        GtkSelectionData *selection_data,
        guint info, guint time)
{
    CajaInformationPanel *information_panel;
    EelBackground *background;

    g_return_if_fail (CAJA_IS_INFORMATION_PANEL (widget));

    information_panel = CAJA_INFORMATION_PANEL (widget);

    switch (info)
    {
    case TARGET_MATE_URI_LIST:
    case TARGET_URI_LIST:
        receive_dropped_uri_list (information_panel,
                                  gdk_drag_context_get_selected_action (context), x, y, selection_data);
        break;
    case TARGET_COLOR:
        receive_dropped_color (information_panel,
                               gdk_drag_context_get_selected_action (context), x, y, selection_data);
        break;
    case TARGET_BGIMAGE:
        if (hit_test (information_panel, x, y) == BACKGROUND_PART)
            receive_dropped_uri_list (information_panel,
                                      gdk_drag_context_get_selected_action (context), x, y, selection_data);
        break;
    case TARGET_BACKGROUND_RESET:
        background = eel_get_widget_background ( GTK_WIDGET (information_panel));
        if (background != NULL)
        {
            eel_background_reset (background);
        }
        break;
    case TARGET_KEYWORD:
        receive_dropped_keyword (information_panel, x, y, selection_data);
        break;
    default:
        g_warning ("unknown drop type");
    }
}

/* handle the context menu if necessary */
static gboolean
caja_information_panel_press_event (GtkWidget *widget, GdkEventButton *event)
{
    CajaInformationPanel *information_panel;

    if (gtk_widget_get_window (widget) != event->window)
    {
        return FALSE;
    }

    information_panel = CAJA_INFORMATION_PANEL (widget);

    /* handle the context menu */
    if (event->button == CONTEXTUAL_MENU_BUTTON)
    {
        GtkWidget *menu;

        menu = caja_information_panel_create_context_menu (information_panel);
        eel_pop_up_context_menu (GTK_MENU(menu),
                                 event);
    }
    return TRUE;
}

static GtkWindow *
caja_information_panel_get_window (CajaInformationPanel *information_panel)
{
    GtkWidget *result;

    result = gtk_widget_get_ancestor (GTK_WIDGET (information_panel), GTK_TYPE_WINDOW);

    return result == NULL ? NULL : GTK_WINDOW (result);
}

static void
command_button_callback (GtkWidget *button, GAppInfo *application)
{
    CajaInformationPanel *information_panel;
    GList files;

    information_panel = CAJA_INFORMATION_PANEL (g_object_get_data (G_OBJECT (button), "user_data"));

    files.next = NULL;
    files.prev = NULL;
    files.data = information_panel->details->file;
    caja_launch_application (application, &files,
                             caja_information_panel_get_window (information_panel));
}

/* interpret commands for buttons specified by metadata. Handle some built-in ones explicitly, or fork
   a shell to handle general ones */
/* for now, we don't have any of these */
static void
metadata_button_callback (GtkWidget *button, const char *command_str)
{
    //CajaInformationPanel *self = CAJA_INFORMATION_PANEL (g_object_get_data (G_OBJECT (button), "user_data"));
}

/* utility routine that allocates the command buttons from the command list */

static void
add_command_button (CajaInformationPanel *information_panel, GAppInfo *application)
{
    char *temp_str;
    GtkWidget *temp_button, *label;

    /* There's always at least the "Open with..." button */
    information_panel->details->has_buttons = TRUE;

    temp_str = g_strdup_printf (_("Open With %s"), g_app_info_get_display_name (application));
    temp_button = gtk_button_new_with_label (temp_str);
    label = gtk_bin_get_child (GTK_BIN (temp_button));
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_START);
    g_free (temp_str);
    gtk_box_pack_start (GTK_BOX (information_panel->details->button_box),
                        temp_button,
                        FALSE, FALSE,
                        0);

    g_signal_connect_data (temp_button,
                           "clicked",
                           G_CALLBACK (command_button_callback),
                           g_object_ref (application),
                           (GClosureNotify)g_object_unref,
                           0);

    g_object_set_data (G_OBJECT (temp_button), "user_data", information_panel);

    gtk_widget_show (temp_button);
}

/* utility to construct command buttons for the information_panel from the passed in metadata string */

static void
add_buttons_from_metadata (CajaInformationPanel *information_panel, const char *button_data)
{
    char **terms;
    char *button_name, *command_string;
    const char *term;
    int index;
    GtkWidget *temp_button;

    /* split the button specification into a set of terms */
    button_name = NULL;
    terms = g_strsplit (button_data, ";", 0);

    /* for each term, either create a button or attach a property to one */
    for (index = 0; (term = terms[index]) != NULL; index++)
    {
        char *current_term, *temp_str;

        current_term = g_strdup (term);
        temp_str = strchr (current_term, '=');
        if (temp_str)
        {
            *temp_str = '\0';
            if (!g_ascii_strcasecmp (current_term, "button"))
            {
                if (button_name)
                    g_free (button_name);

                button_name = g_strdup (temp_str + 1);
            }
            else if (!g_ascii_strcasecmp (current_term, "script"))
            {
                if (button_name != NULL)
                {
                    temp_button = gtk_button_new_with_label (button_name);
                    gtk_box_pack_start (GTK_BOX (information_panel->details->button_box),
                                        temp_button,
                                        FALSE, FALSE,
                                        0);
                    information_panel->details->has_buttons = TRUE;
                    command_string = g_strdup (temp_str + 1);

                    g_signal_connect_data (temp_button,
                                           "clicked",
                                           G_CALLBACK (metadata_button_callback),
                                           command_string,
                                           (GClosureNotify)g_free,
                                           0);

                    g_object_set_data (G_OBJECT (temp_button), "user_data", information_panel);

                    gtk_widget_show (temp_button);
                }
            }
        }
        g_free(current_term);
    }
    g_free (button_name);
    g_strfreev (terms);
}

/*
 * caja_information_panel_update_buttons:
 *
 * Update the list of program-launching buttons based on the current uri.
 */
static void
caja_information_panel_update_buttons (CajaInformationPanel *information_panel)
{
    char *button_data;

    /* dispose of any existing buttons */
    if (information_panel->details->has_buttons)
    {
        gtk_container_remove (GTK_CONTAINER (information_panel->details->container),
                              information_panel->details->button_box_centerer);
        make_button_box (information_panel);
    }

    /* create buttons from file metadata if necessary */
    button_data = caja_file_get_metadata (information_panel->details->file,
                                          CAJA_METADATA_KEY_SIDEBAR_BUTTONS,
                                          NULL);
    if (button_data)
    {
        add_buttons_from_metadata (information_panel, button_data);
        g_free(button_data);
    }

    /* Make a button for the default application */
    if (caja_mime_has_any_applications_for_file (information_panel->details->file) &&
            !caja_file_is_directory (information_panel->details->file))
    {
        GAppInfo *default_app;

        default_app =
            caja_mime_get_default_application_for_file (information_panel->details->file);
        add_command_button (information_panel, default_app);
        g_object_unref (default_app);
    }

    gtk_widget_show (information_panel->details->button_box_centerer);
}

static void
caja_information_panel_update_appearance (CajaInformationPanel *information_panel)
{
    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (information_panel)),
                                 GTK_STYLE_CLASS_VIEW);
}

static void
background_metadata_changed_callback (CajaInformationPanel *information_panel)
{
    CajaFileAttributes attributes;
    gboolean ready;

    attributes = caja_mime_actions_get_required_file_attributes ();
    ready = caja_file_check_if_ready (information_panel->details->file, attributes);

    if (ready)
    {
        caja_information_panel_update_appearance (information_panel);

        /* set up the command buttons */
        caja_information_panel_update_buttons (information_panel);
    }
}

/* here is the key routine that populates the information_panel with the appropriate information when the uri changes */

static void
caja_information_panel_set_uri (CajaInformationPanel *information_panel,
                                const char* new_uri,
                                const char* initial_title)
{
    CajaFile *file;
    CajaFileAttributes attributes;

    g_return_if_fail (CAJA_IS_INFORMATION_PANEL (information_panel));
    g_return_if_fail (new_uri != NULL);
    g_return_if_fail (initial_title != NULL);

    /* there's nothing to do if the uri is the same as the current one */
    if (information_panel->details->file != NULL &&
            caja_file_matches_uri (information_panel->details->file, new_uri))
    {
        return;
    }

    if (information_panel->details->file != NULL)
    {
        g_signal_handler_disconnect (information_panel->details->file,
                                     information_panel->details->file_changed_connection);
        caja_file_monitor_remove (information_panel->details->file, information_panel);
    }

    file = caja_file_get_by_uri (new_uri);

    caja_file_unref (information_panel->details->file);
    information_panel->details->file = file;

    information_panel->details->file_changed_connection =
        g_signal_connect_object (information_panel->details->file, "changed",
                                 G_CALLBACK (background_metadata_changed_callback),
                                 information_panel, G_CONNECT_SWAPPED);

    attributes = caja_mime_actions_get_required_file_attributes ();
    caja_file_monitor_add (information_panel->details->file, information_panel, attributes);

    background_metadata_changed_callback (information_panel);

    /* tell the title widget about it */
    caja_sidebar_title_set_file (information_panel->details->title,
                                 information_panel->details->file,
                                 initial_title);
}

static void
title_changed_callback (CajaWindowInfo *window,
                        char               *new_title,
                        CajaInformationPanel *panel)
{
    caja_sidebar_title_set_text (panel->details->title,
                                 new_title);
}

/* ::style_set handler for the information_panel */
static void
caja_information_panel_style_updated (GtkWidget *widget)
{
    CajaInformationPanel *information_panel;

    information_panel = CAJA_INFORMATION_PANEL (widget);

    caja_information_panel_theme_changed (NULL, NULL, information_panel);
}

static void
loading_uri_callback (CajaWindowInfo *window,
                      char               *uri,
                      CajaInformationPanel *panel)
{
    CajaWindowSlotInfo *slot;
    char *title;

    slot = caja_window_info_get_active_slot (window);

    title = caja_window_slot_info_get_title (slot);
    caja_information_panel_set_uri (panel,
                                    uri,
                                    title);
    g_free (title);
}

static void
selection_changed_callback (CajaWindowInfo *window,
                            CajaInformationPanel *panel)
{
    int selection_count;
    GList *selection;
    char *uri, *name;

    selection = caja_window_info_get_selection (window);
    selection_count = g_list_length (selection);

    if (selection_count == 1)
    {
        GFile *selected;
        CajaFile *file;

        selection = caja_window_info_get_selection (window);
        selected = selection->data;

        /* this should never fail here, as we're displaying the file */
        file = caja_file_get_existing (selected);
        uri = caja_file_get_uri (file);
        name = caja_file_get_display_name (file);

        caja_file_unref (file);
    }
    else
    {
        uri = caja_window_info_get_current_location (window);
        name = caja_window_info_get_title (window);
    }

    caja_information_panel_set_uri (panel, uri, name);

    g_list_free_full (selection, g_object_unref);
    g_free (uri);
    g_free (name);
}

static void
caja_information_panel_set_parent_window (CajaInformationPanel *panel,
        CajaWindowInfo *window)
{
    gpointer slot;
    char *title, *location;

    panel->details->window = window;

    g_signal_connect_object (window, "loading_uri",
                             G_CALLBACK (loading_uri_callback), panel, 0);
    g_signal_connect_object (window, "title_changed",
                             G_CALLBACK (title_changed_callback), panel, 0);
    g_signal_connect_object (window, "selection-changed",
                             G_CALLBACK (selection_changed_callback), panel, 0);

    slot = caja_window_info_get_active_slot (window);

    title = caja_window_slot_info_get_title (slot);
    location = caja_window_slot_info_get_current_location (slot);
    caja_information_panel_set_uri (panel,
                                    location,
                                    title);
    g_free (location);
    g_free (title);
}

static CajaSidebar *
caja_information_panel_create (CajaSidebarProvider *provider,
                               CajaWindowInfo *window)
{
    CajaInformationPanel *panel;

    panel = g_object_new (caja_information_panel_get_type (), NULL);
    caja_information_panel_set_parent_window (panel, window);
    g_object_ref_sink (panel);

    return CAJA_SIDEBAR (panel);
}

static void
sidebar_provider_iface_init (CajaSidebarProviderIface *iface)
{
    iface->create = caja_information_panel_create;
}

static void
caja_information_panel_provider_init (CajaInformationPanelProvider *sidebar)
{
}

static void
caja_information_panel_provider_class_init (CajaInformationPanelProviderClass *class)
{
}

void
caja_information_panel_register (void)
{
    caja_module_add_type (caja_information_panel_provider_get_type ());
}

