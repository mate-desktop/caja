NULL=

AM_CPPFLAGS =\
	-I$(top_srcdir) \
	$(CORE_CFLAGS) \
	$(WARNING_CFLAGS) \
	-DVERSION="\"$(VERSION)\"" \
	-DCAJA_DATADIR=\""$(datadir)/caja"\" \
	-DMATELOCALEDIR=\""$(prefix)/${DATADIRNAME}/locale"\" \
	$(NULL)

LDADD =\
	$(top_builddir)/libcaja-private/libcaja-private.la \
	$(CORE_LIBS) \
	$(NULL)

noinst_PROGRAMS =\
	test-caja-wrap-table \
	test-caja-search-engine \
	test-caja-directory-async \
	test-caja-copy \
	test-eel-background \
	test-eel-editable-label \
	test-eel-image-table \
	test-eel-labeled-image \
	test-eel-pixbuf-scale \
	$(NULL)

test_caja_copy_SOURCES = test-copy.c test.c

test_caja_wrap_table_SOURCES = test-caja-wrap-table.c test.c

test_caja_search_engine_SOURCES = test-caja-search-engine.c 

test_caja_directory_async_SOURCES = test-caja-directory-async.c

test_eel_background_SOURCES = test-eel-background.c
test_eel_image_table_SOURCES = test-eel-image-table.c test.c
test_eel_labeled_image_SOURCES = test-eel-labeled-image.c test.c test.h
test_eel_pixbuf_scale_SOURCES = test-eel-pixbuf-scale.c test.c test.h

EXTRA_DIST = \
	test.h \
	$(NULL)

-include $(top_srcdir)/git.mk
