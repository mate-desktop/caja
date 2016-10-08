/*
 * Caja
 *
 * Copyright (C) 1999, 2000 Red Hat, Inc.
 * Copyright (C) 1999, 2000 Eazel, Inc.
 *
 * Caja is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Caja is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors: Elliot Lee <sopwith@redhat.com>,
 *          Darin Adler <darin@bentspoon.com>,
 *          John Sullivan <sullivan@eazel.com>,
 *          Cosimo Cecchi <cosimoc@gnome.org>
 *
 */

/* caja-main.c: Implementation of the routines that drive program lifecycle and main window creation/destruction. */

#include <config.h>
#include "caja-main.h"
#if !GTK_CHECK_VERSION (3, 0, 0)
#include "caja-application.h"
#include "caja-self-check-functions.h"
#endif
#include "caja-window.h"
#include <dlfcn.h>
#include <signal.h>
#include <eel/eel-debug.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-self-checks.h>
#if !GTK_CHECK_VERSION (3, 0, 0)
#include <libegg/eggsmclient.h>
#endif
#include <libegg/eggdesktopfile.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gdesktopappinfo.h>
#include <libcaja-private/caja-debug-log.h>
#include <libcaja-private/caja-global-preferences.h>
#if !GTK_CHECK_VERSION (3, 0, 0)
#include <libcaja-private/caja-lib-self-check-functions.h>
#endif
#include <libcaja-private/caja-icon-names.h>
#include <libxml/parser.h>
#ifdef HAVE_LOCALE_H
	#include <locale.h>
#endif
#ifdef HAVE_MALLOC_H
	#include <malloc.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_EXEMPI
	#include <exempi/xmp.h>
#endif

#if !GTK_CHECK_VERSION (3, 0, 0)
/* Keeps track of everyone who wants the main event loop kept active */
static GSList* event_loop_registrants;

static gboolean exit_with_last_window = TRUE;

static gboolean is_event_loop_needed(void)
{
	return event_loop_registrants != NULL || !exit_with_last_window;
}

static int quit_if_in_main_loop (gpointer callback_data)
{
    guint level;

    g_assert (callback_data == NULL);

    level = gtk_main_level ();

    /* We can be called even outside the main loop,
     * so check that we are in a loop before calling quit.
     */
    if (level != 0)
    {
        gtk_main_quit ();
    }

    /* We need to be called again if we quit a nested loop. */
    return level > 1;
}

static void eel_gtk_main_quit_all (void)
{
    /* Calling gtk_main_quit directly only kills the current/top event loop.
     * This idler will be run by the current event loop, killing it, and then
     * by the next event loop, ...
     */
    g_idle_add (quit_if_in_main_loop, NULL);
}

static void event_loop_unregister (GtkWidget *object)
{
    event_loop_registrants = g_slist_remove (event_loop_registrants, object);
    
    if (!is_event_loop_needed ())
    {
        eel_gtk_main_quit_all ();
    }
}

void caja_main_event_loop_register (GtkObject *object)
{
    g_signal_connect (object, "destroy", G_CALLBACK (event_loop_unregister), NULL);
    event_loop_registrants = g_slist_prepend (event_loop_registrants, GTK_WIDGET (object));
}

gboolean caja_main_is_event_loop_mainstay (GtkWidget *object)
{
    return g_slist_length (event_loop_registrants) == 1
           && event_loop_registrants->data == object;
}

void caja_main_event_loop_quit (gboolean explicit)
{
    if (explicit)
    {
        /* Explicit --quit, make sure we don't restart */

        /* To quit all instances, reset exit_with_last_window */
        exit_with_last_window = TRUE;

        if (event_loop_registrants == NULL)
        {
            /* If this is reached, caja must run in "daemon" mode
             * (i.e. !exit_with_last_window) with no windows open.
             * We need to quit_all here because the below loop won't
             * trigger a quit.
             */
            eel_gtk_main_quit_all();
        }

        /* TODO: With the old session we needed to set restart
           style to MATE_RESTART_IF_RUNNING here, but i don't think we need
           that now since mate-session doesn't restart apps except on startup. */
    }
    while (event_loop_registrants != NULL)
    {
        gtk_widget_destroy (event_loop_registrants->data);
    }
}
#endif
static void dump_debug_log (void)
{
    char *filename;

    filename = g_build_filename (g_get_home_dir (), "caja-debug-log.txt", NULL);
    caja_debug_log_dump (filename, NULL); /* NULL GError */
    g_free (filename);
}

static int debug_log_pipes[2];

static gboolean debug_log_io_cb (GIOChannel *io, GIOCondition condition, gpointer data)
{
    char a;

    while (read (debug_log_pipes[0], &a, 1) != 1)
        ;

    caja_debug_log (TRUE, CAJA_DEBUG_LOG_DOMAIN_USER,
                    "user requested dump of debug log");

    dump_debug_log ();
    return FALSE;
}

static void sigusr1_handler (int sig)
{
    while (write (debug_log_pipes[1], "a", 1) != 1)
        ;
}

/* This is totally broken as we're using non-signal safe
 * calls in sigfatal_handler. Disable by default. */
#ifdef USE_SEGV_HANDLER

/* sigaction structures for the old handlers of these signals */
static struct sigaction old_segv_sa;
static struct sigaction old_abrt_sa;
static struct sigaction old_trap_sa;
static struct sigaction old_fpe_sa;
static struct sigaction old_bus_sa;

static void
sigfatal_handler (int sig)
{
    void (* func) (int);

    /* FIXME: is this totally busted?  We do malloc() inside these functions,
     * and yet we are inside a signal handler...
     */
    caja_debug_log (TRUE, CAJA_DEBUG_LOG_DOMAIN_USER,
                    "debug log dumped due to signal %d", sig);
    dump_debug_log ();

    switch (sig)
    {
    case SIGSEGV:
        func = old_segv_sa.sa_handler;
        break;

    case SIGABRT:
        func = old_abrt_sa.sa_handler;
        break;

    case SIGTRAP:
        func = old_trap_sa.sa_handler;
        break;

    case SIGFPE:
        func = old_fpe_sa.sa_handler;
        break;

    case SIGBUS:
        func = old_bus_sa.sa_handler;
        break;

    default:
        func = NULL;
        break;
    }

    /* this scares me */
    if (func != NULL && func != SIG_IGN && func != SIG_DFL)
        (* func) (sig);
}
#endif

static void
setup_debug_log_signals (void)
{
    struct sigaction sa;
    GIOChannel *io;

    if (pipe (debug_log_pipes) == -1)
        g_error ("Could not create pipe() for debug log");

    io = g_io_channel_unix_new (debug_log_pipes[0]);
    g_io_add_watch (io, G_IO_IN, debug_log_io_cb, NULL);

    sa.sa_handler = sigusr1_handler;
    sigemptyset (&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction (SIGUSR1, &sa, NULL);

    /* This is totally broken as we're using non-signal safe
     * calls in sigfatal_handler. Disable by default. */
#ifdef USE_SEGV_HANDLER
    sa.sa_handler = sigfatal_handler;
    sigemptyset (&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGSEGV, &sa, &old_segv_sa);
    sigaction(SIGABRT, &sa, &old_abrt_sa);
    sigaction(SIGTRAP, &sa, &old_trap_sa);
    sigaction(SIGFPE,  &sa, &old_fpe_sa);
    sigaction(SIGBUS,  &sa, &old_bus_sa);
#endif
}

static GLogFunc default_log_handler;

static void
log_override_cb (const gchar   *log_domain,
                 GLogLevelFlags log_level,
                 const gchar   *message,
                 gpointer       user_data)
{
    gboolean is_debug;
    gboolean is_milestone;

    is_debug = ((log_level & G_LOG_LEVEL_DEBUG) != 0);
    is_milestone = !is_debug;

    caja_debug_log (is_milestone, CAJA_DEBUG_LOG_DOMAIN_GLOG, "%s", message);

    if (!is_debug)
        (* default_log_handler) (log_domain, log_level, message, user_data);
}

static void
setup_debug_log_glog (void)
{
    default_log_handler = g_log_set_default_handler (log_override_cb, NULL);
}

static void
setup_debug_log (void)
{
    char *config_filename;

    config_filename = g_build_filename (g_get_home_dir (), "caja-debug-log.conf", NULL);
    caja_debug_log_load_configuration (config_filename, NULL); /* NULL GError */
    g_free (config_filename);

    setup_debug_log_signals ();
    setup_debug_log_glog ();
}

static gboolean
running_in_mate (void)
{
    return (g_strcmp0 (g_getenv ("XDG_CURRENT_DESKTOP"), "MATE") == 0)
        || (g_strcmp0 (g_getenv ("XDG_SESSION_DESKTOP"), "MATE") == 0)
        || (g_strcmp0 (g_getenv ("DESKTOP_SESSION"), "MATE") == 0);
}

static gboolean
running_as_root (void)
{
    return geteuid () == 0;
}
#if GTK_CHECK_VERSION (3, 0, 0)
int
main (int argc, char *argv[])
{
	gint retval;
    CajaApplication *application;

#if defined (HAVE_MALLOPT) && defined(M_MMAP_THRESHOLD)
	/* Caja uses lots and lots of small and medium size allocations,
	 * and then a few large ones for the desktop background. By default
	 * glibc uses a dynamic treshold for how large allocations should
	 * be mmaped. Unfortunately this triggers quickly for caja when
	 * it does the desktop background allocations, raising the limit
	 * such that a lot of temporary large allocations end up on the
	 * heap and are thus not returned to the OS. To fix this we set
	 * a hardcoded limit. I don't know what a good value is, but 128K
	 * was the old glibc static limit, lets use that.
	 */
	mallopt (M_MMAP_THRESHOLD, 128 *1024);
#endif

#if !GLIB_CHECK_VERSION (2, 42, 0)
    /* This will be done by gtk+ later, but for now, force it to MATE */
    g_desktop_app_info_set_desktop_env ("MATE");
#endif

	if (g_getenv ("CAJA_DEBUG") != NULL) {
		eel_make_warnings_and_criticals_stop_in_debugger ();
	}
	
	/* Initialize gettext support */
	bindtextdomain (GETTEXT_PACKAGE, MATELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_set_prgname ("caja");

	if (g_file_test (DATADIR "/applications/caja.desktop", G_FILE_TEST_EXISTS)) {
		egg_set_desktop_file (DATADIR "/applications/caja.desktop");
	}
	
#ifdef HAVE_EXEMPI
	xmp_init();
#endif

	setup_debug_log ();

	/* Initialize the services that we use. */
	LIBXML_TEST_VERSION

    /* Run the caja application. */
    application = caja_application_new();

    retval = g_application_run (G_APPLICATION (application),
                                argc, argv);

    g_object_unref (application);

 	eel_debug_shut_down ();

	return retval;
}

#else
int
main (int argc, char *argv[])
{
    gboolean kill_shell;
    gboolean no_default_window;
    gboolean browser_window;
    gboolean no_desktop;
    gboolean version;
    gboolean autostart_mode;
    const char *autostart_id;
    gchar *geometry;
    gchar **remaining;
    gboolean perform_self_check;
    CajaApplication *application;
    GOptionContext *context;
    GFile *file = NULL;
    GFileInfo *fileinfo = NULL;
    GAppInfo *appinfo = NULL;
    char *uri = NULL;
    char **uris = NULL;
    GPtrArray *uris_array;
    GError *error;
    int i;

    const GOptionEntry options[] =
    {
#ifndef CAJA_OMIT_SELF_CHECK
        {
            "check", 'c', 0, G_OPTION_ARG_NONE, &perform_self_check,
            N_("Perform a quick set of self-check tests."), NULL
        },
#endif
        {
            "version", '\0', 0, G_OPTION_ARG_NONE, &version,
            N_("Show the version of the program."), NULL
        },
        {
            "geometry", 'g', 0, G_OPTION_ARG_STRING, &geometry,
            N_("Create the initial window with the given geometry."), N_("GEOMETRY")
        },
        {
            "no-default-window", 'n', 0, G_OPTION_ARG_NONE, &no_default_window,
            N_("Only create windows for explicitly specified URIs."), NULL
        },
        {
            "no-desktop", '\0', 0, G_OPTION_ARG_NONE, &no_desktop,
            N_("Do not manage the desktop (ignore the preference set in the preferences dialog)."), NULL
        },
        {
            "browser", '\0', 0, G_OPTION_ARG_NONE, &browser_window,
            N_("open a browser window."), NULL
        },
        {
            "quit", 'q', 0, G_OPTION_ARG_NONE, &kill_shell,
            N_("Quit Caja."), NULL
        },
        { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &remaining, NULL,  N_("[URI...]") },

        { NULL }
    };

#if defined (HAVE_MALLOPT) && defined(M_MMAP_THRESHOLD)
    /* Caja uses lots and lots of small and medium size allocations,
     * and then a few large ones for the desktop background. By default
     * glibc uses a dynamic treshold for how large allocations should
     * be mmaped. Unfortunately this triggers quickly for caja when
     * it does the desktop background allocations, raising the limit
     * such that a lot of temporary large allocations end up on the
     * heap and are thus not returned to the OS. To fix this we set
     * a hardcoded limit. I don't know what a good value is, but 128K
     * was the old glibc static limit, lets use that.
     */
    mallopt (M_MMAP_THRESHOLD, 128 *1024);
#endif

#if !GLIB_CHECK_VERSION (2, 42, 0)
    /* This will be done by gtk+ later, but for now, force it to MATE */
    g_desktop_app_info_set_desktop_env ("MATE");
#endif

    if (g_getenv ("CAJA_DEBUG") != NULL)
    {
        eel_make_warnings_and_criticals_stop_in_debugger ();
    }

    /* Initialize gettext support */
    bindtextdomain (GETTEXT_PACKAGE, MATELOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    autostart_mode = FALSE;

    autostart_id = g_getenv ("DESKTOP_AUTOSTART_ID");
    if (autostart_id != NULL && *autostart_id != '\0')
    {
        autostart_mode = TRUE;
    }

    /* Get parameters. */
    remaining = NULL;
    geometry = NULL;
    version = FALSE;
    kill_shell = FALSE;
    no_default_window = FALSE;
    no_desktop = FALSE;
    perform_self_check = FALSE;
    browser_window = FALSE;

    g_set_prgname ("caja");

    if (g_file_test (DATADIR "/applications/caja.desktop", G_FILE_TEST_EXISTS))
    {
        egg_set_desktop_file (DATADIR "/applications/caja.desktop");
    }

    context = g_option_context_new (_("\n\nBrowse the file system with the file manager"));
    g_option_context_add_main_entries (context, options, NULL);

    g_option_context_add_group (context, gtk_get_option_group (TRUE));
    g_option_context_add_group (context, egg_sm_client_get_option_group ());

    error = NULL;
    if (!g_option_context_parse (context, &argc, &argv, &error))
    {
        g_printerr ("Could not parse arguments: %s\n", error->message);
        g_error_free (error);
        return 1;
    }

    g_option_context_free (context);

    if (version)
    {
        g_print ("MATE caja " PACKAGE_VERSION "\n");
        return 0;
    }

#ifdef HAVE_EXEMPI
    xmp_init();
#endif

    setup_debug_log ();

    /* If in autostart mode (aka started by mate-session), we need to ensure
     * caja starts with the correct options.
     */
    if (autostart_mode)
    {
        no_default_window = TRUE;
        no_desktop = FALSE;
    }
    else if (running_as_root () || !running_in_mate ())
    {
        /* do not manage desktop when running as root or on other desktops */
        no_desktop = TRUE;

        /* set smclient mode to "no restart" when running as root or on other desktops */
        egg_sm_client_set_mode (EGG_SM_CLIENT_MODE_NO_RESTART);
    }

    if (perform_self_check && remaining != NULL)
    {
        /* translators: %s is an option (e.g. --check) */
        fprintf (stderr, _("caja: %s cannot be used with URIs.\n"),
                 "--check");
        return EXIT_FAILURE;
    }
    if (perform_self_check && kill_shell)
    {
        fprintf (stderr, _("caja: --check cannot be used with other options.\n"));
        return EXIT_FAILURE;
    }
    if (kill_shell && remaining != NULL)
    {
        fprintf (stderr, _("caja: %s cannot be used with URIs.\n"),
                 "--quit");
        return EXIT_FAILURE;
    }
    if (geometry != NULL && remaining != NULL && remaining[0] != NULL && remaining[1] != NULL)
    {
        fprintf (stderr, _("caja: --geometry cannot be used with more than one URI.\n"));
        return EXIT_FAILURE;
    }

    /* Initialize the services that we use. */
    LIBXML_TEST_VERSION

    /* Initialize preferences. This is needed so that proper
     * defaults are available before any preference peeking
     * happens.
     */
    caja_global_preferences_init ();

    /* exit_with_last_window is already set to TRUE, and we need to keep that value
     * on other desktops or when running caja as root. Otherwise, we read the value
     * from the configuration.
     */
    if (running_in_mate () && !running_as_root())
    {
        exit_with_last_window = g_settings_get_boolean (caja_preferences, CAJA_PREFERENCES_EXIT_WITH_LAST_WINDOW);
    }

    application = NULL;

    /* Do either the self-check or the real work. */
    if (perform_self_check)
    {
		#ifndef CAJA_OMIT_SELF_CHECK
			/* Run the checks (each twice) for caja and libcaja-private. */

			caja_run_self_checks ();
			caja_run_lib_self_checks ();
			eel_exit_if_self_checks_failed ();

			caja_run_self_checks ();
			caja_run_lib_self_checks ();
			eel_exit_if_self_checks_failed ();
		#endif
    }
    else
    {
        /* Convert args to URIs */
        if (remaining != NULL)
        {
            uris_array = g_ptr_array_new ();
            for (i = 0; remaining[i] != NULL; i++)
            {
                file = g_file_new_for_commandline_arg (remaining[i]);
                if (file != NULL)
                {
                    uri = g_file_get_uri (file);
                    if (uri)
                    {
                        fileinfo = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_TYPE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
                        if (fileinfo && g_file_info_get_file_type(fileinfo) == G_FILE_TYPE_DIRECTORY)
                        {
                            g_ptr_array_add (uris_array, uri);
                        }
                        else
                        {
                            if (fileinfo)
                                g_object_unref (fileinfo);
                            fileinfo = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
                            if (fileinfo)
                            {
                                appinfo = g_app_info_get_default_for_type (g_file_info_get_content_type (fileinfo), TRUE);
                                if (appinfo)
                                {
                                    if (g_strcmp0 (g_app_info_get_executable (appinfo), "caja") != 0)
                                    {
                                        g_app_info_launch_default_for_uri (uri, NULL, NULL);
                                    }
                                    else
                                    {
                                        fprintf (stderr, _("caja: set erroneously as default application for '%s' content type.\n"),
                                                 g_file_info_get_content_type (fileinfo));
                                    }
                                    g_object_unref (appinfo);
                                }
                                g_free (uri);
                            }
                            else
                            {
                                g_ptr_array_add (uris_array, uri);
                            }
                        }
                        if (fileinfo)
                            g_object_unref (fileinfo);
                    }
                    if (file)
                        g_object_unref (file);
                }
            }
            if (uris_array->len == 0)
            {
                /* Caja is being used only to open files (not directories), so closing */
                g_strfreev (remaining);
                return EXIT_SUCCESS;
            }
            g_ptr_array_add (uris_array, NULL);
            uris = (char**) g_ptr_array_free (uris_array, FALSE);
            g_strfreev (remaining);
        }


        /* Run the caja application. */
        application = caja_application_new ();

        if (egg_sm_client_is_resumed (application->smclient))
        {
            no_default_window = TRUE;
        }

        caja_application_startup
        (application,
         kill_shell, no_default_window, no_desktop,
         browser_window,
         geometry,
         uris);
        g_strfreev (uris);

        if (unique_app_is_running (application->unique_app) ||
                kill_shell)
        {
            exit_with_last_window = TRUE;
        }

        if (is_event_loop_needed ())
        {
            gtk_main ();
        }
    }

    caja_icon_info_clear_caches ();

    if (application != NULL)
    {
        g_object_unref (application);
    }

    eel_debug_shut_down ();

    caja_application_save_accel_map (NULL);

    return EXIT_SUCCESS;
}
#endif
