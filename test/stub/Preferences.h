#pragma once
#include <map>
#include <string>
#include <stdint.h>
// Kho NVS giả dùng chung cho mọi đối tượng Preferences (giống flash thật: khởi tạo lại
// AcController vẫn đọc được dữ liệu đã lưu). Test gọi nvsStore().clear() để giả lập flash trống.
inline std::map<std::string, long>& nvsStore() {
  static std::map<std::string, long> m;
  return m;
}
class Preferences {
 public:
  bool begin(const char*, bool) { return true; }
  bool getBool(const char* k, bool d) { return nvsStore().count(k) ? nvsStore()[k] : d; }
  uint8_t getUChar(const char* k, uint8_t d) { return nvsStore().count(k) ? nvsStore()[k] : d; }
  uint16_t getUShort(const char* k, uint16_t d) { return nvsStore().count(k) ? nvsStore()[k] : d; }
  void putBool(const char* k, bool v) { nvsStore()[k] = v; }
  void putUChar(const char* k, uint8_t v) { nvsStore()[k] = v; }
  void putUShort(const char* k, uint16_t v) { nvsStore()[k] = v; }
};
