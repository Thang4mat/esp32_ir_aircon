#pragma once
// =============================================================================
//  coolix_frames.h — mã hoá các khung IR của remote máy lạnh (giao thức COOLIX)
//
//  Thuần tính toán, không đụng phần cứng. Mọi quy luật dưới đây được suy ra từ
//  tín hiệu thu thực tế của remote (aircon*.txt) và đã đối chiếu từng byte.
//
//  Mọi khung trên dây đều là 48 bit = 6 byte. Có 2 dạng:
//
//  1) Khung "lệnh 24-bit": mỗi byte đi kèm byte đảo của nó.
//       0xB2BFD0  ->  B2 4D BF 40 D0 2F
//
//  2) Khung "48-bit thô" (Eco, hẹn giờ): 6 byte tự do, KHÔNG phải cặp byte/đảo.
//
//  Light, Turbo, Clean là lệnh 24-bit cố định dạng 1) (B5F5xx), đều là lệnh đảo.
//
//  Khung trạng thái 24-bit = B2 | b1 | b2
//     b1 = (mã quạt << 5) | 0x1F      (0x1F = trường sensor temp "bỏ qua")
//     b2 = (mã nhiệt độ << 4) | (mã chế độ << 2)
<<<<<<< HEAD
//  Ở Auto và Dry remote không cho chọn quạt: mã quạt luôn là 000.
=======
>>>>>>> 590e5a8af47a4000555b4d7ece217e777219d121
// =============================================================================

#include <stdint.h>

namespace coolix {

// Lệnh 24-bit cố định (remote gửi y hệt).
constexpr uint32_t kOffCode = 0xB27BE0;    // Tắt máy
constexpr uint32_t kSwingCode = 0xB26BE0;  // Đảo cánh gió (toggle)
constexpr uint32_t kSleepCode = 0xB2E003;  // Báo "Sleep" — đứng trước khung trạng thái
constexpr uint32_t kLightCode = 0xB5F5A5;  // Bật/tắt đèn hiển thị trên dàn lạnh (đảo)
constexpr uint32_t kTurboCode = 0xB5F5A2;  // Turbo (đảo)
constexpr uint32_t kCleanCode = 0xB5F5AA;  // Clean (đảo)

constexpr uint8_t kTempMin = 17;
constexpr uint8_t kTempMax = 30;

// Mã nhiệt độ 4-bit theo thứ tự 17..30°C (khớp remote thật và kCoolixTempMap).
<<<<<<< HEAD
// Cùng bảng cho Cool, Auto, Dry (aircon2, aircon23).
=======
>>>>>>> 590e5a8af47a4000555b4d7ece217e777219d121
constexpr uint8_t kTempCode[14] = {0x0, 0x1, 0x3, 0x2, 0x6, 0x7, 0x5,
                                   0x4, 0xC, 0xD, 0x9, 0x8, 0xA, 0xB};

// Chỉ số dùng chung với Web UI.
constexpr uint8_t kModeAuto = 0;
constexpr uint8_t kModeCool = 1;
constexpr uint8_t kModeDry = 2;
constexpr uint8_t kModeFan = 3;

constexpr uint8_t kFanAuto = 0;
constexpr uint8_t kFanLow = 1;
constexpr uint8_t kFanMed = 2;
constexpr uint8_t kFanHigh = 3;

// Hẹn giờ: remote chỉ cho 30 phút/bước tới 10 giờ, rồi 1 giờ/bước tới 24 giờ.
// 0 = không hẹn.
inline bool validTimerMin(uint16_t m) {
  if (m == 0) return true;
  if (m <= 600) return m % 30 == 0;
  return m >= 660 && m <= 1440 && m % 60 == 0;
}

// Khung trạng thái 24-bit (mode/fan/temp luôn hợp lệ — người gọi đã kiểm tra).
inline uint32_t state(uint8_t mode, uint8_t fan, uint8_t temp) {
  uint8_t mode_bits;
  switch (mode) {
    case kModeAuto: mode_bits = 0b10; break;
    case kModeCool: mode_bits = 0b00; break;
    default: mode_bits = 0b01; break;  // Dry, và Fan (Fan = Dry + mã nhiệt độ 0xE)
  }
  const uint8_t temp_code = (mode == kModeFan) ? 0xE : kTempCode[temp - kTempMin];

<<<<<<< HEAD
  // Auto/Dry: remote không cho chọn quạt, khung luôn mã 000 dù mức quạt đã lưu là gì
  // (aircon13). Cool/Fan: Thấp/Vừa/Cao có mã riêng, "Auto" là 101.
  uint8_t fan_bits;
  if (mode == kModeAuto || mode == kModeDry) {
    fan_bits = 0b000;
  } else {
    switch (fan) {
      case kFanLow: fan_bits = 0b100; break;
      case kFanMed: fan_bits = 0b010; break;
      case kFanHigh: fan_bits = 0b001; break;
      default: fan_bits = 0b101; break;
    }
=======
  // Quạt "Auto": Dry/Auto dùng mã 000, Cool/Fan dùng mã 101.
  uint8_t fan_bits;
  switch (fan) {
    case kFanLow: fan_bits = 0b100; break;
    case kFanMed: fan_bits = 0b010; break;
    case kFanHigh: fan_bits = 0b001; break;
    default:
      fan_bits = (mode == kModeAuto || mode == kModeDry) ? 0b000 : 0b101;
      break;
>>>>>>> 590e5a8af47a4000555b4d7ece217e777219d121
  }

  const uint8_t b1 = static_cast<uint8_t>((fan_bits << 5) | 0x1F);
  const uint8_t b2 = static_cast<uint8_t>((temp_code << 4) | (mode_bits << 2));
  return 0xB20000UL | (static_cast<uint32_t>(b1) << 8) | b2;
}

// Lệnh 24-bit -> khung 48-bit trên dây (mỗi byte kèm byte đảo).
inline uint64_t wire(uint32_t code24) {
  uint64_t out = 0;
  for (int shift = 16; shift >= 0; shift -= 8) {
    const uint8_t b = static_cast<uint8_t>(code24 >> shift);
    out = (out << 16) | (static_cast<uint64_t>(b) << 8) | static_cast<uint8_t>(b ^ 0xFF);
  }
  return out;
}

// Giá trị hẹn giờ M = 2n-1, n = số nửa giờ (30 phút -> 1 ... 24 giờ -> 95).
inline uint8_t timerValue(uint16_t minutes) {
  return static_cast<uint8_t>(2 * (minutes / 30) - 1);
}

inline uint64_t frame48(const uint8_t (&b)[6]) {
  uint64_t out = 0;
  for (int i = 0; i < 6; i++) out = (out << 8) | b[i];
  return out;
}

// Hẹn giờ BẬT: B2 4D b1 ~b1 (b2|3) (0x80+M). Mang theo cài đặt hiện tại.
inline uint64_t timerOn(uint32_t st, uint16_t on_min) {
  const uint8_t b1 = static_cast<uint8_t>(st >> 8);
  const uint8_t b2 = static_cast<uint8_t>(st);
  const uint8_t f[6] = {0xB2, 0x4D, b1, static_cast<uint8_t>(b1 ^ 0xFF),
                        static_cast<uint8_t>(b2 | 0x03),
                        static_cast<uint8_t>(0x80 + timerValue(on_min))};
  return frame48(f);
}

// Hẹn giờ TẮT: B2 4D X ~X (b2|(M>>5)) Z
//   X = (b1 & 0xE0) | (M & 31): giá trị hẹn giờ nằm trong trường sensor temp.
//   Z = 0xFF nếu không có hẹn giờ BẬT, ngược lại 0x80 + M_bật (remote gộp
//   cả hai hẹn giờ vào một khung).
inline uint64_t timerOff(uint32_t st, uint16_t off_min, uint16_t on_min) {
  const uint8_t b1 = static_cast<uint8_t>(st >> 8);
  const uint8_t b2 = static_cast<uint8_t>(st);
  const uint8_t m = timerValue(off_min);
  const uint8_t x = static_cast<uint8_t>((b1 & 0xE0) | (m & 0x1F));
  const uint8_t z = on_min == 0 ? 0xFF : static_cast<uint8_t>(0x80 + timerValue(on_min));
  const uint8_t f[6] = {0xB2, 0x4D, x, static_cast<uint8_t>(x ^ 0xFF),
                        static_cast<uint8_t>(b2 | (m >> 5)), z};
  return frame48(f);
}

<<<<<<< HEAD
// Eco: B5 4A F5 0A (0x82 bật | 0x83 tắt) b2'. b2 là byte nhiệt độ/chế độ hiện tại.
//   Khung BẬT: b2' = b2 (không mang mã quạt).
//   Khung TẮT: b2' = b2 | fan, fan = mức quạt hiện tại ở 2 bit thấp
//              (0 Auto, 1 Thấp, 2 Vừa, 3 Cao; khớp kFan*), (aircon19, aircon27).
inline uint64_t eco(uint32_t st, bool on, uint8_t fan) {
  const uint8_t b2 = static_cast<uint8_t>(st);
  const uint8_t last = on ? b2 : static_cast<uint8_t>(b2 | fan);
  const uint8_t f[6] = {0xB5, 0x4A, 0xF5, 0x0A, static_cast<uint8_t>(on ? 0x82 : 0x83), last};
=======
// Eco: B5 4A F5 0A (0x82 bật | 0x83 tắt) b2 — b2 là byte nhiệt độ/chế độ hiện tại.
inline uint64_t eco(uint32_t st, bool on) {
  const uint8_t f[6] = {0xB5, 0x4A, 0xF5, 0x0A, static_cast<uint8_t>(on ? 0x82 : 0x83),
                        static_cast<uint8_t>(st)};
>>>>>>> 590e5a8af47a4000555b4d7ece217e777219d121
  return frame48(f);
}

}  // namespace coolix
