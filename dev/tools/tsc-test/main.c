/*
 * tsc-test: checks that a guest's TSC follows hv_vcpu_set_tsc_relative and hv_vcpu_write_msr(HV_MSR_TSC),
 * the way Virtualization.framework sets it (one relative value for every vCPU).
 *
 * Guest (real mode, at 0x1000): RDTSC; HLT. The guest's EDX:EAX after the HLT exit is its TSC.
 *   1. set_tsc_relative(-hv_tsc_clock())  -> the guest's TSC must be small (it started at ~0)
 *   2. write_msr(TSC, 1 << 40)            -> the guest's TSC must be just above 1 << 40
 *   3. two vCPUs, same relative value     -> their TSCs must be close to each other
 * Build: cc -arch x86_64 -O2 -o tsc-test main.c -framework Hypervisor && codesign -s - -f --entitlements hypervisor.entitlements tsc-test
 */
#include <Hypervisor/Hypervisor.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#define RAM_SIZE (1u << 20)
static int failures;
#define EXPECT(c, ...) do { printf((c) ? "  ok: " : "  FAIL: "); printf(__VA_ARGS__); printf("\n"); if (!(c)) failures++; } while (0)

static const uint8_t kProg[] = { 0x0F, 0x31, 0xF4, 0xEB, 0xFD };   /* rdtsc; hlt; jmp $-1 */

static void wvmcs(hv_vcpuid_t v, uint32_t f, uint64_t x) { hv_vmx_vcpu_write_vmcs(v, f, x); }
static uint64_t rvmcs(hv_vcpuid_t v, uint32_t f) { uint64_t x = 0; hv_vmx_vcpu_read_vmcs(v, f, &x); return x; }
static uint64_t cap2ctrl(uint64_t cap, uint64_t ctrl) { return (ctrl | (cap & 0xffffffff)) & (cap >> 32); }

static void setup(hv_vcpuid_t v)
{
    static const uint32_t seg[8][4] = {
        { VMCS_GUEST_ES, VMCS_GUEST_ES_LIMIT, VMCS_GUEST_ES_AR, VMCS_GUEST_ES_BASE },
        { VMCS_GUEST_CS, VMCS_GUEST_CS_LIMIT, VMCS_GUEST_CS_AR, VMCS_GUEST_CS_BASE },
        { VMCS_GUEST_SS, VMCS_GUEST_SS_LIMIT, VMCS_GUEST_SS_AR, VMCS_GUEST_SS_BASE },
        { VMCS_GUEST_DS, VMCS_GUEST_DS_LIMIT, VMCS_GUEST_DS_AR, VMCS_GUEST_DS_BASE },
        { VMCS_GUEST_FS, VMCS_GUEST_FS_LIMIT, VMCS_GUEST_FS_AR, VMCS_GUEST_FS_BASE },
        { VMCS_GUEST_GS, VMCS_GUEST_GS_LIMIT, VMCS_GUEST_GS_AR, VMCS_GUEST_GS_BASE },
        { VMCS_GUEST_LDTR, VMCS_GUEST_LDTR_LIMIT, VMCS_GUEST_LDTR_AR, VMCS_GUEST_LDTR_BASE },
        { VMCS_GUEST_TR, VMCS_GUEST_TR_LIMIT, VMCS_GUEST_TR_AR, VMCS_GUEST_TR_BASE },
    };
    for (int i = 0; i < 8; i++) {
        wvmcs(v, seg[i][0], 0); wvmcs(v, seg[i][1], 0xFFFF); wvmcs(v, seg[i][3], 0);
        wvmcs(v, seg[i][2], i == 1 ? 0x9B : i == 6 ? 0x82 : i == 7 ? 0x8B : 0x93);
    }
    wvmcs(v, VMCS_GUEST_GDTR_LIMIT, 0xFFFF); wvmcs(v, VMCS_GUEST_IDTR_LIMIT, 0xFFFF);
    wvmcs(v, VMCS_GUEST_CR0, 0x30); wvmcs(v, VMCS_GUEST_CR4, 0); wvmcs(v, VMCS_GUEST_IA32_EFER, 0);
    uint64_t pin = 0, proc = 0, proc2 = 0, entry = 0;
    hv_vmx_read_capability(HV_VMX_CAP_PINBASED, &pin);
    hv_vmx_read_capability(HV_VMX_CAP_PROCBASED, &proc);
    hv_vmx_read_capability(HV_VMX_CAP_PROCBASED2, &proc2);
    hv_vmx_read_capability(HV_VMX_CAP_ENTRY, &entry);
    wvmcs(v, VMCS_CTRL_PIN_BASED, cap2ctrl(pin, 0));
    wvmcs(v, VMCS_CTRL_CPU_BASED, cap2ctrl(proc, CPU_BASED_HLT | CPU_BASED_TSC_OFFSET) | CPU_BASED_SECONDARY_CTLS);
    wvmcs(v, VMCS_CTRL_CPU_BASED2, cap2ctrl(proc2, 0));
    wvmcs(v, VMCS_CTRL_VMENTRY_CONTROLS, cap2ctrl(entry, 0));
    hv_vcpu_write_register(v, HV_X86_RFLAGS, 2);
    hv_vcpu_write_register(v, HV_X86_RIP, 0x1000);
}

/* Runs to the HLT and returns the TSC the guest read. */
static uint64_t guest_tsc(hv_vcpuid_t v)
{
    hv_vcpu_write_register(v, HV_X86_RIP, 0x1000);
    for (int i = 0; i < 1000; i++) {
        if (hv_vcpu_run(v) != HV_SUCCESS) return 0;
        if ((rvmcs(v, VMCS_RO_EXIT_REASON) & 0xFFFF) == VMX_REASON_HLT) break;
    }
    uint64_t a = 0, d = 0;
    hv_vcpu_read_register(v, HV_X86_RAX, &a);
    hv_vcpu_read_register(v, HV_X86_RDX, &d);
    return (d << 32) | (a & 0xffffffff);
}

static uint64_t g_rel, g_tsc1;

static void *second_vcpu(void *arg)
{
    (void)arg;
    hv_vcpuid_t v1 = 0;
    if (hv_vcpu_create(&v1, HV_VCPU_DEFAULT) != HV_SUCCESS)
        return NULL;
    setup(v1);
    hv_vcpu_set_tsc_relative(v1, g_rel);
    g_tsc1 = guest_tsc(v1);
    hv_vcpu_destroy(v1);
    return NULL;
}

int main(void)
{
    if (hv_vm_create(HV_VM_DEFAULT) != HV_SUCCESS) { fprintf(stderr, "hv_vm_create failed\n"); return 1; }
    uint8_t *ram = mmap(NULL, RAM_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    memcpy(ram + 0x1000, kProg, sizeof(kProg));
    hv_vm_map(ram, 0, RAM_SIZE, HV_MEMORY_READ | HV_MEMORY_WRITE | HV_MEMORY_EXEC);
    hv_vcpuid_t v0 = 0;
    if (hv_vcpu_create(&v0, HV_VCPU_DEFAULT) != HV_SUCCESS) { fprintf(stderr, "hv_vcpu_create failed\n"); return 1; }
    setup(v0);

    const uint64_t host = hv_tsc_clock();
    uint64_t raw = guest_tsc(v0);
    printf("host TSC (hv_tsc_clock) %#llx, guest TSC before any write %#llx\n", host, raw);

    /* 1. relative, as Virtualization.framework does it at the start of a VM: the guest's TSC starts at about 0 */
    hv_vcpu_set_tsc_relative(v0, (uint64_t)0 - hv_tsc_clock());
    uint64_t t1 = guest_tsc(v0);
    EXPECT(t1 < (1ull << 36), "set_tsc_relative(-hv_tsc_clock()): guest TSC %#llx is small", t1);

    /* 2. absolute through the TSC MSR */
    hv_vcpu_write_msr(v0, 0x10, 1ull << 40);
    uint64_t t2 = guest_tsc(v0);
    EXPECT(t2 >= (1ull << 40) && t2 < (1ull << 40) + (1ull << 36), "write_msr(TSC, 1<<40): guest TSC %#llx", t2);

    /* 3. a second vCPU (its own thread, as Hypervisor.framework requires) with the same relative value agrees with the first */
    g_rel = (uint64_t)0 - hv_tsc_clock();
    hv_vcpu_set_tsc_relative(v0, g_rel);
    pthread_t th;
    pthread_create(&th, NULL, second_vcpu, NULL);
    uint64_t a = guest_tsc(v0);
    pthread_join(th, NULL);
    uint64_t b = g_tsc1;
    uint64_t diff = a > b ? a - b : b - a;
    EXPECT(b != 0 && diff < (1ull << 32), "two vCPUs with one relative value: %#llx and %#llx (differ by %#llx)", a, b, diff);
    hv_vcpu_destroy(v0);
    hv_vm_destroy();
    printf(failures ? "tsc-test: %d FAILED\n" : "tsc-test: OK\n", failures);
    return failures != 0;
}
