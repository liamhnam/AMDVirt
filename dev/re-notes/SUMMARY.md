# RyzenHV — ghi chú phân tích Virtualization.framework (2026-11-10)

Máy: Ryzen 7 4800HS (Zen 2, family 0x17 model 0x60), macOS 15.1 (24B83).
Nguồn: `com.apple.Virtualization.VirtualMachine` (lát x86_64), phân tích tĩnh bằng `dyld_info`/`otool`.
Chỉ ghi lại giao diện và hành vi, không chép code của Apple.

## CPU (CPUID)
- Có SVM rev 1, 32768 ASID: NP, NRIPS, VmcbClean, FlushByAsid, DecodeAssists, PauseFilter, AVIC, VMSAVEvirt, vGIF, GMET.
- **Không có leaf 0x8000_0021, nên không có `CpuidUserDis`.** Máy này không giả được CPUID vendor cho
  Parallels/VMware mà không vá app.

## Virtualization.framework dùng gì của Hypervisor.framework
- Chỉ **20 hàm công khai** (`vz-hv-imports.txt`). **Không** dùng `hv_vm_space_*`, không dùng trap private.
- `hv_vm_create(0)`: cấu hình mặc định.
- `hv_vcpu_run_until(vcpu, -1)`: deadline = FOREVER, nên việc RyzenHV bỏ qua deadline **không ảnh hưởng**.
- **APIC, IOAPIC và IPI được giả lập trong userland** (`VzCore::Hypervisor::Apic`, `IoApic`, `ApicBus`),
  nên **không cần APIC trong kernel**.
- Có `X86::VmxChecker`: VZ tự kiểm tra trạng thái VMX, nên capability và giá trị field VMCS phải nhất quán như trên VMX thật.
- Đọc `hv_vmx_read_capability` 34 lần, với 12 loại ID: 0x2, 0x5–0xe, 0x10
  (PROCBASED2, BASIC, TRUE_PIN/PROC/ENTRY/EXIT, MISC, CR0/CR4 FIXED0/1, EPT_VPID_CAP).
- MSR native (`hv_vcpu_enable_native_msr`): 0x10 TSC, 0x174–0x176 SYSENTER, 0xC0000081–84 STAR/LSTAR/CSTAR/SFMASK,
  0xC0000100–102 FS/GS/KERNEL_GS.
- Field VMCS: 42 field (`vz-vmcs-fields.txt`). RyzenHV có chỗ lưu cho tất cả. Các field cần **làm đúng ngữ nghĩa**:
  - Mask/shadow của CR0/CR4 (0x6000–0x6006): hiện chỉ lưu
  - TPR threshold (0x401C)
  - Entry IRQ info và error code (0x4016/0x4018)
  - Interruptibility (0x4824)
  - IDT-vectoring info (0x4408)
  - VMX instruction info (0x440E): hiện chỉ lưu

## Hệ quả cho plan
Virtualization.framework **nhỏ hơn nhiều so với ước tính trước** (không cần APIC trong kernel, `vm_space` hay trap private).
Việc còn lại tập trung vào độ chính xác của lớp giả lập VMX cho đúng 20 API trên, và phần capability.

## Reset khi guest reboot (2026-11-11, đọc disassembly `re-work/vmxpc.s`, chỉ ghi hành vi)
- Ghi cổng 0xCF9 (handler 0x100058dc0): nếu bit 2 bật → gọi delegate->vtable[+0x128](hard = !(val & 2)).
- `VzCore::VirtualMachine::perform_reset` (0x10011cee0): dừng vCPU, `hv_tsc_clock`, gửi lambda tới mọi vCPU thread, rồi reset thiết bị
  qua dispatch_group (gọi `*0x38` từng thiết bị) và chờ.
- Lambda mỗi vCPU (vtable 0x100290448, operator() 0x10011e7f0): barrier, rồi 0x10003af40 nạp lại trạng thái CPU từ bản lưu
  (VMCS 0x6820/0x681e/CR0/CR4/segment, write_register, write_msr), rồi `hv_vcpu_set_tsc_relative`. **Không chạm APIC.**
- Vòng vCPU (0x100157060…): đối tượng APIC ở vcpu+0xb8, khoá ở +0x44, **IRR 8×32 bit ở +0x48..+0x64**, cờ +0x68/+0x69.
  Có IRR ≠ 0 → nếu 0x100042b00 cho phép thì tiêm (ghi 0x4016), không thì bật interrupt-window (0x4002 |= 4).
- VZ **không** chọn đường APIC theo capability: các lần đọc PROCBASED2 (0x1000f6f00) chỉ dùng cho VMX lồng nhau (0x10015adb0).
  Giả thuyết "VZ chạy nhánh dự phòng vì kext không cho APIC virtualization" **bị loại**.
- Chưa tìm được chỗ VZ xoá IRR/LVT của APIC khi reset (có thể nằm trong reset thiết bị chạy bất đồng bộ qua dispatch_group).

## AppleHV (Intel) quản lý RAM guest thế nào (2026-11-12, chỉ ghi giao diện/hành vi)
- Kernel export `hv_ept_pmap_create(void **pmap, void **eptp)`: gọi `pmap_create_options(ledger của task, 0, 3)` (64-bit | EPT) và trả về pmap + EPTP (pmap+0x60).
  Tức bảng trang EPT của guest là một **pmap của hệ thống bộ nhớ ảo xnu**.
- AppleHV (trong SystemKernelExtensions.kc) import: `hv_ept_pmap_create`, `vm_map_create`, `vm_map_disable_hole_optimization`,
  `mach_vm_remap`, `vm_fault`, `vm_map_deallocate`, `get_task_map`, `vm_protect`; có hàm `hv_vmx_space_t::ept_fault`,
  `hv_vmx_vcpu_t::handle_ept_violation`, `TASK_TRAP_vm_map/unmap/protect`. Không có import nào kiểu wire/prepare cho RAM guest
  (chỉ `wired_alloc_mdsec` cho cấu trúc vCPU).
- Suy ra cơ chế: không gian guest = một vm_map dùng pmap EPT; `hv_vm_map` = `mach_vm_remap` vùng nhớ của tiến trình vào vm_map đó;
  EPT violation → `vm_fault` trên vm_map guest. RAM guest là trang thường: **có thể nén, swap, thu hồi**; khi xnu đẩy trang ra,
  pmap (EPT) tự gỡ mục tương ứng. Callback `hv_callbacks.memory_pressure` có trong `hv_support_kext.h`.
- Với kext bên thứ ba trên máy này: `vm_map_create`, `mach_vm_remap`, `vm_fault`, `vm_map_deallocate`, `hv_ept_pmap_create`,
  `get_task_map`, `vm_map_disable_hole_optimization` link được; `pmap_create_options`, `pmap_destroy`, `vm_map_remove` **không** export.
- AMD NPT dùng định dạng bảng trang x86 thường, khác EPT: mục EPT có kiểu bộ nhớ WB (6<<3 = 0x30) sẽ thành PCD|A trong NPT → RAM guest
  bị uncached. Nên **không dùng thẳng pmap EPT làm nCR3** được.

## Có làm RAM guest "giống Apple" trên AMD được không (2026-11-12)
Kext bên thứ ba link được (đã thử bằng kext dò): `hv_ept_pmap_create`, `vm_map_create`, `mach_vm_remap`, `vm_fault`, `vm_map_deallocate`,
`get_task_map`, `get_task_pmap`, `pmap_find_phys`, `vm_map_wire`, `vm_map_unwire`, `vm_map_copyin`, `mach_vm_region`.
KHÔNG link được: `pmap_create(_options)`, `pmap_destroy`, `pmap_enter(_options)`, `pmap_remove`, `pmap_disconnect`, `pmap_page_protect`,
`pmap_is_modified/referenced`, `pmap_clear_refmod`, `vm_map_create_options`, `vm_map_remove`, `vm_map_wire_kernel`, `vm_map_page_info`, `get_map_pmap`.
1. **Dùng pmap EPT của xnu làm NPT** (`hv_ept_pmap_create` + `vm_map_create` + `mach_vm_remap` + `vm_fault`, đúng cách AppleHV):
   - xnu `osfmk/x86_64/pmap.c`, `pmap_flush_tlbs`: với pmap EPT gọi `mp_cpus_call(CPUMASK_ALL, ASYNC, invept, pm_eptp)`.
     `INVEPT` là lệnh VMX, trên AMD là #UD → **panic** ngay lần đầu xnu sửa/gỡ một trang guest (pageout, nén, COW).
   - `pmap_init`: `pmap_ept_support_ad = vmx_hv_support() && …` → trên AMD = FALSE.
   - Định dạng: `INTEL_EPT_WB = 0x30` (bit 4,5) → trong NPT là PCD|A → RAM guest uncached. Bit X (bit 2) thành US, không có NX.
   → Loại, trừ khi vá kernel lúc chạy (thay `invept`) — trái nguyên tắc "không can thiệp OS".
2. **Dùng chung bảng trang của app** (cho NPT trỏ vào page table của tiến trình VZ/QEMU):
   - VZ cấp RAM guest là vùng `rw-/rwx` (đo trên Docker: Memory Tag 240, 8 GB, base 0x118801000) → PTE có **NX**, guest không thực thi được.
   - Base 0x…801000 không cùng căn 2 MiB với GPA 0 → không chia sẻ được trang PT.
   → Loại.
3. **Ghim lười + gỡ ghim theo áp lực bộ nhớ** (đang có, mở rộng): trang chỉ bị ghim khi đang nằm trong NPT; gỡ ghim → trang lại là
   trang thường của app, xnu nén/swap được. Thiếu: phản ứng theo áp lực bộ nhớ của hệ thống (hiện chỉ theo giới hạn cố định).
4. **Dùng chung bảng trang của app qua một vùng alias thực thi được (KHẢ THI, chưa làm)** — 2026-11-12:
   - Ý tưởng: kext đặt trong tiến trình client một cửa sổ căn 512 GiB `W`; `hv_vm_map(uva, gpa)` = `mach_vm_remap` (không copy) vùng uva
     tới `W + gpa` rồi `mach_vm_protect` thêm quyền thực thi theo hv perm. NPT gốc là trang riêng của kext, chỉ một mục PML4 = mục PML4 của
     client cho cửa sổ W (đọc từ CR3 của luồng vCPU + `ml_phys_read_double_64`). Lá NPT chính là PTE của xnu (định dạng x86 thường).
   - Trang chưa có → NPF → `vm_fault(get_task_map(task), W+gpa, prot, …)` rồi chạy tiếp. Không ghim gì; xnu tự nén/swap; CPU tự đặt bit A/D
     trong chính PTE của xnu nên xnu biết trang nào đang dùng/đã sửa.
   - xnu `pmap_flush_tlbs` (pmap thường): gửi IPI tới CPU có `pm_cr3` khớp active CR3 **hoặc task CR3**, bỏ qua CPU idle; chờ ACK trước khi
     giải phóng trang. Luồng vCPU thuộc tiến trình client → CPU đang chạy guest nhận IPI → #VMEXIT → kext flush ASID mỗi VMRUN.
   - xnu chỉ giải phóng trang bảng trang khi thay PT bằng superpage 2 MiB (`pmap_enter_options`); PDPT sống tới `pmap_destroy`.
   - W+X: vm_map chặn W+X chỉ khi `cs_process_global_enforcement()` hoặc `vm_map_cs_enforcement(map)`. Máy này: `vm.cs_process_enforcement=0`,
     tiến trình VZ có cờ VALID|KILL|RUNTIME|PLATFORM_BINARY, **không** ENFORCEMENT (máy đang tắt AMFI — trên macOS gốc chưa kiểm).
   - Export dùng được: `mach_vm_allocate/deallocate/protect/remap/map`, `vm_protect`, `vm_fault`, `get_task_map`, `ml_phys_read_double_64`.
   - Rủi ro: máy có AMFI bật có thể từ chối W+X cho VZ; phụ thuộc bố cục bảng trang/CR3 của xnu (PCID bits); cần thử kỹ.
