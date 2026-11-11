/*
 * hvf-race-test - hv_vm_destroy while other threads of the same process call hv_vm_map / hv_vm_unmap / hv_vm_protect.
 *
 * Before the trap guards, the destroying thread freed the VM under the mapping threads (a use-after-free in the kernel). Now every
 * call either works or fails cleanly (HV_NO_DEVICE after the destroy), and the machine stays up. Runs ROUNDS rounds.
 *
 *     ./build/hvf-race-test [rounds=20]
 */
#include <Hypervisor/hv.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define THREADS 4
#define PAGES   64

static void *gMem;
static atomic_int gStop;
static atomic_long gOk, gFail;

static void *mapper(void *arg)
{
    long t = (long)arg;
    while (!atomic_load(&gStop)) {
        for (int i = 0; i < PAGES; i++) {
            hv_gpaddr_t gpa = 0x100000ull * (uint64_t)(t + 1) + 0x1000ull * (uint64_t)i;
            void *uva = (char *)gMem + 0x1000 * i;
            if (hv_vm_map(uva, gpa, 0x1000, HV_MEMORY_READ | HV_MEMORY_WRITE) == HV_SUCCESS) atomic_fetch_add(&gOk, 1);
            else atomic_fetch_add(&gFail, 1);
            hv_vm_protect(gpa, 0x1000, HV_MEMORY_READ);
            hv_vm_unmap(gpa, 0x1000);
        }
    }
    return NULL;
}

int main(int argc, char **argv)
{
    int rounds = argc > 1 ? atoi(argv[1]) : 20;
    gMem = mmap(NULL, 0x1000 * PAGES, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (gMem == MAP_FAILED) { perror("mmap"); return 1; }
    for (int r = 0; r < rounds; r++) {
        hv_return_t c = hv_vm_create(HV_VM_DEFAULT);
        if (c != HV_SUCCESS) { fprintf(stderr, "round %d: hv_vm_create failed: %#x\n", r, (unsigned)c); return 1; }
        atomic_store(&gStop, 0);
        pthread_t th[THREADS];
        for (long t = 0; t < THREADS; t++) pthread_create(&th[t], NULL, mapper, (void *)t);
        usleep(20000 + (useconds_t)(r % 7) * 3000);
        hv_return_t d = hv_vm_destroy();        /* while the mappers run */
        atomic_store(&gStop, 1);
        for (int t = 0; t < THREADS; t++) pthread_join(th[t], NULL);
        if (d != HV_SUCCESS) { fprintf(stderr, "round %d: hv_vm_destroy failed: %#x\n", r, (unsigned)d); return 1; }
        if ((r + 1) % 50 == 0) printf("  %d rounds\n", r + 1);
    }
    printf("PASS: %d rounds of hv_vm_destroy against %d mapping threads (%ld maps ok, %ld refused)\n", rounds, THREADS,
           atomic_load(&gOk), atomic_load(&gFail));
    return 0;
}
