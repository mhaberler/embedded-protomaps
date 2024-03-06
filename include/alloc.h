#pragma once

#ifdef ESP32
#include <esp_heap_caps.h>

#define MALLOC(size) heap_caps_malloc(size, MALLOC_CAP_SPIRAM)
#define REALLOC(buffer, size) heap_caps_realloc(buffer, size, MALLOC_CAP_SPIRAM)
#define FREE(ptr) heap_caps_free(ptr)

#else

#define MALLOC(size) malloc(size_t)
#define REALLOC(buffer, size) realloc(buffer, size)
#define FREE(ptr) heap_caps_free(ptr)

#endif
