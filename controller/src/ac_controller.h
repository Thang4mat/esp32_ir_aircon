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
//     Máy đang tắt mà đang hẹn BẬT thì đổi cài đặt cũng gửi lại khung hẹn BẬT (aircon22).
//   - Sleep = cờ của remote: khi cờ bật, MỌI khung remote gửi (khung thường
//     hoặc khung hẹn giờ) đều đi sau một khung kSleepCode. Bật/Tắt Sleep không
//     ảnh hưởng hẹn giờ. Sleep chỉ có ở Auto và Cool.
//   - Eco = khung 48-bit riêng, có Bật/Tắt tường minh. Eco chỉ có ở Cool.
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
//   - Tắt máy xóa Sleep, Eco, Turbo và mọi hẹn giờ; mức quạt giữ nguyên.
//   - Swing = lệnh đảo trạng thái, không mang cài đặt.
//   - Light = lệnh đảo đèn hiển thị, không mang cài đặt, phát được cả khi máy tắt
//     (máy chỉ bíp) và cả trong lúc Clean (máy đổi đèn). ESP32 không lưu trạng thái Light.
//   - Turbo = lệnh đảo, ESP32 giữ cờ. Chỉ có ở Cool (máy đang bật). Bật Turbo tắt
//     Sleep và Eco; ngược lại bật Sleep, Eco hoặc Clean tắt Turbo. Đổi nhiệt độ và
//     đặt hẹn giờ giữ Turbo; đổi chế độ hoặc tốc độ quạt, và tắt máy, xóa Turbo.
//     Bật Turbo không đổi quạt.
//   - Clean = lệnh đảo, ESP32 giữ cờ (không lưu NVS). Chạy AC_CLEAN_DURATION_MS rồi
//     máy tự tắt. Bật Clean xóa hẹn giờ "Sau X giờ" (của máy), Sleep, Eco, Turbo. Hẹn giờ
//     "Lúc hh:mm" (của ESP32) vẫn giữ: hẹn Tắt vẫn chạy, hẹn Bật bị bỏ qua vì máy không
//     nhận nút Bật trong lúc Clean. Trong lúc Clean máy chỉ nhận nút Clean (dừng Clean,
//     máy tắt hẳn), nút Nguồn (tắt) và nút Light; mọi lệnh khác bị từ chối.
//     Cờ Clean mất khi khởi động lại ESP32.
//
//  HẸN GIỜ có hai loại, loại này loại trừ loại kia (đặt loại nào thì loại kia bị hủy):
//   1) "Sau X giờ" (on_timer/off_timer, phút): MÁY LẠNH tự đếm, ESP32 chỉ gửi khung hẹn
//      giờ rồi THEO DÕI để trạng thái trên Web UI khớp máy. t_start = epoch lúc gửi khung
//      hẹn giờ (lưu NVS). Đến hạn, update() cập nhật power mà KHÔNG phát IR. Hẹn TẮT chạy
//      xong thì Sleep/Eco/Turbo bị xóa như tắt máy bằng nút Nguồn (người dùng xác nhận
//      2026-11-05). Đổi nhiệt độ/quạt/chế độ/Eco/Sleep/Turbo/Swing/Light khi đang hẹn giờ:
//      máy GIỮ MỐC CŨ, không đếm lại (người dùng thử trên máy thật 2026-11-05), nên chỉ
//      setTimers() mới đặt t_start mới. ESP32 tắt/khởi động lại giữa chừng không ảnh hưởng
//      vì t_start là giờ thực: update() tính lại từ t_start.
//   2) "Lúc hh:mm" (on_at/off_at, epoch UTC): ESP32 tự đếm theo giờ thực, đến giờ phát
//      khung Bật/Tắt như nút Nguồn (không dùng khung hẹn giờ của remote). Hẹn MỘT LẦN,
//      mốc kế tiếp (hôm nay nếu còn, nếu không thì ngày mai). Phát lệnh dù ESP32 đang nghĩ
//      máy ở trạng thái nào (để sửa lệch trạng thái do dùng remote gốc). Nếu ESP32 vừa khởi
//      động mà giờ hẹn đã qua thì lệnh CHƯA TỪNG được gửi: xóa hẹn, GIỮ NGUYÊN power và ghi
//      cảnh báo (missed_*, chỉ trong RAM, mất khi người dùng thao tác tiếp). Hẹn Bật khi đang
//      Clean cũng bị bỏ qua và ghi cảnh báo. Giờ Bật và giờ Tắt không được trùng nhau.
//  Nguồn thủ công (setPower) xóa MỌI hẹn giờ. Đặt hẹn giờ cần có giờ thực (NTP): chưa có
//  giờ thì từ chối. Đồng hồ được truyền vào begin() (ClockFn) để test PC dùng đồng hồ giả.
//
//  Các hàm set*() trả về nullptr nếu OK, hoặc chuỗi lỗi (HTTP 400) nếu không
//  hợp lệ — không tự sửa giá trị sai.
// =============================================================================

#include <Arduino.h>
#include <IRsend.h>
#include <Preferences.h>

#include "coolix_frames.h"
#include "config.h"

// Đồng hồ thực: trả epoch UTC (giây), 0 = chưa đồng bộ giờ.
using ClockFn = uint32_t (*)();

struct AcState {
  bool power = false;
  uint8_t mode = coolix::kModeAuto;
  uint8_t fan = coolix::kFanAuto;
  uint8_t temp = AC_TEMP_DEFAULT;
  bool sleep = false;
  bool eco = false;
  bool turbo = false;
  bool clean = false;      // chỉ trong RAM, không lưu NVS
  uint16_t on_timer = 0;   // phút, hẹn "Sau X giờ" (máy đếm), 0 = không hẹn
  uint16_t off_timer = 0;  // phút, hẹn "Sau X giờ" (máy đếm), 0 = không hẹn
  uint32_t t_start = 0;    // epoch lúc gửi khung hẹn giờ "Sau X giờ"; 0 khi không có hẹn đó
  uint32_t on_at = 0;      // epoch đích hẹn "Lúc hh:mm" (ESP32 đếm), 0 = không hẹn
  uint32_t off_at = 0;     // epoch đích hẹn "Lúc hh:mm" (ESP32 đếm), 0 = không hẹn
};

// Số liệu hẹn giờ cho Web UI (ESP32 tính sẵn, trình duyệt không phụ thuộc đồng hồ của nó).
struct TimerInfo {
  int16_t clock = -1;        // phút trong ngày hiện tại (giờ địa phương); -1 = chưa có giờ
  uint8_t kind = 0;          // 0 không hẹn, 1 "Sau X giờ", 2 "Lúc hh:mm"
  uint32_t on_left = 0;      // giây còn lại tới giờ Bật (0 khi không hẹn hoặc đã tới hạn)
  uint32_t off_left = 0;     // giây còn lại tới giờ Tắt
  int16_t on_due = -1;       // phút trong ngày của giờ Bật; -1 = không hẹn hoặc chưa có giờ
  int16_t off_due = -1;      // phút trong ngày của giờ Tắt
  int16_t missed_on = -1;    // phút trong ngày của hẹn Bật không được thực hiện; -1 = không có
  int16_t missed_off = -1;   // phút trong ngày của hẹn Tắt không được thực hiện; -1 = không có
};

class AcController {
 public:
  // irsend đã được begin() ở setup(). Đọc lại cài đặt từ NVS nhưng KHÔNG gửi
  // IR lúc khởi động (không biết chắc trạng thái thật của máy khi ESP32 mất điện).
  // Lúc này chưa có giờ thực nên chưa đối chiếu được mốc hẹn giờ: update() làm việc đó.
  void begin(IRsend& irsend, ClockFn clock) {
    irsend_ = &irsend;
    clock_ = clock;
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
    st_.t_start = prefs_.getUInt("t_start", 0);
    st_.on_at = prefs_.getUInt("on_at", 0);
    st_.off_at = prefs_.getUInt("off_at", 0);
    if (!valid(st_)) {
      Serial.println("[NVS] Dữ liệu đã lưu không hợp lệ, dùng cài đặt mặc định.");
      st_ = AcState();
    }
  }

  const AcState& state() const { return st_; }

  // Gọi trong loop(). Hết thời gian Clean thì máy đã tự tắt: cập nhật cờ cho khớp.
  // Rồi xử lý các hẹn giờ đã tới hạn (cần giờ thực).
  void update() {
    if (st_.clean && millis() - clean_start_ms_ >= AC_CLEAN_DURATION_MS) {
      st_.clean = false;
      st_.power = false;
      save();
      Serial.println("[AC] Clean xong, máy tự tắt.");
    }
    updateTimers();
  }

  const char* setPower(bool on) {
    if (on) {
      if (const char* e = rejectIfCleaning()) return e;
    }
    st_.power = on;
    // Nguồn thủ công hủy mọi hẹn giờ. Khung Bật là khung trạng thái thường nên cũng
    // hủy hẹn giờ trên máy.
    clearTimers();
    if (on) {
      sendState();
    } else {
      sendOff();
    }
    commit();
    return nullptr;
  }

  // Đổi mode/fan/temp. Máy đang tắt: chỉ lưu, áp dụng khi bấm Bật (khung trạng
  // thái nào cũng ngầm bật máy nên không thể gửi khi ESP32 nghĩ là máy đang tắt).
  // Đang hẹn giờ "Sau X giờ": khung hẹn giờ được gửi lại nhưng t_start GIỮ NGUYÊN (máy giữ mốc cũ).
  const char* setClimate(int mode, int fan, int temp) {
    if (const char* e = rejectIfCleaning()) return e;
    if (mode < coolix::kModeAuto || mode > coolix::kModeFan) return "Chế độ không hợp lệ (0-3).";
    if (fan < coolix::kFanAuto || fan > coolix::kFanHigh) return "Tốc độ quạt không hợp lệ (0-3).";
    if (temp < AC_TEMP_MIN || temp > AC_TEMP_MAX) return "Nhiệt độ ngoài khoảng 17-30°C.";
    if (fan != st_.fan && (mode == coolix::kModeAuto || mode == coolix::kModeDry))
      return "Không đổi được tốc độ quạt ở chế độ Auto và Dry.";
    if (mode != st_.mode) {
      // Máy tự tắt Sleep, Eco và Turbo khi đổi chế độ (kể cả sang chế độ có Sleep).
      st_.sleep = false;
      st_.eco = false;
      st_.turbo = false;
    }
    if (fan != st_.fan) {
      // Đổi tốc độ quạt tắt Sleep và Turbo; Eco và hẹn giờ giữ nguyên.
      st_.sleep = false;
      st_.turbo = false;
    }
    st_.mode = static_cast<uint8_t>(mode);
    st_.fan = static_cast<uint8_t>(fan);
    st_.temp = static_cast<uint8_t>(temp);
    if (st_.power || timersActive()) sendState();
    commit();
    return nullptr;
  }

  // Hẹn giờ "Sau X giờ" (máy đếm). 0 = không hẹn. Đặt hẹn giờ mới thì hủy hẹn "Lúc hh:mm".
  // Cả hai mốc được tính từ lúc gửi khung này (t_start = bây giờ).
  const char* setTimers(int on_min, int off_min) {
    if (const char* e = rejectIfCleaning()) return e;
    if (on_min < 0 || on_min > 1440 || !coolix::validTimerMin(static_cast<uint16_t>(on_min)))
      return "Hẹn giờ bật không hợp lệ.";
    if (off_min < 0 || off_min > 1440 || !coolix::validTimerMin(static_cast<uint16_t>(off_min)))
      return "Hẹn giờ tắt không hợp lệ.";
    const bool any = on_min != 0 || off_min != 0;
    const uint32_t t = now();
    if (any && t == 0) return noClockError();
    st_.on_at = 0;  // hẹn "Lúc hh:mm" bị hủy (chỉ ở ESP32, không có gì để gửi cho máy)
    st_.off_at = 0;
    st_.on_timer = static_cast<uint16_t>(on_min);
    st_.off_timer = static_cast<uint16_t>(off_min);
    st_.t_start = any ? t : 0;
    if (timersActive() || st_.power) {
      sendState();       // không còn hẹn giờ: khung trạng thái thường = hủy hẹn giờ
    } else {
      sendOff();         // máy đang tắt: khung "hủy" chính là khung Tắt
    }
    commit();
    return nullptr;
  }

  // Hẹn giờ "Lúc hh:mm" (ESP32 đếm theo giờ thực). Tham số là phút trong ngày 0-1439
  // (giờ địa phương, 6:25 = 385), -1 = không hẹn. Hẹn một lần, mốc kế tiếp.
  // Đặt hẹn giờ này thì hủy hẹn "Sau X giờ" (gửi khung hủy cho máy nếu đang có).
  const char* setSchedule(int on_min, int off_min) {
    if (const char* e = rejectIfCleaning()) return e;
    if (on_min < -1 || on_min > 1439) return "Giờ hẹn bật không hợp lệ (0-1439 phút trong ngày).";
    if (off_min < -1 || off_min > 1439) return "Giờ hẹn tắt không hợp lệ (0-1439 phút trong ngày).";
    if (on_min >= 0 && on_min == off_min) return "Giờ bật và giờ tắt không được trùng nhau.";
    if (on_min < 0 && off_min < 0) return cancelTimers();
    const uint32_t t = now();
    if (t == 0) return noClockError();
    if (timersActive()) sendCancelTimers();
    st_.on_at = on_min >= 0 ? nextOccurrence(t, on_min) : 0;
    st_.off_at = off_min >= 0 ? nextOccurrence(t, off_min) : 0;
    commit();
    return nullptr;
  }

  // Hủy mọi hẹn giờ đang có (cả hai loại). Không có hẹn giờ thì không phát gì.
  const char* cancelTimers() {
    if (const char* e = rejectIfCleaning()) return e;
    if (timersActive()) sendCancelTimers();
    st_.on_at = 0;
    st_.off_at = 0;
    commit();
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
      st_.fan = coolix::kFanAuto;  // bật Sleep đặt quạt về Auto
    }
    if (st_.power) sendState();
    commit();
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
      st_.fan = coolix::kFanAuto;  // bật Eco đặt quạt về Auto
    }
    if (st_.power) send48(coolix::eco(currentState(), on, st_.fan), 1);
    commit();
    return nullptr;
  }

  // Lệnh đảo: không mang cài đặt và ESP32 không biết trạng thái thật nên không lưu gì.
  const char* swing() {
    if (const char* e = rejectIfCleaning()) return e;
    send48(coolix::wire(coolix::kSwingCode), 1);
    return nullptr;
  }

  // Đèn hiển thị: lệnh đảo, phát được cả khi máy tắt (máy chỉ bíp) và cả trong lúc Clean.
  const char* light() {
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
    commit();
    return nullptr;
  }

  // Clean là lệnh đảo. Bật: máy phải đang chạy; chạy AC_CLEAN_DURATION_MS rồi tự tắt.
  // Tắt giữa chừng: máy tắt hẳn. Chỉ phát khi cờ thật sự đổi.
  const char* setClean(bool on) {
    if (on && !st_.power) return "Phải bật máy trước khi bật Clean.";
    if (st_.clean == on) return nullptr;
    st_.clean = on;
    if (on) {
      clearDelayTimers();  // hẹn giờ của máy bị xóa; hẹn "Lúc hh:mm" của ESP32 vẫn giữ
      st_.sleep = false;
      st_.eco = false;
      st_.turbo = false;
      clean_start_ms_ = millis();
    } else {
      st_.power = false;
    }
    send48(coolix::wire(coolix::kCleanCode), 1);
    commit();
    return nullptr;
  }

  // Số liệu hẹn giờ cho Web UI. Chưa có giờ thực thì clock = -1 và không tính được mốc.
  TimerInfo timerInfo() const {
    TimerInfo i;
    i.missed_on = missed_on_;
    i.missed_off = missed_off_;
    if (timersActive()) {
      i.kind = 1;
    } else if (st_.on_at != 0 || st_.off_at != 0) {
      i.kind = 2;
    }
    const uint32_t t = now();
    if (t == 0) return i;
    i.clock = static_cast<int16_t>(minuteOfDay(t));
    const uint32_t on = dueOn();
    const uint32_t off = dueOff();
    if (on != 0) {
      i.on_due = static_cast<int16_t>(minuteOfDay(on));
      i.on_left = on > t ? on - t : 0;
    }
    if (off != 0) {
      i.off_due = static_cast<int16_t>(minuteOfDay(off));
      i.off_left = off > t ? off - t : 0;
    }
    return i;
  }

 private:
  enum : uint32_t { kSecPerMin = 60UL, kSecPerDay = 86400UL };

  IRsend* irsend_ = nullptr;
  ClockFn clock_ = nullptr;
  Preferences prefs_;
  AcState st_;
  uint32_t clean_start_ms_ = 0;
  bool time_checked_ = false;  // đã đối chiếu mốc hẹn giờ lần đầu sau khi có giờ (chỉ RAM)
  int16_t missed_on_ = -1;     // cảnh báo hẹn Bật không được thực hiện (chỉ RAM)
  int16_t missed_off_ = -1;

  uint32_t now() const { return clock_(); }

  static const char* noClockError() { return "Chưa đồng bộ giờ, thử lại sau ít phút."; }

  const char* rejectIfCleaning() const {
    return st_.clean ? "Đang Clean: máy chỉ nhận nút Clean, Tắt nguồn và Light." : nullptr;
  }

  static bool valid(const AcState& s) {
    const bool hasDelay = s.on_timer != 0 || s.off_timer != 0;
    const bool hasSchedule = s.on_at != 0 || s.off_at != 0;
    return s.mode <= coolix::kModeFan && s.fan <= coolix::kFanHigh &&
           s.temp >= AC_TEMP_MIN && s.temp <= AC_TEMP_MAX &&
           coolix::validTimerMin(s.on_timer) && coolix::validTimerMin(s.off_timer) &&
           (!s.turbo || (s.power && s.mode == coolix::kModeCool && !s.sleep && !s.eco)) &&
           !(hasDelay && hasSchedule) && hasDelay == (s.t_start != 0);
  }

  // Mọi thao tác của người dùng kết thúc ở đây: xóa cảnh báo "hẹn giờ không được thực hiện"
  // rồi lưu NVS. update() dùng save() trực tiếp để không xóa cảnh báo vừa ghi.
  void commit() {
    missed_on_ = -1;
    missed_off_ = -1;
    save();
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
    prefs_.putUInt("t_start", st_.t_start);
    prefs_.putUInt("on_at", st_.on_at);
    prefs_.putUInt("off_at", st_.off_at);
  }

  // Chỉ tính hẹn giờ "Sau X giờ" (hẹn giờ của máy): quyết định khung hẹn giờ hay khung thường.
  bool timersActive() const { return st_.on_timer != 0 || st_.off_timer != 0; }

  void clearDelayTimers() {
    st_.on_timer = 0;
    st_.off_timer = 0;
    st_.t_start = 0;
  }

  void clearTimers() {
    clearDelayTimers();
    st_.on_at = 0;
    st_.off_at = 0;
  }

  // Các cờ chỉ có nghĩa khi máy đang chạy.
  void clearRunFlags() {
    st_.sleep = false;
    st_.eco = false;
    st_.turbo = false;
    st_.clean = false;
  }

  uint32_t currentState() const { return coolix::state(st_.mode, st_.fan, st_.temp); }

  // Epoch đến hạn của hẹn "Sau X giờ" (mốc gửi khung + số phút), 0 = không hẹn.
  uint32_t delayDue(uint16_t minutes) const {
    return minutes == 0 ? 0 : st_.t_start + static_cast<uint32_t>(minutes) * kSecPerMin;
  }
  uint32_t dueOn() const { return timersActive() ? delayDue(st_.on_timer) : st_.on_at; }
  uint32_t dueOff() const { return timersActive() ? delayDue(st_.off_timer) : st_.off_at; }

  // Phút trong ngày (giờ địa phương) của một epoch UTC.
  static int minuteOfDay(uint32_t epoch) {
    return static_cast<int>(((epoch + static_cast<uint32_t>(TIMEZONE_OFFSET_SEC)) % kSecPerDay) / kSecPerMin);
  }

  // Mốc epoch kế tiếp ứng với phút-trong-ngày minute: hôm nay nếu còn ở tương lai,
  // nếu không (kể cả trùng đúng giây hiện tại) thì ngày mai.
  static uint32_t nextOccurrence(uint32_t t, int minute) {
    const uint32_t tz = static_cast<uint32_t>(TIMEZONE_OFFSET_SEC);
    const uint32_t local = t + tz;
    uint32_t target = local - local % kSecPerDay + static_cast<uint32_t>(minute) * kSecPerMin;
    if (target <= local) target += kSecPerDay;
    return target - tz;
  }

  // Hủy hẹn giờ "Sau X giờ" trên máy: khung trạng thái thường (đang chạy) hoặc khung Tắt (đang tắt).
  void sendCancelTimers() {
    clearDelayTimers();
    if (st_.power) {
      sendState();
    } else {
      sendOff();
    }
  }

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

  // Khung Tắt máy. Tắt máy xóa Sleep, Eco, Turbo và Clean (các cờ này chỉ có nghĩa khi máy
  // chạy). Việc xóa hẹn giờ do người gọi quyết định (setPower xóa hết, hẹn "Lúc hh:mm" chỉ xóa mốc vừa dùng).
  void sendOff() {
    clearRunFlags();
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

  // Xử lý hẹn giờ đã tới hạn. Chỉ làm được khi đã có giờ thực.
  void updateTimers() {
    const uint32_t t = now();
    if (t == 0) return;
    const bool first = !time_checked_;  // lần đầu có giờ kể từ khi khởi động
    time_checked_ = true;
    if (first && !timestampsSane(t)) {
      Serial.println("[NVS] Mốc thời gian hẹn giờ không hợp lệ, dùng cài đặt mặc định.");
      st_ = AcState();
      save();
      return;
    }
    const bool delayChanged = runDelayTimers(t);
    const bool scheduleChanged = runSchedules(t, first);
    if (delayChanged || scheduleChanged) save();
  }

  // Mốc đã lưu phải hợp lý so với giờ thực: t_start không ở tương lai, mốc "Lúc hh:mm" không xa quá 24 giờ.
  bool timestampsSane(uint32_t t) const {
    return st_.t_start <= t && st_.on_at <= t + kSecPerDay && st_.off_at <= t + kSecPerDay;
  }

  // Hẹn "Sau X giờ": máy đã tự làm nên ESP32 chỉ cập nhật trạng thái, KHÔNG phát IR.
  // Xử lý theo thứ tự thời gian (cả hai tính từ cùng t_start). Trùng giờ: Bật rồi Tắt (suy ra,
  // chưa có tín hiệu thu).
  bool runDelayTimers(uint32_t t) {
    if (!timersActive()) return false;
    bool changed = false;
    for (int i = 0; i < 2; i++) {
      const bool onReady = st_.on_timer != 0 && delayDue(st_.on_timer) <= t;
      const bool offReady = st_.off_timer != 0 && delayDue(st_.off_timer) <= t;
      if (!onReady && !offReady) break;
      changed = true;
      if (onReady && (!offReady || delayDue(st_.on_timer) <= delayDue(st_.off_timer))) {
        st_.power = true;
        st_.on_timer = 0;
      } else {
        st_.power = false;
        clearRunFlags();  // hẹn Tắt chạy xong: Sleep/Eco/Turbo bị xóa như tắt máy bằng nút Nguồn
        st_.off_timer = 0;
      }
    }
    if (!timersActive()) st_.t_start = 0;
    return changed;
  }

  // Hẹn "Lúc hh:mm": ESP32 phát lệnh Bật/Tắt. lost = ESP32 vừa khởi động nên giờ hẹn đã qua
  // thì lệnh chưa từng được gửi (ESP32 đang tắt hoặc chưa có giờ): xóa hẹn, giữ nguyên power.
  // Trùng giờ không xảy ra (setSchedule từ chối); nếu có hai mốc cùng tới hạn thì theo thứ tự thời gian.
  bool runSchedules(uint32_t t, bool lost) {
    bool changed = false;
    for (int i = 0; i < 2; i++) {
      const bool onReady = st_.on_at != 0 && st_.on_at <= t;
      const bool offReady = st_.off_at != 0 && st_.off_at <= t;
      if (!onReady && !offReady) break;
      changed = true;
      if (onReady && (!offReady || st_.on_at <= st_.off_at)) {
        const uint32_t at = st_.on_at;
        st_.on_at = 0;
        if (lost || st_.clean) {  // Clean: máy bỏ qua nút Bật (aircon26/27)
          missed_on_ = static_cast<int16_t>(minuteOfDay(at));
          Serial.printf("[AC] Hẹn Bật %02d:%02d không được thực hiện.\n", minuteOfDay(at) / 60, minuteOfDay(at) % 60);
        } else {
          st_.power = true;  // như nút Nguồn: khung trạng thái thường, phát dù ESP32 nghĩ máy đang bật
          sendState();
        }
      } else {
        const uint32_t at = st_.off_at;
        st_.off_at = 0;
        if (lost) {
          missed_off_ = static_cast<int16_t>(minuteOfDay(at));
          Serial.printf("[AC] Hẹn Tắt %02d:%02d không được thực hiện.\n", minuteOfDay(at) / 60, minuteOfDay(at) % 60);
        } else {
          st_.power = false;  // phát cả khi ESP32 nghĩ máy đã tắt (sửa lệch do dùng remote gốc)
          sendOff();
        }
      }
    }
    return changed;
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
