# AMDVirt: ready-made build

`Hypervisor.framework` on AMD Ryzen (hackintosh, OpenCore). Everything is built and inside: the kext, the shim, QEMU and the `amdvirt` installer.
Nothing needs compiling; Xcode and Homebrew are not required.

Tested on: Ryzen 7 5800H, macOS 15.8.1, OpenCore. Parallels Desktop only **26.5.0 (build 57613)**. This is an experimental
project: install it on a test EFI first (a USB stick, for example) and keep a working EFI without the kext.

## Install

```bash
tar xzf amdvirt-*-macos-x86_64.tar.gz && cd amdvirt-*/
xattr -dr com.apple.quarantine .      # if the archive was downloaded with a browser
./amdvirt                                 # menu
```

1. **`./amdvirt kext`** copies `AMDVirt.kext` to EFI/OC/Kexts, adds an entry to `config.plist` (with a backup and a validity check)
   and appends the boot-args `amdvirt_enable_svm=1 amdvirt_allow_users=1 amfi_get_out_of_my_way=0x1`. Reboot from that EFI.
   SVM Mode must be enabled in the BIOS.
2. **`./amdvirt doctor`** checks that everything is in place: CPU, boot-args, the kext (its ABI must match this build), QEMU, Parallels.
   **Native mode (new in 0.7, experimental):** `./amdvirt kext --native` also adds `amdvirt_hv=1`. Then the kext itself is the hypervisor provider and
   `./amdvirt docker-desktop` leaves Docker's own files alone (no shim, no wrappers). Read `docs/legacy/NATIVE.md` first: it lists what is tested and what is not.
3. Then whatever you need:
   - **Docker Desktop (the real one):** `./amdvirt docker-desktop get` (downloads Docker Desktop 4.97.0 from docker.com and installs it;
     skip if you already have exactly that build), then `./amdvirt docker-desktop`, then start Docker as usual (`open -a Docker`).
     Details: `docs/legacy/DOCKER-DESKTOP.md`. Quit Docker Desktop before running it.
   - **A small Docker VM instead (no Docker Desktop):** `./amdvirt docker up`, then `eval "$(./amdvirt docker env)"` and `docker run --rm hello-world`.
     It is a small Linux VM (Alpine, 2 vCPUs, 2 GiB) running `dockerd`; images live in `~/.amdvirt/docker/disk.raw`.
     To reach a container port from the Mac: `./amdvirt docker up --port 8080`, then `docker run -p 8080:80 nginx` and `http://127.0.0.1:8080`. Stop it with `./amdvirt docker down`.
     The first start downloads about 150 MB (the Alpine kernel, packages, the docker CLI) and takes a few minutes.
   - **Parallels Desktop 26.5.0:** `./amdvirt parallels`, then the VM settings it prints. To undo: `./amdvirt parallels undo`.
     With the kext in native mode (`./amdvirt kext --native`) there is also `./amdvirt parallels native` (experimental, see `docs/legacy/NATIVE.md`) and
     `./amdvirt autostart install`, which turns `kern.hv_support` on at every boot (it is 0 after each restart and Parallels needs it); `./amdvirt autostart remove` undoes it.

## What is inside

| path | what |
|---|---|
| `amdvirt` | installer and launcher |
| `AMDVirt.kext` | the kext (AMD-V / SVM), unsigned, loaded through OpenCore |
| `lib/libamdvirt.dylib` | the `Hypervisor.framework` shim |
| `lib/libPrlXmlModel.1.dylib` | stand-in for a Parallels library (build 57613 only) |
| `lib/libPrlXmlModel.1.native.dylib` | the same stand-in for native mode: it patches the CPU-vendor check and arms the keyboard; the hypervisor calls stay Apple's |
| `qemu/` | QEMU 11.1.2 with HVF and user networking (GPL, sources: `LICENSES/SOURCES.txt`) |
| `bin/amdvirt-info` | kext diagnostics (`amdvirt-info`, `amdvirt-info log`) |
| `docs/` | details: Parallels, Docker Desktop, the small Docker VM, installing on a Ryzentosh |

## If something is wrong

`./amdvirt doctor` says what to fix. For the kext: logs with `./bin/amdvirt-info log`; if the system does not boot, pick the fallback
EFI without the kext in OpenCore. Project: https://github.com/liamhnam/AMDVirt
