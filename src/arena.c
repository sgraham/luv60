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

// These are vectored by force_include.h for the IR library.

typedef struct ArenaMallocHeader {
  size_t size;
} ArenaMallocHeader;

Arena* arena_ir;

void* arena_ir_aligned_alloc(size_t size, size_t align) {
  size_t extra = CLAMP_MAX(sizeof(ArenaMallocHeader), align);
  size_t total = size + extra;
  uint8_t* base_p = arena_push(arena_ir, total, align);
  uint8_t* user_p = base_p + extra;
  ((ArenaMallocHeader*)(user_p - sizeof(ArenaMallocHeader)))->size = size;
  return user_p;
}

void* arena_ir_malloc(size_t size) {
  return arena_ir_aligned_alloc(size, 8);
}

void* arena_ir_calloc(size_t num, size_t size) {
  size_t total = num * size;
  void* p = arena_ir_malloc(total);
  memset(p, 0, total);
  return p;
}

size_t arena_ir_allocated_size(void* ptr) {
  return ((ArenaMallocHeader*)((uint8_t*)ptr - sizeof(ArenaMallocHeader)))->size;
}

void* arena_ir_realloc(void* ptr, size_t size) {
  if (size == 0) {
    arena_ir_free(ptr);
    return NULL;
  }

  void* new_p = arena_ir_malloc(size);
  if (ptr) {
    memcpy(new_p, ptr, CLAMP_MIN(arena_ir_allocated_size(ptr), size));
  }
  arena_ir_free(ptr);
  return new_p;
}

void arena_ir_free(void* ptr) {
  (void)ptr;
}
