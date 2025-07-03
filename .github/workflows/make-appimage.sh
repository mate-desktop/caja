#!/bin/sh

set -ex

ARCH="$(uname -m)"
VERSION="$(echo "$GITHUB_SHA" | cut -c 1-9)"
UPINFO="gh-releases-zsync|$(echo $GITHUB_REPOSITORY | tr '/' '|')|latest|*$ARCH.AppImage.zsync"
RUNTIME="https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-$ARCH"
ICON="https://github.com/mate-desktop/mate-icon-theme/raw/master/mate/256x256/apps/system-file-manager.png"

# Add appimage deployment dependencies
git clone 'https://gitlab.archlinux.org/archlinux/packaging/packages/libxml2.git' ./libxml2 && (
	cd ./libxml2
	# Install deps
	pacman -Syu --noconfirm \
	base-devel              \
	binutils                \
	git                     \
	meson                   \
	python                  \
	squashfs-tools          \
	wget                    \
	zsync

	# We need to also edit makepkg to be able to build as root
	sed -i 's|EUID == 0|EUID == 1|g' /usr/bin/makepkg
	sed -i 's|MAKEFLAGS=.*|MAKEFLAGS="-j$(nproc)"|; s|#MAKEFLAGS|MAKEFLAGS|' /etc/makepkg.conf

	# build libxml2 without linking to libicudata
	sed -i -e 's/icu=enabled/icu=disabled/' -e '/--with-icu/d' ./PKGBUILD
	cat ./PKGBUILD
	makepkg -f --skippgpcheck
	pacman -U --noconfirm ./*.pkg.tar.*
)
rm -rf ./libxml2

# Prepare AppDir
mkdir -p ./AppDir/shared/bin ./AppDir/shared/lib ./AppDir/share ./AppDir/bin
cd ./AppDir

cp -v /lib64/ld-linux*          ./bin/ld-linux.so
cp -v /usr/bin/caja*            ./shared/bin
cp -v /usr/lib/libstartup*      ./shared/lib
cp -v /usr/lib/libX11.so*       ./shared/lib
cp -v /usr/lib/libwayland-*     ./shared/lib
cp -rv /usr/lib/gvfs            ./shared/lib
cp -rv /usr/lib/gio             ./shared/lib
cp -rv /usr/lib/gtk-3.0         ./shared/lib
cp -rv /usr/lib/gdk-pixbuf-2.0  ./shared/lib
cp -rv /usr/share/caja          ./share
cp -rv /usr/share/glib-2.0      ./share
cp -rv /usr/share/libthai       ./share

cp -rv /usr/share/applications/caja.desktop ./
wget --retry-connrefused --tries=30 "$ICON" -O ./caja.png
cp -v ./caja.png ./.DirIcon

# ADD LIBRARIES
ldd /usr/bin/caja*  \
	/usr/lib/gvfs/*  \
	/usr/lib/gio/modules/* \
	/usr/lib/gtk-3.0/*/*  \
	/usr/lib/gdk-pixbuf-2.0/*/loaders/* \
	| awk -F"[> ]" '{print $4}' | xargs -I {} cp -vn {} ./shared/lib

# remove full path from gdk loaders.cache file
find ./shared/lib -type f -regex '.*gdk.*loaders.cache' \
	-exec sed -i 's|/.*lib.*/gdk-pixbuf.*/.*/loaders/||g' {} \;

( cd ./shared/lib && find ./*/* -type f -regex '.*\.so.*' -exec ln -s {} ./ \; )

# strip libs and bins
find ./shared -type f -exec strip -s -R .comment --strip-unneeded {} \;

# Caja is hardcoded to look in /usr/share/caja
# It doesn't check XDG_DATA_DIRS 🥲
# So we will fix it with a hack™
sed -i 's|/usr/share/caja|/tmp/.caja/caja|g' ./shared/bin/caja*
          
# Add dynamic linker wrapper for each binary we bundle
LD_LINUX_WRAPPER='#!/bin/sh
CURRENTDIR="$(cd "${0%/*}" && echo "$PWD")"
exec "$CURRENTDIR"/ld-linux.so \
	--library-path "$CURRENTDIR"/../shared/lib --argv0 "$0" \
	"$CURRENTDIR"/../shared/bin/"${0##*/}" "$@"'

for bin in ./shared/bin/*; do
	echo "$LD_LINUX_WRAPPER" > ./bin/"${bin##*/}"
done

# Make the AppRun and set the needed env variables
echo '#!/bin/sh

set -e

CURRENTDIR="$(cd "${0%/*}" && echo "$PWD")"
BIN="${ARGV0#./}"
unset ARGV0

# make symlink to our share dir since the caja binaries will look in /tmp/.caja
ln -sfn "$CURRENTDIR"/share /tmp/.caja

export PATH="$CURRENTDIR/bin:$PATH"
export GTK_PATH="$CURRENTDIR"/shared/lib/gtk-3.0
export GTK_EXE_PREFIX="$CURRENTDIR"/shared/lib/gtk-3.0
export GTK_DATA_PREFIX="$CURRENTDIR"/shared/lib/gtk-3.0
export GIO_MODULE_DIR="$CURRENTDIR"/shared/lib/gio/modules
export LIBTHAI_DICTDIR="$CURRENTDIR"/share/libthai
export GSETTINGS_SCHEMA_DIR="$CURRENTDIR"/share/glib-2.0/schemas
export GDK_PIXBUF_MODULEDIR="$CURRENTDIR"/shared/lib/gdk-pixbuf-2.0
export GDK_PIXBUF_MODULE_FILE="$GDK_PIXBUF_MODULEDIR"/2.10.0/loaders.cache

if [ -f "$CURRENTDIR"/bin/"$BIN" ]; then
	exec "$CURRENTDIR"/bin/"$BIN" "$@"
elif [ -f "$CURRENTDIR"/bin/"$1" ]; then
	BIN="$1"
	shift
	exec "$CURRENTDIR"/bin/"$BIN" "$@"
else
	exec "$CURRENTDIR"/bin/caja "$@"
fi' > ./AppRun

chmod +x ./AppRun ./bin/*

# MAKE APPIAMGE MANUALLY
cd ..

# Turn AppDir into a squashfs image and concatenate the runtime to it
mksquashfs ./AppDir AppDir.squashfs -comp zstd -Xcompression-level 22
wget --retry-connrefused --tries=30 "$RUNTIME" -O ./runtime

echo "Adding update information \"$UPINFO\" to runtime..."
printf "$UPINFO" > data.upd_info
objcopy --update-section=.upd_info=data.upd_info \
	--set-section-flags=.upd_info=noload,readonly ./runtime

cat ./AppDir.squashfs >> ./runtime
printf 'AI\x02' | dd of=./runtime bs=1 count=3 seek=8 conv=notrunc
mv -v ./runtime ./Caja-"$VERSION"-anylinux-"$ARCH".AppImage

echo "Generating zsync file..."
zsyncmake ./*.AppImage -u ./*.AppImage

mkdir ./dist
mv -v ./*.AppImage* ./dist

echo "All Done!"
