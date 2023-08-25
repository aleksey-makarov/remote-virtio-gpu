# How to test on NIX/QEMU

> This document describes how to test Remote GPU software in the Nix/QEMU environment.

### What is Nix

`Nix` is a package manager that provides reproducible environment.
[`NixOS`](https://nixos.org/) is a Linux distributive that uses `Nix` as package manager.
Flakes is a recent feature of the `Nix` package manager that simplifies configuration of
complex projects.

### Intallation

`Nix` can be installed on virtually any Linux distributive.
Follow [the link](https://nixos.org/download) (single-user installation).
After installation, enable flake support.
To do that, create file `~/.config/nix/nix.conf` or `/etc/nix/nix.conf`
and add this line to it:

    experimental-features = nix-command flakes

For the reference [look here](https://nixos.wiki/wiki/Flakes).

### How to run development environment

Clone the Remote GPU repo from GitHub, enter to the created directory and run `nix` development environment:

    git clone https://github.com/aleksey-makarov/remote-virtio-gpu.git
    cd remote-virtio-gpu
    git checkout -b for_merge_03 origin/for_merge_03
    nix develop

The last command will download the most of the required software from the `Nix` caches,
build `remote-virtio-gpu` and `virtio-lo` projects and create the environment.
After it completes you will be presented a shell environment where all the required
software is ready to use.

### How to run QEMU inside the development environment

Among others, there is `startvm.sh` script in the `PATH` of that environment.
Start it.  It creates a `QEMU` disk `nixos.qcow2` with all the required software
and starts `QEMU` with that disk.  It creates a serial shell sessioin to the
machine in the termial.  You will be authomatically logged in as `root`.
Also it creates `xchg` directory that is mounted inside `QEMU` as `/tmp/xchg`.
You can run

    . /tmp/xchg/tty.sh

to fix terminal settings inside of the serial terminal of the `QEMU`.
All the following commands should be issued inside that `QEMU` serial shell
session.

### Start remote VIRTIO GPU

Run Remote GPU software with these commands:

    rvgpu-renderer -b '1280x800@0,0' -g /dev/dri/card0 -S seat0 -p 55667 &
    rvgpu-proxy -s '1280x800@0,0' -n '127.0.0.1:55667' &

You will get green screen on the `QEMU` window.

### Run uhmitest

Run `uhmitest` test with this command:

    LIBUHMIGL_DEVICE_NAME=/dev/dri/card1 uhmitest

You will get the fabulous gears on the `QEMU` window.  Kill it with `Ctrl-C`.

### Run kmscube

Run `kmscube` test with this command:

    kmscube -D /dev/dri/card1

As this test procedure does not support the sync feature so far, the cube
will move too quick (see the [Restrictions]() section)

The same with video textures:

    kmscube -D /dev/dri/card1 -V /tmp/xchg/video_test_M_800x450.mp4

You can use virtually any video here.  This is the format of the video that was
checked:

    video_test_M_800x450.mp4: ISO Media, MP4 Base Media v1 [IS0 14496-12:2003]

### Run glmark2

    glmark2-es2-drm --visual-config id=62 --winsys-options=drm-device=/dev/dri/card1

### Run GStreamer

    gst-launch-1.0 -v videotestsrc ! videobox autocrop=true ! kmssink force-modesetting=true bus-id='virtio-lo.0'
    gst-launch-1.0 -v filesrc location=/tmp/xchg/video_test_M_800x450.mp4 ! decodebin ! videoconvert ! videoscale ! videobox ! "video/x-raw, width=1280, height=800" ! kmssink driver-name=virtio_gpu force-modesetting=true  bus-id='virtio-lo.0'

### Run weston

    export XDG_RUNTIME_DIR=/tmp
    weston --seat=seat_virtual -S wayland-uhmi &
    export WAYLAND_DISPLAY=wayland-uhmi

After these commands there will be `Weston` display manager running in the `QEMU` window.
All the following commands could be run in a new terminal in Weston or in the
`QEMU` serial session in the development terminal.

### Run ffmpeg

    ffplay /tmp/xchg/video_test_M_800x450.mp4

### Run GStreamer

    gst-launch-1.0 -v videotestsrc ! glimagesink
    gst-launch-1.0 -v filesrc location='/tmp/xchg/video_test_M_800x450.mp4' ! decodebin ! glimagesink

### Run glmark under Wayland

    glmark-es2-wayland

### Restrictions

Some features are not tested with this document.  It will be fixed soon:

- `rvgpu-renderer` works with GBM backend.  The document will be enhanced to explain how to test it
  with the Wayland backend.

- The sync feature is not tested.  Testing it requires building custom kernel.
  The description of how to do that will be merged soon.
