#include "test.h"

#include <eel/eel-wrap-table.h>
#include <eel/eel-labeled-image.h>
#include <eel/eel-vfs-extensions.h>
#include <libcaja-private/caja-customization-data.h>
#include <libcaja-private/caja-icon-info.h>

int 
main (int argc, char* argv[])
{
	CajaCustomizationData *customization_data;
	GtkWidget *window;
	GtkWidget *emblems_table, *button, *scroller;
	char *emblem_name, *stripped_name;
	GdkPixbuf *pixbuf;
	char *label;

	test_init (&argc, &argv);

	window = test_window_new ("Wrap Table Test", 10);

	gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);

	/* The emblems wrapped table */
	emblems_table = eel_wrap_table_new (TRUE);

	gtk_widget_show (emblems_table);
	gtk_container_set_border_width (GTK_CONTAINER (emblems_table), 8);
	
	scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);

	/* Viewport */
#if GTK_CHECK_VERSION (3, 0, 0)
	gtk_container_add (GTK_CONTAINER (scroller),
					   emblems_table);
#else
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scroller),
	                                       emblems_table);
#endif

	gtk_container_add (GTK_CONTAINER (window), scroller);

	gtk_widget_show (scroller);

#if 0
	/* Get rid of default lowered shadow appearance. 
	 * This must be done after the widget is realized, due to
	 * an apparent bug in gtk_viewport_set_shadow_type.
	 */
 	g_signal_connect (GTK_BIN (scroller->child), 
			  "realize", 
			  remove_default_viewport_shadow, 
			  NULL);
#endif


	/* Use caja_customization to make the emblem widgets */
	customization_data = caja_customization_data_new ("emblems", TRUE,
							      CAJA_ICON_SIZE_SMALL, 
							      CAJA_ICON_SIZE_SMALL);
	
	while (caja_customization_data_get_next_element_for_display (customization_data,
									 &emblem_name,
									 &pixbuf,
									 &label)) {	

		stripped_name = eel_filename_strip_extension (emblem_name);
		g_free (emblem_name);
		
		if (strcmp (stripped_name, "erase") == 0) {
			g_object_unref (pixbuf);
			g_free (label);
			g_free (stripped_name);
			continue;
		}

		button = eel_labeled_image_check_button_new (label, pixbuf);
		g_free (label);
		g_object_unref (pixbuf);

		/* Attach parameters and signal handler. */
		g_object_set_data_full (G_OBJECT (button),
					"caja_property_name",
					stripped_name,
					(GDestroyNotify) g_free);
				     
		gtk_container_add (GTK_CONTAINER (emblems_table), button);
	}

	gtk_widget_show_all (emblems_table);

	gtk_widget_show (window);
	
	gtk_main ();
	
	return 0;
}
