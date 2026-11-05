#pragma once
#include "Arduino.h"
using TickType_t = uint32_t;
constexpr int pdPASS = 1;
constexpr uint32_t portMAX_DELAY = UINT32_MAX;
#define pdMS_TO_TICKS(ms) (ms)
struct StaticSemaphore_t { unsigned depth = 0; };
using SemaphoreHandle_t = StaticSemaphore_t*;
extern void (*mockControlTask)(void*);
extern unsigned mockTaskTicks;
struct TaskFinished {};
inline TickType_t xTaskGetTickCount() { return millis(); }
inline int xTaskCreatePinnedToCore(void (*task)(void*), const char*, int, void*, int, void*, int) { mockControlTask = task; return pdPASS; }
inline void vTaskDelayUntil(TickType_t* wake, TickType_t period) {
  *wake += period; mockNow = *wake;
  if (--mockTaskTicks == 0) throw TaskFinished{};
}
inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutexStatic(StaticSemaphore_t* buffer) { return buffer; }
inline int xSemaphoreTakeRecursive(SemaphoreHandle_t mutex, uint32_t) { ++mutex->depth; return 1; }
inline int xSemaphoreGiveRecursive(SemaphoreHandle_t mutex) { --mutex->depth; return 1; }
