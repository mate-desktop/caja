/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Copyright (C) 2004 Red Hat, Inc
 * Copyright (c) 2007 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 * XMP support by Hubert Figuiere <hfiguiere@novell.com>
 */

#include <config.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#ifdef HAVE_EXIF
#include <libexif/exif-data.h>
#include <libexif/exif-ifd.h>
#include <libexif/exif-loader.h>
#endif /*HAVE_EXIF*/
#ifdef HAVE_EXEMPI
#include <exempi/xmp.h>
#include <exempi/xmpconsts.h>
#endif /*HAVE_EXEMPI*/

#include <eel/eel-vfs-extensions.h>

#include <libcaja-extension/caja-property-page-provider.h>
#include <libcaja-private/caja-module.h>

#include "caja-image-properties-page.h"

#define LOAD_BUFFER_SIZE 8192

struct _CajaImagePropertiesPagePrivate
{
    GCancellable *cancellable;
    GtkWidget *vbox;
    GtkWidget *loading_label;
    GdkPixbufLoader *loader;
    gboolean got_size;
    gboolean pixbuf_still_loading;
    char buffer[LOAD_BUFFER_SIZE];
    int width;
    int height;
#ifdef HAVE_EXIF
    ExifLoader *exifldr;
#endif /*HAVE_EXIF*/
#ifdef HAVE_EXEMPI
    XmpPtr     xmp;
#endif /*HAVE_EXEMPI*/
};

#ifdef HAVE_EXIF
struct ExifAttribute
{
    ExifTag tag;
    char *value;
    gboolean found;
};
#endif /*HAVE_EXIF*/

enum
{
    PROP_URI
};

typedef struct
{
    GObject parent;
} CajaImagePropertiesPageProvider;

typedef struct
{
    GObjectClass parent;
} CajaImagePropertiesPageProviderClass;


static GType caja_image_properties_page_provider_get_type (void);
static void  property_page_provider_iface_init                (CajaPropertyPageProviderIface *iface);

G_DEFINE_TYPE_WITH_PRIVATE (CajaImagePropertiesPage, caja_image_properties_page, GTK_TYPE_BOX);

G_DEFINE_TYPE_WITH_CODE (CajaImagePropertiesPageProvider, caja_image_properties_page_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CAJA_TYPE_PROPERTY_PAGE_PROVIDER,
                                 property_page_provider_iface_init));

static void
caja_image_properties_page_finalize (GObject *object)
{
    CajaImagePropertiesPage *page;

    page = CAJA_IMAGE_PROPERTIES_PAGE (object);

    if (page->details->cancellable)
    {
        g_cancellable_cancel (page->details->cancellable);
        g_object_unref (page->details->cancellable);
        page->details->cancellable = NULL;
    }

    G_OBJECT_CLASS (caja_image_properties_page_parent_class)->finalize (object);
}

static void
file_close_callback (GObject      *object,
                     GAsyncResult *res,
                     gpointer      data)
{
    CajaImagePropertiesPage *page;
    GInputStream *stream;

    page = CAJA_IMAGE_PROPERTIES_PAGE (data);
    stream = G_INPUT_STREAM (object);

    g_input_stream_close_finish (stream, res, NULL);

    g_object_unref (page->details->cancellable);
    page->details->cancellable = NULL;
}

static GtkWidget *
append_label (GtkWidget *vbox,
              const char *str)
{
    GtkWidget *label;

    label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (label), str);
    gtk_label_set_xalign (GTK_LABEL (label), 0);
    gtk_label_set_yalign (GTK_LABEL (label), 0);
    gtk_label_set_selectable (GTK_LABEL (label), TRUE);

    /* setting can_focus to FALSE will allow to make the label
     * selectable but without the cursor showing.
     */
    gtk_widget_set_can_focus (label, FALSE);

    gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);

    return label;
}

static GtkWidget *
append_label_take_str (GtkWidget *vbox,
                       char *str)
{
    GtkWidget *retval;

    retval = append_label (vbox, str);
    g_free (str);

    return retval;
}

#ifdef HAVE_EXIF
static char *
exif_string_to_utf8 (const char *exif_str)
{
    char *utf8_str;

    if (g_utf8_validate (exif_str, -1, NULL))
    {
        return g_strdup (exif_str);
    }

    utf8_str = g_locale_to_utf8 (exif_str, -1, NULL, NULL, NULL);
    if (utf8_str != NULL)
    {
        return utf8_str;
    }

    return eel_make_valid_utf8 (exif_str);
}

static void
exif_content_callback (ExifContent *content, gpointer data)
{
    struct ExifAttribute *attribute;
    char b[1024];

    attribute = (struct ExifAttribute *)data;
    if (attribute->found)
    {
        return;
    }

    attribute->value = g_strdup (exif_content_get_value (content, attribute->tag, b, sizeof(b)));
    if (attribute->value != NULL)
    {
        attribute->found = TRUE;
    }
}

static char *
exifdata_get_tag_name_utf8 (ExifTag tag)
{
    return exif_string_to_utf8 (exif_tag_get_name (tag));
}

static char *
exifdata_get_tag_value_utf8 (ExifData *data, ExifTag tag)
{
    struct ExifAttribute attribute;
    char *utf8_value;

    attribute.tag = tag;
    attribute.value = NULL;
    attribute.found = FALSE;

    exif_data_foreach_content (data, exif_content_callback, &attribute);

    if (attribute.found)
    {
        utf8_value = exif_string_to_utf8 (attribute.value);
        g_free (attribute.value);
    }
    else
    {
        utf8_value = NULL;
    }

    return utf8_value;
}

static gboolean
append_tag_value_pair (CajaImagePropertiesPage *page,
                       ExifData *data,
                       ExifTag   tag,
                       char     *description)
{
    char *utf_attribute;
    char *utf_value;

    utf_attribute = exifdata_get_tag_name_utf8 (tag);
    utf_value = exifdata_get_tag_value_utf8 (data, tag);

    if ((utf_attribute == NULL) || (utf_value == NULL))
    {
        g_free (utf_attribute);
        g_free (utf_value);
        return FALSE;
    }

    append_label_take_str
    (page->details->vbox,
     g_strdup_printf ("<b>%s:</b> %s",
                      description ? description : utf_attribute,
                      utf_value));

    g_free (utf_attribute);
    g_free (utf_value);
    return TRUE;
}

static void
append_exifdata_string (ExifData *exifdata, CajaImagePropertiesPage *page)
{
    if (exifdata && exifdata->ifd[0] && exifdata->ifd[0]->count)
    {
        append_tag_value_pair (page, exifdata, EXIF_TAG_MAKE, _("Camera Brand"));
        append_tag_value_pair (page, exifdata, EXIF_TAG_MODEL, _("Camera Model"));

        /* Choose which date to show in order of relevance */
        if (!append_tag_value_pair (page, exifdata, EXIF_TAG_DATE_TIME_ORIGINAL, _("Date Taken")))
        {
            if (!append_tag_value_pair (page, exifdata, EXIF_TAG_DATE_TIME_DIGITIZED, _("Date Digitized")))
            {
                append_tag_value_pair (page, exifdata, EXIF_TAG_DATE_TIME, _("Date Modified"));
            }
        }

        append_tag_value_pair (page, exifdata, EXIF_TAG_EXPOSURE_TIME, _("Exposure Time"));
        append_tag_value_pair (page, exifdata, EXIF_TAG_APERTURE_VALUE, _("Aperture Value"));
        append_tag_value_pair (page, exifdata, EXIF_TAG_ISO_SPEED_RATINGS, _("ISO Speed Rating"));
        append_tag_value_pair (page, exifdata, EXIF_TAG_FLASH,_("Flash Fired"));
        append_tag_value_pair (page, exifdata, EXIF_TAG_METERING_MODE, _("Metering Mode"));
        append_tag_value_pair (page, exifdata, EXIF_TAG_EXPOSURE_PROGRAM, _("Exposure Program"));
        append_tag_value_pair (page, exifdata, EXIF_TAG_FOCAL_LENGTH,_("Focal Length"));
        append_tag_value_pair (page, exifdata, EXIF_TAG_SOFTWARE, _("Software"));
    }
}
#endif /*HAVE_EXIF*/

#ifdef HAVE_EXEMPI
static void
append_xmp_value_pair (CajaImagePropertiesPage *page,
                       XmpPtr      xmp,
                       const char *ns,
                       const char *propname,
                       char       *descr)
{
    uint32_t options;
    XmpStringPtr value;

    value = xmp_string_new();

    if (xmp_get_property (xmp, ns, propname, value, &options))
    {
        if (XMP_IS_PROP_SIMPLE (options))
        {
            append_label_take_str
            (page->details->vbox,
             g_strdup_printf ("<b>%s:</b> %s",
                              descr, xmp_string_cstr (value)));
        }
        else if (XMP_IS_PROP_ARRAY (options))
        {
            XmpIteratorPtr iter;

            iter = xmp_iterator_new (xmp, ns, propname, XMP_ITER_JUSTLEAFNODES);
            if (iter)
            {
                GString *str;
                gboolean first = TRUE;

                str = g_string_new (NULL);

                g_string_append_printf (str, "<b>%s:</b> ",
                                        descr);
                while (xmp_iterator_next (iter, NULL, NULL, value, &options)
                        && !XMP_IS_PROP_QUALIFIER(options))
                {
                    if (!first)
                    {
                        g_string_append_printf (str, ", ");
                    }
                    else
                    {
                        first = FALSE;
                    }
                    g_string_append_printf (str,
                                            "%s",
                                            xmp_string_cstr(value));
                }
                xmp_iterator_free(iter);
                append_label_take_str (page->details->vbox,
                                       g_string_free (str, FALSE));
            }
        }
    }
    xmp_string_free(value);
}

static void
append_xmpdata_string (XmpPtr xmp, CajaImagePropertiesPage *page)
{
    if (xmp != NULL)
    {
        append_xmp_value_pair (page, xmp, NS_IPTC4XMP, "Location", _("Location"));
        append_xmp_value_pair (page, xmp, NS_DC, "description", _("Description"));
        append_xmp_value_pair (page, xmp, NS_DC, "subject", _("Keywords"));
        append_xmp_value_pair (page, xmp, NS_DC, "creator", _("Creator"));
        append_xmp_value_pair (page, xmp, NS_DC, "rights", _("Copyright"));
        append_xmp_value_pair (page, xmp, NS_XAP,"Rating", _("Rating"));
        /* TODO add CC licenses */
    }
}
#endif /*HAVE EXEMPI*/

static void
load_finished (CajaImagePropertiesPage *page)
{
    gtk_widget_destroy (page->details->loading_label);

    if (page->details->loader != NULL) {
        gdk_pixbuf_loader_close (page->details->loader, NULL);
    }

    if (page->details->got_size)
    {
        GdkPixbufFormat *format;
        char *name, *desc;
#ifdef HAVE_EXIF
        ExifData *exif_data;
#endif /*HAVE_EXIF*/

        format = gdk_pixbuf_loader_get_format (page->details->loader);

        name = gdk_pixbuf_format_get_name (format);
        desc = gdk_pixbuf_format_get_description (format);
        append_label_take_str
        (page->details->vbox,
         g_strdup_printf ("<b>%s</b> %s (%s)",
                          _("Image Type:"), name, desc));
        append_label_take_str
        (page->details->vbox,
         g_strdup_printf (ngettext ("<b>Width:</b> %d pixel",
                                    "<b>Width:</b> %d pixels",
                                    page->details->width),
                          page->details->width));
        append_label_take_str
        (page->details->vbox,
         g_strdup_printf (ngettext ("<b>Height:</b> %d pixel",
                                    "<b>Height:</b> %d pixels",
                                    page->details->height),
                          page->details->height));
        g_free (name);
        g_free (desc);

#ifdef HAVE_EXIF
        exif_data = exif_loader_get_data (page->details->exifldr);
        append_exifdata_string (exif_data, page);
        exif_data_unref (exif_data);
#endif /*HAVE_EXIF*/
#ifdef HAVE_EXEMPI
        append_xmpdata_string (page->details->xmp, page);
#endif /*HAVE_EXEMPI*/
    }
    else
    {
        append_label (page->details->vbox,
                      _("Failed to load image information"));
    }

    if (page->details->loader != NULL)
    {
        g_object_unref (page->details->loader);
        page->details->loader = NULL;
    }
#ifdef HAVE_EXIF
    if (page->details->exifldr != NULL)
    {
        exif_loader_unref (page->details->exifldr);
        page->details->exifldr = NULL;
    }
#endif /*HAVE_EXIF*/
#ifdef HAVE_EXEMPI
    if (page->details->xmp != NULL)
    {
        xmp_free(page->details->xmp);
        page->details->xmp = NULL;
    }
#endif /*HAVE_EXEMPI*/
}

static void
file_read_callback (GObject      *object,
                    GAsyncResult *res,
                    gpointer      data)
{
    CajaImagePropertiesPage *page;
    GInputStream *stream;
    gssize count_read;
    GError *error;
    gboolean done_reading;

    page = CAJA_IMAGE_PROPERTIES_PAGE (data);
    stream = G_INPUT_STREAM (object);

    error = NULL;
    done_reading = FALSE;
    count_read = g_input_stream_read_finish (stream, res, &error);

    if (count_read > 0)
    {
        int exif_still_loading;

        g_assert (count_read <= sizeof(page->details->buffer));

#ifdef HAVE_EXIF
        exif_still_loading = exif_loader_write (page->details->exifldr,
                                                page->details->buffer,
                                                count_read);
#else
        exif_still_loading = 0;
#endif /*HAVE_EXIF*/

        if (page->details->pixbuf_still_loading)
        {
            if (!gdk_pixbuf_loader_write (page->details->loader,
                                          page->details->buffer,
                                          count_read,
                                          NULL))
            {
                page->details->pixbuf_still_loading = FALSE;
            }
        }

        if (page->details->pixbuf_still_loading ||
                (exif_still_loading == 1))
        {
            g_input_stream_read_async (G_INPUT_STREAM (stream),
                                       page->details->buffer,
                                       sizeof (page->details->buffer),
                                       0,
                                       page->details->cancellable,
                                       file_read_callback,
                                       page);
        }
        else
        {
            done_reading = TRUE;
        }
    }
    else
    {
        /* either EOF, cancelled or an error occurred */
        done_reading = TRUE;
    }

    if (done_reading)
    {
        load_finished (page);
        g_input_stream_close_async (stream,
                                    0,
                                    page->details->cancellable,
                                    file_close_callback,
                                    page);
    }
}

static void
size_prepared_callback (GdkPixbufLoader *loader,
                        int              width,
                        int              height,
                        gpointer         callback_data)
{
    CajaImagePropertiesPage *page;

    page = CAJA_IMAGE_PROPERTIES_PAGE (callback_data);

    page->details->height = height;
    page->details->width = width;
    page->details->got_size = TRUE;
    page->details->pixbuf_still_loading = FALSE;
}

static void
file_open_callback (GObject      *object,
                    GAsyncResult *res,
                    gpointer      data)
{
    CajaImagePropertiesPage *page;
    GFile *file;
    GFileInputStream *stream;
    GError *error;

    page = CAJA_IMAGE_PROPERTIES_PAGE (data);
    file = G_FILE (object);

    error = NULL;
    stream = g_file_read_finish (file, res, &error);
    if (stream)
    {
        page->details->loader = gdk_pixbuf_loader_new ();
        page->details->pixbuf_still_loading = TRUE;
        page->details->width = 0;
        page->details->height = 0;
#ifdef HAVE_EXIF
        page->details->exifldr = exif_loader_new ();
#endif /*HAVE_EXIF*/

        g_signal_connect (page->details->loader,
                          "size_prepared",
                          G_CALLBACK (size_prepared_callback),
                          page);

        g_input_stream_read_async (G_INPUT_STREAM (stream),
                                   page->details->buffer,
                                   sizeof (page->details->buffer),
                                   0,
                                   page->details->cancellable,
                                   file_read_callback,
                                   page);

        g_object_unref (stream);
    }
}

static void
load_location (CajaImagePropertiesPage *page,
               const char                  *location)
{
    GFile *file;

    g_assert (CAJA_IS_IMAGE_PROPERTIES_PAGE (page));
    g_assert (location != NULL);

    page->details->cancellable = g_cancellable_new ();
    file = g_file_new_for_uri (location);

#ifdef HAVE_EXEMPI
    {
        /* Current Exempi does not support setting custom IO to be able to use Mate-vfs */
        /* So it will only work with local files. Future version might remove this limitation */
        XmpFilePtr xf;
        char *localname;

        localname = g_filename_from_uri (location, NULL, NULL);
        if (localname)
        {
            xf = xmp_files_open_new (localname, 0);
            page->details->xmp = xmp_files_get_new_xmp (xf); /* only load when loading */
            xmp_files_close (xf, 0);
            g_free (localname);
        }
        else
        {
            page->details->xmp = NULL;
        }
    }
#endif /*HAVE_EXEMPI*/

    g_file_read_async (file,
                       0,
                       page->details->cancellable,
                       file_open_callback,
                       page);

    g_object_unref (file);
}

static void
caja_image_properties_page_class_init (CajaImagePropertiesPageClass *class)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (class);

    object_class->finalize = caja_image_properties_page_finalize;
}

static void
caja_image_properties_page_init (CajaImagePropertiesPage *page)
{
    page->details = caja_image_properties_page_get_instance_private (page);

    gtk_orientable_set_orientation (GTK_ORIENTABLE (page), GTK_ORIENTATION_VERTICAL);

    gtk_box_set_homogeneous (GTK_BOX (page), FALSE);
    gtk_box_set_spacing (GTK_BOX (page), 2);
    gtk_container_set_border_width (GTK_CONTAINER (page), 6);

    page->details->vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    page->details->loading_label =
        append_label (page->details->vbox,_("loading..."));
    gtk_box_pack_start (GTK_BOX (page),
                        page->details->vbox,
                        FALSE, TRUE, 2);

    gtk_widget_show_all (GTK_WIDGET (page));
}

static GList *
get_property_pages (CajaPropertyPageProvider *provider,
                    GList *files)
{
    GList *pages;
    CajaPropertyPage *real_page;
    CajaFileInfo *file;
    char *uri;
    CajaImagePropertiesPage *page;

    /* Only show the property page if 1 file is selected */
    if (!files || files->next != NULL)
    {
        return NULL;
    }

    file = CAJA_FILE_INFO (files->data);

    if (!
            (caja_file_info_is_mime_type (file, "image/x-bmp") ||
             caja_file_info_is_mime_type (file, "image/x-ico") ||
             caja_file_info_is_mime_type (file, "image/jpeg") ||
             caja_file_info_is_mime_type (file, "image/gif") ||
             caja_file_info_is_mime_type (file, "image/png") ||
             caja_file_info_is_mime_type (file, "image/pnm") ||
             caja_file_info_is_mime_type (file, "image/ras") ||
             caja_file_info_is_mime_type (file, "image/tga") ||
             caja_file_info_is_mime_type (file, "image/tiff") ||
             caja_file_info_is_mime_type (file, "image/wbmp") ||
             caja_file_info_is_mime_type (file, "image/x-xbitmap") ||
             caja_file_info_is_mime_type (file, "image/x-xpixmap")))
    {
        return NULL;
    }

    pages = NULL;

    uri = caja_file_info_get_uri (file);

    page = g_object_new (caja_image_properties_page_get_type (), NULL);
    load_location (page, uri);

    g_free (uri);

    real_page = caja_property_page_new
                ("CajaImagePropertiesPage::property_page",
                 gtk_label_new (_("Image")),
                 GTK_WIDGET (page));
    pages = g_list_append (pages, real_page);

    return pages;
}

static void
property_page_provider_iface_init (CajaPropertyPageProviderIface *iface)
{
    iface->get_pages = get_property_pages;
}


static void
caja_image_properties_page_provider_init (CajaImagePropertiesPageProvider *sidebar)
{
}

static void
caja_image_properties_page_provider_class_init (CajaImagePropertiesPageProviderClass *class)
{
}

void
caja_image_properties_page_register (void)
{
    caja_module_add_type (caja_image_properties_page_provider_get_type ());
}

