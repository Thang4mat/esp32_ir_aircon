// CAPTURE: bắt tín hiệu IR từ remote gốc và in ra Serial Monitor.
// Mục đích: xác minh giao thức trước khi tin dùng mạch phát.
//
// Cach dung:
// 1. Lap mach thu IR theo so do "Mach thu IR (RX)".
// 2. pio run -t upload && pio device monitor
// 3. Hướng remote vào mắt thu, bấm Power/Cool 24C/Fan...
// 4. Đọc dòng "Protocol" trong kết quả. Remote máy lạnh Midea của dự án này cho
//    COOLIX (24 bits) với Power/Mode/Fan/Temp/Swing, COOLIX48 (48 bits) với Eco
//    và Hẹn giờ -> khớp firmware ở project "controller".
//    Nút Sleep có thể hiện sai thành BOSCH144/"Quiet": do 3 khung IR dính nhau
//    trong một lần bắt, không phải lỗi giao thức.
//    Nếu là protocol khác hoặc UNKNOWN -> xem lại raw data, không đoán mò.

#include <Arduino.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <IRac.h>
#include "config.h"

static IRrecv irrecv(IR_RX_PIN, IR_CAPTURE_BUFFER_SIZE, IR_TIMEOUT_MS, true);
static decode_results results;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(50);

  irrecv.setUnknownThreshold(IR_MIN_UNKNOWN_SIZE);
  irrecv.enableIRIn();

  Serial.println("[Capture] San sang. Huong remote vao mat thu va bam nut.");
}

void loop() {
  if (irrecv.decode(&results)) {
    Serial.println("----------------------------------------");
    Serial.println(resultToHumanReadableBasic(&results));

    // Neu thu vien nhan dien duoc day la lenh may lanh, in giai ma chi tiet
    // (mode/temp/fan...) de doi chieu voi nut da bam tren remote.
    String description = IRAcUtils::resultAcToString(&results);
    if (description.length() > 0) {
      Serial.print("[AC] ");
      Serial.println(description);
    }

    // In ma nguon C++ tuong ung, tien loi khi can luu lai lam tham khao.
    Serial.println(resultToSourceCode(&results));

    irrecv.resume();
  }
}
