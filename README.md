# ESP32 điều khiển máy lạnh Midea MAFA-09CDN8 qua IR

Điều khiển máy lạnh (chỉ có IR) bằng ESP32 qua Web UI tự host trên chính ESP32.
Remote gốc của máy phát giao thức **COOLIX** (không phải Midea 48-bit như phiên
bản đầu của dự án đã giả định) — toàn bộ firmware đã được viết lại theo tín hiệu
thu thực tế từ remote.

Hiện chỉ có project cho **ESP32 DevKit V1** (`controller/`). Bản ESP32-C3 SuperMini
**chưa làm** vì chưa có board; các ghi chú về C3 ở cuối file chỉ để tham khảo.

> Firmware `controller/` ở bản trước (chưa có Light/Turbo/Clean) đã được **build bằng
> PlatformIO, nạp lên ESP32 DevKit V1 và thử trên máy lạnh thật**: ESP32 phát tín hiệu đúng
> như remote gốc, chưa thấy điều gì bất thường. Bản hiện tại (có Light/Turbo/Clean và các quy
> luật quạt/Eco/Sleep mới) **đã build, nạp và thử trên máy thật, mọi thứ ổn** (xác nhận 2026-10-23). Chi tiết xem mục "Đã kiểm chứng".

## Bước 1 — Xác minh giao thức (nếu đổi remote/máy)

```
cd capture
pio run -t upload
pio device monitor
```

Hướng remote vào mắt thu, bấm từng nút. Kết quả mong đợi: `COOLIX` (24 bits) cho
Power/Mode/Fan/Temp/Swing/Light/Turbo/Clean, `COOLIX48` (48 bits) cho Eco và Hẹn giờ. Nút Sleep có
thể hiện thành `BOSCH144`/"Quiet" — do 3 khung IR dính nhau trong một lần bắt,
không phải lỗi giao thức. Nếu ra protocol khác hoặc UNKNOWN → dừng lại, không đoán mò.

## Bước 2 — Cấu hình Wi-Fi

```
cd controller
cp src/secrets.h.example src/secrets.h
```

Sửa `src/secrets.h`, điền đúng tên và mật khẩu Wi-Fi nhà. Quá 30 giây không vào
được Wi-Fi, ESP32 báo lỗi trên Serial Monitor (kèm mã lý do) rồi tự khởi động lại.

ESP32 luôn dùng IP tĩnh, cấu hình trong `src/config.h` (`WIFI_STATIC_IP`, `WIFI_GATEWAY`,
`WIFI_SUBNET`). Mặc định `192.168.6.200`, gateway `192.168.6.1`, subnet `255.255.255.0`.
Router của dự án không cho đặt IP ngoài dải DHCP, nên `192.168.6.200` nằm trong dải; IP này hoạt động
tốt. Khi đổi mạng, đổi cho khớp mạng mới và chọn IP không trùng thiết bị khác.
Không đặt được IP tĩnh thì ESP32 báo lỗi trên Serial Monitor rồi tự khởi động lại.

## Bước 3 — Lắp mạch phát IR & nạp firmware chính

Lắp theo sơ đồ "Mạch phát IR (TX)" (R1=1kΩ, Q1=2N2222, R2=27Ω/2W, 2× LED IR333-C
nối tiếp). Mạch giống hệt nhau cho cả 2 board, chỉ khác chân GPIO (xem `config.h`).

```
pio run -t upload
pio device monitor
```

## Bước 4 — Điều khiển

Mở `http://esp32-aircon.local` hoặc trực tiếp bằng IP tĩnh (mặc định `http://192.168.6.200`).
mDNS được khởi động lại mỗi lần Wi-Fi nối lại (log `[mDNS] Sẵn sàng ...` xuất hiện lại trên
Serial Monitor). Nếu `.local` vẫn không vào được sau một thời gian, dùng IP.

## Cấu trúc thư mục

```
esp32_ir_aircon/
├── README.md
├── capture/                         # ESP32 DevKit V1 — thu và in tín hiệu remote (IRrecvDumpV2)
│   ├── platformio.ini
│   └── src/
│       ├── main.cpp                 # thu IR, in giao thức + mã nguồn C++ ra Serial Monitor
│       └── config.h                 # chân GPIO mắt thu (IR_RX_PIN = 15), kích thước bộ đệm
├── test/                            # bộ test chạy trên PC (không cần ESP32)
│   ├── run.sh                       # biên dịch và chạy test
│   ├── test_controller.cpp          # đối chiếu khung IR với tín hiệu thu + kịch bản trạng thái + NVS
│   └── stub/                        # giả lập Arduino.h, IRsend.h, Preferences.h (kho NVS dùng chung)
└── controller/                      # ESP32 DevKit V1 — firmware chính
    ├── platformio.ini
    └── src/
        ├── main.cpp                 # Wi-Fi + HTTP, nối các phần với nhau
        ├── ac_controller.h          # trạng thái, lưu NVS, chọn khung IR nào để gửi
        ├── coolix_frames.h          # mã hoá khung IR (thuần tính toán)
        ├── web_ui.h                 # trang Web UI (PROGMEM)
        ├── config.h                 # chân GPIO, hằng số
        └── secrets.h.example        # mẫu Wi-Fi, copy thành secrets.h rồi điền thật
```

## Tính năng

Power, Mode (Auto/Cool/Dry/Fan), Fan (Auto/Thấp/Vừa/Cao), Nhiệt độ 17–30°C,
Sleep (Bật/Tắt), Eco (Bật/Tắt), Turbo (Bật/Tắt), Clean (Bật/Tắt), Light (nút đảo),
Swing (nút đảo), Hẹn giờ bật và hẹn giờ tắt (đặt được cùng lúc; 30 phút/bước tới
10 giờ, 1 giờ/bước tới 24 giờ). Không có Heat (máy lạnh một chiều).

## Cách firmware mô hình hoá remote

Suy ra từ tín hiệu thu thực tế:

- Khung trạng thái thường = "bật máy với mode/fan/temp này"; không có bit nguồn.
  Tắt máy là một khung riêng. Khung trạng thái thường cũng là lệnh **hủy hẹn giờ**.
- Khi máy đang tắt, đổi mode/fan/temp chỉ được lưu, không phát IR (mọi khung
  trạng thái đều ngầm bật máy). Áp dụng khi bấm Bật.
- Hẹn giờ = một khung 48-bit mang theo cả cài đặt hiện tại. Đổi mode/fan/temp
  khi đang hẹn giờ gửi lại khung hẹn giờ với cài đặt mới (giữ hẹn giờ), kể cả khi máy đang
  tắt mà đang hẹn BẬT. Một khung hẹn TẮT mang theo cả hẹn BẬT.
- Sleep: nếu bật, khung `B2E003` đi trước mọi khung remote gửi (khung thường
  hoặc khung hẹn giờ). Bật/Tắt Sleep không ảnh hưởng hẹn giờ. Chỉ có ở Auto và Cool.
- Eco chỉ có ở Cool. Khung BẬT Eco không mang mã quạt; khung TẮT Eco mang mã quạt hiện tại ở
  2 bit thấp của byte cuối (0 Auto, 1 Thấp, 2 Vừa, 3 Cao), ví dụ `B54AF50A83C2` = tắt Eco khi quạt Vừa, 25°C.
- Sleep và Eco loại trừ nhau: bật cái này thì máy tự tắt cái kia.
- **Quạt**: bật Eco hoặc Sleep đặt quạt về Auto; tắt Eco/Sleep giữ mức quạt hiện tại.
  Đổi tốc độ quạt tắt Sleep và Turbo, **không** tắt Eco, giữ hẹn giờ. Chọn lại đúng mức quạt đang lưu
  không tính là đổi (Web UI luôn gửi kèm quạt hiện tại). Đổi nhiệt độ giữ Sleep, Eco, Turbo, hẹn giờ.
- Auto và Dry: remote không cho chọn quạt, khung luôn mang mã quạt `000`. Mức quạt đã chọn được giữ
  để dùng lại khi về Cool/Fan (kể cả qua tắt/bật máy). ESP32 từ chối đổi quạt ở hai chế độ này (HTTP 400)
  và Web UI khóa nút quạt. Bảng mã nhiệt độ của Auto và Dry giống Cool.
- Đổi **chế độ** tự tắt Sleep, Eco, Turbo (phải bật lại thủ công), giữ hẹn giờ.
- Tắt máy xóa Sleep, Eco, Turbo và hẹn giờ; mức quạt giữ nguyên. Bấm Bật (khung thường) cũng hủy hẹn giờ.
- Swing chỉ gửi lệnh đảo, không lưu trạng thái (ESP32 không đọc được máy).
- Light (`B5F5A5`): lệnh đảo đèn hiển thị, không lưu trạng thái. Phát được cả khi máy
  tắt (máy chỉ bíp) và cả trong lúc Clean (máy đổi đèn).
- Turbo (`B5F5A2`): lệnh đảo, ESP32 giữ cờ và chỉ phát khi cờ đổi. Chỉ ở Cool và khi máy
  đang bật. Bật Turbo tắt Sleep và Eco, không đổi quạt; bật Sleep, Eco hoặc Clean tắt Turbo. Đổi nhiệt độ
  và đặt hẹn giờ giữ Turbo; đổi chế độ hoặc tốc độ quạt, và tắt máy, xóa Turbo.
- Clean (`B5F5AA`): lệnh đảo, ESP32 giữ cờ (không lưu NVS) và chỉ phát khi cờ đổi. Chỉ khi
  máy đang bật. Chạy 20 phút (`AC_CLEAN_DURATION_MS`, ESP32 tự đếm bằng `millis()`) rồi máy
  tự tắt: `update()` trong `loop()` đặt cờ Clean = tắt và nguồn = tắt. Bật Clean xóa hẹn
  giờ BẬT/TẮT, Sleep, Eco, Turbo. Dừng Clean giữa chừng (Tắt Clean) thì máy tắt hẳn. Trong
  lúc Clean máy chỉ nhận nút Clean, nút Nguồn (tắt) và nút Light; ESP32 từ chối (HTTP 400) mọi lệnh khác.

## API (GET, trả về state JSON hoặc HTTP 400 kèm lý do)

| Đường dẫn | Tham số bắt buộc |
|---|---|
| `/api/state` | — |
| `/api/power` | `on` = 0/1 |
| `/api/climate` | `mode` 0-3 (Auto/Cool/Dry/Fan), `fan` 0-3 (Auto/Thấp/Vừa/Cao), `temp` 17-30. Ở Auto/Dry `fan` phải bằng mức đang lưu |
| `/api/timer` | `on`, `off` = số phút (0, 30..600 bước 30, 660..1440 bước 60) |
| `/api/sleep` | `on` = 0/1 (bật: chỉ khi máy đang chạy ở Auto/Cool) |
| `/api/eco` | `on` = 0/1 (bật: chỉ khi máy đang chạy ở Cool) |
| `/api/turbo` | `on` = 0/1 (bật: chỉ khi máy đang chạy ở Cool) |
| `/api/clean` | `on` = 0/1 (bật: chỉ khi máy đang chạy; tắt: máy tắt hẳn) |
| `/api/light` | — (lệnh đảo; phát được cả khi máy tắt và khi Clean) |
| `/api/swing` | — |

Trong lúc Clean, mọi đường dẫn trên trừ `/api/state`, `/api/clean`, `/api/light` và `/api/power?on=0`
đều trả HTTP 400. State JSON có thêm `turbo` và `clean`.

Tham số thiếu, không phải số hoặc ngoài khoảng đều bị từ chối, không tự sửa.

## Chạy test trên PC

Cần `g++` (C++17). Từ thư mục gốc của dự án:

```
./test/run.sh
```

Bộ test không cần ESP32 hay PlatformIO. Nó gồm:

- đối chiếu từng byte mã khung trong `coolix_frames.h` với tín hiệu thu thật của remote;
- kiểm tra logic chọn khung IR trong `ac_controller.h` (Sleep, Eco, Turbo, Clean, Light,
  quạt, hẹn giờ, đổi chế độ, tắt máy, Swing, ca lỗi); Clean hết giờ được kiểm bằng đồng hồ giả
  `g_millis` trong `test/stub/Arduino.h`, kể cả khi `millis()` tràn số;
- kiểm tra lưu/nạp NVS (stub `Preferences` giữ dữ liệu giữa các lần khởi tạo).

Kết quả mong đợi: `321/321 dat` (người dùng đã chạy xác nhận). Nếu file `run.sh` mất quyền thực thi
(sau khi giải nén zip), chạy `sh test/run.sh`. Chạy lại sau mỗi lần sửa `coolix_frames.h` hoặc
`ac_controller.h`. Bộ test chỉ kiểm tra logic mã hoá và chọn khung, KHÔNG kiểm tra thời
gian xung, mạch phát hay Wi-Fi.

## Đã kiểm chứng

- **Remote thật**: hành vi của máy lạnh (Sleep, Eco, hẹn giờ, đổi chế độ/nhiệt/quạt, tắt
  máy, Light/Turbo/Clean) đã được thu và đối chiếu bằng remote thật, qua `capture/`
  (`aircon.txt` ... `aircon27.txt`).
- **Trên máy thật bằng ESP32 DevKit V1**: bản firmware trước khi thêm Light/Turbo/Clean
  đã build, nạp và thử. ESP32 phát tín hiệu đến máy lạnh giống remote, chưa thấy điều gì
  bất thường. **Bản v2** (Light, Turbo, Clean, quy luật quạt/Eco/Sleep mới, Clean 20 phút, hẹn BẬT + TẮT
  cùng lúc) cũng đã build, nạp và thử trên máy thật: ESP32 phát đúng như remote, mọi thứ ổn (xác nhận 2026-10-23).
- **IP tĩnh `192.168.6.200`** (nằm trong dải DHCP của router, router không cho đặt ngoài dải): hoạt
  động tốt trên mạng thật (người dùng xác nhận).
- **Quy luật do người dùng thử trực tiếp trên remote và máy lạnh** (không có khung IR riêng):
  Turbo và Clean chỉ ở Cool/khi máy bật; tắt/hủy lẫn nhau; Clean chạy 20 phút; dừng Clean máy tắt
  hẳn; trong Clean Swing không phản ứng, Bật nguồn bị bỏ qua (bấm lại thì máy tắt ngay), Light vẫn
  đổi đèn; máy tắt thì Turbo/Clean không phản ứng còn Light chỉ bíp; Eco rồi Turbo/Sleep và hẹn giờ khi
  Turbo (quan sát bằng LED máy).
- **Turbo rồi Eco** (`aircon28`): quạt Thấp, bật Turbo, bật Eco rồi tắt Eco cho khung tắt Eco `B54AF50A83D0` (mã quạt `00` = Auto),
  nên bật Eco đặt quạt về Auto kể cả khi đang Turbo (xác nhận bằng tín hiệu thu, 2026-10-23).
- **Trên PC**: đối chiếu từng byte khung IR với tín hiệu thu: bảng nhiệt Cool/Auto/Dry 17–30,
  Mode/Fan (kể cả quạt ở Fan, quạt giữ qua Dry/Auto), hẹn BẬT và hẹn TẮT ở các mốc 0.5–24 giờ
  (Cool, Dry, Auto, Fan), hẹn BẬT + TẮT cùng lúc (TẮT 0.5 giờ + BẬT 1 giờ = `B24DA15ED083`),
  máy tắt + hẹn BẬT + đổi nhiệt (`aircon22`), Sleep (kể cả quạt Thấp, đổi quạt, chọn lại Auto, Sleep ở
  Auto), Eco (khung tắt mang mã quạt `83C0`–`83C3`, `83D0`, `83D2`; đổi quạt/nhiệt trong Eco), chuyển
  Eco → Sleep → Eco, Turbo rồi Eco/Sleep, hẹn TẮT khi Turbo, đổi chế độ khi đang Sleep/Eco/hẹn giờ,
  tắt máy, Swing, Light trong Clean, NVS và các ca báo lỗi.

## Giả định chưa có tín hiệu thu

Firmware đang làm như dưới đây dựa trên suy luận, chưa có tín hiệu thu từ remote xác nhận.
Mỗi mục đều có ca test hoặc ghi chú trong code; nếu thu được tín hiệu khác thì sửa code và test.

- **Hẹn BẬT + TẮT cùng lúc**: đã thử trên máy thật qua ESP32, hoạt động như remote (xác nhận 2026-10-23).
  Không còn là giả định.
- **Thời lượng Clean 20 phút** là con số người dùng đo trên remote; ESP32 tự đếm bằng
  `millis()`, không nhận tín hiệu nào từ máy. Nếu máy chạy lệch vài giây hoặc vài phút thì
  cờ Clean trên ESP32 lệch theo.

Chưa kiểm chứng: bản ESP32-C3 SuperMini (chưa có board, chưa viết code).

Nếu máy phản ứng khác remote, thu tín hiệu mới bằng `capture/` rồi sửa đúng chỗ trong
`coolix_frames.h` / `ac_controller.h`.

## Giới hạn của điều khiển IR một chiều

- ESP32 không đọc được trạng thái thật của máy. Nếu dùng xen kẽ remote gốc, trạng
  thái trên Web UI (nhất là nguồn, Sleep, Eco, hẹn giờ) có thể lệch máy thật.
- Nếu ESP32 nghĩ máy đang tắt nhưng máy đang bật (hoặc ngược lại), lệnh tiếp theo
  có thể cho kết quả ngược ý muốn.
- Cờ Clean không lưu NVS. Nếu ESP32 mất điện hoặc khởi động lại giữa lúc Clean, cờ Clean
  mất và trạng thái nguồn trên ESP32 có thể lệch máy (máy vẫn đang Clean rồi tự tắt). Khi
  đó hãy dùng remote hoặc đồng bộ lại bằng nút Nguồn. Web UI không tự cập nhật: sau khi
  Clean hết giờ, tải lại trang để thấy máy đã tắt.
- Cài đặt được lưu vào flash (NVS) mỗi khi đổi. Mất nguồn/khởi động lại không mất
  cài đặt, nhưng ESP32 **không tự phát lại lệnh IR khi khởi động**. Hẹn giờ lưu
  trong NVS không tự hết hạn: sau khi hẹn giờ đã chạy xong trên máy, Web UI vẫn
  hiện hẹn giờ đó cho tới khi bạn đổi nguồn hoặc đặt lại.

## Lưu ý riêng cho ESP32-C3 SuperMini

- **Board PlatformIO dùng `esp32-c3-devkitm-1`** — chưa có board definition riêng
  tên "SuperMini", đây là lựa chọn tương thích chuẩn cho các board clone.
- **Bắt buộc có `-DARDUINO_USB_MODE=1` và `-DARDUINO_USB_CDC_ON_BOOT=1`** trong
  `build_flags` — board không có chip USB-Serial riêng, thiếu cờ thì Serial Monitor
  không hoạt động bình thường.
- **Chân GPIO khác hẳn DevKit V1**: tránh GPIO2/8/9 (strapping, GPIO8 còn nối LED
  xanh trên nhiều bản). Đã chọn GPIO4 (phát IR) và GPIO5 (thu IR, chỉ dùng ở `capture/`).
- Nếu không vào được chế độ nạp lần đầu: giữ nút **BOOT**, nhấn rồi thả **RESET**,
  thả **BOOT**.
- Nếu Serial báo `reason=2` (hoặc 202) lặp lại ở **mọi** lần kết nối dù mật khẩu
  đúng: nhiều bản clone C3 SuperMini bị lỗi anten, cách khắc phục đã biết là giảm
  công suất phát bằng `WiFi.setTxPower(WIFI_POWER_8_5dBm);` ngay sau `WiFi.begin()`
  (đổi lại phạm vi sóng còn khoảng 10 m). Chỉ thêm khi gặp đúng triệu chứng này.
- Bản C3 chưa được viết (chưa có board).

## Nguồn cấp

USB-C liên tục, không qua công tắc.
