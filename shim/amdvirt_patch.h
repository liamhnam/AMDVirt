/*
 * amdvirt_patch.h - patches of the applications' own code, found by the bytes around them instead of by an address.
 *
 * Parallels and HyperKit check the CPUID vendor (GenuineIntel), and HyperKit compares the guest linear address of an EPT
 * violation, which AMD does not report. Those are single branches in the applications. A patch here is a pattern of
 * bytes (with `??` for the bytes that change from build to build: displacements, stack offsets), the place in it to
 * change, and what the pattern looks like afterwards. The code is searched in memory, the patch is applied only if the
 * pattern is found exactly once, and a build where it is not found is left alone and says so: no addresses to update
 * for every release, and no way to patch the wrong place.
 *
 * amdvirt_patch_locate / amdvirt_patch_apply are plain C and are tested on the host (tests/patch_test.c).
 */
#ifndef AMDVIRT_PATCH_H
#define AMDVIRT_PATCH_H

#include <stddef.h>
#include <stdint.h>

struct amdvirt_patch {
    const char *name;
    const char *find;      /* hex bytes, "??" matches any byte, spaces are ignored */
    const char *patched;   /* the same bytes after patching, same length; "??" keeps the byte as it is */
};

typedef enum {
    AMDVIRT_PATCH_APPLIED = 0,   /* found once and changed */
    AMDVIRT_PATCH_ALREADY,       /* found once in its patched form (a file patched by an older install) */
    AMDVIRT_PATCH_NOT_FOUND,     /* neither form is in the code: another build of the application */
    AMDVIRT_PATCH_AMBIGUOUS,     /* more than one place matches, nothing is touched */
    AMDVIRT_PATCH_BAD_PATTERN,
    AMDVIRT_PATCH_NO_ACCESS      /* could not make the code writable */
} amdvirt_patch_result;

const char *amdvirt_patch_result_name(amdvirt_patch_result r);

/* Looks for the patch in buf[0..len). On AMDVIRT_PATCH_APPLIED / AMDVIRT_PATCH_ALREADY, *offset is where the pattern starts. */
amdvirt_patch_result amdvirt_patch_locate(const uint8_t *buf, size_t len, const struct amdvirt_patch *p, size_t *offset);

/* Writes the patched bytes over the pattern at buf + offset (the "??" bytes stay). */
int amdvirt_patch_apply(uint8_t *at, const struct amdvirt_patch *p);

/* Patches the code of a loaded image (the __TEXT segment of `mh`), in memory. Returns what amdvirt_patch_locate found. */
struct mach_header;
amdvirt_patch_result amdvirt_patch_image(const struct mach_header *mh, const struct amdvirt_patch *p);

/*
 * Parallels' monitor (libMonitor.dylib) drops key presses until its port-0x80 (POST code) handler has run with the value 0. To run that handler
 * from outside, three things are needed: the handler itself and the two flags it sets. They are found in the code, not by address: the handler by
 * the bytes of its start, the flags by decoding the rip-relative operands of the two instructions that touch them. All three are offsets from the
 * start of the image (its __TEXT segment starts at the Mach-O header). Found only if the code looks exactly like this, once.
 */
struct amdvirt_monitor_kbd {
    size_t handler;     /* int handler(uint32_t port, uint32_t size, uint64_t count, void *data) */
    size_t armed;       /* the flag in the monitor's saved state (a byte) */
    size_t second;      /* a global of the library (a byte): the handler does nothing while it is set */
};
amdvirt_patch_result amdvirt_find_monitor_kbd(const uint8_t *code, size_t len, struct amdvirt_monitor_kbd *out);
#ifdef __APPLE__
amdvirt_patch_result amdvirt_find_monitor_kbd_image(const struct mach_header *mh, struct amdvirt_monitor_kbd *out);
/* The same, remembered for the last image asked about (the search reads about 1 MB of code, the callers ask on every keyboard access); thread-safe.
 * *fresh is set to 1 when this call did the search, so the caller can log the result once per load of the monitor. */
amdvirt_patch_result amdvirt_find_monitor_kbd_cached(const struct mach_header *mh, struct amdvirt_monitor_kbd *out, int *fresh);
#endif

/*
 * A patch with several forms (the same check compiled with other registers or another kind of branch): exactly one form must be found, once, in total.
 * *which is the index of the form that was found.
 */
amdvirt_patch_result amdvirt_patch_locate_any(const uint8_t *buf, size_t len, const struct amdvirt_patch *forms, unsigned n, size_t *offset, unsigned *which);
amdvirt_patch_result amdvirt_patch_image_any(const struct mach_header *mh, const struct amdvirt_patch *forms, unsigned n);

/* The patches themselves. */
extern const struct amdvirt_patch amdvirt_patch_prl_vendor[];      /* prl_vm_app and libMonitor.dylib: CPUID vendor check, in several forms */
extern const unsigned amdvirt_patch_prl_vendor_count;
extern const struct amdvirt_patch amdvirt_patch_hyperkit_vendor;
extern const struct amdvirt_patch amdvirt_patch_hyperkit_gla;

#endif
