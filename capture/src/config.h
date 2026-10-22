#pragma once

// Chan GPIO noi mat thu hong ngoai (VS1838B / TSOP1738 / TL1838), chan OUT.
// Luu y: GPIO15 la strapping pin cua ESP32 (anh huong che do boot).
// Neu thay ESP32 boot bat thuong khi cam mat thu vao, doi sang GPIO16 hoac GPIO17.
#define IR_RX_PIN 15

// CANH BAO PHAN CUNG: thu tu chan VCC/GND/OUT tren module VS1838B khong dong
// nhat giua cac nha san xuat/lo hang — kiem tra datasheet hoac chu in tren
// board (hoac do bang VOM) truoc khi cap nguon, tranh cam nguoc VCC/GND lam
// chay module.

// Kich thuoc bo dem va thoi gian cho (theo khuyen nghi cua thu vien IRremoteESP8266)
#define IR_CAPTURE_BUFFER_SIZE 1024
#define IR_TIMEOUT_MS 50
#define IR_MIN_UNKNOWN_SIZE 12
