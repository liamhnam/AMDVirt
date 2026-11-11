/*
 * Log.hpp - kernel-side logging for the kext.
 *
 * Messages go into a 64 KiB ring buffer that userland reads with
 * `amdvirt-info log [-f]`, and are mirrored to IOLog (system log) up to the
 * level given by boot-arg amdvirt_iolog (default INFO).
 *
 * Boot-args:
 *   amdvirt_loglevel=N   ring level, 0..5 (none, error, warn, info, debug, trace)
 *   amdvirt_iolog=N      IOLog mirror level, same scale
 *
 * Rules for call sites:
 *   - never log from a mp_rendezvous action or any interrupt-disabled context
 *   - use AMDVIRT_SYNC right BEFORE something that can panic: it always reaches
 *     IOLog immediately, so the last line before a crash is visible
 */
#ifndef AMDVIRT_LOG_HPP
#define AMDVIRT_LOG_HPP

#include <stdint.h>
#include <libkern/OSAtomic.h>
#include "../../include/AMDVirtShared.h"

namespace amdvirt {

/* Defined in Log.cpp. */
extern AMDVirtStats gStats;

namespace log {

void     init();
void     write(uint32_t level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void     writeSync(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void     assertFailed(const char *expr, const char *file, int line);

/* Fills a AMDVirtLogChunk from `cursor`. `out` must point to a full AMDVIRT_LOG_CHUNK_SIZE buffer. */
void     read(uint64_t cursor, AMDVirtLogChunk *out);

void     setLevel(uint32_t level);
uint32_t level();

} // namespace log
} // namespace amdvirt

/* Atomic bumps of the counters exposed by `amdvirt-info stats`. */
#define AMDVIRT_STAT_INC(field) OSIncrementAtomic64((volatile SInt64 *)&amdvirt::gStats.field)
#define AMDVIRT_STAT_ADD(field, n) OSAddAtomic64((SInt64)(n), (volatile SInt64 *)&amdvirt::gStats.field)

#define AMDVIRT_ERR(...)   amdvirt::log::write(AMDVIRT_LOG_ERROR, __VA_ARGS__)
#define AMDVIRT_WARN(...)  amdvirt::log::write(AMDVIRT_LOG_WARN, __VA_ARGS__)
#define AMDVIRT_INFO(...)  amdvirt::log::write(AMDVIRT_LOG_INFO, __VA_ARGS__)
#define AMDVIRT_DBG(...)   amdvirt::log::write(AMDVIRT_LOG_DEBUG, __VA_ARGS__)
#define AMDVIRT_TRACE(...) amdvirt::log::write(AMDVIRT_LOG_TRACE, __VA_ARGS__)
#define AMDVIRT_SYNC(...)  amdvirt::log::writeSync(__VA_ARGS__)

/* Does not panic: logs, counts, and lets you see it in `amdvirt-info stats`. */
#define AMDVIRT_ASSERT(cond)                                                    \
    do {                                                                    \
        if (!(cond))                                                        \
            amdvirt::log::assertFailed(#cond, __FILE__, __LINE__);              \
    } while (0)

#endif /* AMDVIRT_LOG_HPP */
