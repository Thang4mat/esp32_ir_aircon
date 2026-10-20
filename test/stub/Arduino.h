#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
struct SerialStub { void println(const char*){} void printf(const char*, ...){} };
static SerialStub Serial;
// Đồng hồ giả: test tự chỉnh g_millis để giả lập thời gian trôi.
inline uint32_t g_millis = 0;
inline uint32_t millis() { return g_millis; }
