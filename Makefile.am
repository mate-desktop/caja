include $(top_srcdir)/Makefile.shared

if USE_NLS
PO_SUBDIR = po
endif

SUBDIRS = \
	$(PO_SUBDIR) \
	eel \
	libcaja-extension \
	mate-submodules \
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
	--enable-compile-warnings=no \
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
