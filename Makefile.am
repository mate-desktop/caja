include $(top_srcdir)/Makefile.shared

SUBDIRS = \
	eel \
	libcaja-extension \
	libegg \
	libcaja-private \
	src \
	test \
	po \
	data \
	icons \
	docs \
	$(NULL)

EXTRA_DIST = \
	autogen.sh \
	COPYING.LIB \
	COPYING.EXTENSIONS \
	HACKING \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	Makefile.shared \
	$(NULL)

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update

DISTCHECK_CONFIGURE_FLAGS = \
	--disable-update-mimedb \
	--disable-icon-update \
	--enable-gtk-doc \
	CFLAGS='-Wno-deprecated-declarations'

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

# Build ChangeLog from GIT  history
ChangeLog:
	$(AM_V_GEN) if test -d $(top_srcdir)/.git; then \
		GIT_DIR="$(top_srcdir)/.git" git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog
