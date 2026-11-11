# Test results

All results come from one machine: **ASUS ROG Zephyrus G14 (GA401), Ryzen 7 4800HS (Zen 2, 16 threads), macOS Sequoia 15.1, OpenCore**,
November 2026.

✅ works · ⚠️ partly verified · ❌ known problem · ⏳ not tested yet

## Apps

| app | result |
|---|---|
| **Docker Desktop** 4.99.0, unmodified | ✅ `hello-world`; `-p 8080:80` reachable on macOS localhost; bind mounts (checksums match); `docker build`; `docker compose`; 20 containers at once; stress-ng 60 s |
| **Lima** 2.3.0, `vz` | ✅ Alpine and Ubuntu guests; network; virtiofs mounts; Docker Engine inside; stress-ng 60 s |
| **UTM** 5.0.8, Apple Virtualization | ✅ Linux kernel boots on 2 vCPUs |
| **UTM** 5.0.8, QEMU + HVF, Linux | ✅ Alpine Linux to the login prompt |
| **UTM** 5.0.8, QEMU + HVF, Windows | ⚠️ the Windows kernel runs on 2 vCPUs for 3 min; the host stays responsive (average latency 0.04 s) and its clock correct; the desktop was not checked |

## Kext behaviour

| check | result |
|---|---|
| Boot without boot-args | ✅ SVM active on 16/16 threads, `kern.hv_support = 1` |
| Entitlement | ✅ a client without `com.apple.security.hypervisor` gets `HV_DENIED`; signed clients run |
| Guest TSC set by the client (`hv_vcpu_set_tsc_relative`, `hv_vcpu_write_msr`), on one and two vCPUs | ✅ |
| Guest writes to the TSC (Windows at boot) | ✅ kept as a guest offset, the host clock is unchanged |
| MSRs a client makes native (QEMU: STAR…TSC_AUX, TSC, SYSENTER, FS/GS base) | ✅ all swapped by VMRUN/VMLOAD or by the kext; other MSRs are refused and stay trapped |
| A guest that reboots itself (Lima, repeated) | ❌ can hang in its firmware after a reboot |
| Sleep and wake with a VM running | ⏳ |
| Runs of many hours | ⏳ |
| Windows with 16 vCPUs, up to the desktop | ⏳ |
| Other Ryzen generations, macOS 26 | ⏳ |

Unit tests (`make test`) run on every push, see the Build badge in the README.
