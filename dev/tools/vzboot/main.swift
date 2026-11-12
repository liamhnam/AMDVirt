// vzboot: boots a Linux kernel + initramfs with Apple's Virtualization.framework and prints the
// guest's serial console to stdout. Used to test RyzenHV's native Hypervisor.framework front end.
//
// Usage: vzboot <kernel> <initramfs> [seconds=90] [cmdline] [disk-image]
import Foundation
import Virtualization

setvbuf(stdout, nil, _IONBF, 0)
let args = CommandLine.arguments
guard args.count >= 3 else {
    FileHandle.standardError.write("usage: vzboot <kernel> <initramfs> [seconds] [cmdline] [disk]\n".data(using: .utf8)!)
    exit(2)
}
let seconds = args.count > 3 ? (Double(args[3]) ?? 90) : 90
let cmdline = args.count > 4 ? args[4] : "console=hvc0 earlyprintk=hvc0 loglevel=7"

func log(_ s: String) { FileHandle.standardError.write("[vzboot] \(s)\n".data(using: .utf8)!) }

log("VZVirtualMachine.isSupported = \(VZVirtualMachine.isSupported)")

let boot = VZLinuxBootLoader(kernelURL: URL(fileURLWithPath: args[1]))
boot.initialRamdiskURL = URL(fileURLWithPath: args[2])
boot.commandLine = cmdline

let cfg = VZVirtualMachineConfiguration()
cfg.bootLoader = boot
cfg.cpuCount = 2
cfg.memorySize = 1024 * 1024 * 1024

let console = VZVirtioConsoleDeviceSerialPortConfiguration()
console.attachment = VZFileHandleSerialPortAttachment(fileHandleForReading: FileHandle.standardInput,
                                                       fileHandleForWriting: FileHandle.standardOutput)
cfg.serialPorts = [console]
cfg.entropyDevices = [VZVirtioEntropyDeviceConfiguration()]

if args.count > 5 {
    do {
        let att = try VZDiskImageStorageDeviceAttachment(url: URL(fileURLWithPath: args[5]), readOnly: true)
        cfg.storageDevices = [VZVirtioBlockDeviceConfiguration(attachment: att)]
    } catch {
        log("disk attach failed: \(error)")
    }
}

do {
    try cfg.validate()
    log("configuration valid")
} catch {
    log("validate failed: \(error)")
    exit(3)
}

let vm = VZVirtualMachine(configuration: cfg)

final class Delegate: NSObject, VZVirtualMachineDelegate {
    func guestDidStop(_ virtualMachine: VZVirtualMachine) { log("guest stopped"); exit(0) }
    func virtualMachine(_ virtualMachine: VZVirtualMachine, didStopWithError error: Error) {
        log("VM stopped with error: \(error)"); exit(4)
    }
}
let delegate = Delegate()
vm.delegate = delegate

vm.start { result in
    switch result {
    case .success: log("VM started")
    case .failure(let e): log("start failed: \(e)"); exit(5)
    }
}

DispatchQueue.main.asyncAfter(deadline: .now() + seconds) {
    log("time limit reached (\(Int(seconds)) s), state = \(vm.state.rawValue); stopping")
    vm.stop { err in
        if let err = err { log("stop error: \(err)") }
        exit(0)
    }
    DispatchQueue.main.asyncAfter(deadline: .now() + 5) { exit(0) }
}

RunLoop.main.run()
