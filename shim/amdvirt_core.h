/*
 * amdvirt_core.h - the semantics of Intel's x86 Hypervisor.framework API, on top of the AMDVirt kext.
 *
 * This is the part of the shim that decides what hv_vcpu_read_register(HV_X86_CR2) means, which error code a call
 * returns, what hv_vmx_read_capability reports, and so on. It does not talk to the kernel itself: it goes through
 * an AMDVirtBackend (amdvirt_iokit.c in the real shim, a fake in tests/shim_core_test.c), and it does not include
 * <Hypervisor/Hypervisor.h>, so it builds and is tested on any machine. hv_shim.c adapts it to the real prototypes.
 *
 * Limits of this first version, all reported as errors rather than faked:
 *   - one VM per process (several vCPUs per VM; each runs on the thread that created it)
 *   - no in-kernel APIC / IOAPIC / ATPIC (hv_vcpu_apic_*, hv_vm_ioapic_*, ...): HV_UNSUPPORTED; QEMU emulates those
 *   - no VM address spaces (hv_vm_space_*)
 */
#ifndef AMDVIRT_CORE_H
#define AMDVIRT_CORE_H

#include <stdint.h>
#include <stddef.h>

#include "../include/AMDVirtShared.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- hv_return_t values (the same numbers as Apple's, checked against the SDK header in the tests) ---- */
enum {
    AMDVIRT_HV_SUCCESS      = 0,
    AMDVIRT_HV_ERROR        = (int32_t)0xfae94001,
    AMDVIRT_HV_BUSY         = (int32_t)0xfae94002,
    AMDVIRT_HV_BAD_ARGUMENT = (int32_t)0xfae94003,
    AMDVIRT_HV_NO_RESOURCES = (int32_t)0xfae94005,
    AMDVIRT_HV_NO_DEVICE    = (int32_t)0xfae94006,
    AMDVIRT_HV_DENIED       = (int32_t)0xfae94007,
    AMDVIRT_HV_UNSUPPORTED  = (int32_t)0xfae9400f
};

/* ---- hv_x86_reg_t (same numbering as the SDK) ---- */
enum {
    AMDVIRT_X86_RIP = 0, AMDVIRT_X86_RFLAGS, AMDVIRT_X86_RAX, AMDVIRT_X86_RCX, AMDVIRT_X86_RDX, AMDVIRT_X86_RBX, AMDVIRT_X86_RSI, AMDVIRT_X86_RDI,
    AMDVIRT_X86_RSP, AMDVIRT_X86_RBP, AMDVIRT_X86_R8, AMDVIRT_X86_R9, AMDVIRT_X86_R10, AMDVIRT_X86_R11, AMDVIRT_X86_R12, AMDVIRT_X86_R13,
    AMDVIRT_X86_R14, AMDVIRT_X86_R15, AMDVIRT_X86_CS, AMDVIRT_X86_SS, AMDVIRT_X86_DS, AMDVIRT_X86_ES, AMDVIRT_X86_FS, AMDVIRT_X86_GS,
    AMDVIRT_X86_IDT_BASE, AMDVIRT_X86_IDT_LIMIT, AMDVIRT_X86_GDT_BASE, AMDVIRT_X86_GDT_LIMIT, AMDVIRT_X86_LDTR, AMDVIRT_X86_LDT_BASE,
    AMDVIRT_X86_LDT_LIMIT, AMDVIRT_X86_LDT_AR, AMDVIRT_X86_TR, AMDVIRT_X86_TSS_BASE, AMDVIRT_X86_TSS_LIMIT, AMDVIRT_X86_TSS_AR,
    AMDVIRT_X86_CR0, AMDVIRT_X86_CR1, AMDVIRT_X86_CR2, AMDVIRT_X86_CR3, AMDVIRT_X86_CR4, AMDVIRT_X86_DR0, AMDVIRT_X86_DR1, AMDVIRT_X86_DR2,
    AMDVIRT_X86_DR3, AMDVIRT_X86_DR4, AMDVIRT_X86_DR5, AMDVIRT_X86_DR6, AMDVIRT_X86_DR7, AMDVIRT_X86_TPR, AMDVIRT_X86_XCR0, AMDVIRT_X86_REGISTERS_MAX
};

/* ---- hv_vmx_capability_t ---- */
enum {
    AMDVIRT_VMX_CAP_PINBASED = 0, AMDVIRT_VMX_CAP_PROCBASED, AMDVIRT_VMX_CAP_PROCBASED2, AMDVIRT_VMX_CAP_ENTRY, AMDVIRT_VMX_CAP_EXIT,
    AMDVIRT_VMX_CAP_BASIC, AMDVIRT_VMX_CAP_TRUE_PINBASED, AMDVIRT_VMX_CAP_TRUE_PROCBASED, AMDVIRT_VMX_CAP_TRUE_ENTRY,
    AMDVIRT_VMX_CAP_TRUE_EXIT, AMDVIRT_VMX_CAP_MISC, AMDVIRT_VMX_CAP_CR0_FIXED0, AMDVIRT_VMX_CAP_CR0_FIXED1,
    AMDVIRT_VMX_CAP_CR4_FIXED0, AMDVIRT_VMX_CAP_CR4_FIXED1, AMDVIRT_VMX_CAP_VMCS_ENUM, AMDVIRT_VMX_CAP_EPT_VPID_CAP
};

/* What the real thing accepts for hv_vm_map and friends. */
#define AMDVIRT_HV_MEMORY_MASK 0x7u

/*
 * The kernel side. Every function returns an IOReturn-style code (0 = success) and is given `ctx`. Functions the
 * backend does not need can be NULL; the core then answers HV_UNSUPPORTED.
 */
typedef struct AMDVirtBackend {
    void *ctx;
    int (*vm_create)(void *ctx, uint32_t flags);                 /* AMDVIRT_VMCREATE_* */
    int (*vm_destroy)(void *ctx);
    int (*vm_map)(void *ctx, uint64_t uva, uint64_t gpa, uint64_t size, uint32_t flags);
    int (*vm_unmap)(void *ctx, uint64_t gpa, uint64_t size);
    int (*vm_protect)(void *ctx, uint64_t gpa, uint64_t size, uint32_t flags);
    int (*vmcs_read)(void *ctx, uint32_t field, uint64_t *value);
    int (*vmcs_write)(void *ctx, uint32_t field, uint64_t value);
    int (*get_regs)(void *ctx, AMDVirtRegs *regs);
    int (*set_regs)(void *ctx, const AMDVirtRegs *regs);
    int (*get_fpu)(void *ctx, AMDVirtFpuState *fpu);
    int (*set_fpu)(void *ctx, const AMDVirtFpuState *fpu);
    int (*msr)(void *ctx, uint32_t op, uint32_t msr, uint64_t value, uint64_t *out);
    int (*run)(void *ctx, AMDVirtExit *exit);
    int (*kick)(void *ctx);
    uint64_t (*now)(void *ctx);                                  /* mach_absolute_time units, for exec time */
    uint64_t (*tsc)(void *ctx);                                  /* host TSC */
    unsigned long (*thread_id)(void *ctx);                       /* identifies the calling thread */
    /* Fast path (ABI 8); without them the core falls back to one call per register / field. */
    int (*run_ex)(void *ctx, const AMDVirtRunIn *in, AMDVirtRunOut *out);
    int (*vmcs_read_many)(void *ctx, const AMDVirtReadMany *in, AMDVirtReadManyOut *out);
    /*
     * More than one vCPU (ABI 9). With these the per-vCPU functions above (vmcs_*, *_regs, *_fpu, msr, run, run_ex, kick,
     * vmcs_read_many) get the pointer vcpu_open returned instead of `ctx`; the VM-wide ones keep using `ctx`.
     * vcpu_open(index 0) returns the vCPU that vm_create made. Without vcpu_open there is one vCPU and its ctx is `ctx`.
     */
    int (*vcpu_open)(void *ctx, uint32_t index, void **vctx);
    int (*vcpu_close)(void *ctx, uint32_t index, void *vctx);
} AMDVirtBackend;

/* Installs the backend. The core copies it. Must be called before the first API call. */
void amdvirt_core_init(const AMDVirtBackend *backend);

/* Whether hv_vmx_read_capability offers the TPR shadow (CPU-based control bit 21); default no. */
extern int amdvirt_core_offer_tpr_shadow;
extern int amdvirt_core_offer_nmi_window;
extern int (*amdvirt_core_vcpu_hint)(void);

/* Optional trace sink, printf style; NULL (the default) disables tracing. */
extern void (*amdvirt_core_trace)(const char *fmt, ...);

/* ---- the API (return values are the hv_return_t numbers above) ---- */
int32_t amdvirt_vm_create(uint64_t flags);
int32_t amdvirt_vm_destroy(void);
int32_t amdvirt_vm_map(uint64_t uva, uint64_t gpa, size_t size, uint64_t flags);
int32_t amdvirt_vm_unmap(uint64_t gpa, size_t size);
int32_t amdvirt_vm_protect(uint64_t gpa, size_t size, uint64_t flags);
int32_t amdvirt_vm_sync_tsc(uint64_t tsc);

int32_t amdvirt_vcpu_create(uint32_t *vcpu, uint64_t flags);
int32_t amdvirt_vcpu_destroy(uint32_t vcpu);
int32_t amdvirt_vcpu_run(uint32_t vcpu);
void amdvirt_vcpu_override_exit(uint32_t vcpu, uint64_t reason, uint64_t instrLen);
int32_t amdvirt_vcpu_interrupt(const uint32_t *vcpus, unsigned int count);
int32_t amdvirt_vcpu_read_register(uint32_t vcpu, uint32_t reg, uint64_t *value);
int32_t amdvirt_vcpu_write_register(uint32_t vcpu, uint32_t reg, uint64_t value);
int32_t amdvirt_vcpu_read_fpstate(uint32_t vcpu, void *buffer, size_t size);
int32_t amdvirt_vcpu_write_fpstate(uint32_t vcpu, const void *buffer, size_t size);
int32_t amdvirt_vcpu_read_msr(uint32_t vcpu, uint32_t msr, uint64_t *value);
int32_t amdvirt_vcpu_write_msr(uint32_t vcpu, uint32_t msr, uint64_t value);
int32_t amdvirt_vcpu_enable_native_msr(uint32_t vcpu, uint32_t msr, int enable);
int32_t amdvirt_vcpu_flush(uint32_t vcpu);
int32_t amdvirt_vcpu_invalidate_tlb(uint32_t vcpu);
int32_t amdvirt_vcpu_get_exec_time(uint32_t vcpu, uint64_t *time);
uint64_t amdvirt_tsc_clock(void);
int32_t amdvirt_vcpu_set_tsc_relative(uint32_t vcpu, int64_t offset);
int32_t amdvirt_vcpu_set_msr_access(uint32_t vcpu, uint32_t msr, uint32_t flags);
int32_t amdvirt_vmx_get_msr_info(uint32_t field, uint64_t *value);
int32_t amdvirt_vmx_vcpu_set_apic_address(uint32_t vcpu, uint64_t gpa);
int32_t amdvirt_vmx_vcpu_set_apic_address_space(uint32_t vcpu, uint32_t asid, uint64_t gpa);

int32_t amdvirt_vmx_vcpu_read_vmcs(uint32_t vcpu, uint32_t field, uint64_t *value);
int32_t amdvirt_vmx_vcpu_write_vmcs(uint32_t vcpu, uint32_t field, uint64_t value);
int32_t amdvirt_vmx_read_capability(uint32_t cap, uint64_t *value);

/* Drops the register/VMCS cache (the tests change the fake kernel's state behind the core's back). */
void amdvirt_core_cache_reset(void);

/* Number of vCPUs that exist. */
unsigned amdvirt_core_vcpu_count(void);

/* Maps a kernel return code (IOReturn) to an hv_return_t. */
int32_t amdvirt_hv_from_ioreturn(int ior);

/* Which VMCS field holds the x86 register `reg`, or 0 when it is not a plain VMCS/extension field. For tests. */
uint32_t amdvirt_core_register_field(uint32_t reg);

#ifdef __cplusplus
}
#endif

#endif /* AMDVIRT_CORE_H */
