#include "luv60.h"

#include "../third_party/sqbe/sqbe.h"

#include "dict.h"

typedef struct RuntimeStr {
  const uint8_t* data;
  int64_t length;
} RuntimeStr;

typedef struct RuntimeRange {
  int64_t start;
  int64_t stop;
  int64_t step;
} RuntimeRange;

typedef enum SymKind {
  SYM_NONE,
  SYM_VAR,
  SYM_CONST,
  SYM_FUNC,
  SYM_TYPE,
  SYM_PACKAGE,
} SymKind;

typedef union Val {
  bool b;
  uint8_t u8;
  int8_t i8;
  uint16_t u16;
  int16_t i16;
  uint32_t u32;
  int32_t i32;
  uint64_t u64;
  int64_t i64;
  uintptr_t p;
  float f;
  double d;
} Val;

typedef enum SymScopeDecl {
  SSD_NONE,
  SSD_DECLARED_GLOBAL,
  SSD_DECLARED_LOCAL,
  SSD_DECLARED_PARAMETER,
  SSD_DECLARED_NONLOCAL,
} SymScopeDecl;

typedef struct Sym {
  SymKind kind;
  Str name;
  Type type;
  union {
    SqRef ref;
    struct {
      // (kind == SYM_FUNC) or (kind == SYM_VAR and scope_decl == GLOBAL)
      SqSymbol global;
      SqRef ref2;  // Upvals for SYM_FUNC
    };
  };
  SymScopeDecl scope_decl;
} Sym;

#define MAX_SCOPES 32
#define MAX_FUNC_PARAMS 32
#define MAX_STRUCT_FIELDS 64
#define MAX_UPVALS 32
#define MAX_PACKAGE_DEPTH 16

typedef enum ScopeResult {
  SCOPE_RESULT_GLOBAL,
  SCOPE_RESULT_UNDEFINED,
  SCOPE_RESULT_LOCAL,
  SCOPE_RESULT_PARAMETER,
  SCOPE_RESULT_UPVALUE,
} ScopeResult;

typedef struct Upval {
  Str name;
  Type type;
  uint32_t offset;
  ScopeResult scope_result;
  // Only valid when SCOPE_RESULT_LOCAL or _PARAMETER, and only in the specific
  // scope it's meant for. GLOBAL/UNDEFINED are not valid, and UPVALUE means it
  // needs to be acquired through the functions $up, not this ref.
  SqRef ref;
} Upval;

typedef struct UpvalMap {
  Upval upvals[MAX_UPVALS];
  int num_upvals;
  uint32_t alloc_size;
} UpvalMap;

typedef struct SmallFlatNameSymMap {
  Str names[16];
  Sym syms[16];
  int num_entries;
} SmallFlatNameSymMap;

static void flat_name_map_init(SmallFlatNameSymMap* nm) {
  nm->num_entries = 0;
}

typedef struct Scope {
  // FuncData
  Sym* func_sym;
  Sym* return_slot;
  SqBlock return_block;
  SqItemCtx func_item_ctx;
  uint64_t arena_saved_pos;
  UpvalMap upval_map;
  SqRef upval_base;

  // VarScope
  union {
    DictImpl sym_dict;  // when is_full_dict
    SmallFlatNameSymMap flat_map;
  };
  uint64_t arena_pos;
  bool is_function;
  bool is_module;
  bool is_full_dict;
} Scope;

typedef struct TokenCursor {
  uint32_t token_index;
  TokenKind cur_kind;
  TokenKind prev_kind;
  int paren_level;
} TokenCursor;

typedef struct Parser {
  Arena* arena;
  Arena* var_scope_arena;
  const char* cur_filename;

  const char* file_contents;
  uint32_t num_tokens;
  uint32_t* token_offsets;

  TokenCursor cursor;

  TokenKind token_buffer[16];
  int num_buffered_tokens;
  int indent_levels[12];  // This is the maximum possible in lexer.
  int num_indents;

  Scope scopes[MAX_SCOPES];
  int num_scopes;
  Scope* cur_scope;

  int verbose;

  Str static_str_main;
  Str static_str_repr;
  Str static_str_ret;
  Str static_str_up;

  SqSymbol i32_print_fmt;
  SqSymbol str_print_fmt;
  SqSymbol range2_print_fmt;
  SqSymbol range3_print_fmt;
  SqSymbol str_true;
  SqSymbol str_false;

  SqType sq_type_str;
  SqType sq_type_range;

  int str_counter;
} Parser;

static Parser parser;

#define OPK_BIT_CONST 0x1
#define OPK_BIT_RVAL_REF 0x2
#define OPK_BIT_LVAL_REF 0x4
#define OPK_BIT_LOCAL_ADDR 0x8
#define OPK_BIT_SECOND_REF 0x10
#define OPK_BIT_GLOBAL_ADDR 0x20

typedef enum OpKind {
  // an actual number at compile time in .val
  OPK_CONST = OPK_BIT_CONST,

  // an immediate value in .ref
  OPK_REF_RVAL = OPK_BIT_RVAL_REF,

  // address of a local in .ref, not a named variable (e.g. a compound
  // literal, range, etc.)
  OPK_REF_RVAL_LOCAL_ADDR = OPK_BIT_RVAL_REF | OPK_BIT_LOCAL_ADDR,

  // address of a local in .ref to a named variable (could be stored to)
  OPK_REF_LVAL_LOCAL_ADDR = OPK_BIT_LVAL_REF | OPK_BIT_LOCAL_ADDR,

  // global address as const in .ref, and an additional address that points at
  // the closure block in .ref2
  OPK_REF_RVAL_LOCAL_ADDR_BOUND_FUNC = OPK_BIT_SECOND_REF | OPK_BIT_RVAL_REF | OPK_BIT_LOCAL_ADDR,

  // global address as const in .ref (read-only; generally a function address)
  OPK_REF_RVAL_GLOBAL_ADDR = OPK_BIT_RVAL_REF | OPK_BIT_GLOBAL_ADDR,

  // global address as const in .ref (read-only; generally a function address),
  // and a self pointer in .ref2.
  OPK_REF_RVAL_GLOBAL_ADDR_BOUND_FUNC = OPK_BIT_SECOND_REF | OPK_BIT_RVAL_REF | OPK_BIT_GLOBAL_ADDR,

  // global variable address as const in .ref (could be stored to)
  OPK_REF_LVAL_GLOBAL_ADDR = OPK_BIT_LVAL_REF | OPK_BIT_GLOBAL_ADDR,
} OpKind;

typedef struct Operand {
  OpKind kind;
  Type type;
  union {
    Val val;
    SqRef ref;
  };
  SqRef ref2; // Used for fat function pointers and $up.
} Operand;

static inline FORCE_INLINE bool op_is_const(Operand op) {
  return op.kind == OPK_CONST;
}

static inline FORCE_INLINE bool op_is_local_addr(Operand op) {
  return op.kind & OPK_BIT_LOCAL_ADDR;
}

static inline FORCE_INLINE bool op_is_global_addr(Operand op) {
  return op.kind & OPK_BIT_GLOBAL_ADDR;
}

static inline FORCE_INLINE bool op_is_null(Operand op) {
  return op.kind == 0;
}

static inline FORCE_INLINE bool op_has_ref2(Operand op) {
  return op.kind & OPK_BIT_SECOND_REF;
}

typedef struct OpVec {
  union {
    Operand* data;
    Operand short_data[16];
  };
  Arena* arena;
  int64_t size;
  int64_t capacity;
} OpVec;

static inline FORCE_INLINE void opv_init(OpVec* vec, Arena* arena) {
  vec->size = 0;
  vec->capacity = COUNTOFI(vec->short_data);
  vec->arena = arena;
}

static inline FORCE_INLINE void opv_free(OpVec* vec) {
  vec->size = 0;
}

static inline FORCE_INLINE int64_t opv_size(OpVec* vec) {
  return vec->size;
}

static void opv_ensure_capacity(OpVec* vec, int64_t size) {
  if (size <= vec->capacity) {
    return;
  }

  // capacity starts at short_data len, so always moving to allocated if
  // growing.
  Operand* new = arena_push(vec->arena, sizeof(Operand) * size, _Alignof(Operand));
  Operand* old = vec->capacity <= COUNTOFI(vec->short_data) ? vec->short_data : vec->data;
  memcpy(new, old, sizeof(Operand) * vec->size);
  vec->capacity = size;
}

static Operand opv_at(OpVec* vec, int64_t i) {
  ASSERT(i < vec->size);
  if (vec->capacity <= COUNTOFI(vec->short_data)) {
    return vec->short_data[i];
  }
  return vec->data[i];
}

static void opv_set(OpVec* vec, int64_t i, Operand op) {
  ASSERT(i < vec->size);
  if (vec->capacity <= COUNTOFI(vec->short_data)) {
    vec->short_data[i] = op;
  } else {
    vec->data[i] = op;
  }
}

static void opv_append(OpVec* vec, Operand op) {
  opv_ensure_capacity(vec, vec->size + 1);
  ++vec->size;
  opv_set(vec, vec->size - 1, op);
}

static Operand operand_null;

typedef enum LastStatementType {
  LST_NON_RETURN,
  LST_RETURN_VOID,
  LST_RETURN_VALUE,
} LastStatementType;

static LastStatementType parse_statement(bool toplevel);
static Operand parse_expression(Type* expected);
static LastStatementType parse_block(void);

static SqType type_to_sqtype(Type type) {
  if (type_kind(type) == TYPE_STR) {
    return parser.sq_type_str;
  }
  if (type_kind(type) == TYPE_RANGE) {
    return parser.sq_type_range;
  }
  if (type_is_aggregate(type)) {
    ASSERT(false && "todo; aggregate");
    return sq_type_void;
  }
  switch (type_kind(type)) {
    case TYPE_VOID:
      return sq_type_void;
    case TYPE_BOOL:
      return sq_type_ubyte;
    case TYPE_U8:
      return sq_type_ubyte;
    case TYPE_U16:
      return sq_type_uhalf;
    case TYPE_U32:
      return sq_type_word;
    case TYPE_U64:
      return sq_type_long;
    case TYPE_I8:
      return sq_type_sbyte;
    case TYPE_I16:
      return sq_type_shalf;
    case TYPE_I32:
      return sq_type_word;
    case TYPE_I64:
      return sq_type_long;
    case TYPE_DOUBLE:
      return sq_type_double;
    case TYPE_FLOAT:
      return sq_type_single;
    case TYPE_PTR:
      return sq_type_long;
    default:
      base_writef_stderr("type_to_sqtype: %s\n", type_as_str(type));
      ASSERT(false && "todo");
      abort();
  }
}

static ScopeResult scope_lookup_single(Scope* scope, Str name, bool crossed_function, Sym** sym);
static ScopeResult scope_lookup_recursive(Str name, Sym** sym);

#if 0
static Operand operand_sym(Type type, LqSymbol lqsym) {
  return (Operand){.type = type, .opkind = OpKindSymbol, .lqsym = lqsym};
}
#endif

static Operand operand_lvalue_local(Type type, SqRef ref) {
  return (Operand){.kind = OPK_REF_LVAL_LOCAL_ADDR, .type = type, .ref = ref};
}

static Operand operand_rvalue_local_addr(Type type, SqRef ref) {
  return (Operand){.kind = OPK_REF_RVAL_LOCAL_ADDR, .type = type, .ref = ref};
}

static Operand operand_bound_local_function(Type type, SqRef ref, SqRef ref2) {
  return (Operand){
      .kind = OPK_REF_RVAL_LOCAL_ADDR_BOUND_FUNC, .type = type, .ref = ref, .ref2 = ref2};
}

static Operand operand_rvalue_global_addr(Type type, SqRef ref) {
  return (Operand){.kind = OPK_REF_RVAL_GLOBAL_ADDR, .type = type, .ref = ref};
}

static Operand operand_rvalue_global_addr_bound(Type type, SqRef ref, SqRef ref2) {
  return (Operand){
      .kind = OPK_REF_RVAL_GLOBAL_ADDR_BOUND_FUNC, .type = type, .ref = ref, .ref2 = ref2};
}

static Operand operand_lvalue_global_addr(Type type, SqRef ref) {
  return (Operand){.kind = OPK_REF_LVAL_GLOBAL_ADDR, .type = type, .ref = ref};
}

static Operand operand_rvalue_imm(Type type, SqRef ref) {
  return (Operand){.kind = OPK_REF_RVAL, .type = type, .ref = ref};
}

static Operand operand_const(Type type, Val val) {
  return (Operand){.kind = OPK_CONST, .type = type, .val = val};
}

static inline uint32_t cur_offset(void) {
  return parser.token_offsets[parser.cursor.token_index];
}

static inline uint32_t prev_offset(void) {
  return parser.token_offsets[parser.cursor.token_index - 1];
}

static StrView get_strview_for_offsets(uint32_t from, uint32_t to) {
  return (StrView){(const char*)&parser.file_contents[from], to - from};
}

static void get_location_and_line_slow(uint32_t offset,
                                       uint32_t* loc_line,
                                       uint32_t* loc_column,
                                       StrView* contents) {
  const char* line_start = (const char*)&parser.file_contents[0];
  uint32_t line = 1;
  uint32_t col = 1;
  const char* find = (const char*)&parser.file_contents[offset];
  for (const char* p = (const char*)&parser.file_contents[0];; ++p) {
    ASSERT(*p != 0);
    if (p == find) {
      const char* line_end = strchr(p, '\n');  // TODO: error on file w/o newline
      *loc_line = line;
      *loc_column = col;
      *contents = (StrView){line_start, line_end - line_start};
      return;
    }
    if (*p == '\n') {
      line += 1;
      col = 1;
      line_start = p + 1;
    } else {
      col += 1;
    }
  }
}

NORETURN static void error_offset(uint32_t offset, const char* message) {
  uint32_t loc_line;
  uint32_t loc_column;
  StrView line;
  get_location_and_line_slow(offset, &loc_line, &loc_column, &line);
  int indent = base_writef_stderr("%s:%d:%d:", parser.cur_filename, loc_line, loc_column);
  base_writef_stderr("%.*s\n", (int)line.size, line.data);
  base_writef_stderr("%*s", indent + loc_column - 1, "");
  base_writef_stderr("^ error: %s\n", message);
  base_exit(1);
}

NORETURN static void error(const char* message) {
  error_offset(prev_offset(), message);
}

NORETURN static void errorf(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  size_t n = 1 + vsnprintf(NULL, 0, fmt, args);
  va_end(args);
  char* str = malloc(n);  // just a simple malloc because we're going to base_exit() momentarily.
  va_start(args, fmt);
  vsnprintf(str, n, fmt, args);
  va_end(args);
  error(str);
}

NORETURN static void errorf_offset(uint32_t offset, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  size_t n = 1 + vsnprintf(NULL, 0, fmt, args);
  va_end(args);
  char* str = malloc(n);  // just a simple malloc because we're going to base_exit() momentarily.
  va_start(args, fmt);
  vsnprintf(str, n, fmt, args);
  va_end(args);
  error_offset(offset, str);
}

static SqType sqbasetype_from_type(Type type) {
  if (type_size(type) == 8) {
    return sq_type_long;
  }
  return sq_type_word;
}

typedef SqRef (*ExtFunc)(SqType, SqRef);

static SqRef _sextsw(SqType size_class, SqRef arg0) {
  ASSERT(size_class.u == sq_type_long.u);
  return sq_i_extuw(arg0);
}

static ExtFunc sext_by_type(Type type) {
  ASSERT(type_is_integer(type));
  ASSERT(type_is_signed(type));
  switch (type_kind(type)) {
    case TYPE_I8:
      return sq_i_extsb;
    case TYPE_I16:
      return sq_i_extsh;
    case TYPE_I32:
      return _sextsw;
    case TYPE_I64:
      error("shouldn't be sext'ing i64");
    default:
      error("unhandled sext");
  }
}

static SqRef _zextuw(SqType size_class, SqRef arg0) {
  ASSERT(size_class.u == sq_type_long.u);
  return sq_i_extuw(arg0);
}

static ExtFunc zext_by_type(Type type) {
  ASSERT(type_is_integer(type));
  ASSERT(!type_is_signed(type));
  switch (type_kind(type)) {
    case TYPE_U8:
      return sq_i_extub;
    case TYPE_U16:
      return sq_i_extuh;
    case TYPE_U32:
      return _zextuw;
    case TYPE_U64:
      error("shouldn't be zext'ing u64");
    default:
      error("unhandled zext");
  }
}

typedef void (*StoreFunc)(SqRef, SqRef);

static void do_memcpy(SqRef from, SqRef into) {
  SqRef memcpy_func = sq_ref_extern("memcpy");
  // TODO 16 for str hardcoded!!!
  sq_i_call3(sq_type_void, memcpy_func, (SqCallArg){sq_type_long, into},
             (SqCallArg){sq_type_long, from}, (SqCallArg){sq_type_long, sq_const_int(16)});
}

static StoreFunc store_by_type(Type type) {
  if (type_is_aggregate(type)) {
    return do_memcpy;
  }
  switch (type_size(type)) {
    case 8:
      return sq_i_storel;
    case 4:
      return sq_i_storew;
    case 2:
      return sq_i_storeh;
    case 1:
      return sq_i_storeb;
    default:
      errorf("invalid store size %zu", type_size(type));
  }
}

typedef SqRef (*LoadFunc)(SqType, SqRef);

static LoadFunc load_by_type(Type type) {
  if (type_kind(type) == TYPE_BOOL) {
    return sq_i_loadub;
  } else if (type_is_unsigned(type)) {
    switch (type_size(type)) {
      case 8:
        return sq_i_load;
      case 4:
        return sq_i_loaduw;
      case 2:
        return sq_i_loaduh;
      case 1:
        return sq_i_loadub;
      default:
        errorf("invalid unsigned load size %zu", type_size(type));
    }
  } else {
    switch (type_size(type)) {
      case 8:
        return sq_i_load;
      case 4:
        return sq_i_loadsw;
      case 2:
        return sq_i_loadsh;
      case 1:
        return sq_i_loadsb;
      default:
        errorf("invalid signed load size %zu", type_size(type));
    }
  }
}

static SqRef operand_to_sqref_imm(Operand* op) {
  switch (op->kind) {
    case OPK_CONST: {
      switch (type_kind(op->type)) {
        case TYPE_BOOL:
          return sq_const_int(op->val.b);
        case TYPE_I8:
          return sq_const_int(op->val.i8);
        case TYPE_U8:
          return sq_const_int((int64_t)op->val.u8);
        case TYPE_I16:
          return sq_const_int(op->val.i16);
        case TYPE_U16:
          return sq_const_int((int64_t)op->val.u16);
        case TYPE_I32:
          return sq_const_int(op->val.i32);
        case TYPE_U32:
          return sq_const_int((int64_t)op->val.u32);
        case TYPE_I64:
          return sq_const_int(op->val.i64);
        case TYPE_U64:
          return sq_const_int((int64_t)op->val.u64);
#if 0
        case TYPE_FLOAT:
          return ir_CONST_FLOAT(op->val.f);
        case TYPE_DOUBLE:
          return ir_CONST_DOUBLE(op->val.d);
#endif
        default:
          error("internal error: unexpected const type.");
      }
    }
    case OPK_REF_RVAL:
      return op->ref;
    case OPK_REF_RVAL_LOCAL_ADDR_BOUND_FUNC:  // assume something else will load ref2
    case OPK_REF_LVAL_LOCAL_ADDR:
    case OPK_REF_RVAL_LOCAL_ADDR: {
      if (type_is_aggregate(op->type)) {
        // TODO: This seems questionable.
        return op->ref;
      }
      LoadFunc func = load_by_type(op->type);
      return func(sqbasetype_from_type(op->type), op->ref);
    }
    case OPK_REF_RVAL_GLOBAL_ADDR:
    case OPK_REF_LVAL_GLOBAL_ADDR:
      if (type_is_aggregate(op->type)) {
        // TODO: This seems questionable. see test/print.luv
        return op->ref;
      }
      LoadFunc func = load_by_type(op->type);
      return func(sqbasetype_from_type(op->type), op->ref);
    default:
      error("internal error: unhandled OpKind");
  }
}

#if 0
static ir_ref addr_for_operand(Operand* op) {
  ir_ref var = ir_VAR(type_to_ir_type(op->type), "&");
  ir_VSTORE(var, operand_to_irref_imm(op));
  return ir_VADDR(var);
}
#endif

typedef struct NameSymPair {
  Str name;
  Sym sym;
} NameSymPair;

static size_t namesym_hash_func(void* vnsp) {
  NameSymPair* nsp = (NameSymPair*)vnsp;
  size_t hash = 0;
  const char* str_data = str_raw_ptr(nsp->name);
  dict_hash_write(&hash, (void*)str_data, str_len(nsp->name));
  return hash;
}

static bool namesym_eq_func(void* void_nsp_a, void* void_nsp_b) {
  NameSymPair* nsp_a = (NameSymPair*)void_nsp_a;
  NameSymPair* nsp_b = (NameSymPair*)void_nsp_b;
  return str_eq(nsp_a->name, nsp_b->name);
}

// Returns pointer into dict where Sym is stored by value, probably bad idea.
static Sym* sym_new(SymKind kind, Str name, Type type) {
  ASSERT(parser.cur_scope);
  if (parser.cur_scope->is_full_dict) {
    NameSymPair nsp = {.name = name,
                      .sym = {
                          .kind = kind,
                          .name = name,
                          .type = type,
                      }};
    DictInsert res = dict_insert(&parser.cur_scope->sym_dict, &nsp, namesym_hash_func,
                                namesym_eq_func, sizeof(NameSymPair), _Alignof(NameSymPair));
    return &((NameSymPair*)dict_rawiter_get(&res.iter))->sym;
  } else {
    SmallFlatNameSymMap* nm = &parser.cur_scope->flat_map;
    int count = nm->num_entries;

    if (count == COUNTOFI(nm->names)) {
      // flat_map is full, 'rehash' into full dict

      // Can't immediately put into cur_scope because the flat_map and
      // dict_sym are a union.
      DictImpl new_dict = dict_new(parser.var_scope_arena, COUNTOFI(nm->names) * 4,
                                   sizeof(NameSymPair), _Alignof(NameSymPair));
      for (int i = 0; i < count; ++i) {
        NameSymPair nsp = {.name = nm->names[i], .sym = nm->syms[i]};
        dict_insert(&new_dict, &nsp, namesym_hash_func, namesym_eq_func,
                    sizeof(NameSymPair), _Alignof(NameSymPair));
      }

      // Now flat_map is dead, overrwrite with the dict and update the bool to
      // indicate we have a full dict.
      parser.cur_scope->is_full_dict = true;
      parser.cur_scope->sym_dict = new_dict;

      // Call the other branch to actually insert the new sym.
      return sym_new(kind, name, type);
    }

    nm->names[nm->num_entries] = name;
    nm->syms[nm->num_entries] = (Sym){.kind = kind, .name = name, .type = type};
    Sym* ret = &nm->syms[nm->num_entries++];
    return ret;
  }
}

static void print_i32(Operand* op) {
  SqRef val = operand_to_sqref_imm(op);
  SqRef print_func = sq_ref_extern("printf");
  SqRef fmt_str = sq_ref_for_symbol(parser.i32_print_fmt);
  sq_i_call3(sq_type_void, print_func, (SqCallArg){sq_type_long, fmt_str}, sq_varargs_begin,
             (SqCallArg){sq_type_word, val});
}

static void print_bool(Operand* op) {
  SqRef val = operand_to_sqref_imm(op);
  SqRef print_func = sq_ref_extern("puts");

  SqBlock true_block = sq_block_declare();
  SqBlock false_block = sq_block_declare();
  SqBlock after_block = sq_block_declare();

  sq_i_jnz(val, true_block, false_block);

  sq_block_start(true_block);
  sq_i_call1(sq_type_void, print_func,
             (SqCallArg){sq_type_long, sq_ref_for_symbol(parser.str_true)});
  sq_i_jmp(after_block);

  sq_block_start(false_block);
  sq_i_call1(sq_type_void, print_func,
             (SqCallArg){sq_type_long, sq_ref_for_symbol(parser.str_false)});

  sq_block_start(after_block);
}

static void print_str(Operand* op) {
  SqRef obj = operand_to_sqref_imm(op);
  SqRef print_func = sq_ref_extern("printf");
  SqRef fmt_str = sq_ref_for_symbol(parser.str_print_fmt);
  SqRef ptr = sq_i_load(sq_type_long, obj);
  SqRef len = sq_i_load(sq_type_word, sq_i_add(sq_type_long, obj, sq_const_int(8)));
  sq_i_call4(sq_type_void, print_func, (SqCallArg){sq_type_long, fmt_str}, sq_varargs_begin,
             (SqCallArg){sq_type_word, len}, (SqCallArg){sq_type_long, ptr});
}

static void print_range(Operand* op) {
  SqRef obj = operand_to_sqref_imm(op);
  SqRef print_func = sq_ref_extern("printf");

  SqBlock block_2 = sq_block_declare();
  SqBlock block_3 = sq_block_declare();
  SqBlock block_after = sq_block_declare();

  SqRef start = sq_i_load(sq_type_long, obj);
  SqRef stop = sq_i_load(sq_type_long, sq_i_add(sq_type_long, obj, sq_const_int(8)));
  SqRef step = sq_i_load(sq_type_long, sq_i_add(sq_type_long, obj, sq_const_int(16)));
  SqRef cmp = sq_i_ceql(sq_type_long, step, sq_const_int(1));
  sq_i_jnz(cmp, block_2, block_3);

  sq_block_start(block_2);
  SqRef fmt_str_2 = sq_ref_for_symbol(parser.range2_print_fmt);
  sq_i_call4(sq_type_void, print_func, (SqCallArg){sq_type_long, fmt_str_2}, sq_varargs_begin,
             (SqCallArg){sq_type_long, start}, (SqCallArg){sq_type_long, stop});
  sq_i_jmp(block_after);

  sq_block_start(block_3);
  SqRef fmt_str_3 = sq_ref_for_symbol(parser.range3_print_fmt);
  sq_i_call5(sq_type_void, print_func, (SqCallArg){sq_type_long, fmt_str_3}, sq_varargs_begin,
             (SqCallArg){sq_type_long, start}, (SqCallArg){sq_type_long, stop},
             (SqCallArg){sq_type_long, step});

  sq_block_start(block_after);
}

#if 0
static void print_float_impl(float val) {
  printf("%f\n", val);
}

static void print_float(Operand* op) {
  ir_ref addr = ir_CONST_ADDR(print_float_impl);
  ir_CALL_1(IR_VOID, addr, operand_to_irref_imm(op));
}

static void print_double_impl(double val) {
  printf("%f\n", val);
}

static void print_double(Operand* op) {
  ir_ref addr = ir_CONST_ADDR(print_double_impl);
  ir_CALL_1(IR_VOID, addr, operand_to_irref_imm(op));
}

static void print_range_impl(RuntimeRange range) {
  if (range.step == 1) {
    printf("range(%" PRIi64 ", %" PRIi64 ")\n", range.start, range.stop);
  } else {
    printf("range(%" PRIi64 ", %" PRIi64 ", %" PRIi64 ")\n", range.start, range.stop, range.step);
  }
}

#endif

static void initialize_aggregate(SqRef base_addr, Type type) {
  size_t size = type_size(type);
  if (type_kind(type) == TYPE_STRUCT && type_struct_has_initializer(type)) {
    ASSERT(false && "todo");
#if 0
    ir_ref memcpy_addr = ir_CONST_ADDR(memcpy);
    ir_ref default_blob = ir_CONST_ADDR(type_struct_initializer_blob(type));
    ir_CALL_3(IR_VOID, memcpy_addr, base_addr, default_blob, ir_CONST_U64(size));
#endif
  } else {
    SqRef memset_func = sq_ref_extern("memset");
    sq_i_call3(sq_type_void, memset_func, (SqCallArg){sq_type_long, base_addr},
               (SqCallArg){sq_type_word, sq_const_int(0)},
               (SqCallArg){sq_type_long, sq_const_int(size)});
  }
}

static Sym* make_local_and_alloc(SymKind kind, Str name, Type type, Operand* initial_value) {
  Sym* new = sym_new(kind, name, type);
  if (type_kind(type) == TYPE_STR) {
    new->ref = sq_i_alloc8(sq_const_int(type_size(type)));
    if (initial_value) {
      SqRef init = operand_to_sqref_imm(initial_value);
      sq_i_storel(sq_i_load(sq_type_long, init), new->ref);
      sq_i_storel(sq_i_load(sq_type_long, sq_i_add(sq_type_long, init, sq_const_int(8))), new->ref);
    } else {
      sq_i_storel(sq_const_int(0), new->ref);
      sq_i_storel(sq_const_int(0), sq_i_add(sq_type_long, new->ref, sq_const_int(8)));
    }
  } else if (type_kind(type) == TYPE_RANGE) {
    ASSERT(false && "local alloc range");
  } else if (type_is_aggregate(type)) {
    if (initial_value) {
      if (!type_eq(initial_value->type, type)) {
        errorf("Cannot initialize aggregate type %s with type %s.",
               type_as_str(initial_value->type), type_as_str(type));
      }
      // We're stealing this value, I think it's OK though for aggregates, and
      // they have to have been just created (?). Alternatively, memcpy to a new
      // alloca, but I think that probably won't be optimized away.
      new->ref = initial_value->ref;
      initial_value->ref = (SqRef){0};
    } else {
      uint32_t size = type_size(type);
      new->ref = sq_i_alloc8(sq_const_int(size));
      initialize_aggregate(new->ref, type);
    }
  } else {
    new->ref = sq_i_alloc8(sq_const_int(type_size(type)));
    StoreFunc store_func = store_by_type(type);
    if (initial_value) {
      store_func(operand_to_sqref_imm(initial_value), new->ref);
    } else {
      store_func(sq_const_int(0), new->ref);
    }
  }
  new->scope_decl = SSD_DECLARED_LOCAL;
  return new;
}

static Sym* make_global(SymKind kind, Str name, Type type, Val initial_value) {
  Sym* new = sym_new(kind, name, type);
  sq_data_start(sq_linkage_default, cstr_copy(parser.arena, name));
  switch (type_kind(type)) {
    case TYPE_BOOL:
      sq_data_byte((uint8_t)initial_value.b);
      break;
    case TYPE_U8:
      sq_data_byte(initial_value.u8);
      break;
    case TYPE_I8:
      sq_data_byte((uint8_t)initial_value.i8);
      break;
    case TYPE_U16:
      sq_data_half(initial_value.u16);
      break;
    case TYPE_I16:
      sq_data_half((uint16_t)initial_value.i16);
      break;
    case TYPE_U32:
      sq_data_word(initial_value.u32);
      break;
    case TYPE_I32:
      sq_data_word((uint32_t)initial_value.i32);
      break;
    case TYPE_U64:
      sq_data_long(initial_value.u64);
      break;
    case TYPE_I64:
      sq_data_long((uint64_t)initial_value.i64);
      break;
    default:
      error("internal error: unexpected global const init.");
  }
  new->global = sq_data_end();
  new->scope_decl = SSD_DECLARED_GLOBAL;

  if (parser.cur_scope->is_function) {
    sq_itemctx_activate(parser.cur_scope->func_item_ctx);
  }

  return new;
}

static Sym* make_param(Str name, Type type, int index) {
  Sym* new = sym_new(SYM_VAR, name, type);
  new->ref = sq_func_param_named(type_to_sqtype(type),
#if BUILD_DEBUG
                                 cstr_copy(parser.arena, name)
#else
                                 NULL
#endif
  );
  new->scope_decl = SSD_DECLARED_PARAMETER;
  return new;
}

static void enter_scope(bool is_module, bool is_function, Sym* funcsym) {
  parser.cur_scope = &parser.scopes[parser.num_scopes++];
  parser.cur_scope->func_sym = funcsym;
  //parser.cur_scope->arena_saved_pos = arena_pos(arena_ir);
  parser.cur_scope->upval_map.num_upvals = 0;
  parser.cur_scope->arena_pos = arena_pos(parser.var_scope_arena);
  parser.cur_scope->is_function = is_function;
  parser.cur_scope->is_module = is_module;
  parser.cur_scope->is_full_dict = !is_function;
  if (parser.cur_scope->is_full_dict) {
    parser.cur_scope->sym_dict =
        dict_new(parser.var_scope_arena, 1 << 20, sizeof(NameSymPair), _Alignof(NameSymPair));
  } else {
    flat_name_map_init(&parser.cur_scope->flat_map);
  }
}

static void leave_scope(void) {
  arena_pop_to(parser.var_scope_arena, parser.cur_scope->arena_pos);
  --parser.num_scopes;
  ASSERT(parser.num_scopes >= 0);
  if (parser.num_scopes == 0) {
    parser.cur_scope = NULL;
  } else {
    parser.cur_scope = &parser.scopes[parser.num_scopes - 1];
  }
}

static void enter_function(Sym* sym,
                           Str param_names[MAX_FUNC_PARAMS],
                           Type param_types[MAX_FUNC_PARAMS]) {
  bool is_nested = parser.num_scopes > 1;  // Module, parent.
  if (is_nested) {
    ASSERT(parser.scopes[parser.num_scopes - 1].is_function);
    ASSERT(parser.scopes[0].is_module);
  }

  enter_scope(/*is_module=*/false, /*is_function=*/true, sym);

  SqLinkage linkage =
      str_eq(sym->name, parser.static_str_main) ? sq_linkage_export : sq_linkage_default;

  Type ret_type = type_func_return_type(sym->type);

  parser.cur_scope->func_item_ctx =
      sq_func_start(linkage, type_to_sqtype(ret_type), cstr_copy(parser.arena, sym->name));

  uint32_t num_params = type_func_num_params(sym->type);
  Sym* param_syms[MAX_FUNC_PARAMS];
  for (uint32_t i = 0; i < num_params; ++i) {
    param_syms[i] = make_param(param_names[i], type_func_param(sym->type, i), i);
  }

  if (type_eq(ret_type, type_void)) {
    parser.cur_scope->return_slot = NULL;
  } else {
    parser.cur_scope->return_slot =
        make_local_and_alloc(SYM_VAR, parser.static_str_ret, ret_type, NULL);
  }
  parser.cur_scope->return_block = sq_block_declare();

  if (is_nested) {
    ASSERT(str_eq(param_syms[0]->name, parser.static_str_up));
    ASSERT(type_kind(param_syms[0]->type) == TYPE_PTR);
    ASSERT(type_eq(type_ptr_subtype(param_syms[0]->type), type_void));
    parser.cur_scope->upval_base = param_syms[0]->ref;
  }
}

static void leave_function(void) {
  Type ret_type = type_func_return_type(parser.cur_scope->func_sym->type);
  sq_block_start(parser.cur_scope->return_block);
  if (type_eq(ret_type, type_void)) {
    sq_i_ret_void();
  } else {
    if (type_is_aggregate(ret_type)) {
      sq_i_ret(parser.cur_scope->return_slot->ref);
    } else {
      LoadFunc func = load_by_type(ret_type);
      sq_i_ret(func(sqbasetype_from_type(ret_type), parser.cur_scope->return_slot->ref));
    }
  }

  parser.cur_scope->func_sym->global = sq_func_end();
  parser.cur_scope->func_item_ctx = (SqItemCtx){0};

  bool is_nested = parser.num_scopes > 2;  // Module, parent function, current function.
  if (is_nested) {
    ASSERT(parser.scopes[parser.num_scopes - 1].is_function);
    ASSERT(parser.scopes[parser.num_scopes - 2].is_function);
    ASSERT(parser.scopes[0].is_module);
  }

  if (is_nested) {
    // This is pointing to the nested one, but we have to set cur_scope to the
    // parent one, so that codegen goes to it.
    UpvalMap* inner_uvm = &parser.cur_scope->upval_map;
    Sym* child_func = parser.cur_scope->func_sym;
    parser.cur_scope = &parser.scopes[parser.num_scopes - 2];
    UpvalMap* parent_uvm = &parser.cur_scope->upval_map;
    sq_itemctx_activate(parser.cur_scope->func_item_ctx);

    SqRef upval_data = sq_i_alloc8(sq_const_int(inner_uvm->alloc_size));
    child_func->ref2 = upval_data;

    for (int i = 0; i < inner_uvm->num_upvals; ++i) {
      Upval* uv = &inner_uvm->upvals[i];
      switch (uv->scope_result) {
        case SCOPE_RESULT_GLOBAL:
        case SCOPE_RESULT_UNDEFINED:
          error("internal error, unexpected scope_result in upval capture");
        case SCOPE_RESULT_LOCAL:{
          LoadFunc load_func = load_by_type(uv->type);
          SqRef val = load_func(sqbasetype_from_type(uv->type), uv->ref);
          StoreFunc func = store_by_type(uv->type);
          func(val, sq_i_add(sq_type_long, upval_data, sq_const_int(uv->offset)));
          break;
        }
        case SCOPE_RESULT_PARAMETER: {
          StoreFunc func = store_by_type(uv->type);
          func(uv->ref, sq_i_add(sq_type_long, upval_data, sq_const_int(uv->offset)));
          break;
        }
        case SCOPE_RESULT_UPVALUE: {
          // This case is that the upval we're trying to capture is itself an
          // upval in the current function.
          for (int j = 0; j < parent_uvm->num_upvals; ++j) {
            Upval* parent_uv = &parent_uvm->upvals[j];
            if (str_eq(parent_uv->name, uv->name)) {
              ASSERT(type_eq(parent_uv->type, uv->type));
              /*
              base_writef_stderr("want to write %s from %s for %s in %s\n",
                                 cstr_copy(parser.arena, parent_uv->name),
                                 cstr_copy(parser.arena, parser.cur_scope->func_sym->name),
                                 cstr_copy(parser.arena, uv->name),
                                 cstr_copy(parser.arena, child_func->name));
                                 */
              ASSERT(parser.cur_scope->upval_base.u);
              LoadFunc load_func = load_by_type(uv->type);
              SqRef val = load_func(sqbasetype_from_type(uv->type),
                                    sq_i_add(sq_type_long, parser.cur_scope->upval_base,
                                             sq_const_int(parent_uv->offset)));
              StoreFunc store_func = store_by_type(uv->type);
              store_func(val, sq_i_add(sq_type_long, upval_data, sq_const_int(uv->offset)));
              break;
            }
          }
          break;
        }
      }
    }
  }

  leave_scope();
}

static void advance(void) {
again:
  parser.cursor.prev_kind = parser.cursor.cur_kind;
  if (parser.num_buffered_tokens > 0) {
    parser.cursor.cur_kind = parser.token_buffer[--parser.num_buffered_tokens];
#if BUILD_DEBUG
    if (parser.verbose > 1) {
      base_writef_stderr("token %s (buffered)\n", token_enum_name(parser.cursor.cur_kind));
    }
#endif
    return;
  } else {
    ++parser.cursor.token_index;
    ASSERT(parser.cursor.token_index < parser.num_tokens);
    parser.cursor.cur_kind = token_categorize(parser.token_offsets[parser.cursor.token_index]);
  }

  if (parser.cursor.cur_kind == TOK_NL) {
    goto again;
  }
  if (parser.cursor.cur_kind == TOK_NEWLINE_BLANK) {
    parser.cursor.cur_kind = TOK_NEWLINE;
  } else if (parser.cursor.cur_kind >= TOK_NEWLINE_INDENT_0 && parser.cursor.cur_kind <= TOK_NEWLINE_INDENT_40) {
    int n = (parser.cursor.cur_kind - TOK_NEWLINE_INDENT_0) * 4;
    if (n > parser.indent_levels[parser.num_indents - 1]) {
      parser.cursor.cur_kind = TOK_NEWLINE;
      parser.indent_levels[parser.num_indents++] = n;
      parser.token_buffer[parser.num_buffered_tokens++] = TOK_INDENT;
    } else if (n < parser.indent_levels[parser.num_indents - 1]) {
      parser.cursor.cur_kind = TOK_NEWLINE;
      while (parser.num_indents > 1 && parser.indent_levels[parser.num_indents - 1] > n) {
        parser.token_buffer[parser.num_buffered_tokens++] = TOK_DEDENT;
        --parser.num_indents;
      }
    } else {
      parser.cursor.cur_kind = TOK_NEWLINE;
    }
  }

#if BUILD_DEBUG
  if (parser.verbose > 1) {
      base_writef_stderr("token %s\n", token_enum_name(parser.cursor.cur_kind));
  }
#endif
}

static bool match(TokenKind tok_kind) {
  if (parser.cursor.cur_kind != tok_kind) {
    return false;
  }
  advance();
  return true;
}

static bool check(TokenKind tok_kind) {
  return parser.cursor.cur_kind == tok_kind;
}

static bool peek(TokenKind tok_kind) {
  TokenKind old_cur = parser.cursor.cur_kind;
  TokenKind old_prev = parser.cursor.prev_kind;
  advance();

  bool result = parser.cursor.cur_kind == tok_kind;

  // semi-retreat, but keep categorization by buffering it.
  parser.token_buffer[parser.num_buffered_tokens++] = parser.cursor.cur_kind;
  parser.cursor.cur_kind = old_cur;
  parser.cursor.prev_kind = old_prev;

  return result;
}

static void consume(TokenKind tok_kind, const char* message) {
  if (parser.cursor.cur_kind == tok_kind) {
    advance();
    return;
  }
  error_offset(cur_offset(), message);
}

// We need some constant propagation. Needed for fixed array sizes, making
// `const` actually consts, possibly for nicer overflow checking, and something
// is needed for struct initializers. For struct inits, I was planning on just
// jitting a thunk that fills out a MyStruct_defaults, then zero-init for those
// types turns into memcpy that, rather than memset 0. Fixed-size arrays really
// do need to know the size now though because it goes into the type system, and
// allocas, etc. So, we could:
// 1. Just do some constant propagation similar to what IR will do as we fold by
//    stashing a u64 into Operand, and propagating is_const.
// 2. Or, JIT a little function here (which could actually be calling anything
//    that's already compiled (!)) and return the result, sort of blurring the
//    line between compile and runtime.
//
// #2 seems cool, but kind of heavy for simple things. e.g.
//
//     [5]int x = [1,2,3,4,5]
//
// having to jit and call a function that returns 5 seems a bit heavy. It's
// probably reasonable for more complex expressions though because then it's
// sort of just the difference between running a tree-interpret on the is_const
// Operands vs. JITing that evaluation.
//
// It also moves sort of closer to Python since then anything that's already
// been defined at this point could be in scope for evaluation, which makes
// semantics a bit fuzzy on the edges. Since #1 is pretty clear and a strict
// subset of #2's ability, we'll do that for now, and expand flexibility later
// if necessary.
static Operand const_expression(void) {
  Operand expr = parse_expression(&type_u64);
  if (!op_is_const(expr)) {
    error("Expected constant expression.");
  }
  return expr;
}

static Type basic_tok_to_type[NUM_TOKEN_KINDS] = {
    [TOK_BOOL] = type_bool,      //
    [TOK_BYTE] = type_u8,        //
    [TOK_CODEPOINT] = type_i32,  //
    [TOK_DOUBLE] = type_double,  //
    [TOK_F32] = type_float,      //
    [TOK_F64] = type_double,     //
    [TOK_FLOAT] = type_float,    //
    [TOK_I16] = type_i16,        //
    [TOK_I32] = type_i32,        //
    [TOK_I64] = type_i64,        //
    [TOK_I8] = type_i8,          //
    [TOK_INT] = type_i32,        //
    [TOK_STR] = type_str,        //
    [TOK_U16] = type_u16,        //
    [TOK_U32] = type_u32,        //
    [TOK_U64] = type_u64,        //
    [TOK_U8] = type_u8,          //
    [TOK_UINT] = type_u32,       //
};

static int type_ranks[NUM_TYPE_KINDS] = {
    [TYPE_BOOL] = 1,  //
    [TYPE_U8] = 2,    //
    [TYPE_I8] = 2,    //
    [TYPE_U16] = 3,   //
    [TYPE_I16] = 3,   //
    [TYPE_U32] = 4,   //
    [TYPE_I32] = 4,   //
    [TYPE_U64] = 5,   //
    [TYPE_I64] = 5,   //
};

static int type_rank(Type type) {
  int rank = type_ranks[type_kind(type)];
  ASSERT(rank != 0);
  return rank;
}

static Str str_from_previous(void) {
  StrView view = get_strview_for_offsets(prev_offset(), cur_offset());
  ASSERT(view.size > 0);
  while (view.data[view.size - 1] == ' ') {
    --view.size;
  }
  return str_intern_len(view.data, view.size);
}

static Str parse_name(const char* err) {
  consume(TOK_IDENT_VAR, err);
  return str_from_previous();
}

static Str parse_type_name(const char* err) {
  consume(TOK_IDENT_TYPE, err);
  return str_from_previous();
}

static bool is_convertible(Operand* operand, Type dest) {
  Type src = operand->type;
  if (type_eq(dest, src)) {
    return true;
  } else if (type_kind(dest) == TYPE_VOID) {
    return true;
  } else if (type_is_arithmetic(dest) && type_is_arithmetic(src)) {
    // TODO: This would make sense, but have to have small things work
    // automatically somehow, e.g.
    //   u64 u = 123
    // wouldn't compile because 123 is i32 without an explicit suffix, which is
    // annoying -- should be able to have the RHS know what it's expecting to be
    // and return the right type if it fits?
    //&& type_rank(dest) >= type_rank(src) && type_signs_match(dest, src)) {
    return true;
  }
  // TODO: various pointer, null, etc.
  else {
    return false;
  }
}

static bool is_castable(Operand* operand, Type dest) {
  Type src = operand->type;
  if (is_convertible(operand, dest)) {
    return true;
  } else if (type_kind(dest) == TYPE_BOOL) {
    return type_is_ptr_like(src) || type_is_integer(src);
  } else if (type_is_integer(dest)) {
    return type_is_ptr_like(src);
  } else if (type_is_integer(src)) {
    return type_is_ptr_like(dest);
  } else if (type_is_ptr_like(dest) && type_is_ptr_like(src)) {
    return true;
  } else {
    return false;
  }
}

#if COMPILER_CLANG
#pragma clang diagnostic push
// This is a very good warning, but in the n^2 CASE expansion below, there's
// lots of code that isn't interesting, so disable for this block.
#pragma clang diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif

#define CASE(from_type_kind, from_field)                      \
  case from_type_kind:                                        \
    switch (to_type_kind) {                                   \
      case TYPE_BOOL:                                         \
        operand->val.b = (bool)operand->val.from_field;       \
        break;                                                \
      case TYPE_U8:                                           \
        if (operand->val.from_field > UINT8_MAX) {            \
          error("Overflow converting constant to u8.");       \
        }                                                     \
        operand->val.u8 = (uint8_t)operand->val.from_field;   \
        break;                                                \
      case TYPE_I8:                                           \
        if (operand->val.from_field > INT8_MAX) {             \
          error("Overflow converting constant to i8.");       \
        }                                                     \
        operand->val.i8 = (int8_t)operand->val.from_field;    \
        break;                                                \
      case TYPE_U16:                                          \
        if (operand->val.from_field > UINT16_MAX) {           \
          error("Overflow converting constant to u16.");      \
        }                                                     \
        operand->val.u16 = (uint16_t)operand->val.from_field; \
        break;                                                \
      case TYPE_I16:                                          \
        if (operand->val.from_field > INT16_MAX) {            \
          error("Overflow converting constant to i16.");      \
        }                                                     \
        operand->val.i16 = (int16_t)operand->val.from_field;  \
        break;                                                \
      case TYPE_U32:                                          \
        if ((int64_t)operand->val.from_field > UINT32_MAX) {  \
          error("Overflow converting constant to u32.");      \
        }                                                     \
        operand->val.u32 = (uint32_t)operand->val.from_field; \
        break;                                                \
      case TYPE_I32:                                          \
        if (operand->val.from_field > INT32_MAX) {            \
          error("Overflow converting constant to i32.");      \
        }                                                     \
        operand->val.i32 = (int32_t)operand->val.from_field;  \
        break;                                                \
      case TYPE_U64:                                          \
        operand->val.u64 = (uint64_t)operand->val.from_field; \
        break;                                                \
      case TYPE_I64:                                          \
        if (operand->val.from_field > INT64_MAX) {            \
          error("Overflow converting constant to i64.");      \
        }                                                     \
        operand->val.i64 = (int64_t)operand->val.from_field;  \
        break;                                                \
      case TYPE_PTR:                                          \
        operand->val.p = (uintptr_t)operand->val.from_field;  \
        break;                                                \
      default:                                                \
        error("internal error in const cast");                \
    }                                                         \
    break;

static bool cast_operand(Operand* operand, Type type) {
  if (type_kind(operand->type) != type_kind(type)) {
    if (!is_castable(operand, type)) {
      return false;
    }
    if (op_is_const(*operand)) {
      // TODO: enums
      TypeKind from_type_kind = type_kind(operand->type);
      TypeKind to_type_kind = type_kind(type);
      if (from_type_kind == TYPE_FLOAT && to_type_kind == TYPE_DOUBLE) {
        operand->val.d = (double)operand->val.f;
      } else if (from_type_kind == TYPE_DOUBLE && to_type_kind == TYPE_FLOAT) {
        operand->val.f = (double)operand->val.d;
      } else {
        switch (from_type_kind) {
          CASE(TYPE_BOOL, b)
          CASE(TYPE_U8, u8)
          CASE(TYPE_I8, i8)
          CASE(TYPE_U16, u16)
          CASE(TYPE_I16, i16)
          CASE(TYPE_U32, u32)
          CASE(TYPE_I32, i32)
          CASE(TYPE_U64, u64)
          CASE(TYPE_I64, i64)
          CASE(TYPE_PTR, p)
          default:
            error("internal error in const cast");
        }
      }
    } else {
      SqRef ref_to_adjust;
      if (op_is_local_addr(*operand)) {
        ref_to_adjust = operand_to_sqref_imm(operand);
        operand->kind = OPK_REF_RVAL;
      } else {
        ref_to_adjust = operand->ref;
      }

      if (type_size(operand->type) > type_size(type)) {
        //operand->ref = ir_TRUNC(type_to_ir_type(type), ref_to_adjust);
        operand->ref = ref_to_adjust;
      } else if (type_size(operand->type) < type_size(type)) {
        // Source is strictly smaller than the target.
        if (type_is_signed(type) && type_is_signed(operand->type)) {
          // Both signed, sext.
          ExtFunc func = sext_by_type(operand->type);
          operand->ref = func(sqbasetype_from_type(type), ref_to_adjust);
        } else if (!type_is_signed(type) && !type_is_signed(operand->type)) {
          // Both unsigned, zext.
          ExtFunc func = zext_by_type(operand->type);
          operand->ref = func(sqbasetype_from_type(type), ref_to_adjust);
        } else if (type_is_signed(type) && !type_is_signed(operand->type)) {
          // unsigned extending into signed, zext.
          ExtFunc func = zext_by_type(operand->type);
          operand->ref = func(sqbasetype_from_type(type), ref_to_adjust);
        } else {
          ASSERT(!type_is_signed(type) && type_is_signed(operand->type));
          // signed extending into unsigned, error (?)
          error("can't extend signed into larger unsigned");
        }
        if (type_is_signed(type)) {
        } else {
          ExtFunc func = zext_by_type(type);
          operand->ref = func(sqbasetype_from_type(type), ref_to_adjust);
        }
      } else {
        // This is int-to-int, probably not necessary? Not sure.
        operand->ref = ref_to_adjust; // ir_BITCAST(type_to_ir_type(type), ref_to_adjust);
      }
    }
  }

  operand->type = type;
  return true;
}

#undef CASE

#if COMPILER_CLANG
#pragma clang diagnostic pop
#endif

static bool convert_operand(Operand* operand, Type type) {
  if (is_convertible(operand, type)) {
    cast_operand(operand, type);
    return true;
  }
  return false;
}

static Type parse_type(void) {
  if (match(TOK_STAR)) {
    return type_ptr(parse_type());
  }
  if (match(TOK_LSQUARE)) {
    size_t count = 0;
    if (!check(TOK_RSQUARE)) {
      Operand count_op = const_expression();
      cast_operand(&count_op, type_i64);
      count = count_op.val.i64;
      if (count < 0) {
        error("Negative array size.");
      }
    }
    consume(TOK_RSQUARE, "Expect ']' to close array type.");
    Type elem = parse_type();
    if (type_is_none(elem)) {
      error("Expecting type of array or list.");
    }
    if (count == 0) {
      return type_list(elem);
    } else {
      return type_array(elem, count);
    }
  }

  if (match(TOK_LBRACE)) {
    ASSERT(false); abort();
  }

  if (match(TOK_DEF)) {
    ASSERT(false); abort();
  }

  if (parser.cursor.cur_kind >= TOK_BOOL && parser.cursor.cur_kind <= TOK_UINT) {
    Type t = basic_tok_to_type[parser.cursor.cur_kind];
    ASSERT(!type_is_none(t));
    advance();
    return t;
  }

  if (match(TOK_IDENT_TYPE)) {
    Sym* sym;
    Str type_name = str_from_previous();
    ScopeResult scope_result = scope_lookup_recursive(type_name, &sym);
    if (scope_result == SCOPE_RESULT_UNDEFINED) {
      errorf("Undefined type %s.", cstr_copy(parser.arena, type_name));
    } else if (scope_result == SCOPE_RESULT_GLOBAL && sym->kind == SYM_TYPE) {
      return sym->type;
    } else {
      ASSERT(false && "todo");
    }
  }

  return type_none;
}

static uint32_t parse_func_params(bool is_nested,
                                  Type* memfn_self,
                                  Str self_name,
                                  Type out_types[MAX_FUNC_PARAMS],
                                  Str out_names[MAX_FUNC_PARAMS]) {
  uint32_t num_params = 0;
  bool require_more = false;
  if (is_nested) {
    ASSERT(!memfn_self);
    out_types[num_params] = type_ptr(type_void);
    out_names[num_params] = parser.static_str_up;
    ++num_params;
  } else if (memfn_self) {
    ASSERT(!is_nested);
    out_types[num_params] = *memfn_self;
    out_names[num_params] = self_name;
    ++num_params;
  }
  while (require_more || !check(TOK_RPAREN)) {
    require_more = false;
    Type param_type = parse_type();
    if (type_is_none(param_type)) {
      error("Expect function parameter.");
    }
    Str param_name = parse_name("Expect parameter name.");
    ASSERT(param_name.i);
    ASSERT(num_params < MAX_FUNC_PARAMS);
    out_types[num_params] = param_type;
    out_names[num_params] = param_name;
    num_params += 1;

    if (check(TOK_RPAREN)) {
      break;
    }
    consume(TOK_COMMA, "Expect ',' between function parameters.");
    require_more = true;
  }
  consume(TOK_RPAREN, "Expect ')' after function parameters.");
  return num_params;
}

static void skip_newlines(void) {
  while (match(TOK_NEWLINE)) {
  }
}

#if 0
static bool is_floating_type(Type type) {
  return type.i >= TYPE_FLOAT && type.i <= TYPE_DOUBLE;
}
#endif

// ~strtoull with slight differences:
// - handles 0b and 0o but 0 prefix doesn't mean octal
// - allows arbitrary _ as separators
// - uses StrView rather than nul termination
// - extracts and returns u8, i16, etc. suffixes
static uint64_t scan_int(StrView num, Type* suffix) {
  static uint8_t char_to_digit[256] = {
      ['0'] = 0,               //
      ['1'] = 1,               //
      ['2'] = 2,               //
      ['3'] = 3,               //
      ['4'] = 4,               //
      ['5'] = 5,               //
      ['6'] = 6,               //
      ['7'] = 7,               //
      ['8'] = 8,               //
      ['9'] = 9,               //
      ['a'] = 10, ['A'] = 10,  //
      ['b'] = 11, ['B'] = 11,  //
      ['c'] = 12, ['C'] = 12,  //
      ['d'] = 13, ['D'] = 13,  //
      ['e'] = 14, ['E'] = 14,  //
      ['f'] = 15, ['F'] = 15,  //
  };

  int base = 10;
  StrView digits = num;
  if (num.size > 2) {
    if (num.data[0] == '0' && num.data[1] == 'x') {
      base = 16;
      digits = (StrView){num.data + 2, num.size - 2};
    } else if (num.data[0] == '0' && num.data[1] == 'o') {
      base = 8;
      digits = (StrView){num.data + 2, num.size - 2};
    } else if (num.data[0] == '0' && num.data[1] == 'b') {
      base = 2;
      digits = (StrView){num.data + 2, num.size - 2};
    }
  }

  uint64_t val = 0;
  while (digits.size > 0) {
    if (digits.data[0] == '_') {
      digits = (StrView){digits.data + 1, digits.size - 1};
      continue;
    }
    int digit = char_to_digit[(uint8_t)digits.data[0]];
    if (digit == 0 && digits.data[0] != '0') {
      break;
    }
    if (digit >= base) {
      ASSERT(false && "internal error: lexer shouldn't allow digit out of range");
      return 0;
    }
    if (val > (UINT64_MAX - digit) / base) {
      error("Integer literal overflow.");
    }

    val = val * base + digit;
    digits = (StrView){digits.data + 1, digits.size - 1};
  }

  if (digits.size > 0) {
    if (digits.size == 2 && digits.data[0] == 'i' && digits.data[1] == '8') {
      *suffix = type_i8;
    } else if (digits.size == 2 && digits.data[0] == 'u' && digits.data[1] == '8') {
      *suffix = type_u8;
    } else if (digits.size == 3 && digits.data[0] == 'i' && digits.data[1] == '1' &&
               digits.data[2] == '6') {
      *suffix = type_i16;
    } else if (digits.size == 3 && digits.data[0] == 'u' && digits.data[1] == '1' &&
               digits.data[2] == '6') {
      *suffix = type_u16;
    } else if (digits.size == 3 && digits.data[0] == 'i' && digits.data[1] == '3' &&
               digits.data[2] == '2') {
      *suffix = type_i32;
    } else if (digits.size == 3 && digits.data[0] == 'u' && digits.data[1] == '3' &&
               digits.data[2] == '2') {
      *suffix = type_u32;
    } else if (digits.size == 3 && digits.data[0] == 'i' && digits.data[1] == '6' &&
               digits.data[2] == '4') {
      *suffix = type_i64;
    } else if (digits.size == 3 && digits.data[0] == 'u' && digits.data[1] == '6' &&
               digits.data[2] == '4') {
      *suffix = type_u64;
    } else {
      ASSERT(false && "internal error: lexer shouldn't allow unrecognized suffix");
      return 0;
    }
  }

  return val;
}

typedef enum Precedence {
  PREC_NONE,
  PREC_LOWEST,
  PREC_ASSIGNMENT,  // = += -= *= /=
  PREC_OR,          // or
  PREC_AND,         // and
  PREC_EQUALITY,    // == !=
  PREC_COMPARISON,  // < > <= >=
  PREC_RANGE,       // ..
  PREC_TERM,        // + -
  PREC_BITS,        // ^ | infix &
  PREC_SHIFT,       // << >>
  PREC_FACTOR,      // * / %
  PREC_UNARY,       // prefix & not -
  PREC_CALL,        // . () []
} Precedence;

static bool match_assignment(void) {
  const TokenKind tok = parser.cursor.cur_kind;
  if (tok != TOK_EQ) {
    return false;
  }
#if 0
  if (!(tok == TOK_EQ || tok == TOK_PLUSEQ || tok == TOK_MINUSEQ || tok == TOK_STAREQ ||
        tok == TOK_SLASHEQ || tok == TOK_PERCENTEQ || tok == TOK_CARETEQ || tok == TOK_PIPEEQ ||
        tok == TOK_AMPERSANDEQ || tok == TOK_LSHIFTEQ || tok == TOK_RSHIFTEQ)) {
    return false;
  }
#endif
  advance();
  return true;
}

typedef Operand (*PrefixFn)(bool can_assign, Type* expected);
typedef Operand (*InfixFn)(Operand left, bool can_assign, Type* expected);

typedef struct Rule {
  PrefixFn prefix;
  InfixFn infix;
  Precedence prec_for_infix;
} Rule;

static Rule* get_rule(TokenKind tok_kind);
static Operand parse_precedence(Precedence precedence, Type* expected);

static Operand parse_alignof(bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
  return operand_null;
}

static Operand parse_and(Operand left, bool can_assign, Type* expected) {
  // TODO: could have const eval here
  if (!type_is_condition(left.type)) {
    errorf("Left-hand side of or cannot be type %s.", type_as_str(left.type));
  }

  SqBlock block_rval = sq_block_declare();
  SqBlock block_true = sq_block_declare();
  SqBlock block_done = sq_block_declare();

  SqRef result = sq_i_alloc8(sq_const_int(type_size(type_bool)));
  sq_i_storeb(sq_const_int(0), result);

  sq_i_jnz(operand_to_sqref_imm(&left), block_rval, block_done);

  sq_block_start(block_rval);
  Operand right = parse_precedence(PREC_OR, &type_bool);
  if (!type_is_condition(right.type)) {
    errorf("Right-hand side of or cannot be type %s.", type_as_str(right.type));
  }
  sq_i_jnz(operand_to_sqref_imm(&right), block_true, block_done);

  sq_block_start(block_true);
  sq_i_storeb(sq_const_int(1), result);

  sq_block_start(block_done);

  return operand_rvalue_imm(type_bool, sq_i_loadub(sq_type_word, result));
}

static void promote_small_integers(Operand* operand) {
  switch (type_kind(operand->type)) {
    case TYPE_I8:
    case TYPE_I16:
    case TYPE_ENUM:
      cast_operand(operand, type_i32);
      break;
    case TYPE_U8:
    case TYPE_U16:
      cast_operand(operand, type_u32);
      break;

    default:
      // Do nothing
      break;
  }
}

static void unify_arithmetic_operands(Operand* left, Operand* right) {
  // TODO: floats aren't even parsed yet
  ASSERT(type_is_integer(left->type));
  ASSERT(type_is_integer(right->type));
  promote_small_integers(left);
  promote_small_integers(right);
  if (!type_eq(left->type, right->type)) {
    if (type_is_signed(left->type) == type_is_signed(right->type)) {
      if (type_rank(left->type) <= type_rank(right->type)) {
        cast_operand(left, right->type);
      } else {
        cast_operand(right, left->type);
      }
    } else if (type_is_signed(left->type) && op_is_const(*left) &&
               type_rank(right->type) >= type_rank(left->type)) {
      // i32 == u64 -- only want do do this when the thing we're casting is
      // const so that overflow can be checked.
      cast_operand(left, right->type);
    } else if (type_is_signed(right->type) && op_is_const(*right) &&
               type_rank(left->type) >= type_rank(right->type)) {
      // u64 == i32 -- only want to do this when the thing we're casting is
      // const so that overflow can be checked.
      cast_operand(right, left->type);
    } else if (type_is_signed(left->type) && type_size(left->type) > type_size(right->type)) {
      // If the left is signed but strictly larger then we can also cast the
      // right (i.e. i32 == u16).
      cast_operand(right, left->type);
    } else if (type_is_signed(right->type) && type_size(right->type) > type_size(left->type)) {
      // If the right is signed but strictly larger then we can also cast the
      // left (i.e. u16 == i32).
      cast_operand(left, right->type);
    }
  }
  ASSERT(type_eq(left->type, right->type));
}

static unsigned long long eval_binary_op_ull(TokenKind op,
                                             unsigned long long left,
                                             unsigned long long right) {
  error("TODO: ull binary const eval");
}

static unsigned long highest_bit_set(long long val) {
#if COMPILER_MSVC
  unsigned long index;
  bool is_nonzero = _BitScanForward64(&index, val);
  if (is_nonzero) {
    return index;
  }
  return 0;
#else
  if (val == 0) {
    return 0;
  }
  return 63 - __builtin_clzll(val);
#endif
}

static long long eval_binary_op_ll(TokenKind op, long long left, long long right) {
  switch (op) {
    case TOK_STAR: {
      long long result;
      if (
#if COMPILER_MSVC
          _mul_overflow_i64(left, right, &result)
#else
          __builtin_smulll_overflow(left, right, &result)
#endif
      ) {
        errorf("%llu multiplied by %llu overflows.", left, right);
      }
      return result;
    }
    case TOK_SLASH:
      if (right == 0) {
        error("Divide by zero.");
        return 0;
      }
      return left / right;
    case TOK_PERCENT:
      if (right == 0) {
        error("Divide by zero.");
        return 0;
      }
      return left % right;
    case TOK_AMPERSAND:
      return left & right;
    case TOK_LSHIFT: {
      long long required_bits = highest_bit_set(left) + right + 1;
      if (required_bits > 64) {
        errorf("%llu shifted left by %llu requires %llu bits.", left, right, required_bits);
      }
      return left << right;
    }
#if 0  // TODO: signed bit passing
    case IR_SHR:
      error("internal error: SHR on signed.");
    case IR_SAR:
      return left >> right;
#endif
    case TOK_PLUS: {
      long long result;
      if (
#if COMPILER_MSVC
          _add_overflow_i64(0, left, right, &result)
#else
          __builtin_saddll_overflow(left, right, &result)
#endif
      ) {
        errorf("%llu added to %llu overflows.", right, left);
      }
      return result;
    }
    case TOK_MINUS: {
      long long result;
      if (
#if COMPILER_MSVC
          _sub_overflow_i64(0, left, right, &result)
#else
          __builtin_ssubll_overflow(left, right, &result)
#endif
      ) {
        errorf("%lld subtracted from %lld overflows.", right, left);
      }
      return result;
    }
    case TOK_PIPE:
      return left | right;
    case TOK_CARET:
      return left ^ right;
    case TOK_EQEQ:
      return left == right;
    case TOK_BANGEQ:
      return left != right;
    case TOK_LT:
      return left < right;
    case TOK_LEQ:
      return left <= right;
    case TOK_GT:
      return left > right;
    case TOK_GEQ:
      return left >= right;
    default:
      error("internal error: unexpected const op.");
  }
}

static Val eval_binary_op(TokenKind op, Type type, Val left, Val right) {
  if (type_is_integer(type)) {
    Operand left_operand = operand_const(type, left);
    Operand right_operand = operand_const(type, right);
    Operand result_operand;
    if (type_is_signed(type)) {
      cast_operand(&left_operand, type_i64);
      cast_operand(&right_operand, type_i64);
      result_operand = operand_const(
          type_i64,
          (Val){.i64 = eval_binary_op_ll(op, left_operand.val.i64, right_operand.val.i64)});
    } else {
      cast_operand(&left_operand, type_u64);
      cast_operand(&right_operand, type_u64);
      result_operand = operand_const(
          type_u64,
          (Val){.u64 = eval_binary_op_ull(op, left_operand.val.u64, right_operand.val.u64)});
    }
    cast_operand(&result_operand, type);
    return result_operand.val;
  } else {
    return (Val){0};
  }
}

typedef SqRef (*BinOpFunc)(SqType, SqRef, SqRef);

static Operand resolve_binary_op(TokenKind op,
                                 BinOpFunc func,
                                 Operand left,
                                 Operand right,
                                 uint32_t loc) {
  ASSERT(type_eq(left.type, right.type));
  // It didn't really seem worth doing constant eval, but it's needed for array
  // sizes in particular, so we do some constant propagation through is_const
  // Operands.
  if (op_is_const(left) && op_is_const(right)) {
    return operand_const(left.type, eval_binary_op(op, left.type, left.val, right.val));
  } else {
    SqRef result = func(sqbasetype_from_type(left.type), operand_to_sqref_imm(&left),
                        operand_to_sqref_imm(&right));
    return operand_rvalue_imm(left.type, result);
  }
}

static Operand resolve_cmp_op(TokenKind op,
                              BinOpFunc func,
                              Operand left,
                              Operand right,
                              uint32_t loc) {
  ASSERT(type_eq(left.type, right.type));
  if (op_is_const(left) && op_is_const(right)) {
    return operand_const(type_bool, eval_binary_op(op, left.type, left.val, right.val));
  } else  {
    SqRef result = func(sqbasetype_from_type(left.type), operand_to_sqref_imm(&left),
                        operand_to_sqref_imm(&right));
    return operand_rvalue_imm(type_bool, result);
  }
}

static Operand resolve_binary_arithmetic_op(TokenKind op,
                                            BinOpFunc func,
                                            Operand left,
                                            Operand right,
                                            uint32_t loc) {
  unify_arithmetic_operands(&left, &right);
  return resolve_binary_op(op, func, left, right, loc);
}

static Operand resolve_binary_cmp_op(TokenKind op,
                                     BinOpFunc func,
                                     Operand left,
                                     Operand right,
                                     uint32_t loc) {
  unify_arithmetic_operands(&left, &right);
  return resolve_cmp_op(op, func, left, right, loc);
}

static Operand parse_binary(Operand left, bool can_assign, Type* expected) {
  // Remember the operator.
  TokenKind op = parser.cursor.prev_kind;
  uint32_t op_offset = prev_offset();

  // Compile the right operand.
  Rule* rule = get_rule(op);
  Operand rhs = parse_precedence(rule->prec_for_infix + 1, expected);

  typedef struct OpPair {
    BinOpFunc sign;
    BinOpFunc unsign;
  } OpPair;
  static OpPair tok_to_cmp_op[NUM_TOKEN_KINDS] = {
      [TOK_EQEQ] = {sq_i_ceqw, sq_i_ceqw},
      [TOK_BANGEQ] = {sq_i_cnew, sq_i_cnew},
      [TOK_LEQ] = {sq_i_cslew, sq_i_culew},
      [TOK_LT] = {sq_i_csltw, sq_i_cultw},
      [TOK_GEQ] = {sq_i_csgew, sq_i_cugew},
      [TOK_GT] = {sq_i_csgtw, sq_i_cugtw},
  };
  typedef struct IrOpAndErr {
    BinOpFunc func;
    const char* err_msg;
  } IrOpAndErr;
  static IrOpAndErr tok_to_bin_op[NUM_TOKEN_KINDS] = {
      [TOK_PLUS] = {sq_i_add, "Cannot add %s to %s"},
      [TOK_MINUS] = {sq_i_sub, "TODO %s %s"},
      [TOK_STAR] = {sq_i_mul, "TODO %s %s"},
      [TOK_SLASH] = {sq_i_div, "TODO %s %s"},
      [TOK_PERCENT] = {sq_i_rem, "TODO %s %s"},
      // XXX wouldn't this be in unary?
      //[TOK_TILDE] = {IR_NOT,  // TODO
      [TOK_PIPE] = {sq_i_or, "TODO %s %s"},
      [TOK_AMPERSAND] = {sq_i_and, "TODO %s %s"},
      [TOK_CARET] = {sq_i_xor, "TODO %s %s"},
      [TOK_LSHIFT] = {sq_i_shl, "TODO %s %s"},
      // TOK_RSHIFT handled below to do SHR vs SAR
  };
  if (tok_to_cmp_op[op].sign /*anything nonzero in slot*/) {
    if (type_is_arithmetic(left.type) && type_is_arithmetic(rhs.type)) {
      if (!type_signs_match(left.type, rhs.type)) {
        errorf_offset(op_offset, "Comparison with different signs: %s and %s.",
                      type_as_str(left.type), type_as_str(rhs.type));
      } else {
        bool is_signed = type_is_signed(left.type);
        return resolve_binary_cmp_op(op,
                                     is_signed ? tok_to_cmp_op[op].sign : tok_to_cmp_op[op].unsign,
                                     left, rhs, op_offset);
      }
    } else {
      errorf_offset(op_offset, "Cannot compare %s and %s.", type_as_str(left.type),
                    type_as_str(rhs.type));
    }
  } else if (tok_to_bin_op[op].err_msg /* anything nonzero in slot*/) {
    if (type_is_arithmetic(left.type) && type_is_arithmetic(rhs.type)) {
      return resolve_binary_arithmetic_op(op, tok_to_bin_op[op].func, left, rhs, op_offset);
    } else {
      // TODO: special case str here

      errorf_offset(op_offset, tok_to_bin_op[op].err_msg, type_as_str(left.type),
                    type_as_str(rhs.type));
    }
  } else if (op == TOK_RSHIFT) {
    BinOpFunc func = type_is_unsigned(left.type) ? sq_i_shr : sq_i_sar;
    SqRef result = func(sqbasetype_from_type(left.type), operand_to_sqref_imm(&left),
                        operand_to_sqref_imm(&rhs));
    return operand_rvalue_imm(left.type, result);
  } else {
    ASSERT(false && "todo");
    return operand_null;
  }
}

static Operand parse_bool_literal(bool can_assign, Type* expected) {
  ASSERT(parser.cursor.prev_kind == TOK_FALSE || parser.cursor.prev_kind == TOK_TRUE);
  return operand_const(type_bool, (Val){.b = parser.cursor.prev_kind == TOK_FALSE ? 0 : 1});
}

#if 0
#if OS_WINDOWS && ARCH_X64
static bool is_aggregate_in_int_register_x64win(Type type) {
  ASSERT(type_is_aggregate(type));
  switch (type_size(type)) {
    case 1:
    case 2:
    case 4:
    case 8:
      // Note that e.g. `struct {char x[3];}` is passed by pointer, even though
      // it would otherwise "fit".
      return true;
    default:
      return false;
  }
}
#endif

// For 'normal' arguments, this is roughly just using ir_CALL_N. But, because
// structs aren't supported at the IR level, they need to be handled here
// specially. This is different per ABI.
static Operand lower_structs_and_call(Operand* func, uint32_t num_args, ir_ref* arg_values) {
  // TODO: Other cases, mostly Linux+SysV.

  // The complex case below would work without this, but bail to a simple CALL_N
  // if we know the function type doesn't have any aggregates being passed.
#if (OS_WINDOWS && ARCH_X64) || (OS_MAC && ARCH_ARM64)
  if ((type_func_flags(func->type) & TFF_HAS_AGGREGATE_ARGS) == 0)
#endif
  {
    Type ret_type = type_func_return_type(func->type);
    return operand_rvalue_imm(
        ret_type, ir_CALL_N(type_to_ir_type(ret_type), func->ref, num_args, arg_values));
  }

#if OS_WINDOWS && ARCH_X64
  // Ref: https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention in
  // particular, "Parameter passing" and "Return values". IR handles integer and
  // floating point values in general, we just need to handle the cases of small
  // (8/16/32/64) aggregates being passed in integer registers, and large
  // aggregates being passed by pointer.
  ir_ref new_arg_values[MAX_FUNC_PARAMS];
  Type new_ret_type;
  uint32_t num_new_args = 0;
  Type ret_type = type_func_return_type(func->type);
  ir_ref out_ret;
  bool ret_type_packed_into_int = false;
  if (type_is_aggregate(ret_type)) {
    ret_type_packed_into_int = is_aggregate_in_int_register_x64win(ret_type);
    if (ret_type_packed_into_int) {
      new_ret_type = type_u64;
    } else {
      // Create a slot for the callee to write to, and pass that as the first
      // arg. That same pointer will be returned by the callee.
      out_ret = ir_ALLOCA(ir_CONST_U64(type_size(ret_type)));
      new_ret_type = type_ptr(ret_type);
      new_arg_values[num_new_args] = out_ret;
      ++num_new_args;
    }
  } else {
    new_ret_type = ret_type;
  }

  ASSERT(type_func_num_params(func->type) == num_args);
  for (uint32_t i = 0; i < num_args; ++i) {
    Type param = type_func_param(func->type, i);
    if (type_is_aggregate(param)) {
      ir_ref size = ir_CONST_U64(type_size(param));
      if (is_aggregate_in_int_register_x64win(param)) {
        ir_ref tmp_int = ir_VAR(IR_U64, "pack");
        ir_ref memcpy_addr = ir_CONST_ADDR(memcpy);
        ir_CALL_3(IR_VOID, memcpy_addr, ir_VADDR(tmp_int), arg_values[i], size);
        new_arg_values[num_new_args] = ir_VLOAD(IR_U64, tmp_int);
        ++num_new_args;
      } else {
        // Copy the argument by value to a new stack location (it can't be the one
        // already on the stack because the callee might modify it), and then
        // pass a pointer to that.
        ir_ref copy = ir_ALLOCA(size);
        ir_ref memcpy_addr = ir_CONST_ADDR(memcpy);
        // TODO: maybe pass Operand so we can check the arg_values is an addr.
        ir_CALL_3(IR_VOID, memcpy_addr, copy, arg_values[i], size);
        new_arg_values[num_new_args] = copy;
        ++num_new_args;
      }
    } else {
      new_arg_values[num_new_args++] = arg_values[i];
    }
  }

  ir_ref rv = ir_CALL_N(type_to_ir_type(new_ret_type), func->ref, num_new_args, new_arg_values);
  ASSERT(!type_is_aggregate(new_ret_type));
  if (ret_type_packed_into_int) {
    ir_ref tmp_int = ir_VAR(IR_U64, "unpack");
    ir_VSTORE(tmp_int, rv);
    ir_ref size = ir_CONST_U64(type_size(ret_type));
    ir_ref unpacked = ir_ALLOCA(size);
    ir_ref memcpy_addr = ir_CONST_ADDR(memcpy);
    ir_CALL_3(IR_VOID, memcpy_addr, unpacked, ir_VADDR(tmp_int), size);
    return operand_rvalue_local_addr(ret_type, unpacked);
  } else {
    return operand_rvalue_imm(ret_type, rv);
  }
#elif OS_MAC && ARCH_ARM64

  // aarch64 generally: https://github.com/ARM-software/abi-aa/blob/a82eef0433556b30539c0d4463768d9feb8cfd0b/aapcs64/aapcs64.rst#682parameter-passing-rules
  // macOS: https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms#Pass-arguments-to-functions-correctly
  // I don't think we need to do anything extra for Apple, as I think it only
  // involves details that would be handled by IR lower down.
  //
  // Roughly for struct calling:
  // - >16, copy to alloca, and replace with pointer.
  // - <= 16, copy in 8 byte chunks to uint64 registers
  //
  // Struct return is more troublesome:
  // - <= 16 into same registers used for passing, think we can only get out 8
  // because there's no U128, so we can get x0, but not x1.
  // - >16 pointer goes in x8, don't think this can be done without IR changes

  Type ret_type = type_func_return_type(func->type);
  if (type_is_aggregate(ret_type)) {
    error("todo; cannot return aggregates on macOS yet");
  }

  ASSERT(type_func_num_params(func->type) == num_args);
  for (uint32_t i = 0; i < num_args; ++i) {
    Type param = type_func_param(func->type, i);
    if (type_is_aggregate(param)) {
      error("todo; cannot pass aggregates on macOS yet");
    }
  }

  error("internal error; lower_structs_and_call");
#endif
}
#endif

static Operand parse_call(Operand left, bool can_assign, Type* expected) {
  if (can_assign && match_assignment()) {
    CHECK(false && "todo; returning address i think");
  }
  if (type_kind(left.type) != TYPE_FUNC) {
    errorf("Expected function type, but type is %s.", type_as_str(left.type));
  }
  SqCallArg arg_values[MAX_FUNC_PARAMS];
  uint32_t num_args = 0;

  if (type_func_flags(left.type) & (TFF_NESTED | TFF_MEMFN)) {
    ASSERT(op_has_ref2(left));
    arg_values[0] = (SqCallArg){sq_type_long, left.ref2};
    ++num_args;
  }

  if (!check(TOK_RPAREN)) {
    for (;;) {
      if (num_args >= type_func_num_params(left.type)) {
        errorf("Passing >= %d argument%s to function, but it expects %d.", num_args + 1,
               num_args + 1 == 1 ? "" : "s", type_func_num_params(left.type));
      }
      uint32_t arg_offset = cur_offset();
      Type param_type = type_func_param(left.type, num_args);
      Operand arg = parse_precedence(PREC_OR, &param_type);
      if (!convert_operand(&arg, param_type)) {
        errorf_offset(arg_offset, "Call argument %d is type %s, but function expects type %s.",
                      num_args + 1, type_as_str(arg.type), type_as_str(param_type));
      }
      arg_values[num_args].type = type_to_sqtype(arg.type);
      arg_values[num_args].value = operand_to_sqref_imm(&arg);
      ++num_args;
      if (!match(TOK_COMMA)) {
        break;
      }
    }
  }
  if (num_args < type_func_num_params(left.type)) {
    errorf("Passing only %d argument%s to function, but it expects %d.", num_args,
           num_args == 1 ? "" : "s", type_func_num_params(left.type));
  }

  consume(TOK_RPAREN, "Expect ')' after arguments.");
  Type ret_type = type_func_return_type(left.type);
  return operand_rvalue_imm(ret_type,
                            sq_i_calla(type_to_sqtype(ret_type), left.ref, num_args, arg_values));
}

static Operand parse_compound_literal(bool can_assign, Type* expected) {
  Type lit_type;
  Str type_name = str_from_previous();

  Sym* sym;
  ScopeResult scope_result = scope_lookup_recursive(type_name, &sym);
  if (scope_result == SCOPE_RESULT_UNDEFINED) {
    errorf("Undefined type %s.", cstr_copy(parser.arena, type_name));
  } else if (scope_result == SCOPE_RESULT_GLOBAL && sym->kind == SYM_TYPE) {
    lit_type = sym->type;
    if (type_kind(lit_type) != TYPE_STRUCT) {
      errorf("Cannot construct compound literal of type %s.", type_as_str(lit_type));
    }
  } else {
    error("TODO: unhandled case in compound literal.");
  }

  consume(TOK_LPAREN, "Expecting '(' to start compound literal.");
  Str field_names[MAX_STRUCT_FIELDS];
  Operand field_values[MAX_STRUCT_FIELDS];
  uint32_t field_offsets[MAX_STRUCT_FIELDS];
  int num_fields = 0;
  if (!check(TOK_RPAREN)) {
    for (;;) {
      if (check(TOK_IDENT_VAR) && peek(TOK_EQ)) {
        Str field_name = parse_name("Expected field name.");
        field_names[num_fields] = field_name;
        consume(TOK_EQ, "Expecting '=' following initializer name.");
      } else {
        field_names[num_fields] = (Str){0};
      }

      field_offsets[num_fields] = cur_offset();
      field_values[num_fields] = parse_precedence(PREC_OR, NULL);
      ++num_fields;
      if (!match(TOK_COMMA)) {
        break;
      }
    }
  }
  consume(TOK_RPAREN, "Expect ')' after compound literal.");

  size_t lit_size = type_size(lit_type);
  SqRef base_addr = sq_i_alloc8(sq_const_int(lit_size));
  initialize_aggregate(base_addr, lit_type);

  uint32_t index = 0;
  for (int i = 0; i < num_fields; ++i, ++index) {
    if (!str_is_none(field_names[i])) {
      index = type_struct_field_index_by_name(lit_type, field_names[i]);
    }
    if (index >= type_struct_num_fields(lit_type)) {
      error_offset(field_offsets[i], "Initializer does not correspond to field.");
    }
    Type field_type = type_struct_field_type(lit_type, index);
    if (!convert_operand(&field_values[i], field_type)) {
      errorf("Cannot convert initializer values of type %s to type %s.",
             type_as_str(field_values[i].type), type_as_str(field_type));
    }
    uint32_t field_offset = type_struct_field_offset(lit_type, index);
    StoreFunc func = store_by_type(field_type);
    func(operand_to_sqref_imm(&field_values[i]),
         sq_i_add(sq_type_long, base_addr, sq_const_int(field_offset)));
  }

  return operand_rvalue_local_addr(lit_type, base_addr);
}

static Operand parse_dict_literal(bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
  return operand_null;
}

static Str memfn_name_from_type_name(Str type_name, Str func_name) {
  return str_internf("%.*s$%.*s", str_len(type_name), str_raw_ptr(type_name), str_len(func_name),
                     str_raw_ptr(func_name));
}

static Str memfn_name_from_type(Type type, Str func_name) {
  return memfn_name_from_type_name(type_decl_name(type), func_name);
}

static Sym* lookup_memfn(Type type, Str func_name) {
  Str memfn_name = memfn_name_from_type(type, func_name);
  Sym* sym;
  ScopeResult scope_result = scope_lookup_recursive(memfn_name, &sym);
  if (scope_result == SCOPE_RESULT_UNDEFINED) {
    return NULL;
  } else if (scope_result == SCOPE_RESULT_GLOBAL && sym->kind == SYM_FUNC) {
    return sym;
  } else {
    error("internal error: lookup_memfn");
  }
}

static Operand parse_dot(Operand left, bool can_assign, Type* expected) {
  // TODO: package, and maybe const or types after . ?
  Str name = parse_name("Expect property name after '.'.");
  uint32_t name_offset = prev_offset();

  if (can_assign && match_assignment()) {
    while (type_kind(left.type) == TYPE_PTR) {
      left = operand_lvalue_local(type_ptr_subtype(left.type), sq_i_load(sq_type_long, left.ref));
    }

    if (type_kind(left.type) == TYPE_STRUCT) {
      uint32_t field_offset;
      Type field_type;
      if (type_struct_find_field_by_name(left.type, name, &field_type, &field_offset)) {
        Operand rhs_value = parse_expression(expected);

        StoreFunc func = store_by_type(field_type);
        func(operand_to_sqref_imm(&rhs_value),
             sq_i_add(sq_type_long, operand_to_sqref_imm(&left), sq_const_int(field_offset)));
        return operand_null;
      } else {
        errorf_offset(name_offset, "'%s' is not a field of type %s.",
                      cstr_copy(parser.arena, name),
                      cstr_copy(parser.arena, type_struct_decl_name(left.type)));
      }
    } else {
      error("todo; assigning to unexpected thing");
    }
  } else {
    Type original_left_type = left.type;
    while (type_kind(left.type) == TYPE_PTR) {
      left = operand_lvalue_local(type_ptr_subtype(left.type), sq_i_load(sq_type_long, left.ref));
    }
    if (type_kind(left.type) == TYPE_STRUCT) {
      uint32_t field_offset;
      Type field_type;
      if (type_struct_find_field_by_name(left.type, name, &field_type, &field_offset)) {
        LoadFunc func = load_by_type(field_type);
        SqRef ref =
            func(sqbasetype_from_type(field_type),
                 sq_i_add(sq_type_long, operand_to_sqref_imm(&left), sq_const_int(field_offset)));
        return operand_rvalue_imm(field_type, ref);
      }

      // Not an error yet; could be a memfn below.
    }

    Sym* func_sym = {0};
    switch (type_kind(left.type)) {
      case TYPE_ARRAY:
      case TYPE_LIST:
        error("TODO: polymorphic array/list memfns");

      case TYPE_DICT:
        error("TODO: polymorphic dict memfns");

      default: {
        Sym* sym = lookup_memfn(left.type, name);
        if (!sym) {
          errorf("Undefined member function %s.", cstr_copy(parser.arena, name));
        } else {
          func_sym = sym;
        }
        break;
      }
    }

    if (type_kind(func_sym->type) != TYPE_FUNC) {
      error("internal error: memfn resolved to non-function");
    }
    if (type_func_num_params(func_sym->type) < 1) {
      // Parser shouldn't get this far.
      error("internal error: memfn with no parameters");
    }

    // left could have been:
    //    ****Stuff x
    //    x.memfn()
    // The auto-deref would find Stuff for memfn lookup, and now left.type will
    // just be Stuff. The target memfn always just gets *Stuff, so we need to
    // build that from the left that we originally had.
    SqRef self_ptr;
    if (type_kind(original_left_type) == TYPE_STRUCT || type_is_basic(original_left_type)) {
      SqRef addr = sq_i_alloc8(sq_const_int(8));
      sq_i_storel(operand_to_sqref_imm(&left), addr);
      self_ptr = addr;
      // TODO: i think struct happens to work since it'll be an alloca
      // (memfn_basic), but in memfn_on_basic_type it doesn't since self will
      // be an int that needs a pointer to it, rather than just the temporary.
      // probably other places that are similar too, hrm.
      ASSERT(false && "addr");
#if 0
      ir_ref addr = ir_VAR(IR_ADDR, "self*");
      ir_VSTORE(addr, operand_to_irref_imm(&left));
      self_ptr = ir_VADDR(addr);
#endif
    } else if (type_kind(original_left_type) == TYPE_PTR &&
               type_kind(type_ptr_subtype(original_left_type)) == TYPE_STRUCT) {
      self_ptr = left.ref;
    } else {
      error("TODO: self ptr");
    }
    return operand_rvalue_global_addr_bound(func_sym->type, sq_ref_for_symbol(func_sym->global),
                                            self_ptr);
  }

  ASSERT(false && "todo");
  return operand_null;
}

static Operand parse_grouping(bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
  return operand_null;
}
static Operand parse_in_or_not_in(Operand left, bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
  return operand_null;
}

static Operand parse_len(bool can_assign, Type* expected) {
  consume(TOK_LPAREN, "Expect '(' after len.");
  Operand len_of = parse_precedence(PREC_OR, NULL);
  consume(TOK_RPAREN, "Expect ')' after len.");
  switch (type_kind(len_of.type)) {
    case TYPE_ARRAY:
      return operand_const(type_u64, (Val){.u64 = type_array_count(len_of.type)});
    case TYPE_LIST:
      return operand_rvalue_imm(
          type_u64, sq_i_load(sq_type_long, sq_i_add(sq_type_long, len_of.ref, sq_const_int(8))));
    case TYPE_DICT:
    case TYPE_STR:
      error("TODO: len impl");
    default:
      errorf("Cannot use len on type %s.", type_as_str(len_of.type));
  }
}

static bool scan_to_determine_if_comprehension(TokenCursor* original, TokenCursor* at_for) {
  ASSERT(parser.num_buffered_tokens == 0);

  *original = parser.cursor;
  original->paren_level = token_get_continuation_paren_level();

  // We start the scan after the starting [.
  int square_bracket_count = 1;
  for (;;) {
    if (parser.cursor.cur_kind == TOK_LSQUARE) {
      ++square_bracket_count;
    } else if (parser.cursor.cur_kind == TOK_RSQUARE) {
      --square_bracket_count;
      if (square_bracket_count == 0) {
        parser.cursor = *original;
        token_restore_continuation_paren_level(original->paren_level);
        return false;
      }
    } else if (parser.cursor.cur_kind == TOK_FOR) {
      *at_for = parser.cursor;
      return true;
    } else if (parser.cursor.cur_kind == TOK_NEWLINE || parser.cursor.cur_kind == TOK_EOF) {
      error("Expecting ']' to end list literal or comprehension.");
    }

    parser.cursor.prev_kind = parser.cursor.cur_kind;
    ++parser.cursor.token_index;
    ASSERT(parser.cursor.token_index < parser.num_tokens);
    parser.cursor.cur_kind = token_categorize(parser.token_offsets[parser.cursor.token_index]);
    ASSERT(parser.cursor.cur_kind != TOK_NEWLINE_BLANK);
    ASSERT(parser.cursor.cur_kind < TOK_NEWLINE_INDENT_0 ||
           parser.cursor.cur_kind > TOK_NEWLINE_INDENT_40);
  }
}

typedef enum IterationKind {
  ITK_UNKNOWN = 0,
  ITK_ARRAY,
} IterationKind;

typedef struct IterationData {
  IterationKind kind;
  Sym* itsym;
#if 0
  ir_ref loop;
  ir_ref cond;
  union {
    ir_ref index;
  } ARRAY;
#endif
} IterationData;

#if 0
static IterationData iteration_prolog(Str it, Operand* over) {
  Type it_type;
  if (type_kind(over->type) == TYPE_ARRAY) {
    it_type = type_array_subtype(over->type);
  } else {
    errorf("Can't iterate over type %s.", type_as_str(over->type));
  }

  IterationData itd = {.kind = ITK_ARRAY };
  itd.itsym = make_local_and_alloc(SYM_VAR, it, it_type, NULL);

  itd.ARRAY.index = ir_VAR(IR_U64, "index");
  ir_VSTORE(itd.ARRAY.index, ir_CONST_U64(0));

  itd.loop = ir_LOOP_BEGIN(ir_END());

  ir_ref cur = ir_VLOAD_U64(itd.ARRAY.index);
  itd.cond = ir_IF(ir_LT(cur, ir_CONST_U64(type_array_count(over->type))));
  ir_IF_TRUE(itd.cond);

  ir_ref arr_load_addr = ir_ADD_A(
      over->ref, ir_MUL(IR_U64, ir_CONST_U64(type_size(it_type)), ir_VLOAD_U64(itd.ARRAY.index)));
  ir_VSTORE(itd.itsym->ref, ir_LOAD(type_to_ir_type(it_type), arr_load_addr));

  return itd;
}

static void iteration_epilog(IterationData itd) {
  if (itd.kind == ITK_ARRAY) {
    ir_VSTORE(itd.ARRAY.index, ir_ADD_U64(ir_VLOAD_U64(itd.ARRAY.index), ir_CONST_U64(1)));
    ir_MERGE_SET_OP(itd.loop, 2, ir_LOOP_END());
    ir_IF_FALSE(itd.cond);
  } else {
    error("Unhandled iteration epilog.");
  }
}
#endif

static Operand parse_list_comprehension(TokenCursor original, TokenCursor at_for, Type* expected) {
  parser.cursor = at_for;
  consume(TOK_FOR, "Expect 'for' to start list comprehension.");
#if 0
  Str it = parse_name("Expect iterator name of list comprehension.");
#endif
  // TODO: other forms for enumerate
  consume(TOK_IN, "Expect 'in'.");
#if 0
  Operand over = parse_expression(NULL);
#endif
  if (check(TOK_FOR)) {
    error("todo; multiple for in list compr");
  }
  if (check(TOK_IF)) {
    error("todo; conditional in list compr");
  }
  TokenCursor after_clauses = parser.cursor;

  // In general, we have to assume a slice output here because even if iterating
  // over an array, it could be filtered, so we can't know the number of
  // outputs. So this only creates an array if |expected| is provided
  // explicitly.
  // TODO: implement this once there's slices!

  // TODO: enter a full function scope here? or some third non-module,
  // non-function type of scope?
  // enter_scope(false, false, NULL);

#if 0
  IterationData itd = iteration_prolog(it, &over);

  parser.cursor = original;

  Operand elem = parse_expression(NULL);
  (void)elem;

  iteration_epilog(itd);
#endif

  // leave_scope();

  parser.cursor = after_clauses;

  return operand_null;
}

static Operand parse_list_literal(Type* expected) {
  OpVec elems;
  opv_init(&elems, parser.arena);

  for (;;) {
    if (check(TOK_RSQUARE)) {
      // Allow trailing comma.
      break;
    }

    Operand elem = parse_expression(NULL);
    opv_append(&elems, elem);

    if (!match(TOK_COMMA)) {
      break;
    }
  }

  if (elems.size == 0) {
    if (!expected) {
      error("Cannot deduce type of empty list with no explicit type on left-hand side.");
    }
    if (type_kind(*expected) == TYPE_LIST || type_kind(*expected) == TYPE_ARRAY) {
      error("todo; empty slice/array");
    } else {
      errorf("Cannot convert empty list literal to expected type %s.", type_as_str(*expected));
    }
  } else {
    // TODO: for ints, it'd be nice to promote all of them to the largest
    // required if that would make it work, so that:
    // [1, 2, 0xffff_ffff_ffff_ffff] would pass without doing
    // [1u64, 2, 0xffff_ffff_ffff_ffff] instead.
    Operand first_item = opv_at(&elems, 0);
    SqRef arr_base = sq_i_alloc8(sq_const_int(type_size(first_item.type) * elems.size));
    sq_i_storel(operand_to_sqref_imm(&first_item), arr_base);
    for (int i = 1; i < elems.size; ++i) {
      Operand next_item = opv_at(&elems, i);
      if (!convert_operand(&next_item, first_item.type)) {
        errorf("List item %d is of type %s which does not match type %s of first element.", i + 1,
               type_as_str(next_item.type), type_as_str(first_item.type));
      }
      sq_i_storel(operand_to_sqref_imm(&next_item),
                  sq_i_add(sq_type_long, arr_base, sq_const_int(type_size(first_item.type) * i)));
    }
    return operand_rvalue_imm(type_array(first_item.type, elems.size), arr_base);
  }
}

static Operand parse_list_literal_or_compr(bool can_assign, Type* expected) {
  TokenCursor original;
  TokenCursor at_for;
  bool is_compr = scan_to_determine_if_comprehension(&original, &at_for);

  Operand result;
  if (is_compr) {
    result = parse_list_comprehension(original, at_for, expected);
  } else {
    result = parse_list_literal(expected);
  }

  consume(TOK_RSQUARE, "Expect ']' to terminate list.");
  return result;
}

static Operand parse_null_literal(bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
  return operand_null;
}

// Should probably write one that doesn't require nul termination instead.
// We also don't support 'e' in the lexer, but this does, and probably other
// minor variations. But this is OK for simple 1.0 type things for now.
#if 0
static double scan_double(StrView num) {
  char* copy = arena_push(parser.arena, num.size + 1, 1);
  memcpy(copy, num.data, num.size);
  copy[num.size] = 0;
  char* end;
  *(char*)memchr(copy, '`', num.size) = '.';
  return strtod(copy, &end);
}
#endif

static Operand parse_float_literal(bool can_assign, Type* expected) {
#if 0
  StrView view = get_strview_for_offsets(prev_offset(), cur_offset());
  while (view.data[view.size - 1] == ' ') {
    --view.size;
  }
  double val = scan_double(view);
  return operand_const(type_double, (Val){.d = val});
#endif
  ASSERT(false && "todo");
  return operand_null;
}

static Operand parse_int_literal(bool can_assign, Type* expected) {
  Type suffix = {0};
  StrView view = get_strview_for_offsets(prev_offset(), cur_offset());
  ASSERT(view.size > 0);
  while (view.data[view.size - 1] == ' ') {
    --view.size;
  }
  uint64_t val = scan_int(view, &suffix);
  Operand operand = operand_const(type_u64, (Val){.u64 = val});
  Type type = type_u64;
  bool overflow = false;
  switch (type_kind(suffix)) {
    case TYPE_NONE:
      type = type_i32;
      if (val > INT32_MAX) {
        type = type_u32;
        if (val > UINT32_MAX) {
          type = type_i64;
          if (val > INT64_MAX) {
            type = type_u64;
          }
        }
      }
      break;
    case TYPE_I8:
      type = type_i8;
      if (val > INT8_MAX) {
        overflow = true;
      }
      break;
    case TYPE_U8:
      type = type_u8;
      if (val > UINT8_MAX) {
        overflow = true;
      }
      break;
    case TYPE_I16:
      type = type_i16;
      if (val > INT16_MAX) {
        overflow = true;
      }
      break;
    case TYPE_U16:
      type = type_u16;
      if (val > UINT16_MAX) {
        overflow = true;
      }
      break;
    case TYPE_I32:
      type = type_i32;
      if (val > INT32_MAX) {
        overflow = true;
      }
      break;
    case TYPE_U32:
      type = type_u32;
      if (val > UINT32_MAX) {
        overflow = true;
      }
      break;
    case TYPE_I64:
      type = type_i64;
      overflow = val > INT64_MAX;
      break;
    case TYPE_U64:
      type = type_u64;
      break;
    default:
      ASSERT(false);
      break;
  }
  if (overflow) {
    error("Integer literal overflow.");
  }

  cast_operand(&operand, type);
  return operand;
}

static Operand parse_offsetof(bool can_assign, Type* expected) {
  consume(TOK_LPAREN, "Expect '(' after offsetof.");
  Type type = parse_type();
  if (type_kind(type) != TYPE_STRUCT) {
    errorf("Cannot use offsetof on non-struct type %s.", type_as_str(type));
  }
  consume(TOK_COMMA, "Expect ','.");
  // An expression here is too general and a field isn't general enough. It
  // should really be "stuff that comes after a ." but for now we only support a
  // field name since that's the most common use.
  Str field = parse_name("Expect field name in offsetof.");
  uint32_t name_offset = prev_offset();
  consume(TOK_RPAREN, "Expect ')' after offsetof.");
  for (uint32_t i = 0; i < type_struct_num_fields(type); ++i) {
    if (str_eq(type_struct_field_name(type, i), field)) {
      uint32_t offset = type_struct_field_offset(type, i);
      return operand_const(type_i32, (Val){.i32 = offset});
    }
  }
  errorf_offset(name_offset, "'%s' is not a field of type %s.", cstr_copy(parser.arena, field),
                cstr_copy(parser.arena, type_struct_decl_name(type)));
}

static Operand parse_or(Operand left, bool can_assign, Type* expected) {
  // TODO: could have const eval here
  if (!type_is_condition(left.type)) {
    errorf("Left-hand side of or cannot be type %s.", type_as_str(left.type));
  }

  SqBlock block_rval = sq_block_declare();
  SqBlock block_false = sq_block_declare();
  SqBlock block_done = sq_block_declare();

  SqRef result = sq_i_alloc8(sq_const_int(type_size(type_bool)));
  sq_i_storeb(sq_const_int(1), result);

  sq_i_jnz(operand_to_sqref_imm(&left), block_done, block_rval);

  sq_block_start(block_rval);
  Operand right = parse_precedence(PREC_OR, &type_bool);
  if (!type_is_condition(right.type)) {
    errorf("Right-hand side of or cannot be type %s.", type_as_str(right.type));
  }
  sq_i_jnz(operand_to_sqref_imm(&right), block_done, block_false);

  sq_block_start(block_false);
  sq_i_storeb(sq_const_int(0), result);

  sq_block_start(block_done);

  return operand_rvalue_imm(type_bool, sq_i_loadub(sq_type_word, result));
}

static Operand parse_range(bool can_assign, Type* expected) {
  consume(TOK_LPAREN, "Expect '(' after range.");
  Operand first = parse_precedence(PREC_OR, &type_i64);
  if (!convert_operand(&first, type_i64)) {
    errorf("Cannot convert type %s to i64.", type_as_str(first.type));
  }

  Operand second = operand_null;
  Operand third = operand_null;
  if (match(TOK_COMMA)) {
    second = parse_precedence(PREC_OR, &type_i64);
    if (!convert_operand(&second, type_i64)) {
      errorf("Cannot convert type %s to i64.", type_as_str(second.type));
    }
    if (match(TOK_COMMA)) {
      third = parse_precedence(PREC_OR, &type_i64);
      if (!convert_operand(&third, type_i64)) {
        errorf("Cannot convert type %s to i64.", type_as_str(third.type));
      }
    }
  }
  consume(TOK_RPAREN, "Expect ')' after range.");

  SqRef range = sq_i_alloc8(sq_const_int(24));
  SqRef astart = range;
  SqRef astop = sq_i_add(sq_type_long, range, sq_const_int(8));
  SqRef astep = sq_i_add(sq_type_long, range, sq_const_int(16));
  if (type_is_none(second.type)) {
    // range(0, first, 1)
    sq_i_storel(sq_const_int(0), astart);
    sq_i_storel(operand_to_sqref_imm(&first), astop);
    sq_i_storel(sq_const_int(1), astep);
  } else {
    // range(first, second, third || 1)
    sq_i_storel(operand_to_sqref_imm(&first), astart);
    sq_i_storel(operand_to_sqref_imm(&second), astop);
    if (type_is_none(third.type)) {
      sq_i_storel(sq_const_int(1), astep);
    } else {
      sq_i_storel(operand_to_sqref_imm(&third), astep);
    }
  }
  return operand_rvalue_local_addr(type_range, range);
}

static Operand parse_sizeof(bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
  return operand_null;
}

static SqRef emit_string_obj(StrView str) {
  ++parser.str_counter;

  sq_data_start(sq_linkage_default,
                cstr_copy(parser.arena, str_internf("strdat_%d", parser.str_counter)));
  for (uint32_t i = 0; i < str.size; ++i) {
    sq_data_byte(str.data[i]);
  }
  sq_data_byte(0);
  SqSymbol string_data = sq_data_end();

  sq_data_start(sq_linkage_default,
                cstr_copy(parser.arena, str_internf("strobj_%d", parser.str_counter)));
  sq_data_ref(string_data, 0);
  sq_data_long(str.size);
  SqSymbol string_obj = sq_data_end();

  sq_itemctx_activate(parser.cur_scope->func_item_ctx);
  return sq_ref_for_symbol(string_obj);
}

static Operand parse_string(bool can_assign, Type* expected) {
  StrView strview = get_strview_for_offsets(prev_offset(), cur_offset());
  StrView inside_quotes = {strview.data + 1, strview.size - 2};
  if (memchr(strview.data, '\\', strview.size) != NULL) {  // worthwhile?
    // Mutates source buffer!
    uint32_t new_len = str_process_escapes((char*)inside_quotes.data, inside_quotes.size);
    if (new_len == 0) {
      error("Invalid string escape.");
    }
    inside_quotes.size = new_len;
    return operand_rvalue_global_addr(type_str, emit_string_obj(inside_quotes));
  } else {
    return operand_rvalue_global_addr(type_str, emit_string_obj(inside_quotes));
  }
}

static Operand parse_string_interpolate(bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
  return operand_null;
}

static Operand parse_subscript(Operand left, bool can_assign, Type* expected) {
  SqRef target_addr;
  Type subtype;

  if (match(TOK_COLON)) {
    if (check(TOK_RSQUARE)) {  // [:]
      // slice(left, NULL, NULL);
      error("TODO: [:]");
    } else {  // [:x]
      // slice(left, NULL, parse_expression())
      error("TODO: [:x]");
    }
  } else {
    Operand subscript = parse_expression(NULL);
    if (match(TOK_COLON)) {
      if (check(TOK_RSQUARE)) {  // [x:]
        // slice(left, subscript, NULL)
        error("TODO: [x:]");
      } else {  // [x:y]
        // slice(left, subscript, parse_expression());
        error("TODO: [x:y]");
      }
    } else {
      // Regular subscript.
      TypeKind left_type_kind = type_kind(left.type);
      switch (left_type_kind) {
        case TYPE_ARRAY:
        case TYPE_LIST:
        case TYPE_PTR:
        case TYPE_STR: {
          if (!type_is_integer(subscript.type)) {
            errorf("Cannot subscript using type %s.", type_as_str(subscript.type));
          }
          if (left_type_kind == TYPE_ARRAY) {
            ASSERT(op_is_local_addr(left));
            subtype = type_array_subtype(left.type);
            target_addr = sq_i_add(sq_type_long, left.ref,
                                   sq_i_mul(sq_type_long, sq_const_int(type_size(subtype)),
                                            operand_to_sqref_imm(&subscript)));
          } else if (left_type_kind == TYPE_PTR) {
            subtype = type_ptr_subtype(left.type);
            target_addr = sq_i_add(
                sq_type_long, op_is_local_addr(left) ? sq_i_load(sq_type_long, left.ref) : left.ref,
                sq_i_mul(sq_type_long, sq_const_int(type_size(subtype)),
                         operand_to_sqref_imm(&subscript)));
          } else {
            error("TODO: subscript impl");
          }
        break;
        default:
          errorf("Cannot subscript type %s.", type_as_str(left.type));
        }
      }
    }
  }
  consume(TOK_RSQUARE, "Expect ']' to complete subscript.");

  ASSERT(!type_is_none(subtype));
  if (can_assign && match_assignment()) {
    Operand rhs = parse_expression(NULL); // TODO: do type here
    if (!convert_operand(&rhs, subtype)) {
      errorf("Cannot store type %s into %s.", type_as_str(rhs.type), type_as_str(left.type));
    }
    StoreFunc func = store_by_type(rhs.type);
    func(operand_to_sqref_imm(&rhs), target_addr);
    return operand_null;
  } else {
    return operand_rvalue_imm(subtype, sq_i_load(sqbasetype_from_type(subtype), target_addr));
  }
}

static Operand parse_typeid(bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
  return operand_null;
}

static long long eval_unary_op_ll(TokenKind op, long long val) {
  switch (op) {
    case TOK_PLUS:
      return +val;
    case TOK_MINUS:
      return -val;
#if 0 // TODO
    case TOK_TILDE:
      return ~val;
#endif
    case TOK_NOT:
      return !val;
    default:
      error("Unexpected unary op.");
  }
}

static unsigned long long eval_unary_op_ull(TokenKind op, unsigned long long val) {
  error("TODO: ull unary const eval");
}

static Val eval_unary_op(TokenKind op, Type type, Val val) {
  if (type_is_integer(type)) {
    Operand operand = operand_const(type, val);
    if (type_is_signed(type)) {
      cast_operand(&operand, type_i64);
      operand.val.i64 = eval_unary_op_ll(op, operand.val.i64);
    } else {
      cast_operand(&operand, type_u64);
      operand.val.u64 = eval_unary_op_ull(op, operand.val.u64);
    }
    cast_operand(&operand, type);
    return operand.val;
  } else {
    errorf("Unexpected type %s in eval_unary_op.", type_as_str(type));
  }
}

static Operand parse_unary(bool can_assign, Type* expected) {
  TokenKind op_kind = parser.cursor.prev_kind;
  Operand expr = parse_precedence(PREC_UNARY, expected);
  if (op_kind == TOK_MINUS) {
    if (op_is_const(expr)) {
      return operand_const(expr.type, eval_unary_op(op_kind, expr.type, expr.val));
    } else {
      ASSERT(false && "todo");
      return operand_null;
#if 0
      return operand_rvalue_imm(expr.type,
                                ir_NEG(type_to_ir_type(expr.type), operand_to_irref_imm(&expr)));
#endif
    }
  } else if (op_kind == TOK_NOT) {
    // TODO: const eval
    if (type_is_condition(expr.type)) {
      return operand_rvalue_imm(expr.type, sq_i_ceqw(sqbasetype_from_type(expr.type),
                                                     operand_to_sqref_imm(&expr), sq_const_int(0)));
    } else {
      errorf("Type %s cannot be used in a boolean not.", type_as_str(expr.type));
    }
  } else if (op_kind == TOK_AMPERSAND) {
#if 0
    return operand_rvalue_imm(type_ptr(expr.type), ir_VADDR(expr.ref));
#endif
    return operand_rvalue_imm(type_ptr(expr.type), expr.ref);
  } else {
    error("unary operator not implemented");
  }
}

static Sym* find_in_scope(Scope* scope, Str name) {
  if (BRANCH_UNLIKELY(scope->is_full_dict)) {
    DictRawIter iter =
        dict_find(&scope->sym_dict, &name, namesym_hash_func, namesym_eq_func, sizeof(NameSymPair));
    NameSymPair* nsp = (NameSymPair*)dict_rawiter_get(&iter);
    if (!nsp) {
      return NULL;
    }
    return &nsp->sym;
  } else {
    SmallFlatNameSymMap* nm = &scope->flat_map;
    for (int i = nm->num_entries - 1; i >= 0; --i) {
      if (str_eq(nm->names[i], name))
        return &nm->syms[i];
    }
    return NULL;
  }
}

// Returns pointer into sym_dict/flat_map (where Sym is stored by value),
// probably a bad idea.
// if crossed_function, map parameters and locals to upvalue
static ScopeResult scope_lookup_single(Scope* scope, Str name, bool crossed_function, Sym** sym) {
  Sym* found_sym = find_in_scope(scope, name);
  if (found_sym) {
    *sym = found_sym;
    SymScopeDecl sd = found_sym->scope_decl;
    switch (sd) {
      case SSD_DECLARED_GLOBAL: {
        if (scope->is_module) {
          return SCOPE_RESULT_GLOBAL;
        } else {
          ASSERT(scope->is_function && !crossed_function);
          return SCOPE_RESULT_LOCAL;
        }
      }
      case SSD_DECLARED_LOCAL:
        if (scope->is_module) {
          return SCOPE_RESULT_GLOBAL;
        } else if (crossed_function) {
          return SCOPE_RESULT_UPVALUE;
        } else {
          return SCOPE_RESULT_LOCAL;
        }
        break;
      case SSD_DECLARED_PARAMETER:
        if (crossed_function) {
          return SCOPE_RESULT_UPVALUE;
        } else {
          return SCOPE_RESULT_PARAMETER;
        }
      case SSD_DECLARED_NONLOCAL:
        return SCOPE_RESULT_UPVALUE;
      default:
        errorf("internal error, SymScopeDecl: %d", sd);
    }
  }
  return SCOPE_RESULT_UNDEFINED;
}

// Returns pointer into sym_dict/flat_map (where Sym is stored by value),
// probably a bad idea.
static ScopeResult scope_lookup_recursive(Str name, Sym** sym) {
  *sym = NULL;
  bool crossed_function = false;
  Scope* cur_scope = parser.cur_scope;
  for (;;) {
    ScopeResult res = scope_lookup_single(cur_scope, name, crossed_function, sym);
    if (res != SCOPE_RESULT_UNDEFINED) {
      return res;
    }
    // otherwise keep going upwards

    if (cur_scope->is_function) {
      crossed_function = true;
    }

    if (cur_scope == &parser.scopes[0]) {
      break;
    }
    ASSERT(cur_scope >= &parser.scopes[0] &&
           cur_scope <= &parser.scopes[parser.num_scopes - 1]);
    cur_scope--;  // parent
  }

  return SCOPE_RESULT_UNDEFINED;
}

static int create_upval(Scope* scope, Str name, Sym* sym) {
  UpvalMap* uvm = &scope->upval_map;
  if (uvm->num_upvals >= COUNTOFI(uvm->upvals)) {
    error("Too many upvals.");
  }
  int upval_index = uvm->num_upvals++;

  Type type = sym->type;
  uvm->alloc_size = ALIGN_UP(uvm->alloc_size, type_align(type));

  Upval* uv = &uvm->upvals[upval_index];
  *uv = (Upval){.name = name, .type = type, .offset = uvm->alloc_size};
  uvm->alloc_size += type_size(type);

  // If we created a reference in the current scope, we need to walk up parent
  // scopes creating upvals there to make sure that middle scopes that didn't
  // otherwise use the value themselves will have it forwarded to them, so
  // that it can be captured by the inner-most.

  ASSERT(scope >= &parser.scopes[1] && scope <= &parser.scopes[parser.num_scopes - 1]);
  Scope* parent_scope = scope - 1;
  if (parent_scope->upval_base.u) {
    Sym* parent_sym;
    ScopeResult parent_scope_result = scope_lookup_single(parent_scope, name, false, &parent_sym);
    switch (parent_scope_result) {
      case SCOPE_RESULT_GLOBAL:
        error("internal error, shouldn't be upval'ing global");
        break;
      case SCOPE_RESULT_UPVALUE:
        error("internal error, not sure what to do with this yet, nonlocal in middle?");
        break;
      case SCOPE_RESULT_PARAMETER:
        // If it's known in the parent, then save this lookup type, and we're done.
        ASSERT(parent_sym == sym);
        uv->scope_result = SCOPE_RESULT_PARAMETER;
        uv->ref = sym->ref;
        break;
      case SCOPE_RESULT_LOCAL:
        // If it's known in the parent, then save this lookup type, and we're done.
        ASSERT(parent_sym == sym);
        uv->scope_result = SCOPE_RESULT_LOCAL;
        uv->ref = sym->ref;
        break;
      case SCOPE_RESULT_UNDEFINED:
        // This is not defined in the parent, so it must be an upval in the
        // parent (in a "middle" def that doesn't actually declare or use the
        // variable we're looking for). We recurse and create an upval in
        // the parent, but we don't (cannot) create a "load" because the
        // ir_ref values would be in the current function, not the parent.
        uv->scope_result = SCOPE_RESULT_UPVALUE;
        create_upval(parent_scope, name, sym);
        break;
    }
  } else {
    // If the parent isn't a nested function, then it must be toplevel so
    // there's nothing to capture-forward to it, and the method of looking
    // it up must be just a local or a param.
    // TODO: assert something about sym here.
    uv->scope_result =
        sym->scope_decl == SSD_DECLARED_LOCAL ? SCOPE_RESULT_LOCAL : SCOPE_RESULT_PARAMETER;
    uv->ref = sym->ref;
  }

  return upval_index;
}

static Operand find_or_create_upval(Scope* scope, Str name, Sym* sym) {
  ASSERT(!str_is_none(name));
  UpvalMap* uvm = &scope->upval_map;
  int upval_index;
  for (upval_index = uvm->num_upvals - 1; upval_index >= 0; --upval_index) {
    if (str_eq(name, uvm->upvals[upval_index].name)) {
      break;
    }
  }

  if (upval_index < 0) {
    // Didn't find it in the existing map, add a reference and then return it.
    upval_index = create_upval(scope, name, sym);
  }

  Type type = sym->type;
  LoadFunc func = load_by_type(type);
  SqRef val = func(
      sqbasetype_from_type(type),
      sq_i_add(sq_type_long, scope->upval_base, sq_const_int(uvm->upvals[upval_index].offset)));
  return operand_rvalue_imm(type, val);
}

static Operand load_value(ScopeResult scope_result, Sym* sym, Str var_name) {
  switch (scope_result) {
    case SCOPE_RESULT_LOCAL:
      if (type_kind(sym->type) == TYPE_FUNC) {
        if (type_func_is_nested(sym->type)) {
          return operand_bound_local_function(sym->type, sq_ref_for_symbol(sym->global), sym->ref2);
        } else {
          return operand_rvalue_global_addr(sym->type, sq_ref_for_symbol(sym->global));
        }
      } else {
        if (sym->scope_decl == SSD_DECLARED_GLOBAL) {
          return operand_lvalue_global_addr(sym->type, sq_ref_for_symbol(sym->global));
        } else {
          return operand_lvalue_local(sym->type, sym->ref);
        }
      }
    case SCOPE_RESULT_PARAMETER: {
      return operand_rvalue_imm(sym->type, sym->ref);
    }
    case SCOPE_RESULT_GLOBAL: {
      if (type_kind(sym->type) == TYPE_FUNC) {
        // Doesn't make sense in our use for GLOBAL to be bound I don't think.
        return operand_rvalue_global_addr(sym->type, sq_ref_for_symbol(sym->global));
      } else {
        return operand_lvalue_global_addr(sym->type, sq_ref_for_symbol(sym->global));
      }
    }
    case SCOPE_RESULT_UPVALUE: {
      // We already did a scope_lookup() so we know the in the current function,
      // we need to reference this value through $up.
      Operand value = find_or_create_upval(parser.cur_scope, var_name, sym);
      return value;
    }
    case SCOPE_RESULT_UNDEFINED: {
      errorf("Undefined reference to '%s'.", cstr_copy(parser.arena, var_name));
    }
  }
}

static Operand parse_variable(bool can_assign, Type* expected) {
  Str target = str_from_previous();
  Sym* sym = NULL;
  ScopeResult scope_result = scope_lookup_recursive(target, &sym);
  if (can_assign && match_assignment()) {
    TokenKind eq_kind = parser.cursor.prev_kind;
    TokenKind eq_offset = prev_offset();
    switch (scope_result) {
      case SCOPE_RESULT_LOCAL: {
        // If we found an existing local, we're just assigning to it here.
        ASSERT(sym);
        Operand op = parse_expression(NULL);
        if (!convert_operand(&op, sym->type)) {
          errorf("Cannot assign type %s to type %s.", type_as_str(op.type), type_as_str(sym->type));
        }
        if (eq_kind == TOK_EQ) {
          StoreFunc func = store_by_type(op.type);
          func(operand_to_sqref_imm(&op), sym->ref);
          return operand_null;
        } else {
          error_offset(eq_offset, "Unhandled assignment type.");
        }
      }
      case SCOPE_RESULT_UNDEFINED:
      case SCOPE_RESULT_GLOBAL: {
        if (parser.cur_scope->is_function) {
          ASSERT(!parser.cur_scope->is_module);

#if 0
          // Assigning to a global from a function.
          ASSERT(sym);
          Operand op = parse_expression(NULL);
          if (!convert_operand(&op, sym->type)) {
            errorf("Cannot assign type %s to type %s.", type_as_str(op.type),
                   type_as_str(sym->type));
          }
          ASSERT(eq_kind == TOK_EQ);
          ir_STORE(ir_CONST_ADDR(sym->addr), operand_to_irref_imm(&op));
          return operand_null;
#endif
          ASSERT((scope_result == SCOPE_RESULT_UNDEFINED && !sym) ||
                 (scope_result == SCOPE_RESULT_GLOBAL && sym));
          // If a local wasn't found, then implicitly create and initialize it.
          // (If it was found in the global scope, then it's not relevant for
          // assignment because a `global blah` will be found as a local with a
          // scope_decl of GLOBAL instead.)
          if (eq_kind == TOK_EQ) {
            // Local variable declaration without a type.
            Operand op = parse_expression(NULL);
            make_local_and_alloc(SYM_VAR, target, op.type, &op);
            return operand_null;
          } else {
            error_offset(eq_offset,
                         "Cannot use an augmented assignment when implicitly declaring a local.");
          }
        } else {
          ASSERT(parser.cur_scope->is_module);
          ASSERT(!parser.cur_scope->is_function);
          ASSERT(eq_kind == TOK_EQ);
          if (scope_result == SCOPE_RESULT_UNDEFINED) {
            // Global variable declaration without a type.
            Operand op = const_expression();
            if (!op_is_const(op)) {
              error("Global initializers must be constants.");
            }
            make_global(SYM_VAR, target, op.type, op.val);
            return operand_null;
#if 0
            Sym* new_global = make_global(SYM_VAR, target, op.type, op.val);
            return operand_lvalue_global_addr(op.type, sq_ref_for_symbol(new_global->global));
#endif
          } else {
            ASSERT(scope_result == SCOPE_RESULT_GLOBAL);
            error("Cannot re-initialize an existing global.");
          }
        }
      }

      case SCOPE_RESULT_PARAMETER: {
        error_offset(eq_offset, "Function parameters are immutable.");
      }

      case SCOPE_RESULT_UPVALUE: {
        error_offset(eq_offset, "Upvalues are immutable.");
      }
    }
  } else {
    return load_value(scope_result, sym, target);
  }
}

// Has to match the order in tokens.inc.
static Rule rules[NUM_TOKEN_KINDS] = {
    {NULL, NULL, PREC_NONE},  // TOK_INVALID
    {NULL, NULL, PREC_NONE},  // TOK_EOF
    {NULL, NULL, PREC_NONE},  // TOK_INDENT
    {NULL, NULL, PREC_NONE},  // TOK_DEDENT
    {NULL, NULL, PREC_NONE},  // TOK_NEWLINE

    {NULL, NULL, PREC_NONE},  // TOK_NEWLINE_BLANK
    {NULL, NULL, PREC_NONE},  // TOK_NEWLINE_INDENT_0
    {NULL, NULL, PREC_NONE},  // TOK_NEWLINE_INDENT_4
    {NULL, NULL, PREC_NONE},  // TOK_NEWLINE_INDENT_8
    {NULL, NULL, PREC_NONE},  // TOK_NEWLINE_INDENT_12
    {NULL, NULL, PREC_NONE},  // TOK_NEWLINE_INDENT_16
    {NULL, NULL, PREC_NONE},  // TOK_NEWLINE_INDENT_20
    {NULL, NULL, PREC_NONE},  // TOK_NEWLINE_INDENT_24
    {NULL, NULL, PREC_NONE},  // TOK_NEWLINE_INDENT_28
    {NULL, NULL, PREC_NONE},  // TOK_NEWLINE_INDENT_32
    {NULL, NULL, PREC_NONE},  // TOK_NEWLINE_INDENT_36
    {NULL, NULL, PREC_NONE},  // TOK_NEWLINE_INDENT_40
    {NULL, NULL, PREC_NONE},  // TOK_NL

    {parse_unary, parse_binary, PREC_BITS},                     // TOK_AMPERSAND
    {parse_alignof, NULL, PREC_NONE},                           // TOK_ALIGNOF
    {parse_unary, NULL, PREC_NONE},                             // TOK_ALLOC
    {NULL, parse_and, PREC_AND},                                // TOK_AND
    {NULL, NULL, PREC_NONE},                                    // TOK_AS
    {NULL, parse_binary, PREC_EQUALITY},                        // TOK_BANGEQ
    {NULL, NULL, PREC_NONE},                                    // TOK_BREAK
    {NULL, parse_binary, PREC_BITS},                            // TOK_CARET
    {parse_unary, NULL, PREC_NONE},                             // TOK_CAST
    {NULL, NULL, PREC_NONE},                                    // TOK_CHECK
    {NULL, NULL, PREC_NONE},                                    // TOK_COLON
    {NULL, NULL, PREC_NONE},                                    // TOK_COMMA
    {NULL, NULL, PREC_NONE},                                    // TOK_CONST
    {NULL, NULL, PREC_NONE},                                    // TOK_CONTINUE
    {NULL, NULL, PREC_NONE},                                    // TOK_DEF
    {NULL, NULL, PREC_NONE},                                    // TOK_DEL
    {NULL, parse_dot, PREC_CALL},                               // TOK_DOT
    {NULL, NULL, PREC_NONE},                                    // TOK_ELIF
    {NULL, NULL, PREC_NONE},                                    // TOK_ELSE
    {NULL, NULL, PREC_NONE},                                    // TOK_EQ
    {NULL, parse_binary, PREC_EQUALITY},                        // TOK_EQEQ
    {NULL, NULL, PREC_NONE},                                    // TOK_ERROR
    {parse_bool_literal, NULL, PREC_NONE},                      // TOK_FALSE
    {parse_float_literal, NULL, PREC_NONE},                     // TOK_FLOAT_LITERAL
    {NULL, NULL, PREC_NONE},                                    // TOK_FOR
    {NULL, NULL, PREC_NONE},                                    // TOK_FOREIGN
    {NULL, parse_binary, PREC_COMPARISON},                      // TOK_GEQ
    {NULL, NULL, PREC_NONE},                                    // TOK_GLOBAL
    {NULL, parse_binary, PREC_COMPARISON},                      // TOK_GT
    {parse_variable, NULL, PREC_NONE},                          // TOK_IDENT_VAR
    {parse_compound_literal, NULL, PREC_NONE},                  // TOK_IDENT_TYPE
    {parse_variable, NULL, PREC_NONE},                          // TOK_IDENT_CONST
    {NULL, NULL, PREC_NONE},                                    // TOK_IDENT_DECORATOR
    {NULL, NULL, PREC_NONE},                                    // TOK_IF
    {NULL, NULL, PREC_NONE},                                    // TOK_IMPORT
    {NULL, parse_in_or_not_in, PREC_COMPARISON},                // TOK_IN
    {parse_int_literal, NULL, PREC_NONE},                       // TOK_INT_LITERAL
    {parse_dict_literal, NULL, PREC_NONE},                      // TOK_LBRACE
    {parse_len, NULL, PREC_NONE},                               // TOK_LEN
    {NULL, parse_binary, PREC_COMPARISON},                      // TOK_LEQ
    {parse_grouping, parse_call, PREC_CALL},                    // TOK_LPAREN
    {NULL, parse_binary, PREC_SHIFT},                           // TOK_LSHIFT
    {parse_list_literal_or_compr, parse_subscript, PREC_CALL},  // TOK_LSQUARE
    {NULL, parse_binary, PREC_COMPARISON},                      // TOK_LT
    {parse_unary, parse_binary, PREC_TERM},                     // TOK_MINUS
    {NULL, NULL, PREC_NONE},                                    // TOK_NONLOCAL
    {parse_unary, parse_in_or_not_in, PREC_COMPARISON},         // TOK_NOT
    {parse_null_literal, NULL, PREC_NONE},                      // TOK_NULL
    {parse_offsetof, NULL, PREC_NONE},                          // TOK_OFFSETOF
    {NULL, NULL, PREC_NONE},                                    // TOK_ON
    {NULL, parse_or, PREC_OR},                                  // TOK_OR
    {NULL, NULL, PREC_NONE},                                    // TOK_PASS
    {NULL, parse_binary, PREC_FACTOR},                          // TOK_PERCENT
    {NULL, parse_binary, PREC_BITS},                            // TOK_PIPE
    {NULL, parse_binary, PREC_TERM},                            // TOK_PLUS
    {NULL, NULL, PREC_NONE},                                    // TOK_PRINT
    {parse_range, NULL, PREC_NONE},                             // TOK_RANGE
    {NULL, NULL, PREC_NONE},                                    // TOK_RBRACE
    {NULL, NULL, PREC_NONE},                                    // TOK_RELOCATE
    {NULL, NULL, PREC_NONE},                                    // TOK_RETURN
    {NULL, NULL, PREC_NONE},                                    // TOK_RPAREN
    {NULL, parse_binary, PREC_SHIFT},                           // TOK_RSHIFT
    {NULL, NULL, PREC_NONE},                                    // TOK_RSQUARE
    {parse_sizeof, NULL, PREC_NONE},                            // TOK_SIZEOF
    {NULL, parse_binary, PREC_FACTOR},                          // TOK_SLASH
    {NULL, parse_binary, PREC_FACTOR},                          // TOK_STAR
    {parse_string_interpolate, NULL, PREC_NONE},                // TOK_STRING_INTERP
    {parse_string, NULL, PREC_NONE},                            // TOK_STRING_QUOTED
    {parse_string, NULL, PREC_NONE},                            // TOK_STRING_RAW
    {NULL, NULL, PREC_NONE},                                    // TOK_STRUCT
    {parse_bool_literal, NULL, PREC_NONE},                      // TOK_TRUE
    {NULL, NULL, PREC_NONE},                                    // TOK_TYPEDEF
    {parse_typeid, NULL, PREC_NONE},                            // TOK_TYPEID
    {NULL, NULL, PREC_NONE},                                    // TOK_TYPEOF
    {NULL, NULL, PREC_NONE},                                    // TOK_WITH

    {NULL, NULL, PREC_NONE},  // TOK_BOOL
    {NULL, NULL, PREC_NONE},  // TOK_BYTE
    {NULL, NULL, PREC_NONE},  // TOK_CODEPOINT
    {NULL, NULL, PREC_NONE},  // TOK_CONST_CHAR
    {NULL, NULL, PREC_NONE},  // TOK_CONST_OPAQUE
    {NULL, NULL, PREC_NONE},  // TOK_DOUBLE
    {NULL, NULL, PREC_NONE},  // TOK_F16
    {NULL, NULL, PREC_NONE},  // TOK_F32
    {NULL, NULL, PREC_NONE},  // TOK_F64
    {NULL, NULL, PREC_NONE},  // TOK_FLOAT
    {NULL, NULL, PREC_NONE},  // TOK_I16
    {NULL, NULL, PREC_NONE},  // TOK_I32
    {NULL, NULL, PREC_NONE},  // TOK_I64
    {NULL, NULL, PREC_NONE},  // TOK_I8
    {NULL, NULL, PREC_NONE},  // TOK_INT
    {NULL, NULL, PREC_NONE},  // TOK_OPAQUE
    {NULL, NULL, PREC_NONE},  // TOK_SIZE_T
    {NULL, NULL, PREC_NONE},  // TOK_STR
    {NULL, NULL, PREC_NONE},  // TOK_U16
    {NULL, NULL, PREC_NONE},  // TOK_U32
    {NULL, NULL, PREC_NONE},  // TOK_U64
    {NULL, NULL, PREC_NONE},  // TOK_U8
    {NULL, NULL, PREC_NONE},  // TOK_UINT
};

static Rule* get_rule(TokenKind tok_kind) {
  ASSERT(tok_kind < NUM_TOKEN_KINDS);
  return &rules[tok_kind];
}

static Operand parse_precedence(Precedence precedence, Type* expected) {
  advance();
  PrefixFn prefix_rule = get_rule(parser.cursor.prev_kind)->prefix;
  if (!prefix_rule) {
    errorf("Expect expression after prefix %s.", token_enum_name(parser.cursor.prev_kind));
  }

  bool can_assign = precedence <= PREC_ASSIGNMENT;
  Operand left = prefix_rule(can_assign, expected);

  while (precedence <= get_rule(parser.cursor.cur_kind)->prec_for_infix) {
    advance();
    InfixFn infix_rule = get_rule(parser.cursor.prev_kind)->infix;
    if (!infix_rule) {
      errorf("Expect expression after infix %s.", token_enum_name(parser.cursor.prev_kind));
    }
    left = infix_rule(left, can_assign, expected);
  }

  if (can_assign && match_assignment()) {
    error("Invalid assignment target.");
  }

  return left;
}

static Operand parse_expression(Type* expected) {
  return parse_precedence(PREC_LOWEST, expected);
}

static void expect_end_of_statement(const char* after_what) {
  if (!match(TOK_NEWLINE)) {
    errorf("Expect newline after %s statement.", after_what);
  }
}

static Operand if_statement_cond_helper(void) {
  Operand cond = parse_expression(NULL);
  if (!type_is_condition(cond.type)) {
    errorf("Result of condition expression cannot be type %s.", type_as_str(cond.type));
  }
  consume(TOK_COLON, "Expect ':' to start if/elif.");
  consume(TOK_NEWLINE, "Expect newline after ':' to start if/elif.");
  consume(TOK_INDENT, "Expect indent to start if/elif.");
  return cond;
}

static void if_statement(void) {
  do {
    Operand opcond = if_statement_cond_helper();
    ASSERT(type_kind(opcond.type) == TYPE_BOOL && "todo, other types");

    SqBlock true_block = sq_block_declare();
    SqBlock false_block = sq_block_declare();
    SqBlock after_block = sq_block_declare();

    sq_i_jnz(operand_to_sqref_imm(&opcond), true_block, false_block);

    sq_block_start(true_block);
    LastStatementType lst = parse_block();
    if (lst != LST_NON_RETURN) {
      sq_i_jmp(parser.cur_scope->return_block);
    } else {
      sq_i_jmp(after_block);
    }

    sq_block_start(false_block);
    if (match(TOK_ELSE)) {
      consume(TOK_COLON, "Expect ':' to start else.");
      consume(TOK_NEWLINE, "Expect newline after ':' to start else.");
      consume(TOK_INDENT, "Expect indent to start else.");
      LastStatementType lst = parse_block();
      if (lst != LST_NON_RETURN) {
        sq_i_jmp(parser.cur_scope->return_block);
      }
      sq_block_start(after_block);
      break;  // No more elifs.
    } else {
      sq_block_start(after_block);
    }

  } while (match(TOK_ELIF));
}

static void for_statement(void) {
  // Can be:
  // 1. no condition
  // 2. condition only, while style
  // 3. iter in expr
  // 4. *ptr in expr
  // 5. index, iter enumerate expr
  // 6. index, *ptr enumerate expr

  if (check(TOK_COLON)) {
    // Nothing, case 1:
  } else {  // if (check(TOK_IDENT_VAR) && peek(2, TOK_IN)) {
    // Case 3.
    Str it_name = parse_name("Expect iterator name.");
    consume(TOK_IN, "Expect 'in'.");
    Operand expr = parse_expression(NULL);
    if (type_eq(expr.type, type_range)) {
      ASSERT(op_is_local_addr(expr));
      SqRef astart = expr.ref;
      SqRef astop = sq_i_add(sq_type_long, expr.ref, sq_const_int(8));
      SqRef astep = sq_i_add(sq_type_long, expr.ref, sq_const_int(16));

      SqRef start = sq_i_load(sq_type_long, astart);
      SqRef stop = sq_i_load(sq_type_long, astop);
      SqRef step = sq_i_load(sq_type_long, astep);

      SqRef is_neg = sq_i_csltl(sq_type_long, step, sq_const_int(0));

      // TODO: This probably needs work if the Range isn't trivial, start
      // should be using the Operand expr or something maybe
      Sym* it = make_local_and_alloc(SYM_VAR, it_name, type_i64, NULL);
      sq_i_storel(start, it->ref);

      SqBlock loop = sq_block_declare_and_start();

      SqRef cur = sq_i_load(sq_type_long, it->ref);

      SqBlock block_neg_step = sq_block_declare();
      SqBlock block_pos_step = sq_block_declare();
      SqBlock block_cont = sq_block_declare();
      SqBlock block_after = sq_block_declare();

      // (is_neg ? cur > stop : cur < stop)
      sq_i_jnz(is_neg, block_neg_step, block_pos_step);

      sq_block_start(block_neg_step);
      sq_i_jnz(sq_i_csgtl(sq_type_long, cur, stop), block_cont, block_after);

      sq_block_start(block_pos_step);
      sq_i_jnz(sq_i_csltl(sq_type_long, cur, stop), block_cont, block_after);

      sq_block_start(block_cont);

      consume(TOK_COLON, "Expect ':' to start for.");
      consume(TOK_NEWLINE, "Expect newline after ':' to start for.");
      consume(TOK_INDENT, "Expect indent to start for.");
      LastStatementType lst = parse_block();
      ASSERT(lst == LST_NON_RETURN && "todo; return from loop");

      SqRef it_val = sq_i_load(sq_type_long, it->ref);
      SqRef inc = sq_i_add(sq_type_long, it_val, step);
      sq_i_storel(inc, it->ref);

      sq_i_jmp(loop);

      sq_block_start(block_after);
    } else {
      errorf("Unhandled for/in over type %s.", type_as_str(expr.type));
    }
  }
}

static void print_statement(void) {
  Operand val = parse_expression(NULL);

  // If __repr__ exists for the type, call it, and then use print_str.
  Sym* sym = lookup_memfn(val.type, parser.static_str_repr);
  if (sym) {
    ASSERT(false && "todo");
#if 0
    ir_ref str = ir_CALL_1(IR_I32, ir_CONST_ADDR(sym->addr), addr_for_operand(&val));
    ir_ref addr = ir_CONST_ADDR(print_i32_impl);
    ir_CALL_1(IR_VOID, addr, str);
#endif
  } else {
    if (type_eq(val.type, type_str)) {
      print_str(&val);
    } else if (type_eq(val.type, type_bool)) {
      print_bool(&val);
    } else if (type_eq(val.type, type_range)) {
      print_range(&val);
#if 0
    } else if (type_eq(val.type, type_float)) {
      print_float(&val);
    } else if (type_eq(val.type, type_double)) {
      print_double(&val);
#endif
    } else if (convert_operand(&val, type_i32)) {
      print_i32(&val);
    } else {
      errorf("TODO: don't know how to print type %s.", type_as_str(val.type));
    }
  }
  expect_end_of_statement("print");
}

static LastStatementType parse_block(void) {
  LastStatementType lst = LST_NON_RETURN;
  while (!check(TOK_DEDENT)) {
    lst = parse_statement(/*toplevel=*/false);
    skip_newlines();
  }

  consume(TOK_DEDENT, "Expect end of block.");
  return lst;
}

// TODO: decorators
static void def_statement(void) {
  Type return_type = parse_type();
  if (type_is_none(return_type)) {
    return_type = type_void;
  }
  uint32_t function_start_offset = cur_offset();
  Str name = parse_name("Expect function name.");
  consume(TOK_LPAREN, "Expect '(' after function name.");

  Type param_types[MAX_FUNC_PARAMS];
  Str param_names[MAX_FUNC_PARAMS];
  bool is_nested = parser.num_scopes > 1;
  if (is_nested) {
    ASSERT(parser.scopes[parser.num_scopes - 1].is_function);
    ASSERT(parser.scopes[0].is_module);
  }
  uint32_t num_params =
      parse_func_params(is_nested, /*memfn_self=*/NULL, (Str){0}, param_types, param_names);

  consume(TOK_COLON, "Expect ':' before function body.");
  consume(TOK_NEWLINE, "Expect newline before function body. (TODO: single line)");
  skip_newlines();
  consume(TOK_INDENT, "Expect indent before function body. (TODO: single line)");

  Type functype =
      type_function(param_types, num_params, return_type, is_nested ? TFF_NESTED : TFF_NONE);

  Sym* funcsym = sym_new(SYM_FUNC, name, functype);
  funcsym->scope_decl = is_nested ? SSD_DECLARED_LOCAL : SSD_DECLARED_GLOBAL;  // ?
  enter_function(funcsym, param_names, param_types);
  LastStatementType lst = parse_block();
  if (lst == LST_NON_RETURN) {
    if (!type_eq(type_void, type_func_return_type(functype))) {
      errorf_offset(function_start_offset,
                    "Function returns %s, but there is no return at the end of the body.",
                    type_as_str(type_func_return_type(functype)));
    }
  }

  leave_function();
}

static void foreign_statement(void) {
  Type return_type = parse_type();
  if (type_is_none(return_type)) {
    return_type = type_void;
  }
  Str name = parse_name("Expect function name.");
  consume(TOK_LPAREN, "Expect '(' after function name.");

  Type param_types[MAX_FUNC_PARAMS];
  Str param_names[MAX_FUNC_PARAMS];
  uint32_t num_params = parse_func_params(/*is_nested=*/false, /*memfn_self=*/NULL, (Str){0},
                                          param_types, param_names);
  Type functype =
      type_function(param_types, num_params, return_type, TFF_FOREIGN);
  Sym* funcsym = sym_new(SYM_FUNC, name, functype);
  funcsym->scope_decl = SSD_DECLARED_GLOBAL;

  ASSERT(false && "todo");
#if 0
  funcsym->addr = parser.get_extern((StrView){str_raw_ptr(name), str_len(name)});
#endif
}

static void on_statement(void) {
  Type on_type;
  Str on_type_name;
  if (parser.cursor.cur_kind >= TOK_BOOL && parser.cursor.cur_kind <= TOK_UINT) {
    on_type = basic_tok_to_type[parser.cursor.cur_kind];
    on_type_name = type_decl_name(on_type);
    advance();
  } else if (check(TOK_IDENT_TYPE)) {
    advance();
    on_type_name = str_from_previous();
    Sym* sym;
    ScopeResult scope_result = scope_lookup_recursive(on_type_name, &sym);
    if (scope_result == SCOPE_RESULT_UNDEFINED) {
      errorf("Undefined type %s.", cstr_copy(parser.arena, on_type_name));
    } else if (scope_result == SCOPE_RESULT_GLOBAL && sym->kind == SYM_TYPE) {
      on_type = sym->type;
    } else {
      error("internal error: unexpected lookup result.");
    }
  } else {
    error("Expect struct type name.");
  }

  bool is_foreign = false;
  if (match(TOK_FOREIGN)) {
    is_foreign = true;
  } else {
    consume(TOK_DEF, "Expect 'def' to start function body.");
  }

  Type return_type = parse_type();
  if (type_is_none(return_type)) {
    return_type = type_void;
  }
  uint32_t function_start_offset = cur_offset();
  Str func_name = parse_name("Expect function name.");
  consume(TOK_LPAREN, "Expect '(' after function name.");
  Str self_name = parse_name("Expect 'self' token.");
  if (!check(TOK_RPAREN)) {
    consume(TOK_COMMA, "Expect comma after 'self' name.");
  }

  Type param_types[MAX_FUNC_PARAMS];
  Str param_names[MAX_FUNC_PARAMS];
  Type self_arg = type_ptr(on_type);
  uint32_t num_params = parse_func_params(/*is_nested=*/false, /*is_memfn=*/&self_arg, self_name,
                                          param_types, param_names);

  ASSERT(!is_foreign && "todo");

  if (!is_foreign) {
    consume(TOK_COLON, "Expect ':' before function body.");
    consume(TOK_NEWLINE, "Expect newline before function body. (TODO: single line)");
    while (match(TOK_NEWLINE)) {
    }
    consume(TOK_INDENT, "Expect indented function body.");
  }

  Type functype = type_function(param_types, num_params, return_type, TFF_MEMFN);

  ASSERT(str_eq(on_type_name, type_decl_name(on_type)));
  Str full_name = memfn_name_from_type_name(on_type_name, func_name);
  Sym* funcsym = sym_new(SYM_FUNC, full_name, functype);
  funcsym->scope_decl = SSD_DECLARED_GLOBAL;
  enter_function(funcsym, param_names, param_types);
  LastStatementType lst = parse_block();
  if (lst == LST_NON_RETURN) {
    if (!type_eq(type_void, type_func_return_type(functype))) {
      errorf_offset(function_start_offset,
                    "Function returns %s, but there is no return at the end of the body.",
                    type_as_str(type_func_return_type(functype)));
    }
  }

  leave_function();
}

static void struct_statement() {
  Str name = parse_type_name("Expect struct type name.");
  consume(TOK_COLON, "Expect ':' after struct name.");
  consume(TOK_NEWLINE, "Expect newline to start struct.");
  consume(TOK_INDENT, "Expect indented struct body.");

  Str field_names[MAX_STRUCT_FIELDS];
  Type field_types[MAX_STRUCT_FIELDS];
  Operand field_initializers[MAX_STRUCT_FIELDS];
  bool have_initializers = false;
  uint32_t num_fields = 0;
  for (;;) {
    if (check(TOK_DEDENT)) {
      break;
    }

    Type field_type = parse_type();
    if (type_is_none(field_type)) {
      error("Expect struct field type.");
    }
    field_types[num_fields] = field_type;

    Str field_name = parse_name("Expect struct field name.");
    for (uint32_t i = 0; i < num_fields; ++i) {
      if (str_eq(field_names[i], field_name)) {
        errorf("Duplicate struct field name '%s'.", cstr_copy(parser.arena, field_name));
      }
    }
    field_names[num_fields] = field_name;

    if (match(TOK_EQ)) {
      // TODO: need to support compound_literal is_const evaluation here
      field_initializers[num_fields] = const_expression();
      have_initializers = true;
    } else {
      field_initializers[num_fields] = operand_null;
    }

    ++num_fields;

    do {
      consume(TOK_NEWLINE, "Expect newline after struct field.");
    } while (check(TOK_NEWLINE));
  }
  consume(TOK_DEDENT, "Expecting dedent after struct definition.");

  Type strukt = type_new_struct(name, num_fields, field_names, field_types, have_initializers);
  if (have_initializers) {
    ASSERT(false && "need to make a data to copy");
    uint8_t* blob = arena_push(parser.arena, type_size(strukt), type_align(strukt));
    memset(blob, 0, type_size(strukt));
    for (uint32_t i = 0; i < num_fields; ++i) {
      if (!op_is_null(field_initializers[i])) {
        if (!op_is_const(field_initializers[i])) {
          errorf("Expecting constant initializer for field %s.",
                 cstr_copy(parser.arena, field_names[i]));
        }
        Type field_type = type_struct_field_type(strukt, i);
        if (!convert_operand(&field_initializers[i], field_type)) {
          errorf("Cannot convert initializer to type %s.", type_as_str(field_type));
        }
        // TODO: not 100% certain this is not copying garbage from the val field
        // out of the range of the size of type if it gets convert_operand'd.
        ASSERT(false && "todo");
#if 0
        memcpy(blob + type_struct_field_offset(strukt, i), &field_initializers[i].val,
               type_size(field_type));
#endif
      }
    }
    type_struct_set_initializer_blob(strukt, blob);
  }
  Sym* new = sym_new(SYM_TYPE, name, strukt);
  new->scope_decl = SSD_DECLARED_GLOBAL;
}

static void import_statement(void) {
  Str parts[MAX_PACKAGE_DEPTH];
  int num_parts = 0;
  parts[num_parts++] = parse_name("Expect package name.");
  for (;;) {
    if (match(TOK_DOT)) {
      parts[num_parts++] = parse_name("Expecting nested package name after '.'.");
    } else {
      break;
    }
  }

  // TODO: many things
  // The end result from the imported package is a DictImpl containing
  // name->syms for def, struct, const, var.
  // need address/value for def/var/const.
  // need Type to check and call propertly
  // need struct layout, so it's pretty much the full sym_dict
}

static void parse_variable_statement(Type type) {
  Str name = parse_name("Expect variable or typed variable name.");
  ASSERT(name.i);

  bool have_init;
  ASSERT(!type_is_none(type));
  have_init = match(TOK_EQ);
  uint32_t eq_offset = prev_offset();
  if (have_init) {
    Operand op = parse_expression(&type);
    if (!convert_operand(&op, type)) {
      errorf_offset(eq_offset, "Initializer cannot be converted from type %s to declared type %s.",
                    type_as_str(op.type), type_as_str(type));
    }
    make_local_and_alloc(SYM_VAR, name, op.type, &op);
  } else {
    make_local_and_alloc(SYM_VAR, name, type, NULL);
  }

  expect_end_of_statement("variable declaration");
}

static LastStatementType return_statement(void) {
  Type func_ret = type_func_return_type(parser.cur_scope->func_sym->type);
  ASSERT(!type_is_none(func_ret));
  Operand op = operand_null;
  if (!type_eq(func_ret, type_void)) {
    op = parse_expression(NULL);
    if (!convert_operand(&op, func_ret)) {
      errorf("Cannot convert type %s to expected return type %s.", type_as_str(op.type),
             type_as_str(func_ret));
    }
    StoreFunc func = store_by_type(op.type);
    func(operand_to_sqref_imm(&op), parser.cur_scope->return_slot->ref);
    return LST_RETURN_VALUE;
  } else {
    consume(TOK_NEWLINE, "Expected newline after return in function with no return type.");
    return LST_RETURN_VOID;
  }
}

static void global_statement(void) {
  Str name = parse_name("Expect variable name after global.");

  Sym* sym;
  ScopeResult scope_result = scope_lookup_single(&parser.scopes[0], name, true, &sym);
  if (scope_result != SCOPE_RESULT_GLOBAL) {
    errorf("Undefined global '%.*s'.", str_len(name), str_raw_ptr(name));
  }
  Sym* new = sym_new(SYM_VAR, name, sym->type);
  new->scope_decl = SSD_DECLARED_GLOBAL;
  new->global = sym->global;
}

static LastStatementType parse_statement(bool toplevel) {
  LastStatementType lst = LST_NON_RETURN;

  skip_newlines();

  switch (parser.cursor.cur_kind) {
    case TOK_DEF:
      advance();
      def_statement();
      break;
    case TOK_FOREIGN:
      advance();
      if (!toplevel) error("foreign statement only allowed at top level.");
      foreign_statement();
      break;
    case TOK_ON:
      advance();
      if (!toplevel) error("on statement only allowed at top level.");
      on_statement();
      break;
    case TOK_IMPORT:
      advance();
      if (!toplevel) error("import statement only allowed at top level.");
      import_statement();
      break;
    case TOK_STRUCT:
      advance();
      if (!toplevel) error("struct statement only allowed at top level.");
      struct_statement();
      break;
    case TOK_IF:
      advance();
      if (toplevel) error("if statement not allowed at top level.");
      if_statement();
      break;
    case TOK_FOR:
      advance();
      if (toplevel) error("for statement not allowed at top level.");
      for_statement();
      break;
    case TOK_PRINT:
      advance();
      if (toplevel) error("print statement not allowed at top level.");
      print_statement();
      break;
    case TOK_PASS:
      advance();
      if (toplevel) error("pass statement not allowed at top level.");
      expect_end_of_statement("pass");
      break;
    case TOK_RETURN:
      advance();
      if (toplevel) error("return statement not allowed at top level.");
      lst = return_statement();
      break;
    case TOK_GLOBAL:
      advance();
      if (toplevel) error("global statement not allowed at top level.");
      global_statement();
      break;
    default: {
      Type var_type = parse_type();
      if (!type_is_none(var_type)) {
        parse_variable_statement(var_type);
        break;
      }

      parse_expression(NULL);
      expect_end_of_statement("top level");
      break;
    }
  }

  skip_newlines();
  return lst;
}

static int sqbe_callback_output_function(const char* fmt, va_list ap) {
  const char* prefix = "SQBE INTERNAL ERROR: ";
  size_t n = 1 + vsnprintf(NULL, 0, fmt, ap) + strlen(prefix);
  char* str = malloc(n);  // just a simple malloc because we're going to base_exit() momentarily.
  strcpy(str, prefix);
  vsnprintf(str + strlen(prefix), n, fmt, ap);
  error(str);
}

static void parse_impl(Arena* main_arena,
                       Arena* temp_arena,
                       const char* filename,
                       ReadFileResult file,
                       int verbose,
                       FILE* out_file) {
  type_init(main_arena);

  parser.arena = main_arena;
  parser.var_scope_arena = temp_arena;
  // In the case of "a.a." the worst case for offsets is the same as the number
  // of characters in the buffer.
  parser.token_offsets = (uint32_t*)base_mem_large_alloc(file.allocated_size * sizeof(uint32_t));
  parser.file_contents = (const char*)file.buffer;
  parser.cur_filename = filename;
  parser.num_scopes = 0;
  parser.cur_scope = NULL;
  parser.cursor = (TokenCursor){-1, 0, 0, 0};
  parser.indent_levels[0] = 0;
  parser.num_indents = 1;
  parser.num_buffered_tokens = 0;
  parser.verbose = verbose;
  parser.static_str_main = str_intern_len("main", 4);
  parser.static_str_repr = str_intern_len("__repr__", 8);
  parser.static_str_ret = str_intern_len("$ret", 4);
  parser.static_str_up = str_intern_len("$up", 3);
  parser.str_counter = 0;

  SqConfiguration config = SQ_CONFIGURATION_DEFAULT;
  config.output = out_file;
  config.output_function = sqbe_callback_output_function;
  if (verbose) {
    config.debug_flags = "P";
    // config.debug_flags = "PMNCFAILSRT";
  }
  sq_init(&config);

  sq_data_start(sq_linkage_default, "i32_print_fmt");
  sq_data_string("%d\n");
  sq_data_byte(0);
  parser.i32_print_fmt = sq_data_end();

  sq_data_start(sq_linkage_default, "str_print_fmt");
  sq_data_string("%.*s\n");
  sq_data_byte(0);
  parser.str_print_fmt = sq_data_end();

  sq_data_start(sq_linkage_default, "range2_print_fmt");
  sq_data_string("range(%lld, %lld)\n");
  sq_data_byte(0);
  parser.range2_print_fmt = sq_data_end();

  sq_data_start(sq_linkage_default, "range3_print_fmt");
  sq_data_string("range(%lld, %lld, %lld)\n");
  sq_data_byte(0);
  parser.range3_print_fmt = sq_data_end();

  sq_data_start(sq_linkage_default, "str_true");
  sq_data_string("true");
  sq_data_byte(0);
  parser.str_true = sq_data_end();

  sq_data_start(sq_linkage_default, "str_false");
  sq_data_string("false");
  sq_data_byte(0);
  parser.str_false = sq_data_end();

  sq_type_struct_start("str", 8);
  sq_type_add_field(sq_type_long); // data
  sq_type_add_field(sq_type_long); // len
  parser.sq_type_str = sq_type_struct_end();

  sq_type_struct_start("range", 8);
  sq_type_add_field(sq_type_long); // start
  sq_type_add_field(sq_type_long); // stop
  sq_type_add_field(sq_type_long); // step
  parser.sq_type_range = sq_type_struct_end();

  enter_scope(/*is_module=*/true, /*is_function=*/false, NULL);

  parser.num_tokens = lex_indexer(file.buffer, file.allocated_size, parser.token_offsets);
  token_init(file.buffer);
  if (parser.verbose > 1) {
    token_dump_offsets(parser.num_tokens, parser.token_offsets, file.file_size);
  }
  advance();

  while (parser.cursor.cur_kind != TOK_EOF) {
    parse_statement(/*toplevel=*/true);
  }

  leave_scope();

  sq_shutdown();
}
