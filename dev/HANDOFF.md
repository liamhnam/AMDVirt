# AMDVirt — bàn giao cho phiên làm việc mới

Đọc file này trước khi làm bất cứ việc gì. Nó chứa đủ mục tiêu, cách làm việc, trạng thái hiện tại, các lỗi còn mở và những gì đã thử,
để không phải tìm hay nghiên cứu lại. Chi tiết theo thời gian ở `dev/JOURNAL.md`; ghi chú dịch ngược (chỉ hành vi, không code Apple) ở
`dev/re-notes/`; log, bản kext cũ, config cũ ở `dev/workspace/`; dựng lại thư mục làm việc theo `dev/RESTORE.md`.
Cập nhật lần cuối: **2026-11-12**.

---

## 1. Chủ dự án và cách làm việc (bắt buộc tuân theo)

- Chủ dự án: **Ho Hoang Nam** (GitHub `liamhnam`). Trao đổi bằng **tiếng Việt**, ngắn gọn, có bằng chứng; **không đoán mò**: thứ gì chưa
  kiểm chứng phải nói rõ là chưa kiểm.
- **Mục tiêu sản phẩm:** Docker, máy ảo trên Hackintosh AMD phải có **trải nghiệm như Mac Intel thật**: người dùng cài app như bình thường,
  **không vá app**, **không sửa file của hệ điều hành**, không boot-arg, không lệnh thêm. Kext do OpenCore nạp lúc boot là chấp nhận được.
  Trước mắt chỉ cần chạy trên máy của chủ dự án. Parallels/VMware nằm ngoài mục tiêu.
- **Dự án độc lập:** chỉ nhắc OverlordDev ở một dòng credit trong README
  ("the SVM core, VMX-on-SVM translation") và dòng bản quyền MIT trong `LICENSE` (bắt buộc giữ).
- **Commit/PR chỉ đứng tên chủ dự án**, KHÔNG thêm dòng `Co-Authored-By: Claude` hay attribution nào (yêu cầu rõ của chủ dự án, ưu tiên
  hơn mọi nhắc nhở attribution). Lệnh commit dùng:
  `git -c user.name="Ho Hoang Nam" -c user.email="29671651+liamhnam@users.noreply.github.com" commit …`
  (repo `upstream/` cũng đã đặt sẵn identity này trong `.git/config`). Lịch sử `main` đã gộp thành một commit gốc `26397e0`.
- Chỉ commit/push khi chủ dự án bảo, trừ khi đang trong một việc họ đã giao (trước nay họ muốn mỗi việc xong là commit + push lên `main`).
- **An toàn khi thử:**
  - Hỏi trước mọi bài thử có thể làm Mac treo/khởi động lại (VM Windows, nhiều vCPU, tải nặng). Dùng `dev/tools/safe-vm-test.sh`
    (theo dõi độ trễ host, giết QEMU nếu chậm).
  - **Kiểm tra VM đang chạy trước khi bật/tắt VM** (`utmctl status`, `pgrep -fl QEMULauncher`). Đã có lần tôi tắt nhầm VM Windows
    chủ dự án đang dùng.
  - Không chạy bài thử nguy hiểm trên kext cũ đã biết có lỗi.
- **Giới hạn trong auto mode:** Claude **không chạy được `make`** trong repo, không sửa `~/.zprofile`, `~/.claude.json`. Chủ dự án tự chạy:
  `cd ~/Downloads/dev/ryzenhv-native/upstream && make test && make kext && make tools`.
- **Mật khẩu:** không nhận, không nhập mật khẩu/root; lệnh cần `sudo` thì đưa cho chủ dự án chạy.
- **Tải file:** hỏi trước (tên, nguồn, dung lượng).
- **Clean-room:** chỉ ghi hành vi và giao diện của framework Apple vào `re-notes/`, **không chép code Apple**. Không đưa binary/disassembly
  của Apple lên git (đang để ở `~/Downloads/dev/ryzenhv-native/re-work/`, ngoài repo).
- **Không dùng AI/REA phân tích NootedRed hay ForgedInvariant** (của ChefKiss, tác giả cấm).

---

## 2. Máy và môi trường

- ASUS ROG Zephyrus G14 GA401, **Ryzen 7 4800HS** (Zen 2, 16 luồng), 16 GB RAM, macOS **Sequoia 15.1 (24B83)**, OpenCore 1.0.4,
  SMBIOS MacBookPro16,3. iGPU Renoir dùng NootedRed (fork steve12311 0.9.0), GTX 1650 dùng NullMoth driver 2.9 (nên **AMFI đang tắt**:
  `amfi_get_out_of_my_way=0x1 amfi=0x80`). ForgedInvariant 1.0.1 đồng bộ TSC.
- SVM rev 1, 32768 ASID; có NP, NRIPS, VmcbClean, FlushByAsid, DecodeAssists, PauseFilter, AVIC, vVMSAVE, vGIF, GMET.
  **Không có `CpuidUserDis`** (leaf 0x8000_0021) → không giả hãng CPU được → Parallels/VMware không chạy nếu không vá app.
- **EFI:**
  | Phân vùng | Vai trò |
  |---|---|
  | `RHVTEST` (thường `disk0s4`, số có thể đổi) | **Firmware tự boot vào đây.** Có `AMDVirt.kext` (mục `Kernel → Add` số 29). boot-args hiện: `… rhvtest=1 debug=0x100` (`debug=0x100` còn sót từ lúc điều tra panic, nên gỡ) |
  | `disk0s2` (mount ở `/Volumes/EFI 1`) | **Dự phòng sạch**, không có kext; dấu `rhvfallback=1`; chọn bằng **Esc** lúc logo ASUS |
  | `disk0s1` | Không dùng (Bootx64.efi 9 KB từ 2022) |
  - Mount RHVTEST: `diskutil list disk0` rồi `diskutil mount diskXsY` (không cần sudo).
  - Sao lưu EFI chính: `~/Downloads/EFI-backup-20261110-081003.zip`, `~/Downloads/EFI-backup-20261110-060020/`.
  - Cứu máy: Esc → chọn EFI dự phòng; hoặc boot-arg `-amdvirtoff`; hoặc OpenShell đổi tên `EFI\OC\Kexts\AMDVirt.kext`.
- **Thư mục trên máy** `~/Downloads/dev/ryzenhv-native/`:
  | Đường dẫn | Nội dung |
  |---|---|
  | `upstream/` | repo git AMDVirt (remote `origin` = github.com/liamhnam/AMDVirt; `overlorddev` = repo gốc RyzenHV 0.7.9). Nhánh local: `main`, `development`, `backup/full-history` (lịch sử cũ trước khi gộp, KHÔNG push), `overlorddev-main` |
  | `CLAUDE.md` | bản gốc của `dev/JOURNAL.md` (nhật ký + checklist) |
  | `re-notes/` | bản gốc của `dev/re-notes/` |
  | `re-work/` | **chỉ ở máy**: các bản kext cũ (`kext-*`), config cũ, `vmxpc` (lát x86_64 của VZ XPC), `vmxpc.s` (otool disassembly 22 MB), Ghidra snapshot |
  | `tools/` | bản gốc của `dev/tools/` (script thử, `vzboot`, `tsc-test`) |
  | `test-images/` | Alpine ISO, kernel/initramfs cho vzboot, log các lần thử (`capture-*.log`, `safe-*.log`, `repro-hlt-*`, `tsc-*.txt`) |
  | `release/ryzenhv-0.7.9/` | bản release RyzenHV gốc (QEMU 11.1.2 có HVF ở `qemu/bin/`) |
- **Công cụ:** Node 24 `~/.local/opt/node/bin` (không trong PATH); Ghidra 12.1.4 `~/.local/opt/ghidra_12.1.4_PUBLIC` (nạp lại `vmxpc`
  vượt 330 s → đọc thẳng `re-work/vmxpc.s` + script Python giải chained fixups thay vì Ghidra); REA 6.8.0 (MCP); `gh` ở `~/.local/bin/gh`
  (đã đăng nhập `liamhnam`, scope repo/workflow); Lima 2.3.0 `~/.local/opt/lima/bin` (instance `rhv-alpine` 2 CPU/2 GB, `rhv-docker`);
  Docker Desktop 4.99.0 (CLI `~/.docker/bin`, đang để 8 CPU; RAM mặc định 8 GB); UTM 5.0.8 (`/Applications/UTM.app/Contents/MacOS/utmctl`,
  VM: `Windows` (QEMU q35 UEFI TPM 4 GB, **4 vCPU**, virtio-vga-gl), `rhv-alpine-qemu`, `rhv-alpine-apple`).

---

## 3. Quy trình thử một kext mới

1. Sửa code; Claude tự kiểm cú pháp bằng `xcrun clang++ … -fsyntax-only` (cờ kernel như trong `kext/Makefile`) và chạy unit test bằng
   `make -n test BUILD=<scratch>` rồi chạy các lệnh đó (Claude không gọi `make` thật được).
2. Chủ dự án: `make test && make kext && make tools`.
3. Claude: sao `/Volumes/RHVTEST/EFI/OC/Kexts/AMDVirt.kext` hiện tại vào `re-work/kext-<tên>/`, chép `upstream/build/AMDVirt.kext` vào
   RHVTEST, ghi UUID (`dwarfdump --uuid …/Contents/MacOS/AMDVirt`).
4. Chủ dự án khởi động lại.
5. Claude kiểm: `kmutil showloaded | grep AMDVirt` (đúng UUID), `sysctl kern.hv_support` (=1), `build/amdvirt-info`, `amdvirt-info log`,
   `amdvirt-info stats` (`assertFailures`, `vmInvalidExits` phải 0); chạy `build/hvf-smoke`, `build/hvf-ro-map-test`, `build/hvf-race-test`,
   Docker `hello-world` + nginx `-p 8080:80` (tên container ≥ 2 ký tự), Lima `limactl start rhv-alpine` + `limactl shell rhv-alpine uname -sr`.
6. Commit (chỉ tên chủ dự án) + push `main`; CI GitHub Actions (`.github/workflows/build.yml`, runner `macos-15-intel`) phải xanh.

Lệnh hay dùng: `B=~/Downloads/dev/ryzenhv-native/upstream/build; $B/amdvirt-info [log|stats|hv]`; điều khiển (`hv debug`, `hv flightdump`,
`hv sysctl`) cần sudo vì đã bỏ `amdvirt_allow_users`.

---

## 4. Trạng thái code (nhánh `main`, commit `84c6a49`, kext 0.1.0)

Kext đang chạy trên máy: **UUID 31793FB7** (bản trước ở `re-work/kext-racefix2/` = 4569203E).

Đã làm và đã kiểm trên máy:
- Đổi tên toàn bộ RyzenHV → **AMDVirt**: `AMDVirt.kext`, bundle `com.liamhnam.AMDVirt`, class `com_liamhnam_AMDVirt*`, công cụ `amdvirt`,
  `amdvirt-info`, boot-arg `amdvirt_*` / `-amdvirtoff`, macro `AMDVIRT_*`, namespace `amdvirt::`.
- Front end Hypervisor.framework native (`HvFrontend.cpp`, đăng ký `hv_set_traps`), SVM và front end **tự bật** khi nạp, `kern.hv_support`
  tự = 1 (thread_call kiểm lại 5 phút). Không cần boot-arg.
- Kiểm tra entitlement `com.apple.security.hypervisor` / `com.apple.vm.hypervisor` trong `hv_vm_create` (`HV_DENIED` nếu thiếu).
- CPUID: ẩn bit AMD/SPEC_CTRL mà client không giả lập (sửa panic IBPB của Linux guest).
- TSC: `hv_vcpu_write_msr(TSC)` + `hv_vcpu_set_tsc_relative` áp đúng; TSC native chỉ cho **đọc**, `WRMSR TSC` của guest thành TSC offset
  (sửa lỗi Windows ghi TSC thật → đồng hồ macOS nhảy tới năm 2262 → Mac đơ).
- MSR native chỉ cho các MSR trong `MsrMap::info()` (đều được VMCB/VMLOAD/kext hoán đổi); DEBUGCTL/SPEC_CTRL luôn bị chặn.
- Requeue sự kiện tiêm bị cắt ngang; ASID riêng mỗi vCPU + FlushByAsid.
- **Gỡ toàn bộ code vá app** (HvPatch, shim DYLD, script Parallels/Docker, app quản lý, bộ release cũ).
- **Chống race:** registry `gLive`, `enterVm/leaveVm`, `taskGuarded/threadGuarded` (HvFrontend.cpp); `hv_vm_destroy` chờ `busy`=0;
  vCPU sau `exec()` bị từ chối. `tools/hvf-race-test` (mặc định 20 vòng) PASS.
- **Ghim RAM lười + giới hạn** (`Vm.cpp`, `GuestRegions.hpp`, `Npt.hpp::testClearAccessed`): `hv_vm_map` chỉ ghi vùng; NPF lần đầu ghim
  khối 2 MiB (`faultIn`); giới hạn chung ½ RAM (`amdvirt_pin_limit_mb`); vượt thì gỡ khối ít dùng theo bit Accessed (`evictLocked`,
  `reclaim`, xét VM mình trước rồi VM khác); `amdvirt_lazypin=0` = cách cũ. Đo: Docker 8 GB chỉ ghim ~0,95 GB (trước 8 GB);
  Docker + Windows 4 GB/4 vCPU chạy 3 phút, độ trễ host TB 0,056 s. **Trường hợp chạm giới hạn/gỡ ghim chưa từng xảy ra trong thử.**
- Tài liệu: README (Features, Requirements, Compatibility, Installation, Boot arguments, Known issues, Troubleshooting, Credits, License),
  `docs/INSTALL.md`, `docs/TESTING.md`, `docs/DEBUGGING.md`, `docs/ARCHITECTURE.md`, `docs/legacy/` (tài liệu cũ), `CHANGELOG.md`.
  GitHub About: "Hypervisor.framework for AMD Ryzen Hackintoshes" + topics.

App đã chạy: Docker Desktop 4.99.0 (đủ: run/build/compose/port/mount), Lima vz + Docker Engine, UTM Apple mode (Linux), UTM QEMU
(Linux tới login; Windows chạy kernel, chưa xác nhận tới desktop).

---

## 5. Lỗi còn mở (chi tiết để khỏi điều tra lại)

### 5.1 Guest Linux treo ở firmware khi tự reboot
- Tái hiện: `dev/tools/repro-hlt.sh rhv-alpine 12` (Lima reboot lặp) → thường treo ở lần 1–3: vCPU0 `HLT` mãi ở firmware rip `0x7fa5813f`.
- Đã chứng minh (trace `test-images/repro-hlt-041203/ring-merged.txt`): Linux reboot bằng `OUT 0xCF9,6` lúc đang tắt IF và có ngắt timer
  0xEC chờ; ngay khi reset VZ vẫn bật interrupt-window (`0x4002=0x200084`); lần tiêm đầu tiên sau reset là **chính VZ ghi
  `0x4016=0x800000ec`** → firmware không có handler/EOI → ISR kẹt lớp 0xE → timer firmware 0x40 bị chặn. Reboot thành công thì ngắt đầu là 0x40.
- Đã loại: TPR, đường trả sự kiện (requeue), trap thiếu, TSC-deadline, TSC; giả thuyết "VZ chạy nhánh APIC dự phòng vì caps PROCBASED2"
  (VZ luôn dùng APIC userland; caps PROCBASED2 chỉ dùng cho VMX lồng nhau).
- Đã dịch ngược (xem `dev/re-notes/SUMMARY.md` mục Reset): CF9 handler 0x100058dc0 → `VirtualMachine::perform_reset` 0x10011cee0 → lambda mỗi
  vCPU (operator() 0x10011e7f0) chỉ nạp lại thanh ghi (0x10003af40) và `set_tsc_relative`, **không chạm APIC**; APIC ở `vcpu+0xb8`, IRR
  8×32 bit ở `+0x48..+0x64`, vòng tiêm ở 0x100157060. Chưa tìm được chỗ VZ xoá IRR/LVT (có thể trong reset thiết bị chạy qua dispatch_group).
- Cách tạm: tắt hẳn rồi bật VM. Chủ dự án đã cho dừng hướng này.

### 5.2 Linux guest đánh dấu TSC "unstable" (dùng HPET)
- Linux báo warp ~1800–2700 cycles. Đo userland ping-pong 16 luồng: TSC host đi lùi tới ~16 000–23 000 cycles giữa các core **khi đang chạy**,
  trong khi kext đo lúc boot (rendezvous, ping-pong kiểu NTP) chỉ thấy lệch tĩnh ≤ 449 cycles → lệch **động** (nghi ForgedInvariant
  đồng bộ lại khi chạy). Đã thử bù tĩnh theo từng core (kext 6A36C0C1) → **không hiệu quả**, đã gỡ. Ghi là Known issue. Docker không bị
  (nó bật `tsc=reliable`).

### 5.3 RAM guest chưa "giống Apple" — có phương án khả thi, CHƯA làm (việc tiếp theo được đề xuất)
Bằng chứng ở `dev/re-notes/SUMMARY.md` (mục "AppleHV quản lý RAM" và "Có làm RAM guest giống Apple…").
- Apple (Intel): AppleHV import `hv_ept_pmap_create`, `vm_map_create`, `mach_vm_remap`, `vm_fault` → RAM guest là trang thường, nén/swap được.
- Kext bên thứ ba link được: `hv_ept_pmap_create`, `vm_map_create`, `mach_vm_remap`, `vm_fault`, `vm_map_deallocate`, `get_task_map`,
  `get_task_pmap`, `pmap_find_phys`, `vm_map_wire/unwire`, `vm_map_copyin`, `mach_vm_allocate/deallocate/protect/map/region`, `vm_protect`,
  `ml_phys_read_double_64`, `ml_phys_write_double_64`. KHÔNG: `pmap_create(_options)`, `pmap_enter`, `pmap_remove`, `pmap_disconnect`,
  `pmap_page_protect`, `pmap_is_modified/referenced`, `vm_map_remove`, `vm_map_wire_kernel`, `get_map_pmap`, `get_cr3_*`, `ml_phys_read_64`.
  (Dò bằng một kext nhỏ + `scripts/check-kext-symbols.sh`.)
- **Loại:** dùng pmap EPT của xnu làm NPT — xnu `pmap_flush_tlbs` gọi `mp_cpus_call(CPUMASK_ALL, ASYNC, invept, …)`; `INVEPT` trên AMD là #UD
  → panic ngay lần đầu xnu sửa trang guest; thêm nữa `INTEL_EPT_WB=0x30` = PCD|A trong NPT (uncached).
- **Loại:** cho NPT trỏ thẳng vào bảng trang của app — VZ cấp RAM guest `rw-` (NX) và base `0x…801000` lệch căn 2 MiB.
- **KHẢ THI (đề xuất):** dùng chung bảng trang của app qua **alias thực thi được**:
  1. Mỗi VM: đặt trong tiến trình client một cửa sổ căn **512 GiB** `W` (`mach_vm_allocate` FIXED để giữ chỗ).
  2. `hv_vm_map(uva, gpa)`: `mach_vm_remap` (copy=FALSE) vùng uva tới `W+gpa`, rồi `mach_vm_protect` theo hv perm (+X).
  3. NPT gốc = một trang của kext; mục PML4[gpa>>39] = mục PML4 của client cho `W` (đọc CR3 khi đang ở luồng vCPU, bỏ bit PCID, đọc bằng
     `ml_phys_read_double_64`). Lá NPT là PTE của xnu (định dạng x86 thường, US=1, PAT WB).
  4. NPF not-present trong vùng: `vm_fault(get_task_map(task), W+gpa, prot, FALSE, …)` rồi chạy tiếp; NPF ghi vào trang COW cũng vậy.
  5. `hv_vm_unmap` = `mach_vm_deallocate` alias; `hv_vm_protect` = `mach_vm_protect` alias.
  - Vì sao an toàn: xnu `pmap_flush_tlbs` (pmap thường) gửi IPI tới CPU có `pm_cr3` khớp active CR3 **hoặc task CR3**, bỏ qua CPU idle, chờ ACK
    trước khi giải phóng trang → CPU đang chạy guest (luồng vCPU thuộc client) nhận IPI → #VMEXIT → kext flush ASID mỗi VMRUN. xnu chỉ giải
    phóng trang bảng trang khi thay PT bằng superpage 2 MiB; PDPT sống tới `pmap_destroy`.
  - W+X: vm_map chặn W+X chỉ khi `cs_process_global_enforcement()` hoặc `vm_map_cs_enforcement(map)`. Máy này `vm.cs_process_enforcement=0`;
    tiến trình VZ cờ `VALID|KILL|RUNTIME|PLATFORM_BINARY`, **không** `ENFORCEMENT`. **Chưa kiểm trên macOS có AMFI bật.**
  - Rủi ro: AMFI bật có thể chặn W+X cho VZ; phụ thuộc bố cục bảng trang/CR3 của xnu; cần kiểm kỹ (race, superpage, KPTI).
  - Chủ dự án đã nói "tạm ngưng" ngay trước khi bắt đầu cài đặt — **hỏi lại trước khi làm**.

### 5.4 Chưa thử
Sleep/wake khi có VM chạy; chạy liên tục nhiều giờ; Windows 16 vCPU tới desktop (lỗi đơ cũ với 16 vCPU + Docker có thể là hết RAM, đã sửa
bằng ghim lười nhưng chưa thử lại); trường hợp chạm giới hạn ghim và gỡ ghim; hiệu năng so với Mac Intel (chưa đo).

---

## 6. Việc còn lại (theo thứ tự đề xuất)

1. (Hỏi trước) Cài đặt phương án 5.3 "alias + dùng chung bảng trang", sau boot-arg để bật/tắt, thử từng bước.
2. Gỡ `debug=0x100` khỏi boot-args RHVTEST (config trước khi thêm ở `re-work/config-RHVTEST-before-debug.plist`).
3. Thử sleep/wake, chạy nhiều giờ, Windows 16 vCPU.
4. Đưa AMDVirt vào dùng hằng ngày (RHVTEST đã là EFI firmware tự boot; disk0s2 giữ sạch để cứu máy) và ra release v0.1.0 trên GitHub.
5. (Tuỳ) Quay lại lỗi reboot 5.1.

Ước lượng "giống Mac Intel" đã báo chủ dự án: ~74% (bảng chấm theo mảng trong lịch sử trao đổi): API 95, app 80, RAM 60, đồng hồ 60,
ổn định 55, bảo mật 90, trải nghiệm 90; hiệu năng chưa đo.

---

## 7. Bộ nhớ của Claude trên máy

`~/.claude/projects/-Users-liamhnam-Downloads/memory/`: `MEMORY.md` (chỉ mục), `hackintosh-g14-setup.md`, `minecraft-nvidia-nvk.md`,
`commits-author-only.md` (commit chỉ tên chủ dự án), và con trỏ tới file này.
