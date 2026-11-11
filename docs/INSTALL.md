# Installing AMDVirt

AMDVirt is loaded by OpenCore at boot. It changes no file on the macOS volume and needs no boot-args.

> Work on a **copy** of your EFI (a USB stick or a second EFI partition) until AMDVirt has booted fine a few times, and keep an EFI
> **without** it that you can pick in the firmware boot menu.

## 1. Requirements

| | |
|---|---|
| CPU | AMD Ryzen with SVM, nested paging (NPT) and NRIP save. Every Zen CPU has them |
| Firmware | **SVM Mode** enabled in the BIOS (often under *Advanced → CPU Configuration*) |
| Bootloader | OpenCore |
| macOS | Sequoia 15 (15.1 is tested). Other versions are untested |

## 2. Build

Needs the Xcode Command Line Tools (`xcode-select --install`).

```bash
make test     # unit tests, userland only
make kext     # build/AMDVirt.kext
make tools    # build/amdvirt-info, a status and log tool (optional)
```

## 3. OpenCore: Kernel → Add

Copy `build/AMDVirt.kext` to `EFI/OC/Kexts/` and add an entry to `config.plist`:

| field | value |
|---|---|
| Arch | Any |
| BundlePath | AMDVirt.kext |
| Comment | AMDVirt |
| Enabled | true |
| ExecutablePath | Contents/MacOS/AMDVirt |
| MinKernel / MaxKernel | empty |
| PlistPath | Contents/Info.plist |

AMDVirt does not depend on Lilu; any position after Lilu works. `scripts/install-to-efi.sh <mounted EFI>` copies the kext and keeps a backup of
the old one.

## 4. Check

After a restart:

```bash
kmutil showloaded | grep AMDVirt     # com.liamhnam.AMDVirt is loaded
sysctl kern.hv_support               # kern.hv_support: 1
./build/amdvirt-info                     # SVM supported, active on every CPU
```

Then install Docker Desktop, UTM, Lima… as usual.

| `amdvirt-info` shows | means |
|---|---|
| `SVM: NOT supported` | the CPU or firmware hides SVM |
| `BIOS disable: YES` | enable SVM Mode in the BIOS |
| `active on` fewer than all CPUs | some cores came online late; they are switched on when the first VM starts |
| `service not found` | the kext is not loaded: check the OpenCore entry and the OpenCore log |

## 5. Uninstall

Remove the entry from `Kernel → Add` (or set `Enabled` to false) and delete `EFI/OC/Kexts/AMDVirt.kext`.

## If the Mac does not boot

1. In the firmware boot menu, pick the EFI **without** AMDVirt.
2. Or add `-amdvirtoff` to `boot-args` from the OpenCore picker or the other EFI: the kext loads but does nothing.
3. Or, from the OpenCore Shell, rename `EFI\OC\Kexts\AMDVirt.kext`.

Logs, counters and the self-test are described in [DEBUGGING.md](DEBUGGING.md). `scripts/collect-debug.sh` gathers a report to attach to an issue.
