#!/usr/bin/bash

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Fedora
requires=(
	autoconf-archive
	cairo-gobject-devel
	ccache
	clang
	clang-analyzer
	cppcheck-htmlreport
	dbus-glib-devel
	desktop-file-utils
	exempi-devel
	gcc
	git
	gobject-introspection-devel
	gtk-layer-shell-devel
	gtk3-devel
	libSM-devel
	libexif-devel
	libnotify-devel
	libselinux-devel
	libxml2-devel
	make
	mate-common
	mate-desktop-devel
	pango-devel
	python3-lxml
	redhat-rpm-config
	startup-notification-devel
)

infobegin "Update system"
dnf update -y
infoend

infobegin "Install dependency packages"
dnf install -y ${requires[@]}
infoend
