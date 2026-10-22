#pragma once

// Chan GPIO noi transistor dieu khien 2 LED IR (mach phat, xem so do "Mach phat IR (TX)")
#define IR_LED_PIN 4

// Ten mDNS -> truy cap Web UI qua http://esp32-aircon.local (khong can nho IP)
#define MDNS_HOSTNAME "esp32-aircon"
#define HTTP_PORT 80

// Giới hạn nhiệt độ của máy Midea MAFA-09CDN8 (độ C)
#define AC_TEMP_MIN 17
#define AC_TEMP_MAX 30
#define AC_TEMP_DEFAULT 25

<<<<<<< HEAD
// Clean (tự làm sạch dàn lạnh) chạy 20 phút rồi máy tự tắt (người dùng đo trên remote).
// Máy không báo gì về ESP32 nên ESP32 tự đếm thời gian này bằng millis().
#define AC_CLEAN_DURATION_MS 1200000UL
=======
// Clean (tự làm sạch dàn lạnh) chạy 30 phút rồi máy tự tắt. Máy không báo gì về ESP32
// nên ESP32 tự đếm thời gian này bằng millis().
#define AC_CLEAN_DURATION_MS 1800000UL
>>>>>>> 590e5a8af47a4000555b4d7ece217e777219d121

// Quá 30 giây không vào được Wi-Fi thì báo lỗi rồi khởi động lại (không chờ vô hạn).
#define WIFI_CONNECT_TIMEOUT_MS 30000UL

// IP tĩnh (luôn dùng, không có DHCP). Mỗi macro là 4 số cách nhau bằng dấu phẩy.
<<<<<<< HEAD
// Router của dự án không cho đặt IP ngoài dải DHCP; 192.168.6.200 nằm trong dải và chạy tốt.
// Khi đổi mạng, chọn IP không trùng thiết bị khác.
=======
// WIFI_STATIC_IP phải nằm NGOÀI dải DHCP của router để không trùng thiết bị khác.
>>>>>>> 590e5a8af47a4000555b4d7ece217e777219d121
// DNS dùng chung địa chỉ gateway (firmware không phân giải tên miền ngoài).
#define WIFI_STATIC_IP 192, 168, 6, 200
#define WIFI_GATEWAY 192, 168, 6, 1
#define WIFI_SUBNET 255, 255, 255, 0

// ---------------------------------------------------------------------------
// GHI CHU PHAN CUNG MACH PHAT IR (tham khao khi doi linh kien, khong dung trong
// code — cac gia tri nay chi anh huong dong dien thuc te qua LED, khong lien
// quan toi chan GPIO hay logic phan mem o tren).
//
// Transistor da dung: 2N2222 (NPN, TO-92).
//
// R1 (dien tro base, giua GPIO4 va chan B cua transistor):
//   I_b = (V_GPIO - V_BE(sat)) / R1
//   forced_beta = I_c_mong_muon / I_b   (nen thap hon nhieu h_FE that de dam
//                 bao ban hoa chac chan du linh kien lech thong so)
//   Vi du: V_GPIO=3.3V, V_BE(sat)=0.7V, R1=1kOhm
//       -> I_b = (3.3-0.7)/1000 = 2.6mA
//       -> forced_beta = 80mA/2.6mA ~ 31, thap hon nhieu h_FE min cua 2N2222
//          (~100) -> transistor ban hoa chac chan.
//   Da dung: R1 = 1kOhm, 1/4W hoac 2W deu duoc (cong suat thuc te ~7uW,
//   du thua rat nhieu du chon loai nao).
//
// R2 (dien tro gioi han dong, giua collector va 2 LED IR mac noi tiep):
//   I_LED = (V_VIN - 2*Vf_LED - V_CE(sat)) / R2
//   LED da dung: 2x IR333-C (Everlight, 940nm, dinh muc dong lien tuc toi da
//   100mA — datasheet: Vf typ 1.2-1.5V @ 20mA, 1.4-1.8V @ 100mA).
//   Target ~80mA (duoi tran 100mA de co margin), kiem tra ca 2 dau dai Vf:
//       Vf typ (~1.4-1.5V/LED): R2=27Ohm -> I_LED=(5-3.0-0.2)/27 ~ 67mA
//       Vf thap nhat (~1.2V/LED, truong hop xau nhat):
//           R2=27Ohm -> I_LED=(5-2.4-0.2)/27 ~ 89mA  (van duoi tran 100mA)
//           R2=22Ohm -> I_LED=(5-2.4-0.2)/22 ~ 109mA (VUOT TRAN, KHONG dung)
//   Cong thuc tong quat khi doi sang LED IR khac (Vf hoac dinh muc dong khac):
//       R2 = (V_VIN - 2*Vf_LED - V_CE(sat)) / I_LED_mong_muon
//   -> chon R2 the nao de I_LED o truong hop Vf THAP NHAT van duoi dinh muc,
//      khong chi tinh theo Vf typical.
//   Da dung: R2 = 27Ohm, 2W (cong suat thuc te ~0.12-0.21W tuy Vf thuc te,
//   van du margin so voi 2W).
// ---------------------------------------------------------------------------
