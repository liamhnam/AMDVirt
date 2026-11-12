# Dựng lại thư mục làm việc từ nhánh `development`

Thư mục làm việc gốc trên máy là `~/Downloads/dev/ryzenhv-native/`. Nếu nó bị xoá, nhánh này chứa đủ để dựng lại; những thứ không được đưa
lên (vì là file của Apple, có thể tải lại, hoặc là bí mật) được ghi cách lấy lại ở dưới.

## Bố cục và nơi mỗi thứ nằm trong nhánh

| Thư mục trên máy | Trong nhánh `development` | Ghi chú |
|---|---|---|
| `upstream/` | chính repo này | `main` = code kext; `development` = `main` + `dev/` + `CLAUDE.md` |
| `CLAUDE.md` (nhật ký) | `dev/JOURNAL.md` | |
| `HANDOFF.md` | `dev/HANDOFF.md` | **đọc đầu tiên** |
| `re-notes/` | `dev/re-notes/` | ghi chú hành vi Apple (clean-room) |
| `tools/` | `dev/tools/` | script thử, mã nguồn `vzboot`, `tsc-test` (binary build lại, xem dưới) |
| `test-images/` | `dev/workspace/test-images/` | log mọi lần thử; thiếu ISO/kernel (xem dưới) |
| `re-work/` | `dev/workspace/re-work/` | các bản kext cũ (`kext-*`), config RHVTEST cũ (**SMBIOS đã che**), config UTM/Docker cũ; thiếu file Apple (xem dưới) |
| `release/ryzenhv-0.7.9/` | không có | tải lại (xem dưới) |

## Lệnh dựng lại

```bash
mkdir -p ~/Downloads/dev/ryzenhv-native && cd ~/Downloads/dev/ryzenhv-native
git clone -b development https://github.com/liamhnam/AMDVirt.git upstream
cd upstream && git fetch origin main:main && git remote add overlorddev https://github.com/OverlordDev/ryzen-hypervisor.framework.git
git config user.name "Ho Hoang Nam"; git config user.email "29671651+liamhnam@users.noreply.github.com"
cd ..
cp upstream/dev/HANDOFF.md HANDOFF.md; cp upstream/dev/JOURNAL.md CLAUDE.md
cp -R upstream/dev/re-notes re-notes; cp -R upstream/dev/tools tools
cp -R upstream/dev/workspace/test-images test-images; cp -R upstream/dev/workspace/re-work re-work
```

Làm code trên `main` (nhánh `development` chỉ thêm tài liệu); khi xong một việc, cập nhật `dev/HANDOFF.md`/`dev/JOURNAL.md` trên `development`
và rebase/merge `main` vào `development` để hai nhánh không lệch.

## File không có trong nhánh và cách lấy lại

| File | Vì sao không đưa lên | Cách lấy lại | SHA-256 |
|---|---|---|---|
| `test-images/alpine-virt-3.24.4-x86_64.iso` (67 MB) | tải lại được | `curl -LO https://dl-cdn.alpinelinux.org/alpine/v3.24/releases/x86_64/alpine-virt-3.24.4-x86_64.iso` (hỏi chủ dự án trước khi tải) | `157d353f8e54252cad2d9812a0d56bb7fa2231fc154a110e1d180d7480ef8b4f` |
| `test-images/vmlinuz-virt`, `initramfs-virt` | lấy từ ISO | `cd test-images && tar -xf alpine-virt-3.24.4-x86_64.iso boot/vmlinuz-virt boot/initramfs-virt && mv boot/* . && rmdir boot` | `1e8ac7ae…b44ebe9d`, `255d0282…7540abea` |
| `re-work/vmxpc` | binary của Apple, không được phát tán | `lipo -thin x86_64 /System/Library/Frameworks/Virtualization.framework/Versions/A/XPCServices/com.apple.Virtualization.VirtualMachine.xpc/Contents/MacOS/com.apple.Virtualization.VirtualMachine -output re-work/vmxpc` (macOS 15.1 24B83) | `3ea421b443c39a3ef779eac617ec74f2647dc2e78b04957e797307a332c042cd` |
| `re-work/vmxpc.s` (22 MB) | disassembly của code Apple | `otool -tV re-work/vmxpc > re-work/vmxpc.s` | — |
| `re-work/vmxpc.ghidra-snapshot`, `re-work/decomp/*.c` | phân tích/decompile code Apple | mở lại `vmxpc` bằng REA/Ghidra (Ghidra mất >330 s; dùng `vmxpc.s` nhanh hơn) | — |
| `release/ryzenhv-0.7.9/` | bản release của dự án gốc, tải lại được | GitHub OverlordDev/ryzen-hypervisor.framework, Releases, `ryzenhv-0.7.9-macos-x86_64.tar.gz` (có QEMU 11.1.2 HVF) | kext gốc `165ac7d4…81de865b9` |
| SMBIOS thật trong `config-*.plist` | bí mật của máy (repo công khai) | lấy từ EFI đang chạy (`/Volumes/RHVTEST/EFI/OC/config.plist`) | — |
| Binary `tools/vzboot/vzboot`, `tools/tsc-test/tsc-test` | build lại được | xem dưới | — |

Build lại công cụ:

```bash
cd ~/Downloads/dev/ryzenhv-native/tools
# vzboot (Swift, cần wrapper swiftc nếu CLT có modulemap thừa: ~/.local/opt/swiftc-fix/swiftc)
swiftc -O vzboot/main.swift -o vzboot/vzboot && codesign -s - --force --entitlements vzboot/vzboot.entitlements vzboot/vzboot
# tsc-test
cc -arch x86_64 -O2 tsc-test/main.c -framework Hypervisor -o tsc-test/tsc-test && codesign -s - --force --entitlements tsc-test/hypervisor.entitlements tsc-test/tsc-test
```

Các script `dev/tools/*.sh` dùng đường dẫn `$P/upstream/build/amdvirt-info` với `P` = thư mục cha của `tools/`, nên chạy từ bố cục ở trên.

## Công cụ ngoài repo (cài lại nếu máy mới)

Node 24 (`~/.local/opt/node`), Ghidra 12.1.4 (`~/.local/opt/ghidra_12.1.4_PUBLIC`, cần build decompiler native), REA 6.8.0 (MCP),
`gh` 2.103.3 (`~/.local/bin/gh`, `gh auth login --web`), Lima 2.3.0 (`~/.local/opt/lima`), Docker Desktop 4.99.0 (Intel), UTM 5.0.8.
Mọi lần tải phải hỏi chủ dự án trước (tên, nguồn, dung lượng).
