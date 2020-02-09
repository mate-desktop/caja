AC_PREREQ(2.54)

dnl ===========================================================================

m4_define(gdk-pixbuf_minver,           2.36.5)
m4_define(glib_minver,                 2.58.1)
m4_define(gio_minver,                  2.50.0)
m4_define(mate_desktop_minver,         1.17.3)
m4_define(pango_minver,                1.1.2)
m4_define(gtk_minver,                  3.22.0)
m4_define(xml_minver,                  2.4.7)
m4_define(exif_minver,                 0.6.14)
m4_define(exempi_minver,               1.99.5)
m4_define(gail_minver,                 3.0.0)
m4_define(notify_minver,               0.7.0)


dnl 1. If the library code has changed at all since last release, then increment revision.
dnl 2. If any interfaces have been added, then increment current and set revision to 0.
dnl Interface break is not allowed.
m4_define(caja_extension_current,  5)
m4_define(caja_extension_revision, 0)

AC_INIT([caja], [1.24.0], [https://mate-desktop.org])

dnl ---------------------------------------------------------------------------

AC_CONFIG_SRCDIR(src)
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.9 tar-ustar dist-xz no-dist-gzip check-news])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_MAINTAINER_MODE
AC_SUBST([ACLOCAL_AMFLAGS], ["\${ACLOCAL_FLAGS}"])

AC_SUBST(GDKPIXBUF_REQUIRED, [gdk-pixbuf_minver])
AC_SUBST(GLIB_REQUIRED, [glib_minver])
AC_SUBST(GIO_REQUIRED, [gio_minver])
AC_SUBST(MATE_DESKTOP_REQUIRED, [mate_desktop_minver])
AC_SUBST(PANGO_REQUIRED, [pango_minver])
AC_SUBST(GTK_REQUIRED, [gtk_minver])
AC_SUBST(XML_REQUIRED, [xml_minver])
AC_SUBST(GAIL_REQUIRED, [gail_minver])

dnl We need to decrement current by one in the calculation of the age because
dnl the library was started with version "1:0:0" instead of "0:0:0"
AC_SUBST(CAJA_EXTENSION_VERSION_INFO, [caja_extension_current]:[caja_extension_revision]:`expr [caja_extension_current] - 1`)

AC_C_BIGENDIAN
AC_C_CONST
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_DISABLE_STATIC
AM_PROG_LIBTOOL
PKG_PROG_PKG_CONFIG
dnl Give error and exit if we dont have pkgconfig
if test "x$PKG_CONFIG" = "x"; then
  AC_MSG_ERROR([you need to have pkgconfig installed!])
fi
AC_PATH_PROG([GLA11Y], [gla11y], [true])

AC_CHECK_LIB(m, floor)

PKG_CHECK_MODULES(ALL, [
    gdk-pixbuf-2.0 >= gdk-pixbuf_minver
    glib-2.0 >= glib_minver
    mate-desktop-2.0 >= mate_desktop_minver
    gthread-2.0
    gio-unix-2.0
    gio-2.0 >= gio_minver
    pango >= pango_minver
    gtk+-3.0 >= gtk_minver
    libnotify
    libxml-2.0 >= xml_minver
    gail-3.0 >= gail_minver
])
dnl ==========================================================================

GETTEXT_PACKAGE=caja
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",
    [the gettext translation domain])

AH_TEMPLATE([ENABLE_INSTALLER])
AH_TEMPLATE([ENABLE_NLS])
AH_TEMPLATE([HAVE_CATGETS])
AH_TEMPLATE([HAVE_LC_MESSAGES])
AH_TEMPLATE([HAVE_LIBBZ2])
AH_TEMPLATE([HAVE_LIBJPEG])
AH_TEMPLATE([HAVE_MEDUSA])
AH_TEMPLATE([HAVE_STPCPY])
AH_TEMPLATE([bzclose])
AH_TEMPLATE([bzCompress])
AH_TEMPLATE([bzCompressInit])
AH_TEMPLATE([bzDecompress])
AH_TEMPLATE([bzDecompressInit])
AH_TEMPLATE([bzdopen])
AH_TEMPLATE([bzerror])
AH_TEMPLATE([bzflush])
AH_TEMPLATE([bzopen])
AH_TEMPLATE([bzread])
AH_TEMPLATE([bzwrite])

AC_DEFINE([HAVE_GTK_MULTIHEAD], [], [needed for egg-screen-exec functions])

AH_TEMPLATE([HAVE_STARTUP_NOTIFICATION])
AH_TEMPLATE([HAVE_EXIF])

AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT([external])

dnl ==========================================================================

GTK_DOC_CHECK([1.4])

dnl ==========================================================================

AC_CHECK_PROGS(PERL, perl5 perl)
AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)

dnl ==========================================================================

AC_CHECK_HEADERS(sys/mount.h sys/vfs.h sys/param.h malloc.h)
AC_CHECK_FUNCS(mallopt)

dnl ==========================================================================

AM_CONDITIONAL(HAVE_EXIF, false)

dnl libexif checking

PKG_CHECK_MODULES(EXIF, libexif >= exif_minver,
    [AM_CONDITIONAL(HAVE_EXIF, true)
     AC_DEFINE(HAVE_EXIF, 1, [Define to enable EXIF support])
    ],
    [AM_CONDITIONAL(HAVE_EXIF, false)])

AC_SUBST(EXIF_CFLAGS)
AC_SUBST(EXIF_LIBS)

dnl ==========================================================================

dnl exempi checking

AM_CONDITIONAL(HAVE_EXEMPI, false)

AC_ARG_ENABLE(xmp,
    AC_HELP_STRING([--disable-xmp],
                   [build without xmp support]))
msg_xmp=no
if test "x$enable_xmp" != "xno"; then
    PKG_CHECK_MODULES(EXEMPI, exempi-2.0 >= exempi_minver,
        [AM_CONDITIONAL(HAVE_EXEMPI, true)
         AC_DEFINE(HAVE_EXEMPI, 1, [Define to enable xmp support])]
        msg_xmp=yes,
        [AM_CONDITIONAL(HAVE_EXEMPI, false)])

    AC_SUBST(EXEMPI_CFLAGS)
    AC_SUBST(EXEMPI_LIBS)
fi

dnl ==========================================================================

dnl ****************************
dnl *** Check for libselinux ***
dnl ****************************

SELINUX_LIBS=
msg_selinux=no
AC_CHECK_LIB(selinux, is_selinux_enabled,
   [AC_CHECK_HEADERS(selinux/selinux.h,
     [AC_SEARCH_LIBS(selinux_raw_to_trans_context, selinux,
       [AC_DEFINE(HAVE_SELINUX, 1, [Define to 1 if libselinux is available])
        SELINUX_LIBS="-lselinux"
        msg_selinux=yes])
     ])
   ])
AC_SUBST(SELINUX_LIBS)


AC_ARG_ENABLE(empty_view,
 AC_HELP_STRING([--enable-empty-view], [Enable empty view]),
 [ENABLE_EMPTY_VIEW=1
  AC_DEFINE(ENABLE_EMPTY_VIEW, 1, [define to enable the empty view that is used for performance measurement])])

AC_SUBST(ENABLE_EMPTY_VIEW)
AM_CONDITIONAL(ENABLE_EMPTY_VIEW, test "x$ENABLE_EMPTY_VIEW" = "x1")

dnl ==========  EEL/CAJA OMIT SELF CHECK  ====================================
AC_ARG_ENABLE(self-check,
    AC_HELP_STRING([--disable-self-check], [build without self check]))
msg_self_check=yes
if test "x$enable_self_check" = "xno"; then
    msg_self_check=no
    AC_DEFINE(EEL_OMIT_SELF_CHECK, 1, [define to disable eel self check])
    AC_DEFINE(CAJA_OMIT_SELF_CHECK, 1, [define to disable caja self check])
fi
AM_CONDITIONAL(ENABLE_SELF_CHECK, test "x$msg_self_check" = "xyes")

dnl ==========================================================================

AC_ARG_ENABLE(packagekit,
    AC_HELP_STRING([--disable-packagekit],
        [build without PackageKit support]))
msg_packagekit=no
if test "x$enable_packagekit" != "xno"; then
    msg_packagekit=yes
    AC_DEFINE(ENABLE_PACKAGEKIT, 1, [define to enable PackageKit mimetype installer])
fi

# ==========================================================================
# Turn on the additional warnings last, so -Werror doesn't affect other tests.

WARNING_CFLAGS=""

AC_ARG_ENABLE(more-warnings,
AC_HELP_STRING([--enable-more-warnings], [Maximum compiler warnings]),
set_more_warnings="$enableval",[
if test -f $srcdir/CVSVERSION; then
    is_cvs_version=true
    set_more_warnings=yes
else
    set_more_warnings=no
fi
])
AC_MSG_CHECKING(for more warnings, including -Werror)
if test "$GCC" = "yes" -a "$set_more_warnings" != "no"; then
    AC_MSG_RESULT(yes)
    WARNING_CFLAGS="\
    -Wall \
    -Wmissing-declarations -Wmissing-prototypes \
    -Wnested-externs -Wpointer-arith \
    -Wcast-align \
    -Werror"

    for option in -Wstrict-aliasing=0 -Wno-pointer-sign; do
        SAVE_CFLAGS="$CFLAGS"
        CFLAGS="$CFLAGS $option"
        AC_MSG_CHECKING([whether gcc understands $option])
        AC_TRY_COMPILE([], [],
            has_option=yes,
            has_option=no,)
        if test $has_option = yes; then
            WARNING_CFLAGS="$WARNING_CFLAGS $option"
        fi
        AC_MSG_RESULT($has_option)
        CFLAGS="$SAVE_CFLAGS"
        unset has_option
        unset SAVE_CFLAGS
    done
    unset option
else
    AC_MSG_RESULT(no)
fi

AC_SUBST(WARNING_CFLAGS)

dnl ==========================================================================

dnl libegg
LIBEGG_MODULES="gtk+-3.0"
LIBEGG_CFLAGS="`$PKG_CONFIG --cflags $LIBEGG_MODULES`"
AC_SUBST(LIBEGG_CFLAGS)
LIBEGG_LIBS="`$PKG_CONFIG --libs $LIBEGG_MODULES`"
AC_SUBST(LIBEGG_LIBS)

dnl libcaja-extension
LIBCAJA_EXTENSION_MODULES="glib-2.0 gtk+-3.0"
LIBCAJA_EXTENSION_CFLAGS="`$PKG_CONFIG --cflags $LIBCAJA_EXTENSION_MODULES`"
AC_SUBST(LIBCAJA_EXTENSION_CFLAGS)
LIBCAJA_EXTENSION_LIBS="`$PKG_CONFIG --libs $LIBCAJA_EXTENSION_MODULES`"
AC_SUBST(LIBCAJA_EXTENSION_LIBS)

dnl core caja
PKG_CHECK_MODULES(GMODULE, gmodule-2.0, [GMODULE_ADD="gmodule-2.0"],[GMODULE_ADD=""])
CORE_MODULES="glib-2.0 $GMODULE_ADD mate-desktop-2.0 gthread-2.0 gio-2.0 gio-unix-2.0 gail-3.0 libxml-2.0 $EXTRA_CORE_MODULES gtk+-3.0 x11"
CORE_CFLAGS="`$PKG_CONFIG --cflags $CORE_MODULES`"
AC_SUBST(CORE_CFLAGS)
CORE_LIBS="`$PKG_CONFIG --libs $CORE_MODULES`"
AC_SUBST(CORE_LIBS)

DISABLE_DEPRECATED_CFLAGS="-DG_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED"
AC_SUBST(DISABLE_DEPRECATED_CFLAGS)

dnl Multimedia keys
AC_CHECK_HEADERS([X11/XF86keysym.h])

dnl ==============================================
dnl Special GSettings section
dnl ==============================================

GLIB_GSETTINGS

##################################################
# Check for introspection
##################################################
GOBJECT_INTROSPECTION_CHECK([0.6.4])

dnl ==========================================================================

AC_PATH_PROG(UPDATE_MIME_DATABASE, update-mime-database, no)

AC_ARG_ENABLE(update-mimedb,
   AC_HELP_STRING([--disable-update-mimedb],
                   [disable the update-mime-database after install [default=no]]),,
    enable_update_mimedb=yes)
AM_CONDITIONAL(ENABLE_UPDATE_MIMEDB, test x$enable_update_mimedb = xyes)

AC_ARG_ENABLE(icon-update, AC_HELP_STRING([--disable-icon-update],
                                        [Disable icon cache update]))
if (test "$enable_icon_update" != no); then
        AC_PATH_PROG(UPDATE_ICON_CACHE, [gtk-update-icon-cache])
fi
AM_CONDITIONAL([ICON_UPDATE], [test -n "$UPDATE_ICON_CACHE"])

AC_CONFIG_FILES([
Makefile
libegg/Makefile
data/Makefile
data/icons/Makefile
data/patterns/Makefile
docs/Makefile
docs/reference/Makefile
docs/reference/libcaja-extension/Makefile
docs/reference/libcaja-extension/version.xml
eel/Makefile
icons/Makefile
libcaja-private/Makefile
libcaja-extension/Makefile
libcaja-extension/libcaja-extension.pc
libcaja-extension/libcaja-extension-uninstalled.pc
po/Makefile.in
src/Makefile
src/file-manager/Makefile
test/Makefile
])

AC_OUTPUT

dnl ==========================================================================

echo "
caja-$VERSION:

    prefix:                       ${prefix}
    source code location:         ${srcdir}
    compiler:                     ${CC}
    compiler flags:               ${CFLAGS}
    warning flags:                ${WARNING_CFLAGS}
    xmp support:                  $msg_xmp
    PackageKit support:           $msg_packagekit
    Self check:                   $msg_self_check

    caja-extension documentation: ${enable_gtk_doc}
    caja-extension introspection: ${found_introspection}
"
