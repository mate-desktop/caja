include $(top_srcdir)/Makefile.shared

noinst_LTLIBRARIES=libcaja-file-manager.la

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/cut-n-paste-code \
	$(CORE_CFLAGS) \
	$(WARNING_CFLAGS) \
	-DCAJA_DATADIR=\""$(datadir)/caja"\" \
	-DDATADIR=\""$(datadir)"\" \
	$(DISABLE_DEPRECATED_CFLAGS) \
	$(NULL)



libcaja_file_manager_la_SOURCES = \
	fm-actions.h \
	fm-desktop-icon-view.c \
	fm-desktop-icon-view.h \
	fm-directory-view.c \
	fm-directory-view.h \
	fm-ditem-page.c \
	fm-ditem-page.h \
	fm-error-reporting.c \
	fm-error-reporting.h \
	fm-icon-container.c \
	fm-icon-container.h \
	fm-icon-view.c \
	fm-icon-view.h \
	fm-list-model.c \
	fm-list-model.h \
	fm-list-view-private.h \
	fm-list-view.c \
	fm-list-view.h \
	fm-properties-window.c \
	fm-properties-window.h \
	fm-tree-model.c \
	fm-tree-model.h \
	fm-tree-view.c \
	fm-tree-view.h \
	caja-audio-mime-types.h \
	$(NULL)

EMPTY_VIEW_SOURCES = \
	fm-empty-view.c \
	fm-empty-view.h

if ENABLE_EMPTY_VIEW
libcaja_file_manager_la_SOURCES += $(EMPTY_VIEW_SOURCES)
endif

uidir = $(datadir)/caja/ui
ui_DATA = \
	caja-desktop-icon-view-ui.xml \
	caja-directory-view-ui.xml \
	caja-icon-view-ui.xml \
	caja-list-view-ui.xml \
	$(NULL)

EXTRA_DIST = $(ui_DATA)
