# AMDVirt (the app)

A small SwiftUI app that does by buttons what `./amdvirt` does in a terminal. It keeps no logic of its own: it downloads
the release bundle from GitHub (kext, shim, scripts, `amdvirt`) into `~/Library/Application Support/AMDVirt` and drives that bundle.

| page | what it does |
|---|---|
| Status | the latest release on GitHub, download / update, `amdvirt doctor` as a checklist |
| Install the kext | finds the OpenCore EFI (mounts EFI partitions, asks for your password), asks for a confirmation that you have a working EFI without the kext, then runs `amdvirt kext <EFI>` as root: kext copied, `config.plist` backed up and edited, boot-args set |
| Parallels Desktop | enable (prepare a private copy, install the wrapper as root), turn 3D off in all stopped VMs, undo |
| Docker Desktop | download 4.97.0, enable, undo |
| Log | everything the commands print |

Things the app cannot do for you: turn SVM on in the BIOS, and restart the Mac after the kext is installed.

```
./build-app.sh [version]     # build/AMDVirt.app and build/AMDVirt-app.zip, needs only the Command Line Tools
AMDVIRT_BUNDLE=/path/to/amdvirt-x.y.z open -n build/AMDVirt.app --env AMDVIRT_BUNDLE=/path/to/amdvirt-x.y.z   # use a local bundle
```

The app is ad-hoc signed. Downloaded from a browser it carries the quarantine flag: right-click → Open, or
`xattr -dr com.apple.quarantine AMDVirt.app`.
