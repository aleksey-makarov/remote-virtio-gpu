{
  pkgs,
  lib,
  config,
  modulesPath,
  ...
}:
with lib; {
  imports = [
    (modulesPath + "/profiles/qemu-guest.nix")
    # (modulesPath + "/profiles/minimal.nix")
    (modulesPath + "/virtualisation/qemu-vm.nix")
    # (modulesPath + "/services/misc/spice-vdagentd.nix")
    # (modulesPath + "/services/ttys/gpm.nix")
  ];

  # https://nixos.wiki/wiki/Linux_kernel

  config = {
    system.stateVersion = "23.05";
    hardware.opengl.enable = true;

    # KERNEL

    boot = {
    #      # kernelPackages = pkgs.linuxPackages_4_14;
    #      # kernelPatches = (import ./linux-patches.nix)."4_14";
    #
    #      kernelPackages = pkgs.linuxPackages_5_4;
    #      # kernelPatches = (import ./linux-patches.nix)."5_4";
    #      kernelPatches = [
    #        {
    #          name = "virgl sync drm support";
    #          patch = ./0001-drm-virtio-Add-VSYNC-support-linux-5-4.patch;
    #        }
    #      ];
    #

        extraModulePackages = with config.boot.kernelPackages; [virtio-lo];
        kernelModules = ["virtio-lo"];

        # kernelParams = [ "video=1920x1080" ];

    #      # extraModulePackages = with pkgs.linuxPackages_4_19; [ virtio-lo ];
    #
    #      # kernelParams = [ "drm.debug=0x1ff" ];
    };

    # environment.etc = {
    #   # Creates /etc/nanorc
    #   debug_txt_file = {
    #     text = ''
    #         # virtio-lo:
    #         ${config.boot.kernelPackages.virtio-lo}
    #     '';
    #     # The UNIX file mode bits
    #     mode = "0444";
    #   };
    # };

    # from profiles/minimal.nix
    documentation.enable = false;
    documentation.doc.enable = false;
    documentation.man.enable = false;
    documentation.nixos.enable = false;
    documentation.info.enable = false;
    programs.bash.enableCompletion = false;
    programs.command-not-found.enable = false;

    programs.dconf.enable = true;

    services.getty.autologinUser = "root";

    services.udev.packages = [ pkgs.remote-virtio-gpu ];

    virtualisation = {
      # diskSize = 8000; # MB
      # diskImage = "/home/amakarov/work/uhmi-nix/nixos.qcow2";
      # writableStoreUseTmpfs = false;
      memorySize = 4 * 1024;
      cores = 4;
      forwardPorts = [
        {
          from = "host";
          host.port = 10022;
          guest.port = 22;
        }
      ];
      qemu = {
        # package = pkgs.pkgs_orig.qemu;

        # consoles = [];
        # [ "console=tty1" ];

        options = [
          "-device virtio-vga-gl"
          "-display sdl,gl=on"

          # "-display sdl,gl=off"
          # "-vga none"
          # "-nographic"
          "-serial stdio"

          # "-chardev qemu-vdagent,id=ch1,name=vdagent,clipboard=on"
          # "-device virtio-serial-pci"
          # "-device virtserialport,chardev=ch1,id=ch1,name=com.redhat.spice.0"
        ];
      };
    };

    # fileSystems."/" = {
    #     device = "/dev/disk/by-label/nixos";
    #     fsType = "ext4";
    #     autoResize = true;
    # };

    security.polkit.enable = true;

    networking.firewall.enable = false;

    # boot = {
    #   growPartition = true;
    #   # kernelParams = [ "console=ttyS0 boot.shell_on_fail" ];
    #   # loader.timeout = 5;
    # };

    # services.qemuGuest.enable = true; # ???

    services.openssh.enable = true;
    services.openssh.settings.PermitRootLogin = "yes";
    # services.openssh.passwordAuthentication = true;

    # services.xserver = {
    #    enable = true;
    #    displayManager.gdm.enable = true;
    #    desktopManager.gnome.enable = true;
    # };

    # services.qemuGuest.enable = true;
    # services.spice-vdagentd.enable = true;
    # services.gpm.enable = true;

    fonts.packages = with pkgs; [
      ## good:
      # noto-fonts
      # noto-fonts-cjk
      # noto-fonts-emoji

      ## good ones:
      # fira-code
      # fira-code-symbols

      ## too dense and does not work with midnight commander
      mplus-outline-fonts.githubRelease
    ];

    environment.systemPackages = with pkgs; [
      vim
      micro
      wget
      mc
      tree
      tmux

      kmscube
      glmark2
      glxinfo
      mediainfo
      mesa-demos
      evtest

      weston
      vanilla-dmz

      remote-virtio-gpu
      # remote-virtio-gpu.src

      uhmitest

      gst_all_1.gstreamer # this is .bin (probably this is a bug)
      gst_all_1.gstreamer.out
      gst_all_1.gst-plugins-base
      gst_all_1.gst-plugins-good
      gst_all_1.gst-plugins-bad
      gst_all_1.gst-plugins-ugly
      gst_all_1.gst-devtools

      ffmpeg_6-full
    ];

    users.mutableUsers = false;

    users.users.root = {
      password = "";
      openssh.authorizedKeys.keys = ["ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABgQDYiMGe5zxNUAbYnJMRWrVfQrPxbPH77bpY3JvRTd2xM/Pdm+o6zbPYToJcDZWBUDO3XuQFCtrLuEGM5IBKlrf7JCsk/yeoCS8tcFjEJxMTE1FQVuwxOlrbNSDF2aeA9XpIPg2mL2JUBj6YOF141GWXNra1X/s6bOfAwmxgZw/RnPY7+6ZFFwTGgWniurc3oeCOdT09aX5RDIEUcnni8ye7fLNQJHv3egz62ORVswJ7CuLtVcdK6gMOVCeBC0DFPUkt0SXLUQUwU5HpWKB1Xx9EKWPmdlZk+0pXz14DgiGfseCbRDQGLqvHE7WxT/MxSHzLqicAlrXMAAdz3EsA2D1dTetb0d20PvViYkDYIa/phzdueM8RbzGaItPKffsMZx9aUMALnbEKeyNPUzfyLohrqT6yflZ1N3o6EWEGXTBpAnHEjYBgdWR4tcKyfBu6sjWzEYM0jnIXnbRPjdoPdg+JR4+S4MzoPDprB86Nr722Jg03xa+sQudS9IBgY8YvYwM= amakarov@NB-100862.open-synergy.com"];
    };
  };
}
