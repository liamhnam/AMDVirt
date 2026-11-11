#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>
#include <pexpert/pexpert.h>

#include "HvFrontend.hpp"
#include "Log.hpp"
#include "AMDVirt.hpp"
#include "SVM.hpp"

#define super IOService

#define AMDVIRT_VERSION "0.7.9"

OSDefineMetaClassAndStructors(com_liamhnam_AMDVirt, IOService)

namespace {

bool bootArgFlag(const char *name)
{
    uint32_t v = 0;
    return PE_parse_boot_argn(name, &v, sizeof(v));
}

bool bootArgValue(const char *name, uint32_t *out)
{
    return PE_parse_boot_argn(name, out, sizeof(*out));
}

/*
 * Bisecting a crash at boot: amdvirt_stage=N runs the start-up steps up to N and stops there.
 * Each step is announced with plain IOLog BEFORE it runs, so with -v the last line on screen
 * is the step that was running when the machine died. amdvirt_slow=N (seconds, max 10) waits after
 * each announcement so there is time to read or photograph it.
 */
enum {
    STAGE_LOADED   = 0,   /* start() entered, nothing else touched; returns false */
    STAGE_LOG      = 1,   /* + IOService::start, log ring, banner */
    STAGE_CPUID    = 2,   /* + CPUID probe on every CPU (no MSR reads) */
    STAGE_MSR      = 3,   /* + VM_CR / EFER reads (the full read-only probe) */
    STAGE_SERVICE  = 4,   /* + sleep/wake notifier, registerService, amdvirt-info works */
    STAGE_ENABLE   = 5    /* + honour amdvirt_enable_svm (default when amdvirt_stage is not given) */
};

uint32_t gStage = STAGE_ENABLE;
uint32_t gSlow  = 0;

void announce(uint32_t stage, const char *what)
{
    IOLog("AMDVirt: [stage %u] %s\n", stage, what);
    if (gSlow)
        IOSleep(gSlow * 1000);
}

} // namespace

bool com_liamhnam_AMDVirt::start(IOService *provider)
{
    /* Kill switch first, before we touch anything. Plain IOLog: our own log is not up yet. */
    if (bootArgFlag("-amdvirtoff")) {
        IOLog("AMDVirt: boot-arg -amdvirtoff present, not starting\n");
        return false;
    }

    if (bootArgValue("amdvirt_stage", &gStage) && gStage > STAGE_ENABLE)
        gStage = STAGE_ENABLE;
    if (bootArgValue("amdvirt_slow", &gSlow) && gSlow > 10)
        gSlow = 10;

    announce(STAGE_LOADED, "start() reached");
    if (gStage < STAGE_LOG)
        return false;

    announce(STAGE_LOG, "IOService::start and log ring");
    if (!super::start(provider))
        return false;

    amdvirt::log::init();
    AMDVIRT_INFO("AMDVirt " AMDVIRT_VERSION " (ABI %d) built " __DATE__ " " __TIME__, AMDVIRT_ABI_VERSION);

    if (gStage < STAGE_CPUID) {
        AMDVIRT_INFO("amdvirt_stage=%u: stopping after the banner", gStage);
        return true;
    }
    bool readMsrs = gStage >= STAGE_MSR;
    announce(STAGE_CPUID, "CPUID probe on every CPU");
    if (readMsrs)
        announce(STAGE_MSR, "reading VM_CR and EFER on every CPU");
    amdvirt::svm::probe(&fInfo, readMsrs);
    if (!readMsrs)
        AMDVIRT_INFO("amdvirt_stage=%u: MSRs were not read, BIOS-disabled/locked flags are unknown", gStage);

    AMDVIRT_INFO("cpus=%u amd=%u family=0x%x model=0x%x step=%u", fInfo.nCpus, fInfo.vendorIsAmd,
             fInfo.cpuFamily, fInfo.cpuModel, fInfo.cpuStepping);
    AMDVIRT_INFO("svm=%u disabledByBios=%u locked=%u rev=%u asids=%u features=0x%x usableCpus=%u",
             fInfo.svmSupported, fInfo.svmDisabledByBios, fInfo.svmLocked, fInfo.svmRevision,
             fInfo.numAsids, fInfo.svmFeatures, fInfo.nCpusSvmUsable);

    uint32_t selftest = 0;
    bootArgValue("amdvirt_selftest", &selftest);
    fSelfTest = selftest != 0;

    if (gStage < STAGE_SERVICE) {
        AMDVIRT_INFO("amdvirt_stage=%u: not registering the service", gStage);
        return true;
    }
    announce(STAGE_SERVICE, "sleep/wake notifier and registerService");

    /* IOPMrootDomain is only forward-declared in the public SDK headers. It derives from
     * IOService with single inheritance, so a reinterpret_cast is safe. */
    IOService *root = reinterpret_cast<IOService *>(getPMRootDomain());
    fPMNotifier = root ? root->registerInterest(gIOPriorityPowerStateInterest,
                                                &com_liamhnam_AMDVirt::sleepWakeHandler, this)
                       : nullptr;
    if (!fPMNotifier)
        AMDVIRT_WARN("could not register for sleep/wake notifications");

    registerService();

    if (gStage < STAGE_ENABLE)
        return true;

    if (!fInfo.vendorIsAmd || !fInfo.svmSupported) {
        AMDVIRT_WARN("no SVM on this CPU, diagnostics only");
    } else if (fInfo.svmDisabledByBios) {
        AMDVIRT_WARN("SVM is disabled in firmware (VM_CR.SVMDIS), enable 'SVM Mode' in BIOS");
    } else if (fInfo.nCpusSvmUsable != fInfo.nCpus) {
        AMDVIRT_WARN("SVM is usable on only %u of %u CPUs, refusing to enable", fInfo.nCpusSvmUsable,
                 fInfo.nCpus);
    } else {
        /* SVM is enabled by default when every CPU can use it, so the kext works without boot-args (a VM app installed the usual way
         * just runs). amdvirt_enable_svm=0 keeps it to the probe; -amdvirtoff keeps the kext from starting at all. */
        uint32_t enable = 1;
        bootArgValue("amdvirt_enable_svm", &enable);
        if (enable) {
            announce(STAGE_ENABLE, "enabling SVM (EFER.SVME and VM_HSAVE_PA on every CPU)");
            uint32_t n = amdvirt::svm::enableAll();
            if (n && fSelfTest) {
                uint32_t ok, total;
                amdvirt::svm::verifyAll(&ok, &total);
            }
        } else {
            AMDVIRT_INFO("probe only (boot-arg amdvirt_enable_svm=0)");
        }
    }
    amdvirt::hv::start(this);        /* front end A, only with boot-arg amdvirt_hv=1 */
    return true;
}

void com_liamhnam_AMDVirt::stop(IOService *provider)
{
    AMDVIRT_INFO("stopping");
    amdvirt::hv::stop();
    if (fPMNotifier) {
        fPMNotifier->remove();
        fPMNotifier = nullptr;
    }
    amdvirt::svm::disableAll();
    super::stop(provider);
}

IOReturn com_liamhnam_AMDVirt::sleepWakeHandler(void *target, void *, UInt32 messageType,
                                                   IOService *, void *, vm_size_t)
{
    com_liamhnam_AMDVirt *me = static_cast<com_liamhnam_AMDVirt *>(target);

    switch (messageType) {
    case kIOMessageSystemWillSleep:
        AMDVIRT_STAT_INC(sleepEvents);
        amdvirt::svm::suspend();
        break;
    case kIOMessageSystemHasPoweredOn:
        AMDVIRT_STAT_INC(wakeEvents);
        if (amdvirt::svm::resume() && me->fSelfTest) {
            uint32_t ok, total;
            amdvirt::svm::verifyAll(&ok, &total);
        }
        break;
    default:
        break;
    }

    /* Returning success acknowledges the notification. */
    return kIOReturnSuccess;
}

void com_liamhnam_AMDVirt::getInfo(AMDVirtInfo *out)
{
    /* The CPUs are counted at start-up, and on some systems (macOS 26) only one is online then: keep the numbers live. */
    fInfo.nCpus           = amdvirt::svm::cpuCount();
    fInfo.nCpusSvmUsable  = amdvirt::svm::usableCpuCount();
    fInfo.nCpusSvmActive  = amdvirt::svm::activeCpuCount();
    *out = fInfo;
}

void com_liamhnam_AMDVirt::getStats(AMDVirtStats *out)
{
    amdvirt::gStats.logLevel = amdvirt::log::level();
    *out = amdvirt::gStats;
}

bool com_liamhnam_AMDVirt::runSelfTest(uint32_t *ok, uint32_t *total)
{
    return amdvirt::svm::verifyAll(ok, total);
}
