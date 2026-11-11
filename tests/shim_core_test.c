/*
 * shim_core_test.c - tests for shim/amdvirt_core.c with a fake kernel behind it. Also compares our copies of the API's
 * constants (error codes, register numbers, capability numbers) with the real SDK header.
 */
#include <Hypervisor/Hypervisor.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "../shim/amdvirt_core.h"
#include "../shim/amdvirt_fields.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

/* ---- constants must equal Apple's ---- */
_Static_assert(AMDVIRT_HV_SUCCESS == HV_SUCCESS, "HV_SUCCESS");
_Static_assert(AMDVIRT_HV_ERROR == HV_ERROR, "HV_ERROR");
_Static_assert(AMDVIRT_HV_BUSY == HV_BUSY, "HV_BUSY");
_Static_assert(AMDVIRT_HV_BAD_ARGUMENT == HV_BAD_ARGUMENT, "HV_BAD_ARGUMENT");
_Static_assert(AMDVIRT_HV_NO_RESOURCES == HV_NO_RESOURCES, "HV_NO_RESOURCES");
_Static_assert(AMDVIRT_HV_NO_DEVICE == HV_NO_DEVICE, "HV_NO_DEVICE");
_Static_assert(AMDVIRT_HV_DENIED == HV_DENIED, "HV_DENIED");
_Static_assert(AMDVIRT_HV_UNSUPPORTED == HV_UNSUPPORTED, "HV_UNSUPPORTED");
#define SAME(a, b) _Static_assert((int)(a) == (int)(b), #a)
SAME(AMDVIRT_X86_RIP, HV_X86_RIP); SAME(AMDVIRT_X86_RFLAGS, HV_X86_RFLAGS); SAME(AMDVIRT_X86_RAX, HV_X86_RAX);
SAME(AMDVIRT_X86_RSP, HV_X86_RSP); SAME(AMDVIRT_X86_R15, HV_X86_R15); SAME(AMDVIRT_X86_CS, HV_X86_CS); SAME(AMDVIRT_X86_GS, HV_X86_GS);
SAME(AMDVIRT_X86_IDT_BASE, HV_X86_IDT_BASE); SAME(AMDVIRT_X86_GDT_LIMIT, HV_X86_GDT_LIMIT); SAME(AMDVIRT_X86_LDTR, HV_X86_LDTR);
SAME(AMDVIRT_X86_LDT_AR, HV_X86_LDT_AR); SAME(AMDVIRT_X86_TR, HV_X86_TR); SAME(AMDVIRT_X86_TSS_AR, HV_X86_TSS_AR);
SAME(AMDVIRT_X86_CR0, HV_X86_CR0); SAME(AMDVIRT_X86_CR1, HV_X86_CR1); SAME(AMDVIRT_X86_CR2, HV_X86_CR2); SAME(AMDVIRT_X86_CR3, HV_X86_CR3);
SAME(AMDVIRT_X86_CR4, HV_X86_CR4); SAME(AMDVIRT_X86_DR0, HV_X86_DR0); SAME(AMDVIRT_X86_DR3, HV_X86_DR3); SAME(AMDVIRT_X86_DR4, HV_X86_DR4);
SAME(AMDVIRT_X86_DR6, HV_X86_DR6); SAME(AMDVIRT_X86_DR7, HV_X86_DR7); SAME(AMDVIRT_X86_TPR, HV_X86_TPR); SAME(AMDVIRT_X86_XCR0, HV_X86_XCR0);
SAME(AMDVIRT_X86_REGISTERS_MAX, HV_X86_REGISTERS_MAX);
SAME(AMDVIRT_VMX_CAP_PINBASED, HV_VMX_CAP_PINBASED); SAME(AMDVIRT_VMX_CAP_PROCBASED, HV_VMX_CAP_PROCBASED);
SAME(AMDVIRT_VMX_CAP_PROCBASED2, HV_VMX_CAP_PROCBASED2); SAME(AMDVIRT_VMX_CAP_ENTRY, HV_VMX_CAP_ENTRY);
SAME(AMDVIRT_VMX_CAP_EXIT, HV_VMX_CAP_EXIT); SAME(AMDVIRT_VMX_CAP_BASIC, HV_VMX_CAP_BASIC);
SAME(AMDVIRT_VMX_CAP_TRUE_PROCBASED, HV_VMX_CAP_TRUE_PROCBASED); SAME(AMDVIRT_VMX_CAP_MISC, HV_VMX_CAP_MISC);
SAME(AMDVIRT_VMX_CAP_CR0_FIXED0, HV_VMX_CAP_CR0_FIXED0); SAME(AMDVIRT_VMX_CAP_CR4_FIXED1, HV_VMX_CAP_CR4_FIXED1);
SAME(AMDVIRT_HV_MEMORY_MASK, HV_MEMORY_READ | HV_MEMORY_WRITE | HV_MEMORY_EXEC);

/* ---- the fake kernel ---- */
#define IOR_BAD   0xe00002c2
#define IOR_BUSY  0xe00002d5
#define IOR_UNSUP 0xe00002c7

static struct {
    int vmCreated, vmDestroyed, kicks, runs;
    int getRegs, setRegs, vmcsReads, vmcsWrites;     /* calls that reached the "kernel" */
    int runEx, runExNoRun, readMany;                 /* fast path calls */
    uint32_t createFlags;
    struct { uint64_t uva, gpa, size; uint32_t flags; } map;
    struct { uint64_t gpa, size; } unmap;
    struct { uint64_t gpa, size; uint32_t flags; } prot;
    uint64_t vmcs[0x10000];                 /* indexed by field encoding */
    AMDVirtRegs regs;
    AMDVirtFpuState fpu;
    uint64_t msrs[16][2];                   /* msr number, value */
    int msrCount;
    int nativeMsr;                          /* last native op */
    uint32_t nativeMsrNum;
    uint64_t hostTsc, now;
    unsigned long thread;
    int failNext;
} K;

static int f_vm_create(void *c, uint32_t flags) { (void)c; K.vmCreated++; K.createFlags = flags; return 0; }
static int f_vm_destroy(void *c) { (void)c; K.vmDestroyed++; return 0; }
static int f_vm_map(void *c, uint64_t u, uint64_t g, uint64_t s, uint32_t f)
{ (void)c; K.map.uva = u; K.map.gpa = g; K.map.size = s; K.map.flags = f; return s == 0 ? IOR_BAD : (g == 0x7000 ? IOR_BUSY : 0); }
static int f_vm_unmap(void *c, uint64_t g, uint64_t s) { (void)c; K.unmap.gpa = g; K.unmap.size = s; return 0; }
static int f_vm_protect(void *c, uint64_t g, uint64_t s, uint32_t f) { (void)c; K.prot.gpa = g; K.prot.size = s; K.prot.flags = f; return 0xe00002f0; }
static int f_vmcs_read(void *c, uint32_t f, uint64_t *v) { (void)c; K.vmcsReads++; if (f >= 0x10000) return IOR_BAD; *v = K.vmcs[f]; return 0; }
static int f_vmcs_write(void *c, uint32_t f, uint64_t v) { (void)c; K.vmcsWrites++; if (f >= 0x10000) return IOR_BAD; K.vmcs[f] = v; return 0; }
static int f_get_regs(void *c, AMDVirtRegs *r) { (void)c; K.getRegs++; *r = K.regs; return 0; }
static int f_set_regs(void *c, const AMDVirtRegs *r) { (void)c; K.setRegs++; K.regs = *r; return 0; }
static int f_get_fpu(void *c, AMDVirtFpuState *f) { (void)c; *f = K.fpu; return 0; }
static int f_set_fpu(void *c, const AMDVirtFpuState *f) { (void)c; if (f->xcr0 == 0) return IOR_BAD; K.fpu = *f; return 0; }
static int f_msr(void *c, uint32_t op, uint32_t msr, uint64_t value, uint64_t *out)
{
    (void)c;
    if (msr == 0x80) return IOR_BAD;
    if (op == AMDVIRT_MSR_NATIVE_ENABLE || op == AMDVIRT_MSR_NATIVE_DISABLE) { K.nativeMsr = (int)op; K.nativeMsrNum = msr; return 0; }
    for (int i = 0; i < K.msrCount; i++)
        if (K.msrs[i][0] == msr) {
            if (op == AMDVIRT_MSR_READ) *out = K.msrs[i][1]; else K.msrs[i][1] = value;
            return 0;
        }
    if (op == AMDVIRT_MSR_WRITE) { K.msrs[K.msrCount][0] = msr; K.msrs[K.msrCount][1] = value; K.msrCount++; return 0; }
    *out = 0;
    return 0;
}
static int f_run(void *c, AMDVirtExit *e) { (void)c; K.runs++; K.now += 1000; K.regs.rip += 2; K.vmcs[0x4402] = 12 + (uint64_t)K.runs; e->reason = AMDVIRT_EXIT_HLT; return K.failNext ? (int)0xe00002d8 : 0; }
static int f_kick(void *c) { (void)c; K.kicks++; return 0; }
static uint64_t f_now(void *c) { (void)c; return K.now; }
static uint64_t f_tsc(void *c) { (void)c; return K.hostTsc; }
static unsigned long f_thread(void *c) { (void)c; return K.thread; }

/* The fast path: the same fake kernel behind VmRunEx / VmcsReadMany. */
static const uint32_t kFastFields[AMDVIRT_RUN_NFIELDS] = {
#define X(e) e,
    AMDVIRT_RUN_FIELDS_LIST(X)
#undef X
};

static int f_run_ex(void *c, const AMDVirtRunIn *in, AMDVirtRunOut *out)
{
    (void)c;
    memset(out, 0, sizeof(*out));
    K.runEx++;
    if (in->nWrites > AMDVIRT_RUN_MAX_WRITES) return IOR_BAD;
    if (in->flags & AMDVIRT_RUN_REGS) K.regs = in->regs;
    for (uint32_t i = 0; i < in->nWrites; i++) {
        if (in->writes[i].field >= 0x10000) { out->failedWrite = i; return IOR_BAD; }
        K.vmcsWrites++;
        K.vmcs[in->writes[i].field] = in->writes[i].value;
    }
    if (in->flags & AMDVIRT_RUN_NO_RUN) K.runExNoRun++;
    else {
        AMDVirtExit e;
        int rc = f_run(c, &e);
        if (rc) return rc;
    }
    out->regs = K.regs;
    for (unsigned i = 0; i < AMDVIRT_RUN_NFIELDS; i++) out->fields[i] = K.vmcs[kFastFields[i]];
    return 0;
}

static int f_read_many(void *c, const AMDVirtReadMany *in, AMDVirtReadManyOut *out)
{
    (void)c;
    K.readMany++;
    memset(out, 0, sizeof(*out));
    if (in->count > AMDVIRT_READMANY_MAX) return IOR_BAD;
    for (uint32_t i = 0; i < in->count; i++) out->values[i] = K.vmcs[in->fields[i]];
    return 0;
}

static void freshFast(void)
{
    memset(&K, 0, sizeof(K));
    K.fpu.xcr0 = 1;
    K.fpu.size = 832;
    K.thread = 1;
    AMDVirtBackend b = { NULL, f_vm_create, f_vm_destroy, f_vm_map, f_vm_unmap, f_vm_protect, f_vmcs_read, f_vmcs_write,
                     f_get_regs, f_set_regs, f_get_fpu, f_set_fpu, f_msr, f_run, f_kick, f_now, f_tsc, f_thread,
                     f_run_ex, f_read_many, NULL, NULL };
    amdvirt_core_init(&b);
}

static void fresh(void)
{
    memset(&K, 0, sizeof(K));
    K.fpu.xcr0 = 1;
    K.fpu.size = 832;
    K.thread = 1;
    AMDVirtBackend b = { NULL, f_vm_create, f_vm_destroy, f_vm_map, f_vm_unmap, f_vm_protect, f_vmcs_read, f_vmcs_write,
                     f_get_regs, f_set_regs, f_get_fpu, f_set_fpu, f_msr, f_run, f_kick, f_now, f_tsc, f_thread,
                     NULL, NULL, NULL, NULL };   /* the one-call-per-field path */
    amdvirt_core_init(&b);
}

static uint32_t makeVcpu(void)
{
    uint32_t v = 99;
    CHECK(amdvirt_vm_create(0) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vcpu_create(&v, 0) == AMDVIRT_HV_SUCCESS);
    return v;
}

static void testLifecycle(void)
{
    fresh();
    uint32_t v = 0;
    uint64_t x;
    /* Nothing works without a VM. */
    CHECK(amdvirt_vcpu_create(&v, 0) == AMDVIRT_HV_NO_DEVICE);
    CHECK(amdvirt_vm_map(0x1000, 0, 4096, 7) == AMDVIRT_HV_NO_DEVICE);
    CHECK(amdvirt_vm_destroy() == AMDVIRT_HV_NO_DEVICE);
    CHECK(amdvirt_vcpu_read_register(0, AMDVIRT_X86_RIP, &x) == AMDVIRT_HV_NO_DEVICE);

    CHECK(amdvirt_vm_create(0) == AMDVIRT_HV_SUCCESS);
    CHECK(K.vmCreated == 1 && (K.createFlags & AMDVIRT_VMCREATE_VMX) && (K.createFlags & AMDVIRT_VMCREATE_NO_RAM));
    CHECK(amdvirt_vm_create(0) == AMDVIRT_HV_BUSY && K.vmCreated == 1);                    /* one VM per process */

    CHECK(amdvirt_vcpu_create(NULL, 0) == AMDVIRT_HV_BAD_ARGUMENT);
    CHECK(amdvirt_vcpu_create(&v, 8) == AMDVIRT_HV_BAD_ARGUMENT);                           /* unknown option bits */
    CHECK(amdvirt_vcpu_create(&v, 0) == AMDVIRT_HV_SUCCESS && v == 0);
    uint32_t v2;
    CHECK(amdvirt_vcpu_create(&v2, 0) == AMDVIRT_HV_NO_RESOURCES);                          /* one vCPU */
    CHECK(amdvirt_vm_destroy() == AMDVIRT_HV_BUSY);                                         /* vCPU still there */
    CHECK(amdvirt_vcpu_read_register(5, AMDVIRT_X86_RIP, &x) == AMDVIRT_HV_BAD_ARGUMENT);       /* wrong vCPU id */
    CHECK(amdvirt_vcpu_destroy(0) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vcpu_destroy(0) == AMDVIRT_HV_BAD_ARGUMENT);
    CHECK(amdvirt_vm_destroy() == AMDVIRT_HV_SUCCESS && K.vmDestroyed == 1);
    CHECK(amdvirt_vm_destroy() == AMDVIRT_HV_NO_DEVICE);
    CHECK(amdvirt_vm_create(0) == AMDVIRT_HV_SUCCESS);                                      /* and again */
}

static void testMemory(void)
{
    fresh();
    CHECK(amdvirt_vm_create(0) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vm_map(0xabc000, 0x100000, 0x2000, 3) == AMDVIRT_HV_SUCCESS);
    CHECK(K.map.uva == 0xabc000 && K.map.gpa == 0x100000 && K.map.size == 0x2000 && K.map.flags == 3);
    CHECK(amdvirt_vm_map(0xabc000, 0, 4096, 8) == AMDVIRT_HV_BAD_ARGUMENT);                 /* HV_MEMORY_UEXEC et al. */
    CHECK(amdvirt_vm_map(0xabc000, 0, 0, 7) == AMDVIRT_HV_BAD_ARGUMENT);                    /* kernel BadArgument */
    CHECK(amdvirt_vm_map(0xabc000, 0x7000, 4096, 7) == AMDVIRT_HV_BUSY);                    /* kernel Busy */
    CHECK(amdvirt_vm_unmap(0x100000, 0x1000) == AMDVIRT_HV_SUCCESS && K.unmap.gpa == 0x100000 && K.unmap.size == 0x1000);
    CHECK(amdvirt_vm_protect(0x100000, 0x1000, 1) == AMDVIRT_HV_BAD_ARGUMENT && K.prot.flags == 1);   /* kernel NotFound */
    CHECK(amdvirt_vm_protect(0x100000, 0x1000, 16) == AMDVIRT_HV_BAD_ARGUMENT);
}

static void testRegisters(void)
{
    fresh();
    uint32_t v = makeVcpu();
    uint64_t x;

    /* General purpose registers go through AMDVirtRegs, each to its own slot. */
    for (uint32_t r = AMDVIRT_X86_RIP; r <= AMDVIRT_X86_R15; r++)
        CHECK(amdvirt_vcpu_write_register(v, r, 0x1000 + r) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vcpu_flush(v) == AMDVIRT_HV_SUCCESS);                  /* register writes are buffered until flush or run */
    CHECK(K.regs.rip == 0x1000 && K.regs.rflags == 0x1001 && K.regs.rax == 0x1002 && K.regs.rcx == 0x1003 &&
          K.regs.rdx == 0x1004 && K.regs.rbx == 0x1005 && K.regs.rsi == 0x1006 && K.regs.rdi == 0x1007 &&
          K.regs.rsp == 0x1008 && K.regs.rbp == 0x1009 && K.regs.r8 == 0x100a && K.regs.r15 == 0x1011);
    for (uint32_t r = AMDVIRT_X86_RIP; r <= AMDVIRT_X86_R15; r++)
        CHECK(amdvirt_vcpu_read_register(v, r, &x) == AMDVIRT_HV_SUCCESS && x == 0x1000 + r);

    /* Writing one register must not disturb the others. */
    CHECK(amdvirt_vcpu_write_register(v, AMDVIRT_X86_RAX, 0xAA) == AMDVIRT_HV_SUCCESS && amdvirt_vcpu_flush(v) == AMDVIRT_HV_SUCCESS);
    CHECK(K.regs.rbx == 0x1005 && K.regs.rax == 0xAA);

    /* Everything else is a VMCS (or extension) field, each to its own encoding. */
    struct { uint32_t reg, field; } map[] = {
        { AMDVIRT_X86_CS, 0x802 }, { AMDVIRT_X86_SS, 0x804 }, { AMDVIRT_X86_DS, 0x806 }, { AMDVIRT_X86_ES, 0x800 }, { AMDVIRT_X86_FS, 0x808 },
        { AMDVIRT_X86_GS, 0x80a }, { AMDVIRT_X86_IDT_BASE, 0x6818 }, { AMDVIRT_X86_IDT_LIMIT, 0x4812 }, { AMDVIRT_X86_GDT_BASE, 0x6816 },
        { AMDVIRT_X86_GDT_LIMIT, 0x4810 }, { AMDVIRT_X86_LDTR, 0x80c }, { AMDVIRT_X86_LDT_BASE, 0x6812 }, { AMDVIRT_X86_LDT_LIMIT, 0x480c },
        { AMDVIRT_X86_LDT_AR, 0x4820 }, { AMDVIRT_X86_TR, 0x80e }, { AMDVIRT_X86_TSS_BASE, 0x6814 }, { AMDVIRT_X86_TSS_LIMIT, 0x480e },
        { AMDVIRT_X86_TSS_AR, 0x4822 }, { AMDVIRT_X86_CR0, 0x6800 }, { AMDVIRT_X86_CR3, 0x6802 }, { AMDVIRT_X86_CR4, 0x6804 },
        { AMDVIRT_X86_CR2, 0xF000 }, { AMDVIRT_X86_DR0, 0xF010 }, { AMDVIRT_X86_DR1, 0xF012 }, { AMDVIRT_X86_DR2, 0xF014 },
        { AMDVIRT_X86_DR3, 0xF016 }, { AMDVIRT_X86_DR6, 0xF002 }, { AMDVIRT_X86_DR7, 0x681a }, { AMDVIRT_X86_TPR, 0xF004 },
    };
    for (unsigned i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        CHECK(amdvirt_core_register_field(map[i].reg) == map[i].field);
        if (map[i].reg == AMDVIRT_X86_TPR)                       /* 8-bit APIC TPR in the API, the 4-bit class in the kernel */
            continue;
        CHECK(amdvirt_vcpu_write_register(v, map[i].reg, 0x5000 + i) == AMDVIRT_HV_SUCCESS);
        CHECK(K.vmcs[map[i].field] == 0x5000 + i);
        CHECK(amdvirt_vcpu_read_register(v, map[i].reg, &x) == AMDVIRT_HV_SUCCESS && x == 0x5000 + i);
    }
    /* HV_X86_TPR is the 8-bit APIC TPR (0xF0 = class 15); the kernel keeps the 4-bit class in V_TPR. */
    CHECK(amdvirt_vcpu_write_register(v, AMDVIRT_X86_TPR, 0xF0) == AMDVIRT_HV_SUCCESS && K.vmcs[0xF004] == 0xF);
    CHECK(amdvirt_vcpu_read_register(v, AMDVIRT_X86_TPR, &x) == AMDVIRT_HV_SUCCESS && x == 0xF0);
    K.vmcs[0xF004] = 0xC; amdvirt_core_cache_reset();
    CHECK(amdvirt_vcpu_read_register(v, AMDVIRT_X86_TPR, &x) == AMDVIRT_HV_SUCCESS && x == 0xC0);
    /* DR4/DR5 alias DR6/DR7. */
    CHECK(amdvirt_vcpu_write_register(v, AMDVIRT_X86_DR4, 0xD6) == AMDVIRT_HV_SUCCESS && K.vmcs[0xF002] == 0xD6);
    CHECK(amdvirt_vcpu_read_register(v, AMDVIRT_X86_DR5, &x) == AMDVIRT_HV_SUCCESS && x == K.vmcs[0x681a]);
    /* Registers the hardware does not have. */
    CHECK(amdvirt_vcpu_read_register(v, AMDVIRT_X86_CR1, &x) == AMDVIRT_HV_BAD_ARGUMENT);
    CHECK(amdvirt_vcpu_write_register(v, AMDVIRT_X86_CR1, 0) == AMDVIRT_HV_BAD_ARGUMENT);
    CHECK(amdvirt_vcpu_read_register(v, AMDVIRT_X86_REGISTERS_MAX, &x) == AMDVIRT_HV_BAD_ARGUMENT);
    CHECK(amdvirt_vcpu_read_register(v, 9999, &x) == AMDVIRT_HV_BAD_ARGUMENT);
    CHECK(amdvirt_vcpu_read_register(v, AMDVIRT_X86_RIP, NULL) == AMDVIRT_HV_BAD_ARGUMENT);

    /* XCR0 is in the FPU state. */
    CHECK(amdvirt_vcpu_read_register(v, AMDVIRT_X86_XCR0, &x) == AMDVIRT_HV_SUCCESS && x == 1);
    uint64_t bv = 7;
    memcpy(K.fpu.area + 512, &bv, 8);                      /* the saved image says AVX state is in use */
    CHECK(amdvirt_vcpu_write_register(v, AMDVIRT_X86_XCR0, 7) == AMDVIRT_HV_SUCCESS && K.fpu.xcr0 == 7);
    memcpy(&bv, K.fpu.area + 512, 8);
    CHECK(bv == 7);
    CHECK(amdvirt_vcpu_write_register(v, AMDVIRT_X86_XCR0, 3) == AMDVIRT_HV_SUCCESS);
    memcpy(&bv, K.fpu.area + 512, 8);
    CHECK(bv == 3);                                        /* AVX no longer marked in use */
    CHECK(amdvirt_vcpu_write_register(v, AMDVIRT_X86_XCR0, 0) == AMDVIRT_HV_BAD_ARGUMENT);   /* kernel refuses */
}

/* The fast path: one kernel call per guest exit, writes buffered, the usual reads served from the reply. */
static void testFastPath(void)
{
    freshFast();
    uint32_t v = makeVcpu();
    uint64_t x;
    K.regs.rax = 0x11; K.regs.rip = 0x2000; K.regs.rflags = 0x202;
    K.vmcs[0x4402] = 30; K.vmcs[0x6400] = 0x80;

    /* Writes wait for the run, in one call, together with the dirty registers. */
    CHECK(amdvirt_vmx_vcpu_write_vmcs(v, 0x4002, 0x840065fe) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vmx_vcpu_write_vmcs(v, 0x4016, 0x80000030) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vmx_vcpu_write_vmcs(v, 0x4002, 0x840065fa) == AMDVIRT_HV_SUCCESS);          /* same field again: last wins */
    CHECK(amdvirt_vcpu_write_register(v, AMDVIRT_X86_RAX, 0x99) == AMDVIRT_HV_SUCCESS);
    CHECK(K.vmcsWrites == 0 && K.runEx == 0 && K.getRegs == 1);                       /* only the register fetch */
    CHECK(amdvirt_vcpu_run(v) == AMDVIRT_HV_SUCCESS);
    CHECK(K.runEx == 1 && K.vmcsWrites == 2 && K.vmcs[0x4002] == 0x840065fa && K.vmcs[0x4016] == 0x80000030);
    CHECK(K.regs.rax == 0x99 && K.runs == 1 && K.setRegs == 0);

    /* After the exit nothing more is asked of the kernel for the usual reads. */
    int calls = K.getRegs + K.vmcsReads + K.readMany + K.runEx;
    CHECK(amdvirt_vmx_vcpu_read_vmcs(v, 0x4402, &x) == AMDVIRT_HV_SUCCESS && x == 13);
    CHECK(amdvirt_vmx_vcpu_read_vmcs(v, 0x6400, &x) == AMDVIRT_HV_SUCCESS && x == 0x80);
    CHECK(amdvirt_vcpu_read_register(v, AMDVIRT_X86_RIP, &x) == AMDVIRT_HV_SUCCESS && x == 0x2002);
    CHECK(amdvirt_vcpu_read_register(v, AMDVIRT_X86_RAX, &x) == AMDVIRT_HV_SUCCESS && x == 0x99);
    CHECK(amdvirt_vcpu_read_register(v, AMDVIRT_X86_CR4, &x) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vcpu_read_register(v, AMDVIRT_X86_TPR, &x) == AMDVIRT_HV_SUCCESS);
    CHECK(K.getRegs + K.vmcsReads + K.readMany + K.runEx == calls);

    /* A field the kernel refuses is refused at once, and nothing is buffered for it. */
    CHECK(amdvirt_vmx_vcpu_write_vmcs(v, 0x4402, 1) == AMDVIRT_HV_BAD_ARGUMENT);              /* read-only exit field */
    CHECK(amdvirt_vmx_vcpu_write_vmcs(v, 0xFFFFF, 1) == AMDVIRT_HV_BAD_ARGUMENT);             /* no such field */
    CHECK(amdvirt_vmx_vcpu_write_vmcs(v, 0xF010, 0x0000800000000000ull) == AMDVIRT_HV_BAD_ARGUMENT);   /* non-canonical DR0 */
    CHECK(amdvirt_vcpu_flush(v) == AMDVIRT_HV_SUCCESS && K.runEx == 1);

    /* Reading a field with a write pending sends the write first. */
    CHECK(amdvirt_vmx_vcpu_write_vmcs(v, 0x6804, 0x26b0) == AMDVIRT_HV_SUCCESS && K.vmcs[0x6804] == 0);
    CHECK(amdvirt_vmx_vcpu_read_vmcs(v, 0x6804, &x) == AMDVIRT_HV_SUCCESS && x == 0x26b0);
    CHECK(K.runEx == 2 && K.runExNoRun == 1 && K.vmcs[0x6804] == 0x26b0 && K.runs == 1);

    /* An MSR access sees the buffered VMCS writes (EFER is both). */
    CHECK(amdvirt_vmx_vcpu_write_vmcs(v, 0x2806, 0xd01) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vcpu_read_msr(v, 0xC0000082, &x) == AMDVIRT_HV_SUCCESS);
    CHECK(K.vmcs[0x2806] == 0xd01);
    CHECK(amdvirt_vcpu_write_msr(v, 0xC0000082, 5) == AMDVIRT_HV_SUCCESS);

    /* Segment state comes back with the run (ABI 11), no extra call. */
    for (uint32_t i = 0; i < 8; i++) { K.vmcs[0x800 + 2 * i] = 0x10 + i; K.vmcs[0x6806 + 2 * i] = 0x1000 * i; K.vmcs[0x4814 + 2 * i] = 0x93 + i; }
    CHECK(amdvirt_vcpu_run(v) == AMDVIRT_HV_SUCCESS);                                         /* drops the cache */
    int rm = K.readMany, rd = K.vmcsReads;
    CHECK(amdvirt_vcpu_read_register(v, AMDVIRT_X86_CS, &x) == AMDVIRT_HV_SUCCESS && x == 0x11);
    CHECK(amdvirt_vmx_vcpu_read_vmcs(v, 0x6808, &x) == AMDVIRT_HV_SUCCESS && x == 0x1000);    /* CS base */
    CHECK(amdvirt_vmx_vcpu_read_vmcs(v, 0x4816, &x) == AMDVIRT_HV_SUCCESS && x == 0x94);      /* CS access rights */
    CHECK(amdvirt_vcpu_read_register(v, AMDVIRT_X86_TR, &x) == AMDVIRT_HV_SUCCESS && x == 0x17);
    CHECK(K.readMany == rm && K.vmcsReads == rd);
    CHECK(amdvirt_vcpu_run(v) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vcpu_read_register(v, AMDVIRT_X86_DS, &x) == AMDVIRT_HV_SUCCESS && K.readMany == rm && K.vmcsReads == rd);

    /* More writes than fit in one call: flushed on the way, none lost, order kept. */
    uint32_t many[100];
    unsigned nMany = 0;
    for (unsigned i = 0; i < sizeof(kAMDVirtVmcsEncodings) / sizeof(kAMDVirtVmcsEncodings[0]) && nMany < 100; i++)
        if (amdvirt_vmcs_write_status(kAMDVirtVmcsEncodings[i], 0) == AMDVIRT_FIELD_OK && kAMDVirtVmcsEncodings[i] != 0x4002 &&
            kAMDVirtVmcsEncodings[i] != 0x6804)
            many[nMany++] = kAMDVirtVmcsEncodings[i];
    CHECK(nMany == 100);
    for (uint32_t i = 0; i < 150; i++)
        CHECK(amdvirt_vmx_vcpu_write_vmcs(v, many[i % 100], 1000 + i) == AMDVIRT_HV_SUCCESS);       /* the first 50 twice */
    CHECK(amdvirt_vcpu_run(v) == AMDVIRT_HV_SUCCESS);
    for (uint32_t f = 0; f < 100; f++)
        CHECK(K.vmcs[many[f]] == (f < 50 ? 1100 + f : 1000 + f));

    /* A failed run call is an error, and the buffered state does not linger. */
    CHECK(amdvirt_vmx_vcpu_write_vmcs(v, 0x4002, 7) == AMDVIRT_HV_SUCCESS);
    K.failNext = 1;
    CHECK(amdvirt_vcpu_run(v) != AMDVIRT_HV_SUCCESS);
    K.failNext = 0;
    CHECK(amdvirt_vcpu_flush(v) == AMDVIRT_HV_SUCCESS);
}

/* An XSAVE image that marks components XCR0 has not enabled yet (Parallels writes XSTATE_BV=7 at XCR0=1). */
static void testFpuWhole(void)
{
    fresh();
    uint32_t v = makeVcpu();
    uint8_t img[1024], back[1024];
    uint64_t bv, x;
    memset(img, 0, sizeof(img));
    for (int i = 576; i < 832; i++) img[i] = (uint8_t)i;                              /* YMM_Hi data */
    bv = 7;
    memcpy(img + 512, &bv, 8);
    CHECK(amdvirt_vcpu_write_fpstate(v, img, 832) == AMDVIRT_HV_SUCCESS);
    memcpy(&bv, K.fpu.area + 512, 8);
    CHECK(bv == 3 && K.fpu.xcr0 == 1);                                                /* the kernel got what XRSTOR accepts */
    CHECK(amdvirt_vcpu_read_fpstate(v, back, sizeof(back)) == AMDVIRT_HV_SUCCESS && memcmp(back, img, 832) == 0);   /* the client gets it all back */

    CHECK(amdvirt_vcpu_write_register(v, AMDVIRT_X86_XCR0, 7) == AMDVIRT_HV_SUCCESS);
    memcpy(&bv, K.fpu.area + 512, 8);
    CHECK(bv == 7 && K.fpu.xcr0 == 7 && K.fpu.area[600] == (uint8_t)600);             /* AVX state arrived once AVX was enabled */
    CHECK(amdvirt_vcpu_read_register(v, AMDVIRT_X86_XCR0, &x) == AMDVIRT_HV_SUCCESS && x == 7);

    /* The guest running makes the kernel's image the only truth. */
    fresh();
    v = makeVcpu();
    bv = 7; memcpy(img + 512, &bv, 8);
    CHECK(amdvirt_vcpu_write_fpstate(v, img, 832) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vcpu_run(v) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vcpu_write_register(v, AMDVIRT_X86_XCR0, 7) == AMDVIRT_HV_SUCCESS);
    memcpy(&bv, K.fpu.area + 512, 8);
    CHECK(bv == 3);                                                                   /* nothing is resurrected after a run */
}

/* The functions Parallels uses beyond the basics. */
static void testParallelsApi(void)
{
    fresh();
    uint32_t v = makeVcpu();
    uint64_t x = 99;

    K.hostTsc = 5000;
    CHECK(amdvirt_tsc_clock() == 5000);
    CHECK(amdvirt_vcpu_set_tsc_relative(v, -300) == AMDVIRT_HV_SUCCESS && K.vmcs[0x2010] == (uint64_t)-300);
    CHECK(amdvirt_vcpu_set_tsc_relative(7, 0) == AMDVIRT_HV_BAD_ARGUMENT);

    CHECK(amdvirt_vcpu_set_msr_access(v, 0xC0000081, 3) == AMDVIRT_HV_SUCCESS && K.nativeMsr == AMDVIRT_MSR_NATIVE_ENABLE && K.nativeMsrNum == 0xC0000081);
    CHECK(amdvirt_vcpu_set_msr_access(v, 0xC0000081, 0) == AMDVIRT_HV_SUCCESS && K.nativeMsr == AMDVIRT_MSR_NATIVE_DISABLE);
    CHECK(amdvirt_vcpu_set_msr_access(v, 0xC0000081, 4) == AMDVIRT_HV_BAD_ARGUMENT);

    for (uint32_t f = 0; f <= 8; f++)
        CHECK(amdvirt_vmx_get_msr_info(f, &x) == AMDVIRT_HV_SUCCESS && x == 0);
    CHECK(amdvirt_vmx_get_msr_info(9, &x) == AMDVIRT_HV_BAD_ARGUMENT && amdvirt_vmx_get_msr_info(0, NULL) == AMDVIRT_HV_BAD_ARGUMENT);

    CHECK(amdvirt_vmx_vcpu_set_apic_address(v, 0xFEE00000) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vmx_vcpu_set_apic_address(v, 0xFEE00010) == AMDVIRT_HV_BAD_ARGUMENT);              /* not page aligned */
    CHECK(amdvirt_vmx_vcpu_set_apic_address(3, 0xFEE00000) == AMDVIRT_HV_BAD_ARGUMENT);              /* no such vCPU */
    CHECK(amdvirt_vmx_vcpu_set_apic_address_space(v, 0, 0xFEE00000) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vmx_vcpu_set_apic_address_space(v, 1, 0xFEE00000) == AMDVIRT_HV_BAD_ARGUMENT);     /* only space 0 exists */
}

/* The state cache: how many calls reach the kernel, and that nothing stale is ever returned. */
static void testCache(void)
{
    fresh();
    uint32_t v = makeVcpu();
    uint64_t x;
    K.regs.rax = 0x11; K.regs.rip = 0x2000; K.regs.rsp = 0x3000; K.regs.rflags = 0x202;
    K.vmcs[0x4402] = 30; K.vmcs[0x6400] = 0x80;

    /* Sixteen registers, RIP and the VMCS twins of RIP/RSP/RFLAGS: one call. */
    for (uint32_t r = AMDVIRT_X86_RIP; r <= AMDVIRT_X86_R15; r++)
        CHECK(amdvirt_vcpu_read_register(v, r, &x) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vmx_vcpu_read_vmcs(v, 0x681e, &x) == AMDVIRT_HV_SUCCESS && x == 0x2000);
    CHECK(amdvirt_vmx_vcpu_read_vmcs(v, 0x681c, &x) == AMDVIRT_HV_SUCCESS && x == 0x3000);
    CHECK(amdvirt_vmx_vcpu_read_vmcs(v, 0x6820, &x) == AMDVIRT_HV_SUCCESS && x == 0x202);
    CHECK(K.getRegs == 1 && K.vmcsReads == 0);

    /* Other fields are read once. */
    for (int i = 0; i < 5; i++)
        CHECK(amdvirt_vmx_vcpu_read_vmcs(v, 0x4402, &x) == AMDVIRT_HV_SUCCESS && x == 30);
    CHECK(K.vmcsReads == 1);

    /* Register writes are buffered and visible to reads, including as VMCS fields; one write-back. */
    CHECK(amdvirt_vcpu_write_register(v, AMDVIRT_X86_RAX, 0x99) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vcpu_write_register(v, AMDVIRT_X86_RIP, 0x2004) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vmx_vcpu_write_vmcs(v, 0x6820, 0x246) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vcpu_read_register(v, AMDVIRT_X86_RAX, &x) == AMDVIRT_HV_SUCCESS && x == 0x99);
    CHECK(amdvirt_vmx_vcpu_read_vmcs(v, 0x681e, &x) == AMDVIRT_HV_SUCCESS && x == 0x2004);
    CHECK(amdvirt_vcpu_read_register(v, AMDVIRT_X86_RFLAGS, &x) == AMDVIRT_HV_SUCCESS && x == 0x246);
    CHECK(K.setRegs == 0 && K.regs.rax == 0x11 && K.getRegs == 1);
    CHECK(amdvirt_vcpu_run(v) == AMDVIRT_HV_SUCCESS);
    CHECK(K.setRegs == 1 && K.runs == 1);                          /* flushed by the run, before it */

    /* The guest ran: the cache is gone, reads see what the run changed (our fake run moves RIP by 2 and the reason). */
    CHECK(amdvirt_vcpu_read_register(v, AMDVIRT_X86_RIP, &x) == AMDVIRT_HV_SUCCESS && x == 0x2006);
    CHECK(amdvirt_vcpu_read_register(v, AMDVIRT_X86_RAX, &x) == AMDVIRT_HV_SUCCESS && x == 0x99);
    CHECK(amdvirt_vmx_vcpu_read_vmcs(v, 0x4402, &x) == AMDVIRT_HV_SUCCESS && x == 13);
    CHECK(K.getRegs == 2);

    /* A VMCS write is passed through at once, and the old value is not served afterwards. */
    CHECK(amdvirt_vmx_vcpu_read_vmcs(v, 0x4002, &x) == AMDVIRT_HV_SUCCESS && x == 0);
    int w = K.vmcsWrites;
    CHECK(amdvirt_vmx_vcpu_write_vmcs(v, 0x4002, 0x84000004) == AMDVIRT_HV_SUCCESS && K.vmcsWrites == w + 1 && K.vmcs[0x4002] == 0x84000004);
    CHECK(amdvirt_vmx_vcpu_read_vmcs(v, 0x4002, &x) == AMDVIRT_HV_SUCCESS && x == 0x84000004);
    /* An unknown field is an error every time, not a cached zero. */
    CHECK(amdvirt_vmx_vcpu_read_vmcs(v, 0xFFFFF, &x) == AMDVIRT_HV_BAD_ARGUMENT);
    CHECK(amdvirt_vmx_vcpu_read_vmcs(v, 0xFFFFF, &x) == AMDVIRT_HV_BAD_ARGUMENT);

    /* Writing an MSR can change VMCS fields (EFER): the VMCS cache is dropped. */
    K.vmcs[0x2806] = 0x500;
    CHECK(amdvirt_vmx_vcpu_read_vmcs(v, 0x2806, &x) == AMDVIRT_HV_SUCCESS && x == 0x500);
    K.vmcs[0x2806] = 0xd01;
    CHECK(amdvirt_vmx_vcpu_read_vmcs(v, 0x2806, &x) == AMDVIRT_HV_SUCCESS && x == 0x500);       /* still cached */
    CHECK(amdvirt_vcpu_write_msr(v, 0xC0000080, 0xd01) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vmx_vcpu_read_vmcs(v, 0x2806, &x) == AMDVIRT_HV_SUCCESS && x == 0xd01);

    /* Many different fields at once must all stay correct (the cache is a small hash table). */
    for (uint32_t f = 0; f < 100; f++)
        K.vmcs[0x800 + 2 * f] = 1000 + f;
    amdvirt_core_cache_reset();
    for (int pass = 0; pass < 2; pass++)
        for (uint32_t f = 0; f < 100; f++)
            CHECK(amdvirt_vmx_vcpu_read_vmcs(v, 0x800 + 2 * f, &x) == AMDVIRT_HV_SUCCESS && x == 1000 + f);
    CHECK(amdvirt_vmx_vcpu_write_vmcs(v, 0x800 + 2 * 7, 5) == AMDVIRT_HV_SUCCESS);
    for (uint32_t f = 0; f < 100; f++)
        CHECK(amdvirt_vmx_vcpu_read_vmcs(v, 0x800 + 2 * f, &x) == AMDVIRT_HV_SUCCESS && x == (f == 7 ? 5u : 1000 + f));

    /* A dead vCPU leaves nothing behind. */
    CHECK(amdvirt_vcpu_write_register(v, AMDVIRT_X86_RBX, 0x77) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vcpu_destroy(v) == AMDVIRT_HV_SUCCESS);
    uint32_t v2 = 9;
    CHECK(amdvirt_vcpu_create(&v2, 0) == AMDVIRT_HV_SUCCESS);
    K.regs.rbx = 0x55;
    CHECK(amdvirt_vcpu_read_register(v2, AMDVIRT_X86_RBX, &x) == AMDVIRT_HV_SUCCESS && x == 0x55);
}

static void testFpuMsrVmcs(void)
{
    fresh();
    uint32_t v = makeVcpu();
    uint8_t buf[1024];
    for (int i = 0; i < 832; i++) K.fpu.area[i] = (uint8_t)i;
    CHECK(amdvirt_vcpu_read_fpstate(v, buf, 831) == AMDVIRT_HV_BAD_ARGUMENT);              /* buffer too small */
    CHECK(amdvirt_vcpu_read_fpstate(v, NULL, 1024) == AMDVIRT_HV_BAD_ARGUMENT);
    memset(buf, 0, sizeof(buf));
    CHECK(amdvirt_vcpu_read_fpstate(v, buf, sizeof(buf)) == AMDVIRT_HV_SUCCESS);
    CHECK(memcmp(buf, K.fpu.area, 832) == 0 && buf[832] == 0);                      /* exactly size bytes */
    for (int i = 0; i < 832; i++) buf[i] = (uint8_t)(255 - (i & 0xFF));
    memset(buf + 512, 0, 64);                                                       /* a valid, empty header */
    CHECK(amdvirt_vcpu_write_fpstate(v, buf, 100) == AMDVIRT_HV_BAD_ARGUMENT);
    CHECK(amdvirt_vcpu_write_fpstate(v, buf, 832) == AMDVIRT_HV_SUCCESS && memcmp(K.fpu.area, buf, 832) == 0);

    uint64_t x = 0;
    CHECK(amdvirt_vcpu_write_msr(v, 0xC0000082, 0xdead) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vcpu_read_msr(v, 0xC0000082, &x) == AMDVIRT_HV_SUCCESS && x == 0xdead);
    CHECK(amdvirt_vcpu_read_msr(v, 0x1B, &x) == AMDVIRT_HV_SUCCESS && x == 0xFEE00900ull);  /* APIC base: answered by the shim */
    CHECK(amdvirt_vcpu_write_msr(v, 0x1B, 0xFEE00800ull) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vcpu_read_msr(v, 0x1B, &x) == AMDVIRT_HV_SUCCESS && x == 0xFEE00800ull && K.msrCount == 1);   /* never reached the kernel */
    CHECK(amdvirt_vcpu_read_msr(v, 0x80, &x) == AMDVIRT_HV_BAD_ARGUMENT);                   /* other unknown MSRs: the kernel refuses */
    CHECK(amdvirt_vcpu_enable_native_msr(v, 0xC0000081, 1) == AMDVIRT_HV_SUCCESS && K.nativeMsr == AMDVIRT_MSR_NATIVE_ENABLE && K.nativeMsrNum == 0xC0000081);
    CHECK(amdvirt_vcpu_enable_native_msr(v, 0xC0000081, 0) == AMDVIRT_HV_SUCCESS && K.nativeMsr == AMDVIRT_MSR_NATIVE_DISABLE);

    CHECK(amdvirt_vmx_vcpu_write_vmcs(v, 0x4002, 0x8401E172) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vmx_vcpu_read_vmcs(v, 0x4002, &x) == AMDVIRT_HV_SUCCESS && x == 0x8401E172);
    CHECK(amdvirt_vmx_vcpu_read_vmcs(v, 0xFFFFF, &x) == AMDVIRT_HV_BAD_ARGUMENT);          /* kernel refuses */
    CHECK(amdvirt_vmx_vcpu_read_vmcs(v, 0x4002, NULL) == AMDVIRT_HV_BAD_ARGUMENT);

    CHECK(amdvirt_vcpu_flush(v) == AMDVIRT_HV_SUCCESS && amdvirt_vcpu_invalidate_tlb(v) == AMDVIRT_HV_SUCCESS);
    K.hostTsc = 1000;
    CHECK(amdvirt_vm_sync_tsc(5000) == AMDVIRT_HV_SUCCESS && K.vmcs[0x2010] == 4000);       /* guest TSC = host TSC + 4000 */
}

static void *otherThread(void *arg)
{
    int *results = arg;
    uint64_t x;
    K.thread = 2;                                          /* now the "calling thread" is a different one */
    results[0] = amdvirt_vcpu_run(0);
    results[1] = amdvirt_vcpu_read_register(0, AMDVIRT_X86_RIP, &x);
    uint32_t v = 0;
    results[2] = amdvirt_vcpu_interrupt(&v, 1);                /* this one is allowed from anywhere */
    return NULL;
}

static void testRunAndThreads(void)
{
    fresh();
    uint32_t v = makeVcpu();
    CHECK(amdvirt_vcpu_run(v) == AMDVIRT_HV_SUCCESS && K.runs == 1);
    uint64_t t = 0;
    CHECK(amdvirt_vcpu_get_exec_time(v, &t) == AMDVIRT_HV_SUCCESS && t == 1000);
    CHECK(amdvirt_vcpu_run(v) == AMDVIRT_HV_SUCCESS && amdvirt_vcpu_get_exec_time(v, &t) == AMDVIRT_HV_SUCCESS && t == 2000);
    K.failNext = 1;
    CHECK(amdvirt_vcpu_run(v) == AMDVIRT_HV_NO_DEVICE);            /* kernel NotReady */
    K.failNext = 0;

    /* hv_vcpu_interrupt arguments. */
    uint32_t ids[2] = { 0, 0 }, bad = 3;
    CHECK(amdvirt_vcpu_interrupt(ids, 2) == AMDVIRT_HV_SUCCESS && K.kicks == 2);
    CHECK(amdvirt_vcpu_interrupt(&bad, 1) == AMDVIRT_HV_BAD_ARGUMENT);
    CHECK(amdvirt_vcpu_interrupt(NULL, 1) == AMDVIRT_HV_BAD_ARGUMENT && amdvirt_vcpu_interrupt(ids, 0) == AMDVIRT_HV_BAD_ARGUMENT);

    /* vCPU calls belong to the creating thread; hv_vcpu_interrupt does not. */
    int res[3] = { 0, 0, 0 };
    pthread_t th;
    pthread_create(&th, NULL, otherThread, res);
    pthread_join(th, NULL);
    CHECK(res[0] == AMDVIRT_HV_BAD_ARGUMENT);
    CHECK(res[1] == AMDVIRT_HV_BAD_ARGUMENT);
    CHECK(res[2] == AMDVIRT_HV_SUCCESS);
    K.thread = 1;
    CHECK(amdvirt_vcpu_run(v) == AMDVIRT_HV_SUCCESS);
}

static void testErrorMapping(void)
{
    CHECK(amdvirt_hv_from_ioreturn(0) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_hv_from_ioreturn((int)0xe00002bd) == AMDVIRT_HV_NO_RESOURCES);
    CHECK(amdvirt_hv_from_ioreturn((int)0xe00002c1) == AMDVIRT_HV_DENIED);
    CHECK(amdvirt_hv_from_ioreturn((int)0xe00002c2) == AMDVIRT_HV_BAD_ARGUMENT);
    CHECK(amdvirt_hv_from_ioreturn((int)0xe00002c7) == AMDVIRT_HV_UNSUPPORTED);
    CHECK(amdvirt_hv_from_ioreturn((int)0xe00002d5) == AMDVIRT_HV_BUSY);
    CHECK(amdvirt_hv_from_ioreturn((int)0xe00002c0) == AMDVIRT_HV_NO_DEVICE);
    CHECK(amdvirt_hv_from_ioreturn((int)0xe0000999) == AMDVIRT_HV_ERROR);
    CHECK(amdvirt_hv_from_ioreturn(-1) == AMDVIRT_HV_ERROR);
}

static void testCapabilities(void)
{
    uint64_t v;
    CHECK(amdvirt_vmx_read_capability(999, &v) == AMDVIRT_HV_BAD_ARGUMENT);
    CHECK(amdvirt_vmx_read_capability(AMDVIRT_VMX_CAP_PINBASED, NULL) == AMDVIRT_HV_BAD_ARGUMENT);

    /* Every control capability: the bits that must be 1 must also be allowed to be 1. */
    for (uint32_t c = AMDVIRT_VMX_CAP_PINBASED; c <= AMDVIRT_VMX_CAP_TRUE_EXIT; c++) {
        if (c == AMDVIRT_VMX_CAP_BASIC)
            continue;
        CHECK(amdvirt_vmx_read_capability(c, &v) == AMDVIRT_HV_SUCCESS);
        uint32_t must1 = (uint32_t)v, may1 = (uint32_t)(v >> 32);
        CHECK((must1 & ~may1) == 0);
    }

    /* The way QEMU's cap2ctrl builds controls from them: (wanted | must1) & may1. */
    CHECK(amdvirt_vmx_read_capability(AMDVIRT_VMX_CAP_PROCBASED, &v) == AMDVIRT_HV_SUCCESS);
    uint32_t wanted = (1u << 7) | (1u << 10) | (1u << 3) | (1u << 21) | (1u << 22);      /* HLT MWAIT TSC_OFFSET TPR_SHADOW NMI_WINDOW */
    uint32_t ctl = (wanted | (uint32_t)v) & (uint32_t)(v >> 32);
    CHECK((ctl & (1u << 7)) && (ctl & (1u << 10)) && (ctl & (1u << 3)));                  /* what we implement: kept */
    CHECK(!(ctl & (1u << 21)) && !(ctl & (1u << 22)));                                    /* TPR shadow, NMI window: dropped */
    CHECK(!(ctl & (1u << 15)) && !(ctl & (1u << 16)));                                    /* CR3 exiting is not forced */
    amdvirt_core_offer_tpr_shadow = 1;                                                        /* Parallels: TPR shadow on offer */
    CHECK(amdvirt_vmx_read_capability(AMDVIRT_VMX_CAP_PROCBASED, &v) == AMDVIRT_HV_SUCCESS);
    CHECK((uint32_t)(v >> 32) & (1u << 21));
    CHECK(amdvirt_vmx_read_capability(AMDVIRT_VMX_CAP_TRUE_PROCBASED, &v) == AMDVIRT_HV_SUCCESS);
    CHECK((uint32_t)(v >> 32) & (1u << 21));
    amdvirt_core_offer_tpr_shadow = 0;
    CHECK(amdvirt_vmx_read_capability(AMDVIRT_VMX_CAP_PROCBASED2, &v) == AMDVIRT_HV_SUCCESS);
    CHECK(!(((uint32_t)(v >> 32)) & (1u << 0)));                                          /* no APIC virtualization */
    CHECK(((uint32_t)(v >> 32)) & (1u << 7));                                             /* unrestricted guest */
    CHECK(amdvirt_vmx_read_capability(AMDVIRT_VMX_CAP_PINBASED, &v) == AMDVIRT_HV_SUCCESS);
    CHECK(!(((uint32_t)(v >> 32)) & (1u << 6)));                                          /* no preemption timer */
    CHECK(amdvirt_vmx_read_capability(AMDVIRT_VMX_CAP_CR4_FIXED1, &v) == AMDVIRT_HV_SUCCESS && (v & (1u << 13)));    /* VMXE is accepted (QEMU always sets it) */
    CHECK(amdvirt_vmx_read_capability(AMDVIRT_VMX_CAP_CR4_FIXED0, &v) == AMDVIRT_HV_SUCCESS && v == 0);
}


/* ---- several vCPUs: each has its own backend context and its own cached state ---- */
static struct { AMDVirtRegs regs; int kicks, opened, closed; } MV[3];
static int m_vcpu_open(void *c, uint32_t i, void **v) { (void)c; if (i >= 3) return (int)IOR_BAD; MV[i].opened++; *v = (void *)(uintptr_t)(i + 1); return 0; }
static int m_vcpu_close(void *c, uint32_t i, void *v) { (void)c; (void)v; MV[i].closed++; return 0; }
static int m_get_regs(void *c, AMDVirtRegs *r) { *r = MV[(uintptr_t)c - 1].regs; return 0; }
static int m_set_regs(void *c, const AMDVirtRegs *r) { MV[(uintptr_t)c - 1].regs = *r; return 0; }
static int m_kick(void *c) { MV[(uintptr_t)c - 1].kicks++; return 0; }
static int m_vmcs_read(void *c, uint32_t f, uint64_t *v) { *v = (uint64_t)f * 16 + (uintptr_t)c; return 0; }

static void testMultiVcpu(void)
{
    memset(&K, 0, sizeof(K));
    memset(MV, 0, sizeof(MV));
    K.thread = 1;
    AMDVirtBackend b = { NULL, f_vm_create, f_vm_destroy, f_vm_map, f_vm_unmap, f_vm_protect, m_vmcs_read, f_vmcs_write,
                     m_get_regs, m_set_regs, f_get_fpu, f_set_fpu, f_msr, f_run, m_kick, f_now, f_tsc, f_thread,
                     NULL, NULL, m_vcpu_open, m_vcpu_close };
    amdvirt_core_init(&b);
    CHECK(amdvirt_vm_create(0) == AMDVIRT_HV_SUCCESS);
    uint32_t a = 99, c = 99, d = 99, e = 99;
    CHECK(amdvirt_vcpu_create(&a, 0) == AMDVIRT_HV_SUCCESS && a == 0);
    CHECK(amdvirt_vcpu_create(&c, 0) == AMDVIRT_HV_SUCCESS && c == 1);
    CHECK(amdvirt_vcpu_create(&d, 0) == AMDVIRT_HV_SUCCESS && d == 2);
    CHECK(amdvirt_vcpu_create(&e, 0) != AMDVIRT_HV_SUCCESS);                    /* the backend refuses a fourth */

    /* registers are per vCPU */
    CHECK(amdvirt_vcpu_write_register(a, AMDVIRT_X86_RAX, 5) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vcpu_write_register(c, AMDVIRT_X86_RAX, 9) == AMDVIRT_HV_SUCCESS);
    uint64_t v = 0;
    CHECK(amdvirt_vcpu_read_register(a, AMDVIRT_X86_RAX, &v) == AMDVIRT_HV_SUCCESS && v == 5);
    CHECK(amdvirt_vcpu_read_register(c, AMDVIRT_X86_RAX, &v) == AMDVIRT_HV_SUCCESS && v == 9);
    CHECK(amdvirt_vcpu_flush(a) == AMDVIRT_HV_SUCCESS && amdvirt_vcpu_flush(c) == AMDVIRT_HV_SUCCESS);
    CHECK(MV[0].regs.rax == 5 && MV[1].regs.rax == 9);

    /* VMCS reads go to the vCPU asked (the fake answers field*16 + its context) */
    CHECK(amdvirt_vmx_vcpu_read_vmcs(a, 0x4402, &v) == AMDVIRT_HV_SUCCESS && v == 0x4402u * 16 + 1);
    CHECK(amdvirt_vmx_vcpu_read_vmcs(d, 0x4402, &v) == AMDVIRT_HV_SUCCESS && v == 0x4402u * 16 + 3);

    /* hv_vcpu_interrupt kicks exactly the vCPUs named */
    uint32_t list[2] = { c, d };
    CHECK(amdvirt_vcpu_interrupt(list, 2) == AMDVIRT_HV_SUCCESS);
    CHECK(MV[0].kicks == 0 && MV[1].kicks == 1 && MV[2].kicks == 1);
    uint32_t bad = 7;
    CHECK(amdvirt_vcpu_interrupt(&bad, 1) == AMDVIRT_HV_BAD_ARGUMENT);

    /* the APIC base: only the first vCPU is the bootstrap processor */
    CHECK(amdvirt_vcpu_read_msr(a, 0x1B, &v) == AMDVIRT_HV_SUCCESS && (v & 0x100));
    CHECK(amdvirt_vcpu_read_msr(c, 0x1B, &v) == AMDVIRT_HV_SUCCESS && !(v & 0x100) && (v & 0x800));

    /* the VM goes only after its last vCPU */
    CHECK(amdvirt_vcpu_destroy(c) == AMDVIRT_HV_SUCCESS && MV[1].closed == 1);
    CHECK(amdvirt_vm_destroy() == AMDVIRT_HV_BUSY);
    CHECK(amdvirt_vcpu_destroy(a) == AMDVIRT_HV_SUCCESS && amdvirt_vcpu_destroy(d) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vm_destroy() == AMDVIRT_HV_SUCCESS);
    /* a freed slot is reused */
    CHECK(amdvirt_vm_create(0) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vcpu_create(&a, 0) == AMDVIRT_HV_SUCCESS && a == 0);
    CHECK(amdvirt_vcpu_destroy(a) == AMDVIRT_HV_SUCCESS && amdvirt_vm_destroy() == AMDVIRT_HV_SUCCESS);
}

static int hintSlot = -1;
static int hintFn(void) { return hintSlot; }

/* HyperKit: a vCPU thread names the slot it wants, whatever the order of arrival; NMI-window exiting is offered to it. */
static void testHyperkitHooks(void)
{
    memset(&K, 0, sizeof(K));
    memset(MV, 0, sizeof(MV));
    K.thread = 1;
    AMDVirtBackend b = { NULL, f_vm_create, f_vm_destroy, f_vm_map, f_vm_unmap, f_vm_protect, m_vmcs_read, f_vmcs_write,
                     m_get_regs, m_set_regs, f_get_fpu, f_set_fpu, f_msr, f_run, m_kick, f_now, f_tsc, f_thread,
                     NULL, NULL, m_vcpu_open, m_vcpu_close };
    amdvirt_core_init(&b);
    amdvirt_core_vcpu_hint = hintFn;
    CHECK(amdvirt_vm_create(0) == AMDVIRT_HV_SUCCESS);
    uint32_t a = 99, c = 99, d = 99;
    hintSlot = 2;
    CHECK(amdvirt_vcpu_create(&c, 0) == AMDVIRT_HV_SUCCESS && c == 2);          /* asked for 2, got 2 although it came first */
    hintSlot = 0;
    CHECK(amdvirt_vcpu_create(&a, 0) == AMDVIRT_HV_SUCCESS && a == 0);
    hintSlot = 2;
    CHECK(amdvirt_vcpu_create(&d, 0) == AMDVIRT_HV_SUCCESS && d == 1);          /* 2 is taken: the lowest free slot */
    hintSlot = -1;
    amdvirt_core_vcpu_hint = NULL;

    uint64_t cap = 0;
    amdvirt_core_offer_nmi_window = 0;
    CHECK(amdvirt_vmx_read_capability(AMDVIRT_VMX_CAP_PROCBASED, &cap) == AMDVIRT_HV_SUCCESS && !((cap >> 32) & (1u << 22)));
    amdvirt_core_offer_nmi_window = 1;
    CHECK(amdvirt_vmx_read_capability(AMDVIRT_VMX_CAP_PROCBASED, &cap) == AMDVIRT_HV_SUCCESS && ((cap >> 32) & (1u << 22)));
    CHECK(amdvirt_vmx_read_capability(AMDVIRT_VMX_CAP_PINBASED, &cap) == AMDVIRT_HV_SUCCESS && ((cap >> 32) & (1u << 5)));
    amdvirt_core_offer_nmi_window = 0;
    CHECK(amdvirt_vcpu_destroy(a) == AMDVIRT_HV_SUCCESS && amdvirt_vcpu_destroy(c) == AMDVIRT_HV_SUCCESS && amdvirt_vcpu_destroy(d) == AMDVIRT_HV_SUCCESS);
    CHECK(amdvirt_vm_destroy() == AMDVIRT_HV_SUCCESS);
}

int main(void)
{
    testLifecycle();
    testMemory();
    testRegisters();
    testCache();
    testParallelsApi();
    testFpuWhole();
    testMultiVcpu();
    testHyperkitHooks();
    testFastPath();
    testFpuMsrVmcs();
    testRunAndThreads();
    testErrorMapping();
    testCapabilities();
    if (fails) { printf("shim_core_test: %d FAILED\n", fails); return 1; }
    printf("shim_core_test: OK\n");
    return 0;
}
