#pragma once

#define ARENA_HEADER_SIZE 128
typedef struct Arena {
  uint64_t original_commit_size;
  uint64_t original_reserve_size;
  uint64_t cur_pos;
  uint64_t cur_commit;
  uint64_t cur_reserve;
} Arena;

_Static_assert(sizeof(Arena) < ARENA_HEADER_SIZE, "Arena too large");

Arena* arena_create(uint64_t reserve_size, uint64_t commit_size);
void arena_destroy(Arena* arena);
void* arena_push(Arena* arena, uint64_t size, uint64_t align);
uint64_t arena_pos(Arena* arena);
void arena_pop_to(Arena* arena, uint64_t pos);

