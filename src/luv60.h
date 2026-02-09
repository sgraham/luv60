#pragma once

#include "base_config.h"
#include "base_core.h"

#include "../third_party/sqbe/sqbe.h"

#if OS_WINDOWS && ARCH_X64
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

#include "dict.h"

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

void str_intern_pool_init(Arena* arena);
void str_intern_pool_destroy_for_tests(void);

Str str_intern_len(const char* str, uint32_t len);
Str str_intern(const char* str);
Str str_internf(const char* fmt, ...);
uint32_t str_process_escapes(char* str, uint32_t len);

Str str_cat(Str a, Str b);

uint32_t str_len(Str str);
const char* str_raw_ptr_impl_long_string(Str str);
#define str_raw_ptr(str) \
  ((((str).i) >> 63) ? str_raw_ptr_impl_long_string(str) : (const char*)&(str).i)

static inline FORCE_INLINE bool str_eq(Str a, Str b) {
  return a.i == b.i;
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

typedef struct StartsWithStr {
  Str s;
} StartsWithStr;

static inline bool start_str_eq_func(void* void_a, void* void_b);
static size_t inline start_str_hash_func(void* v);

static size_t inline start_str_hash_func(void* v) {
  (void)start_str_eq_func;

  StartsWithStr* sws = (StartsWithStr*)v;
  size_t hash = 0;
  const char* str_data = str_raw_ptr(sws->s);
  dict_hash_write(&hash, (void*)str_data, str_len(sws->s));
  return hash;
}

static bool start_str_eq_func(void* void_a, void* void_b) {
  (void)start_str_hash_func;

  StartsWithStr* sws_a = (StartsWithStr*)void_a;
  StartsWithStr* sws_b = (StartsWithStr*)void_b;
  return str_eq(sws_a->s, sws_b->s);
}


// path.c

void path_split(Arena* arena, const char* input, const char** source_dir, char** filename);
void path_normalize_to_slash_in_place(char* path);
void path_without_slashes_in_place(char* path);
void path_trim_extension_if_exists(char* filename, char* ext);


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
void token_init(const unsigned char* file_contents, DictImpl* import_dict);
TokenKind token_categorize(uint32_t offset, uint32_t next_offset);
int token_get_continuation_paren_level(void);
void token_restore_continuation_paren_level(int level);

// fmt_lex.c
typedef enum {
  FMTTOK_LITERAL,        // Literal text outside braces
  FMTTOK_LBRACE,         // Opening {
  FMTTOK_RBRACE,         // Closing }
  FMTTOK_FIELD_NAME,     // Field name/number (before :)
  FMTTOK_CONVERSION,     // Conversion specifier (!r, !s, !a)
  FMTTOK_COLON,          // : before format spec
  FMTTOK_FORMAT_SPEC,    // Format specification
  FMTTOK_DOT,            // . for attribute access
  FMTTOK_LBRACKET,       // [ for index/key access
  FMTTOK_RBRACKET,       // ] for index/key access
  FMTTOK_ESCAPED_BRACE,  // {{ or }}
  FMTTOK_EOF,
  FMTTOK_ERROR
} FmtTokenKind;

typedef struct {
  FmtTokenKind kind;
  StrView data;
} FmtToken;

void fmtlex_start(const char* input_cstr);
FmtToken fmtlex_next(void);


// module.c
typedef struct Module {
  uint32_t u;
} Module;

typedef struct ImportNameAndModule {
  Str name;
  Module module;
} ImportNameAndModule;

void module_init(Arena* arena,
                 Arena* parse_temp_arena,
                 const char* source_dir,
                 const char* output_dir,
                 int verbose,
                 bool syntax_only);
Module module_add(StrView basename);

typedef struct ImportedModuleScope ImportedModuleScope;

static inline bool module_eq(Module a, Module b) { return a.u == b.u; }
bool module_is_in_error(Module module);
Str module_load_path(Module module);
Str module_output_path(Module module);
Str module_import_as(Module module);
Str module_symbol_prefix(Module module);
ReadFileResult module_read_file_result(Module module);
void module_set_scope(Module module, ImportedModuleScope *scope);
ImportedModuleScope* module_get_scope(Module module);

size_t module_num_modules(void);
Module module_get_module_by_index(size_t i);


// type.c

typedef struct Type {
  uint32_t u;
} Type;

#define TYPEKINDS_X \
  X(NONE)           \
  X(VOID)           \
  X(BOOL)           \
  X(CODEPT)         \
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
  X(LIST)           \
  X(ARRAY)          \
  X(PTR)            \
  X(FUNC)           \
  X(DICT)           \
  X(STRUCT)         \
  X(UNION)          \
  X(CONST)          \
  X(MODULE)

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
#define type_codept BASIC_TYPE_CONSTANT_IMPL(TYPE_CODEPT)
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
#define type_module BASIC_TYPE_CONSTANT_IMPL(TYPE_MODULE)

void type_init(Arena* arena);
void type_destroy_for_tests(void);
// returned str is either allocated into the arena passed to type_init(), or a constant.
const char* type_as_str(Type type);

Str type_decl_name(Type type);

size_t type_size(Type type);
size_t type_align(Type type);
size_t type_padding(Type type);

typedef enum TypeFuncFlags {
  TFF_NONE = 0,
  TFF_NESTED = 1,
  TFF_MEMFN = 2,
  TFF_FOREIGN = 4,
  TFF_HAS_AGGREGATE_ARGS = 8,
} TypeFuncFlags;

Type type_function(Type* params, size_t num_params, Type return_type, TypeFuncFlags flags);
Type type_ptr(Type subtype);
Type type_array(Type subtype, size_t size);
Type type_list(Type subtype);
Type type_dict(Type key, Type value);
// structs are different than e.g. ptrs in that they're never the same as
// another one, so this is not 'intern'ing, but simply creating the Type value,
// and every call will result in a different (new) Type being returned.
Type type_new_struct(Str name,
                     uint32_t num_fields,
                     Str* field_names,
                     Type* field_types,
                     bool has_initializer);
void type_struct_set_initializer_symbol(Type type, SqSymbol init_sym);
void type_struct_set_sqtype(Type type, SqType sqtype);
void type_struct_set_module(Type type, Module module);

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
bool type_signs_match(Type a, Type b);
bool type_is_basic(Type type);

uint32_t type_func_num_params(Type type);
Type type_func_return_type(Type type);
Type type_func_param(Type type, uint32_t i);
bool type_func_is_nested(Type type);
bool type_func_is_memfn(Type type);
TypeFuncFlags type_func_flags(Type type);

Type type_ptr_subtype(Type type);

Type type_array_subtype(Type type);
uint32_t type_array_count(Type type);

Type type_list_subtype(Type type);

Type type_dict_key(Type type);
Type type_dict_value(Type type);

uint32_t type_struct_num_fields(Type type);
Str type_struct_decl_name(Type type);
bool type_struct_has_initializer(Type type);
SqSymbol type_struct_initializer_sym(Type type);
SqType type_struct_sqtype(Type type);
Module type_struct_module(Type type);
Str type_struct_field_name(Type type, uint32_t i);
Type type_struct_field_type(Type type, uint32_t i);
uint32_t type_struct_field_offset(Type type, uint32_t i);
uint32_t type_struct_field_index_by_name(Type type, Str name);  // == num_fields if not found
bool type_struct_find_field_by_name(Type type, Str name, Type* out_type, uint32_t* out_offset);


// parse.c

void parse_one_time_initialization_code_gen(Arena* main_arena, int verbose);
void parse_code_gen(Arena* temp_arena, Module module);

void parse_one_time_initialization_syntax_check(Arena* main_arena, int verbose);
void parse_syntax_check(Arena* temp_arena, Module module);
