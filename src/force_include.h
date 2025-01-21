#include <stddef.h>

void* arena_ir_malloc(size_t size);
void* arena_ir_calloc(size_t num, size_t size);
void* arena_ir_realloc(void* ptr, size_t size);
void arena_ir_free(void* ptr);
#define ir_mem_malloc arena_ir_malloc
#define ir_mem_calloc arena_ir_calloc
#define ir_mem_realloc arena_ir_realloc
#define ir_mem_free arena_ir_free

// ir_disasm is written against an older capstone revision.
#define CAPSTONE_AARCH64_COMPAT_HEADER
