#!/bin/sh

set -ex

ARCH="$(uname -m)"
VERSION="$(echo "$GITHUB_SHA" | cut -c 1-9)"
URUNTIME="https://github.com/VHSgunzo/uruntime/releases/latest/download/uruntime-appimage-dwarfs-$ARCH"
URUNTIME_LITE="https://github.com/VHSgunzo/uruntime/releases/latest/download/uruntime-appimage-dwarfs-lite-$ARCH"
UPINFO="gh-releases-zsync|$(echo $GITHUB_REPOSITORY | tr '/' '|')|latest|*$ARCH.AppImage.zsync"
SHARUN="https://github.com/VHSgunzo/sharun/releases/latest/download/sharun-$ARCH-aio"
ICON="https://github.com/mate-desktop/mate-icon-theme/raw/master/mate/256x256/apps/system-file-manager.png"

# Add appimage deployment dependencies
pacman -Syu --noconfirm wget xorg-server-xvfb zsync

# Use libxml2 that doesn't link to 30 MiB libicudata lib
case "$ARCH" in
  'x86_64')  PKG_TYPE='x86_64.pkg.tar.zst';;
  'aarch64') PKG_TYPE='aarch64.pkg.tar.xz';;
esac
LIBXML_URL="https://github.com/pkgforge-dev/llvm-libs-debloated/releases/download/continuous/libxml2-iculess-$PKG_TYPE"
wget --retry-connrefused --tries=30 "$LIBXML_URL" -O ./libxml2.pkg.tar.zst
pacman -U --noconfirm ./libxml2.pkg.tar.zst
rm -f ./libxml2.pkg.tar.zst

# For some reason mesa gets dlopened during bundling, which is not needed at all for caja to work
# so we will remove it to prevent all of mesa and its dependencies from being bundled
pacman -Rsndd --noconfirm mesa || true

# Prepare AppDir
mkdir -p ./AppDir/share && (
	cd ./AppDir

	cp -rv /usr/share/caja ./share

	cp -rv /usr/share/applications/caja.desktop ./
	wget --retry-connrefused --tries=30 "$ICON" -O ./caja.png
	cp -v ./caja.png ./.DirIcon

	# ADD LIBRARIES
	wget --retry-connrefused --tries=30 "$SHARUN" -O ./sharun-aio
	chmod +x ./sharun-aio
	xvfb-run -a \
		./sharun-aio l -p -v -e -s -k \
		/usr/bin/caja*                \
		/usr/lib/gvfs/*               \
		/usr/lib/gio/modules/*        \
		/usr/lib/gtk-3.0/*/*          \
		/usr/lib/gdk-pixbuf-*/*/loaders/*
	rm -f ./sharun-aio
	ln ./sharun ./AppRun
	./sharun -g

	# Caja is hardcoded to look in /usr/share/caja
	# It doesn't check XDG_DATA_DIRS 🥲
	# So we will fix it with a hack™
	sed -i 's|/usr/share/caja|././/share/caja|g' ./shared/bin/caja*
	echo 'SHARUN_WORKING_DIR=${SHARUN_DIR}' > ./.env
)

# MAKE APPIAMGE WITH URUNTIME
wget --retry-connrefused --tries=30 "$URUNTIME"      -O ./uruntime
wget --retry-connrefused --tries=30 "$URUNTIME_LITE" -O ./uruntime-lite
chmod +x ./uruntime*

# Add udpate info to runtime
echo "Adding update information \"$UPINFO\" to runtime..."
./uruntime-lite --appimage-addupdinfo "$UPINFO"

echo "Generating AppImage..."
./uruntime --appimage-mkdwarfs -f \
	--set-owner 0 --set-group 0          \
	--no-history --no-create-timestamp   \
	--compression zstd:level=22 -S26 -B8 \
	--header uruntime-lite               \
	-i ./AppDir -o ./Caja-"$VERSION"-anylinux-"$ARCH".AppImage

echo "Generating zsync file..."
zsyncmake ./*.AppImage -u ./*.AppImage

mkdir ./dist
mv -v ./*.AppImage* ./dist

echo "All Done!"
