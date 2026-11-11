/*
 * Host-side unit test for the pure helpers in AMDVirtShared.h.
 * Builds with any C compiler, no macOS SDK needed:  make test
 */
#include <stdio.h>
#include <stdlib.h>

#include "../include/AMDVirtRing.h"
#include "../include/AMDVirtShared.h"

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
            exit(1);                                                         \
        }                                                                    \
    } while (0)

static void test_ring_basic(void)
{
    char storage[16], out[32];
    AMDVirtRing r;
    uint64_t next, dropped;

    amdvirt_ring_init(&r, storage, sizeof(storage));
    amdvirt_ring_write(&r, "hello", 5);

    uint32_t n = amdvirt_ring_read(&r, 0, out, sizeof(out), &next, &dropped);
    CHECK(n == 5 && memcmp(out, "hello", 5) == 0);
    CHECK(next == 5 && dropped == 0);

    /* Nothing new: zero bytes, cursor stays. */
    n = amdvirt_ring_read(&r, next, out, sizeof(out), &next, &dropped);
    CHECK(n == 0 && next == 5 && dropped == 0);
}

static void test_ring_wrap(void)
{
    char storage[8], out[32];
    AMDVirtRing r;
    uint64_t next, dropped;

    amdvirt_ring_init(&r, storage, sizeof(storage));
    amdvirt_ring_write(&r, "AAAAAA", 6);   /* head 6 */
    amdvirt_ring_write(&r, "BBBBBB", 6);   /* head 12, ring holds the last 8: "AABBBBBB" */

    /* A reader still at 0 lost the first 4 bytes. */
    uint32_t n = amdvirt_ring_read(&r, 0, out, sizeof(out), &next, &dropped);
    CHECK(dropped == 4);
    CHECK(n == 8 && memcmp(out, "AABBBBBB", 8) == 0);
    CHECK(next == 12);

    /* A reader that kept up loses nothing. */
    n = amdvirt_ring_read(&r, 6, out, sizeof(out), &next, &dropped);
    CHECK(dropped == 0 && n == 6 && memcmp(out, "BBBBBB", 6) == 0);
}

static void test_ring_oversized_write(void)
{
    char storage[4], out[32];
    AMDVirtRing r;
    uint64_t next, dropped;

    amdvirt_ring_init(&r, storage, sizeof(storage));
    amdvirt_ring_write(&r, "0123456789", 10);   /* only "6789" survives */

    uint32_t n = amdvirt_ring_read(&r, 0, out, sizeof(out), &next, &dropped);
    CHECK(dropped == 6);
    CHECK(n == 4 && memcmp(out, "6789", 4) == 0);
    CHECK(next == 10);
}

static void test_ring_chunked_reader(void)
{
    char storage[64], out[3];
    AMDVirtRing r;
    uint64_t cursor = 0, dropped;
    char result[64];
    size_t got = 0;

    amdvirt_ring_init(&r, storage, sizeof(storage));
    amdvirt_ring_write(&r, "abcdefghij", 10);

    /* Reader with a 3-byte buffer reassembles the stream. */
    for (;;) {
        uint32_t n = amdvirt_ring_read(&r, cursor, out, sizeof(out), &cursor, &dropped);
        if (!n)
            break;
        memcpy(result + got, out, n);
        got += n;
    }
    CHECK(got == 10 && memcmp(result, "abcdefghij", 10) == 0);

    /* A cursor from the future (stale client) is clamped, not trusted. */
    uint32_t n = amdvirt_ring_read(&r, 9999, out, sizeof(out), &cursor, &dropped);
    CHECK(n == 0 && cursor == 10);
}

int main(void)
{
    /* Ryzen 7 5800H (Cezanne): CPUID Fn0000_0001 EAX = 0x00A50F00 -> family 19h, model 50h. */
    CHECK(amdvirt_amd_family(0x00A50F00u) == 0x19);
    CHECK(amdvirt_amd_model(0x00A50F00u) == 0x50);
    CHECK(amdvirt_amd_stepping(0x00A50F00u) == 0);

    /* Zen 2 (Matisse): EAX = 0x00870F10 -> family 17h, model 71h, stepping 0. */
    CHECK(amdvirt_amd_family(0x00870F10u) == 0x17);
    CHECK(amdvirt_amd_model(0x00870F10u) == 0x71);

    /* Vendor string "AuthenticAMD". */
    CHECK(amdvirt_is_amd_vendor(0x68747541u, 0x444D4163u, 0x69746E65u));
    /* "GenuineIntel": EBX=Genu, EDX=ineI, ECX=ntel. */
    CHECK(!amdvirt_is_amd_vendor(0x756E6547u, 0x6C65746Eu, 0x49656E69u));

    /* Struct layouts are part of the kext/userland ABI. */
    CHECK(sizeof(AMDVirtInfo) == 14 * sizeof(uint32_t));
    CHECK(sizeof(AMDVirtStats) == 25 * sizeof(uint64_t));
    CHECK(sizeof(AMDVirtRegs) == 18 * sizeof(uint64_t));
    CHECK(sizeof(AMDVirtExit) == 48);
    CHECK(sizeof(AMDVirtLogChunk) == AMDVIRT_LOG_CHUNK_SIZE);
    CHECK(kAMDVirtMethodCount == 24);   /* 24th selector (HvControl) appended, ABI stays 11 */
    CHECK(kAMDVirtMethodVmMsr == 18 && kAMDVirtMethodVmKick == 19);
    CHECK(kAMDVirtMethodVmRunEx == 20 && kAMDVirtMethodVmcsReadMany == 21 && kAMDVirtMethodVmJoin == 22);
    CHECK(sizeof(AMDVirtRunIn) == 1688 && sizeof(AMDVirtRunOut) == 8 + 48 + sizeof(AMDVirtRegs) + 55 * 8 && sizeof(AMDVirtReadMany) == 196 && sizeof(AMDVirtReadManyOut) == 384);
    CHECK(kAMDVirtMethodVmGetFpu == 16 && kAMDVirtMethodVmSetFpu == 17);
    CHECK(kAMDVirtMethodVmcsRead == 14 && kAMDVirtMethodVmcsWrite == 15);
    CHECK(kAMDVirtMethodVmMap == 11 && kAMDVirtMethodVmUnmap == 12 && kAMDVirtMethodVmProtect == 13);
    CHECK(AMDVIRT_ABI_VERSION == 11);
    CHECK(sizeof(AMDVirtFpuState) == 16 + AMDVIRT_FPU_AREA_MAX);

    /* NPF decoding: write to a present (read-only) page; fetch from a not-present page; walk fault. */
    CHECK(amdvirt_npf_present(0x3) && amdvirt_npf_is_write(0x3) && !amdvirt_npf_is_fetch(0x3));
    CHECK(!amdvirt_npf_present(0x14) && amdvirt_npf_is_fetch(0x14) && !amdvirt_npf_is_write(0x14));
    CHECK(amdvirt_npf_in_walk(1ull << 33) && !amdvirt_npf_in_walk(1ull << 32));

    /* IOIO decoding: OUT DX,AL to port 0x3F8 as reported by SVM. */
    {
        uint64_t info1 = ((uint64_t)0x3F8 << 16) | (1u << 4);
        CHECK(amdvirt_ioio_port(info1) == 0x3F8);
        CHECK(!amdvirt_ioio_is_in(info1) && !amdvirt_ioio_is_string(info1) && !amdvirt_ioio_has_rep(info1));
        CHECK(amdvirt_ioio_size(info1) == 1);
        CHECK(amdvirt_ioio_is_in(info1 | 1) && amdvirt_ioio_size(((uint64_t)0x60 << 16) | (1u << 6) | 1) == 4);
    }

    test_ring_basic();
    test_ring_wrap();
    test_ring_oversized_write();
    test_ring_chunked_reader();

    puts("host_test: OK");
    return 0;
}
