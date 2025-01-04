#pragma once

#include "base_config.h"
#include "base_core.h"

#if ARCH_X64
#include <intrin.h>
#endif

typedef struct StrView {
  const char* data;
  uint32_t size;
} StrView;

// base_win.c

typedef struct ReadFileResult {
  unsigned char* buffer;
  size_t file_size;
  size_t allocated_size;
} ReadFileResult;

void base_writef_stderr(const char* fmt, ...);
unsigned char* base_large_alloc_rw(size_t size);
unsigned char* base_large_alloc_rwx(size_t size);
void base_set_protection_rx(unsigned char* ptr, size_t size);
ReadFileResult base_read_file(const char* filename);
void base_exit(int rc);


// str.c

typedef struct Str {
  uint32_t i;
} Str;

extern char* str_intern_pool;

void str_intern_pool_init(void);

Str str_intern_len(const char* str, uint32_t len);

static inline FORCEINLINE const char* cstr(Str str) {
  return &str_intern_pool[str.i];
}

static inline FORCEINLINE uint32_t str_len(Str str) {
  return *(uint32_t*)&str_intern_pool[str.i - sizeof(uint32_t)];
}


// lex.c

typedef enum TokenKind {
#define TOKEN(n) TOK_##n,
#include "tokens.inc"
#undef TOKEN
  NUM_TOKEN_KINDS,
} TokenKind;

void lex_start(const uint8_t* buf, size_t byte_count_rounded_up);
void lex_next_block(uint8_t token_kinds[128], uint32_t token_offsets[128]);
StrView lex_get_strview(uint32_t from, uint32_t to);


// parse.c

typedef struct Type {
  uint32_t i;
} Type;

#define TYPEKINDS_X \
  X(NONE)           \
  X(VOID)           \
  X(BOOL)           \
  X(U8)             \
  X(I8)             \
  X(U16)            \
  X(I16)            \
  X(U32)            \
  X(I32)            \
  X(U64)            \
  X(I64)            \
  X(ENUM)           \
  X(FLOAT)          \
  X(DOUBLE)         \
  X(STR)            \
  X(RANGE)          \
  X(SLICE)          \
  X(ARRAY)          \
  X(PTR)            \
  X(FUNC)           \
  X(DICT)           \
  X(STRUCT)         \
  X(UNION)          \
  X(CONST)          \
  X(PACKAGE)        \
  X(CHAR) /* only for interop foreign calls */

typedef enum TypeKind {
#define X(x) TYPE_##x,
  TYPEKINDS_X
#undef X
      NUM_TYPE_KINDS,
} TypeKind;

void parse(const char* filename, ReadFileResult file);


// gen.c

typedef struct ContFixup {
    unsigned char* func_base;
    int32_t offset_of_list_head;
} ContFixup;

void gen_init(void);
void gen_finish_and_dump(void);
int gen_finish_and_run(void);
ContFixup gen_func_entry(void);
void gen_func_exit_and_patch_func_entry(ContFixup* fixup, Type return_type);
void gen_push_number(uint64_t val, Type suffix, ContFixup* cont);
void gen_add(ContFixup* cont);
void gen_store_local(uint32_t offset, Type type);
void gen_load_local(uint32_t offset, Type type, ContFixup* cont);
void gen_error(const char* message);
extern unsigned char* gen_p;

ContFixup snip_make_cont_fixup(unsigned char* function_base);
void snip_patch_cont_fixup(ContFixup* fixup, unsigned char* target);
