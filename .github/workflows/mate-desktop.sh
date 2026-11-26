#!/usr/bin/bash

set -e
set -o pipefail

NAME="mate-desktop"
TEMP_DIR=$(mktemp -d)
OS=$(cat /etc/os-release | grep ^ID | head -n 1 | awk -F= '{ print $2}')
TAG=$1
CACHE_DIR=$2

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages to build mate-desktop
# https://gitlab.archlinux.org/archlinux/packaging/packages/mate-desktop
arch_requires=(
	autoconf-archive
	gobject-introspection
	mate-common
	intltool
)

# https://salsa.debian.org/debian-mate-team/mate-desktop/-/blob/master/debian/control
debian_requires=(
	autoconf-archive
	autopoint
	gobject-introspection
	gtk-doc-tools
	intltool
	iso-codes
	libdconf-dev
	libgdk-pixbuf-2.0-dev
	libgirepository1.0-dev
	libglib2.0-dev
	libglib2.0-doc
	libgtk-3-dev
	libgtk-3-doc
	librsvg2-bin
	libstartup-notification0-dev
	libx11-dev
	libxml2-dev
	libxrandr-dev
	mate-common
)

# https://src.fedoraproject.org/rpms/mate-desktop/blob/rawhide/f/mate-desktop.spec
fedora_requires=(
	dconf-devel
	desktop-file-utils
	gobject-introspection-devel
	make
	mate-common
	startup-notification-devel
	gtk3-devel
	iso-codes-devel
	gobject-introspection-devel
	cairo-gobject-devel
)

# https://git.launchpad.net/ubuntu/+source/mate-desktop/tree/debian/control
ubuntu_requires=(
	autoconf-archive
	autopoint
	gobject-introspection
	gtk-doc-tools
	intltool
	iso-codes
	libdconf-dev
	libgdk-pixbuf-2.0-dev
	libgirepository1.0-dev
	libglib2.0-dev
	libglib2.0-doc
	libgtk-3-dev
	libgtk-3-doc
	librsvg2-bin
	libstartup-notification0-dev
	libx11-dev
	libxml2-dev
	libxrandr-dev
	mate-common
)

requires=$(eval echo '${'"${OS}_requires[@]}")

infobegin "Install Depends for mate-desktop"
case ${OS} in
arch)
	pacman --noconfirm -Syu
	pacman --noconfirm -S ${requires[@]}
	;;
debian | ubuntu)
	apt-get update -qq
	env DEBIAN_FRONTEND=noninteractive \
		apt-get install --assume-yes --no-install-recommends ${requires[@]}
	;;
fedora)
	dnf update -y
	dnf install -y ${requires[@]}
	;;
esac
infoend

# Use cached packages first
if [ -f $CACHE_DIR/${NAME}-${TAG}.tar.xz ]; then
	echo "Found cache package, reuse it"
	tar -C / -Jxf $CACHE_DIR/${NAME}-${TAG}.tar.xz
else
	git clone --recurse-submodules https://github.com/mate-desktop/${NAME}

	# Foldable output information
	infobegin "Configure"
	cd ${NAME}
	git checkout v${TAG}
	if [[ ${OS} == "debian" || ${OS} == "ubuntu" ]]; then
		./autogen.sh --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu --libexecdir=/usr/lib/x86_64-linux-gnu || {
			cat config.log
			exit 1
		}
	else
		./autogen.sh --prefix=/usr || {
			cat config.log
			exit 1
		}
	fi
	infoend

	infobegin "Build"
	make -j ${JOBS}
	infoend

	infobegin "Install"
	make install
	infoend

	# Cache this package version
	infobegin "Cache"
	[ -d ${CACHE_DIR} ] || mkdir -p ${CACHE_DIR}
	make install DESTDIR=${TEMP_DIR}
	cd $TEMP_DIR
	tar -J -cf $CACHE_DIR/${NAME}-${TAG}.tar.xz *
	infoend
fi
