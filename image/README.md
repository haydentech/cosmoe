# Cosmoe Ubuntu autoinstall image

This directory is used by `make image` to build a "seed" ISO for autoinstalling a
minimal Ubuntu Server 26.04 VM that boots directly into Cosmoe.

The target is Ubuntu 26.04 LTS Server (`ubuntu-26.04.*-live-server-amd64.iso`).
It uses the `ubuntu-server-minimal` source, not Ubuntu Desktop. When changing
the release or ISO, inspect the ISO's `casper/install-sources.yaml` and update
`autoinstall.yaml` if the minimal source ID differs.

Important: Build the Cosmoe .deb packages on Ubuntu 26.04, so the runtime library
dependencies match the image.

## What is installed

The installed system will contain the Ubuntu Server minimal base plus:

- Cosmoe runtime packages `cosmoe` and `cosmoe-apps`
- labwc and Xwayland
- Xcursor themes for visible pointer cursors
- Mesa userspace drivers and `mesa-utils`
- D-Bus, PipeWire/WirePlumber, portals, polkit agent, and mako required by the
  current `cosmoe-desktop.sh` session launcher
- NetworkManager and OpenSSH for administration
- greetd, configured to auto-start the Cosmoe desktop as user `cosmoe`

If the session exits for some reason, greetd fallsback to a text login on VT1.

## Build the seed ISO

On the Linux build host, install the normal Cosmoe Debian build dependencies
and an ISO authoring tool, for example:

```bash
sudo apt install xorriso openssl
```

Autoinstall requires an encrypted password. `make image` generates a fresh hash for
the development password `cosmoe`, builds the existing runtime `.deb` packages,
and creates the seed ISO:

```bash
make image
```

The result is `image/out/cosmoe-autoinstall.iso`.

## VirtualBox installation

1. Create a new 64-bit Ubuntu/Linux VM with at least 2 CPUs, 4 GiB RAM, and a
   24 GiB dynamically allocated virtual disk. Use the `VMSVGA` graphics
   controller and disable 3D acceleration.
2. Attach an Ubuntu 26.04 LTS live-server ISO as the first optical disk and
   `image/out/cosmoe-autoinstall.iso` as a second optical disk.
3. Boot the VM. At the Ubuntu boot menu, edit the install entry and append
   `autoinstall` to the Linux kernel command line, then boot it. The `CIDATA`
   volume label on the second ISO lets NoCloud discover `user-data`.  If you
   forget to do this or miss the window to make the change, don't worry.
   The installer will detect the disc and prompt you later in the install
   to confirm that the autoinstall is what you want.
4. The installer downloads the listed packages, installs the local Cosmoe
   packages from the seed ISO, and powers off when finished.
5. Detach both optical disks and start the VM. It should automatically start
   the Cosmoe desktop.

This method expects networking during installation because Ubuntu packages are
installed from Ubuntu repositories.

## Troubleshooting

- If the installer does not start automatically, confirm `autoinstall` was
  appended to the Ubuntu boot entry and that the seed ISO is attached.
- If Subiquity reports that the installation source is unknown, check
  `casper/install-sources.yaml` in the exact Ubuntu Server ISO and update
  `source.id`.
- In VirtualBox VM settings, confirm `VMSVGA` video, 128 MB video memory, and
  3D acceleration is disabled before starting the session.
- If the graphical session exits, return to the text login and inspect:

  ```bash
  sudo journalctl -b -u greetd --no-pager
  journalctl --user -b --no-pager
  ```
