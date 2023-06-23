#!/usr/bin/env bash

YELLOW=$(tput setaf 3)
RESET=$(tput sgr0)

function info()
{
	msg="$1"
	echo
	echo -e "${YELLOW}${msg}${RESET}"
	echo
}

function build_all()
{
	info "build remote-virtio-gpu"
	nix build

	info "build remote-virtio-gpu-linux_5_10"
	nix build .#remote-virtio-gpu-linux_5_10 -o result_5_10

	info "build remote-virtio-gpu-debug"
	nix build .#remote-virtio-gpu-debug -o result_debug

	info "build remote-virtio-gpu-debug.debug"
	nix build .#remote-virtio-gpu-debug.debug -o result_debug

	info "build native release"
	./build_release.sh

	# info "build deb"
	# ./mk_package.sh

	info "build native debug"
	./build_debug.sh
}

build_all
