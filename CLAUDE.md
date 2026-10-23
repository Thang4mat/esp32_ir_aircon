# CLAUDE.md

> File này là bộ nhớ của dự án. Đọc toàn bộ ở đầu mỗi phiên làm việc, trước khi làm bất cứ việc gì khác.

## 0. Ngôn ngữ

- Trả lời bằng tiếng Việt.
- Giữ nguyên tiếng Anh cho: code, tên biến, tên hàm, đường dẫn file, thuật ngữ kỹ thuật (fallback, fail fast, runtime, stack trace, context window...). Không dịch.

## 1. Quy trình đầu phiên (bắt buộc)

1. Kiểm tra `CLAUDE.md` ở thư mục gốc dự án.
   - Chưa có: tạo mới, điền mục 6 (Cấu trúc dự án) và mục 7 (Tiến độ) từ những gì khảo sát được, rồi báo người dùng xác nhận.
   - Đã có: đọc và tuân theo. Nếu nội dung mâu thuẫn với code thực tế, báo người dùng, không tự đoán.
2. Xác nhận ngắn gọn (1-2 câu) phiên này sẽ làm gì, dựa trên mục 7.

## 2. Nguyên tắc thiết kế

- **Simple beats complex.** Không overengineer. Chọn giải pháp đơn giản nhất đáp ứng yêu cầu hiện tại.
- **One way, no fallbacks.** Mỗi việc có đúng một cách làm đúng. Không thêm đường thay thế, không thêm backup cho cơ chế chính. Tin cơ chế chính.
- **Fail fast.** Khi precondition không thỏa, `throw` error rõ ràng. Không nuốt lỗi, không trả về giá trị mặc định để che lỗi.
- **Clarity over compatibility.** Code rõ ràng quan trọng hơn backward compatibility. Không giữ code cũ chỉ để tương thích nếu người dùng chưa yêu cầu.
- **Separation of concerns.** Mỗi hàm chỉ có một trách nhiệm.
- **TypeScript trước runtime check.** Để type system bắt lỗi. Không thêm validation runtime thừa cho dữ liệu mà type đã đảm bảo.

## 3. Phương pháp phát triển

- **Surgical changes.** Chỉ sửa đúng chỗ cần sửa. Không refactor, đổi format, hay dọn code ngoài phạm vi yêu cầu. Thấy vấn đề khác thì báo, không tự sửa.
- **Fix root cause.** Sửa nguyên nhân gốc, không vá triệu chứng.
- **Evidence-based debugging.** Thêm logging tối thiểu, có mục tiêu. Gỡ log tạm sau khi xong.
- **Collaborative.** Khi có nhiều hướng hợp lý, nêu ngắn gọn các lựa chọn và đề xuất một hướng. Chờ người dùng chọn nếu thay đổi lớn.

## 4. Giao thức điều tra bug

Làm theo đúng thứ tự, không nhảy bước:

1. **Reproduce.** Tái hiện bug ổn định. Không tái hiện được thì chưa coi là bug; báo người dùng và xin thêm thông tin.
2. **Hypothesis.** Viết ra một giả thuyết cụ thể, kiểm chứng được, trước khi sửa code.
3. **Isolate.** Loại trừ từng biến cho đến khi chỉ còn một điểm lỗi hợp lý.
4. **Evidence.** Xác nhận bằng stack trace, log, hoặc test fail. Giả thuyết đẹp nhưng không có bằng chứng thì chưa đủ.
5. **Fix.** Chỉ sửa sau khi đã chứng minh được lỗi. Không sửa thứ chưa chứng minh là hỏng.
6. **Verify.** Chạy lại bước 1 để xác nhận bug đã hết.

## 5. Quản lý context window và "vùng ngu ngốc"

**Vùng ngu ngốc (dumb zone):** khi context window chứa quá nhiều thông tin (khoảng 200k-250k tokens), chất lượng suy luận và độ chính xác của model giảm rõ rệt. Biểu hiện: quên quy tắc trong file này, lặp lại lỗi đã sửa, bịa tên hàm hoặc đường dẫn, sửa sai chỗ.

**Claude phải chủ động cảnh báo người dùng:**

| Mức context (ước lượng) | Hành động |
|---|---|
| ~120k-150k | Nhắc nhẹ: context đang lớn, nên kết thúc tác vụ hiện tại gọn gàng. |
| ~150k-200k | Cảnh báo rõ: sắp vào vùng ngu ngốc. Cập nhật mục 7, đề xuất chạy `/compact` hoặc mở phiên mới. |
| 200k+ | Dừng nhận việc mới. Cập nhật mục 7 ngay, yêu cầu người dùng chạy `/clear` hoặc mở phiên mới. |

Quy tắc áp dụng:

- Dùng số liệu thực khi có (hiển thị usage, lệnh `/context`). Nếu không có, ước lượng dựa trên độ dài hội thoại, số file đã đọc, và output tool; nói rõ đó là ước lượng.
- Cảnh báo bằng 1-2 câu, nêu mức hiện tại và hành động đề xuất. Không giảng giải.
- Cũng cảnh báo sớm khi thấy dấu hiệu của chính mình đang suy giảm (lặp lại, mâu thuẫn với điều đã nói, quên quy tắc), dù chưa tới ngưỡng.
- Trước khi người dùng dọn context, luôn cập nhật mục 7 để phiên sau tiếp tục được mà không mất thông tin.
- Giữ context sạch: không đọc cả file lớn khi chỉ cần một đoạn, không dán lại output dài không cần thiết.

## 6. Cấu trúc dự án

Dự án: ESP32 điều khiển máy lạnh Midea Inverter 1HP MAFA-09CDN8 (chỉ có IR, giao thức COOLIX) qua Web UI tự host trên ESP32. Board có: ESP32 DevKit V1. ESP32-C3 SuperMini CHƯA có: KHÔNG viết code C3 cho tới khi người dùng yêu cầu.

Mã nguồn: người dùng đã thêm repo `esp32_ir_aircon` vào thư mục dự án (không cần zip). Nếu phiên mới không thấy mã nguồn thì xin người dùng cung cấp lại, không dựng từ trí nhớ. Làm việc tại `/home/claude/work/esp32_ir_aircon/`; giao file đã sửa dạng `esp32_ir_aircon_patch.zip` (đúng đường dẫn tương đối trong repo).

```
README.md                  hướng dẫn build/nạp/dùng, mô hình hoá remote, giả định chưa thu
capture/                   firmware thu tín hiệu IR (IRrecvDumpV2), chân RX GPIO15
controller/                firmware chính (DevKit V1)
  platformio.ini           platform/lib_deps không ghim phiên bản
  src/coolix_frames.h      mã hoá khung COOLIX (24/48-bit), state(mode,fan,temp), eco(), timer...
  src/ac_controller.h      AcState, NVS, chọn khung gửi, quy luật remote (comment đầu file)
  src/main.cpp             Wi-Fi (IP tĩnh), mDNS, WebServer, API
  src/web_ui.h             HTML/JS Web UI (hàm render() tô sáng nút theo state)
  src/config.h             chân GPIO, dải nhiệt, IP tĩnh, AC_CLEAN_DURATION_MS
  src/secrets.h.example    placeholder Wi-Fi (secrets.h thật không nằm trong zip)
test/                      test logic trên PC (stub Arduino/IRsend/Preferences)
  run.sh, test_controller.cpp
```

Bằng chứng gốc: `aircon.txt` ... `aircon28.txt` (đầu ra IRrecvDumpV2 của remote thật, người dùng upload). `aircon12`-`aircon19`: Eco/Sleep/Turbo/quạt/hẹn giờ ở Auto-Fan. `aircon20`-`aircon28` (2026-10-21/23): đóng các giả định còn lại (chi tiết mục 7). Handoff: `handoff-esp32_ir_aircon_1..4.md` (bối cảnh, quy luật Sleep/Eco/hẹn giờ/Light/Turbo/Clean, bộ test).

Lệnh:
- Test PC: `./test/run.sh` hoặc `sh test/run.sh` nếu mất quyền thực thi (cần g++ C++17). Kết quả gần nhất: `321/321 dat` (người dùng đã chạy xác nhận; đã chạy 22 đột biến, đều bị bắt). Đã thêm 3 ca `aircon28` (Turbo rồi Eco): kỳ vọng `324/324`, CHỜ người dùng chạy xác nhận.
- Build/nạp: `cd controller && pio run -t upload && pio device monitor` (container KHÔNG có PlatformIO: không được khẳng định "build được"; người dùng build/nạp và báo lại).
- Đóng gói: xoá `test/.test_bin`, kiểm tra file ẩn (`.gitignore`), chạy lại test từ chính bản giải nén.

Quy ước: mỗi ca test mới phải kèm tín hiệu thu làm bằng chứng; so độ phủ bằng đột biến (không chỉ đếm số ca). Giả định chưa có tín hiệu thu phải ghi rõ trong README.

## 7. Tiến độ

- **Đã xong:**
  - Firmware DevKit V1 (Power, Mode, Fan, Temp, Swing, Sleep, Eco, Hẹn giờ BẬT/TẮT, Light, Turbo, Clean), Web UI, README. Bản cũ (trước Light/Turbo/Clean) đã build/nạp/thử trên máy thật ở mức chung. IP tĩnh 192.168.6.200 hoạt động tốt (người dùng xác nhận; nằm trong dải DHCP của router, router không cho đặt ngoài dải; ca đã ĐÓNG). Bản v2 (Light/Turbo/Clean, quy luật quạt mới, hẹn BẬT+TẮT cùng lúc) đã build/nạp/thử trên máy thật, mọi thứ ổn (người dùng xác nhận 2026-10-23).
  - Phiên 2026-10-22: SỬA CODE theo danh sách đã xác nhận: Clean 20 phút; Light phát được trong Clean; `eco(st,on,fan)` (khung TẮT mang mã quạt); Dry/Auto luôn mã quạt `000`; bật Eco/Sleep đặt quạt Auto; đổi quạt tắt Sleep+Turbo (không tắt Eco); `setClimate()` từ chối đổi quạt ở Dry/Auto; Web UI khóa nút quạt ở Dry/Auto + chú thích; README đóng các giả định. Test 321/321, 16/16 đột biến bị bắt. File đổi: `coolix_frames.h`, `ac_controller.h`, `config.h`, `web_ui.h`, `README.md`, `test/test_controller.cpp`, `test/stub/*` (nội dung stub không đổi). `main.cpp`, NVS không đổi.
- **Bản chuẩn:** v2 (`esp32_ir_aircon_2.zip`) thay thế hoàn toàn bản v1 (người dùng đối chiếu, đã kiểm lại: logic firmware trùng v1, test 321 > 306, bắt thêm đột biến NVS-fan mà v1 để lọt). Người dùng đã chạy test 321/321.
- **Trạng thái:** DỰ ÁN HOÀN THÀNH (người dùng xác nhận 2026-10-23). Không còn việc đang làm.
- **Việc tiếp theo:**
  1. (Tuỳ chọn, không quan trọng) Người dùng tự kiểm tra lỗi mDNS `.local` qua đêm khi có thời gian; đã vào được bằng IP tĩnh.
  2. Bản C3 SuperMini chỉ khi người dùng có board và yêu cầu.
- **Quy luật quạt/Sleep/Eco/Turbo/Clean (ĐÃ XÁC NHẬN), mỗi mục có bằng chứng:**
  1. Bật Eco hoặc bật Sleep thì `fan = Auto` (Eco `aircon12/13/14`; Sleep `aircon15/17/18/25`).
  2. Tắt Eco/Sleep: `fan` giữ mức hiện tại (`aircon18/19`).
  3. Đổi tốc độ quạt: tắt Sleep và Turbo, KHÔNG tắt Eco. Chọn lại đúng mức đang lưu khi Sleep không tắt Sleep.
  4. Đổi nhiệt giữ Sleep, Eco, Turbo, hẹn giờ. Hẹn giờ giữ Eco và giữ Turbo (`aircon14`, `aircon26/27`).
  5. Bật Turbo không đổi quạt. Bật Eco hoặc Sleep tắt Turbo (`aircon27`). Turbo rồi Eco: quạt về Auto, khung tắt Eco `83D0` dù quạt trước đó là Thấp (`aircon28`).
  6. Tắt máy xoá Eco; đổi chế độ xoá Sleep/Eco/Turbo; `fan` giữ mức cuối.
  7. Dry/Auto: remote không cho chọn quạt, khung mã `000`; quạt đã chọn giữ khi quay lại Cool.
  8. Khung TẮT Eco mang mã quạt MỨC CUỐI ở 2 bit thấp byte cuối (0 Auto, 1 Thấp, 2 Vừa, 3 Cao); chọn lại Auto = `00`; đổi nhiệt độc lập; khung BẬT Eco không mang mã quạt (`aircon12-14/18/19/27`).
  9. Clean chạy 20 phút (người dùng đo). Trong Clean: Swing không phản ứng (`aircon26`); Bật nguồn (`B29FC0`) bị bỏ qua, bấm lại (`B27BE0`) máy tắt ngay; Light (`B5F5A5`) máy có phản ứng (`aircon26/27`). ESP32: từ chối mọi lệnh trừ Clean, Tắt nguồn, Light.
- **Giả định còn lại (ghi vào README, chưa có tín hiệu thu riêng):**
  - Hẹn BẬT+TẮT cùng lúc: ESP32 phát đúng như remote, đã thử trên máy thật, ổn (2026-10-23). Ca ĐÓNG.
  - Clean 20 phút do ESP32 tự đếm bằng `millis()`: giới hạn của điều khiển một chiều (không thể có tín hiệu thu), người dùng đã đo thực tế 20 phút. Ca ĐÓNG.
- **Vấn đề còn mở:**
  - Lỗi mDNS `.local` qua đêm: người dùng sẽ kiểm tra sau nếu có thời gian (không quan trọng).
  - Quan sát cũ (không phải lỗi): `platformio.ini` không ghim phiên bản; Swing và Light là lệnh đảo, Web UI không hiển thị; Web UI không tự cập nhật; lệnh đổi trạng thái qua HTTP GET; README nhắc sơ đồ mạch phát IR nhưng zip không có; hẹn giờ đã chạy xong trên máy mà ESP32 vẫn giữ Eco/hẹn giờ trong NVS (giới hạn đã biết, chỉ ghi README).
  - Quy trình chuẩn: kiểm tra bằng chứng -> trình bày thiết kế -> người dùng xác nhận -> mới code/xuất file.

## 8. Quy tắc cập nhật file này

- Cập nhật mục 6 và 7 ngay khi có thay đổi đáng kể, không dồn đến cuối phiên.
- Chỉ ghi thông tin sẽ còn đúng và hữu ích cho phiên sau. Xóa mục đã lỗi thời thay vì chồng thêm.
- Mục 2-5 chỉ sửa khi người dùng yêu cầu.
- Giữ file dưới ~200 dòng để tiết kiệm context mỗi lần đọc.
