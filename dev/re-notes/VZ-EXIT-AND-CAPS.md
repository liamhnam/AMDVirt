# Virtualization.framework: exit handler và capability (phân tích tĩnh, 2026-11-10)

Nguồn: lát x86_64 của `com.apple.Virtualization.VirtualMachine` (sha256 3ea421b4…), Ghidra 12.1.4 qua REA.
Symbol nội bộ đã bị strip, nên địa chỉ bên dưới là địa chỉ trong lát x86_64 trên macOS 15.1 (24B83).
Chỉ ghi lại hành vi, không chép code.

## Quy tắc chung: mọi lỗi từ `hv_*` đều làm tiến trình dừng hẳn
Sau gần như mọi lời gọi `hv_*` đều có kiểm tra "khác 0 thì gọi hàm không quay về" (0x10023bd16).
Tức là **RyzenHV không được trả lỗi** cho bất kỳ lời gọi hợp lệ nào mà Virtualization.framework thực hiện.
Một trap chưa cài đặt mà trả `HV_UNSUPPORTED` sẽ làm VM chết ngay.

## Bộ xử lý VM exit (0x10003b8c0)
- Switch theo exit reason (16 bit thấp). Các reason được xử lý:
  0, 1, 2, 7, 8, 10, 12, 18–27 (lệnh VMX, cho VMX lồng nhau), 28 (CR), 29 (DR), 30 (I/O), 31/32 (MSR),
  37 (MTF), 43 (TPR below threshold), 48 (EPT violation), 55 (XSETBV).
- **`default`: dừng tiến trình với thông báo "Unhandled exit reason %u".**
  Vì vậy RyzenHV không được sinh ra các reason như 3, 9, 13–16, 33, 36, 39, 40, 51, 54,
  trừ khi đúng như Intel (khi client bật control tương ứng).
  - `VmcsState::applyControls` của RyzenHV đã làm đúng việc này: HLT, INVLPG, RDPMC, RDTSC (kèm RDTSCP), PAUSE, MWAIT,
    MONITOR, WBINVD chỉ bị chặn khi control VMX tương ứng được bật.
  - INVD và INIT luôn bị chặn, **giống Intel** (VMX luôn exit với hai sự kiện này). Linux guest bình thường không gặp.
- Exit 7 (interrupt window): xóa bit 2 của CPU_BASED. Exit 8 (NMI window): xóa bit 22 của CPU_BASED.
- Nó đọc: exit qualification 0x6400, độ dài lệnh 0x440C, VMX instruction info 0x440E, RIP, RFLAGS,
  CR0 0x6800 cùng **CR0 shadow 0x6004**, EFER 0x2806, AR của CS/SS (0x4816/0x4818). Các giá trị này dùng để biết guest
  đang ở chế độ CPU nào khi giả lập lệnh (MMIO, I/O chuỗi).

## Capability
- Hàm đọc đơn giản (không có `hv_vmx_read_capability` thì dừng tiến trình): ID 2 (PROCBASED2), 6, 7, 8, 9 (TRUE_PIN/PROC/ENTRY/EXIT).
  Nơi dùng chính là 0x10015adb0: **kiểm tra một field VMCS có tồn tại không**, theo Intel SDM, cho
  VMREAD/VMWRITE của guest (VMX lồng nhau).
- 16 hàm 0x10015b4e0–0x10015bbb0: giả lập RDMSR IA32_VMX_* (0x480–0x491) **cho guest**, chỉ khi guest có VMX (cờ 0x20).
  BASIC được ghép: `cap & 0x7c000000000000 | 0x80068800000001`.
- Exit handler đọc ID 10 (MISC) khi giả lập VMREAD/VMWRITE, và ID 0xB–0xE (CR0/CR4 FIXED) khi giả lập VMXON.
- **Kết luận:** với guest Linux bình thường (không bật VMX lồng nhau), giá trị capability gần như không ảnh hưởng.
  Chỉ cần mọi ID (2, 5–0xE, 0x10) **trả thành công**. RyzenHV đã có `kCaps[17]` cho ID 0–16.

## Rủi ro đã thấy trong RyzenHV
- NMI window (CPU_BASED bit 22): `kCaps[1]` cho phép bật (allowed-1), nhưng chú thích nói đường native **chưa cài đặt NMI window**.
  Nếu Virtualization.framework bật bit này và chờ exit 8 thì NMI có thể không bao giờ được tiêm. Cần thử thật.
- Đường native trả `HV_UNSUPPORTED` cho trap chưa có: task trap 2, 3, 8–11, 14, 15; thread trap 6, 8, 13–15.
  Cần biết `hv_vcpu_set_tsc_relative`, `hv_tsc_clock`, `hv_vcpu_enable_native_msr` đi qua trap nào.
  **Cách nhanh nhất là chạy một VM thật rồi đọc `rhv-info log`.**
