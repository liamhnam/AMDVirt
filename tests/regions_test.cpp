// Host-side tests for kext/Source/GuestRegions.hpp (the ranges hv_vm_map records before lazy pinning).
#include <stdio.h>

#include "../kext/Source/GuestRegions.hpp"

using namespace amdvirt::regions;

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main()
{
    Region store[8];
    List l;
    l.attach(store, 8);

    // add, find, overlap
    CHECK(l.add(0x100000, 0x400000, 0x7000000000, 7));
    CHECK(l.add(0x0, 0xA0000, 0x6000000000, 7));
    CHECK(!l.add(0x200000, 0x1000, 0x1, 7));            // inside the first range
    CHECK(!l.add(0x9F000, 0x2000, 0x1, 7));             // overlaps the end of the second
    CHECK(l.count() == 2 && l.at(0).gpa == 0x0);         // kept sorted
    CHECK(l.find(0x100000) && l.find(0x100000)->uva == 0x7000000000);
    CHECK(!l.find(0xA0000));
    CHECK(l.covers(0x100000, 0x400000) && !l.covers(0x0, 0x100000));

    // chunkFor clips to the range
    uint64_t s, e;
    const Region *r;
    CHECK(l.chunkFor(0x101000, 0x200000, &s, &e, &r) && s == 0x100000 && e == 0x200000);
    CHECK(l.chunkFor(0x4FF000, 0x200000, &s, &e, &r) && s == 0x400000 && e == 0x500000);
    CHECK(l.chunkFor(0x5000, 0x200000, &s, &e, &r) && s == 0x0 && e == 0xA0000);
    CHECK(!l.chunkFor(0xB0000, 0x200000, &s, &e, &r));

    // protect splits and keeps uva consistent
    CHECK(l.protect(0x200000, 0x100000, 1));
    CHECK(l.count() == 4);
    const Region *mid = l.find(0x200000);
    CHECK(mid && mid->perm == 1 && mid->size == 0x100000 && mid->uva == 0x7000100000);
    CHECK(l.find(0x1FF000)->perm == 7 && l.find(0x300000)->perm == 7);
    CHECK(l.find(0x300000)->uva == 0x7000200000);

    // remove a hole in the middle, then the rest
    CHECK(l.remove(0x180000, 0x10000));
    CHECK(!l.find(0x180000) && l.find(0x17F000) && l.find(0x190000));
    CHECK(l.find(0x190000)->uva == 0x7000090000);
    CHECK(l.remove(0x0, 0x1000000));
    CHECK(l.count() == 0);

    // a full list refuses a split instead of corrupting
    for (uint64_t i = 0; i < 8; i++)
        CHECK(l.add(i * 0x10000, 0x10000, 0, 7));
    CHECK(!l.add(0x100000, 0x1000, 0, 7));
    CHECK(!l.protect(0x8000, 0x1000, 1));              // would need two more slots
    CHECK(l.remove(0x0, 0x10000));                      // whole range: no split needed
    CHECK(l.count() == 7);

    if (fails) {
        printf("regions_test: %d FAILED\n", fails);
        return 1;
    }
    printf("regions_test: OK\n");
    return 0;
}
