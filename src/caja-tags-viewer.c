/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Caja
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Tags property page for the file Properties dialog. Reads and writes the
 *  "user.xdg.tags" extended attribute (GIO name "xattr::xdg.tags"), which is
 *  the same attribute the caja tag search reads (caja-search-engine-simple.c).
 */

#include <config.h>

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include <libcaja-private/caja-module.h>
#include <libcaja-extension/caja-property-page-provider.h>
#include <libcaja-extension/caja-property-page.h>
#include <libcaja-extension/caja-file-info.h>

#include "caja-tags-viewer.h"

#define XATTR_XDG_TAGS "xattr::xdg.tags"

/* ------------------------------------------------------------------ */
/* Instance structures                                                 */
/* ------------------------------------------------------------------ */

typedef struct _CajaTagsViewerDetails CajaTagsViewerDetails;

typedef struct _CajaTagsViewerClass CajaTagsViewerClass;

struct _CajaTagsViewerDetails
{
    CajaFileInfo *file;
    GFile *location;
    GCancellable *cancellable;

    GtkWidget *flowbox;       /* GtkFlowBox holding the tag chips */
    GtkWidget *entry;         /* GtkEntry for typing a new tag */
    GtkWidget *add_button;
    GtkWidget *helper_label;

    GList *tags;              /* current tags (normalized gchar *), source of truth */

    gboolean writable;
    gboolean loading;         /* TRUE while populating from disk */
};

struct _CajaTagsViewer
{
    GtkBox parent_instance;
    CajaTagsViewerDetails *details;
};

struct _CajaTagsViewerClass
{
    GtkBoxClass parent_class;
};

G_DEFINE_TYPE (CajaTagsViewer, caja_tags_viewer, GTK_TYPE_BOX)

/* The provider: implements CajaPropertyPageProvider. Its registered GObject
 * type name is "CajaTagsViewerProvider", which fm-properties-window.c
 * whitelists for built-in module property pages. */
typedef struct _CajaTagsViewerProvider CajaTagsViewerProvider;
struct _CajaTagsViewerProvider
{
    GObject parent;
};

typedef struct
{
    GObjectClass parent_class;
} CajaTagsViewerProviderClass;

static GType caja_tags_viewer_provider_get_type (void);
static void property_page_provider_iface_init (CajaPropertyPageProviderIface *iface);

G_DEFINE_TYPE_WITH_CODE (CajaTagsViewerProvider, caja_tags_viewer_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CAJA_TYPE_PROPERTY_PAGE_PROVIDER,
                                 property_page_provider_iface_init));

/* Forward declarations */
static void caja_tags_viewer_save (CajaTagsViewer *self);
static void caja_tags_viewer_load (CajaTagsViewer *self);

/* ------------------------------------------------------------------ */
/* Tag helpers                                                         */
/* ------------------------------------------------------------------ */

/* Normalize a tag the same way caja-query.c:caja_query_add_tag does, so that
 * what the user types matches what the search engine normalizes to. */
static gchar *
normalize_tag (const gchar *raw)
{
    gchar *trimmed;
    gchar *normalized;
    gchar *lower;

    trimmed = g_strdup (raw);
    g_strstrip (trimmed);

    if (*trimmed == '\0')
    {
        g_free (trimmed);
        return NULL;
    }

    normalized = g_utf8_normalize (trimmed, -1, G_NORMALIZE_NFD);
    g_free (trimmed);

    lower = g_utf8_strdown (normalized, -1);
    g_free (normalized);

    return lower;
}

/* Case-insensitive membership check against the normalized list. */
static gboolean
tags_contains (GList *tags, const gchar *normalized_tag)
{
    GList *l;

    for (l = tags; l != NULL; l = l->next)
    {
        if (g_strcmp0 ((const gchar *) l->data, normalized_tag) == 0)
        {
            return TRUE;
        }
    }
    return FALSE;
}

static void
tags_free (GList *tags)
{
    g_list_free_full (tags, g_free);
}

/* ------------------------------------------------------------------ */
/* Chips UI                                                            */
/* ------------------------------------------------------------------ */

static void rebuild_chips (CajaTagsViewer *self);

static void
on_chip_remove_clicked (GtkButton *button, gpointer user_data)
{
    CajaTagsViewer *self = CAJA_TAGS_VIEWER (user_data);
    CajaTagsViewerDetails *details = self->details;
    GtkWidget *chip_row;
    const gchar *tag;

    /* The close button's parent is the horizontal box inside the FlowBoxChild */
    chip_row = gtk_widget_get_parent (GTK_WIDGET (button));
    tag = (const gchar *) g_object_get_data (G_OBJECT (chip_row), "tag-text");

    if (tag != NULL)
    {
        GList *l;
        for (l = details->tags; l != NULL; l = l->next)
        {
            if (g_strcmp0 ((const gchar *) l->data, tag) == 0)
            {
                g_free (l->data);
                details->tags = g_list_delete_link (details->tags, l);
                break;
            }
        }

        rebuild_chips (self);

        if (!details->loading)
        {
            caja_tags_viewer_save (self);
        }
    }
}

static GtkWidget *
create_chip (CajaTagsViewer *self, const gchar *tag)
{
    GtkWidget *child;
    GtkWidget *row;
    GtkWidget *label;
    GtkWidget *close_btn;
    GtkStyleContext *ctx;
    gchar *markup;

    child = gtk_flow_box_child_new ();

    row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);

    label = gtk_label_new (tag);
    gtk_label_set_max_width_chars (GTK_LABEL (label), 30);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);

    close_btn = gtk_button_new_from_icon_name ("window-close-symbolic",
                                                GTK_ICON_SIZE_MENU);
    gtk_button_set_relief (GTK_BUTTON (close_btn), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text (close_btn, _("Remove tag"));

    g_object_set_data_full (G_OBJECT (row), "tag-text", g_strdup (tag), g_free);

    gtk_box_pack_start (GTK_BOX (row), label, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (row), close_btn, FALSE, FALSE, 0);

    g_signal_connect (close_btn, "clicked",
                      G_CALLBACK (on_chip_remove_clicked), self);

    /* Style the chip as a small pill */
    ctx = gtk_widget_get_style_context (row);
    gtk_style_context_add_class (ctx, "caja-tag-chip");

    markup = g_strdup_printf ("<small>%s</small>", tag);
    gtk_label_set_markup (GTK_LABEL (label), markup);
    g_free (markup);

    if (!self->details->writable)
    {
        gtk_widget_set_sensitive (close_btn, FALSE);
    }

    gtk_container_add (GTK_CONTAINER (child), row);
    gtk_widget_show_all (child);

    return child;
}

static void
rebuild_chips (CajaTagsViewer *self)
{
    CajaTagsViewerDetails *details = self->details;
    GList *l;

    /* Remove all existing children */
    gtk_container_foreach (GTK_CONTAINER (details->flowbox),
                           (GtkCallback) gtk_widget_destroy, NULL);

    for (l = details->tags; l != NULL; l = l->next)
    {
        GtkWidget *chip = create_chip (self, (const gchar *) l->data);
        gtk_container_add (GTK_CONTAINER (details->flowbox), chip);
    }

    gtk_widget_set_visible (details->flowbox, details->tags != NULL);
}

/* ------------------------------------------------------------------ */
/* Add / remove from entry                                             */
/* ------------------------------------------------------------------ */

static void
add_tags_from_entry (CajaTagsViewer *self)
{
    CajaTagsViewerDetails *details = self->details;
    const gchar *text;
    gchar **tokens;
    guint i;
    gboolean added = FALSE;

    if (!details->writable)
    {
        return;
    }

    text = gtk_entry_get_text (GTK_ENTRY (details->entry));

    /* Allow comma or space separated input */
    tokens = g_strsplit_set (text, ", ", -1);

    for (i = 0; tokens[i] != NULL; i++)
    {
        gchar *norm;

        norm = normalize_tag (tokens[i]);
        if (norm == NULL)
        {
            continue;
        }

        if (!tags_contains (details->tags, norm))
        {
            details->tags = g_list_append (details->tags, norm);
            added = TRUE;
        }
        else
        {
            g_free (norm);
        }
    }

    g_strfreev (tokens);

    if (added)
    {
        gtk_entry_set_text (GTK_ENTRY (details->entry), "");
        rebuild_chips (self);
        caja_tags_viewer_save (self);
    }
}

static void
on_add_clicked (GtkButton *button, gpointer user_data)
{
    add_tags_from_entry (CAJA_TAGS_VIEWER (user_data));
}

static void
on_entry_activate (GtkEntry *entry, gpointer user_data)
{
    add_tags_from_entry (CAJA_TAGS_VIEWER (user_data));
}

static void
on_entry_changed (GtkEditable *editable, gpointer user_data)
{
    CajaTagsViewer *self = CAJA_TAGS_VIEWER (user_data);
    const gchar *text = gtk_entry_get_text (GTK_ENTRY (self->details->entry));
    gboolean has_text = text != NULL && *text != '\0';
    gtk_widget_set_sensitive (self->details->add_button,
                              has_text && self->details->writable);
}

static gboolean
on_entry_focus_out (GtkWidget *widget,
                     GdkEventFocus *event,
                     gpointer user_data)
{
    CajaTagsViewer *self = CAJA_TAGS_VIEWER (user_data);
    const gchar *text = gtk_entry_get_text (GTK_ENTRY (self->details->entry));

    /* If the user typed something but didn't press Enter/Add, commit it. */
    if (text != NULL && *text != '\0')
    {
        add_tags_from_entry (self);
    }
    return FALSE;
}

/* ------------------------------------------------------------------ */
/* Save (GIO xattr::xdg.tags write)                                   */
/* ------------------------------------------------------------------ */

static void
save_callback (GObject *source_object,
               GAsyncResult *res,
               gpointer user_data)
{
    CajaTagsViewer *self;
    GError *error = NULL;

    self = CAJA_TAGS_VIEWER (user_data);

    if (!g_file_set_attributes_finish (G_FILE (source_object), res, NULL, &error))
    {
        CajaTagsViewerDetails *details = self->details;

        if (!g_cancellable_is_cancelled (details->cancellable))
        {
            gchar *msg;

            msg = g_strdup_printf (_("Could not save tags: %s"),
                                   error->message ? error->message :
                                   _("filesystem does not support extended attributes"));
            gtk_label_set_text (GTK_LABEL (details->helper_label), msg);
            g_free (msg);
        }
        g_error_free (error);
    }

    g_object_unref (self);
}

static void
caja_tags_viewer_save (CajaTagsViewer *self)
{
    CajaTagsViewerDetails *details;
    GFileInfo *info;
    guint n;
    guint i;
    GList *l;
    gchar **tokens;
    gchar *joined;

    g_return_if_fail (CAJA_IS_TAGS_VIEWER (self));

    details = self->details;

    if (details->location == NULL || details->cancellable == NULL)
    {
        return;
    }

    info = g_file_info_new ();

    n = g_list_length (details->tags);

    if (n == 0)
    {
        /* Unset the attribute entirely */
        g_file_info_set_attribute (info, XATTR_XDG_TAGS,
                                   G_FILE_ATTRIBUTE_TYPE_INVALID, NULL);
    }
    else
    {
        tokens = g_new0 (gchar *, n + 1);
        for (l = details->tags, i = 0; l != NULL; l = l->next, i++)
        {
            tokens[i] = (gchar *) l->data;
        }
        joined = g_strjoinv (",", tokens);
        g_file_info_set_attribute_string (info, XATTR_XDG_TAGS, joined);
        g_free (joined);
        g_free (tokens);
    }

    g_file_set_attributes_async (details->location,
                                 info,
                                 G_FILE_QUERY_INFO_NONE,
                                 G_PRIORITY_DEFAULT,
                                 details->cancellable,
                                 save_callback,
                                 g_object_ref (self));

    g_object_unref (info);
}

/* ------------------------------------------------------------------ */
/* Load (GIO xattr::xdg.tags read)                                    */
/* ------------------------------------------------------------------ */

static void
load_callback (GObject *source_object,
               GAsyncResult *res,
               gpointer user_data)
{
    CajaTagsViewer *self = CAJA_TAGS_VIEWER (user_data);
    CajaTagsViewerDetails *details;
    GFileInfo *info;
    GError *error = NULL;

    details = self->details;

    info = g_file_query_info_finish (G_FILE (source_object), res, &error);

    if (error != NULL)
    {
        g_error_free (error);
        g_object_unref (self);
        return;
    }

    details->loading = TRUE;

    if (info != NULL && g_file_info_has_attribute (info, XATTR_XDG_TAGS))
    {
        const gchar *raw;
        gchar **tokens;
        guint i;

        raw = g_file_info_get_attribute_string (info, XATTR_XDG_TAGS);

        if (raw != NULL && *raw != '\0')
        {
            tokens = g_strsplit (raw, ",", -1);

            for (i = 0; tokens[i] != NULL; i++)
            {
                gchar *norm;

                norm = normalize_tag (tokens[i]);
                if (norm == NULL)
                {
                    continue;
                }

                if (!tags_contains (details->tags, norm))
                {
                    details->tags = g_list_append (details->tags, norm);
                }
                else
                {
                    g_free (norm);
                }
            }

            g_strfreev (tokens);
        }
    }

    rebuild_chips (self);

    details->loading = FALSE;

    if (info != NULL)
    {
        g_object_unref (info);
    }

    g_object_unref (self);
}

static void
caja_tags_viewer_load (CajaTagsViewer *self)
{
    CajaTagsViewerDetails *details;

    g_return_if_fail (CAJA_IS_TAGS_VIEWER (self));
    details = self->details;

    if (details->location == NULL)
    {
        return;
    }

    /* Cancel any previous in-flight load */
    g_cancellable_cancel (details->cancellable);
    g_object_unref (details->cancellable);
    details->cancellable = g_cancellable_new ();

    g_file_query_info_async (details->location,
                             XATTR_XDG_TAGS,
                             G_FILE_QUERY_INFO_NONE,
                             G_PRIORITY_DEFAULT,
                             details->cancellable,
                             load_callback,
                             g_object_ref (self));
}

/* ------------------------------------------------------------------ */
/* GObject / GtkWidget                                                 */
/* ------------------------------------------------------------------ */

static void
caja_tags_viewer_finalize (GObject *object)
{
    CajaTagsViewer *self = CAJA_TAGS_VIEWER (object);
    CajaTagsViewerDetails *details = self->details;

    if (details->cancellable != NULL)
    {
        g_cancellable_cancel (details->cancellable);
        g_object_unref (details->cancellable);
    }
    if (details->location != NULL)
    {
        g_object_unref (details->location);
    }
    if (details->file != NULL)
    {
        g_object_unref (details->file);
    }
    tags_free (details->tags);
    g_free (details);

    G_OBJECT_CLASS (caja_tags_viewer_parent_class)->finalize (object);
}

static void
caja_tags_viewer_class_init (CajaTagsViewerClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = caja_tags_viewer_finalize;
}

static void
caja_tags_viewer_init (CajaTagsViewer *self)
{
    CajaTagsViewerDetails *details;
    GtkWidget *content_box;
    GtkWidget *input_box;

    details = g_new0 (CajaTagsViewerDetails, 1);
    self->details = details;

    details->tags = NULL;
    details->writable = TRUE;
    details->loading = FALSE;
    details->cancellable = g_cancellable_new ();

    /* The main page is a vertical box with some padding */
    content_box = GTK_WIDGET (self);
    gtk_orientable_set_orientation (GTK_ORIENTABLE (content_box),
                                    GTK_ORIENTATION_VERTICAL);
    gtk_box_set_spacing (GTK_BOX (content_box), 8);
    g_object_set (content_box, "margin", 12, NULL);

    /* Chips container */
    details->flowbox = gtk_flow_box_new ();
    gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (details->flowbox),
                                     GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (details->flowbox), 1);
    gtk_widget_set_halign (details->flowbox, GTK_ALIGN_START);
    gtk_widget_set_valign (details->flowbox, GTK_ALIGN_START);
    gtk_box_pack_start (GTK_BOX (content_box), details->flowbox, TRUE, TRUE, 0);

    /* Input row: entry + Add button */
    input_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

    details->entry = gtk_entry_new ();
    gtk_entry_set_placeholder_text (GTK_ENTRY (details->entry),
                                    _("Type a tag and press Enter"));
    gtk_widget_set_hexpand (details->entry, TRUE);

    details->add_button = gtk_button_new_with_label (_("Add"));
    gtk_widget_set_sensitive (details->add_button, FALSE);

    gtk_box_pack_start (GTK_BOX (input_box), details->entry, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (input_box), details->add_button, FALSE, FALSE, 0);

    gtk_box_pack_start (GTK_BOX (content_box), input_box, FALSE, FALSE, 0);

    /* Helper label */
    details->helper_label = gtk_label_new (_("Tags are stored in the file's "
                                             "extended attributes (user.xdg.tags) "
                                             "and are searchable via the tag "
                                             "search row."));
    gtk_label_set_line_wrap (GTK_LABEL (details->helper_label), TRUE);
    gtk_label_set_max_width_chars (GTK_LABEL (details->helper_label), 50);
    gtk_widget_set_halign (details->helper_label, GTK_ALIGN_START);
    GtkStyleContext *hctx = gtk_widget_get_style_context (details->helper_label);
    gtk_style_context_add_class (hctx, "dim-label");

    gtk_box_pack_start (GTK_BOX (content_box), details->helper_label,
                        FALSE, FALSE, 0);

    /* Signals */
    g_signal_connect (details->add_button, "clicked",
                      G_CALLBACK (on_add_clicked), self);
    g_signal_connect (details->entry, "activate",
                      G_CALLBACK (on_entry_activate), self);
    g_signal_connect (details->entry, "changed",
                      G_CALLBACK (on_entry_changed), self);
    g_signal_connect (details->entry, "focus-out-event",
                      G_CALLBACK (on_entry_focus_out), self);

    gtk_widget_show_all (content_box);
}

/* ------------------------------------------------------------------ */
/* Provider                                                           */
/* ------------------------------------------------------------------ */

static GList *
get_property_pages (CajaPropertyPageProvider *provider,
                    GList *files)
{
    GList *pages;
    CajaPropertyPage *page;
    CajaFileInfo *file;
    char *uri;
    char *scheme;
    CajaTagsViewer *viewer;

    /* Single-file selection only (parity with Notes) */
    if (files == NULL || files->next != NULL)
    {
        return NULL;
    }

    file = CAJA_FILE_INFO (files->data);
    uri = caja_file_info_get_uri (file);

    /* Only local files support xattrs */
    scheme = caja_file_info_get_uri_scheme (file);
    if (g_strcmp0 (scheme, "file") != 0)
    {
        g_free (uri);
        g_free (scheme);
        return NULL;
    }
    g_free (scheme);

    viewer = g_object_new (CAJA_TYPE_TAGS_VIEWER, NULL);
    viewer->details->file = g_object_ref (file);
    viewer->details->location = caja_file_info_get_location (file);
    viewer->details->writable = caja_file_info_can_write (file);

    if (!viewer->details->writable)
    {
        gtk_widget_set_sensitive (viewer->details->entry, FALSE);
        gtk_widget_set_sensitive (viewer->details->add_button, FALSE);
        gtk_label_set_text (GTK_LABEL (viewer->details->helper_label),
                            _("This file is read-only; tags cannot be modified."));
    }

    /* Load existing tags asynchronously */
    caja_tags_viewer_load (viewer);

    page = caja_property_page_new ("CajaTagsViewer::property_page",
                                   gtk_label_new (_("Tags")),
                                   GTK_WIDGET (viewer));
    pages = g_list_append (NULL, page);

    g_free (uri);
    return pages;
}

static void
property_page_provider_iface_init (CajaPropertyPageProviderIface *iface)
{
    iface->get_pages = get_property_pages;
}

static void
caja_tags_viewer_provider_init (CajaTagsViewerProvider *provider)
{
}

static void
caja_tags_viewer_provider_class_init (CajaTagsViewerProviderClass *klass)
{
}

void
caja_tags_viewer_register (void)
{
    caja_module_add_type (caja_tags_viewer_provider_get_type ());
}
