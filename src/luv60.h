#pragma once

#include "base_config.h"
#include "base_core.h"

#include "../third_party/ir/ir.h"
#include "../third_party/ir/ir_builder.h"

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

int base_writef_stderr(const char* fmt, ...);
unsigned char* base_large_alloc_rw(size_t size);  // At least 64 aligned.
//unsigned char* base_large_alloc_rwx(size_t size);
void base_large_alloc_free(void* ptr);
//void base_set_protection_rx(unsigned char* ptr, size_t size);
ReadFileResult base_read_file(const char* filename);
NORETURN void base_exit(int rc);


// str.c

typedef struct Str {
  uint32_t i;
} Str;

extern char* str_intern_pool;

void str_intern_pool_init(void);
void str_intern_pool_destroy_for_tests(void);

Str str_intern_len(const char* str, uint32_t len);
Str str_intern(const char* str);
Str str_internf(const char* fmt, ...);
Str str_process_escapes(const char* str, uint32_t len);

static inline FORCE_INLINE const char* cstr(Str str) {
  return &str_intern_pool[str.i];
}

static inline FORCE_INLINE uint32_t str_len(Str str) {
  return *(uint32_t*)&str_intern_pool[str.i - sizeof(uint32_t)];
}

static inline FORCE_INLINE bool str_eq(Str a, Str b) {
  return a.i == b.i;
}

static inline FORCE_INLINE bool str_is_none(Str s) {
  return s.i == 0;
}

// lex.c

typedef enum TokenKind {
#define TOKEN(n) TOK_##n,
#include "tokens.inc"
#undef TOKEN
  NUM_TOKEN_KINDS,
} TokenKind;

uint32_t lex_indexer(const uint8_t* buf, uint32_t byte_count_rounded_up, uint32_t* token_offsets);


// token.c

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

void type_init(void);
void type_destroy_for_tests(void);
// returned str is either the cstr() of an interned string, or a constant.
const char* type_as_str(Type type);

size_t type_size(Type type);
size_t type_align(Type type);

Type type_function(Type* params, size_t num_params, Type return_type);

static inline FORCE_INLINE bool type_is_none(Type a) { return a.u == 0; }
static inline FORCE_INLINE bool type_eq(Type a, Type b) { return a.u == b.u; }
static inline FORCE_INLINE TypeKind type_kind(Type a) { return (TypeKind)(a.u & 0xff); }

uint32_t type_func_num_params(Type type);
Type type_func_return_type(Type type);
Type type_func_param(Type type, uint32_t i);


// parse.c

void* parse(const char* filename, ReadFileResult file, bool verbose);
