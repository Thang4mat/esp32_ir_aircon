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

// Khung 24-bit -> 48-bit trên dây, viết độc lập với coolix::wire()
static uint64_t w24(uint32_t c) {
  uint64_t o = 0;
  for (int s = 16; s >= 0; s -= 8) { uint8_t b = (c >> s) & 0xFF; o = (o << 16) | ((uint64_t)b << 8) | (uint8_t)~b; }
  return o;
}
struct Rig {
  IRsend ir; AcController ac;
  Rig() { nvsStore().clear(); ac.begin(ir); }
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
    eq(eco(s30,true), 0xB54AF50A82B0ULL, "eco bật @30C"); eq(eco(s30,false), 0xB54AF50A83B0ULL, "eco tắt @30C");
    eq(eco(s,true), 0xB54AF50A82D0ULL, "eco bật @26C");  eq(eco(s,false), 0xB54AF50A83D0ULL, "eco tắt @26C");
    const uint32_t sd = state(kModeDry, kFanAuto, 26);
    eq(timerOn(sd,300), 0xB24D1FE0D793ULL, "Dry timerOn 5h"); eq(timerOff(sd,600,0), 0xB24D07F8D5FFULL, "Dry timerOff 10h");
  }
  // 1. Bảng nhiệt Cool 17-30 (aircon2.txt)
  { const uint32_t code[14] = {0xB2BF00,0xB2BF10,0xB2BF30,0xB2BF20,0xB2BF60,0xB2BF70,0xB2BF50,
                               0xB2BF40,0xB2BFC0,0xB2BFD0,0xB2BF90,0xB2BF80,0xB2BFA0,0xB2BFB0};
    Rig r; r.ac.setClimate(1, 0, 26); r.ac.setPower(true);
    for (int t = 17; t <= 30; t++) { r.clear(); r.ac.setClimate(1, 0, t);
      CHECK(seq(r.ir, {{w24(code[t-17]),48,1}}), "bảng nhiệt Cool"); } }

  // 2. Mode/Fan (aircon.txt)
  { Rig r; r.ac.setClimate(1,0,26); r.ac.setPower(true);
    r.clear(); r.ac.setClimate(1,1,26); CHECK(seq(r.ir,{{w24(0xB29FD0),48,1}}), "Cool quạt Thấp B29FD0");
    r.clear(); r.ac.setClimate(1,2,26); CHECK(seq(r.ir,{{w24(0xB25FD0),48,1}}), "Cool quạt Vừa B25FD0");
    r.clear(); r.ac.setClimate(1,3,26); CHECK(seq(r.ir,{{w24(0xB23FD0),48,1}}), "Cool quạt Cao B23FD0");
    r.clear(); r.ac.setClimate(2,0,26); CHECK(seq(r.ir,{{w24(0xB21FD4),48,1}}), "Dry B21FD4");
    r.clear(); r.ac.setClimate(0,0,26); CHECK(seq(r.ir,{{w24(0xB21FD8),48,1}}), "Auto B21FD8");
    r.clear(); r.ac.setClimate(3,0,26); CHECK(seq(r.ir,{{w24(0xB2BFE4),48,1}}), "Fan B2BFE4"); }

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

  // 6. Hẹn BẬT 5h + TẮT 10h (aircon5.txt)
  { Rig r; coolOn(r); r.ac.setTimers(300,0); r.clear(); r.ac.setTimers(300,600);
    CHECK(seq(r.ir,{{0xB24DA758D193ULL,48,1}}), "BẬT 5h + TẮT 10h");
    r.clear(); r.ac.setTimers(0,600); CHECK(seq(r.ir,{{0xB24DA758D1FFULL,48,1}}), "hủy BẬT, còn TẮT 10h");
    r.clear(); r.ac.setTimers(300,0); CHECK(seq(r.ir,{{0xB24DBF40D393ULL,48,1}}), "hủy TẮT, còn BẬT 5h");
    r.clear(); r.ac.setTimers(0,0);   CHECK(seq(r.ir,{{w24(0xB2BFD0),48,1}}), "hủy cả hai = khung thường"); }

  // 7. Hẹn TẮT 0.5h + BẬT 1h (aircon11.txt) — mục MỚI
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

  // 13. MỚI (aircon11.txt): Eco -> Sleep -> Eco
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
    IRsend ir2; AcController b; b.begin(ir2);   // không xóa kho NVS: giống khởi động lại
    const AcState& s = b.state();
    CHECK(s.power && s.mode==1 && s.fan==0 && s.temp==26 && s.sleep && !s.eco && s.on_timer==0 && s.off_timer==120,
          "NVS: nạp lại power/mode/fan/temp/sleep/hẹn giờ");
    CHECK(ir2.log.empty(), "khởi động không phát IR");
    Rig e; coolOn(e); e.ac.setEco(true);
    IRsend ir3; AcController b3; b3.begin(ir3);
    CHECK(b3.state().eco && !b3.state().sleep, "NVS: nạp lại Eco");
    Rig h; coolOn(h); h.ac.setTimers(90,0);
    IRsend ir4; AcController b4; b4.begin(ir4);
    CHECK(b4.state().on_timer == 90 && b4.state().off_timer == 0, "NVS: nạp lại hẹn BẬT");
    Rig bad; { Preferences p; p.begin("ac", false); p.putBool("power", true); p.putUChar("temp", 99); }
    IRsend ir5; AcController b5; b5.begin(ir5);
    CHECK(!b5.state().power && b5.state().temp == AC_TEMP_DEFAULT && b5.state().mode == coolix::kModeAuto,
          "NVS hỏng (temp=99) -> dùng mặc định cả bộ, không dùng nửa vời"); }

  // 20. GIẢ ĐỊNH, CHƯA CÓ TÍN HIỆU THU: máy tắt đang hẹn BẬT, đổi nhiệt -> gửi lại khung hẹn BẬT.
  //     (aircon10.txt mục 3 chỉ mô tả bằng lời.) Nếu sau này thu được tín hiệu khác thì sửa ca này.
  { Rig r; r.ac.setClimate(1,0,26); r.ac.setTimers(60,0);
    CHECK(seq(r.ir,{{0xB24DBF40D383ULL,48,1}}), "máy tắt + hẹn BẬT 1h = B24DBF40D383");
    r.clear(); r.ac.setClimate(1,0,27);
    CHECK(seq(r.ir,{{0xB24DBF409383ULL,48,1}}), "[giả định] máy tắt + hẹn BẬT, tăng nhiệt = B24DBF409383");
    CHECK(!r.ac.state().power && r.ac.state().on_timer == 60, "máy vẫn tắt, hẹn BẬT còn"); }

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
  // Bật Turbo tắt Sleep/Eco; bật Sleep/Eco tắt Turbo
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
  // Đổi nhiệt giữ Turbo; đổi quạt hoặc chế độ tắt Turbo
  { Rig r; coolOn(r); r.ac.setTurbo(true); r.clear(); r.ac.setClimate(1,0,27);
    CHECK(seq(r.ir, {{w24(0xB2BF90),48,1}}) && r.ac.state().turbo, "Turbo + tăng nhiệt: khung thường, giữ Turbo");
    r.ac.setClimate(1,0,27); CHECK(r.ac.state().turbo, "đặt lại cùng quạt/chế độ: giữ Turbo");
    r.ac.setClimate(1,1,27); CHECK(!r.ac.state().turbo, "đổi quạt tắt Turbo");
    Rig m; coolOn(m); m.ac.setTurbo(true); m.ac.setClimate(2,0,26); CHECK(!m.ac.state().turbo, "đổi chế độ sang Dry tắt Turbo");
    Rig a; coolOn(a); a.ac.setTurbo(true); a.ac.setClimate(0,0,26); CHECK(!a.ac.state().turbo, "đổi chế độ sang Auto tắt Turbo"); }
  // Turbo không ảnh hưởng hẹn giờ
  { Rig r; coolOn(r); r.ac.setTimers(0,120); r.clear(); r.ac.setTurbo(true);
    CHECK(seq(r.ir, {{0xB54AF50AA25DULL,48,1}}) && r.ac.state().off_timer == 120, "bật Turbo khi hẹn TẮT: chỉ khung Turbo, giữ hẹn giờ");
    r.ac.setTurbo(false); CHECK(r.ac.state().off_timer == 120, "tắt Turbo giữ hẹn giờ");
    // SUY RA từ ca đổi nhiệt (khung trạng thái cùng chế độ/quạt giữ Turbo), chưa có tín hiệu riêng:
    Rig q; coolOn(q); q.ac.setTurbo(true); q.ac.setTimers(0,60);
    CHECK(q.ac.state().turbo, "[suy ra] đặt hẹn giờ khi Turbo giữ Turbo"); }
  // Tắt máy xóa Turbo; bật lại không còn Turbo
  { Rig r; coolOn(r); r.ac.setTurbo(true); r.ac.setPower(false);
    CHECK(!r.ac.state().turbo, "tắt máy xóa Turbo");
    r.clear(); r.ac.setPower(true);
    CHECK(seq(r.ir, {{w24(0xB2BFD0),48,1}}) && !r.ac.state().turbo, "bật lại: khung thường, không còn Turbo"); }

  // 23. Clean (aircon.txt: B5F5AA; quy luật do người dùng thử trên remote)
  CHECK(AC_CLEAN_DURATION_MS == 30UL * 60UL * 1000UL, "Clean chạy 30 phút (người dùng đo trên remote)");
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
  // Trong lúc Clean mọi lệnh khác bị từ chối, không phát IR, không đổi trạng thái
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
    CHECK(r.ac.light() != nullptr, "Clean: Light bị từ chối");
    CHECK(r.ir.log.empty(), "Clean: lệnh bị từ chối không phát IR");
    const AcState& s = r.ac.state();
    CHECK(s.power && s.clean && s.mode == 1 && s.fan == 0 && s.temp == 26 && !s.sleep && !s.eco && !s.turbo &&
          s.on_timer == 0 && s.off_timer == 0, "Clean: bị từ chối không đổi trạng thái"); }
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
  // Hết 30 phút: ESP32 tự đặt máy tắt, không phát IR, lưu NVS
  { Rig r; coolOn(r); g_millis = 1000; r.ac.setClean(true); r.clear();
    g_millis = 1000 + AC_CLEAN_DURATION_MS - 1; r.ac.update();
    CHECK(r.ac.state().clean && r.ac.state().power, "29:59: còn Clean");
    g_millis = 1000 + AC_CLEAN_DURATION_MS; r.ac.update();
    CHECK(!r.ac.state().clean && !r.ac.state().power && r.ir.log.empty(), "30:00: hết Clean, máy tắt, không phát IR");
    IRsend ir2; AcController b; b.begin(ir2);
    CHECK(!b.state().power && !b.state().clean, "NVS: sau Clean hết giờ máy tắt");
    r.clear(); CHECK(r.ac.setPower(true) == nullptr && seq(r.ir, {{w24(0xB2BFD0),48,1}}), "sau Clean hết giờ: Bật nguồn bình thường");
    Rig p; coolOn(p); g_millis = 99999999UL; p.ac.update();
    CHECK(p.ac.state().power, "update() khi không Clean: không đổi gì");
    Rig w; coolOn(w); g_millis = 0xFFFFFFFFu - 1000u; w.ac.setClean(true);
    g_millis += AC_CLEAN_DURATION_MS - 1; w.ac.update();
    CHECK(w.ac.state().clean, "millis() tràn số: 29:59 vẫn còn Clean");
    g_millis += 1; w.ac.update();
    CHECK(!w.ac.state().clean && !w.ac.state().power, "millis() tràn số: 30:00 hết Clean");
    g_millis = 0; }
  // Clean không lưu NVS: khởi động lại thì cờ Clean mất
  { Rig r; coolOn(r); r.ac.setClean(true);
    IRsend ir2; AcController b; b.begin(ir2);
    CHECK(!b.state().clean && b.state().power && ir2.log.empty(), "khởi động lại: cờ Clean mất, không phát IR"); }

  // 24. NVS: Turbo lưu/nạp; Turbo không hợp lệ -> mặc định cả bộ
  { Rig r; coolOn(r); r.ac.setTurbo(true);
    IRsend i1; AcController b1; b1.begin(i1);
    CHECK(b1.state().turbo && b1.state().power && b1.state().mode == 1 && i1.log.empty(), "NVS: nạp lại Turbo");
    Rig x1; { Preferences p; p.begin("ac", false); p.putBool("turbo", true); }
    IRsend i2; AcController b2; b2.begin(i2);
    CHECK(!b2.state().turbo && !b2.state().power, "NVS: Turbo khi máy tắt -> mặc định");
    Rig x2; { Preferences p; p.begin("ac", false); p.putBool("power", true); p.putUChar("mode", 2); p.putBool("turbo", true); }
    IRsend i3; AcController b3; b3.begin(i3);
    CHECK(!b3.state().turbo && !b3.state().power, "NVS: Turbo ở Dry -> mặc định");
    Rig x3; { Preferences p; p.begin("ac", false); p.putBool("power", true); p.putUChar("mode", 1); p.putBool("sleep", true); p.putBool("turbo", true); }
    IRsend i4; AcController b4; b4.begin(i4);
    CHECK(!b4.state().turbo && !b4.state().sleep, "NVS: Turbo + Sleep -> mặc định"); }

  printf("%d/%d dat\n", pass, pass + fail);
  return fail ? 1 : 0;
}
