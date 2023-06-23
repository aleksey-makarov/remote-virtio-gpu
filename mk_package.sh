#!/usr/bin/env bash

set -x

PKGBUILD=$(date '+%y%m%d%H%M%S')
# SRC=$(pwd)

INSTALL_DIR=$(readlink install)
BUILDX=$(basename -s .install "${INSTALL_DIR}")

case ${BUILDX} in
	*_release)
		BUILD=${BUILDX%%_release}
		;;
	*)
		BUILD=${BUILDX}
esac

PACKAGE_NAME=remote-virtio-gpu
PACKAGE_VERSION=0.1

rm -rf "${INSTALL_DIR}/DEBIAN"
mkdir "${INSTALL_DIR}/DEBIAN"

cat <<EOF > "${INSTALL_DIR}/DEBIAN/control"
Package: ${PACKAGE_NAME}
Version: ${PACKAGE_VERSION}-${BUILD}.${PKGBUILD}
Section: base
Priority: optional
Architecture: amd64
Maintainer: Aleksei Makarov <alm@opensynergy.com>
Depends: libegl1 (>= 1.3.0)
       , libgl1-mesa-dri (>= 20.0.0)
       , libvirglrenderer1 (>= 0.8.2)
       , libwayland-egl1 (>= 1.18.0)
       , libinput10 (>= 1.15.5)
       , libgles2 (>= 1.3.0)
Description: Remote VIRTIO GPU
EOF


FILENAME=./xchg/"${PACKAGE_NAME}_${PACKAGE_VERSION}-${BUILD}.${PKGBUILD}.deb"
rm -f ./xchg/"${PACKAGE_NAME}"*.deb
dpkg-deb --build "${INSTALL_DIR}" "${FILENAME}"
