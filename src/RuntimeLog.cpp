#include "RuntimeLog.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <cstdarg>
namespace {
struct Entry { char text[192]; };
QueueHandle_t queue = nullptr;
}
RuntimeLogger RuntimeLog;
void RuntimeLogger::begin() { queue = xQueueCreate(16, sizeof(Entry)); }
void RuntimeLogger::printf(const char* format, ...) {
  if (!queue) return;
  Entry entry{};
  va_list args; va_start(args, format);
  vsnprintf(entry.text, sizeof(entry.text), format, args);
  va_end(args);
  xQueueSend(queue, &entry, 0); // Drop on overflow; never delay control.
}
void RuntimeLogger::println(const char* message) { printf("%s\n", message); }
void RuntimeLogger::drain() {
  Entry entry{};
  for (int i = 0; queue && i < 4 && xQueueReceive(queue, &entry, 0) == pdTRUE; ++i)
    Serial.print(entry.text);
}
