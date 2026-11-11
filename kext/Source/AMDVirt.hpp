#ifndef AMDVIRT_HPP
#define AMDVIRT_HPP

#include <IOKit/IOService.h>

#include "../../include/AMDVirtShared.h"

/*
 * Root service of the kext. Matches on IOResources (see Info.plist), probes
 * SVM on start() and hands out user clients.
 *
 * Boot-args:
 *   -amdvirtoff          kill switch: the kext loads but does nothing
 *   amdvirt_enable_svm=1  turn SVM on (default is probe only)
 *   amdvirt_selftest=1    verify EFER/VM_HSAVE_PA on every CPU after enabling
 *   amdvirt_loglevel=N    see Log.hpp
 *   amdvirt_iolog=N       see Log.hpp
 */
class com_liamhnam_AMDVirt : public IOService {
    OSDeclareDefaultStructors(com_liamhnam_AMDVirt)

public:
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;

    /* Refreshes the dynamic fields and copies the current state out. */
    void getInfo(AMDVirtInfo *out);
    void getStats(AMDVirtStats *out);

    /* Returns false when SVM is not active. */
    bool runSelfTest(uint32_t *ok, uint32_t *total);

private:
    static IOReturn sleepWakeHandler(void *target, void *refCon, UInt32 messageType,
                                     IOService *provider, void *messageArgument,
                                     vm_size_t argSize);

    AMDVirtInfo     fInfo;
    IONotifier *fPMNotifier;
    bool        fSelfTest;
};

#endif /* AMDVIRT_HPP */
