#include "memory.h"
#include <cassert>

static void arena_init(Arena *arena, size_t size, uint8_t *base) {
  arena->size = size;
  arena->base = base;
  arena->used = 0;
}

#define push_size(arena, type) (type *)push_size_(arena, sizeof(type))
#define push_array(arena, type, count)                                         \
  (type *)push_size_(arena, count * sizeof(type))
void *push_size_(Arena *arena, size_t size) {
  assert(arena->used + size <= arena->size);
  void *res = arena->base + arena->used;
  arena->used += size;
  return res;
}
