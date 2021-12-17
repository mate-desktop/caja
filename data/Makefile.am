NULL =

desktopdir = $(datadir)/applications
desktop_DATA = \
	caja.desktop \
	caja-home.desktop \
	caja-computer.desktop \
	caja-folder-handler.desktop \
	caja-file-management-properties.desktop  \
	caja-browser.desktop \
	caja-autorun-software.desktop \
	mate-network-scheme.desktop
desktop_in_files = $(desktop_DATA:.desktop=.desktop.in)
desktop_in_in_files = $(desktop_DATA:.desktop=.desktop.in.in)

%.desktop.in: %.desktop.in.in
	$(AM_V_GEN) sed -e 's|@bindir[@]|$(bindir)|g' $< > $@

%.desktop: %.desktop.in
if USE_NLS
	$(AM_V_GEN) GETTEXTDATADIR=$(top_srcdir) $(MSGFMT) --desktop \
		--keyword= --keyword=Name --keyword=GenericName --keyword=Comment --keyword=Keywords \
		--template $< -d $(top_srcdir)/po -o $@
else
	$(AM_V_GEN) cp -f $< $@
endif

mimedir = $(datadir)/mime/packages
mime_DATA = caja.xml
mime_in_files = $(mime_DATA:.xml=.xml.in)

$(mime_DATA): $(mime_in_files)
if USE_NLS
	$(AM_V_GEN) GETTEXTDATADIR=$(top_srcdir) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@
else
	$(AM_V_GEN) cp -f $< $@
endif

servicedir = $(datadir)/dbus-1/services
service_in_files = org.mate.freedesktop.FileManager1.service.in
service_DATA = $(service_in_files:.service.in=.service)

org.mate.freedesktop.FileManager1.service: org.mate.freedesktop.FileManager1.service.in Makefile
	$(AM_V_GEN) sed -e "s|\@bindir\@|$(bindir)|" $< > $@

cajadatadir = $(datadir)/caja
cajadata_DATA = \
	browser.xml \
	caja-extras.placeholder  \
	caja-suggested.placeholder \
	caja.css \
	caja-desktop.css \
	a11y-caja-desktop-base.css \
	caja-desktop-ContrastHigh.css \
	caja-desktop-ContrastHighInverse.css \
	caja-desktop-HighContrast.css \
	caja-desktop-HighContrastInverse.css \
	$(NULL)

# app data file
appdatadir = $(datadir)/metainfo
appdata_in_in_files = caja.appdata.xml.in.in
appdata_in_files = $(appdata_in_in_files:.appdata.xml.in.in=.appdata.xml.in)
appdata_DATA = $(appdata_in_files:.xml.in=.xml)

$(appdata_DATA): $(appdata_in_files)
if USE_NLS
	$(AM_V_GEN) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@
else
	$(AM_V_GEN) cp -f $< $@
endif

ui_files = \
	caja-file-management-properties.ui \
	caja-bookmarks-window.ui \
	$(NULL)

EXTRA_DIST = \
	$(appdata_in_in_files) \
	$(cajadata_DATA) \
	$(desktop_in_in_files) \
	$(mime_in_files) \
	$(service_in_files) \
	$(ui_files) \
	caja-resources.gresource.xml \
	freedesktop-dbus-interfaces.xml \
	ui-a11y.suppr \
	$(NULL)

DISTCLEANFILES = \
	$(appdata_in_files) \
	$(desktop_in_files) \
	$(NULL)

CLEANFILES = \
	$(appdata_DATA) \
	$(desktop_DATA) \
	$(mime_DATA) \
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

include $(top_srcdir)/gla11y.mk
-include $(top_srcdir)/git.mk
