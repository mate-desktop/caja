#include <eel/eel-gdk-pixbuf-extensions.h>

#include "test.h"

#define DEST_WIDTH 32
#define DEST_HEIGHT 32

int
main (int argc, char* argv[])
{
	GdkPixbuf *pixbuf, *scaled;
	GError *error;
	gint64 t1, t2;
	int width;
	int height;

	test_init (&argc, &argv);

	if (argc != 2) {
		printf ("Usage: test <image filename>\n");
		exit (1);
	}

	error = NULL;
	pixbuf = gdk_pixbuf_new_from_file (argv[1], &error);

	if (pixbuf == NULL) {
		printf ("error loading pixbuf: %s\n", error->message);
		exit (1);
	}

	width = gdk_pixbuf_get_width (pixbuf);
        height = gdk_pixbuf_get_height (pixbuf);
	printf ("scale factors: %f, %f\n",
		((double) width)  / ((double) DEST_WIDTH),
		((double) height) / ((double) DEST_HEIGHT));

	t1 = g_get_monotonic_time ();
	scaled = eel_gdk_pixbuf_scale_down (pixbuf, DEST_WIDTH, DEST_HEIGHT);
	t2 = g_get_monotonic_time ();
	g_object_unref (scaled);
	g_print ("Time for eel_gdk_pixbuf_scale_down: %" G_GINT64_FORMAT " usecs\n",  t2 - t1);

	t1 = g_get_monotonic_time ();
	scaled = gdk_pixbuf_scale_simple (pixbuf, DEST_WIDTH, DEST_HEIGHT, GDK_INTERP_NEAREST);
	t2 = g_get_monotonic_time ();
	g_object_unref (scaled);
	g_print ("Time for INTERP_NEAREST: %" G_GINT64_FORMAT " usecs\n", t2 - t1);

	t1 = g_get_monotonic_time ();
	scaled = gdk_pixbuf_scale_simple (pixbuf, DEST_WIDTH, DEST_HEIGHT, GDK_INTERP_BILINEAR);
	t2 = g_get_monotonic_time ();
	g_object_unref (scaled);
	g_print ("Time for INTERP_BILINEAR: %" G_GINT64_FORMAT " usecs\n", t2 - t1);

	scaled = eel_gdk_pixbuf_scale_down (pixbuf, DEST_WIDTH, DEST_HEIGHT);
	gdk_pixbuf_save (scaled, "eel_scaled.png", "png", NULL, NULL);
	g_object_unref (scaled);

	scaled = gdk_pixbuf_scale_simple (pixbuf, DEST_WIDTH, DEST_HEIGHT, GDK_INTERP_NEAREST);
	gdk_pixbuf_save (scaled, "nearest_scaled.png", "png", NULL, NULL);
	g_object_unref (scaled);

	scaled = gdk_pixbuf_scale_simple (pixbuf, DEST_WIDTH, DEST_HEIGHT, GDK_INTERP_BILINEAR);
	gdk_pixbuf_save (scaled, "bilinear_scaled.png", "png", NULL, NULL);
	g_object_unref (scaled);

	return 0;
}
