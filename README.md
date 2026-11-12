# AMDVirt

[![Build](https://github.com/liamhnam/AMDVirt/actions/workflows/build.yml/badge.svg)](https://github.com/liamhnam/AMDVirt/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![Platform](https://img.shields.io/badge/platform-macOS%2015%20x86__64-lightgrey)
![Status](https://img.shields.io/badge/status-alpha-orange)

`Hypervisor.framework` for AMD Ryzen Hackintoshes.

On an Intel Mac, Docker Desktop, UTM, Lima and QEMU run virtual machines through Apple's `Hypervisor.framework`, which needs Intel VT-x.
On a Ryzen, macOS reports virtualization as unsupported and these apps refuse to start. AMDVirt is a kernel extension that provides the same
framework on top of AMD-V (SVM), so the apps run **unmodified**, with no boot-args and no changes to macOS.

> [!WARNING]
> AMDVirt is alpha-quality kernel code: a bug can panic or freeze the Mac. Try it from a copy of your OpenCore EFI first, and keep a
> working EFI **without** it.

## Features

- **Works like a real Mac:** install Docker Desktop, UTM or Lima as usual; nothing is patched, wrapped or pinned to a version.
- **Zero configuration:** add the kext to OpenCore; `kern.hv_support` is on at every boot without boot-args or launch daemons.
- **Host protection:** a guest cannot change the host's clock or CPU registers, and only apps signed with the
  `com.apple.security.hypervisor` entitlement can create VMs, as on a real Mac.

## Requirements

- AMD Ryzen CPU (any Zen generation) with **SVM Mode** enabled in the BIOS
- OpenCore
- macOS Sequoia 15

## Compatibility

| app | status |
|---|---|
| Docker Desktop | ✅ |
| Lima (`vz`) | ✅ |
| UTM, Apple Virtualization | ✅ Linux |
| UTM, QEMU | ✅ Linux · ⚠️ Windows (boots, not fully tested) |
| Parallels Desktop, VMware Fusion | ❌ not supported: they require an Intel CPU |

Tested on a Ryzen 7 4800HS with macOS 15.1. Details: [docs/TESTING.md](docs/TESTING.md).

## Installation

There are no release builds yet. Build from source with the Xcode Command Line Tools:

```bash
make test     # unit tests
make kext     # build/AMDVirt.kext
```

Copy `build/AMDVirt.kext` to `EFI/OC/Kexts/`, add it under `Kernel → Add` in `config.plist` and restart. `sysctl kern.hv_support` should
print `1`. Full guide and recovery steps: [docs/INSTALL.md](docs/INSTALL.md).

## Boot arguments

None are required. For troubleshooting:

| boot-arg | effect |
|---|---|
| `-amdvirtoff` | load the kext but do nothing |
| `amdvirt_enable_svm=0` | do not enable SVM, only probe the CPU |
| `amdvirt_hv=0` | do not provide `Hypervisor.framework` |
| `amdvirt_hv_sysctl=0` | do not set `kern.hv_support` at boot |
| `amdvirt_entitlement=0` | allow VMs from apps without the hypervisor entitlement |
| `amdvirt_loglevel=N` | kernel log level, 1 (errors) to 5 (trace); default 3 |
| `amdvirt_selftest=1` | verify the SVM state of every core after enabling and after wake |
| `amdvirt_pin_limit_mb=N` | at most N MiB of guest memory pinned at a time, all VMs together (default: half the RAM) |
| `amdvirt_lazypin=0` | pin a VM's whole memory when it is mapped, as before 0.1.0 |

## Known issues

- A guest that restarts itself can hang in its firmware. Stop and start the VM.
- Linux guests may mark the TSC unstable and use HPET instead (slower timekeeping): the host's TSCs drift apart between cores
  while the Mac runs, which a guest sees when its vCPUs move between cores. Docker Desktop is not affected (it sets `tsc=reliable`).
- Not tested yet: sleep and wake with a VM running, long runs, Windows with many vCPUs, other Ryzen generations.

## Troubleshooting

- **The Mac does not boot:** choose the EFI without AMDVirt in the firmware boot menu, or boot with `-amdvirtoff`.
- **An app says virtualization is not supported:** check `kmutil showloaded | grep AMDVirt` and `sysctl kern.hv_support`.
- **Reporting a bug:** open an [issue](https://github.com/liamhnam/AMDVirt/issues) with your CPU, macOS version, the app and its version,
  and the output of `scripts/collect-debug.sh`. Logs and diagnostics: [docs/DEBUGGING.md](docs/DEBUGGING.md).

## Credits

- [OverlordDev](https://github.com/OverlordDev/ryzen-hypervisor.framework) — the SVM core, VMX-on-SVM translation
- [Acidanthera](https://github.com/acidanthera) — OpenCore
- [QEMU](https://gitlab.com/qemu-project/qemu), [UTM](https://github.com/utmapp/UTM), [Lima](https://github.com/lima-vm/lima) — the apps AMDVirt is tested with
- AMD64 Architecture Programmer's Manual and Intel SDM — the SVM and VMX specifications

## License

[MIT](LICENSE). AMDVirt contains no Apple code and is not affiliated with Apple or AMD. Changes: [CHANGELOG.md](CHANGELOG.md).
