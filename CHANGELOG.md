# Changelog

## 0.1.0 (unreleased)

- Renamed throughout: `AMDVirt.kext` (`com.liamhnam.AMDVirt`), IOKit classes `com_liamhnam_AMDVirt*`, tools `amdvirt` and `amdvirt-info`,
  boot-args `amdvirt_*` and `-amdvirtoff` (the old `rhv_*` boot-args are no longer read).
- Guest writes to the TSC MSR are trapped and kept as a TSC offset; the host's TSC is never written (a Windows guest used to freeze the Mac).
- `hv_vcpu_write_msr(TSC)` and `hv_vcpu_set_tsc_relative` are applied.
- CPUID: AMD-only and speculation-control bits that clients do not emulate are hidden (fixes a Linux guest panic on IBPB).
- An injection cut short by the time slice stays pending instead of being lost.
- Per-vCPU ASID with FlushByAsid.
- SVM and the `Hypervisor.framework` front end are on by default; `kern.hv_support` is set at boot. No boot-args are needed.
- `hv_vm_create` requires the `com.apple.security.hypervisor` entitlement.
- Documentation: new README and install guide; notes from earlier versions moved to `docs/legacy/`.
