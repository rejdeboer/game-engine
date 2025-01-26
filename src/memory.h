#if !defined(MEMORY_H)
#include <cstddef>
#include <cstdint>

struct Arena {
  size_t size;
  uint8_t *base;
  size_t used;
};

void arena_init(Arena *arena, size_t size, uint8_t *base);
void *push_size_(Arena *arena, size_t size);

template <typename T> T *push_size(Arena *arena) {
  return static_cast<T *>(push_size_(arena, sizeof(T)));
}

template <typename T> T *push_array(Arena *arena, size_t count) {
  return static_cast<T *>(push_size_(arena, count * sizeof(T)));
}

#define MEMORY_H
#endif
