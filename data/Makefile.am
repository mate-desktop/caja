NULL =

xml_in_files = caja.xml.in
xml_files = $(xml_in_files:.xml.in=.xml)
@INTLTOOL_XML_RULE@

if WITH_GTK3
autostartdir     = $(sysconfdir)/xdg/autostart
autostart_DATA = $(autostart_in_files:caja-autostart.desktop.in=caja-autostart.desktop)
autostart_in_files = \
	caja-autostart.desktop.in
endif

desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

if WITH_GTK3
desktop_in_files = \
	caja-home.desktop.in \
	caja-computer.desktop.in \
	caja-folder-handler.desktop.in \
	caja-file-management-properties.desktop.in  \
	caja-browser.desktop.in                     \
	caja-autorun-software.desktop.in
else
desktop_in_files = \
	caja.desktop.in \
	caja-home.desktop.in \
	caja-computer.desktop.in \
	caja-folder-handler.desktop.in \
	caja-file-management-properties.desktop.in  \
	caja-browser.desktop.in                     \
	caja-autorun-software.desktop.in
endif
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
	caja.css \
	$(NULL)

# app data file
appdatadir = $(datadir)/appdata
appdata_in_files = caja.appdata.xml.in
appdata_DATA = $(appdata_in_files:.xml.in=.xml)


if WITH_GTK3
EXTRA_DIST = $(cajadata_DATA) \
	caja.css \
	freedesktop-dbus-interfaces.xml \
	$(xml_in_files) \
	$(desktop_in_files) \
    $(autostart_DATA) \
	$(service_in_files) \
	$(appdata_in_files) \
	$(NULL)
else
EXTRA_DIST = $(cajadata_DATA) \
	caja.desktop \
	caja.desktop.in \
	caja.css \
	freedesktop-dbus-interfaces.xml \
	$(xml_in_files) \
	$(desktop_in_files) \
	$(service_in_files) \
	$(appdata_in_files) \
	$(NULL)
endif

if WITH_GTK3
CLEANFILES = $(xml_files) \
	$(desktop_DATA) \
    $(autostart_DATA) \
	$(service_DATA) \
    caja-autostart.desktop \
	caja.appdata.xml \
	$(NULL)
else
CLEANFILES = $(xml_files) \
	$(desktop_DATA) \
	$(service_DATA) \
	caja.appdata.xml \
	$(NULL)
endif

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
