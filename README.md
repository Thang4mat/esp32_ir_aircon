# ESP32 điều khiển máy lạnh Midea MAFA-09CDN8 qua IR

Điều khiển máy lạnh (chỉ có IR) bằng ESP32 qua Web UI tự host trên chính ESP32.
Remote gốc của máy phát giao thức **COOLIX** (không phải Midea 48-bit như phiên
bản đầu của dự án đã giả định) — toàn bộ firmware đã được viết lại theo tín hiệu
thu thực tế từ remote.

Hiện chỉ có project cho **ESP32 DevKit V1** (`controller/`). Bản ESP32-C3 SuperMini
**chưa làm** vì chưa có board; các ghi chú về C3 ở cuối file chỉ để tham khảo.

> Bản **v2** (Light/Turbo/Clean, quy luật quạt/Eco/Sleep mới) đã build, nạp và thử trên máy
> lạnh thật, mọi thứ ổn (xác nhận 2026-10-23).
> Bản **v3** (hẹn giờ theo giờ thực: NTP, hẹn "Lúc hh:mm", tự theo dõi hẹn giờ, Web UI tự cập nhật)
> đã build, nạp và thử trên máy lạnh thật bằng ESP32 DevKit V1: NTP đồng bộ đúng giờ và 5 phép thử hẹn giờ
> đều đạt (xác nhận 2026-11-10). Xem mục "Đã kiểm chứng".

## Bước 1 — Xác minh giao thức (nếu đổi remote/máy)

Lắp tạm mạch thu IR theo sơ đồ `so_do_hoan_chinh_mach_thu_ir_rx.png` (module VS1838B: VCC → 3V3, GND → GND,
OUT → GPIO15; gỡ ra sau khi xác minh xong). Thứ tự chân VCC/GND/OUT không cố định giữa các lô module, kiểm tra
chữ in trên board trước khi cấp nguồn.

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

**Giờ thực (NTP)**: sau khi nối Wi-Fi, ESP32 đồng bộ giờ từ `pool.ntp.org` (`NTP_SERVER` trong
`config.h`), múi giờ UTC+7 (`TIMEZONE_OFFSET_SEC`). DNS dùng địa chỉ gateway nên **router phải chuyển
tiếp DNS** (hầu hết đều có). Serial Monitor in `[NTP] Đã đồng bộ giờ: hh:mm (UTC+7)` khi thành công, hoặc
`[NTP] CẢNH BÁO ...` nếu quá 30 giây vẫn chưa có giờ. Chưa có giờ thì **không đặt được hẹn giờ** (HTTP 400,
"Chưa đồng bộ giờ"); các chức năng khác vẫn dùng bình thường.

## Bước 3 — Lắp mạch phát IR & nạp firmware chính

Lắp theo sơ đồ "Mạch phát IR (TX)" (file `so_do_hoan_chinh_mach_phat_ir_tx.png`; R1=1kΩ, Q1=2N2222,
R2=27Ω/2W, 2× LED IR333-C nối tiếp). Mạch giống hệt nhau cho cả 2 board, chỉ khác chân GPIO (xem `config.h`).

```
pio run -t upload
pio device monitor
```

## Bước 4 — Điều khiển

Mở `http://esp32-aircon.local` hoặc trực tiếp bằng IP tĩnh (mặc định `http://192.168.6.200`).
mDNS được khởi động lại mỗi lần Wi-Fi nối lại (log `[mDNS] Sẵn sàng ...` xuất hiện lại trên
Serial Monitor). Nếu `.local` vẫn không vào được sau một thời gian, dùng IP.
Trang tự cập nhật trạng thái mỗi 5 giây khi tab đang hiển thị.

## Cấu trúc thư mục

```
esp32_ir_aircon/
├── README.md
├── so_do_hoan_chinh_mach_phat_ir_tx.png   # sơ đồ mạch phát IR (Bước 3)
├── so_do_hoan_chinh_mach_phat_ir_tx.svg   # file nguồn của sơ đồ trên: sửa SVG rồi xuất lại PNG
├── so_do_hoan_chinh_mach_thu_ir_rx.png    # sơ đồ mạch thu IR, lắp tạm (Bước 1)
├── so_do_hoan_chinh_mach_thu_ir_rx.svg    # file nguồn của sơ đồ trên: sửa SVG rồi xuất lại PNG
├── capture/                         # ESP32 DevKit V1 — thu và in tín hiệu remote (IRrecvDumpV2)
│   ├── platformio.ini
│   └── src/
│       ├── main.cpp                 # thu IR, in giao thức + mã nguồn C++ ra Serial Monitor
│       └── config.h                 # chân GPIO mắt thu (IR_RX_PIN = 15), kích thước bộ đệm
├── test/                            # bộ test chạy trên PC (không cần ESP32)
│   ├── run.sh                       # biên dịch và chạy test
│   ├── test_controller.cpp          # đối chiếu khung IR với tín hiệu thu + kịch bản trạng thái + hẹn giờ + NVS
│   └── stub/                        # giả lập Arduino.h, IRsend.h, Preferences.h (kho NVS dùng chung)
└── controller/                      # ESP32 DevKit V1 — firmware chính
    ├── platformio.ini
    └── src/
        ├── main.cpp                 # Wi-Fi + NTP + HTTP, nối các phần với nhau
        ├── ac_controller.h          # trạng thái, lưu NVS, hẹn giờ, chọn khung IR nào để gửi
        ├── coolix_frames.h          # mã hoá khung IR (thuần tính toán)
        ├── web_ui.h                 # trang Web UI (PROGMEM)
        ├── config.h                 # chân GPIO, hằng số, NTP/múi giờ
        └── secrets.h.example        # mẫu Wi-Fi, copy thành secrets.h rồi điền thật
```

## Tính năng

Power, Mode (Auto/Cool/Dry/Fan), Fan (Auto/Thấp/Vừa/Cao), Nhiệt độ 17–30°C,
Sleep (Bật/Tắt), Eco (Bật/Tắt), Turbo (Bật/Tắt), Clean (Bật/Tắt), Light (nút đảo),
Swing (nút đảo), và **hẹn giờ Bật/Tắt theo hai cách** (đặt được cả hẹn Bật lẫn hẹn Tắt trong cùng một cách):
**Sau X giờ** (30 phút/bước tới 10 giờ, 1 giờ/bước tới 24 giờ) và **Lúc hh:mm** (chính xác tới phút).
Không có Heat (máy lạnh một chiều).

## Hẹn giờ

Hai cách hẹn khác bản chất, loại này loại trừ loại kia (đặt cách nào thì cách kia bị hủy):

| | **Sau X giờ** | **Lúc hh:mm** |
|---|---|---|
| Ai đếm giờ | **Máy lạnh** (ESP32 gửi khung hẹn giờ 48-bit của remote) | **ESP32**, tới giờ mới gửi lệnh Bật/Tắt |
| ESP32 tắt giữa chừng | Máy vẫn chạy đúng hẹn | **Không có lệnh nào được gửi** |
| Độ chính xác | 30 phút (tới 10 giờ), 1 giờ (tới 24 giờ) | Chính xác tới phút |
| Lặp lại | Một lần | Một lần |

**Sau X giờ.** ESP32 lưu `t_start` (giờ thực lúc gửi khung hẹn giờ) vào NVS và chỉ **theo dõi**: đến hạn,
ESP32 cập nhật nguồn cho khớp máy mà **không phát IR**. Nhờ dùng giờ thực nên ESP32 tắt hoặc khởi động lại
giữa chừng vẫn tính đúng. Hẹn **Tắt** chạy xong thì Sleep/Eco/Turbo bị xóa như tắt máy bằng nút Nguồn.
Đang hẹn giờ mà đổi nhiệt độ/quạt/chế độ/Eco/Sleep/Turbo/Swing/Light thì **máy giữ mốc cũ** (không đếm lại),
nên `t_start` giữ nguyên; chỉ bấm "Đặt hẹn giờ" mới đặt mốc mới. Có cả hẹn Bật và hẹn Tắt thì xử lý theo
thứ tự thời gian, cả hai tính từ cùng `t_start`.

**Lúc hh:mm.** Khung hẹn giờ của remote không biểu diễn được "6:25", nên ESP32 tự đếm: lưu mốc thời gian
thực kế tiếp (hôm nay nếu còn, nếu không thì ngày mai; đúng giờ hiện tại coi là ngày mai) vào NVS, tới giờ gửi
đúng khung như nút Nguồn (Bật: khung trạng thái thường với cài đặt đang lưu; Tắt: `B27BE0`). Gửi lệnh dù
ESP32 đang nghĩ máy ở trạng thái nào (sửa lệch do dùng remote gốc). Xong thì chỉ xóa mốc vừa dùng, giữ mốc
còn lại (đặt được "Bật 6:00, Tắt 22:00"). Giờ Bật và giờ Tắt không được trùng nhau.
**ESP32 phải luôn bật tới giờ hẹn.**

- **Hẹn bị lỡ**: nếu ESP32 khởi động lại (hoặc chưa có giờ) mà giờ hẹn đã qua thì lệnh *chưa từng được gửi*.
  ESP32 xóa hẹn đó, **giữ nguyên trạng thái nguồn** (máy có thể vẫn đang chạy) và Web UI hiện cảnh báo, cho tới
  khi bạn thao tác tiếp hoặc ESP32 khởi động lại (cảnh báo chỉ lưu trong RAM).
- **Clean**: bật Clean xóa hẹn "Sau X giờ" (của máy) nhưng giữ hẹn "Lúc hh:mm" (của ESP32): hẹn Tắt vẫn chạy
  (dừng Clean, máy tắt), hẹn Bật bị bỏ qua vì máy không nhận nút Bật khi đang Clean, kèm cảnh báo.
- **Nút Nguồn** (Bật/Tắt thủ công) xóa mọi hẹn giờ, cả hai cách.
- Chưa có giờ NTP: không đặt được hẹn giờ. Hủy hẹn giờ vẫn được.
- Web UI chỉ gửi lệnh khi bấm **Đặt hẹn giờ** (đổi ô chọn không gửi gì), có nút **Hủy hẹn giờ**, và hiện
  "Tắt sau 1 giờ 25 phút (lúc 07:50)". Số liệu do ESP32 tính, không phụ thuộc đồng hồ trình duyệt.

## Cách firmware mô hình hoá remote

Suy ra từ tín hiệu thu thực tế:

- Khung trạng thái thường = "bật máy với mode/fan/temp này"; không có bit nguồn.
  Tắt máy là một khung riêng. Khung trạng thái thường cũng là lệnh **hủy hẹn giờ**.
- Khi máy đang tắt, đổi mode/fan/temp chỉ được lưu, không phát IR (mọi khung
  trạng thái đều ngầm bật máy). Áp dụng khi bấm Bật.
- Hẹn giờ "Sau X giờ" = một khung 48-bit mang theo cả cài đặt hiện tại. Đổi mode/fan/temp
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
- Tắt máy xóa Sleep, Eco, Turbo và mọi hẹn giờ; mức quạt giữ nguyên. Bấm Bật (khung thường) cũng hủy hẹn giờ.
- Swing chỉ gửi lệnh đảo, không lưu trạng thái (ESP32 không đọc được máy).
- Light (`B5F5A5`): lệnh đảo đèn hiển thị, không lưu trạng thái. Phát được cả khi máy
  tắt (máy chỉ bíp) và cả trong lúc Clean (máy đổi đèn).
- Turbo (`B5F5A2`): lệnh đảo, ESP32 giữ cờ và chỉ phát khi cờ đổi. Chỉ ở Cool và khi máy
  đang bật. Bật Turbo tắt Sleep và Eco, không đổi quạt; bật Sleep, Eco hoặc Clean tắt Turbo. Đổi nhiệt độ
  và đặt hẹn giờ giữ Turbo; đổi chế độ hoặc tốc độ quạt, và tắt máy, xóa Turbo.
- Clean (`B5F5AA`): lệnh đảo, ESP32 giữ cờ (không lưu NVS) và chỉ phát khi cờ đổi. Chỉ khi
  máy đang bật. Chạy 20 phút (`AC_CLEAN_DURATION_MS`, ESP32 tự đếm bằng `millis()`) rồi máy
  tự tắt: `update()` trong `loop()` đặt cờ Clean = tắt và nguồn = tắt. Bật Clean xóa hẹn giờ
  "Sau X giờ", Sleep, Eco, Turbo (hẹn "Lúc hh:mm" của ESP32 vẫn giữ). Dừng Clean giữa chừng (Tắt Clean) thì
  máy tắt hẳn. Trong lúc Clean máy chỉ nhận nút Clean, nút Nguồn (tắt) và nút Light; ESP32 từ chối (HTTP 400)
  mọi lệnh khác.

## API (GET, trả về state JSON hoặc HTTP 400 kèm lý do)

| Đường dẫn | Tham số bắt buộc |
|---|---|
| `/api/state` | — |
| `/api/power` | `on` = 0/1 (xóa mọi hẹn giờ) |
| `/api/climate` | `mode` 0-3 (Auto/Cool/Dry/Fan), `fan` 0-3 (Auto/Thấp/Vừa/Cao), `temp` 17-30. Ở Auto/Dry `fan` phải bằng mức đang lưu |
| `/api/timer` | hẹn **Sau X giờ**: `on`, `off` = số phút (0, 30..600 bước 30, 660..1440 bước 60). Cần có giờ NTP nếu đặt hẹn; `0,0` = hủy |
| `/api/schedule` | hẹn **Lúc hh:mm**: `on`, `off` = phút trong ngày 0..1439 (6:25 = 385), `-1` = không hẹn. Cần có giờ NTP; `on` ≠ `off`; `-1,-1` = hủy |
| `/api/cancel` | — (hủy mọi hẹn giờ, cả hai cách) |
| `/api/sleep` | `on` = 0/1 (bật: chỉ khi máy đang chạy ở Auto/Cool) |
| `/api/eco` | `on` = 0/1 (bật: chỉ khi máy đang chạy ở Cool) |
| `/api/turbo` | `on` = 0/1 (bật: chỉ khi máy đang chạy ở Cool) |
| `/api/clean` | `on` = 0/1 (bật: chỉ khi máy đang chạy; tắt: máy tắt hẳn) |
| `/api/light` | — (lệnh đảo; phát được cả khi máy tắt và khi Clean) |
| `/api/swing` | — |

Trong lúc Clean, mọi đường dẫn trên trừ `/api/state`, `/api/clean`, `/api/light` và `/api/power?on=0`
đều trả HTTP 400 (kể cả đặt/hủy hẹn giờ).

State JSON: `power, mode, fan, temp, sleep, eco, turbo, clean, on_timer, off_timer` (như trước) và các trường
hẹn giờ do ESP32 tính sẵn: `kind` (0 không hẹn, 1 Sau X giờ, 2 Lúc hh:mm), `clock` (phút trong ngày hiện tại, -1 =
chưa có giờ), `on_left`/`off_left` (giây còn lại), `on_due`/`off_due` (phút trong ngày của giờ đến hạn, -1 = không
hẹn), `missed_on`/`missed_off` (phút trong ngày của hẹn không được thực hiện, -1 = không có).

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
- kiểm tra hẹn giờ theo giờ thực bằng đồng hồ giả `g_epoch` (truyền vào `AcController::begin()`): mốc kế tiếp
  của "Lúc hh:mm" (hôm nay/ngày mai/qua nửa đêm/qua ranh giới ngày UTC), đến hạn, ESP32 khởi động lại trước/sau hạn,
  hẹn bị lỡ, loại trừ hai loại hẹn giờ, giữ mốc cũ khi đổi cài đặt, chưa có giờ;
- kiểm tra lưu/nạp NVS (stub `Preferences` giữ dữ liệu giữa các lần khởi tạo), kể cả mốc thời gian bất thường.

Kết quả mong đợi: `462/462 dat` (chạy trong container khi phát triển; người dùng chạy xác nhận ở máy mình). Nếu file
`run.sh` mất quyền thực thi (sau khi giải nén zip), chạy `sh test/run.sh`. Chạy lại sau mỗi lần sửa `coolix_frames.h`
hoặc `ac_controller.h`. Bộ test chỉ kiểm tra logic mã hoá, chọn khung và hẹn giờ, KHÔNG kiểm tra thời
gian xung, mạch phát, Wi-Fi, NTP hay Web UI.

## Đã kiểm chứng

- **Remote thật**: hành vi của máy lạnh (Sleep, Eco, hẹn giờ, đổi chế độ/nhiệt/quạt, tắt
  máy, Light/Turbo/Clean) đã được thu và đối chiếu bằng remote thật, qua `capture/`
  (`aircon.txt` ... `aircon28.txt`).
- **Trên máy thật bằng ESP32 DevKit V1**: bản **v2** (Light, Turbo, Clean, quy luật quạt/Eco/Sleep mới, Clean 20 phút,
  hẹn BẬT + TẮT cùng lúc) đã build, nạp và thử: ESP32 phát đúng như remote, mọi thứ ổn (xác nhận 2026-10-23).
- **Bản v3 trên máy thật bằng ESP32 DevKit V1** (người dùng thử 2026-11-10): router phân giải được `pool.ntp.org`,
  giờ UTC+7 đúng. Hẹn Tắt "Sau X giờ" 30 phút: đếm ngược đúng, máy tắt, trang tự đổi sang "Tắt" không cần tải lại.
  Hẹn "Lúc hh:mm": ESP32 gửi lệnh đúng giờ, máy bật/tắt đúng hẹn (không có phản ứng bíp). Rút điện ESP32 qua giờ hẹn
  "Lúc hh:mm": trang báo hẹn bị lỡ, nguồn giữ như trước khi rút. Rút điện qua giờ hẹn "Sau X giờ": máy tự tắt đúng
  hẹn (máy tự đếm), trang hiện "Tắt".
- **IP tĩnh `192.168.6.200`** (nằm trong dải DHCP của router, router không cho đặt ngoài dải): hoạt
  động tốt trên mạng thật (người dùng xác nhận).
- **Quy luật do người dùng thử trực tiếp trên remote và máy lạnh** (không có khung IR riêng):
  Turbo và Clean chỉ ở Cool/khi máy bật; tắt/hủy lẫn nhau; Clean chạy 20 phút; dừng Clean máy tắt
  hẳn; trong Clean Swing không phản ứng, Bật nguồn bị bỏ qua (bấm lại thì máy tắt ngay), Light vẫn
  đổi đèn; máy tắt thì Turbo/Clean không phản ứng còn Light chỉ bíp; Eco rồi Turbo/Sleep và hẹn giờ khi
  Turbo (quan sát bằng LED máy).
- **Người dùng thử trên máy thật 2026-11-05** (dùng cho v3): khi đang hẹn giờ mà đổi nhiệt độ, tốc độ quạt, chế độ,
  Eco, Sleep, Turbo, Swing, Light thì máy **giữ mốc hẹn giờ cũ**, không đếm lại; khi hẹn giờ Tắt chạy xong thì
  Eco/Sleep/Turbo bị xóa như tắt máy bằng nút Nguồn.
- **Turbo rồi Eco** (`aircon28`): quạt Thấp, bật Turbo, bật Eco rồi tắt Eco cho khung tắt Eco `B54AF50A83D0` (mã quạt `00` = Auto),
  nên bật Eco đặt quạt về Auto kể cả khi đang Turbo (xác nhận bằng tín hiệu thu, 2026-10-23).
- **Trên PC**: đối chiếu từng byte khung IR với tín hiệu thu: bảng nhiệt Cool/Auto/Dry 17–30,
  Mode/Fan (kể cả quạt ở Fan, quạt giữ qua Dry/Auto), hẹn BẬT và hẹn TẮT ở các mốc 0.5–24 giờ
  (Cool, Dry, Auto, Fan), hẹn BẬT + TẮT cùng lúc (TẮT 0.5 giờ + BẬT 1 giờ = `B24DA15ED083`),
  máy tắt + hẹn BẬT + đổi nhiệt (`aircon22`), Sleep (kể cả quạt Thấp, đổi quạt, chọn lại Auto, Sleep ở
  Auto), Eco (khung tắt mang mã quạt `83C0`–`83C3`, `83D0`, `83D2`; đổi quạt/nhiệt trong Eco), chuyển
  Eco → Sleep → Eco, Turbo rồi Eco/Sleep, hẹn TẮT khi Turbo, đổi chế độ khi đang Sleep/Eco/hẹn giờ,
  tắt máy, Swing, Light trong Clean, NVS và các ca báo lỗi. Logic hẹn giờ theo giờ thực của v3 cũng kiểm
  trên PC (đồng hồ giả), đã chạy 60 đột biến vào logic mới, đều bị bắt.

## Giả định chưa có tín hiệu thu

Firmware đang làm như dưới đây dựa trên suy luận, chưa có tín hiệu thu hoặc phép thử trên máy thật xác nhận.
Mỗi mục đều có ca test hoặc ghi chú trong code; nếu thấy máy phản ứng khác thì sửa code và test.

- **Thời lượng Clean 20 phút** là con số người dùng đo trên remote; ESP32 tự đếm bằng
  `millis()`, không nhận tín hiệu nào từ máy. Nếu máy chạy lệch vài giây hoặc vài phút thì
  cờ Clean trên ESP32 lệch theo.
- **Đặt lại hẹn giờ "Sau X giờ"** (bấm "Đặt hẹn giờ" khi đã có hẹn, hoặc thêm hẹn Bật vào hẹn Tắt đang có): ESP32
  coi cả hai mốc được máy đếm lại từ lúc gửi khung mới (`t_start` = bây giờ). Chưa có phép thử riêng.
- **Hẹn BẬT và hẹn TẮT cùng một mốc phút** (chỉ có ở "Sau X giờ"): ESP32 xử lý Bật rồi Tắt (máy tắt). Chưa biết
  máy thật làm gì. ("Lúc hh:mm" thì không cho trùng giờ.)
- **Hẹn BẬT "Sau X giờ" chạy xong khi máy đang chạy** (ESP32 nghĩ máy chạy): giữ nguyên Eco/Sleep/Turbo (chỉ
  hẹn Tắt mới xóa chúng).
- **Hẹn "Lúc hh:mm" gửi đúng khung của nút Nguồn** (Bật = khung trạng thái thường, Tắt = `B27BE0`), kể cả khi
  ESP32 nghĩ máy đang ở đúng trạng thái đó. Với Bật khi ESP32 nghĩ máy đang chạy và đang bật Eco/Turbo, chưa biết máy
  có giữ các cờ đó không.

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
  đó hãy dùng remote hoặc đồng bộ lại bằng nút Nguồn.
- Cài đặt được lưu vào flash (NVS) mỗi khi đổi. Mất nguồn/khởi động lại không mất
  cài đặt, nhưng ESP32 **không tự phát lại lệnh IR khi khởi động**.
- Hẹn giờ "Sau X giờ" do máy đếm, ESP32 chỉ theo dõi bằng giờ thực nên cần có giờ NTP để biết hẹn giờ đã chạy
  xong. Nếu ESP32 khởi động mà không có Internet, trạng thái hẹn giờ trên trang được giữ nguyên cho tới khi có giờ.
- Hẹn giờ "Lúc hh:mm" cần ESP32 **luôn bật** tới giờ hẹn; nếu bị tắt qua giờ hẹn thì lệnh không được gửi (xem mục Hẹn giờ).
- NVS của bản firmware cũ (v2) mà đang có hẹn giờ "Sau X giờ" sẽ bị coi là không hợp lệ (thiếu `t_start`) và
  cài đặt quay về mặc định ở lần khởi động đầu tiên sau khi nạp v3.

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
