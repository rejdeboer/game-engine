#include "memory.h"
#include <cassert>

void arena_init(Arena *arena, size_t size, uint8_t *base) {
  arena->size = size;
  arena->base = base;
  arena->used = 0;
}

void *push_size_(Arena *arena, size_t size) {
  assert(arena->used + size <= arena->size);
  void *res = arena->base + arena->used;
  arena->used += size;
  return res;
}
