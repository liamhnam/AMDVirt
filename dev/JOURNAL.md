# AMDVirt (trước là RyzenHV native) — Docker/VM trên Hackintosh AMD

File này lưu tiến trình, kế hoạch và những thông tin cần nhớ của dự án. Claude Code tự đọc file này khi làm việc trong thư mục dự án.
**Cập nhật file này sau mỗi phiên làm việc.**

## Mục tiêu (yêu cầu của chủ dự án)
- Trải nghiệm như native: người dùng **cài Docker, VM như bình thường, không cần quan tâm gì**.
- **Không can thiệp file của hệ điều hành.** Kext do OpenCore nạp lúc boot thì chấp nhận được; không ghi vào `/System`.
- **Không vá app** (không vá byte, không shim, không wrapper, không khóa phiên bản app).
- Trước mắt **chỉ cần chạy trên máy của chủ dự án**.
- Đích chính: **Virtualization.framework** → Docker Desktop bản mới, Lima/Colima chế độ vz, UTM (Apple mode), OrbStack, Tart.
- Parallels và VMware **nằm ngoài mục tiêu**: CPU không có `CpuidUserDis`, nên không qua được kiểm tra hãng CPU mà không vá app.

## Máy
- ASUS ROG Zephyrus G14 GA401, Ryzen 7 4800HS (Zen 2, family 0x17 model 0x60), 16 luồng, macOS **15.1 (24B83)**.
- SVM rev 1, 32768 ASID: NP, NRIPS, VmcbClean, FlushByAsid, DecodeAssists, PauseFilter, AVIC, vVMSAVE, vGIF, GMET. **Không có leaf 0x8000_0021, nên không có `CpuidUserDis`.**
- iGPU Renoir dùng NootedRed (fork steve12311), GTX 1650 dùng NullMoth, nên **AMFI đang tắt** (do driver NVIDIA, không phải do RyzenHV).
- Chưa thử SVM khi sleep/wake.

## Bố cục EFI (quan trọng)
| Phân vùng | Vai trò | Ghi chú |
|---|---|---|
| `RHVTEST` (hiện là `disk0s4`, **số có thể đổi sau mỗi lần boot**) | **Firmware tự boot vào đây.** Có RyzenHV | boot-args: `rhv_enable_svm=1 rhv_allow_users=1 rhv_hv=1` |
| EFI chính `disk0s2` (mount tại `/Volumes/EFI 1`) | **Dự phòng**, bản sạch không có RyzenHV | Có dấu `rhvfallback=1`. Chọn bằng **Esc** lúc logo ASUS |

- Sao lưu: `~/Downloads/EFI-backup-20261110-081003.zip` (EFI chính trước khi thêm RyzenHV) và `~/Downloads/EFI-backup-20261110-060020/`.
- **Kiểm tra đang chạy EFI nào:** `nvram boot-args` (có `rhv_enable_svm=1` là RHVTEST, có `rhvfallback=1` là dự phòng).
- Mount: `diskutil mount disk0s2`; RHVTEST thì tìm số phân vùng bằng `diskutil list disk0`.
- Nếu panic: Esc → chọn mục OpenCore còn lại. Hoặc vào OpenShell, đổi tên `EFI\OC\Kexts\RyzenHV.kext`.

## Thư mục dự án
| Đường dẫn | Nội dung |
|---|---|
| `upstream/` | Repo **AMDVirt** (remote `origin` = github.com/liamhnam/AMDVirt, `overlorddev` = repo gốc 0.7.9). Nhánh local `main` → `origin/main`. **Lịch sử đã gộp thành 1 commit (26397e0) chỉ đứng tên chủ dự án; commit không thêm dòng Co-Authored-By Claude.** Lịch sử cũ ở nhánh local `backup/full-history`, code gốc ở `overlorddev-main`. MIT. Git identity local: noreply GitHub |
| `release/ryzenhv-0.7.9/` | Bản release (đã kiểm tra sha256): `rhv`, `bin/rhv-info`, QEMU 11.1.2 có HVF, `RyzenHV.kext` |
| `re-notes/` | Ghi chú phân tích ngược (`SUMMARY.md`, danh sách import/field) |
| `test-images/` | Alpine virt 3.24.4 ISO, log thử QEMU |

## Công cụ
- Node 24: `~/.local/opt/node/bin` (**chưa có trong PATH**, phải export khi dùng).
- Ghidra 12.1.4: `~/.local/opt/ghidra_12.1.4_PUBLIC` (REA chỉ chấp nhận 12.1.x; đã build decompiler native cho macOS). JDK 21 hệ thống.
- REA 6.8.0 đã đăng ký MCP với Claude Code. Môi trường của MCP có `PATH`, `GHIDRA_INSTALL_DIR`, `JAVA_HOME`, `REA_DARWIN_SWIFTC_PATH`, `SDKROOT`.
- `~/.local/opt/swiftc-fix/swiftc`: wrapper né file thừa `/Library/Developer/CommandLineTools/usr/include/swift/module.modulemap` (trùng `SwiftBridging`).
- Command Line Tools có clang 16, SDK 15.2, có Kernel.framework.
- **Bị chặn trong auto mode:** chạy `make` trong `upstream/`, sửa `~/.zprofile`, sửa `~/.claude.json`. Chủ dự án tự chạy những việc này.

## Lệnh hay dùng
```bash
R=~/Downloads/dev/ryzenhv-native/release/ryzenhv-0.7.9
$R/bin/rhv-info               # trạng thái SVM
$R/bin/rhv-info hv            # front-end native
$R/bin/rhv-info stats         # bộ đếm VM/exit
$R/bin/rhv-info log           # log kernel của kext
sudo $R/bin/rhv-info hv sysctl 1   # bật kern.hv_support (mất sau mỗi lần boot)
sudo $R/bin/rhv-info selftest
cd $R && ./rhv doctor
# Thử QEMU (thoát bằng Ctrl-A X):
$R/qemu/bin/qemu-system-x86_64 -L $R/qemu/share/qemu -accel hvf -cpu host -smp 2 -m 1024 \
  -cdrom ~/Downloads/dev/ryzenhv-native/test-images/alpine-virt-3.24.4-x86_64.iso -boot d -nographic -serial mon:stdio
```

## Đã biết về Virtualization.framework (xem `re-notes/SUMMARY.md`)
- Tiến trình VM: `/System/Library/Frameworks/Virtualization.framework/Versions/A/XPCServices/com.apple.Virtualization.VirtualMachine.xpc/Contents/MacOS/com.apple.Virtualization.VirtualMachine` (fat; tách lát x86_64 bằng `lipo -thin x86_64`).
- Chỉ dùng **20 hàm `hv_*` công khai**. Không dùng `hv_vm_space_*` hay trap private.
- `hv_vm_create(0)`. `hv_vcpu_run_until(vcpu, -1)` (chạy không giới hạn, deadline không quan trọng).
- **APIC, IOAPIC, IPI giả lập trong userland** (`VzCore::Hypervisor::Apic`), nên **không cần APIC trong kernel**.
- Có `X86::VmxChecker`: tự kiểm tra trạng thái VMX, nên capability và giá trị VMCS phải nhất quán.
- `hv_vmx_read_capability`: 34 lần gọi, các ID 0x2, 0x5–0xe, 0x10.
- MSR native: 0x10, 0x174–0x176, 0xC0000081–84, 0xC0000100–102.
- 42 field VMCS. RyzenHV có chỗ lưu cho tất cả, nhưng các field sau **mới chỉ lưu, chưa có ngữ nghĩa**: mask/shadow CR0/CR4 (0x6000–0x6006), TPR threshold (0x401C), VMX instr info (0x440E). Cần kiểm tra thêm: tiêm sự kiện (0x4016/0x4018), interruptibility (0x4824), IDT-vectoring (0x4408).

## Nhật ký
- **2026-11-10:**
  - Dựng công cụ: Node, Ghidra, REA.
  - Tạo phân vùng RHVTEST, nạp RyzenHV 0.7.9.
  - **Lõi chạy được trên máy:** SVM 16/16, selftest 16/16, `rhv doctor` All good, không panic.
  - **QEMU `-accel hvf` boot Alpine Linux tới dấu nhắc đăng nhập** (khoảng 368 nghìn exit, `vmInvalidExits = 0`).
  - Phát hiện firmware ASUS tự boot vào RHVTEST, nên đã đổi vai hai phân vùng EFI.
  - Phân tích exit handler và capability của Virtualization.framework bằng REA (`re-notes/VZ-EXIT-AND-CAPS.md`;
    snapshot Ghidra ở `re-work/vmxpc.ghidra-snapshot`, mở lại bằng `open_binary` kèm `snapshot_path`).
  - Viết `tools/vzboot` (Swift, ký ad-hoc kèm entitlement `com.apple.security.virtualization`).
    **Virtualization.framework đã tạo VM và boot kernel Linux 6.18 trên RyzenHV** (`test-images/vzboot-run1.log`).
  - Guest panic sau `Run /init`: `write_ibpb` → `WRMSR 0x49` → `#GP`. Nguyên nhân: Virtualization.framework chép nguyên
    leaf 0x80000008 EBX của máy (AMD có IBPB/IBRS/SSBD) nhưng không biết MSR 0x48/0x49.
  - Bản vá trên nhánh git `native-vz` (`upstream/kext/Source/HvFrontend.cpp`, chưa commit): với mọi client trừ Parallels,
    ẩn nhóm SPEC_CTRL trong 0x80000008 EBX và SVM/TOPOEXT/PERFCTR/MWAITX trong 0x80000001 ECX. **Chờ build và thử.**
  - Build kext có bản vá (UUID 1996EEA7…, `make test` đạt hết), cài vào RHVTEST. Bản cũ ở `re-work/kext-prev/`.
  - **Mốc: Virtualization.framework boot Alpine Linux 3.24 hoàn chỉnh tới `localhost login:`** (OpenRC, virtio-blk, virtio-console),
    `vmInvalidExits = 0`, `assertFailures = 0` (`test-images/vzboot-run3.log`).
  - Cài Lima 2.3.0 vào `~/.local/opt/lima` (`limactl` có sẵn entitlement virtualization). Instance: `rhv-alpine` (template alpine, vz, 2 CPU, 2 GB).
  - **Mốc: Lima chế độ vz chạy được.** READY, `limactl shell` OK, ping Internet, `apk add` OK, stress-ng 60 s (cpu×2, vm 256M, hdd) đạt 4/4,
    `reboot` trong guest OK. `vmInvalidExits = 0`.
  - **Lỗi chập chờn:** sau lần reboot đầu (Lima tự reboot guest ở lần boot đầu tiên), có lúc vCPU0 kẹt ở firmware UEFI:
    `HLT` liên tục tại rip 0x7fa5813f, IF=1, khoảng 125 nghìn exit/s, CPU 100%. Lần thứ hai và lần reboot thủ công thì không bị.
    Nghi trạng thái ngắt hoặc tiêm sự kiện còn sót sau khi reset vCPU. Chưa tái hiện được.
  - Điều tra lỗi treo HLT:
    - Virtualization.framework **dùng lại vCPU khi guest reboot** (không gọi `vcpu_create` lại), nên trạng thái RyzenHV giữ trong kernel có thể còn sót.
    - Bộ xử lý HLT của nó: tăng RIP thêm độ dài lệnh. Nếu APIC (userland) không có IRR thì cho vCPU ngủ; nếu có IRR và IF=1 thì chạy lại ngay.
      Log lúc treo ghi `intr 0`, tức nó thấy ngắt đang chờ nhưng **không tiêm**. Nghi ngờ: interruptibility/STI shadow (0x4824),
      TPR (RyzenHV không đọc virtual-APIC page, `tSetApicAddress` bỏ qua), hoặc ISR/PPR.
    - Script tái hiện: `tools/repro-hlt.sh` (reboot lặp lại, khi treo thì dump flight recorder).
      Cần `rhv-info` mới: `cd upstream && make tools` (bản release không có lệnh `hv debug` / `hv flightdump`).
    - **Tái hiện được** (lần reboot thứ 2, `test-images/repro-hlt-093358/`). Flight recorder cho thấy: trước mỗi lần chạy, client chỉ ghi RIP=HLT+1,
      **không bao giờ ghi 0x4016** (không tiêm); interruptibility = 0; IF=1. Lần tiêm cuối của v0 là 0xEC (timer của Linux trước reboot).
    - Vòng lặp vCPU của Virtualization.framework (0x100157060): sau mỗi exit nó đọc TPR qua `read_register(0x31)`, tính PPR = max(TPR, lớp ISR),
      và chỉ tiêm khi lớp IRR cao nhất > PPR. Nếu 0x4016 còn valid thì nó không tiêm gì mới. Hàm 0x100042b00 kiểm tra interruptibility/IF/NMI-blocking.
    - **Nghi phạm chính:** khi phải cắt ngang lần chạy mà sự kiện chưa vào được guest (sau 16 lần hoãn vì kick), RyzenHV chỉ báo lại qua
      IDT-vectoring (0x4408) trên exit "external interrupt". Virtualization.framework không đọc trường đó, nên ngắt mất, bit ISR kẹt và PPR chặn mãi.
    - **Bản vá (nhánh `native-vz`, chưa commit):** `VmcsState::requeueEvent()` đặt lại sự kiện vào 0x4016 kèm bit valid, gọi ở nhánh timeslice
      trong `Vm.cpp`, có log "an undelivered injection … stays pending". Đã thêm unit test, `vmcs_state_test: OK`.
    - **Thêm cho debug:** snapshot in thêm `vtpr / tprpage / tprthr`; flight ghi thêm các lần client ghi 0x4002, 0x401c và TPR (0xF004).
      Nếu vẫn treo mà không có log requeue, thì xem `vtpr`: khác 0 nghĩa là thủ phạm là TPR.
    - Đã build (`make test` đạt hết) và cài vào RHVTEST: UUID **5CABAE0F**. Bản trước (chỉ có sửa CPUID, 1996EEA7) ở `re-work/kext-cpuidfix/`,
      bản gốc 0.7.9 ở `re-work/kext-prev/`. Chờ khởi động lại để chạy `repro-hlt.sh` 20 lần.
    - **Lần thử với kext 5CABAE0F (2026-11-11):** vẫn treo ngay lần reboot 1 (`test-images/repro-hlt-035303/`).
      `vtpr 0, tprpage 0, tprthr 0`, **không có** log requeue. Vậy không phải TPR và không phải đường trả ngược sự kiện
      (bản vá requeue vẫn giữ vì đúng, nhưng không chạm lỗi này). Lần tiêm cuối của v0 vẫn là 0xEC (timer Linux trước reboot):
      APIC của Virtualization.framework đã chặn ngay từ lúc reset (có thể còn ISR/IRR từ trước reboot).
    - Thêm debug bit 4: dừng flight recorder 64 exit sau khi client reset vCPU (ghi RIP 0xFFF0), để giữ lại cách guest reboot.
      `repro-hlt.sh` giờ bật `hv debug 5` trước mỗi lần reboot. Đã build (`make test` đạt) và cài vào RHVTEST: UUID **E97789B3**.
      Bản trước (5CABAE0F, có requeue) ở `re-work/kext-requeue/`.
    - **Kết quả với E97789B3 (`test-images/repro-hlt-040222/`):** Linux reboot bằng **`OUT 0xCF9, 6`** (exit 30, qual 0xcf90000).
      Lúc đó Linux đã tắt IF, Virtualization.framework có ngắt chờ (timer) nên đang bật interrupt-window (`4002=0x200084`).
      Sau reset (RIP=0xfff0, CS=f000), nó **vẫn ghi `4002=0x200084` ở mọi lần chạy**: APIC userland **giữ IRR qua reset bằng CF9**.
      Giả thuyết: ngắt cũ (0xEC) bị tiêm vào firmware khi firmware bật IF, firmware không EOI, ISR kẹt, timer firmware bị chặn.
      Chưa biết trên Intel có giống không. (Log bị vòng lặp HLT ghi đè 4 MB, chỉ còn 278 exit.)
    - Thêm debug bit 8: cùng bit 4, bắt đầu đếm 64 exit **từ lần tiêm đầu tiên sau reset**. Script giờ dùng `hv debug 13`
      và **tắt VM trước khi dump**. Đã build và cài vào RHVTEST: UUID **8D9021A8**; bản trước (E97789B3) ở `re-work/kext-debug4/`.
    - **`rhv-info` không cần sudo** khi có `rhv_allow_users=1` (kể cả `hv sysctl 1`, `hv debug`, `hv flightdump`). Claude tự chạy được vòng tái hiện;
      chủ dự án chỉ cần build kext và khởi động lại. Script đã bỏ sudo. Dump theo từng đoạn 100 exit (`hv flightdump 100 SKIP`), vì log ring chỉ ~64 KB.
    - **Nguyên nhân đã khoanh vùng (`test-images/repro-hlt-041203/ring-merged.txt` so với `ok-reboot-041925/`):**
      - Reboot **thành công**: ngắt đầu tiên tiêm vào firmware sau reset là **0x40** (timer của firmware).
      - Reboot **treo**: ngắt đầu tiên là **0xEC** (timer LAPIC của Linux, từ trước reset). Firmware không có handler/không EOI,
        bit ISR 0xEC kẹt, lớp 0xE chặn timer 0x40 của firmware → `HLT` mãi.
      - Tức là timer LAPIC của Linux **vẫn còn chờ hoặc vẫn bắn sau khi reset bằng `OUT 0xCF9,6`**. Đây là trạng thái trong APIC userland
        của Virtualization.framework. Chưa biết vì sao lại xảy ra trên RyzenHV mà (có lẽ) không xảy ra trên Intel.
      - Đã loại: TPR, đường trả ngược sự kiện, trap không hỗ trợ (VZ chỉ dùng trap 0,2,4,5,7,11,12,13 — đều có), TSC-deadline (VZ không xử lý MSR 0x6E0).
      - **Nghi phạm tiếp theo: TSC.** RyzenHV bỏ qua `hv_vcpu_write_msr(0x10)` / `hv_vcpu_set_tsc_relative` (VZ gọi 3 lần, có thể ở lúc reset).
        Guest báo "TSC warped randomly between CPUs, marking unstable". Cần đọc ngược Hypervisor.framework xem giá trị TSC nằm ở đâu
        trong trang dùng chung (cờ 0x100 trong `tVcpuRun`), rồi đặt VMCB tsc_offset = giá trị − rdtsc.
    - **Sửa TSC (2026-11-11):** đọc disassembly của Hypervisor.framework (`dyld_info -disassemble`):
      - `hv_vcpu_write_msr(0x10, t)` → rw[+0xc68] = t, rw[+0xc70] = 0, flags |= 0x108 (TSC tuyệt đối).
      - `hv_vcpu_set_tsc_relative(v, rel)` → rw[+0xc78] = rel, xóa +0xc68/+0xc70, flags |= 0x108. Header SDK: TSC guest = hv_tsc_clock() + rel.
      - `hv_tsc_clock()` = rdtsc + [commpage+8] − [commpage+0] (RyzenHV để 0 nên bằng TSC host).
      - Virtualization.framework gọi `set_tsc_relative` với cùng một giá trị (vm+0x48) cho mọi vCPU.
      - Bản vá trong `tVcpuRun` (cờ 0x100): abs≠0 thì offset = abs − rdtsc, ngược lại offset = rel + (cp[1]−cp[0]), rồi `vmcsWrite(0x2010)`.
        Biên dịch thử: sạch. Test: `tools/tsc-test` (3 ca). Trên kext cũ 8D9021A8: **2 FAIL** (tái hiện lỗi, đúng như mong đợi).
        Đã build (`make test` đạt) và cài vào RHVTEST: UUID **0CC1A63C**; bản trước (8D9021A8) ở `re-work/kext-debug8/`.
      - **Kết quả 0CC1A63C:** `tsc-test` **đạt 3/3** (relative, absolute, hai vCPU). Log kext ghi "vcpuN TSC set (relative …)".
        Trong guest Linux, độ lệch TSC giảm từ 1856 xuống **783 cycles** nhưng vẫn "unstable" → phần còn lại là **lệch TSC giữa các core vật lý
        của Ryzen** (ForgedInvariant chưa đồng bộ tuyệt đối). Cần bù theo từng core (đo lệch mỗi core lúc khởi động, cộng vào offset trước VMRUN) — việc riêng.
      - **Lỗi treo vẫn còn** (reboot 2, `test-images/repro-hlt-043640/`): reset ở exit 46069, lần tiêm đầu sau reset vẫn là **0xEC**. TSC không phải nguyên nhân.
  - **Docker qua Lima vz (2026-11-11, kext 0CC1A63C):** instance `rhv-docker` (`template:docker`, Ubuntu 26.04.1 LTS, kernel 7.0.0-38,
    Docker Engine 29.9.3, 2 CPU, 4 GB). Kết quả: `hello-world` (cả từ host qua `docker.lima` với `LIMA_INSTANCE=rhv-docker`),
    alpine, nginx `-p 8080:80` trả HTTP 200, `docker build` (python3 + chạy code lúc build), 20 container chạy cùng lúc,
    stress-ng trong container 60 s đạt 4/4, mount virtiofs từ host (thư mục nhà mount ở **/Users/liamhnam**, không phải `$HOME` của guest)
    với checksum file 70 MB khớp. dmesg không có panic/lockup. `vmInvalidExits = 0`.
  - **MỐC: Docker Desktop 4.99.0 (Intel) chạy trên RyzenHV (2026-11-11, kext 0CC1A63C).** Tải từ desktop.docker.com
    (`~/Downloads/dev/docker-desktop/Docker.dmg`, sha256 0584a21e…, chữ ký Docker Inc 9BNSXJN65R, notarized), cài vào /Applications,
    chủ dự án tự đồng ý điều khoản và nhập mật khẩu. **Không vá app nào**: VM của Docker chạy trong tiến trình
    `com.apple.Virtualization.VirtualMachine`, trap từ pid đó đi qua RyzenHV; không có hyperkit/qemu/krun. CLI ở `~/.docker/bin`.
    Engine: kernel 7.0.14-linuxkit, 16 CPU, ~7,7 GB (cmdline Docker có `tsc=reliable`, nên lệch TSC không ảnh hưởng).
    Kết quả: hello-world, alpine, nginx `-p 8080:80` trả HTTP 200 **trên localhost của macOS**, mount `~/…` (checksum khớp),
    `docker build`, `docker compose` (nginx + redis, HTTP 200 + PONG), 20 container cùng lúc, stress-ng 60 s (cpu×4, vm×2, hdd) đạt 7/7.
    `vmInvalidExits = 0`, `assertFailures = 0`.
  - **Tự bật kern.hv_support lúc khởi động (2026-11-11, chưa build):** `start()` giờ mặc định gọi `setSysctl(true)` khi có `rhv_hv`
    (tắt bằng `rhv_hv_sysctl=0`), và một `thread_call` kiểm tra lại mỗi 5 s trong 5 phút. Nếu giá trị bị đặt lại khác 1 (nghi AppleHV,
    vốn nạp sau RyzenHV: index 154 so với 76), kext ghi 1 lại và đọc kiểm tra qua `hv_get_support()` và OID. `stop()` hủy thread_call.
    Đã build (`make test` đạt, 406 symbol khớp kernel) và cài vào RHVTEST: UUID **B2173654**; bản trước (0CC1A63C, sửa TSC) ở `re-work/kext-tsc/`.
    **Kết quả sau khởi động lại:** log "kern.hv_support was 0, now 1" ở giây 0.00026; AppleHV **không** đặt lại (thread_call không phải ghi lần nào).
    Mở Docker Desktop là chạy luôn (engine sẵn sàng sau ~6 s), hello-world và nginx HTTP 200, **không cần lệnh nào**.
  - **Bỏ boot-args + kiểm tra entitlement (2026-11-11, chưa build):**
    - `RyzenHV.cpp`: SVM bật **mặc định** khi mọi CPU dùng được (`rhv_enable_svm=0` để chỉ probe, `-rhv_off` để không chạy).
    - `HvFrontend.cpp start()`: front-end native bật mặc định khi `svm::activeOnAllUsable()` (`rhv_hv=0` để tắt).
    - `tVmCreate`: như AppleHV, chỉ cho tạo VM nếu tiến trình có entitlement `com.apple.security.hypervisor` (hoặc `com.apple.vm.hypervisor`),
      nếu không trả `HV_DENIED` (0xfae94007) và ghi log. Tắt bằng `rhv_entitlement=0`. Đã kiểm tra: VZ XPC, QEMU bản release, tsc-test,
      hvf-ro-map-test đều có entitlement; `hvf-smoke` không có nên Makefile giờ ký nó.
    - `rhv_allow_users` chỉ áp dụng cho user client riêng (rhv-info), không liên quan đường native; bỏ đi thì lệnh điều khiển của rhv-info cần sudo.
    - Đã build (`make test` đạt, 407 symbol khớp, `make tools` ký hvf-smoke) và cài vào RHVTEST: UUID **1EC9F836**; bản trước (B2173654) ở
      `re-work/kext-autosysctl/`. **Đã bỏ cả 3 boot-arg `rhv_*`** khỏi config RHVTEST, chỉ còn dấu `rhvtest=1`;
      config cũ ở `re-work/config-RHVTEST-with-rhv-bootargs.plist`.
    - **Kết quả sau reboot, KHÔNG còn boot-arg `rhv_*`:** SVM tự bật 16/16, front-end tự đăng ký, `hv_support` tự = 1. Docker Desktop chạy
      (engine ~6 s, hello-world, nginx HTTP 200), `tsc-test` OK, `hvf-smoke` (đã ký) PASS. Bản `hvf-smoke` không ký bị từ chối
      `0xfae94007` (HV_DENIED), log "has no com.apple.security.hypervisor entitlement; hv_vm_create denied".
      Lưu ý: bỏ `rhv_allow_users` nên lệnh điều khiển của `rhv-info` (hv sysctl / debug / flightdump) giờ cần sudo; info/log/stats/hv vẫn không cần.
  - **UTM 5.0.8 (2026-11-11, kext 1EC9F836, không boot-arg):** tải từ github.com/utmapp/UTM (`~/Downloads/dev/utm/UTM.dmg`, sha256 7b2d7b2e…,
    chữ ký Turing Software WDNLXAD4W8, notarized), cài vào /Applications. Tạo VM bằng AppleScript (khóa serial ports phải viết `«class SrPt»`).
    - **QEMU + HVF** (`rhv-alpine-qemu`): Alpine boot tới `localhost login:` trên serial ptty; trap `vcpu_run` từ QEMULauncher (pid 1563) qua RyzenHV.
    - **Apple Virtualization** (`rhv-alpine-apple`): ổ removable chỉ lưu bookmark, chế độ Apple báo "Cannot access resource" → chép ISO vào
      `<vm>.utm/Data/` và đặt `Drive[0].ImageName`. Guest chạy kernel Linux trên cả 2 vCPU (rip 0xffffffff…), rảnh 1,3% CPU, không có exit lỗi.
      Console trống vì serial của chế độ Apple là hvc0 còn ISO Alpine in ra ttyS0 (không phải lỗi RyzenHV).
  - **LỖI: VM Windows trong UTM (QEMU) làm Mac khởi động lại (2026-11-11, kext 1EC9F836).** Chủ dự án tạo VM "Windows" (QEMU, q35,
    UEFI, TPM, 4 GB, CPUCount 0 = 16 vCPU, CPU "default", virtio-vga-gl), chạy 3 lần đều làm host khởi động lại → **host panic trong kext**.
    Không có file .panic, không còn log (khởi động lại đột ngột). Chuẩn bị tái hiện: thêm `debug=0x100` vào boot-args RHVTEST
    (panic thì dừng và hiện chữ trên màn hình; config trước ở `re-work/config-RHVTEST-before-debug.plist`), và `tools/capture-log.sh`
    ghi `rhv-info log -f` ra file kèm fsync từng dòng.
    - Lần thử có watchdog (`tools/safe-vm-test.sh`, 16 vCPU) vẫn làm Mac **đơ cứng ~48 s** sau khi kernel Windows chạy, độ trễ host không tăng
      trước đó → không phải "đói CPU". Giả thuyết TLB/ASID bị loại khỏi vị trí nghi số 1.
    - **NGUYÊN NHÂN (tìm bằng đọc code, 2026-11-11):** file log của macOS mang ngày **năm 2262** (= giới hạn int64 nano giây) → đồng hồ host
      nhảy hỏng. QEMU HVF bật "native" cho MSR TSC (slot 144) và `setNative()` gỡ chặn **cả đọc lẫn ghi** MSR 0x10. Trên SVM, TSC_OFFSET
      chỉ áp cho lệnh đọc, nên `WRMSR TSC` của guest (Windows đồng bộ TSC giữa các CPU lúc boot) **ghi thẳng vào TSC thật của core host**.
      **Sửa:** `setNativeRead()` (MsrMap.hpp) chỉ cho đọc native, ghi vẫn trap; `run()` (Vm.cpp) bắt `WRMSR 0x10` khi `fTscNative`, đặt
      `tscOffset = giá trị − rdtsc`, bước qua lệnh, chạy tiếp (client không thấy exit; log "guest wrote its TSC"). Test `msr_map_test` đạt;
      đã build và cài vào RHVTEST: UUID **51B7226E** (bản trước 1EC9F836 ở `re-work/kext-entitlement/`). VM Windows giảm còn 2 vCPU (config cũ: `re-work/utm-Windows-config-16vcpu.plist`).
      **Đã thử (06:14, 2 vCPU, 180 s): host ổn**, độ trễ trung bình 0,043 s, tối đa 0,11 s; đồng hồ đúng. Log: ở giây 108 Windows ghi TSC = 0 rồi
      0x56ce4ca45e trên cả 2 vCPU → kext giữ thành offset (đúng là thủ phạm cũ). Cả 2 vCPU chạy kernel Windows (rip 0xfffff8…).
      Chưa thử 16 vCPU và chưa xem màn hình guest tới desktop. **DEBUGCTL 0x1d9 / SPEC_CTRL 0x48: KHÔNG có rủi ro** (kiểm 2026-11-11): `msrOp` từ chối NATIVE_ENABLE cho MSR không có trong `MsrMap::info()` (K_UNKNOWN), nên hai MSR này luôn bị chặn; log cho thấy QEMU chỉ xin idx 1–6 (STAR…TSC_AUX) và 144–149 (TSC, SYSENTER, FS/GS), đều được VMCB/VMLOAD hoặc kext tự hoán đổi.
    - **Cách tạm khi treo:** `limactl stop -f <instance> && limactl start <instance>` (tiến trình VZ mới, APIC sạch). Guest Lima/Docker hiếm khi tự reboot.
  - Cảnh báo vô hại trong guest: RDMSR 0xc0010015 / WRMSR 0xc001001f (MSR AMD) → `#GP`, Linux tự xử lý.
  - Ghi nhận: "a TSC write through hv_vcpu_write_msr(0x10) is not applied yet". Virtualization.framework đặt TSC qua MSR 0x10,
    RyzenHV bỏ qua. Cần làm nếu sau này guest lệch giờ.

## Đổi tên kext (2026-11-11)
- Kext giờ là **`AMDVirt.kext`** (file thực thi `AMDVirt`, bundle id `com.liamhnam.AMDVirt`). Class C++ đổi thành `com_liamhnam_AMDVirt…` (RHV_SERVICE_NAME), log kernel có tiền tố `AMDVirt:`, `rhv-info` in "AMDVirt kext". **rhv-info/shim cũ (release 0.7.9) không còn tìm thấy kext**, dùng `upstream/build/rhv-info`. **Đổi tên toàn bộ (2026-11-11):** file nguồn (`AMDVirt.cpp`, `AMDVirtShared.h`…), lệnh `amdvirt` / `amdvirt-info` (`upstream/build/amdvirt-info`), boot-arg `amdvirt_*` và `-amdvirtoff` (boot-arg `rhv_*` không còn tác dụng), macro `AMDVIRT_*`, namespace `amdvirt::`, nhãn launchd `com.liamhnam.amdvirt.hvsysctl`. Chỉ README credit còn link OverlordDev; LICENSE giữ dòng bản quyền gốc. Bản đổi tên toàn bộ: UUID **2E0AEE27** — đã boot, amdvirt-info OK, 4 class mới trong ioreg, Docker OK; commit 4518232 đã push, CI xanh. Bản trước trong re-work/kext-amdvirt-oldclass (2A23411E).
- RHVTEST: mục `Kernel → Add` số 29 đã đổi BundlePath/ExecutablePath; `RyzenHV.kext` (51B7226E) cất ở `re-work/kext-tscfix/`,
  config trước ở `re-work/config-RHVTEST-before-amdvirt.plist`. Bản AMDVirt đầu tiên: UUID **2A23411E** — đã boot, nạp đúng `com.liamhnam.AMDVirt`, SVM 16/16, hv_support tự = 1, Docker hello-world OK; commit 0ccfe58 đã push.
- Nếu không boot được: Esc → EFI dự phòng, rồi trả config cũ và kext cũ.

## Vòng thử kext mới (quy trình)
1. Chủ dự án: `cd ~/Downloads/dev/ryzenhv-native/upstream && make test && make kext` (Claude bị chặn chạy `make`).
2. Claude: chép `upstream/build/AMDVirt.kext` vào RHVTEST, giữ bản cũ thành `RyzenHV.kext.prev` ở `re-work/`.
3. Khởi động lại (máy tự vào RHVTEST), chạy `sudo …/rhv-info hv sysctl 1`.
4. Chạy `tools/vzboot/vzboot test-images/vmlinuz-virt test-images/initramfs-virt 60`, rồi đọc log.

## Checklist
- [x] Mount EFI, sao lưu EFI chính (thư mục và file zip)
- [x] Tạo phân vùng RHVTEST làm EFI test
- [x] Cài Command Line Tools, Node 24, Ghidra 12.1.4, REA và đăng ký REA
- [x] Kiểm tra CPU: có SVM/NPT/AVIC, không có `CpuidUserDis`
- [x] Lõi SVM, NPT, giả lập VMX, front-end native (OverlordDev)
- [x] Boot với RyzenHV, SVM 16/16, selftest đạt
- [x] QEMU `-accel hvf` boot được Linux
- [x] Build và test RyzenHV từ mã nguồn (`make test`, `make kext`, `make tools`)
- [x] Phân tích Virtualization.framework bằng REA (20 hàm `hv_*`, exit handler, capability, vòng tiêm ngắt)
- [x] `tools/vzboot`: Virtualization.framework boot Alpine Linux tới dấu nhắc đăng nhập
- [x] Sửa panic IBPB: ẩn bit CPUID AMD mà client không giả lập
- [x] Lớp giả lập VMX đủ cho Virtualization.framework (capability, CR shadow, tiêm sự kiện, MSR native) — chứng minh bằng Lima/Docker chạy
- [x] Lima chế độ vz (shell, mạng, apk, stress-ng, reboot)
- [x] Docker Engine qua Lima vz (run, build, port, 20 container, stress, mount virtiofs)
- [x] Docker Desktop 4.99.0 bản mới nhất, không vá app (run, port trên localhost, mount, build, compose, 20 container, stress)
- [x] UTM 5.0.8: chế độ QEMU + HVF (Alpine tới login) và chế độ Apple Virtualization (kernel Linux chạy)
- [x] TSC theo từng vCPU (`hv_vcpu_set_tsc_relative`, `write_msr(TSC)`; `tools/tsc-test` đạt)
- [x] Giữ lại sự kiện tiêm chưa vào được guest (requeue) — đúng nhưng không phải nguyên nhân lỗi treo
- [x] Kext tự bật `kern.hv_support` lúc khởi động
- [x] Không cần boot-arg `rhv_*` nào (SVM và front-end tự bật khi có SVM)
- [x] Kiểm tra entitlement `com.apple.security.hypervisor` khi tạo VM (HV_DENIED nếu thiếu)
- [x] VM Windows (UTM/QEMU) làm Mac đơ/khởi động lại — lỗi guest ghi TSC thật, đã vá; 2 vCPU chạy 3 phút ổn
- [ ] Thử Windows với 16 vCPU và chạy tới desktop
- [ ] **LỖI (2026-11-12, kext 4569203E): chạy UTM Windows (CPUCount lại = 0 → 16 vCPU) cùng Docker Desktop (16 CPU) → Mac đơ cứng ngay, phải bấm nguồn.** Không có .panic, unified log không còn dòng AMDVirt nào (mất theo lần tắt cứng). Đọc code: V_INTR_MASKING luôn bật (VmcsState.hpp:294), INTR/NMI bị chặn, ASID riêng mỗi vCPU → đường ngắt host có vẻ đúng. Chưa rõ nguyên nhân; nghi: 32 vCPU trên 16 luồng (lock-holder preemption + kick IPI `mp_cpus_call`), hoặc lỗi 16 vCPU cũ chưa từng thử lại sau bản vá TSC. Biến debug `gPend/gSnap/gStuck[vcpu id]` trong HvFrontend.cpp dùng chung giữa các VM (chỉ khi bật `hv debug`). Bước tiếp: tái hiện có `tools/capture-log.sh`, thử từng VM 16 vCPU riêng, rồi ghép.
- [ ] Thử sleep/wake khi VM đang chạy, và chạy liên tục nhiều giờ
- [ ] Đưa RyzenHV vào EFI chính (bản dùng hằng ngày)
- [ ] Lỗi treo khi guest tự reboot (timer 0xEC cũ của Linux tiêm vào firmware sau reset 0xCF9). **Thử lại trên 0.1.0 (2026-11-11 07:25): vẫn treo ở lần reboot 3** (`test-images/repro-hlt-072559/`, HLT 0x7fa5813f). Trace cũ (`repro-hlt-041203/ring-merged.txt`): ngay lúc reset (w49171) VZ đã bật interrupt-window (4002=200084), và lần tiêm đầu sau reset là **chính VZ ghi 4016=800000ec** (w50625) → APIC userland của VZ giữ IRR 0xEC qua reset. RE tiếp (2026-11-11, xem `re-notes/SUMMARY.md` mục Reset): giả thuyết "nhánh dự phòng vì caps PROCBASED2" **bị loại** (VZ luôn dùng APIC userland). Lambda reset mỗi vCPU không chạm APIC; IRR nằm ở apic+0x48..0x64. Chưa thấy chỗ VZ xoá IRR/LVT khi reset — có thể ở reset thiết bị bất đồng bộ. Ghidra nạp lại vmxpc quá 330 s, dùng `re-work/vmxpc.s` + script Mach-O (chained fixups) thay thế.
- [ ] Bù lệch TSC giữa các core vật lý. **Thử bù tĩnh (2026-11-11, kext 6A36C0C1, chưa commit): KHÔNG hiệu quả.** Kext đo lúc boot (rendezvous, ping-pong kiểu NTP): lệch tĩnh tối đa -449 cycles. Guest Linux vẫn báo warp 2712 cycles (trước sửa 1798) và rơi về HPET (`test-images/tsc-before-skewfix.txt`, `tsc-after-skewfix.txt`). Userland ping-pong 16 luồng thấy TSC đi lùi tới ~16000–23000 cycles → lệch **động** (có thể do ForgedInvariant đồng bộ lại TSC khi chạy), bù một lần không đủ. **Đã gỡ code bù** (git checkout), ghi thành Known issue trong README/TESTING; RHVTEST đã trả về kext 4569203E (có hiệu lực sau lần khởi động lại tới). Không dùng AI/REA phân tích ForgedInvariant (ChefKiss).
- [x] Bỏ phần vá app cũ (HvPatch, shim, patchcheck, script Parallels/Docker, app/, release/) — kext 0.1.0 UUID **B932D7D0**, đã boot: hvf-smoke PASS, Docker hello-world + nginx 200, UTM qemu/apple khởi động, vmInvalidExits 0; commit b33f976, CI xanh. Bản trước ở `re-work/kext-allrenamed/` (2E0AEE27)
- [x] DEBUGCTL/SPEC_CTRL: không có rủi ro (kext từ chối native cho MSR ngoài MsrMap)
- [x] Sửa race condition (`hv_vm_map`/`hv_vm_destroy`, vCPU sau `exec()`): registry `gLive` + `enterVm/leaveVm` + `taskGuarded/threadGuarded` trong HvFrontend.cpp; destroy chờ `busy`=0. Kext UUID **4569203E** đã boot: hvf-smoke, hvf-ro-map-test, Docker+nginx, Lima OK; `hvf-race-test` 220 vòng PASS, không panic. Trường hợp exec() chỉ kiểm bằng đọc code. Commit 26f2f7c, CI xanh. Bản trước ở `re-work/kext-nopatch/` (B932D7D0)
- [x] Commit nhánh `native-vz` (aeb0236) và đưa lên repo công khai **github.com/liamhnam/AMDVirt** (nhánh main, README mới, cảm ơn OverlordDev)
- [ ] **LỖI (2026-11-12): mở UTM + Docker Desktop cùng lúc làm Mac đứng.** Nguyên nhân: kext **ghim (wire) toàn bộ RAM guest** lúc `hv_vm_map` (`IOMemoryDescriptor::prepare`). Đo: nền 4,2 GB wired; chỉ Docker (8 GB, 8 CPU) → 12,3 GB wired (`vmPagesMapped` 2097152); thêm Windows UTM 4 GB → ~16,3 GB trên máy 16 GB → hết RAM, WindowServer/GPU treo (gpuRestart lúc 05:03–05:04 chỉ là hệ quả). Cách tạm: giảm RAM Docker (Settings → Resources) để tổng RAM VM ≤ ~8 GB. **ĐÃ SỬA (2026-11-12, kext 31793FB7):** ghim lười theo khối 2 MiB qua NPF (`GuestRegions.hpp`, `faultIn`), giới hạn chung = ½ RAM (`amdvirt_pin_limit_mb`), vượt thì gỡ khối ít dùng theo bit Accessed (clock, `evictLocked`/`reclaim`); `amdvirt_lazypin=0` để quay về cách cũ. Đo: Docker một mình ghim ~0,95 GB (trước 8 GB); Docker + Windows 4 GB/4 vCPU chạy 3 phút, độ trễ TB 0,056 s, Windows boot đẩy tổng ghim lên ~4,5 GB (wired 8,4 GB). hvf-smoke/ro-map/race PASS. Bản trước ở `re-work/kext-racefix2/` (4569203E). Commit (push) xem git log.
- [ ] (Tuỳ chọn) Gửi PR cho OverlordDev
- [ ] (Tuỳ chọn) Phân tích `AppleHV.kext` để so khớp mã lỗi/ngữ nghĩa từng hàm

## Nguyên tắc
- Clean-room: chỉ ghi lại hành vi và giao diện của Apple vào `re-notes/`, **không chép code** của Apple.
- **Không dùng AI/REA phân tích NootedRed** (tác giả ChefKiss cấm).
- Ưu tiên gửi PR lên repo gốc của OverlordDev. Nếu họ không nhận hướng "không vá app" thì fork, giữ lõi đồng bộ.
- Mỗi thay đổi kext đều phải thử trên RHVTEST trước, và luôn giữ EFI dự phòng.
