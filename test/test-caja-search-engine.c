#include <gtk/gtk.h>

#include <libcaja-private/caja-search-engine.h>

static void
hits_added_cb (CajaSearchEngine *engine, GSList *hits)
{
	g_print ("hits added\n");
	while (hits) {
		g_print (" - %s\n", (char *)hits->data);
		hits = hits->next;
	}
}

static void
hits_subtracted_cb (CajaSearchEngine *engine, GSList *hits)
{
	g_print ("hits subtracted\n");
	while (hits) {
		g_print (" - %s\n", (char *)hits->data);
		hits = hits->next;
	}
}

static void
finished_cb (CajaSearchEngine *engine)
{
	g_print ("finished!\n");
//	gtk_main_quit ();
}

int
main (int argc, char* argv[])
{
	CajaSearchEngine *engine;
	CajaQuery *query;

	gtk_init (&argc, &argv);

	engine = caja_search_engine_new ();
	g_signal_connect (engine, "hits-added",
			  G_CALLBACK (hits_added_cb), NULL);
	g_signal_connect (engine, "hits-subtracted",
			  G_CALLBACK (hits_subtracted_cb), NULL);
	g_signal_connect (engine, "finished",
			  G_CALLBACK (finished_cb), NULL);

	query = caja_query_new ();
	caja_query_set_text (query, "richard hult");
	caja_search_engine_set_query (engine, query);
	g_object_unref (query);

	caja_search_engine_start (engine);

	gtk_main ();
	return 0;
}
