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

int base_writef_stderr(const char* fmt, ...);
unsigned char* base_large_alloc_rw(size_t size);
unsigned char* base_large_alloc_rwx(size_t size);
void base_large_alloc_free(void* ptr);
void base_set_protection_rx(unsigned char* ptr, size_t size);
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
void lex_get_location_and_line_slow(uint32_t offset,
                                    uint32_t* loc_line,
                                    uint32_t* loc_column,
                                    StrView* contents);

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

static inline FORCEINLINE bool type_is_none(Type a) { return a.u == 0; }
static inline FORCEINLINE bool type_eq(Type a, Type b) { return a.u == b.u; }
static inline FORCEINLINE TypeKind type_kind(Type a) { return (TypeKind)(a.u & 0xff); }

uint32_t type_func_num_params(Type type);
Type type_func_return_type(Type type);
Type type_func_param(Type type, uint32_t i);


// parse.c

void parse(const char* filename, ReadFileResult file);


// gen.c
typedef struct ContFixup {
  unsigned char* func_base;
  int32_t offset_of_list_head;
#if BUILD_DEBUG
  const char* debug_name;
#endif
} ContFixup;

#if 0
void gen_init(void);
void gen_finish_and_dump(void);
int gen_finish_and_run(void);
void gen_error(const char* message);

void gen_func_entry(void);
void gen_func_return(ContFixup* result, Type return_type);
void gen_push_number(uint64_t val, Type suffix, ContFixup* cont);
void gen_add(ContFixup* cont);
void gen_store_local(uint32_t offset, Type type);
void gen_load_local(uint32_t offset, Type type, ContFixup* cont);
void gen_if(ContFixup* cond, ContFixup* then, ContFixup* els);
void gen_jump(ContFixup* to);

#if BUILD_DEBUG
#define gen_make_label(name) gen_make_label_impl(name)
ContFixup gen_make_label_impl(const char* debug_name);
#else
#define gen_make_label(name) gen_make_label_impl()
ContFixup gen_make_label_impl(void);
#endif

void gen_resolve_label(ContFixup* cont);

ContFixup snip_make_cont_fixup(unsigned char* function_base);
void snip_patch_cont_fixup(ContFixup* fixup, unsigned char* target);
#endif

typedef struct IRRef {
  uint32_t i;
} IRRef;

typedef struct IRBlock {
  uint32_t i;
} IRBlock;

void gen_ssa_init(const char* filename);

IRRef gen_ssa_start_function(Str name, Type return_type, int num_params, IRRef* params);
void gen_ssa_end_function(void);

IRRef gen_ssa_make_temp(Type type);
IRBlock gen_ssa_make_block_name(void);
void gen_ssa_start_block(IRBlock block);
void gen_ssa_alloc_local(size_t size, size_t align, IRRef ref);

IRRef gen_ssa_string_constant(Str str);
IRRef gen_ssa_const(uint64_t val, Type type);
void gen_ssa_store(IRRef into, Type type, IRRef val);
IRRef gen_ssa_load(IRRef from, Type type);

// some temp hacks until we have memfn to_str
void gen_ssa_print_i32(IRRef i);
void gen_ssa_print_str(IRRef s);

IRRef gen_ssa_call(Type return_type,
                   IRRef func,
                   int num_args,
                   Type* arg_types,
                   IRRef* arg_values);
void gen_ssa_jump_cond(IRRef cond, IRBlock iftrue, IRBlock iffalse);
void gen_ssa_jump(IRBlock block);
void gen_ssa_return(IRRef val, Type type);

IRRef gen_ssa_add(IRRef a, IRRef b);
IRRef gen_ssa_mul(IRRef a, IRRef b);

typedef enum IRIntCmp {
  IIC_INVALID,
  IIC_EQ,
  IIC_NE,
  IIC_SLE,
  IIC_SLT,
  IIC_SGE,
  IIC_SGT,
  IIC_ULE,
  IIC_ULT,
  IIC_UGE,
  IIC_UGT,
  NUM_IR_INT_CMPS
} IRIntCmp;

IRRef gen_ssa_int_comparison(IRIntCmp cmp, IRRef a, IRRef b);

void gen_ssa_finish(void);
