NULL =

xml_in_files = caja.xml.in
xml_files = $(xml_in_files:.xml.in=.xml)
@INTLTOOL_XML_RULE@

desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
desktop_in_files = \
	caja.desktop.in \
	caja-home.desktop.in \
	caja-computer.desktop.in \
	caja-folder-handler.desktop.in \
	caja-file-management-properties.desktop.in  \
	caja-browser.desktop.in                     \
	caja-autorun-software.desktop.in
@INTLTOOL_DESKTOP_RULE@

mimedir = $(datadir)/mime/packages
mime_DATA = $(xml_files)

servicedir = $(datadir)/dbus-1/services
service_DATA = $(service_in_files:.service.in=.service)
service_in_files = 				\
	org.mate.freedesktop.FileManager1.service.in

org.mate.freedesktop.FileManager1.service: org.mate.freedesktop.FileManager1.service.in Makefile
	$(AM_V_GEN) sed -e "s|\@bindir\@|$(bindir)|" $< > $@

cajadatadir = $(datadir)/caja

cajadata_DATA = \
	browser.xml \
	caja-extras.placeholder  \
	caja-suggested.placeholder \
	$(NULL)

EXTRA_DIST = $(cajadata_DATA) \
	caja.desktop \
	caja.desktop.in \
	freedesktop-dbus-interfaces.xml \
	$(xml_in_files) \
	$(desktop_in_files) \
	$(service_in_files) \
	$(NULL)

CLEANFILES = $(xml_files) \
	$(desktop_DATA) \
	$(service_DATA) \
	$(NULL)

SUBDIRS = \
	icons \
	patterns \
	$(NULL)

install-data-hook:
if ENABLE_UPDATE_MIMEDB
	$(UPDATE_MIME_DATABASE) "$(DESTDIR)$(datadir)/mime"
endif

uninstall-hook:
if ENABLE_UPDATE_MIMEDB
	$(UPDATE_MIME_DATABASE) "$(DESTDIR)$(datadir)/mime"
endif
