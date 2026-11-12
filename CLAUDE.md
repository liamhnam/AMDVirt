# AMDVirt — hướng dẫn cho Claude Code

Bạn đang ở repo AMDVirt (kext Hypervisor.framework cho Hackintosh AMD Ryzen của chủ dự án), nhánh `development`.
Nếu thư mục làm việc `~/Downloads/dev/ryzenhv-native/` không còn, dựng lại theo **`dev/RESTORE.md`**.

Trước khi làm gì trong repo này, đọc **`dev/HANDOFF.md`** (bàn giao đầy đủ: mục tiêu, cách làm việc, trạng thái, lỗi mở, phương án đã/chưa
thử). Nhật ký chi tiết: `dev/JOURNAL.md`. Ghi chú dịch ngược: `dev/re-notes/`. Công cụ thử: `dev/tools/`.

Quy tắc ngắn (chi tiết trong HANDOFF):
- Trả lời tiếng Việt, có bằng chứng, không đoán mò.
- Commit chỉ đứng tên `Ho Hoang Nam <29671651+liamhnam@users.noreply.github.com>`, **không** thêm `Co-Authored-By` Claude.
- Không chạy `make` (bị chặn) — chủ dự án build; Claude cài kext vào RHVTEST, kiểm sau khi khởi động lại.
- Hỏi trước bài thử có thể làm Mac treo; kiểm tra VM đang chạy trước khi bật/tắt VM.
- Không vá app, không sửa file hệ điều hành; không chép code Apple; không phân tích NootedRed/ForgedInvariant.
- Sau mỗi phiên: cập nhật `dev/HANDOFF.md` và `dev/JOURNAL.md` (và bản ở `~/Downloads/dev/ryzenhv-native/`).
