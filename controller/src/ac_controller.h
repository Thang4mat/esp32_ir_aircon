#pragma once
// =============================================================================
//  ac_controller.h — trạng thái máy lạnh + quyết định gửi khung IR nào.
//
//  Mô hình của remote COOLIX (suy ra từ tín hiệu thu thực tế):
//   - Khung trạng thái thường = "bật máy với mode/fan/temp này". Không có bit
//     nguồn riêng: tắt máy là một khung riêng (kOffCode).
//   - Khung trạng thái thường cũng chính là lệnh HỦY hẹn giờ.
//   - Hẹn giờ = khung 48-bit mang theo cả cài đặt hiện tại. Khi đang hẹn giờ,
//     remote gửi lại khung hẹn giờ (không gửi khung thường) mỗi lần đổi cài đặt.
<<<<<<< HEAD
//     Máy đang tắt mà đang hẹn BẬT thì đổi cài đặt cũng gửi lại khung hẹn BẬT (aircon22).
=======
>>>>>>> 590e5a8af47a4000555b4d7ece217e777219d121
//   - Sleep = cờ của remote: khi cờ bật, MỌI khung remote gửi (khung thường
//     hoặc khung hẹn giờ) đều đi sau một khung kSleepCode. Bật/Tắt Sleep không
//     ảnh hưởng hẹn giờ. Sleep chỉ có ở Auto và Cool.
//   - Eco = khung 48-bit riêng, có Bật/Tắt tường minh. Eco chỉ có ở Cool.
<<<<<<< HEAD
//     Khung TẮT Eco mang mã quạt hiện tại ở 2 bit thấp byte cuối (xem coolix::eco).
//   - Sleep và Eco loại trừ nhau: bật cái này thì máy tự tắt cái kia.
//   - Bật Eco hoặc bật Sleep thì quạt về Auto. Tắt Eco/Sleep giữ mức quạt hiện tại.
//   - Đổi nhiệt độ giữ Sleep, Eco, Turbo, hẹn giờ.
//   - Đổi tốc độ quạt tắt Sleep và Turbo, KHÔNG tắt Eco, giữ hẹn giờ. Chọn lại đúng
//     mức quạt đang lưu thì không tính là đổi (Web UI luôn gửi kèm quạt hiện tại).
//   - Đổi chế độ tự tắt Sleep, Eco, Turbo trên máy (phải bật lại thủ công), giữ hẹn giờ.
//   - Auto và Dry: remote không cho chọn quạt, khung luôn mã quạt 000; mức quạt đã
//     chọn vẫn được giữ để dùng lại ở Cool/Fan. setClimate() từ chối đổi quạt ở hai
//     chế độ này.
//   - Tắt máy xóa Sleep, Eco, Turbo và hẹn giờ; mức quạt giữ nguyên.
//   - Swing = lệnh đảo trạng thái, không mang cài đặt.
//   - Light = lệnh đảo đèn hiển thị, không mang cài đặt, phát được cả khi máy tắt
//     (máy chỉ bíp) và cả trong lúc Clean (máy đổi đèn). ESP32 không lưu trạng thái Light.
//   - Turbo = lệnh đảo, ESP32 giữ cờ. Chỉ có ở Cool (máy đang bật). Bật Turbo tắt
//     Sleep và Eco; ngược lại bật Sleep, Eco hoặc Clean tắt Turbo. Đổi nhiệt độ và
//     đặt hẹn giờ giữ Turbo; đổi chế độ hoặc tốc độ quạt, và tắt máy, xóa Turbo.
//     Bật Turbo không đổi quạt.
//   - Clean = lệnh đảo, ESP32 giữ cờ (không lưu NVS). Chạy AC_CLEAN_DURATION_MS rồi
//     máy tự tắt. Bật Clean xóa hẹn giờ, Sleep, Eco, Turbo. Trong lúc Clean máy chỉ
//     nhận nút Clean (dừng Clean, máy tắt hẳn), nút Nguồn (tắt) và nút Light; mọi lệnh
//     khác bị từ chối. Cờ Clean mất khi khởi động lại ESP32.
=======
//   - Sleep và Eco loại trừ nhau: bật cái này thì máy tự tắt cái kia.
//   - Đổi chế độ tự tắt Sleep và Eco trên máy (phải bật lại thủ công). Đổi nhiệt
//     độ hoặc tốc độ quạt thì giữ Sleep, Eco và hẹn giờ.
//   - Tắt máy xóa Sleep, Eco và hẹn giờ.
//   - Swing = lệnh đảo trạng thái, không mang cài đặt.
//   - Light = lệnh đảo đèn hiển thị, không mang cài đặt, phát được cả khi máy tắt
//     (máy chỉ bíp). ESP32 không lưu trạng thái Light.
//   - Turbo = lệnh đảo, ESP32 giữ cờ. Chỉ có ở Cool (máy đang bật). Bật Turbo tắt
//     Sleep và Eco; ngược lại bật Sleep, Eco hoặc Clean tắt Turbo. Đổi nhiệt độ giữ
//     Turbo; đổi chế độ hoặc tốc độ quạt, và tắt máy, xóa Turbo. Không ảnh hưởng hẹn giờ.
//   - Clean = lệnh đảo, ESP32 giữ cờ (không lưu NVS). Chạy AC_CLEAN_DURATION_MS rồi
//     máy tự tắt. Bật Clean xóa hẹn giờ, Sleep, Eco, Turbo. Trong lúc Clean máy chỉ
//     nhận nút Clean (dừng Clean, máy tắt hẳn) và nút Nguồn (tắt); mọi lệnh khác
//     bị từ chối. Cờ Clean mất khi khởi động lại ESP32.
>>>>>>> 590e5a8af47a4000555b4d7ece217e777219d121
//
//  Các hàm set*() trả về nullptr nếu OK, hoặc chuỗi lỗi (HTTP 400) nếu không
//  hợp lệ — không tự sửa giá trị sai.
// =============================================================================

#include <Arduino.h>
#include <IRsend.h>
#include <Preferences.h>

#include "coolix_frames.h"
#include "config.h"

struct AcState {
  bool power = false;
  uint8_t mode = coolix::kModeAuto;
  uint8_t fan = coolix::kFanAuto;
  uint8_t temp = AC_TEMP_DEFAULT;
  bool sleep = false;
  bool eco = false;
  bool turbo = false;
  bool clean = false;      // chỉ trong RAM, không lưu NVS
  uint16_t on_timer = 0;   // phút, 0 = không hẹn
  uint16_t off_timer = 0;  // phút, 0 = không hẹn
};

class AcController {
 public:
  // irsend đã được begin() ở setup(). Đọc lại cài đặt từ NVS nhưng KHÔNG gửi
  // IR lúc khởi động (không biết chắc trạng thái thật của máy khi ESP32 mất điện).
  void begin(IRsend& irsend) {
    irsend_ = &irsend;
    prefs_.begin("ac", false);
    st_.power = prefs_.getBool("power", false);
    st_.mode = prefs_.getUChar("mode", coolix::kModeAuto);
    st_.fan = prefs_.getUChar("fan", coolix::kFanAuto);
    st_.temp = prefs_.getUChar("temp", AC_TEMP_DEFAULT);
    st_.sleep = prefs_.getBool("sleep", false);
    st_.eco = prefs_.getBool("eco", false);
    st_.turbo = prefs_.getBool("turbo", false);
    st_.on_timer = prefs_.getUShort("on_timer", 0);
    st_.off_timer = prefs_.getUShort("off_timer", 0);
    if (!valid(st_)) {
      Serial.println("[NVS] Dữ liệu đã lưu không hợp lệ, dùng cài đặt mặc định.");
      st_ = AcState();
    }
  }

  const AcState& state() const { return st_; }

  // Gọi trong loop(). Hết thời gian Clean thì máy đã tự tắt: cập nhật cờ cho khớp.
  void update() {
    if (!st_.clean) return;
    if (millis() - clean_start_ms_ < AC_CLEAN_DURATION_MS) return;
    st_.clean = false;
    st_.power = false;
    save();
    Serial.println("[AC] Clean xong, máy tự tắt.");
  }

  const char* setPower(bool on) {
    if (on) {
      if (const char* e = rejectIfCleaning()) return e;
    }
    st_.power = on;
    if (on) {
      // Khung Bật là khung trạng thái thường nên hủy hẹn giờ.
      st_.on_timer = 0;
      st_.off_timer = 0;
      sendState();
    } else {
      sendOff();
    }
    save();
    return nullptr;
  }

  // Đổi mode/fan/temp. Máy đang tắt: chỉ lưu, áp dụng khi bấm Bật (khung trạng
  // thái nào cũng ngầm bật máy nên không thể gửi khi ESP32 nghĩ là máy đang tắt).
  const char* setClimate(int mode, int fan, int temp) {
    if (const char* e = rejectIfCleaning()) return e;
    if (mode < coolix::kModeAuto || mode > coolix::kModeFan) return "Chế độ không hợp lệ (0-3).";
    if (fan < coolix::kFanAuto || fan > coolix::kFanHigh) return "Tốc độ quạt không hợp lệ (0-3).";
    if (temp < AC_TEMP_MIN || temp > AC_TEMP_MAX) return "Nhiệt độ ngoài khoảng 17-30°C.";
<<<<<<< HEAD
    if (fan != st_.fan && (mode == coolix::kModeAuto || mode == coolix::kModeDry))
      return "Không đổi được tốc độ quạt ở chế độ Auto và Dry.";
=======
>>>>>>> 590e5a8af47a4000555b4d7ece217e777219d121
    if (mode != st_.mode) {
      // Máy tự tắt Sleep, Eco và Turbo khi đổi chế độ (kể cả sang chế độ có Sleep).
      st_.sleep = false;
      st_.eco = false;
      st_.turbo = false;
    }
<<<<<<< HEAD
    if (fan != st_.fan) {
      // Đổi tốc độ quạt tắt Sleep và Turbo; Eco và hẹn giờ giữ nguyên.
      st_.sleep = false;
      st_.turbo = false;
    }
=======
    if (fan != st_.fan) st_.turbo = false;  // đổi tốc độ quạt tắt Turbo
>>>>>>> 590e5a8af47a4000555b4d7ece217e777219d121
    st_.mode = static_cast<uint8_t>(mode);
    st_.fan = static_cast<uint8_t>(fan);
    st_.temp = static_cast<uint8_t>(temp);
    if (st_.power || timersActive()) sendState();
    save();
    return nullptr;
  }

  const char* setTimers(int on_min, int off_min) {
    if (const char* e = rejectIfCleaning()) return e;
    if (on_min < 0 || on_min > 1440 || !coolix::validTimerMin(static_cast<uint16_t>(on_min)))
      return "Hẹn giờ bật không hợp lệ.";
    if (off_min < 0 || off_min > 1440 || !coolix::validTimerMin(static_cast<uint16_t>(off_min)))
      return "Hẹn giờ tắt không hợp lệ.";
    st_.on_timer = static_cast<uint16_t>(on_min);
    st_.off_timer = static_cast<uint16_t>(off_min);
    if (timersActive() || st_.power) {
      sendState();       // không còn hẹn giờ: khung trạng thái thường = hủy hẹn giờ
    } else {
      sendOff();         // máy đang tắt: khung "hủy" chính là khung Tắt
    }
    save();
    return nullptr;
  }

  const char* setSleep(bool on) {
    if (const char* e = rejectIfCleaning()) return e;
    if (on && !st_.power) return "Phải bật máy trước khi bật Sleep.";
    if (on && st_.mode != coolix::kModeAuto && st_.mode != coolix::kModeCool)
      return "Sleep chỉ dùng được ở chế độ Auto và Cool.";
    st_.sleep = on;
    if (on) {
      st_.eco = false;
      st_.turbo = false;
<<<<<<< HEAD
      st_.fan = coolix::kFanAuto;  // bật Sleep đặt quạt về Auto
=======
>>>>>>> 590e5a8af47a4000555b4d7ece217e777219d121
    }
    if (st_.power) sendState();
    save();
    return nullptr;
  }

  const char* setEco(bool on) {
    if (const char* e = rejectIfCleaning()) return e;
    if (on && !st_.power) return "Phải bật máy trước khi bật Eco.";
    if (on && st_.mode != coolix::kModeCool) return "Eco chỉ dùng được ở chế độ Cool.";
    st_.eco = on;
    if (on) {
      st_.sleep = false;
      st_.turbo = false;
<<<<<<< HEAD
      st_.fan = coolix::kFanAuto;  // bật Eco đặt quạt về Auto
    }
    if (st_.power) send48(coolix::eco(currentState(), on, st_.fan), 1);
=======
    }
    if (st_.power) send48(coolix::eco(currentState(), on), 1);
>>>>>>> 590e5a8af47a4000555b4d7ece217e777219d121
    save();
    return nullptr;
  }

  // Lệnh đảo: không mang cài đặt và ESP32 không biết trạng thái thật nên không lưu gì.
  const char* swing() {
    if (const char* e = rejectIfCleaning()) return e;
    send48(coolix::wire(coolix::kSwingCode), 1);
    return nullptr;
  }

<<<<<<< HEAD
  // Đèn hiển thị: lệnh đảo, phát được cả khi máy tắt (máy chỉ bíp) và cả trong lúc Clean.
  const char* light() {
=======
  // Đèn hiển thị: lệnh đảo, phát được cả khi máy tắt (máy chỉ bíp).
  const char* light() {
    if (const char* e = rejectIfCleaning()) return e;
>>>>>>> 590e5a8af47a4000555b4d7ece217e777219d121
    send48(coolix::wire(coolix::kLightCode), 1);
    return nullptr;
  }

  // Turbo là lệnh đảo nên chỉ phát khi cờ thật sự đổi (bật khi đã bật = không phát).
  const char* setTurbo(bool on) {
    if (const char* e = rejectIfCleaning()) return e;
    if (on && !st_.power) return "Phải bật máy trước khi bật Turbo.";
    if (on && st_.mode != coolix::kModeCool) return "Turbo chỉ dùng được ở chế độ Cool.";
    if (st_.turbo == on) return nullptr;
    st_.turbo = on;
    if (on) {
      st_.sleep = false;
      st_.eco = false;
    }
    send48(coolix::wire(coolix::kTurboCode), 1);
    save();
    return nullptr;
  }

  // Clean là lệnh đảo. Bật: máy phải đang chạy; chạy AC_CLEAN_DURATION_MS rồi tự tắt.
  // Tắt giữa chừng: máy tắt hẳn. Chỉ phát khi cờ thật sự đổi.
  const char* setClean(bool on) {
    if (on && !st_.power) return "Phải bật máy trước khi bật Clean.";
    if (st_.clean == on) return nullptr;
    st_.clean = on;
    if (on) {
      st_.on_timer = 0;
      st_.off_timer = 0;
      st_.sleep = false;
      st_.eco = false;
      st_.turbo = false;
      clean_start_ms_ = millis();
    } else {
      st_.power = false;
    }
    send48(coolix::wire(coolix::kCleanCode), 1);
    save();
    return nullptr;
  }

 private:
  IRsend* irsend_ = nullptr;
  Preferences prefs_;
  AcState st_;
  uint32_t clean_start_ms_ = 0;

  const char* rejectIfCleaning() const {
<<<<<<< HEAD
    return st_.clean ? "Đang Clean: máy chỉ nhận nút Clean, Tắt nguồn và Light." : nullptr;
=======
    return st_.clean ? "Đang Clean: máy chỉ nhận nút Clean và Tắt nguồn." : nullptr;
>>>>>>> 590e5a8af47a4000555b4d7ece217e777219d121
  }

  static bool valid(const AcState& s) {
    return s.mode <= coolix::kModeFan && s.fan <= coolix::kFanHigh &&
           s.temp >= AC_TEMP_MIN && s.temp <= AC_TEMP_MAX &&
           coolix::validTimerMin(s.on_timer) && coolix::validTimerMin(s.off_timer) &&
           (!s.turbo || (s.power && s.mode == coolix::kModeCool && !s.sleep && !s.eco));
  }

  void save() {
    prefs_.putBool("power", st_.power);
    prefs_.putUChar("mode", st_.mode);
    prefs_.putUChar("fan", st_.fan);
    prefs_.putUChar("temp", st_.temp);
    prefs_.putBool("sleep", st_.sleep);
    prefs_.putBool("eco", st_.eco);
    prefs_.putBool("turbo", st_.turbo);
    prefs_.putUShort("on_timer", st_.on_timer);
    prefs_.putUShort("off_timer", st_.off_timer);
  }

  bool timersActive() const { return st_.on_timer != 0 || st_.off_timer != 0; }

  uint32_t currentState() const { return coolix::state(st_.mode, st_.fan, st_.temp); }

  // Gửi khung mô tả cài đặt hiện tại, đúng như remote: nếu Sleep bật thì khung
  // kSleepCode đi trước (gửi 1 lần); sau đó là khung hẹn giờ nếu đang có hẹn giờ,
  // ngược lại là khung trạng thái thường (khung này hủy hẹn giờ trên máy).
  void sendState() {
    if (st_.sleep) send48(coolix::wire(coolix::kSleepCode), 0);
    if (timersActive()) {
      sendTimers();
    } else {
      send48(coolix::wire(currentState()), 1);
    }
  }

  // Tắt máy xóa luôn hẹn giờ, Sleep, Eco, Turbo và Clean (các cờ này chỉ có nghĩa khi máy chạy).
  void sendOff() {
    st_.sleep = false;
    st_.eco = false;
    st_.turbo = false;
    st_.clean = false;
    st_.on_timer = 0;
    st_.off_timer = 0;
    send48(coolix::wire(coolix::kOffCode), 1);
  }

  // Một khung duy nhất mô tả cả hai hẹn giờ: có hẹn tắt thì dùng khung TẮT
  // (mang theo hẹn bật nếu có), chỉ hẹn bật thì dùng khung BẬT.
  void sendTimers() {
    const uint32_t s = currentState();
    send48(st_.off_timer != 0 ? coolix::timerOff(s, st_.off_timer, st_.on_timer)
                              : coolix::timerOn(s, st_.on_timer),
           1);
  }

  // Thời gian đúng theo thư viện IRremoteESP8266 (ir_Coolix.cpp), khớp tín hiệu thu.
  void send48(uint64_t frame, uint16_t repeat) {
    constexpr uint16_t kHdrMark = 4692;
    constexpr uint32_t kHdrSpace = 4416;
    constexpr uint16_t kBitMark = 552;
    constexpr uint32_t kOneSpace = 1656;
    constexpr uint32_t kZeroSpace = 552;
    constexpr uint32_t kMinGap = 5244;
    irsend_->sendGeneric(kHdrMark, kHdrSpace, kBitMark, kOneSpace, kBitMark, kZeroSpace,
                         kBitMark, kMinGap, frame, 48, 38, true, repeat, 50);
    Serial.printf("[IR] %08lX%08lX x%u\n", static_cast<unsigned long>(frame >> 32),
                  static_cast<unsigned long>(frame & 0xFFFFFFFFULL), repeat + 1);
  }
};
