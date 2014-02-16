include $(top_srcdir)/Makefile.shared

SUBDIRS = \
	eel \
	libcaja-extension \
	cut-n-paste-code \
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
	MAINTAINERS \
	README.commits \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	Makefile.shared \
	add-include-prefix \
	$(NULL)

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update

DISTCHECK_CONFIGURE_FLAGS = --disable-update-mimedb --disable-icon-update --enable-gtk-doc

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

distclean-local:
	if test "$(srcdir)" = "."; then :; else \
		rm -f ChangeLog; \
	fi

.PHONY: ChangeLog
