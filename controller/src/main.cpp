// CONTROLLER: kết nối Wi-Fi nhà, host một trang Web UI đơn giản và gửi lệnh IR
// (giao thức COOLIX của remote máy lạnh Midea) mỗi khi người dùng bấm nút.
//
// Kiến trúc "một hướng": trình duyệt -> ESP32 (Web UI + Web server) -> IR.
// Không MQTT, không cloud, không Home Assistant. Chỉ dùng Internet để lấy giờ thực (NTP),
// phục vụ hẹn giờ "Lúc hh:mm" và theo dõi hẹn giờ "Sau X giờ".
//
// Logic chọn khung IR và hẹn giờ nằm trong ac_controller.h, mã hoá khung nằm trong
// coolix_frames.h. File này chỉ nối Wi-Fi, NTP, HTTP và AcController với nhau.

#include <Arduino.h>
#include <ESPmDNS.h>
#include <IRsend.h>
#include <WebServer.h>
#include <WiFi.h>
#include <time.h>

#include "ac_controller.h"
#include "config.h"
#include "web_ui.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Thiếu file secrets.h. Chạy: cp src/secrets.h.example src/secrets.h rồi điền Wi-Fi thật."
#endif

static IRsend irsend(IR_LED_PIN);
static AcController ac;
static WebServer server(HTTP_PORT);
// mDNS phải được khởi động lại mỗi lần Wi-Fi nối lại (xem loop()), nên cần biết
// lần lặp trước Wi-Fi đã nối hay chưa, và mDNS đã chạy hay chưa (để gọi MDNS.end()).
static bool wifi_was_connected = false;
static bool mdns_active = false;
// Theo dõi đồng bộ giờ NTP để in log chẩn đoán (xem reportNtp()).
static uint32_t ntp_start_ms = 0;
static bool ntp_synced = false;
static bool ntp_warned = false;

// ---- Giờ thực ---------------------------------------------------------------

// Đồng hồ cấp cho AcController: epoch UTC (giây), 0 = chưa đồng bộ giờ.
static uint32_t nowEpoch() {
  const time_t t = time(nullptr);
  return t >= static_cast<time_t>(CLOCK_MIN_VALID_EPOCH) ? static_cast<uint32_t>(t) : 0;
}

// ---- Tiện ích HTTP ----------------------------------------------------------

static void replyError(const char* msg) {
  server.send(400, "text/plain; charset=utf-8", msg);
}

// Đọc tham số nguyên bắt buộc (chấp nhận số âm, ví dụ -1). Thiếu hoặc không phải số -> false.
static bool argInt(const char* name, int& out) {
  if (!server.hasArg(name)) return false;
  const String v = server.arg(name);
  if (v.length() == 0) return false;
  char* end = nullptr;
  const long n = strtol(v.c_str(), &end, 10);
  if (*end != '\0') return false;
  out = static_cast<int>(n);
  return true;
}

// Tham số bool bắt buộc: chỉ nhận 0 hoặc 1.
static bool argBool(const char* name, bool& out) {
  int n = 0;
  if (!argInt(name, n) || (n != 0 && n != 1)) return false;
  out = (n == 1);
  return true;
}

static void replyBadArg(const char* name) {
  char buf[64];
  snprintf(buf, sizeof(buf), "Thiếu hoặc sai tham số '%s'.", name);
  replyError(buf);
}

static void replyState() {
  const AcState& s = ac.state();
  const TimerInfo t = ac.timerInfo();
  char buf[512];
  const int n = snprintf(
      buf, sizeof(buf),
      "{\"power\":%s,\"mode\":%u,\"fan\":%u,\"temp\":%u,\"sleep\":%s,\"eco\":%s,"
      "\"turbo\":%s,\"clean\":%s,\"on_timer\":%u,\"off_timer\":%u,"
      "\"kind\":%d,\"clock\":%d,\"on_left\":%lu,\"off_left\":%lu,\"on_due\":%d,\"off_due\":%d,"
      "\"missed_on\":%d,\"missed_off\":%d}",
      s.power ? "true" : "false", s.mode, s.fan, s.temp, s.sleep ? "true" : "false",
      s.eco ? "true" : "false", s.turbo ? "true" : "false", s.clean ? "true" : "false",
      s.on_timer, s.off_timer, t.kind, t.clock, static_cast<unsigned long>(t.on_left),
      static_cast<unsigned long>(t.off_left), t.on_due, t.off_due, t.missed_on, t.missed_off);
  if (n < 0 || n >= static_cast<int>(sizeof(buf))) {
    server.send(500, "text/plain; charset=utf-8", "State JSON vượt bộ đệm.");
    return;
  }
  server.send(200, "application/json", buf);
}

// err == nullptr: thành công, trả về state mới. Ngược lại trả lỗi 400.
static void replyResult(const char* err) {
  if (err) {
    replyError(err);
  } else {
    replyState();
  }
}

// ---- Route ------------------------------------------------------------------

static void handleRoot() { server.send_P(200, "text/html", INDEX_HTML); }

static void handleState() { replyState(); }

static void handlePower() {
  bool on;
  if (!argBool("on", on)) return replyBadArg("on");
  replyResult(ac.setPower(on));
}

static void handleClimate() {
  int mode, fan, temp;
  if (!argInt("mode", mode)) return replyBadArg("mode");
  if (!argInt("fan", fan)) return replyBadArg("fan");
  if (!argInt("temp", temp)) return replyBadArg("temp");
  replyResult(ac.setClimate(mode, fan, temp));
}

// Hẹn giờ "Sau X giờ": số phút (máy lạnh tự đếm).
static void handleTimer() {
  int on_min, off_min;
  if (!argInt("on", on_min)) return replyBadArg("on");
  if (!argInt("off", off_min)) return replyBadArg("off");
  replyResult(ac.setTimers(on_min, off_min));
}

// Hẹn giờ "Lúc hh:mm": phút trong ngày 0-1439 (6:25 -> 385), -1 = không hẹn (ESP32 tự đếm).
static void handleSchedule() {
  int on_min, off_min;
  if (!argInt("on", on_min)) return replyBadArg("on");
  if (!argInt("off", off_min)) return replyBadArg("off");
  replyResult(ac.setSchedule(on_min, off_min));
}

static void handleCancel() { replyResult(ac.cancelTimers()); }

static void handleSleep() {
  bool on;
  if (!argBool("on", on)) return replyBadArg("on");
  replyResult(ac.setSleep(on));
}

static void handleEco() {
  bool on;
  if (!argBool("on", on)) return replyBadArg("on");
  replyResult(ac.setEco(on));
}

static void handleTurbo() {
  bool on;
  if (!argBool("on", on)) return replyBadArg("on");
  replyResult(ac.setTurbo(on));
}

static void handleClean() {
  bool on;
  if (!argBool("on", on)) return replyBadArg("on");
  replyResult(ac.setClean(on));
}

static void handleSwing() { replyResult(ac.swing()); }

static void handleLight() { replyResult(ac.light()); }

// ---- Wi-Fi ------------------------------------------------------------------

// Ghi lý do mất kết nối (mã lỗi 802.11) để chẩn đoán khi Wi-Fi không vào được.
static void onWiFiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  if (event != ARDUINO_EVENT_WIFI_STA_DISCONNECTED) return;
  const uint8_t reason = info.wifi_sta_disconnected.reason;
  const char* hint = "";
  switch (reason) {
    case 201: hint = " (không thấy SSID: kiểm tra tên, băng tần 2.4GHz, khoảng cách)"; break;
    case 202:
    case 2: hint = " (xác thực thất bại: sai mật khẩu hoặc lỗi anten, xem README)"; break;
    case 15: hint = " (handshake timeout: AP không tương thích)"; break;
    default: break;
  }
  Serial.printf("[WiFi] Mất kết nối, reason=%u%s\n", reason, hint);
}

static void connectWiFiOrRestart() {
  WiFi.onEvent(onWiFiEvent);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // bắt buộc khi chạy WebServer, tránh mất gói TCP đầu tiên
  const IPAddress static_ip(WIFI_STATIC_IP);
  const IPAddress gateway(WIFI_GATEWAY);
  const IPAddress subnet(WIFI_SUBNET);
  if (!WiFi.config(static_ip, gateway, subnet, gateway)) {  // phải gọi trước WiFi.begin()
    Serial.println("[WiFi] LỖI: không đặt được IP tĩnh, khởi động lại.");
    delay(1000);
    ESP.restart();
  }
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[WiFi] Đang kết nối tới %s", WIFI_SSID);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
      Serial.println();
      Serial.println("[WiFi] LỖI: không kết nối được Wi-Fi, khởi động lại.");
      delay(1000);
      ESP.restart();
    }
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.printf("[WiFi] Đã kết nối. Địa chỉ IP: %s\n", WiFi.localIP().toString().c_str());
}

// Khởi động (hoặc khởi động lại) mDNS. Gọi MDNS.end() trước nếu đang chạy để tránh
// MDNS.begin() thất bại vì responder cũ còn tồn tại.
static void startMdns() {
  if (mdns_active) MDNS.end();
  mdns_active = false;
  if (!MDNS.begin(MDNS_HOSTNAME)) {
    Serial.println("[mDNS] LỖI: không khởi động được mDNS (vẫn vào được bằng IP).");
    return;
  }
  MDNS.addService("http", "tcp", HTTP_PORT);
  mdns_active = true;
  Serial.printf("[mDNS] Sẵn sàng tại http://%s.local\n", MDNS_HOSTNAME);
}

// ---- NTP --------------------------------------------------------------------

// Chỉ log chẩn đoán, không có đường dự phòng: chưa có giờ thì hẹn giờ bị từ chối (HTTP 400).
// Gọi mỗi vòng loop() cho tới khi đã có giờ.
static void reportNtp() {
  if (ntp_synced) return;
  if (nowEpoch() != 0) {
    ntp_synced = true;
    const int m = ac.timerInfo().clock;  // phút trong ngày (UTC+7)
    Serial.printf("[NTP] Đã đồng bộ giờ: %02d:%02d (UTC+7)\n", m / 60, m % 60);
    return;
  }
  if (!ntp_warned && millis() - ntp_start_ms > NTP_WARN_TIMEOUT_MS) {
    ntp_warned = true;
    Serial.println("[NTP] CẢNH BÁO: quá 30 giây vẫn chưa có giờ nên hẹn giờ bị từ chối. "
                   "Kiểm tra router có chuyển tiếp DNS và có Internet không.");
  }
}

void setup() {
  Serial.begin(115200);
  irsend.begin();
  ac.begin(irsend, nowEpoch);

  connectWiFiOrRestart();

  // Gọi một lần sau khi Wi-Fi đã nối (connectWiFiOrRestart() chờ tới khi nối được).
  // SNTP của lwIP tự đồng bộ lại định kỳ, kể cả sau khi Wi-Fi rớt rồi nối lại.
  configTime(TIMEZONE_OFFSET_SEC, 0, NTP_SERVER);
  ntp_start_ms = millis();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/power", HTTP_GET, handlePower);
  server.on("/api/climate", HTTP_GET, handleClimate);
  server.on("/api/timer", HTTP_GET, handleTimer);
  server.on("/api/schedule", HTTP_GET, handleSchedule);
  server.on("/api/cancel", HTTP_GET, handleCancel);
  server.on("/api/sleep", HTTP_GET, handleSleep);
  server.on("/api/eco", HTTP_GET, handleEco);
  server.on("/api/turbo", HTTP_GET, handleTurbo);
  server.on("/api/clean", HTTP_GET, handleClean);
  server.on("/api/swing", HTTP_GET, handleSwing);
  server.on("/api/light", HTTP_GET, handleLight);
  server.begin();
  Serial.println("[HTTP] Web server đã sẵn sàng.");
}

void loop() {
  server.handleClient();
  ac.update();
  reportNtp();

  // Khởi động mDNS mỗi lần Wi-Fi chuyển từ "chưa nối" sang "đã nối" (lần đầu và mọi lần
  // nối lại), vì responder mDNS có thể ngừng phục vụ sau khi mạng đổi trạng thái.
  // Không gọi trong setup() vì lúc đó chưa có mạng.
  const bool connected = (WiFi.status() == WL_CONNECTED);
  if (connected && !wifi_was_connected) startMdns();
  wifi_was_connected = connected;
}
