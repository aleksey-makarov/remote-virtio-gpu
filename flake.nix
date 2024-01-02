{
  description = "Remote VIRTIO GPU";

  nixConfig.bash-prompt = "[\\033[1;33mremote-virtio-gpu\\033[0m \\w]$ ";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    nixGL = {
      url = "github:guibou/nixGL";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.flake-utils.follows = "flake-utils";
    };
    nix-vscode-extensions = {
      url = "github:nix-community/nix-vscode-extensions";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.flake-utils.follows = "flake-utils";
    };
    virtio-lo = {
      url = "github:aleksey-makarov/remote-virtio-gpu-driver/work";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    uhmitest = {
      url = "github:aleksey-makarov/uhmitest";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.flake-utils.follows = "flake-utils";
      inputs.nix-vscode-extensions.follows = "nix-vscode-extensions";
    };
    kmscube = {
      url = "github:aleksey-makarov/kmscube/work";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.flake-utils.follows = "flake-utils";
      inputs.nix-vscode-extensions.follows = "nix-vscode-extensions";
    };
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
    nixGL,
    nix-vscode-extensions,
    virtio-lo,
    uhmitest,
    kmscube,
  }: let
    system = "x86_64-linux";

    overlay = self: super: {
      linuxKernel =
        super.linuxKernel
        // {
          packagesFor = kernel_: ((super.linuxKernel.packagesFor kernel_).extend (lpself: lpsuper: {
            virtio-lo = super.callPackage virtio-lo {};
          }));
        };

      linuxHeaders_5_10 = super.linuxHeaders.overrideAttrs rec {
        version = "5.10.191";
        src = pkgs.fetchurl {
          url = "mirror://kernel/linux/kernel/v5.x/linux-${version}.tar.xz";
          hash = "sha256-y1RmDtSRfMT5qauT0Rfe/v2Ly+dF7GCC2Qm7/VrpYsI=";
        };
      };

      remote-virtio-gpu = super.callPackage ./default.nix {};

      remote-virtio-gpu-linux_5_10 = self.remote-virtio-gpu.override {
        linuxHeaders = self.linuxHeaders_5_10;
      };

      remote-virtio-gpu-debug = self.remote-virtio-gpu.overrideAttrs (_: _: {
        cmakeBuildType = "Debug";
        separateDebugInfo = true;
      });

      uhmitest = uhmitest.packages.${system}.uhmitest;

      kmscube = kmscube.packages.${system}.kmscube;

      glmark2 = super.glmark2.overrideAttrs (attrs: {
        patches = (attrs.patches or []) ++ [./settings/nix/0001-drm-Use-preferred-mode-if-exist.patch];
      });
    };

    pkgs = (nixpkgs.legacyPackages.${system}.extend overlay).extend nixGL.overlay;

    extensions = nix-vscode-extensions.extensions.${system};

    inherit (pkgs) vscode-with-extensions vscodium;

    vscode = vscode-with-extensions.override {
      vscode = vscodium;
      vscodeExtensions = [
        extensions.vscode-marketplace.ms-vscode.cpptools
        extensions.vscode-marketplace.github.vscode-github-actions
        extensions.vscode-marketplace.bbenoist.nix
      ];
    };

    nixos = pkgs.nixos (import ./settings/nix/configuration.nix);

    startvm_sh = pkgs.writeScriptBin "startvm.sh" ''
      #!${pkgs.bash}/bin/bash
      ${pkgs.coreutils}/bin/mkdir -p ./xchg

      TMPDIR=''$(pwd)
      USE_TMPDIR=1
      export TMPDIR USE_TMPDIR

      TTY_FILE="./xchg/tty.sh"
      read -r rows cols <<< "''$(${pkgs.coreutils}/bin/stty size)"

      cat << EOF > "''${TTY_FILE}"
      export TERM=xterm-256color
      stty rows ''$rows cols ''$cols
      reset
      EOF

      ${pkgs.coreutils}/bin/stty intr ^] # send INTR with Control-]
      ${pkgs.nixgl.nixGLMesa}/bin/nixGLMesa ${nixos.vm}/bin/run-nixos-vm
      ${pkgs.coreutils}/bin/stty intr ^c
    '';

    renderer_h = 800;
    renderer_w = 1280;
    wayland_socket_name = "wayland-nix";

    startrend_sh = pkgs.writeScriptBin "startrend.sh" ''
      #!${pkgs.bash}/bin/bash

      set -x

      XDG_RUNTIME_DIR=$(${pkgs.coreutils}/bin/mktemp -d /tmp/xdg-runtime.XXXXXXXX)
      export XDG_RUNTIME_DIR
      echo "XDG_RUNTIME_DIR: ''$XDG_RUNTIME_DIR"

      on_exit() {
        if [ -n ''${RENDERER_PID+x} ]; then
            kill "''${RENDERER_PID}"
        fi
        echo "rm -rf ''${XDG_RUNTIME_DIR}"
        ${pkgs.coreutils}/bin/rm -rf "''${XDG_RUNTIME_DIR}"
      }

      echo "setting a trap"
      trap 'on_exit' EXIT

      echo "starting weston"
      ${pkgs.nixgl.nixGLMesa}/bin/nixGLMesa ${pkgs.weston}/bin/weston --width 1920 --height 1080 -S "${wayland_socket_name}" &

      echo "sleep"
      sleep 1

      echo "running app"
      export WAYLAND_DISPLAY="${wayland_socket_name}"

      # One fullscreen
      # ${pkgs.nixgl.nixGLMesa}/bin/nixGLMesa ${pkgs.remote-virtio-gpu}/bin/rvgpu-renderer -b ${builtins.toString renderer_w}x${builtins.toString renderer_h}@0,0 -p 55667

      # Two half-scrren
      ${pkgs.nixgl.nixGLMesa}/bin/nixGLMesa ${pkgs.remote-virtio-gpu}/bin/rvgpu-renderer -b 640x800@0,0   -p 55667 &
      RENDERER_PID=$!
      ${pkgs.nixgl.nixGLMesa}/bin/nixGLMesa ${pkgs.remote-virtio-gpu}/bin/rvgpu-renderer -b 640x800@640,0 -p 55668
    '';
  in {
    packages.${system} = rec {
      inherit (pkgs) linuxHeaders_5_10 remote-virtio-gpu remote-virtio-gpu-linux_5_10 remote-virtio-gpu-debug;

      default = remote-virtio-gpu;
    };

    devShells.${system} = rec {
      remote-virtio-gpu = pkgs.mkShell {
        packages = [vscode];
        # inputsFrom = [pkgs.remote-virtio-gpu-linux_5_10];
        inputsFrom = [pkgs.remote-virtio-gpu];
        shellHook = ''
          echo "gst: ${pkgs.gst_all_1.gstreamer}"
          echo "kernel version: ${pkgs.linuxPackages.kernel.modDirVersion}"
          echo "linuxHeaders: ${pkgs.linuxHeaders}"
          echo "linuxHeaders_5_10: ${self.packages.${system}.linuxHeaders_5_10}"
        '';
      };
      default = remote-virtio-gpu;
    };

    apps.${system} = rec {
      startvm = {
        type = "app";
        program = "${startvm_sh}/bin/startvm.sh";
      };
      startrend = {
        type = "app";
        program = "${startrend_sh}/bin/startrend.sh";
      };
      codium = {
        type = "app";
        program = "${vscode}/bin/codium";
      };
      default = startvm;
    };
  };
}
