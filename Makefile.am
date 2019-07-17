include $(top_srcdir)/Makefile.shared

SUBDIRS = \
	po \
	eel \
	libcaja-extension \
	libegg \
	libcaja-private \
	src \
	test \
	data \
	icons \
	docs \
	$(NULL)

EXTRA_DIST = \
	its \
	autogen.sh \
	COPYING.LIB \
	COPYING.EXTENSIONS \
	HACKING \
	Makefile.shared \
	$(NULL)

DISTCHECK_CONFIGURE_FLAGS = \
	--disable-update-mimedb \
	--disable-icon-update \
	--enable-gtk-doc \
	--disable-self-check \
	CFLAGS='-Wno-deprecated-declarations'

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

# Build ChangeLog from GIT  history
ChangeLog:
	$(AM_V_GEN) if test -d $(top_srcdir)/.git; then \
		GIT_DIR="$(top_srcdir)/.git" git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog

-include $(top_srcdir)/git.mk
