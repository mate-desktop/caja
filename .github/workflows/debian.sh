#!/usr/bin/bash

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Debian
requires=(
	autopoint
	ccache
	clang
	clang-tools
	cppcheck
	git
	gobject-introspection
	gtk-doc-tools
	intltool
	libdconf-dev
	libexempi-dev
	libexif-dev
	libgail-3-dev
	libgirepository1.0-dev
	libglib2.0-dev
	libgtk-3-dev
	libgtk-layer-shell-dev
	libmate-desktop-dev
	libnotify-dev
	libpango1.0-dev
	libselinux1-dev
	libstartup-notification0-dev
	libx11-dev
	libxml2-dev
	mate-common
	mate-desktop
	python3-lxml
	quilt
	shared-mime-info
	xvfb
)

infobegin "Update system"
apt-get update -qq
infoend

infobegin "Install dependency packages"
env DEBIAN_FRONTEND=noninteractive \
	apt-get install --assume-yes \
	${requires[@]}
infoend
