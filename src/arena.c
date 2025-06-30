#include "luv60.h"

Arena* arena_create(uint64_t provided_reserve_size, uint64_t provided_commit_size) {
  uint64_t commit_size = ALIGN_UP(provided_commit_size, base_page_size());
  uint64_t reserve_size = ALIGN_UP(provided_reserve_size, base_page_size());

  void* base = base_mem_reserve(reserve_size);
  ASSERT(base);
  CHECK(base_mem_commit(base, commit_size));

  Arena* arena = base;
  arena->original_commit_size = provided_commit_size;
  arena->original_reserve_size = provided_reserve_size;
  arena->cur_pos = ARENA_HEADER_SIZE;
  arena->cur_commit = commit_size;
  arena->cur_reserve = reserve_size;
  // TODO: ASAN integration here, since IR seems to be a bit dicey.

  //base_writef_stderr("ARENA %p, %zu reserve\n", arena, reserve_size);
  return arena;
}

void arena_destroy(Arena* arena) {
  base_mem_release(arena, arena->cur_reserve);
}

void* arena_push(Arena* arena, uint64_t size, uint64_t align) {
  uint64_t pos_pre = ALIGN_UP(arena->cur_pos, align);
  uint64_t pos_post = pos_pre + size;
  //base_writef_stderr("ARENA %p, at %" PRIu64 ", push %" PRIu64 "\n", arena, arena->cur_pos, size);

  // Extend committed range, if necessary.
  if (arena->cur_commit < pos_post) {
    uint64_t commit_post_aligned = pos_post + arena->original_commit_size - 1;
    commit_post_aligned -= commit_post_aligned % arena->original_commit_size;
    uint64_t commit_post_clamped = CLAMP_MAX(commit_post_aligned, arena->cur_reserve);
    uint64_t commit_size = commit_post_clamped - arena->cur_commit;
    uint8_t* commit_ptr = (uint8_t*)arena + arena->cur_commit;
    base_mem_commit(commit_ptr, commit_size);
    arena->cur_commit = commit_post_clamped;
  }

  void* result = NULL;
  if (arena->cur_commit >= pos_post) {
    result = (uint8_t*)arena + pos_pre;
    arena->cur_pos = pos_post;
  }

  if (BRANCH_UNLIKELY(result == NULL)) {
    CHECK(false && "allocation failure");
  }

  // TODO: ASAN

  return result;
}

uint64_t arena_pos(Arena* arena) {
  return arena->cur_pos;
}

void arena_pop_to(Arena* arena, uint64_t pos) {
  uint64_t cpos = CLAMP_MIN(ARENA_HEADER_SIZE, pos);
  arena->cur_pos = cpos;
  // base_writef_stderr("ARENA %p, pop to %" PRIu64 "\n", arena, arena->cur_pos);
  //  TODO: ASAN
}
