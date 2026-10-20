#pragma once
#include <stdint.h>
#include <vector>
struct Sent { uint64_t frame; uint16_t nbits; uint16_t repeat; };
class IRsend {
 public:
  std::vector<Sent> log;
  void sendGeneric(uint16_t, uint32_t, uint16_t, uint32_t, uint16_t, uint32_t, uint16_t, uint32_t,
                   uint64_t data, uint16_t nbits, uint16_t, bool, uint16_t repeat, uint8_t) {
    log.push_back({data, nbits, repeat});
  }
};
