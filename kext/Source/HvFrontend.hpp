/*
 * HvFrontend.hpp - native mode: the kext registers itself with xnu as the hypervisor provider, in the place of Apple's
 * AppleHV, so that the unmodified Hypervisor.framework talks to this kext directly (no shim, no DYLD_INSERT_LIBRARIES).
 * See docs/legacy/NATIVE.md.
 *
 *   amdvirt_hv=1        register the trap tables and callbacks: clients can create VMs and vCPUs and run them
 *   amdvirt_hv=2        debugging only: answer success to the traps that are not implemented
 *   amdvirt_hv_sysctl=1 also write 1 into the variable behind kern.hv_support at boot (untested at boot, `amdvirt-info hv sysctl 1` works)
 *
 * Without amdvirt_hv nothing here runs.
 */
#ifndef AMDVIRT_HVFRONTEND_HPP
#define AMDVIRT_HVFRONTEND_HPP

class com_liamhnam_AMDVirt;

namespace amdvirt {
namespace hv {

/* Registers when boot-arg amdvirt_hv is given (and the sysctl with amdvirt_hv_sysctl=1); harmless otherwise. */
void start(com_liamhnam_AMDVirt *service);
/* Undoes everything (kext stop). */
void stop();

/* Run-time control, used by the user client (amdvirt-info hv ...). Mode 0 unregisters, 1 and 2 register (or just change the mode). */
bool setMode(uint32_t mode);
bool setSysctl(bool on);
bool setTprShadow(bool on);      /* offer the TPR shadow to VMs created afterwards (not HyperKit's) */
bool tprShadow();
uint32_t mode();
bool registered();
bool sysctlTaken();

/* Debugging of a stalled client (amdvirt-info hv debug / flightdump / readguest). Everything goes to the kernel log. Off by default. */
void debugSet(uint32_t flags);
void flightDump(uint32_t count);
void readGuest(uint64_t va);

} // namespace hv
} // namespace amdvirt

#endif
