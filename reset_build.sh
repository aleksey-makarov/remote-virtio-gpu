#!/usr/bin/env bash

set -x

BUILD_DIR=./xchg/remote-virtio-gpu

rm -rf "${BUILD_DIR}"
git clone . "${BUILD_DIR}"
ln -fs -T .. ${BUILD_DIR}/xchg

# cat << EOF > "${BUILD_DIR}"/build.sh
# #!/usr/bin/env bash
# set -x
# mkdir build
# mkdir install
# cmake -D CMAKE_INSTALL_PREFIX=install -D CMAKE_BUILD_TYPE=Debug -D VIRTIO_LO_DIR="\$(pwd)"/src/rvgpu-driver-linux -S . -B build
# cd build
# cmake --build   . --verbose
# cmake --install . --verbose
# EOF
# chmod +x "${BUILD_DIR}"/build.sh

cat << EOF > "${BUILD_DIR}"/prepare.sh
sudo apt install cmake pkg-config libvirglrenderer-dev libegl-dev libgles-dev libwayland-dev libgbm-dev libdrm-dev libinput-dev
EOF

chmod +x "${BUILD_DIR}"/prepare.sh
