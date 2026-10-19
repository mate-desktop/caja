#!/bin/sh

if [ -z "$DESTDIR" ]; then
    echo "Compiling GSettings schemas..."
    glib-compile-schemas "${MESON_INSTALL_PREFIX}/share/glib-2.0/schemas" || true
fi

UPDATE_MIMEDB="${1:-}"
UPDATE_ICON="${2:-}"
UPDATE_DESKTOP="${3:-}"

if [ "$UPDATE_MIMEDB" = "yes" ] && [ -z "$DESTDIR" ]; then
    if command -v update-mime-database >/dev/null 2>&1; then
        echo "Updating MIME database..."
        update-mime-database "${MESON_INSTALL_PREFIX}/share/mime" || true
    fi
fi

if [ "$UPDATE_ICON" = "yes" ] && [ -z "$DESTDIR" ]; then
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        echo "Updating icon cache..."
        gtk-update-icon-cache -qtf "${MESON_INSTALL_PREFIX}/share/icons/hicolor" || true
    fi
fi

if [ -z "$DESTDIR" ]; then
    if command -v update-desktop-database >/dev/null 2>&1; then
        echo "Updating desktop database..."
        update-desktop-database -q "${MESON_INSTALL_PREFIX}/share/applications" || true
    fi
fi
