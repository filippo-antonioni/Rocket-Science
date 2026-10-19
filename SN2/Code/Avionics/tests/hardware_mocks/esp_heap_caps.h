#pragma once
#include <cstddef>
#include <cstdint>
#define MALLOC_CAP_SPIRAM 1u
#define MALLOC_CAP_8BIT 2u
void* heap_caps_malloc(size_t,uint32_t);
size_t heap_caps_get_free_size(uint32_t);
size_t heap_caps_get_largest_free_block(uint32_t);
