/*
 * GuestRegions.hpp - the guest physical ranges a client mapped with hv_vm_map, before any of their pages is pinned.
 *
 * AMDVirt pins guest memory lazily: hv_vm_map only records (gpa, size, uva, perm) here, and a page is pinned and entered
 * into the nested page tables the first time the guest touches it (a nested page fault inside a recorded range). unmap
 * and protect work on the recorded ranges too, splitting a range when only part of it is affected.
 *
 * Pure C++ on a fixed array the caller provides (kept sorted by gpa, ranges never overlap), so tests/regions_test.cpp
 * checks it on the host. Not thread safe: the caller serialises.
 */
#ifndef AMDVIRT_GUEST_REGIONS_HPP
#define AMDVIRT_GUEST_REGIONS_HPP

#include <stddef.h>
#include <stdint.h>

namespace amdvirt {
namespace regions {

struct Region {
    uint64_t gpa;
    uint64_t size;
    uint64_t uva;     /* where gpa lives in the client's address space */
    uint32_t perm;    /* amdvirt::npt::PERM_* */
};

class List {
public:
    List() : fR(nullptr), fCap(0), fN(0) {}
    void attach(Region *storage, size_t capacity) { fR = storage; fCap = capacity; fN = 0; }
    size_t count() const { return fN; }
    const Region &at(size_t i) const { return fR[i]; }

    /* Adds [gpa, gpa+size). False when it overlaps a range already there or the list is full. */
    bool add(uint64_t gpa, uint64_t size, uint64_t uva, uint32_t perm)
    {
        if (!size || gpa + size < gpa || fN >= fCap)
            return false;
        size_t i = lowerBound(gpa);
        if (i > 0 && fR[i - 1].gpa + fR[i - 1].size > gpa)
            return false;
        if (i < fN && fR[i].gpa < gpa + size)
            return false;
        insertAt(i, Region{gpa, size, uva, perm});
        return true;
    }

    /* The range containing `gpa`, or nullptr. */
    const Region *find(uint64_t gpa) const
    {
        size_t i = lowerBound(gpa + 1);
        if (i == 0)
            return nullptr;
        const Region &r = fR[i - 1];
        return (gpa >= r.gpa && gpa - r.gpa < r.size) ? &r : nullptr;
    }

    /* Is every byte of [gpa, gpa+size) inside some range? */
    bool covers(uint64_t gpa, uint64_t size) const
    {
        uint64_t at = gpa, end = gpa + size;
        while (at < end) {
            const Region *r = find(at);
            if (!r)
                return false;
            at = r->gpa + r->size;
        }
        return true;
    }

    /* Forgets [gpa, gpa+size), splitting ranges that stick out. False only when the list is full and a split needs a slot. */
    bool remove(uint64_t gpa, uint64_t size)
    {
        if (!splitAt(gpa) || !splitAt(gpa + size))
            return false;
        size_t i = lowerBound(gpa);
        size_t j = i;
        while (j < fN && fR[j].gpa < gpa + size)
            j++;
        eraseRange(i, j);
        return true;
    }

    /* Sets the permission of [gpa, gpa+size) where it is recorded (gaps are left alone). */
    bool protect(uint64_t gpa, uint64_t size, uint32_t perm)
    {
        if (!splitAt(gpa) || !splitAt(gpa + size))
            return false;
        for (size_t i = lowerBound(gpa); i < fN && fR[i].gpa < gpa + size; i++)
            fR[i].perm = perm;
        return true;
    }

    /*
     * The block to pin for a fault at `gpa`: the `chunk`-aligned window around it, clipped to the range that holds it.
     * False when no range holds `gpa`. `chunk` must be a power of two.
     */
    bool chunkFor(uint64_t gpa, uint64_t chunk, uint64_t *start, uint64_t *end, const Region **reg) const
    {
        const Region *r = find(gpa);
        if (!r)
            return false;
        uint64_t s = gpa & ~(chunk - 1), e = s + chunk;
        if (s < r->gpa)
            s = r->gpa;
        if (e > r->gpa + r->size || e < s)
            e = r->gpa + r->size;
        *start = s;
        *end   = e;
        *reg   = r;
        return true;
    }

private:
    /* First index whose gpa is >= `gpa`. */
    size_t lowerBound(uint64_t gpa) const
    {
        size_t lo = 0, hi = fN;
        while (lo < hi) {
            size_t mid = (lo + hi) / 2;
            if (fR[mid].gpa < gpa) lo = mid + 1; else hi = mid;
        }
        return lo;
    }

    void insertAt(size_t i, const Region &r)
    {
        for (size_t k = fN; k > i; k--)
            fR[k] = fR[k - 1];
        fR[i] = r;
        fN++;
    }

    void eraseRange(size_t i, size_t j)
    {
        size_t n = j - i;
        for (size_t k = i; k + n < fN; k++)
            fR[k] = fR[k + n];
        fN -= n;
    }

    /* Makes `at` a range boundary if it falls inside a range. */
    bool splitAt(uint64_t at)
    {
        const Region *c = find(at);
        if (!c || c->gpa == at)
            return true;
        if (fN >= fCap)
            return false;
        size_t i = (size_t)(c - fR);
        Region left = fR[i], right = fR[i];
        left.size  = at - left.gpa;
        right.gpa  = at;
        right.uva += left.size;
        right.size -= left.size;
        fR[i] = left;
        insertAt(i + 1, right);
        return true;
    }

    Region *fR;
    size_t  fCap;
    size_t  fN;
};

} // namespace regions
} // namespace amdvirt

#endif /* AMDVIRT_GUEST_REGIONS_HPP */
