# AMDVirt

[![Build](https://github.com/liamhnam/AMDVirt/actions/workflows/build.yml/badge.svg)](https://github.com/liamhnam/AMDVirt/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![Platform](https://img.shields.io/badge/platform-macOS%2015%20x86__64-lightgrey)
![Status](https://img.shields.io/badge/status-alpha-orange)

Virtual machines and containers on an **AMD Ryzen Hackintosh**, with the experience of a real Mac.

On an Intel Mac, Docker Desktop, UTM, Lima, QEMU and anything built on `Virtualization.framework` use Apple's `Hypervisor.framework`, which runs
on Intel VT-x. On a Ryzen, macOS reports `kern.hv_support = 0` and they refuse to start. AMDVirt is a kernel extension that implements the same
`Hypervisor.framework` interface on top of AMD-V (SVM), so these apps work **unmodified**.

The goal of this project:

- **Native.** Apps install and run as on a real Mac. No patched apps, no replaced system files, no special settings per app.
- **Nothing to remember.** No boot-args, no commands after each restart. Add the kext to OpenCore and you are done.
- **Safe.** A guest must never be able to hurt the host.

> **Experimental.** This is kernel code: a bug is a panic or a frozen Mac. Test it from a separate copy of your OpenCore EFI first, and keep a
> working EFI **without** the kext.

## Requirements

- AMD Ryzen CPU with SVM, nested paging (NPT) and NRIP save (every Zen CPU), **SVM Mode** enabled in the BIOS
- OpenCore
- macOS Sequoia 15 (15.1 tested)

**Supported apps:** anything that uses `Hypervisor.framework` or `Virtualization.framework` as published by Apple: Docker Desktop, UTM,
Lima, QEMU with `-accel hvf`. **Not supported:** Parallels Desktop and VMware Fusion (they check for an Intel CPU, which cannot be
passed without patching the apps).

## Features

| area | what it does |
|---|---|
| **Native by default** | The `Hypervisor.framework` front end and SVM are on as soon as the kext loads. `kern.hv_support` turns itself on at boot. No boot-args are needed. |
| **Entitlement check** | Like on a real Mac, only processes with `com.apple.security.hypervisor` (or `com.apple.vm.hypervisor`) can create a VM; others get `HV_DENIED`. |
| **Host clock protection** | The guest reads the TSC natively, but its writes are trapped and kept as a guest TSC offset, so a guest (Windows synchronises its TSCs at boot) can never change the host's clock. |
| **Guest TSC** | `hv_vcpu_write_msr(TSC)` and `hv_vcpu_set_tsc_relative` work as on a real Mac (Virtualization.framework uses them). |
| **CPUID** | AMD-only and speculation-control bits a client cannot emulate are hidden from the guest. |
| **Interrupt delivery** | An injection cut short by the time slice stays pending, never lost. |
| **Per-vCPU ASID** | Each vCPU has its own ASID; only its own TLB entries are flushed on VMRUN (with FlushByAsid). |

## Project status

**Alpha.** Daily use works on the author's machine; nothing is verified on other hardware yet.

### Done

- [x] SVM on every CPU, nested page tables, VMX-on-SVM translation, `Hypervisor.framework` front end
- [x] Works out of the box: no boot-args, `kern.hv_support` turns itself on at boot
- [x] Entitlement check in `hv_vm_create` (`HV_DENIED` without `com.apple.security.hypervisor`)
- [x] Virtualization.framework clients: Docker Desktop, Lima, UTM (Apple mode)
- [x] QEMU HVF clients: UTM (QEMU mode), plain `qemu-system-x86_64 -accel hvf`
- [x] Guest TSC: `hv_vcpu_write_msr(TSC)`, `hv_vcpu_set_tsc_relative`, guest `WRMSR TSC` kept as an offset (the host TSC is never written)
- [x] CPUID cleanup for clients that do not emulate AMD/speculation-control MSRs
- [x] Undelivered injections stay pending; per-vCPU ASID

### To do

- [ ] A guest that **reboots itself** can hang in its firmware (a stale timer interrupt is injected after the reset). Workaround: stop and start the VM
- [ ] Compensate the TSC skew between physical cores (Linux may report a TSC warp)
- [ ] Trap guest writes to DEBUGCTL and SPEC_CTRL when a client makes them native
- [ ] Remove the legacy app-patching code (Parallels, older Docker Desktop) and the known races (`hv_vm_map` vs `hv_vm_destroy`, vCPUs after `exec()`)
- [ ] Test: Windows with many vCPUs up to the desktop, sleep and wake with a VM running, runs of many hours, other Ryzen generations, macOS 26
- [ ] Release builds

### Verified tests

One machine: **ASUS ROG Zephyrus G14 (GA401), Ryzen 7 4800HS (Zen 2, 16 threads), macOS Sequoia 15.1, OpenCore**. November 2026.

| test | result |
|---|---|
| Unit tests (`make test`: VMCB layout, NPT, VMX map, VMCS state, FPU, MSR map, patch, shim) | ✅ all pass |
| Boot with the kext, no boot-args | ✅ SVM active on 16/16 threads, `kern.hv_support = 1` |
| **Docker Desktop** 4.99.0, unmodified | ✅ `hello-world`, `-p 8080:80` reachable on macOS localhost, bind mounts (checksums match), `docker build`, `docker compose`, 20 containers at once, stress-ng 60 s |
| **Lima** 2.3.0, vz | ✅ Alpine and Ubuntu guests, network, virtiofs mounts, Docker Engine inside, stress-ng 60 s |
| **UTM** 5.0.8, Apple Virtualization | ✅ Linux kernel boots on 2 vCPUs |
| **UTM** 5.0.8, QEMU + HVF | ✅ Alpine Linux to the login prompt |
| **UTM** 5.0.8, QEMU + HVF, Windows | ⚠️ Windows kernel runs on 2 vCPUs for 3 min, host stays responsive (avg latency 0.04 s) and its clock correct; desktop not checked |
| Guest TSC (`tools/tsc-test`: relative, absolute, two vCPUs) | ✅ 3/3 |
| Entitlement | ✅ an unsigned client gets `HV_DENIED`, signed clients run |
| Guest reboot loop (Lima) | ❌ can hang in firmware after a reboot (see To do) |
| Sleep / wake, long runs, Windows with 16 vCPUs | ⏳ not tested |

## Download

There is no release build yet: build it from source (below). Releases will be published on the
[Releases](https://github.com/liamhnam/AMDVirt/releases) page.

## Build and install

```bash
make test     # unit tests, userland only
make kext     # build/AMDVirt.kext
```

Copy `build/AMDVirt.kext` to `EFI/OC/Kexts/`, add it under `Kernel → Add`, restart, and check that `sysctl kern.hv_support` prints `1`.
No boot-args are needed. Step-by-step guide, checks and recovery: [docs/INSTALL.md](docs/INSTALL.md).

## Boot arguments

None are needed. They exist for troubleshooting:

| boot-arg | effect |
|---|---|
| `-amdvirtoff` | the kext loads but does nothing (emergency switch) |
| `amdvirt_enable_svm=0` | do not enable SVM, only probe the CPU |
| `amdvirt_hv=0` | do not provide `Hypervisor.framework` |
| `amdvirt_hv_sysctl=0` | do not set `kern.hv_support` to 1 at boot |
| `amdvirt_entitlement=0` | let processes without the `com.apple.security.hypervisor` entitlement create VMs |
| `amdvirt_loglevel=N` | kernel log level, 1 (errors) to 5 (trace); default 3 |
| `amdvirt_selftest=1` | check the SVM state of every core after enabling and after wake |

More (step-by-step boot, I/O log level): [docs/DEBUGGING.md](docs/DEBUGGING.md).

## Troubleshooting

- **The Mac does not boot:** pick the EFI without AMDVirt in the firmware boot menu, or boot with `-amdvirtoff`.
- **An app says virtualization is not supported:** check `kmutil showloaded | grep AMDVirt` and `sysctl kern.hv_support`.
  `make tools && ./build/amdvirt-info` shows whether SVM is active on every CPU.
- **A VM hangs after the guest restarts itself:** stop and start the VM (known issue).
- **Reporting a bug:** open an [issue](https://github.com/liamhnam/AMDVirt/issues) with your CPU, macOS version, the app and its
  version, and the report from `scripts/collect-debug.sh`.

## Credits

* [OverlordDev](https://github.com/OverlordDev/ryzen-hypervisor.framework) — the SVM core, VMX-on-SVM translation
* [Acidanthera](https://github.com/acidanthera) — OpenCore, which loads the kext
* [Apple](https://github.com/apple-oss-distributions/xnu) — XNU sources, the reference for kernel interfaces
* [QEMU](https://gitlab.com/qemu-project/qemu), [UTM](https://github.com/utmapp/UTM), [Lima](https://github.com/lima-vm/lima) — the HVF client and the apps AMDVirt is tested with
* [morluto](https://github.com/morluto/rea) / [NSA](https://github.com/NationalSecurityAgency/ghidra) — REA and Ghidra, used to study how clients call `Hypervisor.framework`
* AMD64 Architecture Programmer's Manual Vol. 2 and Intel SDM Vol. 3 — the SVM and VMX specifications

## Legal

AMDVirt contains no Apple code. Apple frameworks were studied only for their behaviour and interfaces. Not affiliated with Apple or AMD.

## License

MIT, see [LICENSE](LICENSE). Changes are listed in [CHANGELOG.md](CHANGELOG.md).
