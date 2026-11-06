// Bộ test chạy trên PC (không cần ESP32, không cần PlatformIO). Chạy: ./test/run.sh
// Đối chiếu coolix_frames.h với tín hiệu thu thật của remote (aircon*.txt) và kiểm tra
// logic chọn khung IR / lưu NVS của ac_controller.h bằng stub phần cứng (test/stub/).
#include <stdio.h>
#include <vector>
#include "Arduino.h"
#include "Preferences.h"
#include "IRsend.h"
#include "ac_controller.h"

static int pass = 0, fail = 0;
#define CHECK(cond, name) do { if (cond) pass++; else { fail++; printf("FAIL: %s (dòng %d)\n", name, __LINE__); } } while (0)
static void eq(uint64_t got, uint64_t exp, const char* n) {
  if (got == exp) pass++;
  else { fail++; printf("FAIL %s: được %012llX, mong đợi %012llX\n", n, (unsigned long long)got, (unsigned long long)exp); }
}

// Đồng hồ thực giả (epoch UTC, giây). Test tự chỉnh g_epoch để giả lập thời gian trôi; 0 = chưa có giờ.
// L() đổi giờ địa phương UTC+7 sang epoch, viết độc lập với TIMEZONE_OFFSET_SEC (ngày 0 = 2026-11-05).
static uint32_t L(int day, int h, int m, int s = 0) {
  return 1793811600u + day * 86400u + h * 3600u + m * 60u + s;  // 1793811600 = 2026-11-05 00:00 +07:00
}
static const uint32_t kT0 = L(0, 10, 0);  // mốc mặc định: 2026-11-05 10:00:00 giờ địa phương
static uint32_t g_epoch = kT0;
static uint32_t testNow() { return g_epoch; }

// Khung 24-bit -> 48-bit trên dây, viết độc lập với coolix::wire()
static uint64_t w24(uint32_t c) {
  uint64_t o = 0;
  for (int s = 16; s >= 0; s -= 8) { uint8_t b = (c >> s) & 0xFF; o = (o << 16) | ((uint64_t)b << 8) | (uint8_t)~b; }
  return o;
}
struct Rig {
  IRsend ir; AcController ac;
  Rig() { nvsStore().clear(); g_epoch = kT0; ac.begin(ir, testNow); ac.update(); }
  void clear() { ir.log.clear(); }
};
static bool seq(const IRsend& ir, std::vector<Sent> exp) {
  if (ir.log.size() != exp.size()) return false;
  for (size_t i = 0; i < exp.size(); i++)
    if (ir.log[i].frame != exp[i].frame || ir.log[i].repeat != exp[i].repeat || ir.log[i].nbits != 48) return false;
  return true;
}
// Máy đang chạy Cool/Auto/26
static void coolOn(Rig& r) {
  r.ac.setClimate(1, 0, 26); r.ac.setPower(true); r.clear();
}

// Đưa đồng hồ giả tới epoch rồi gọi update() như loop() của firmware.
template <class R> static void at(R& r, uint32_t epoch) { g_epoch = epoch; r.ac.update(); }
// Khởi động lại ESP32: bộ điều khiển mới đọc NVS cũ (KHÔNG xóa kho NVS), rồi update() một lần như loop().
// epoch = 0: chưa đồng bộ giờ.
struct Boot {
  IRsend ir; AcController ac;
  explicit Boot(uint32_t epoch) { g_epoch = epoch; ac.begin(ir, testNow); ac.update(); }
};

int main() {
  // 0. Mã hoá khung trực tiếp bằng coolix:: (đối chiếu từng byte với aircon*.txt)
  { using namespace coolix;
    const uint32_t t[14] = {0xB2BF00,0xB2BF10,0xB2BF30,0xB2BF20,0xB2BF60,0xB2BF70,0xB2BF50,
                            0xB2BF40,0xB2BFC0,0xB2BFD0,0xB2BF90,0xB2BF80,0xB2BFA0,0xB2BFB0};
    for (int i = 0; i < 14; i++) eq(state(kModeCool, kFanAuto, 17 + i), t[i], "state() bảng nhiệt Cool");
    eq(state(kModeCool,kFanLow,26), 0xB29FD0, "state() quạt Thấp");
    eq(state(kModeCool,kFanMed,26), 0xB25FD0, "state() quạt Vừa");
    eq(state(kModeCool,kFanHigh,26), 0xB23FD0, "state() quạt Cao");
    eq(state(kModeDry,kFanAuto,26), 0xB21FD4, "state() Dry");
    eq(state(kModeAuto,kFanAuto,26), 0xB21FD8, "state() Auto");
    eq(state(kModeFan,kFanAuto,26), 0xB2BFE4, "state() Fan");
    // Auto/Dry luôn mã quạt 000 dù mức quạt lưu là gì (aircon13: Dry B21F94, Auto B21F98 với quạt Cao)
    eq(state(kModeDry,kFanHigh,27), 0xB21F94, "state() Dry quạt lưu Cao vẫn mã 000");
    eq(state(kModeAuto,kFanHigh,27), 0xB21F98, "state() Auto quạt lưu Cao vẫn mã 000");
    eq(state(kModeDry,kFanLow,27), 0xB21F94, "state() Dry quạt lưu Thấp vẫn mã 000");
    // Fan mode có mã quạt (aircon23)
    eq(state(kModeFan,kFanLow,25), 0xB29FE4, "state() Fan quạt Thấp");
    eq(state(kModeFan,kFanMed,25), 0xB25FE4, "state() Fan quạt Vừa");
    eq(state(kModeFan,kFanHigh,25), 0xB23FE4, "state() Fan quạt Cao");
    eq(wire(0xB2BFD0), 0xB24DBF40D02FULL, "wire(B2BFD0)");
    eq(wire(kSleepCode), 0xB24DE01F03FCULL, "wire(B2E003)");
    eq(wire(kOffCode), 0xB24D7B84E01FULL, "wire(B27BE0)");
    eq(wire(kLightCode), 0xB54AF50AA55AULL, "wire(B5F5A5) Light");
    eq(wire(kTurboCode), 0xB54AF50AA25DULL, "wire(B5F5A2) Turbo");
    eq(wire(kCleanCode), 0xB54AF50AAA55ULL, "wire(B5F5AA) Clean");
    const uint32_t s = state(kModeCool, kFanAuto, 26), s30 = state(kModeCool, kFanAuto, 30);
    eq(timerOn(s,30), 0xB24DBF40D381ULL, "timerOn 0.5h");  eq(timerOn(s,1440), 0xB24DBF40D3DFULL, "timerOn 24h");
    eq(timerOff(s,30,0), 0xB24DA15ED0FFULL, "timerOff 0.5h"); eq(timerOff(s,1440,0), 0xB24DBF40D2FFULL, "timerOff 24h");
    eq(timerOff(s,30,60), 0xB24DA15ED083ULL, "timerOff 0.5h + timerOn 1h");
    eq(timerOff(s,600,300), 0xB24DA758D193ULL, "timerOff 10h + timerOn 5h");
    eq(timerOn(s30,30), 0xB24DBF40B381ULL, "timerOn 0.5h @30C"); eq(timerOff(s30,30,0), 0xB24DA15EB0FFULL, "timerOff 0.5h @30C");
    eq(eco(s30,true,kFanAuto), 0xB54AF50A82B0ULL, "eco bật @30C"); eq(eco(s30,false,kFanAuto), 0xB54AF50A83B0ULL, "eco tắt @30C");
    eq(eco(s,true,kFanAuto), 0xB54AF50A82D0ULL, "eco bật @26C");  eq(eco(s,false,kFanAuto), 0xB54AF50A83D0ULL, "eco tắt @26C");
    // Eco BẬT không mang mã quạt; Eco TẮT mang mã quạt ở 2 bit thấp (aircon19, aircon27)
    const uint32_t s25 = state(kModeCool, kFanAuto, 25);
    eq(eco(s25,true,kFanHigh), 0xB54AF50A82C0ULL, "eco bật không mang quạt");
    eq(eco(s25,false,kFanAuto), 0xB54AF50A83C0ULL, "eco tắt quạt Auto");
    eq(eco(s25,false,kFanLow), 0xB54AF50A83C1ULL, "eco tắt quạt Thấp");
    eq(eco(s25,false,kFanMed), 0xB54AF50A83C2ULL, "eco tắt quạt Vừa");
    eq(eco(s25,false,kFanHigh), 0xB54AF50A83C3ULL, "eco tắt quạt Cao");
    const uint32_t sd = state(kModeDry, kFanAuto, 26);
    eq(timerOn(sd,300), 0xB24D1FE0D793ULL, "Dry timerOn 5h"); eq(timerOff(sd,600,0), 0xB24D07F8D5FFULL, "Dry timerOff 10h");
  }
  // 1. Bảng nhiệt Cool 17-30 (aircon2.txt)
  { const uint32_t code[14] = {0xB2BF00,0xB2BF10,0xB2BF30,0xB2BF20,0xB2BF60,0xB2BF70,0xB2BF50,
                               0xB2BF40,0xB2BFC0,0xB2BFD0,0xB2BF90,0xB2BF80,0xB2BFA0,0xB2BFB0};
    Rig r; r.ac.setClimate(1, 0, 26); r.ac.setPower(true);
    for (int t = 17; t <= 30; t++) { r.clear(); r.ac.setClimate(1, 0, t);
      CHECK(seq(r.ir, {{w24(code[t-17]),48,1}}), "bảng nhiệt Cool"); } }

  // 1b. Bảng nhiệt Auto và Dry 17-30 (aircon23.txt)
  { const uint8_t autoCode[14] = {0x08,0x18,0x38,0x28,0x68,0x78,0x58,0x48,0xC8,0xD8,0x98,0x88,0xA8,0xB8};
    const uint8_t dryCode[14]  = {0x04,0x14,0x34,0x24,0x64,0x74,0x54,0x44,0xC4,0xD4,0x94,0x84,0xA4,0xB4};
    Rig a; a.ac.setClimate(0, 0, 26); a.ac.setPower(true);
    for (int t = 17; t <= 30; t++) { a.clear(); a.ac.setClimate(0, 0, t);
      CHECK(seq(a.ir, {{w24(0xB21F00u | autoCode[t-17]),48,1}}), "bảng nhiệt Auto"); }
    Rig d; d.ac.setClimate(2, 0, 26); d.ac.setPower(true);
    for (int t = 17; t <= 30; t++) { d.clear(); d.ac.setClimate(2, 0, t);
      CHECK(seq(d.ir, {{w24(0xB21F00u | dryCode[t-17]),48,1}}), "bảng nhiệt Dry"); } }

  // 2. Mode/Fan (aircon.txt)
  { Rig r; r.ac.setClimate(1,0,26); r.ac.setPower(true);
    r.clear(); r.ac.setClimate(1,1,26); CHECK(seq(r.ir,{{w24(0xB29FD0),48,1}}), "Cool quạt Thấp B29FD0");
    r.clear(); r.ac.setClimate(1,2,26); CHECK(seq(r.ir,{{w24(0xB25FD0),48,1}}), "Cool quạt Vừa B25FD0");
    r.clear(); r.ac.setClimate(1,3,26); CHECK(seq(r.ir,{{w24(0xB23FD0),48,1}}), "Cool quạt Cao B23FD0");
    r.clear(); r.ac.setClimate(1,0,26); CHECK(seq(r.ir,{{w24(0xB2BFD0),48,1}}), "Cool quạt Auto B2BFD0");
    r.clear(); r.ac.setClimate(2,0,26); CHECK(seq(r.ir,{{w24(0xB21FD4),48,1}}), "Dry B21FD4");
    r.clear(); r.ac.setClimate(0,0,26); CHECK(seq(r.ir,{{w24(0xB21FD8),48,1}}), "Auto B21FD8");
    r.clear(); r.ac.setClimate(3,0,26); CHECK(seq(r.ir,{{w24(0xB2BFE4),48,1}}), "Fan B2BFE4"); }

  // 2b. Quạt ở chế độ Fan (aircon23.txt)
  { Rig r; r.ac.setClimate(3,0,25); r.ac.setPower(true);
    r.clear(); r.ac.setClimate(3,1,25); CHECK(seq(r.ir,{{w24(0xB29FE4),48,1}}), "Fan quạt Thấp B29FE4");
    r.clear(); r.ac.setClimate(3,2,25); CHECK(seq(r.ir,{{w24(0xB25FE4),48,1}}), "Fan quạt Vừa B25FE4");
    r.clear(); r.ac.setClimate(3,3,25); CHECK(seq(r.ir,{{w24(0xB23FE4),48,1}}), "Fan quạt Cao B23FE4");
    r.clear(); r.ac.setClimate(3,0,25); CHECK(seq(r.ir,{{w24(0xB2BFE4),48,1}}), "Fan quạt Auto B2BFE4"); }

  // 2c. Quạt đã chọn giữ khi qua Dry/Auto, remote không cho đổi quạt ở đó (aircon13.txt)
  //     Cool quạt Cao 27C -> Dry -> Fan -> Auto -> Cool -> tắt -> bật
  { Rig r; r.ac.setClimate(1,3,27); r.ac.setPower(true); r.clear();
    CHECK(r.ac.setClimate(2,3,27) == nullptr && seq(r.ir,{{w24(0xB21F94),48,1}}), "[aircon13] Dry khung mã quạt 000 B21F94");
    CHECK(r.ac.state().fan == 3, "Dry giữ mức quạt đã lưu (Cao)");
    r.clear(); CHECK(r.ac.setClimate(3,3,27) == nullptr && seq(r.ir,{{w24(0xB23FE4),48,1}}), "[aircon13] Fan quạt Cao B23FE4");
    r.clear(); CHECK(r.ac.setClimate(0,3,27) == nullptr && seq(r.ir,{{w24(0xB21F98),48,1}}), "[aircon13] Auto B21F98");
    r.clear(); CHECK(r.ac.setClimate(1,3,27) == nullptr && seq(r.ir,{{w24(0xB23F90),48,1}}), "[aircon13] về Cool quạt Cao B23F90");
    r.clear(); r.ac.setPower(false); CHECK(seq(r.ir,{{w24(0xB27BE0),48,1}}), "[aircon13] tắt B27BE0");
    r.clear(); r.ac.setPower(true); CHECK(seq(r.ir,{{w24(0xB23F90),48,1}}), "[aircon13] bật lại giữ quạt Cao B23F90"); }
  // Đổi quạt ở Dry/Auto bị từ chối: không phát IR, không đổi trạng thái
  { Rig r; r.ac.setClimate(2,0,26); r.ac.setPower(true); r.clear();
    CHECK(r.ac.setClimate(2,1,26) != nullptr, "đổi quạt ở Dry lỗi");
    CHECK(r.ac.setClimate(0,1,26) != nullptr, "đổi quạt khi đổi sang Auto lỗi");
    CHECK(r.ac.setClimate(2,3,27) != nullptr, "đổi quạt + nhiệt ở Dry lỗi");
    CHECK(r.ir.log.empty() && r.ac.state().mode == 2 && r.ac.state().fan == 0 && r.ac.state().temp == 26,
          "đổi quạt ở Dry/Auto bị từ chối không đổi gì");
    Rig c; c.ac.setClimate(1,1,26); c.ac.setPower(true); c.clear();
    CHECK(c.ac.setClimate(2,3,26) != nullptr && c.ir.log.empty() && c.ac.state().mode == 1 && c.ac.state().fan == 1,
          "từ Cool sang Dry kèm đổi quạt lỗi");
    CHECK(c.ac.setClimate(0,1,26) == nullptr, "sang Auto giữ nguyên mức quạt (Web UI gửi kèm quạt hiện tại) hợp lệ"); }

  // 3. Hẹn BẬT (aircon3.txt, Cool 26) và hẹn BẬT ở 30°C (aircon2.txt)
  { const int mins[9] = {30,60,90,120,300,600,660,720,1440};
    const uint8_t last[9] = {0x81,0x83,0x85,0x87,0x93,0xA7,0xAB,0xAF,0xDF};
    for (int i = 0; i < 9; i++) { Rig r; coolOn(r); r.ac.setTimers(mins[i], 0);
      uint64_t e = 0xB24DBF40D300ULL | last[i];
      CHECK(seq(r.ir,{{e,48,1}}), "hẹn BẬT 26C"); }
    Rig r; r.ac.setClimate(1,0,30); r.ac.setPower(true); r.clear(); r.ac.setTimers(30,0);
    CHECK(seq(r.ir,{{0xB24DBF40B381ULL,48,1}}), "hẹn BẬT 0.5h 30C"); }

  // 4. Hẹn TẮT (aircon3.txt, Cool 26)
  { const int mins[9] = {30,60,90,120,300,600,660,720,1440};
    const uint64_t e[9] = {0xB24DA15ED0FFULL,0xB24DA35CD0FFULL,0xB24DA55AD0FFULL,0xB24DA758D0FFULL,
      0xB24DB34CD0FFULL,0xB24DA758D1FFULL,0xB24DAB54D1FFULL,0xB24DAF50D1FFULL,0xB24DBF40D2FFULL};
    for (int i = 0; i < 9; i++) { Rig r; coolOn(r); r.ac.setTimers(0, mins[i]);
      CHECK(seq(r.ir,{{e[i],48,1}}), "hẹn TẮT 26C"); }
    Rig r; r.ac.setClimate(1,0,30); r.ac.setPower(true); r.clear(); r.ac.setTimers(0,30);
    CHECK(seq(r.ir,{{0xB24DA15EB0FFULL,48,1}}), "hẹn TẮT 0.5h 30C"); }

  // 5. Chế độ Dry có hẹn giờ (aircon4.txt)
  { Rig r; r.ac.setClimate(2,0,26); r.ac.setPower(true); r.clear();
    r.ac.setTimers(300,0);  CHECK(seq(r.ir,{{0xB24D1FE0D793ULL,48,1}}), "Dry hẹn BẬT 5h");
    r.clear(); r.ac.setTimers(600,0); CHECK(seq(r.ir,{{0xB24D1FE0D7A7ULL,48,1}}), "Dry hẹn BẬT 10h");
    r.clear(); r.ac.setTimers(0,300); CHECK(seq(r.ir,{{0xB24D13ECD4FFULL,48,1}}), "Dry hẹn TẮT 5h");
    r.clear(); r.ac.setTimers(0,600); CHECK(seq(r.ir,{{0xB24D07F8D5FFULL,48,1}}), "Dry hẹn TẮT 10h"); }

  // 5b. Hẹn giờ ở Auto và Fan (aircon18.txt: hẹn BẬT; aircon24.txt: hẹn TẮT)
  { Rig a; a.ac.setClimate(0,0,25); a.ac.setPower(true); a.clear(); a.ac.setTimers(30,0);
    CHECK(seq(a.ir,{{0xB24D1FE0CB81ULL,48,1}}), "Auto 25C hẹn BẬT 0.5h = B24D1FE0CB81 (aircon18)");
    Rig f; f.ac.setClimate(3,2,25); f.ac.setPower(true); f.clear(); f.ac.setTimers(30,0);
    CHECK(seq(f.ir,{{0xB24D5FA0E781ULL,48,1}}), "Fan quạt Vừa hẹn BẬT 0.5h = B24D5FA0E781 (aircon18)");
    Rig a2; a2.ac.setClimate(0,0,25); a2.ac.setPower(true); a2.clear(); a2.ac.setTimers(0,60);
    CHECK(seq(a2.ir,{{0xB24D03FCC8FFULL,48,1}}), "Auto 25C hẹn TẮT 1h = B24D03FCC8FF (aircon24)");
    Rig f2; f2.ac.setClimate(3,1,25); f2.ac.setPower(true); f2.clear(); f2.ac.setTimers(0,60);
    CHECK(seq(f2.ir,{{0xB24D837CE4FFULL,48,1}}), "Fan quạt Thấp hẹn TẮT 1h = B24D837CE4FF (aircon24)"); }

  // 6. Hẹn BẬT 5h + TẮT 10h (aircon5.txt)
  { Rig r; coolOn(r); r.ac.setTimers(300,0); r.clear(); r.ac.setTimers(300,600);
    CHECK(seq(r.ir,{{0xB24DA758D193ULL,48,1}}), "BẬT 5h + TẮT 10h");
    r.clear(); r.ac.setTimers(0,600); CHECK(seq(r.ir,{{0xB24DA758D1FFULL,48,1}}), "hủy BẬT, còn TẮT 10h");
    r.clear(); r.ac.setTimers(300,0); CHECK(seq(r.ir,{{0xB24DBF40D393ULL,48,1}}), "hủy TẮT, còn BẬT 5h");
    r.clear(); r.ac.setTimers(0,0);   CHECK(seq(r.ir,{{w24(0xB2BFD0),48,1}}), "hủy cả hai = khung thường"); }

  // 7. Hẹn TẮT 0.5h + BẬT 1h (aircon11.txt)
  { Rig r; coolOn(r); r.ac.setTimers(60, 30);
    CHECK(seq(r.ir,{{0xB24DA15ED083ULL,48,1}}), "TẮT 0.5h + BẬT 1h = B24DA15ED083 (aircon11)"); }

  // 8. Đổi nhiệt khi đang hẹn TẮT 2h (aircon8.txt)
  { Rig r; coolOn(r); r.ac.setTimers(0,120); r.clear(); r.ac.setClimate(1,0,27);
    CHECK(seq(r.ir,{{0xB24DA75890FFULL,48,1}}), "tăng nhiệt giữ hẹn TẮT 2h = B24DA75890FF");
    CHECK(r.ac.state().off_timer == 120, "hẹn giờ còn sau đổi nhiệt"); }

  // 9. Sleep (aircon.txt, aircon7/9/10)
  { Rig r; coolOn(r); r.ac.setSleep(true);
    CHECK(seq(r.ir,{{w24(0xB2E003),48,0},{w24(0xB2BFD0),48,1}}), "Bật Sleep = B2E003 + B2BFD0 x2");
    r.clear(); r.ac.setClimate(1,0,27);   // aircon7: tăng nhiệt giữ Sleep
    CHECK(seq(r.ir,{{w24(0xB2E003),48,0},{w24(0xB2BF90),48,1}}), "Sleep: tăng nhiệt vẫn có B2E003");
    r.clear(); r.ac.setSleep(false);
    CHECK(seq(r.ir,{{w24(0xB2BF90),48,1}}), "Tắt Sleep = khung thường"); }

  // 9b. Sleep và tốc độ quạt (aircon15/16/17/18/25)
  { // aircon18: quạt Thấp -> bật Sleep: quạt về Auto; tắt Sleep giữ Auto; tăng nhiệt
    Rig r; r.ac.setClimate(1,1,25); r.ac.setPower(true); r.clear(); r.ac.setSleep(true);
    CHECK(seq(r.ir,{{w24(0xB2E003),48,0},{w24(0xB2BFC0),48,1}}), "[aircon18] Sleep khi quạt Thấp = B2E003 + B2BFC0");
    CHECK(r.ac.state().fan == coolix::kFanAuto && r.ac.state().sleep, "bật Sleep đặt quạt Auto");
    r.clear(); r.ac.setSleep(false);
    CHECK(seq(r.ir,{{w24(0xB2BFC0),48,1}}) && r.ac.state().fan == coolix::kFanAuto, "[aircon18] tắt Sleep giữ quạt Auto = B2BFC0");
    r.clear(); r.ac.setClimate(1,0,26);
    CHECK(seq(r.ir,{{w24(0xB2BFD0),48,1}}), "[aircon18] sau tắt Sleep tăng nhiệt B2BFD0"); }
  { // aircon15: Sleep, tăng nhiệt giữ Sleep, đổi quạt tắt Sleep
    Rig r; r.ac.setClimate(1,0,27); r.ac.setPower(true); r.clear(); r.ac.setSleep(true);
    r.clear(); r.ac.setClimate(1,0,28);
    CHECK(seq(r.ir,{{w24(0xB2E003),48,0},{w24(0xB2BF80),48,1}}) && r.ac.state().sleep, "[aircon15] Sleep + tăng nhiệt giữ Sleep");
    r.clear(); r.ac.setClimate(1,1,28);
    CHECK(seq(r.ir,{{w24(0xB29F80),48,1}}), "[aircon15] đổi quạt khi Sleep = B29F80, không có B2E003");
    CHECK(!r.ac.state().sleep, "[aircon15] đổi quạt tắt Sleep"); }
  { // aircon16: chọn lại quạt Auto khi Sleep không tắt Sleep; chọn quạt Thấp thì tắt
    Rig r; r.ac.setClimate(1,0,27); r.ac.setPower(true); r.ac.setSleep(true); r.clear();
    CHECK(r.ac.setClimate(1,0,27) == nullptr && r.ac.state().sleep, "[aircon16] chọn lại quạt Auto giữ Sleep");
    CHECK(seq(r.ir,{{w24(0xB2E003),48,0},{w24(0xB2BF90),48,1}}), "[aircon16] khung vẫn có tiền tố Sleep");
    r.clear(); r.ac.setClimate(1,1,27);
    CHECK(seq(r.ir,{{w24(0xB29F90),48,1}}) && !r.ac.state().sleep, "[aircon16] chọn quạt Thấp = B29F90, tắt Sleep"); }
  { // aircon17: Cool quạt Thấp -> Auto (mode) -> Sleep -> về Cool -> tăng nhiệt
    Rig r; r.ac.setClimate(1,1,27); r.ac.setPower(true); r.clear();
    CHECK(r.ac.setClimate(0,1,27) == nullptr && seq(r.ir,{{w24(0xB21F98),48,1}}), "[aircon17] sang Auto = B21F98");
    r.clear(); r.ac.setSleep(true);
    CHECK(seq(r.ir,{{w24(0xB2E003),48,0},{w24(0xB21F98),48,1}}), "[aircon17] Sleep ở Auto = B2E003 + B21F98");
    CHECK(r.ac.state().fan == coolix::kFanAuto, "[aircon17] Sleep ở Auto đặt quạt Auto");
    r.clear(); r.ac.setClimate(1,0,27);
    CHECK(seq(r.ir,{{w24(0xB2BF90),48,1}}) && !r.ac.state().sleep, "[aircon17] về Cool: quạt Auto B2BF90, Sleep tắt");
    r.clear(); r.ac.setClimate(1,0,28);
    CHECK(seq(r.ir,{{w24(0xB2BF80),48,1}}), "[aircon17] tăng nhiệt B2BF80"); }

  // 10. Sleep + hẹn TẮT 2h (aircon9/10.txt)
  { Rig r; coolOn(r); r.ac.setTimers(0,120); r.clear(); r.ac.setSleep(true);
    CHECK(seq(r.ir,{{w24(0xB2E003),48,0},{0xB24DA758D0FFULL,48,1}}), "Sleep + hẹn TẮT = B2E003 + B24DA758D0FF x2");
    CHECK(r.ac.state().off_timer == 120, "Sleep không hủy hẹn giờ");
    r.clear(); r.ac.setSleep(false);
    CHECK(seq(r.ir,{{0xB24DA758D0FFULL,48,1}}), "Tắt Sleep giữ hẹn giờ"); }

  // 11. Eco (aircon.txt, aircon2.txt, aircon9.txt)
  { Rig r; coolOn(r); r.ac.setEco(true);
    CHECK(seq(r.ir,{{0xB54AF50A82D0ULL,48,1}}), "Bật Eco 26C = B54AF50A82D0");
    r.clear(); r.ac.setEco(false);
    CHECK(seq(r.ir,{{0xB54AF50A83D0ULL,48,1}}), "Tắt Eco 26C = B54AF50A83D0");
    Rig r2; r2.ac.setClimate(1,0,30); r2.ac.setPower(true); r2.clear(); r2.ac.setEco(true);
    CHECK(seq(r2.ir,{{0xB54AF50A82B0ULL,48,1}}), "Bật Eco 30C = B54AF50A82B0");
    r2.clear(); r2.ac.setEco(false);
    CHECK(seq(r2.ir,{{0xB54AF50A83B0ULL,48,1}}), "Tắt Eco 30C = B54AF50A83B0"); }

  // 12. Eco rồi tăng nhiệt giữ Eco (aircon9.txt)
  { Rig r; coolOn(r); r.ac.setEco(true); r.clear(); r.ac.setClimate(1,0,27);
    CHECK(seq(r.ir,{{w24(0xB2BF90),48,1}}), "Eco + tăng nhiệt = B2BF90");
    CHECK(r.ac.state().eco, "Eco còn sau đổi nhiệt"); }

  // 12b. Eco và tốc độ quạt (aircon12/14/19/20/21/27)
  { // Bật Eco đặt quạt về Auto, khung BẬT không mang quạt (aircon12: quạt Thấp, Eco B54AF50A82D0, tăng nhiệt B2BF90)
    Rig r; r.ac.setClimate(1,1,26); r.ac.setPower(true); r.clear(); r.ac.setEco(true);
    CHECK(seq(r.ir,{{0xB54AF50A82D0ULL,48,1}}) && r.ac.state().fan == coolix::kFanAuto, "[aircon12] Eco khi quạt Thấp: 82D0, quạt về Auto");
    r.clear(); r.ac.setClimate(1,0,27);
    CHECK(seq(r.ir,{{w24(0xB2BF90),48,1}}), "[aircon12] tăng nhiệt sau Eco mang quạt Auto B2BF90"); }
  { // aircon19: tắt Eco mang mã quạt hiện tại, 25C; 83C0..83C3
    for (int f = 0; f <= 3; f++) {
      Rig r; r.ac.setClimate(1,0,25); r.ac.setPower(true); r.ac.setEco(true);
      if (f != 0) r.ac.setClimate(1,f,25);
      r.clear(); r.ac.setEco(false);
      CHECK(seq(r.ir,{{0xB54AF50A83C0ULL + f,48,1}}) && r.ac.state().fan == f && !r.ac.state().eco, "[aircon19] tắt Eco mang mã quạt 83C0+quạt"); } }
  { // aircon19 (a): chỉ tăng nhiệt rồi tắt Eco = 83D0
    Rig r; r.ac.setClimate(1,0,25); r.ac.setPower(true); r.ac.setEco(true); r.ac.setClimate(1,0,26); r.clear(); r.ac.setEco(false);
    CHECK(seq(r.ir,{{0xB54AF50A83D0ULL,48,1}}), "[aircon19a] Eco, tăng nhiệt, tắt Eco = 83D0"); }
  { // aircon27: Eco, quạt Vừa, tăng nhiệt, tắt Eco = 83D2
    Rig r; r.ac.setClimate(1,0,25); r.ac.setPower(true); r.ac.setEco(true);
    r.clear(); r.ac.setClimate(1,2,25);
    CHECK(seq(r.ir,{{w24(0xB25FC0),48,1}}) && r.ac.state().eco, "[aircon27] đổi quạt Vừa trong Eco = B25FC0, Eco còn");
    r.clear(); r.ac.setClimate(1,2,26);
    CHECK(seq(r.ir,{{w24(0xB25FD0),48,1}}) && r.ac.state().eco, "[aircon27] tăng nhiệt trong Eco = B25FD0, Eco còn");
    r.clear(); r.ac.setEco(false);
    CHECK(seq(r.ir,{{0xB54AF50A83D2ULL,48,1}}), "[aircon27] tắt Eco = B54AF50A83D2"); }
  { // aircon20/21: đổi quạt nhiều lần trong Eco, Eco không tắt
    Rig r; r.ac.setClimate(1,0,25); r.ac.setPower(true); r.ac.setEco(true);
    const int fans[5] = {1,2,3,0,1};
    const uint32_t exp[5] = {0xB29FC0,0xB25FC0,0xB23FC0,0xB2BFC0,0xB29FC0};
    for (int i = 0; i < 5; i++) { r.clear(); r.ac.setClimate(1,fans[i],25);
      CHECK(seq(r.ir,{{w24(exp[i]),48,1}}) && r.ac.state().eco, "[aircon20] đổi quạt trong Eco gửi khung thường, Eco còn"); }
    r.ac.setPower(false); CHECK(!r.ac.state().eco, "[aircon20] tắt máy xóa Eco");
    r.clear(); r.ac.setPower(true);
    CHECK(seq(r.ir,{{w24(0xB29FC0),48,1}}), "[aircon20] bật lại giữ quạt cuối (Thấp) B29FC0"); }
  { // aircon20: Eco, đổi quạt, chọn lại Auto, tắt máy, bật lại = B2BFC0
    Rig r; r.ac.setClimate(1,0,25); r.ac.setPower(true); r.ac.setEco(true);
    r.ac.setClimate(1,1,25); r.ac.setClimate(1,0,25); r.ac.setPower(false); r.clear(); r.ac.setPower(true);
    CHECK(seq(r.ir,{{w24(0xB2BFC0),48,1}}), "[aircon20] chọn lại Auto rồi tắt/bật máy = B2BFC0"); }

  // 12c. aircon28: Cool, quạt Thấp 26C -> Turbo -> Eco -> tắt Eco -> tắt máy
  { Rig r; r.ac.setClimate(1,1,26); r.ac.setPower(true); r.ac.setTurbo(true); r.clear(); r.ac.setEco(true);
    CHECK(seq(r.ir,{{0xB54AF50A82D0ULL,48,1}}) && r.ac.state().eco && !r.ac.state().turbo && r.ac.state().fan == coolix::kFanAuto,
          "[aircon28] Turbo rồi Eco = B54AF50A82D0, quạt về Auto");
    r.clear(); r.ac.setEco(false);
    CHECK(seq(r.ir,{{0xB54AF50A83D0ULL,48,1}}) && !r.ac.state().eco && r.ac.state().fan == coolix::kFanAuto,
          "[aircon28] tắt Eco = B54AF50A83D0 (mã quạt Auto, không phải 83D1)");
    r.clear(); r.ac.setPower(false);
    CHECK(seq(r.ir,{{w24(0xB27BE0),48,1}}), "[aircon28] tắt máy = B27BE0"); }

  // 13. aircon11.txt: Eco -> Sleep -> Eco
  { Rig r; coolOn(r);
    r.ac.setEco(true);
    CHECK(seq(r.ir,{{0xB54AF50A82D0ULL,48,1}}), "[aircon11] Bật Eco");
    r.clear(); r.ac.setSleep(true);
    CHECK(seq(r.ir,{{w24(0xB2E003),48,0},{w24(0xB2BFD0),48,1}}), "[aircon11] Bật Sleep = BOSCH144 3 khung");
    CHECK(!r.ac.state().eco && r.ac.state().sleep, "Sleep tắt Eco");
    r.clear(); r.ac.setEco(true);
    CHECK(seq(r.ir,{{0xB54AF50A82D0ULL,48,1}}), "[aircon11] Bật Eco lần 2 = 1 khung eco x2, KHÔNG có B2E003");
    CHECK(r.ac.state().eco && !r.ac.state().sleep, "Eco tắt Sleep");
    r.clear(); r.ac.setClimate(1,0,27);
    CHECK(seq(r.ir,{{w24(0xB2BF90),48,1}}), "sau Eco: không còn tiền tố B2E003"); }

  // 14. Đổi chế độ xóa Sleep/Eco, giữ hẹn giờ
  { Rig r; coolOn(r); r.ac.setSleep(true); r.ac.setClimate(0,0,26);
    CHECK(!r.ac.state().sleep, "đổi sang Auto xóa Sleep");
    Rig r2; coolOn(r2); r2.ac.setEco(true); r2.ac.setClimate(2,0,26);
    CHECK(!r2.ac.state().eco, "đổi sang Dry xóa Eco");
    Rig r3; coolOn(r3); r3.ac.setTimers(0,120); r3.ac.setSleep(true); r3.clear(); r3.ac.setClimate(2,0,26);
    CHECK(seq(r3.ir,{{0xB24D07F8D4FFULL,48,1}}), "đổi sang Dry khi hẹn TẮT 2h = B24D07F8D4FF, không B2E003");
    CHECK(r3.ac.state().off_timer == 120 && !r3.ac.state().sleep, "giữ hẹn giờ, xóa Sleep"); }

  // 15. Tắt máy xóa tất cả (aircon.txt: B27BE0)
  { Rig r; coolOn(r); r.ac.setTimers(60,120); r.ac.setSleep(true); r.clear(); r.ac.setPower(false);
    CHECK(seq(r.ir,{{w24(0xB27BE0),48,1}}), "Tắt máy = B27BE0");
    const AcState& s = r.ac.state();
    CHECK(!s.power && !s.sleep && !s.eco && s.on_timer==0 && s.off_timer==0, "Tắt máy xóa Sleep/Eco/hẹn giờ");
    r.clear(); r.ac.setPower(true);
    CHECK(seq(r.ir,{{w24(0xB2BFD0),48,1}}), "Bật lại = khung thường"); }

  // 16. Bật máy khi đang hẹn giờ hủy hẹn giờ
  { Rig r; coolOn(r); r.ac.setTimers(0,120); r.clear(); r.ac.setPower(true);
    CHECK(seq(r.ir,{{w24(0xB2BFD0),48,1}}) && r.ac.state().off_timer==0, "Bật = hủy hẹn giờ"); }

  // 17. Swing, máy tắt không gửi khi đổi cài đặt
  { Rig r; r.ac.swing(); CHECK(seq(r.ir,{{w24(0xB26BE0),48,1}}), "Swing = B26BE0");
    Rig r2; r2.clear(); r2.ac.setClimate(1,0,24); CHECK(r2.ir.log.empty(), "máy tắt: đổi cài đặt không phát IR"); }

  // 18. Lỗi: báo lỗi, không phát IR, không đổi trạng thái
  { Rig r; coolOn(r);
    CHECK(r.ac.setClimate(4,0,26)!=nullptr, "mode=4 lỗi");
    CHECK(r.ac.setClimate(1,4,26)!=nullptr, "fan=4 lỗi");
    CHECK(r.ac.setClimate(1,0,31)!=nullptr, "temp=31 lỗi");
    CHECK(r.ac.setClimate(1,0,16)!=nullptr, "temp=16 lỗi");
    CHECK(r.ac.setTimers(45,0)!=nullptr, "hẹn 45 phút lỗi");
    CHECK(r.ac.setTimers(0,630)!=nullptr, "hẹn 630 phút lỗi");
    CHECK(r.ac.setTimers(1500,0)!=nullptr, "hẹn 1500 phút lỗi");
    CHECK(r.ac.setTimers(0,-30)!=nullptr, "hẹn âm lỗi");
    CHECK(r.ir.log.empty(), "lỗi không phát IR");
    CHECK(r.ac.setTimers(660,0)==nullptr, "hẹn 660 hợp lệ");
    Rig a; CHECK(a.ac.setSleep(true)!=nullptr, "Sleep khi máy tắt lỗi");
    CHECK(a.ac.setEco(true)!=nullptr, "Eco khi máy tắt lỗi");
    Rig d; d.ac.setClimate(2,0,26); d.ac.setPower(true);
    CHECK(d.ac.setSleep(true)!=nullptr, "Sleep ở Dry lỗi");
    CHECK(d.ac.setEco(true)!=nullptr, "Eco ở Dry lỗi");
    Rig u; u.ac.setClimate(0,0,26); u.ac.setPower(true);
    CHECK(u.ac.setSleep(true)==nullptr, "Sleep ở Auto hợp lệ");
    CHECK(u.ac.setEco(true)!=nullptr, "Eco ở Auto lỗi"); }

  // 19. NVS: lưu rồi nạp lại đủ 8 trường; khởi động không phát IR; dữ liệu hỏng -> mặc định
  { Rig r; coolOn(r); r.ac.setTimers(0,120); r.ac.setSleep(true);
    IRsend ir2; AcController b; b.begin(ir2, testNow);   // không xóa kho NVS: giống khởi động lại
    const AcState& s = b.state();
    CHECK(s.power && s.mode==1 && s.fan==0 && s.temp==26 && s.sleep && !s.eco && s.on_timer==0 && s.off_timer==120,
          "NVS: nạp lại power/mode/fan/temp/sleep/hẹn giờ");
    CHECK(ir2.log.empty(), "khởi động không phát IR");
    Rig e; coolOn(e); e.ac.setEco(true);
    IRsend ir3; AcController b3; b3.begin(ir3, testNow);
    CHECK(b3.state().eco && !b3.state().sleep, "NVS: nạp lại Eco");
    Rig h; coolOn(h); h.ac.setTimers(90,0);
    IRsend ir4; AcController b4; b4.begin(ir4, testNow);
    CHECK(b4.state().on_timer == 90 && b4.state().off_timer == 0, "NVS: nạp lại hẹn BẬT");
    Rig bad; { Preferences p; p.begin("ac", false); p.putBool("power", true); p.putUChar("temp", 99); }
    IRsend ir5; AcController b5; b5.begin(ir5, testNow);
    CHECK(!b5.state().power && b5.state().temp == AC_TEMP_DEFAULT && b5.state().mode == coolix::kModeAuto,
          "NVS hỏng (temp=99) -> dùng mặc định cả bộ, không dùng nửa vời");
    Rig w; w.ac.setClimate(1,3,26); w.ac.setClimate(2,3,26);
    IRsend ir6; AcController b6; b6.begin(ir6, testNow);
    CHECK(b6.state().mode == 2 && b6.state().fan == 3, "NVS: quạt đã chọn được giữ khi ở Dry"); }

  // 20. Máy tắt đang hẹn BẬT, đổi nhiệt -> gửi lại khung hẹn BẬT (aircon22.txt)
  { Rig r; r.ac.setClimate(1,0,26); r.ac.setTimers(60,0);
    CHECK(seq(r.ir,{{0xB24DBF40D383ULL,48,1}}), "máy tắt + hẹn BẬT 1h = B24DBF40D383");
    r.clear(); r.ac.setClimate(1,0,27);
    CHECK(seq(r.ir,{{0xB24DBF409383ULL,48,1}}), "máy tắt + hẹn BẬT, tăng nhiệt = B24DBF409383");
    CHECK(!r.ac.state().power && r.ac.state().on_timer == 60, "máy vẫn tắt, hẹn BẬT còn");
    // aircon22 nguyên văn: tắt máy -> hẹn BẬT 1h -> tăng độ -> giảm độ (Cool, quạt Thấp, 25C)
    Rig a; a.ac.setClimate(1,1,25); a.ac.setPower(true); a.ac.setPower(false); a.clear();
    a.ac.setTimers(60,0);
    CHECK(seq(a.ir,{{0xB24D9F60C383ULL,48,1}}), "[aircon22] tắt máy + hẹn BẬT 1h = B24D9F60C383");
    a.clear(); a.ac.setClimate(1,1,26);
    CHECK(seq(a.ir,{{0xB24D9F60D383ULL,48,1}}), "[aircon22] tăng độ = B24D9F60D383");
    a.clear(); a.ac.setClimate(1,1,25);
    CHECK(seq(a.ir,{{0xB24D9F60C383ULL,48,1}}), "[aircon22] giảm độ = B24D9F60C383");
    CHECK(!a.ac.state().power && a.ac.state().on_timer == 60, "[aircon22] máy vẫn tắt, hẹn BẬT còn"); }

  // 21. Light (aircon.txt: B5F5A5): lệnh đảo, phát được cả khi máy tắt, không đổi trạng thái
  { Rig r;   // máy tắt
    CHECK(r.ac.light() == nullptr, "Light khi máy tắt hợp lệ");
    CHECK(seq(r.ir, {{0xB54AF50AA55AULL,48,1}}), "Light = B5F5A5 x2 (máy tắt)");
    CHECK(!r.ac.state().power, "Light không bật máy");
    Rig r2; coolOn(r2); r2.ac.setTimers(0,120); r2.ac.setEco(true); r2.clear();
    CHECK(r2.ac.light() == nullptr && seq(r2.ir, {{0xB54AF50AA55AULL,48,1}}), "Light khi máy chạy = B5F5A5 x2");
    const AcState& s = r2.ac.state();
    CHECK(s.power && s.eco && s.off_timer == 120 && s.mode == 1 && s.temp == 26, "Light không đổi cài đặt/Eco/hẹn giờ"); }

  // 22. Turbo (aircon.txt: B5F5A2; quy luật do người dùng thử trên remote)
  { Rig r; coolOn(r);
    CHECK(r.ac.setTurbo(true) == nullptr && seq(r.ir, {{0xB54AF50AA25DULL,48,1}}), "Bật Turbo = B5F5A2 x2");
    CHECK(r.ac.state().turbo, "cờ Turbo bật");
    r.clear(); CHECK(r.ac.setTurbo(true) == nullptr && r.ir.log.empty(), "bật Turbo khi đã bật: không phát (lệnh đảo)");
    r.clear(); CHECK(r.ac.setTurbo(false) == nullptr && seq(r.ir, {{0xB54AF50AA25DULL,48,1}}), "Tắt Turbo = B5F5A2 x2");
    CHECK(!r.ac.state().turbo, "cờ Turbo tắt");
    r.clear(); CHECK(r.ac.setTurbo(false) == nullptr && r.ir.log.empty(), "tắt Turbo khi đã tắt: không phát"); }
  { Rig a; CHECK(a.ac.setTurbo(true) != nullptr && a.ir.log.empty() && !a.ac.state().turbo, "Turbo khi máy tắt lỗi");
    Rig c; c.ac.setClimate(1,0,26); CHECK(c.ac.setTurbo(true) != nullptr && c.ir.log.empty() && !c.ac.state().turbo,
                                          "Turbo khi máy tắt (đã chọn Cool) vẫn lỗi");
    const int modes[3] = {0, 2, 3};
    for (int i = 0; i < 3; i++) { Rig b; b.ac.setClimate(modes[i],0,26); b.ac.setPower(true); b.clear();
      CHECK(b.ac.setTurbo(true) != nullptr && b.ir.log.empty() && !b.ac.state().turbo, "Turbo ngoài Cool lỗi"); } }
  // Bật Turbo tắt Sleep/Eco; bật Sleep/Eco tắt Turbo (aircon27 mục 2, aircon25)
  { Rig r; coolOn(r); r.ac.setSleep(true); r.clear(); r.ac.setTurbo(true);
    CHECK(seq(r.ir, {{0xB54AF50AA25DULL,48,1}}), "Turbo sau Sleep: chỉ khung Turbo, không có B2E003");
    CHECK(r.ac.state().turbo && !r.ac.state().sleep && !r.ac.state().eco, "Bật Turbo tắt Sleep");
    r.clear(); r.ac.setSleep(true);
    CHECK(seq(r.ir, {{w24(0xB2E003),48,0},{w24(0xB2BFD0),48,1}}), "Bật Sleep khi Turbo = B2E003 + khung thường");
    CHECK(r.ac.state().sleep && !r.ac.state().turbo, "Bật Sleep tắt Turbo");
    Rig e; coolOn(e); e.ac.setEco(true); e.clear(); e.ac.setTurbo(true);
    CHECK(seq(e.ir, {{0xB54AF50AA25DULL,48,1}}) && e.ac.state().turbo && !e.ac.state().eco, "Bật Turbo tắt Eco");
    e.clear(); e.ac.setEco(true);
    CHECK(seq(e.ir, {{0xB54AF50A82D0ULL,48,1}}) && e.ac.state().eco && !e.ac.state().turbo, "Bật Eco tắt Turbo"); }
  // aircon25: Turbo rồi Sleep (quạt Thấp): B2E003 + B2BFC0; Turbo rồi Eco: 82C0
  { Rig r; r.ac.setClimate(1,1,25); r.ac.setPower(true); r.ac.setTurbo(true); r.clear(); r.ac.setSleep(true);
    CHECK(seq(r.ir, {{w24(0xB2E003),48,0},{w24(0xB2BFC0),48,1}}), "[aircon25] Turbo rồi Sleep (quạt Thấp) = B2E003 + B2BFC0");
    CHECK(!r.ac.state().turbo && r.ac.state().fan == coolix::kFanAuto, "[aircon25] Sleep tắt Turbo, quạt Auto");
    Rig e; e.ac.setClimate(1,1,25); e.ac.setPower(true); e.ac.setTurbo(true); e.clear(); e.ac.setEco(true);
    CHECK(seq(e.ir, {{0xB54AF50A82C0ULL,48,1}}) && !e.ac.state().turbo && e.ac.state().eco, "[aircon25] Turbo rồi Eco = B54AF50A82C0"); }
  // Đổi nhiệt giữ Turbo; đổi quạt hoặc chế độ tắt Turbo
  { Rig r; coolOn(r); r.ac.setTurbo(true); r.clear(); r.ac.setClimate(1,0,27);
    CHECK(seq(r.ir, {{w24(0xB2BF90),48,1}}) && r.ac.state().turbo, "Turbo + tăng nhiệt: khung thường, giữ Turbo");
    r.ac.setClimate(1,0,27); CHECK(r.ac.state().turbo, "đặt lại cùng quạt/chế độ: giữ Turbo");
    r.ac.setClimate(1,1,27); CHECK(!r.ac.state().turbo, "đổi quạt tắt Turbo");
    Rig m; coolOn(m); m.ac.setTurbo(true); m.ac.setClimate(2,0,26); CHECK(!m.ac.state().turbo, "đổi chế độ sang Dry tắt Turbo");
    Rig a; coolOn(a); a.ac.setTurbo(true); a.ac.setClimate(0,0,26); CHECK(!a.ac.state().turbo, "đổi chế độ sang Auto tắt Turbo");
    // Bật Turbo không đổi quạt
    Rig q; q.ac.setClimate(1,1,25); q.ac.setPower(true); q.ac.setTurbo(true);
    CHECK(q.ac.state().fan == coolix::kFanLow, "bật Turbo không đổi quạt"); }
  // Turbo không ảnh hưởng hẹn giờ; hẹn giờ khi Turbo giữ Turbo (aircon26/27)
  { Rig r; coolOn(r); r.ac.setTimers(0,120); r.clear(); r.ac.setTurbo(true);
    CHECK(seq(r.ir, {{0xB54AF50AA25DULL,48,1}}) && r.ac.state().off_timer == 120, "bật Turbo khi hẹn TẮT: chỉ khung Turbo, giữ hẹn giờ");
    r.ac.setTurbo(false); CHECK(r.ac.state().off_timer == 120, "tắt Turbo giữ hẹn giờ");
    Rig q; q.ac.setClimate(1,1,25); q.ac.setPower(true); q.ac.setTurbo(true); q.clear(); q.ac.setTimers(0,60);
    CHECK(seq(q.ir, {{0xB24D837CC0FFULL,48,1}}), "[aircon26] hẹn TẮT 1h khi Turbo (quạt Thấp 25C) = B24D837CC0FF");
    CHECK(q.ac.state().turbo && q.ac.state().off_timer == 60, "[aircon26/27] đặt hẹn giờ khi Turbo giữ Turbo"); }
  // Tắt máy xóa Turbo; bật lại không còn Turbo
  { Rig r; coolOn(r); r.ac.setTurbo(true); r.ac.setPower(false);
    CHECK(!r.ac.state().turbo, "tắt máy xóa Turbo");
    r.clear(); r.ac.setPower(true);
    CHECK(seq(r.ir, {{w24(0xB2BFD0),48,1}}) && !r.ac.state().turbo, "bật lại: khung thường, không còn Turbo"); }

  // 23. Clean (aircon.txt: B5F5AA; quy luật do người dùng thử trên remote)
  CHECK(AC_CLEAN_DURATION_MS == 20UL * 60UL * 1000UL, "Clean chạy 20 phút (người dùng đo lại trên remote, aircon26)");
  { Rig a; CHECK(a.ac.setClean(true) != nullptr && a.ir.log.empty() && !a.ac.state().clean, "Clean khi máy tắt lỗi");
    Rig r; coolOn(r);
    CHECK(r.ac.setClean(true) == nullptr && seq(r.ir, {{0xB54AF50AAA55ULL,48,1}}), "Bật Clean = B5F5AA x2");
    CHECK(r.ac.state().clean && r.ac.state().power, "Clean: cờ bật, máy vẫn tính là đang chạy");
    r.clear(); CHECK(r.ac.setClean(true) == nullptr && r.ir.log.empty(), "bật Clean khi đang Clean: không phát"); }
  // Bật Clean xóa hẹn giờ, Sleep, Eco, Turbo
  { Rig r; coolOn(r); r.ac.setTimers(60,120); r.ac.setSleep(true); r.clear(); r.ac.setClean(true);
    const AcState& s = r.ac.state();
    CHECK(seq(r.ir, {{0xB54AF50AAA55ULL,48,1}}), "Clean khi có hẹn giờ + Sleep: chỉ khung Clean");
    CHECK(s.on_timer == 0 && s.off_timer == 0 && !s.sleep && s.clean, "Clean xóa hẹn BẬT/TẮT và Sleep");
    Rig e; coolOn(e); e.ac.setEco(true); e.ac.setClean(true); CHECK(!e.ac.state().eco, "Clean xóa Eco");
    Rig t; coolOn(t); t.ac.setTurbo(true); t.ac.setClean(true); CHECK(!t.ac.state().turbo, "Clean xóa Turbo"); }
  // Trong lúc Clean mọi lệnh khác (trừ Light) bị từ chối, không phát IR, không đổi trạng thái
  { Rig r; coolOn(r); r.ac.setClean(true); r.clear();
    CHECK(r.ac.setPower(true) != nullptr, "Clean: Bật nguồn bị từ chối");
    CHECK(r.ac.setClimate(1,0,27) != nullptr, "Clean: đổi nhiệt bị từ chối");
    CHECK(r.ac.setClimate(0,0,26) != nullptr, "Clean: đổi chế độ bị từ chối");
    CHECK(r.ac.setTimers(0,60) != nullptr, "Clean: hẹn giờ bị từ chối");
    CHECK(r.ac.setSleep(true) != nullptr, "Clean: bật Sleep bị từ chối");
    CHECK(r.ac.setSleep(false) != nullptr, "Clean: tắt Sleep bị từ chối");
    CHECK(r.ac.setEco(true) != nullptr, "Clean: bật Eco bị từ chối");
    CHECK(r.ac.setEco(false) != nullptr, "Clean: tắt Eco bị từ chối");
    CHECK(r.ac.setTurbo(true) != nullptr, "Clean: bật Turbo bị từ chối");
    CHECK(r.ac.setTurbo(false) != nullptr, "Clean: tắt Turbo bị từ chối");
    CHECK(r.ac.swing() != nullptr, "Clean: Swing bị từ chối");
    CHECK(r.ir.log.empty(), "Clean: lệnh bị từ chối không phát IR");
    const AcState& s = r.ac.state();
    CHECK(s.power && s.clean && s.mode == 1 && s.fan == 0 && s.temp == 26 && !s.sleep && !s.eco && !s.turbo &&
          s.on_timer == 0 && s.off_timer == 0, "Clean: bị từ chối không đổi trạng thái");
    // Light vẫn phát được trong Clean (aircon26/27), không đổi trạng thái
    CHECK(r.ac.light() == nullptr && seq(r.ir, {{0xB54AF50AA55AULL,48,1}}), "Clean: Light phát được = B5F5A5 x2");
    CHECK(s.power && s.clean && s.mode == 1 && s.fan == 0 && s.temp == 26 && !s.sleep && !s.eco && !s.turbo &&
          s.on_timer == 0 && s.off_timer == 0, "Clean: Light không đổi trạng thái"); }
  // Tắt nguồn trong lúc Clean
  { Rig r; coolOn(r); r.ac.setClean(true); r.clear();
    CHECK(r.ac.setPower(false) == nullptr && seq(r.ir, {{w24(0xB27BE0),48,1}}), "Tắt nguồn khi Clean = B27BE0");
    CHECK(!r.ac.state().clean && !r.ac.state().power, "Tắt nguồn khi Clean xóa cờ Clean");
    r.clear(); CHECK(r.ac.setPower(true) == nullptr && seq(r.ir, {{w24(0xB2BFD0),48,1}}), "sau đó Bật nguồn lại bình thường"); }
  // Bấm Clean lần hai (dừng giữa chừng): máy tắt hẳn
  { Rig r; coolOn(r); r.ac.setClean(true); r.clear();
    CHECK(r.ac.setClean(false) == nullptr && seq(r.ir, {{0xB54AF50AAA55ULL,48,1}}), "Dừng Clean = B5F5AA x2");
    CHECK(!r.ac.state().clean && !r.ac.state().power, "Dừng Clean: máy tắt hẳn");
    Rig n; coolOn(n); CHECK(n.ac.setClean(false) == nullptr && n.ir.log.empty() && n.ac.state().power,
                            "tắt Clean khi không Clean: không phát, không đổi"); }
  // Hết 20 phút: ESP32 tự đặt máy tắt, không phát IR, lưu NVS
  { Rig r; coolOn(r); g_millis = 1000; r.ac.setClean(true); r.clear();
    g_millis = 1000 + AC_CLEAN_DURATION_MS - 1; r.ac.update();
    CHECK(r.ac.state().clean && r.ac.state().power, "19:59: còn Clean");
    g_millis = 1000 + AC_CLEAN_DURATION_MS; r.ac.update();
    CHECK(!r.ac.state().clean && !r.ac.state().power && r.ir.log.empty(), "20:00: hết Clean, máy tắt, không phát IR");
    IRsend ir2; AcController b; b.begin(ir2, testNow);
    CHECK(!b.state().power && !b.state().clean, "NVS: sau Clean hết giờ máy tắt");
    r.clear(); CHECK(r.ac.setPower(true) == nullptr && seq(r.ir, {{w24(0xB2BFD0),48,1}}), "sau Clean hết giờ: Bật nguồn bình thường");
    Rig p; coolOn(p); g_millis = 99999999UL; p.ac.update();
    CHECK(p.ac.state().power, "update() khi không Clean: không đổi gì");
    Rig w; coolOn(w); g_millis = 0xFFFFFFFFu - 1000u; w.ac.setClean(true);
    g_millis += AC_CLEAN_DURATION_MS - 1; w.ac.update();
    CHECK(w.ac.state().clean, "millis() tràn số: 19:59 vẫn còn Clean");
    g_millis += 1; w.ac.update();
    CHECK(!w.ac.state().clean && !w.ac.state().power, "millis() tràn số: 20:00 hết Clean");
    g_millis = 0; }
  // Clean không lưu NVS: khởi động lại thì cờ Clean mất
  { Rig r; coolOn(r); r.ac.setClean(true);
    IRsend ir2; AcController b; b.begin(ir2, testNow);
    CHECK(!b.state().clean && b.state().power && ir2.log.empty(), "khởi động lại: cờ Clean mất, không phát IR"); }

  // 24. NVS: Turbo lưu/nạp; Turbo không hợp lệ -> mặc định cả bộ
  { Rig r; coolOn(r); r.ac.setTurbo(true);
    IRsend i1; AcController b1; b1.begin(i1, testNow);
    CHECK(b1.state().turbo && b1.state().power && b1.state().mode == 1 && i1.log.empty(), "NVS: nạp lại Turbo");
    Rig x1; { Preferences p; p.begin("ac", false); p.putBool("turbo", true); }
    IRsend i2; AcController b2; b2.begin(i2, testNow);
    CHECK(!b2.state().turbo && !b2.state().power, "NVS: Turbo khi máy tắt -> mặc định");
    Rig x2; { Preferences p; p.begin("ac", false); p.putBool("power", true); p.putUChar("mode", 2); p.putBool("turbo", true); }
    IRsend i3; AcController b3; b3.begin(i3, testNow);
    CHECK(!b3.state().turbo && !b3.state().power, "NVS: Turbo ở Dry -> mặc định");
    Rig x3; { Preferences p; p.begin("ac", false); p.putBool("power", true); p.putUChar("mode", 1); p.putBool("sleep", true); p.putBool("turbo", true); }
    IRsend i4; AcController b4; b4.begin(i4, testNow);
    CHECK(!b4.state().turbo && !b4.state().sleep, "NVS: Turbo + Sleep -> mặc định"); }

  // ===========================================================================
  // HẸN GIỜ THEO GIỜ THỰC (NTP, UTC+7). Nhãn nguồn: [người dùng 2026-11-05] = người dùng thử trên
  // máy thật/remote; [suy ra] = suy luận, chưa có tín hiệu thu; [thiết kế] = thiết kế đã được xác nhận.
  // ===========================================================================
  CHECK(TIMEZONE_OFFSET_SEC == 7 * 3600, "múi giờ Việt Nam UTC+7, không giờ mùa hè");
  CHECK(CLOCK_MIN_VALID_EPOCH == 1767225600UL && L(0,0,0) > CLOCK_MIN_VALID_EPOCH, "mốc giờ hợp lệ = 2026-01-01 00:00 UTC");

  // 25. Hẹn giờ cần giờ thực: chưa có giờ thì từ chối, không phát IR, không đổi trạng thái; hủy thì không cần giờ [thiết kế]
  { Rig r; coolOn(r); g_epoch = 0;
    CHECK(r.ac.setTimers(0,60) != nullptr && r.ir.log.empty() && r.ac.state().off_timer == 0 && r.ac.state().t_start == 0,
          "chưa có giờ: hẹn Sau X giờ bị từ chối, không phát IR");
    CHECK(r.ac.setSchedule(385,-1) != nullptr && r.ir.log.empty() && r.ac.state().on_at == 0,
          "chưa có giờ: hẹn Lúc hh:mm bị từ chối");
    CHECK(r.ac.setTimers(0,0) == nullptr, "chưa có giờ: setTimers(0,0) (hủy) vẫn được");
    CHECK(r.ac.cancelTimers() == nullptr, "chưa có giờ: cancelTimers vẫn được");
    const TimerInfo ti = r.ac.timerInfo();
    CHECK(ti.clock == -1 && ti.on_due == -1 && ti.off_due == -1 && ti.kind == 0, "chưa có giờ: timerInfo clock = -1"); }

  // 26. Hẹn "Sau X giờ": lưu t_start; đến hạn ESP32 cập nhật power, KHÔNG phát IR [thiết kế]
  { Rig r; coolOn(r); r.ac.setTimers(0,60);
    CHECK(r.ac.state().t_start == kT0, "setTimers lưu t_start = giờ thực lúc gửi khung hẹn giờ");
    r.clear();
    at(r, kT0 + 3599);
    CHECK(r.ac.state().power && r.ac.state().off_timer == 60 && r.ac.state().t_start == kT0, "hẹn Tắt 1h: 59:59 chưa tới hạn");
    at(r, kT0 + 3600);
    CHECK(!r.ac.state().power && r.ac.state().off_timer == 0 && r.ac.state().t_start == 0 && r.ir.log.empty(),
          "hẹn Tắt 1h: tới hạn máy tắt, hết hẹn, không phát IR");
    r.clear(); r.ac.setPower(true);
    CHECK(seq(r.ir,{{w24(0xB2BFD0),48,1}}), "sau hẹn Tắt: Bật = khung thường (không còn hẹn giờ)");
    Boot b(kT0 + 3700);
    CHECK(b.ac.state().power && b.ac.state().off_timer == 0, "NVS lưu kết quả sau khi hẹn Tắt chạy xong"); }
  { Rig r; r.ac.setClimate(1,0,26); r.ac.setTimers(30,0);   // máy tắt, hẹn Bật 30 phút
    r.clear();
    at(r, kT0 + 1799);
    CHECK(!r.ac.state().power && r.ac.state().on_timer == 30, "hẹn Bật 30': 29:59 chưa tới hạn");
    at(r, kT0 + 1800);
    CHECK(r.ac.state().power && r.ac.state().on_timer == 0 && r.ac.state().t_start == 0 && r.ir.log.empty(),
          "hẹn Bật 30': tới hạn máy bật, hết hẹn, không phát IR");
    Boot b(kT0 + 1900);
    CHECK(b.ac.state().power && b.ac.state().on_timer == 0 && b.ac.state().t_start == 0, "NVS lưu kết quả sau khi hẹn Bật chạy xong"); }
  // [người dùng 2026-11-05] hẹn Tắt chạy xong: Eco/Sleep/Turbo bị xóa như tắt máy bằng nút Nguồn
  { Rig e; coolOn(e); e.ac.setEco(true); e.ac.setTimers(0,60); at(e, kT0 + 3600);
    CHECK(!e.ac.state().power && !e.ac.state().eco, "[người dùng 2026-11-05] hẹn Tắt chạy xong xóa Eco");
    Rig s; coolOn(s); s.ac.setSleep(true); s.ac.setTimers(0,60); at(s, kT0 + 3600);
    CHECK(!s.ac.state().power && !s.ac.state().sleep, "[người dùng 2026-11-05] hẹn Tắt chạy xong xóa Sleep");
    Rig t; coolOn(t); t.ac.setTurbo(true); t.ac.setTimers(0,60); at(t, kT0 + 3600);
    CHECK(!t.ac.state().power && !t.ac.state().turbo, "[người dùng 2026-11-05] hẹn Tắt chạy xong xóa Turbo");
    t.clear(); t.ac.setPower(true);
    CHECK(seq(t.ir,{{w24(0xB2BFD0),48,1}}) && !t.ac.state().turbo, "sau đó Bật máy: khung thường, không còn Turbo");
    Rig k; coolOn(k); k.ac.setEco(true); k.ac.setTimers(30,0); at(k, kT0 + 1800);
    CHECK(k.ac.state().power && k.ac.state().eco, "[suy ra] hẹn Bật chạy xong không xóa cờ Eco (máy đang chạy)"); }
  // Cả hai hẹn giờ tính từ cùng t_start, xử lý theo thứ tự thời gian
  { Rig r; coolOn(r); r.ac.setTimers(60,30); r.clear();   // đang chạy: Tắt sau 30', Bật sau 60'
    at(r, kT0 + 1800);
    CHECK(!r.ac.state().power && r.ac.state().off_timer == 0 && r.ac.state().on_timer == 60 && r.ac.state().t_start == kT0,
          "TẮT 30' + BẬT 60': tới 30' máy tắt, còn hẹn Bật, giữ t_start");
    at(r, kT0 + 3600);
    CHECK(r.ac.state().power && r.ac.state().on_timer == 0 && r.ac.state().t_start == 0 && r.ir.log.empty(),
          "TẮT 30' + BẬT 60': tới 60' máy bật lại, hết hẹn, không phát IR");
    Rig q; coolOn(q); q.ac.setTimers(30,60);   // Bật sau 30', Tắt sau 60'
    at(q, kT0 + 1800);
    CHECK(q.ac.state().power && q.ac.state().on_timer == 0 && q.ac.state().off_timer == 60, "BẬT 30' + TẮT 60': tới 30' còn hẹn Tắt");
    at(q, kT0 + 3600);
    CHECK(!q.ac.state().power && q.ac.state().off_timer == 0 && q.ac.state().t_start == 0, "BẬT 30' + TẮT 60': tới 60' máy tắt");
    Rig d; coolOn(d); d.ac.setTimers(60,60); at(d, kT0 + 3600);
    CHECK(!d.ac.state().power && d.ac.state().on_timer == 0 && d.ac.state().off_timer == 0,
          "[suy ra] trùng giờ BẬT = TẮT: xử lý Bật rồi Tắt, máy tắt"); }

  // 27. ESP32 tắt/khởi động lại giữa chừng: update() tính lại từ t_start (giờ thực) [thiết kế]
  { Rig r; coolOn(r); r.ac.setTimers(0,120);
    Boot b(kT0 + 600);   // nạp lại trước hạn
    CHECK(b.ac.state().power && b.ac.state().off_timer == 120 && b.ac.state().t_start == kT0 && b.ir.log.empty(),
          "nạp lại trước hạn: hẹn giờ còn nguyên, không phát IR");
    CHECK(b.ac.timerInfo().off_left == 6600, "nạp lại trước hạn: còn 110 phút tính từ t_start đã lưu");
    Boot c(kT0 + 7300);  // nạp lại sau hạn: máy đã tự tắt
    CHECK(!c.ac.state().power && c.ac.state().off_timer == 0 && c.ac.state().t_start == 0 && c.ir.log.empty(),
          "nạp lại sau hạn TẮT: máy đã tự tắt, ESP32 cập nhật power, không phát IR");
    Boot d(kT0 + 7310);
    CHECK(!d.ac.state().power && d.ac.state().off_timer == 0, "NVS đã lưu sau khi cập nhật"); }
  { Rig e; coolOn(e); e.ac.setTimers(30,60); Boot f(kT0 + 7200);    // Bật 30', Tắt 60'
    CHECK(!f.ac.state().power && f.ac.state().on_timer == 0 && f.ac.state().off_timer == 0 && f.ac.state().t_start == 0,
          "nạp lại sau cả hai hạn (Bật 30', Tắt 60'): máy tắt");
    Rig g; coolOn(g); g.ac.setTimers(60,30); Boot h(kT0 + 7200);    // Bật 60', Tắt 30'
    CHECK(h.ac.state().power && h.ac.state().on_timer == 0 && h.ac.state().off_timer == 0 && h.ac.state().t_start == 0,
          "nạp lại sau cả hai hạn (Bật 60', Tắt 30'): máy bật"); }
  { Rig n; coolOn(n); n.ac.setTimers(0,60);
    Boot k(0);   // khởi động chưa có giờ
    CHECK(k.ac.state().power && k.ac.state().off_timer == 60 && k.ac.timerInfo().clock == -1 && k.ac.timerInfo().off_due == -1,
          "khởi động chưa có giờ: giữ nguyên, chưa tính được mốc");
    at(k, kT0 + 4000);
    CHECK(!k.ac.state().power && k.ac.state().off_timer == 0 && k.ir.log.empty(), "có giờ muộn: tính lại theo t_start, máy tắt, không phát IR"); }

  // 28. [người dùng 2026-11-05] Đang hẹn giờ mà đổi nhiệt/quạt/chế độ/Eco/Sleep/Turbo/Swing/Light thì máy
  //     GIỮ MỐC CŨ, không đếm lại => t_start KHÔNG đổi, và khung hẹn giờ gửi lại mang số phút gốc
  { Rig r; coolOn(r); r.ac.setTimers(0,120);
    auto keeps = [&](const char* name) { CHECK(r.ac.state().t_start == kT0 && r.ac.state().off_timer == 120, name); };
    g_epoch = kT0 + 600; r.clear(); r.ac.setClimate(1,0,27);
    CHECK(seq(r.ir,{{0xB24DA75890FFULL,48,1}}), "[aircon8] đổi nhiệt khi hẹn Tắt 2h: khung hẹn giờ mang 2h gốc, không phải số phút còn lại");
    keeps("[người dùng 2026-11-05] đổi nhiệt giữ mốc cũ");
    g_epoch = kT0 + 700; r.ac.setClimate(1,2,27);  keeps("[người dùng 2026-11-05] đổi tốc độ quạt giữ mốc cũ");
    g_epoch = kT0 + 800; r.ac.setEco(true);        keeps("[người dùng 2026-11-05] bật Eco giữ mốc cũ");
    g_epoch = kT0 + 900; r.ac.setEco(false);       keeps("[người dùng 2026-11-05] tắt Eco giữ mốc cũ");
    g_epoch = kT0 + 1000; r.ac.setSleep(true);     keeps("[người dùng 2026-11-05] bật Sleep giữ mốc cũ");
    g_epoch = kT0 + 1100; r.ac.setSleep(false);    keeps("[người dùng 2026-11-05] tắt Sleep giữ mốc cũ");
    g_epoch = kT0 + 1200; r.ac.setTurbo(true);     keeps("[người dùng 2026-11-05] bật Turbo giữ mốc cũ");
    g_epoch = kT0 + 1300; r.ac.setTurbo(false);    keeps("[người dùng 2026-11-05] tắt Turbo giữ mốc cũ");
    g_epoch = kT0 + 1400; r.ac.swing();            keeps("[người dùng 2026-11-05] Swing giữ mốc cũ");
    g_epoch = kT0 + 1500; r.ac.light();            keeps("[người dùng 2026-11-05] Light giữ mốc cũ");
    g_epoch = kT0 + 1600; r.ac.setClimate(2,0,27); keeps("[người dùng 2026-11-05] đổi chế độ giữ mốc cũ");
    at(r, kT0 + 7199);
    CHECK(r.ac.state().power, "sau nhiều lần đổi cài đặt: 119:59 tính từ mốc CŨ vẫn chưa tới hạn");
    at(r, kT0 + 7200);
    CHECK(!r.ac.state().power && r.ac.state().off_timer == 0, "hẹn Tắt chạy đúng mốc cũ (kT0 + 2h), không bị lùi theo lần đổi cài đặt"); }
  { Rig o; o.ac.setClimate(1,0,26); o.ac.setTimers(60,0);   // máy tắt đang hẹn Bật (aircon22)
    g_epoch = kT0 + 900; o.ac.setClimate(1,0,27);
    CHECK(o.ac.state().t_start == kT0 && o.ac.state().on_timer == 60, "[người dùng 2026-11-05] máy tắt đang hẹn Bật, đổi nhiệt giữ mốc cũ");
    Rig n; coolOn(n); n.ac.setTimers(0,120); g_epoch = kT0 + 900; n.ac.setTimers(0,120);
    CHECK(n.ac.state().t_start == kT0 + 900, "[suy ra] đặt lại hẹn giờ bằng setTimers = mốc mới (máy đếm lại từ khung mới)");
    Rig m; coolOn(m); m.ac.setTimers(0,120); g_epoch = kT0 + 900; m.ac.setTimers(60,120);
    CHECK(m.ac.state().t_start == kT0 + 900, "[suy ra] thêm hẹn Bật vào hẹn Tắt đang có: cả hai tính lại từ khung mới"); }

  // 29. Hẹn "Lúc hh:mm": mốc kế tiếp theo giờ địa phương UTC+7; hẹn một lần [thiết kế]
  { Rig r; coolOn(r);   // 10:00:00
    CHECK(r.ac.setSchedule(385, -1) == nullptr && r.ac.state().on_at == L(1,6,25) && r.ac.state().off_at == 0,
          "10:00 hẹn Bật 06:25 = 06:25 ngày mai");
    CHECK(r.ir.log.empty() && r.ac.state().on_timer == 0 && r.ac.state().off_timer == 0 && r.ac.state().t_start == 0,
          "hẹn Lúc hh:mm không phát IR (khi không có hẹn Sau X giờ)");
    CHECK(r.ac.setSchedule(-1, 22*60) == nullptr && r.ac.state().off_at == L(0,22,0) && r.ac.state().on_at == 0,
          "10:00 hẹn Tắt 22:00 = 22:00 hôm nay (đặt lại thay thế hẹn cũ)");
    CHECK(r.ac.setSchedule(6*60, 22*60) == nullptr && r.ac.state().on_at == L(1,6,0) && r.ac.state().off_at == L(0,22,0),
          "hẹn cả Bật 06:00 và Tắt 22:00");
    // Đối chiếu epoch tuyệt đối: 06:00 UTC+7 ngày 2026-11-06 = 2026-11-05 23:00 UTC = 1793919600
    CHECK(r.ac.state().on_at == 1793919600u, "[đối chiếu epoch] 06:00 UTC+7 ngày 06/11 = 2026-11-05 23:00 UTC"); }
  { Rig r; g_epoch = L(0,10,0,0);
    CHECK(r.ac.setSchedule(600,-1) == nullptr && r.ac.state().on_at == L(1,10,0), "đúng 10:00:00 hẹn 10:00 = ngày mai");
    g_epoch = L(0,10,0,30);
    CHECK(r.ac.setSchedule(600,-1) == nullptr && r.ac.state().on_at == L(1,10,0), "10:00:30 hẹn 10:00 (cùng phút, đã qua) = ngày mai");
    CHECK(r.ac.setSchedule(601,-1) == nullptr && r.ac.state().on_at == L(0,10,1), "10:00:30 hẹn 10:01 = hôm nay");
    g_epoch = L(0,23,59,59);
    CHECK(r.ac.setSchedule(0,1439) == nullptr && r.ac.state().on_at == L(1,0,0) && r.ac.state().off_at == L(1,23,59),
          "23:59:59 hẹn 00:00 và 23:59 = cả hai ngày mai");
    g_epoch = L(0,0,0,0);
    CHECK(r.ac.setSchedule(0,1) == nullptr && r.ac.state().on_at == L(1,0,0) && r.ac.state().off_at == L(0,0,1),
          "00:00:00 hẹn 00:00 = ngày mai, hẹn 00:01 = hôm nay");
    g_epoch = L(0,5,30);   // 05:30 giờ địa phương = 22:30 UTC ngày hôm trước
    CHECK(r.ac.setSchedule(385,-1) == nullptr && r.ac.state().on_at == L(0,6,25), "05:30 hẹn 06:25 = hôm nay (qua ranh giới ngày UTC)"); }
  // Tham số sai: từ chối, không phát IR, không đổi trạng thái
  { Rig r; coolOn(r); r.ac.setSchedule(-1, 1320);
    CHECK(r.ac.setSchedule(1440,-1) != nullptr, "giờ Bật 1440 lỗi");
    CHECK(r.ac.setSchedule(-2,-1) != nullptr, "giờ Bật -2 lỗi");
    CHECK(r.ac.setSchedule(-1,1440) != nullptr, "giờ Tắt 1440 lỗi");
    CHECK(r.ac.setSchedule(-1,-2) != nullptr, "giờ Tắt -2 lỗi");
    CHECK(r.ac.setSchedule(385,385) != nullptr, "giờ Bật trùng giờ Tắt lỗi");
    CHECK(r.ac.setSchedule(0,0) != nullptr, "giờ Bật trùng giờ Tắt (00:00) lỗi");
    CHECK(r.ir.log.empty() && r.ac.state().off_at == L(0,22,0) && r.ac.state().on_at == 0, "tham số sai không đổi hẹn giờ đang có, không phát IR");
    CHECK(r.ac.setSchedule(-1,-1) == nullptr && r.ac.state().off_at == 0, "setSchedule(-1,-1) = hủy hẹn giờ");
    Rig c; coolOn(c); c.ac.setSchedule(360, 1320); c.ac.setClean(true); c.clear();
    CHECK(c.ac.setSchedule(400,-1) != nullptr && c.ac.cancelTimers() != nullptr && c.ac.setTimers(0,60) != nullptr && c.ir.log.empty(),
          "đang Clean: đặt/hủy hẹn giờ bị từ chối");
    CHECK(c.ac.state().on_at == L(1,6,0) && c.ac.state().off_at == L(0,22,0), "đang Clean: hẹn Lúc hh:mm giữ nguyên"); }

  // 30. Hẹn "Lúc hh:mm": tới giờ ESP32 phát lệnh Bật/Tắt như nút Nguồn, chỉ xóa mốc vừa dùng [thiết kế]
  { Rig r; g_epoch = L(0,20,0); r.ac.setClimate(1,0,26); r.ac.setPower(true); r.ac.setSchedule(-1, 22*60); r.clear();
    at(r, L(0,21,59,59));
    CHECK(r.ir.log.empty() && r.ac.state().power && r.ac.state().off_at == L(0,22,0), "21:59:59 chưa tới giờ Tắt");
    at(r, L(0,22,0));
    CHECK(seq(r.ir,{{w24(0xB27BE0),48,1}}) && !r.ac.state().power && r.ac.state().off_at == 0, "22:00:00 phát khung Tắt B27BE0, xóa hẹn vừa dùng");
    at(r, L(0,22,0,30)); at(r, L(0,23,0));
    CHECK(r.ir.log.size() == 1, "hẹn một lần: không phát lại");
    Boot b(L(0,23,5)); CHECK(!b.ac.state().power && b.ac.state().off_at == 0, "NVS lưu kết quả sau khi hẹn Tắt chạy"); }
  { Rig r; g_epoch = L(0,23,0); r.ac.setClimate(1,0,26); r.ac.setSchedule(6*60, -1); r.clear();   // máy tắt, hẹn Bật 06:00 ngày mai
    at(r, L(1,5,59,59));
    CHECK(r.ir.log.empty() && !r.ac.state().power, "05:59:59 chưa tới giờ Bật");
    at(r, L(1,6,0));
    CHECK(seq(r.ir,{{w24(0xB2BFD0),48,1}}) && r.ac.state().power && r.ac.state().on_at == 0, "06:00:00 phát khung Bật (trạng thái đã lưu Cool 26 Auto) B2BFD0"); }
  { Rig r; g_epoch = L(0,20,0); r.ac.setClimate(1,0,26); r.ac.setPower(true); r.ac.setSchedule(6*60, 22*60); r.clear();
    at(r, L(0,22,0));
    CHECK(seq(r.ir,{{w24(0xB27BE0),48,1}}) && !r.ac.state().power && r.ac.state().off_at == 0 && r.ac.state().on_at == L(1,6,0),
          "Bật 06:00 + Tắt 22:00: tới 22:00 chỉ xóa hẹn Tắt, giữ hẹn Bật");
    r.clear(); at(r, L(1,6,0));
    CHECK(seq(r.ir,{{w24(0xB2BFD0),48,1}}) && r.ac.state().power && r.ac.state().on_at == 0, "rồi 06:00 sáng hôm sau phát khung Bật"); }
  // Hai hẹn cùng tới hạn trong một lần update() (loop bị trễ): xử lý theo thứ tự thời gian
  { Rig r; g_epoch = L(0,20,0); r.ac.setClimate(1,0,26); r.ac.setPower(true); r.ac.setSchedule(6*60, 22*60); r.clear();   // Tắt 22:00 hôm nay, Bật 06:00 ngày mai
    at(r, L(1,7,0));
    CHECK(seq(r.ir,{{w24(0xB27BE0),48,1},{w24(0xB2BFD0),48,1}}) && r.ac.state().power && r.ac.state().on_at == 0 && r.ac.state().off_at == 0,
          "[suy ra] Tắt 22:00 rồi Bật 06:00 cùng tới hạn: Tắt trước, Bật sau, máy bật");
    Rig q; g_epoch = L(0,3,0); q.ac.setClimate(1,0,26); q.ac.setPower(true); q.ac.setSchedule(6*60, 22*60); q.clear();    // Bật 06:00, Tắt 22:00 hôm nay
    at(q, L(0,23,0));
    CHECK(seq(q.ir,{{w24(0xB2BFD0),48,1},{w24(0xB27BE0),48,1}}) && !q.ac.state().power && q.ac.state().on_at == 0 && q.ac.state().off_at == 0,
          "[suy ra] Bật 06:00 rồi Tắt 22:00 cùng tới hạn: Bật trước, Tắt sau, máy tắt"); }
  // Phát lệnh dù ESP32 đang nghĩ máy ở trạng thái nào (sửa lệch do dùng remote gốc)
  { Rig r; r.ac.setClimate(1,0,26); r.ac.setSchedule(-1, 22*60); r.clear();    // ESP32 nghĩ máy đang tắt
    at(r, L(0,22,0));
    CHECK(seq(r.ir,{{w24(0xB27BE0),48,1}}) && !r.ac.state().power, "ESP32 nghĩ máy tắt: hẹn Tắt vẫn phát B27BE0");
    Rig q; coolOn(q); q.ac.setSchedule(6*60, -1); q.clear();                      // ESP32 nghĩ máy đang chạy
    at(q, L(1,6,0));
    CHECK(seq(q.ir,{{w24(0xB2BFD0),48,1}}) && q.ac.state().power, "ESP32 nghĩ máy chạy: hẹn Bật vẫn phát khung trạng thái"); }
  // Hẹn Tắt xóa Sleep/Eco/Turbo như nút Nguồn
  { Rig e; coolOn(e); e.ac.setEco(true); e.ac.setSchedule(-1, 22*60); at(e, L(0,22,0));
    CHECK(!e.ac.state().power && !e.ac.state().eco, "hẹn Tắt Lúc hh:mm xóa Eco");
    Rig t; coolOn(t); t.ac.setTurbo(true); t.ac.setSchedule(-1, 22*60); at(t, L(0,22,0));
    CHECK(!t.ac.state().power && !t.ac.state().turbo, "hẹn Tắt Lúc hh:mm xóa Turbo"); }
  // Đổi cài đặt khi đang hẹn Lúc hh:mm: gửi khung thường (không phải khung hẹn giờ), giữ hẹn
  { Rig r; coolOn(r); r.ac.setSchedule(-1, 22*60); r.clear(); r.ac.setClimate(1,0,27);
    CHECK(seq(r.ir,{{w24(0xB2BF90),48,1}}) && r.ac.state().off_at == L(0,22,0), "đổi nhiệt khi hẹn Lúc hh:mm: khung thường, giữ hẹn");
    r.ac.setEco(true); r.ac.setClimate(1,0,28); r.ac.setEco(false);
    CHECK(r.ac.state().off_at == L(0,22,0), "Eco/đổi nhiệt không xóa hẹn Lúc hh:mm");
    r.clear(); at(r, L(0,22,0));
    CHECK(seq(r.ir,{{w24(0xB27BE0),48,1}}), "hẹn Tắt sau khi đổi cài đặt vẫn phát B27BE0"); }
  // Clean: hẹn Bật bị bỏ qua (máy không nhận nút Bật trong Clean, aircon26/27), hẹn Tắt vẫn chạy
  { Rig r; coolOn(r); g_epoch = L(0,20,0); r.ac.setSchedule(6*60, -1); r.ac.setClean(true);
    CHECK(r.ac.state().on_at == L(1,6,0) && r.ac.state().clean, "bật Clean không xóa hẹn Lúc hh:mm của ESP32");
    r.clear(); g_millis = 5000; at(r, L(1,6,0));   // (bỏ qua hết giờ Clean: g_millis nhỏ)
    CHECK(r.ir.log.empty() && r.ac.state().clean && r.ac.state().power && r.ac.state().on_at == 0,
          "tới giờ Bật khi đang Clean: bỏ lệnh, hẹn bị xóa, Clean vẫn chạy");
    CHECK(r.ac.timerInfo().missed_on == 360 && r.ac.timerInfo().missed_off == -1, "hẹn Bật bị bỏ qua vì Clean: ghi cảnh báo missed_on = 06:00");
    g_millis = 0;
    Rig q; coolOn(q); g_epoch = L(0,20,0); q.ac.setSchedule(-1, 22*60); q.ac.setClean(true); q.clear();
    at(q, L(0,22,0));
    CHECK(seq(q.ir,{{w24(0xB27BE0),48,1}}) && !q.ac.state().power && !q.ac.state().clean && q.ac.state().off_at == 0,
          "tới giờ Tắt khi đang Clean: phát B27BE0, máy tắt, Clean kết thúc"); }
  // Nguồn thủ công xóa MỌI hẹn giờ [thiết kế]
  { Rig a; coolOn(a); a.ac.setSchedule(360, 1320); a.ac.setPower(true);
    CHECK(a.ac.state().on_at == 0 && a.ac.state().off_at == 0, "bấm Bật xóa hẹn Lúc hh:mm");
    Rig b; coolOn(b); b.ac.setSchedule(360, 1320); b.ac.setPower(false);
    CHECK(b.ac.state().on_at == 0 && b.ac.state().off_at == 0 && !b.ac.state().power, "bấm Tắt xóa hẹn Lúc hh:mm");
    Rig c; coolOn(c); c.ac.setTimers(60,120); c.ac.setPower(false);
    CHECK(c.ac.state().t_start == 0 && c.ac.state().on_timer == 0 && c.ac.state().off_timer == 0, "bấm Tắt xóa hẹn Sau X giờ và t_start"); }

  // 31. ESP32 tắt qua giờ hẹn "Lúc hh:mm": lệnh chưa từng được gửi => xóa hẹn, GIỮ NGUYÊN power, ghi cảnh báo [thiết kế]
  { Rig r; g_epoch = L(0,3,0); r.ac.setClimate(1,0,26); r.ac.setPower(true); r.ac.setSchedule(-1, 385);
    Boot b(L(0,7,0));   // ESP32 mất điện 03:00, bật lại 07:00, quá giờ Tắt 06:25
    CHECK(b.ac.state().power && b.ac.state().off_at == 0 && b.ir.log.empty(),
          "lỡ hẹn Tắt: xóa hẹn, GIỮ power (máy có thể vẫn đang chạy), không phát IR");
    CHECK(b.ac.timerInfo().missed_off == 385 && b.ac.timerInfo().missed_on == -1, "lỡ hẹn Tắt 06:25: cảnh báo missed_off = 385");
    at(b, L(0,7,5));
    CHECK(b.ac.timerInfo().missed_off == 385, "cảnh báo không mất khi update() chạy tiếp");
    b.ac.setClimate(1,0,27);
    CHECK(b.ac.timerInfo().missed_off == -1, "người dùng thao tác tiếp thì cảnh báo mất");
    Boot c(L(0,7,10)); CHECK(c.ac.state().off_at == 0 && c.ac.timerInfo().missed_off == -1, "cảnh báo chỉ trong RAM: NVS đã xóa hẹn, khởi động lại không còn cảnh báo"); }
  { Rig r; g_epoch = L(0,3,0); r.ac.setClimate(1,0,26); r.ac.setSchedule(6*60, -1);   // máy tắt, hẹn Bật 06:00
    Boot b(L(0,7,0));
    CHECK(!b.ac.state().power && b.ac.state().on_at == 0 && b.ir.log.empty() && b.ac.timerInfo().missed_on == 360,
          "lỡ hẹn Bật 06:00: xóa hẹn, giữ power = tắt, cảnh báo missed_on = 360"); }
  { Rig r; g_epoch = L(0,3,0); r.ac.setClimate(1,0,26); r.ac.setPower(true); r.ac.setSchedule(6*60, 22*60);
    Boot b(L(0,7,0));   // hẹn lúc 03:00 -> Bật 06:00 hôm nay, Tắt 22:00 hôm nay; bật lại 07:00: Bật đã lỡ, Tắt chưa tới
    CHECK(b.ac.state().on_at == 0 && b.ac.state().off_at == L(0,22,0) && b.ac.timerInfo().missed_on == 360 && b.ac.timerInfo().missed_off == -1,
          "chỉ hẹn đã quá giờ bị xóa/cảnh báo, hẹn còn lại giữ nguyên"); }
  { Rig r; g_epoch = L(0,3,0); r.ac.setClimate(1,0,26); r.ac.setPower(true); r.ac.setSchedule(-1, 385);
    Boot b(L(0,5,0));   // chưa tới giờ
    CHECK(b.ac.state().off_at == L(0,6,25) && b.ac.timerInfo().missed_off == -1 && b.ir.log.empty(), "nạp lại trước giờ hẹn: giữ hẹn, không cảnh báo");
    at(b, L(0,6,25));
    CHECK(seq(b.ir,{{w24(0xB27BE0),48,1}}) && !b.ac.state().power && b.ac.timerInfo().missed_off == -1,
          "hẹn còn hiệu lực sau khi nạp lại: tới giờ vẫn phát lệnh Tắt"); }
  { Rig r; g_epoch = L(0,3,0); r.ac.setClimate(1,0,26); r.ac.setPower(true); r.ac.setSchedule(-1, 385);
    Boot b(0);   // khởi động chưa có giờ (mất Internet)
    CHECK(b.ac.state().off_at == L(0,6,25) && b.ac.timerInfo().clock == -1 && b.ac.timerInfo().off_due == -1 && b.ir.log.empty(),
          "khởi động chưa có giờ: giữ hẹn, chưa xử lý được");
    at(b, L(0,7,0));
    CHECK(b.ac.state().power && b.ac.state().off_at == 0 && b.ir.log.empty() && b.ac.timerInfo().missed_off == 385,
          "có giờ sau giờ hẹn: coi là lỡ (không phát lệnh muộn), giữ power"); }

  // 32. Hai loại hẹn giờ loại trừ nhau [thiết kế]
  { Rig r; coolOn(r); r.ac.setTimers(0,60); r.clear();
    CHECK(r.ac.setSchedule(-1, 22*60) == nullptr && seq(r.ir,{{w24(0xB2BFD0),48,1}}),
          "Sau X giờ -> Lúc hh:mm (máy chạy): gửi khung trạng thái thường để hủy hẹn giờ trên máy");
    CHECK(r.ac.state().off_timer == 0 && r.ac.state().t_start == 0 && r.ac.state().off_at == L(0,22,0) && r.ac.timerInfo().kind == 2,
          "Sau X giờ -> Lúc hh:mm: hẹn cũ bị xóa, kind = 2");
    Rig o; o.ac.setClimate(1,0,26); o.ac.setTimers(60,0); o.clear();
    CHECK(o.ac.setSchedule(360,-1) == nullptr && seq(o.ir,{{w24(0xB27BE0),48,1}}), "Sau X giờ -> Lúc hh:mm (máy tắt): khung hủy = khung Tắt B27BE0");
    Rig s; coolOn(s); s.ac.setSchedule(-1, 22*60); s.clear();
    CHECK(s.ac.setTimers(0,60) == nullptr && seq(s.ir,{{0xB24DA35CD0FFULL,48,1}}),
          "Lúc hh:mm -> Sau X giờ: gửi khung hẹn TẮT 1h (B24DA35CD0FF)");
    CHECK(s.ac.state().on_at == 0 && s.ac.state().off_at == 0 && s.ac.state().off_timer == 60 && s.ac.state().t_start == kT0 && s.ac.timerInfo().kind == 1,
          "Lúc hh:mm -> Sau X giờ: hẹn Lúc hh:mm bị xóa, kind = 1");
    Rig z; coolOn(z); z.ac.setSchedule(-1, 22*60); z.clear(); z.ac.setTimers(0,0);
    CHECK(z.ac.state().off_at == 0, "setTimers(0,0) cũng xóa hẹn Lúc hh:mm");
    Rig y; coolOn(y); y.ac.setSchedule(360, -1); y.clear(); y.ac.setTimers(0,60);
    CHECK(y.ac.state().on_at == 0 && y.ac.state().off_timer == 60, "Lúc hh:mm (hẹn Bật) -> Sau X giờ: hẹn Bật Lúc hh:mm bị xóa");
    Rig x; coolOn(x); x.ac.setSchedule(360, 1320); x.ac.setTimers(60,0);
    CHECK(x.ac.state().on_at == 0 && x.ac.state().off_at == 0 && x.ac.timerInfo().kind == 1, "Lúc hh:mm (cả hai) -> Sau X giờ: cả hai bị xóa"); }
  // cancelTimers
  { Rig a; coolOn(a); a.ac.setTimers(60,120); a.clear();
    CHECK(a.ac.cancelTimers() == nullptr && seq(a.ir,{{w24(0xB2BFD0),48,1}}) && a.ac.state().on_timer == 0 && a.ac.state().off_timer == 0 && a.ac.state().t_start == 0,
          "hủy hẹn Sau X giờ (máy chạy): khung thường, xóa t_start");
    Rig b; b.ac.setClimate(1,0,26); b.ac.setTimers(60,0); b.clear();
    CHECK(b.ac.cancelTimers() == nullptr && seq(b.ir,{{w24(0xB27BE0),48,1}}) && b.ac.state().on_timer == 0, "hủy hẹn Sau X giờ (máy tắt): khung Tắt");
    Rig c; coolOn(c); c.ac.setSchedule(360, 1320); c.clear();
    CHECK(c.ac.cancelTimers() == nullptr && c.ir.log.empty() && c.ac.state().on_at == 0 && c.ac.state().off_at == 0, "hủy hẹn Lúc hh:mm: không phát IR");
    Rig d; coolOn(d);
    CHECK(d.ac.cancelTimers() == nullptr && d.ir.log.empty(), "hủy khi không có hẹn giờ: không phát IR"); }
  // Hẹn giờ không ảnh hưởng bởi cờ khác
  { Rig k; coolOn(k); k.ac.setSchedule(-1, 22*60); k.ac.setEco(true); k.ac.setClimate(1,2,27); k.ac.setClimate(2,0,27);
    CHECK(k.ac.state().off_at == L(0,22,0), "Eco/đổi quạt/đổi chế độ không xóa hẹn Lúc hh:mm"); }

  // 33. timerInfo cho Web UI (ESP32 tính sẵn, không phụ thuộc đồng hồ trình duyệt) [thiết kế]
  { Rig r; coolOn(r); r.ac.setTimers(0,90);   // 10:00 + 90' = 11:30
    TimerInfo t = r.ac.timerInfo();
    CHECK(t.kind == 1 && t.clock == 600 && t.off_due == 690 && t.off_left == 5400 && t.on_due == -1 && t.on_left == 0 && t.missed_on == -1 && t.missed_off == -1,
          "timerInfo hẹn Tắt 90': kind 1, hiện 10:00, đến hạn 11:30, còn 5400 giây");
    g_epoch = kT0 + 60; t = r.ac.timerInfo();
    CHECK(t.clock == 601 && t.off_left == 5340, "1 phút sau: còn 5340 giây, giờ hiện 10:01");
    g_epoch = kT0 + 5401; t = r.ac.timerInfo();
    CHECK(t.off_left == 0 && t.off_due == 690, "quá hạn nhưng chưa update(): off_left = 0, vẫn còn off_due");
    Rig m; coolOn(m); m.ac.setTimers(60,30);
    t = m.ac.timerInfo();
    CHECK(t.on_due == 660 && t.on_left == 3600 && t.off_due == 630 && t.off_left == 1800, "timerInfo cả hai hẹn: Bật 11:00 còn 3600s, Tắt 10:30 còn 1800s");
    Rig n; g_epoch = L(0,20,0); n.ac.setClimate(1,0,26); n.ac.setPower(true); n.ac.setTimers(0,600);
    CHECK(n.ac.timerInfo().off_due == 360 && n.ac.timerInfo().off_left == 36000, "hẹn Tắt 10h lúc 20:00: đến hạn 06:00 ngày mai (qua nửa đêm)");
    Rig c; coolOn(c); c.ac.setSchedule(360, 1320);
    t = c.ac.timerInfo();
    CHECK(t.kind == 2 && t.on_due == 360 && t.on_left == 72000 && t.off_due == 1320 && t.off_left == 43200,
          "timerInfo Lúc hh:mm: Bật 06:00 còn 20h, Tắt 22:00 còn 12h");
    Rig z; coolOn(z); t = z.ac.timerInfo();
    CHECK(t.kind == 0 && t.on_due == -1 && t.off_due == -1 && t.clock == 600, "không hẹn giờ: kind 0, clock = 600"); }

  // 34. NVS: mốc hẹn giờ phải hợp lý; tổ hợp vô lý -> mặc định cả bộ [thiết kế]
  { Rig x; { Preferences p; p.begin("ac", false); p.putBool("power", true); p.putUShort("off_timer", 60); p.putUInt("t_start", kT0); p.putUInt("off_at", kT0 + 100); }
    IRsend i; AcController b; b.begin(i, testNow);
    CHECK(!b.state().power && b.state().off_timer == 0 && b.state().off_at == 0, "NVS: hẹn Sau X giờ + Lúc hh:mm cùng có -> mặc định cả bộ");
    Rig y; { Preferences p; p.begin("ac", false); p.putBool("power", true); p.putUShort("off_timer", 60); }
    IRsend i2; AcController b2; b2.begin(i2, testNow);
    CHECK(!b2.state().power && b2.state().off_timer == 0, "NVS: có hẹn Sau X giờ nhưng thiếu t_start -> mặc định (cả NVS của bản firmware cũ)");
    Rig z; { Preferences p; p.begin("ac", false); p.putBool("power", true); p.putUInt("t_start", kT0); }
    IRsend i3; AcController b3; b3.begin(i3, testNow);
    CHECK(!b3.state().power && b3.state().t_start == 0, "NVS: có t_start nhưng không có hẹn Sau X giờ -> mặc định");
    Rig w; { Preferences p; p.begin("ac", false); p.putBool("power", true); p.putUShort("off_timer", 60); p.putUInt("t_start", kT0); }
    Boot ok(kT0 + 10);
    CHECK(ok.ac.state().power && ok.ac.state().off_timer == 60 && ok.ac.state().t_start == kT0, "NVS: hẹn Sau X giờ hợp lệ được giữ"); }
  { Rig f; { Preferences p; p.begin("ac", false); p.putBool("power", true); p.putUShort("off_timer", 60); p.putUInt("t_start", kT0 + 1000); }
    Boot b(kT0);
    CHECK(!b.ac.state().power && b.ac.state().off_timer == 0 && b.ac.state().t_start == 0, "t_start ở tương lai (so với giờ thực) -> mặc định cả bộ");
    Boot b2(kT0 + 2000); CHECK(!b2.ac.state().power && b2.ac.state().off_timer == 0 && b2.ac.state().t_start == 0, "mặc định đã được lưu vào NVS (không còn dữ liệu hỏng cũ)");
    Rig e; { Preferences p; p.begin("ac", false); p.putBool("power", true); p.putUShort("off_timer", 60); p.putUInt("t_start", kT0); }
    Boot c(kT0); CHECK(c.ac.state().power && c.ac.state().t_start == kT0, "t_start bằng đúng giờ thực: hợp lệ");
    Rig g; { Preferences p; p.begin("ac", false); p.putBool("power", true); p.putUInt("off_at", kT0 + 86400); }
    Boot d(kT0); CHECK(d.ac.state().power && d.ac.state().off_at == kT0 + 86400, "off_at đúng 24 giờ tới: hợp lệ");
    Rig h; { Preferences p; p.begin("ac", false); p.putBool("power", true); p.putUInt("off_at", kT0 + 86401); }
    Boot e2(kT0); CHECK(!e2.ac.state().power && e2.ac.state().off_at == 0, "off_at xa hơn 24 giờ -> mặc định cả bộ");
    Rig i; { Preferences p; p.begin("ac", false); p.putBool("power", true); p.putUInt("on_at", kT0 + 86400); }
    Boot f2(kT0); CHECK(f2.ac.state().power && f2.ac.state().on_at == kT0 + 86400, "on_at đúng 24 giờ tới: hợp lệ");
    Rig j; { Preferences p; p.begin("ac", false); p.putBool("power", true); p.putUInt("on_at", kT0 + 86401); }
    Boot g2(kT0); CHECK(!g2.ac.state().power && g2.ac.state().on_at == 0, "on_at xa hơn 24 giờ -> mặc định cả bộ"); }
  // Lưu/nạp hẹn Lúc hh:mm
  { Rig r; coolOn(r); r.ac.setSchedule(360, 1320);
    Boot b(kT0 + 60);
    CHECK(b.ac.state().power && b.ac.state().on_at == L(1,6,0) && b.ac.state().off_at == L(0,22,0) && b.ir.log.empty(),
          "NVS: nạp lại hẹn Lúc hh:mm, khởi động không phát IR"); }


  printf("%d/%d dat\n", pass, pass + fail);
  return fail ? 1 : 0;
}
