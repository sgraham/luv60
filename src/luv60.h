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

// arena.c

#define ARENA_HEADER_SIZE 128
typedef struct Arena {
  uint64_t original_commit_size;
  uint64_t original_reserve_size;
  uint64_t cur_pos;
  uint64_t cur_commit;
  uint64_t cur_reserve;
} Arena;

_Static_assert(sizeof(Arena) < ARENA_HEADER_SIZE, "Arena too large");

#define KiB(size) ((size)<<10)
#define MiB(size) ((size)<<20)

Arena* arena_create(uint64_t reserve_size, uint64_t commit_size);
void arena_destroy(Arena* arena);
void* arena_push(Arena* arena, uint64_t size, uint64_t align);
uint64_t arena_pos(Arena* arena);
void arena_pop_to(Arena* arena, uint64_t pos);

extern Arena* arena_ir;


// base_{win,mac}.c

typedef struct ReadFileResult {
  unsigned char* buffer;
  size_t file_size;
  size_t allocated_size;
} ReadFileResult;

int base_writef_stderr(const char* fmt, ...);
uint64_t base_page_size(void);
void *base_mem_reserve(uint64_t size);
bool base_mem_commit(void* ptr, uint64_t size);
void *base_mem_large_alloc(uint64_t size);
void base_mem_decommit(void* ptr, uint64_t size);
void base_mem_release(void* ptr, uint64_t size);
ReadFileResult base_read_file(const char* filename);
NORETURN void base_exit(int rc);
void base_timer_init(void);
uint64_t base_timer_now(void);

// str.c

typedef struct Str {
  uint64_t i;
} Str;

void str_intern_pool_init(Arena* arena, char* parse_buffer, size_t buffer_size);
void str_intern_pool_destroy_for_tests(void);

Str str_intern_len(const char* str, uint32_t len);
Str str_intern(const char* str);
Str str_internf(const char* fmt, ...);
uint32_t str_process_escapes(char* str, uint32_t len);

uint32_t str_len(Str str);
const char* str_raw_ptr_impl_long_string(Str str);
#define str_raw_ptr(str) \
  ((((str).i) >> 63) ? str_raw_ptr_impl_long_string(str) : (const char*)&(str).i)

bool str_eq_impl_long_strings(Str a, Str b);

static inline FORCE_INLINE bool str_eq(Str a, Str b) {
  if ((a.i >> 63) + (b.i >> 63) == 0) {
    return a.i == b.i;
  } else {
    return str_eq_impl_long_strings(a, b);
  }
}

static inline FORCE_INLINE bool str_is_none(Str s) {
  return s.i == 0;
}

static inline char* cstr_copy(Arena* arena, Str s) {
  uint32_t len = str_len(s);
  char* copy = arena_push(arena, len + 1, 1);
  memcpy(copy, str_raw_ptr(s), len);
  copy[len] = 0;
  return copy;
}


// lex.c

typedef enum TokenKind {
#define TOKEN(n) TOK_##n,
#include "tokens.inc"
#undef TOKEN
  NUM_TOKEN_KINDS,
} TokenKind;

uint32_t lex_indexer(const uint8_t* buf, uint32_t byte_count_rounded_up, uint32_t* token_offsets);
void token_dump_offsets(uint32_t num_tokens, uint32_t* token_offsets, size_t file_size);


// token.c

const char* token_enum_name(TokenKind kind);
void token_init(const unsigned char* file_contents);
TokenKind token_categorize(uint32_t offset);


// type.c

typedef struct Type {
  uint32_t u;
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

#define BASIC_TYPE_CONSTANT_IMPL(typekind) \
  (Type) {                                 \
    (typekind << 8) | typekind             \
  }
#define type_none BASIC_TYPE_CONSTANT_IMPL(TYPE_NONE)
#define type_void BASIC_TYPE_CONSTANT_IMPL(TYPE_VOID)
#define type_bool BASIC_TYPE_CONSTANT_IMPL(TYPE_BOOL)
#define type_u8 BASIC_TYPE_CONSTANT_IMPL(TYPE_U8)
#define type_i8 BASIC_TYPE_CONSTANT_IMPL(TYPE_I8)
#define type_u16 BASIC_TYPE_CONSTANT_IMPL(TYPE_U16)
#define type_i16 BASIC_TYPE_CONSTANT_IMPL(TYPE_I16)
#define type_u32 BASIC_TYPE_CONSTANT_IMPL(TYPE_U32)
#define type_i32 BASIC_TYPE_CONSTANT_IMPL(TYPE_I32)
#define type_u64 BASIC_TYPE_CONSTANT_IMPL(TYPE_U64)
#define type_i64 BASIC_TYPE_CONSTANT_IMPL(TYPE_I64)
#define type_float BASIC_TYPE_CONSTANT_IMPL(TYPE_FLOAT)
#define type_double BASIC_TYPE_CONSTANT_IMPL(TYPE_DOUBLE)
#define type_str BASIC_TYPE_CONSTANT_IMPL(TYPE_STR)
#define type_range BASIC_TYPE_CONSTANT_IMPL(TYPE_RANGE)

void type_init(Arena* arena);
void type_destroy_for_tests(void);
// returned str is either allocated into the arena passed to type_init(), or a constant.
const char* type_as_str(Type type);

size_t type_size(Type type);
size_t type_align(Type type);

Type type_function(Type* params, size_t num_params, Type return_type);
Type type_ptr(Type subtype);

static inline FORCE_INLINE bool type_is_none(Type a) { return a.u == 0; }
static inline FORCE_INLINE bool type_eq(Type a, Type b) { return a.u == b.u; }
static inline FORCE_INLINE TypeKind type_kind(Type a) { return (TypeKind)(a.u & 0xff); }
bool type_is_unsigned(Type type);
bool type_is_signed(Type type);
bool type_is_arithmetic(Type type);
bool type_is_integer(Type type);
bool type_is_ptr_like(Type type);
bool type_is_aggregate(Type type);
bool type_is_condition(Type type);

uint32_t type_func_num_params(Type type);
Type type_func_return_type(Type type);
Type type_func_param(Type type, uint32_t i);

Type type_ptr_subtype(Type type);


// parse.c

void* parse_code_gen(Arena* arena,
                     Arena* temp_arena,
                     const char* filename,
                     ReadFileResult file,
                     int verbose,
                     bool ir_only,
                     int opt_level);
void* parse_syntax_check(Arena* arena,
                         Arena* temp_arena,
                         const char* filename,
                         ReadFileResult file,
                         int verbose,
                         bool ir_only,
                         int opt_level);
