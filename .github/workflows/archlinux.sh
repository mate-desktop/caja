#!/usr/bin/bash

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Archlinux
requires=(
	autoconf-archive
	ccache
	clang
	exempi
	file
	gcc
	gcc
	git
	glib2-devel
	gobject-introspection
	gvfs
	intltool
	libexif
	libnotify
	libsm
	make
	mate-common
	mate-desktop
	which
	xorgproto
)

infobegin "Update system"
pacman --noconfirm -Syu
infoend

infobegin "Install dependency packages"
pacman --noconfirm -S ${requires[@]}
infoend
