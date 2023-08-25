{
  stdenv,
  lib,
  libdrm,
  mesa,
  libGL,
  zlib,
  libinput,
  cmake,
  pkg-config,
  hidapi,
  wayland,
  virglrenderer,
  libpng,
  libjpeg,
  linuxPackages,
  libuuid,
}:
stdenv.mkDerivation rec {
  pname = "remote-virtio-gpu";
  version = "0.1";

  src = ./.;

  buildInputs = [
    libdrm
    mesa
    libGL
    zlib
    libinput
    hidapi
    wayland
    virglrenderer
    libpng
    libjpeg
    libuuid
  ];

  nativeBuildInputs = [
    cmake
    pkg-config
    libdrm.dev
    mesa.dev
    libGL.dev
    linuxPackages.virtio-lo.dev
    linuxPackages.kernel.dev
    libuuid.dev
  ];

  capset = ./settings/virgl.capset;

  postPatch = ''
    substituteInPlace include/rvgpu-proxy/rvgpu-proxy.h      --replace /etc/virgl.capset $capset
    substituteInPlace src/rvgpu-proxy/gpu/rvgpu-gpu-device.c --replace librvgpu.so       $out/lib/librvgpu.so
  '';

  meta = with lib; {
    description = "Remote VIRTIO GPU";
    homepage = "https://www.opensynergy.com/";
    license = licenses.mit;
    maintainers = [
      {
        email = "alm@opensynergy.com";
        name = "Aleksei Makarov";
        github = "aleksey.makarov";
        githubId = 19228987;
      }
    ];
    platforms = platforms.linux;
  };
}
