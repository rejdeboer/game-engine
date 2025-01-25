#if !defined(MEMORY_H)
#include <cstddef>
#include <cstdint>

struct Arena {
  size_t size;
  uint8_t *base;
  size_t used;
};

#define MEMORY_H
#endif
