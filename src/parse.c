#include "luv60.h"

typedef enum SymKind {
  SYM_NONE,
  SYM_VAR,
  SYM_CONST,
  SYM_FUNC,
  SYM_TYPE,
  SYM_MODULE,
} SymKind;

typedef struct TokenCursor {
  uint32_t token_index;
  TokenKind cur_kind;
  TokenKind prev_kind;
  int paren_level;
} TokenCursor;

typedef struct PeekToken {
  TokenKind kind;
  uint32_t index;
} PeekToken;

typedef struct TokenizedBuffer {
  Str filename;
  const char* file_contents;
  uint32_t num_tokens;
  uint32_t* token_offsets;

  TokenCursor cursor;

  PeekToken token_peeks[16];
  int num_peeks;
  int indent_levels[12];  // This is the maximum possible in lexer.
  int num_indents;

  DictImpl* import_dict;
} TokenizedBuffer;

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
  Module m;
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
    Module module;
    Val val;  // SYM_CONST
  };
  SymScopeDecl scope_decl;
} Sym;

#define MAX_SCOPES 32
#define MAX_FUNC_PARAMS 32
#define MAX_STRUCT_FIELDS 64
#define MAX_UPVALS 32
#define MAX_FMT_ARGS 32
#define MAX_EXIT_CALLS 32
#define MAX_ITERATION_DATAS 32

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

typedef struct ExitCall {
  SqRef func;
  SqRef obj;
} ExitCall;

typedef struct IterationData IterationData;

typedef struct Scope {
  // FuncData
  Sym* func_sym;
  ExitCall exit_call_stack[MAX_EXIT_CALLS];  // TODO: maybe save bound Operand instead?
  int num_exit_calls;
  IterationData* iteration_datas[MAX_ITERATION_DATAS];
  int num_iteration_datas;
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

typedef enum ImportedSymbolKind {
  ISYM_ERROR,
  ISYM_TYPE,
  ISYM_OBJECT,  // var, func
  ISYM_CONST,
} ImportedSymbolKind;

typedef struct ImportedSymbol {
  Str name;
  SymKind kind;
  Type type;
  union {
    const char* extern_name;  // for VAR/FUNC
    Val value;        // for CONST
  };
} ImportedSymbol;

struct ImportedModuleScope {
  // Str -> ImportedSymbol
  DictImpl syms;
};

#define OPK_BIT_CONST 0x1
#define OPK_BIT_LVAL 0x2
#define OPK_BIT_LOCAL_ADDR 0x4
#define OPK_BIT_SECOND_REF 0x8
#define OPK_BIT_GLOBAL_ADDR 0x10

typedef enum OpKind {
  // an immediate value in .ref
  OPK_REF_RVAL = 0,

  // an actual number at compile time in .val
  OPK_CONST = OPK_BIT_CONST,

  // address of a local in .ref, not a named variable (e.g. a compound
  // literal, range, etc.)
  OPK_REF_RVAL_LOCAL_ADDR = OPK_BIT_LOCAL_ADDR,

  // address of a local in .ref to a named variable (could be stored to)
  OPK_REF_LVAL_LOCAL_ADDR = OPK_BIT_LVAL | OPK_BIT_LOCAL_ADDR,

  // global address as const in .ref, and an additional address that points at
  // the closure block in .ref2
  OPK_REF_RVAL_LOCAL_ADDR_BOUND_FUNC = OPK_BIT_SECOND_REF | OPK_BIT_LOCAL_ADDR,

  // global address as const in .ref (read-only; generally a function address)
  OPK_REF_RVAL_GLOBAL_ADDR = OPK_BIT_GLOBAL_ADDR,

  // global address as const in .ref (read-only; generally a function address),
  // and a self pointer in .ref2.
  OPK_REF_RVAL_GLOBAL_ADDR_BOUND_FUNC = OPK_BIT_SECOND_REF | OPK_BIT_GLOBAL_ADDR,

  // global variable address as const in .ref (could be stored to)
  OPK_REF_LVAL_GLOBAL_ADDR = OPK_BIT_LVAL | OPK_BIT_GLOBAL_ADDR,
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

typedef struct CompilerGlobals {
  Arena* arena;
  DictImpl generics_thunk_cache;

  Str static_str_main;
  Str static_str___str__;
  Str static_str___contains__;
  Str static_str___enter__;
  Str static_str___eq__;
  Str static_str___exit__;
  Str static_str_ret;
  Str static_str_up;

  Operand op_null_ptr;

  int verbose;

  int uniq_counter;
} CompilerGlobals;

static CompilerGlobals glob;

typedef struct TranslationUnit {
  TokenizedBuffer tokbuf;

  Arena* var_scope_arena;
  Scope scopes[MAX_SCOPES];
  int num_scopes;
  Scope* cur_scope;

  // This is similar to scopes[0], but is what is available to other modules
  // that import this one.
  ImportedModuleScope* impscope;

  int evaluating_const;

  SqType sq_type_str;
  SqType sq_type_list;
  SqType sq_type_range;
} TranslationUnit;

static TranslationUnit tu;

static void push_exit_call(SqRef func, SqRef obj) {
  ASSERT(tu.cur_scope->num_exit_calls < MAX_EXIT_CALLS);
  tu.cur_scope->exit_call_stack[tu.cur_scope->num_exit_calls++] =
      (ExitCall){.func = func, .obj = obj};
}

static inline FORCE_INLINE bool op_is_const(Operand op) {
  return op.kind == OPK_CONST;
}

static inline FORCE_INLINE bool op_is_lval(Operand op) {
  return op.kind & OPK_BIT_LVAL;
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

#define VEC_NAME OpVec
#define VEC_T Operand
#define VEC_PREFIX opv_
#define VEC_NUM_SHORT 16
#include "vec_impl.h"

static Operand operand_none;

static Operand load_value(ScopeResult scope_result, Sym* sym, Str var_name);
static ScopeResult scope_lookup_single(Scope* scope, Str name, bool crossed_function, Sym** sym);
static ScopeResult scope_lookup_recursive(Str name, Sym** sym);
static Sym* lookup_memfn(Type type, Str name);

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
    return tu.sq_type_str;
  }
  if (type_kind(type) == TYPE_RANGE) {
    return tu.sq_type_range;
  }
  if (type_kind(type) == TYPE_LIST) {
    return tu.sq_type_list;
  }
  if (type_is_aggregate(type)) {
    return type_struct_sqtype(type);
  }
  switch (type_kind(type)) {
    case TYPE_VOID:
      return sq_type_void;
    case TYPE_BOOL:
      return sq_type_ubyte;
    case TYPE_CODEPT:
      return sq_type_word;
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
  return tu.tokbuf.token_offsets[tu.tokbuf.cursor.token_index];
}

static inline uint32_t prev_offset(void) {
  return tu.tokbuf.token_offsets[tu.tokbuf.cursor.token_index - 1];
}

static StrView get_strview_for_offsets(uint32_t from, uint32_t to) {
  return (StrView){(const char*)&tu.tokbuf.file_contents[from], to - from};
}

static void get_location_and_line_slow(uint32_t offset,
                                       uint32_t* loc_line,
                                       uint32_t* loc_column,
                                       StrView* contents) {
  const char* line_start = (const char*)&tu.tokbuf.file_contents[0];
  uint32_t line = 1;
  uint32_t col = 1;
  const char* find = (const char*)&tu.tokbuf.file_contents[offset];
  for (const char* p = (const char*)&tu.tokbuf.file_contents[0];; ++p) {
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

NORETURN static void error_offset_delta(uint32_t offset, int delta, const char* message) {
  uint32_t loc_line;
  uint32_t loc_column;
  StrView line;
  get_location_and_line_slow(offset, &loc_line, &loc_column, &line);
  loc_column += delta;
  int indent = base_writef_stderr("%.*s:%d:%d:", str_len(tu.tokbuf.filename),
                                  str_raw_ptr(tu.tokbuf.filename), loc_line, loc_column);
  base_writef_stderr("%.*s\n", (int)line.size, line.data);
  base_writef_stderr("%*s", indent + loc_column - 1, "");
  base_writef_stderr("^ error: %s\n", message);
  base_exit(1);
}

NORETURN static void error_offset(uint32_t offset, const char* message) {
  error_offset_delta(offset, 0, message);
}

NORETURN static void error(const char* message) {
  error_offset(prev_offset(), message);
}

NORETURN static void error_cur(const char* message) {
  error_offset(cur_offset(), message);
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

NORETURN static void errorf_cur(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  size_t n = 1 + vsnprintf(NULL, 0, fmt, args);
  va_end(args);
  char* str = malloc(n);  // just a simple malloc because we're going to base_exit() momentarily.
  va_start(args, fmt);
  vsnprintf(str, n, fmt, args);
  va_end(args);
  error_cur(str);
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

NORETURN static void errorf_offset_delta(uint32_t offset, int delta, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  size_t n = 1 + vsnprintf(NULL, 0, fmt, args);
  va_end(args);
  char* str = malloc(n);  // just a simple malloc because we're going to base_exit() momentarily.
  va_start(args, fmt);
  vsnprintf(str, n, fmt, args);
  va_end(args);
  error_offset_delta(offset, delta, str);
}

static SqType sqbasetype_from_type(Type type) {
  if (type_size(type) == 8) {
    return sq_type_long;
  }
  return sq_type_word;
}

static Str memfn_name_from_type_name(Str type_name, Str func_name) {
  return str_internf("%.*s$%.*s", str_len(type_name), str_raw_ptr(type_name), str_len(func_name),
                     str_raw_ptr(func_name));
}

static Str memfn_name_from_type(Type type, Str func_name) {
  return memfn_name_from_type_name(type_decl_name(type), func_name);
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

static void copy_bytes(SqRef from, SqRef to, int count) {
  if (count < 32) {
    sq_i_blit(from, to, count);
  } else {
    SqRef memcpy_func = sq_ref_extern("memcpy");
    sq_i_call3(sq_type_void, memcpy_func, (SqCallArg){sq_type_long, to},
               (SqCallArg){sq_type_long, from}, (SqCallArg){sq_type_long, sq_const_int(count)});
  }
}

static void store_by_type_val_into(Type type, SqRef val, SqRef into) {
  if (type_is_aggregate(type)) {
    copy_bytes(val, into, type_size(type));
  } else if (type_kind(type) == TYPE_DOUBLE) {
    sq_i_stored(val, into);
  } else if (type_kind(type) == TYPE_FLOAT) {
    sq_i_stores(val, into);
  } else {
    switch (type_size(type)) {
      case 8:
        sq_i_storel(val, into);
        break;
      case 4:
        sq_i_storew(val, into);
        break;
      case 2:
        sq_i_storeh(val, into);
        break;
      case 1:
        sq_i_storeb(val, into);
        break;
      default:
        errorf("invalid store size %zu", type_size(type));
    }
  }
}

static SqRef load_by_type_from(Type type, SqRef from) {
  SqType resultsize = sqbasetype_from_type(type);
  ASSERT(resultsize.u == sq_type_long.u || resultsize.u == sq_type_word.u);
  if (type_is_aggregate(type)) {
    SqRef into = sq_i_alloc8(sq_const_int(type_size(type)));
    copy_bytes(from, into, type_size(type));
    return into;
  } else if (type_kind(type) == TYPE_BOOL) {
    return sq_i_loadub(resultsize, from);
  } else if (type_kind(type) == TYPE_CODEPT) {
    return sq_i_load(sq_type_word, from);
  } else if (type_kind(type) == TYPE_PTR) {
    return sq_i_load(resultsize, from);
  } else if (type_kind(type) == TYPE_DOUBLE) {
    return sq_i_load(sq_type_double, from);
  } else if (type_kind(type) == TYPE_FLOAT) {
    return sq_i_load(sq_type_single, from);
  } else if (type_is_unsigned(type)) {
    switch (type_size(type)) {
      case 8:
        return sq_i_load(resultsize, from);
      case 4:
        return sq_i_loaduw(resultsize, from);
      case 2:
        return sq_i_loaduh(resultsize, from);
      case 1:
        return sq_i_loadub(resultsize, from);
      default:
        errorf("invalid unsigned load size %zu", type_size(type));
    }
  } else {
    switch (type_size(type)) {
      case 8:
        return sq_i_load(resultsize, from);
      case 4:
        return sq_i_loadsw(resultsize, from);
      case 2:
        return sq_i_loadsh(resultsize, from);
      case 1:
        return sq_i_loadsb(resultsize, from);
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
        case TYPE_FLOAT:
          return sq_const_single(op->val.f);
        case TYPE_DOUBLE:
          return sq_const_double(op->val.d);
        case TYPE_PTR:
          return sq_const_int((int64_t)(uint64_t)op->val.p);
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
        error("internal error: cannot turn local aggregate into immediate.");
      }
      return load_by_type_from(op->type, op->ref);
    }
    case OPK_REF_RVAL_GLOBAL_ADDR:
    case OPK_REF_LVAL_GLOBAL_ADDR:
      if (type_is_aggregate(op->type)) {
        error("internal error: cannot turn global aggregate into immediate.");
      }
      return load_by_type_from(op->type, op->ref);
    default:
      error("internal error: unhandled OpKind");
  }
}

static SqRef operand_to_sqref_lval(Operand* op) {
  switch (op->kind) {
    case OPK_CONST: {
      SqRef tmp = sq_i_alloc8(sq_const_int(type_size(op->type)));
      store_by_type_val_into(op->type, sq_const_int(op->val.u64), tmp);
      return tmp;
    }
    case OPK_REF_RVAL: {
      SqRef tmp = sq_i_alloc8(sq_const_int(type_size(op->type)));
      store_by_type_val_into(op->type, op->ref, tmp);
      return tmp;
    }
    case OPK_REF_RVAL_LOCAL_ADDR_BOUND_FUNC:  // assume something else will load ref2
    case OPK_REF_LVAL_LOCAL_ADDR:
    case OPK_REF_RVAL_LOCAL_ADDR:
    case OPK_REF_RVAL_GLOBAL_ADDR:
    case OPK_REF_LVAL_GLOBAL_ADDR:
      return op->ref;
    default:
      error("internal error: unhandled OpKind");
  }
}

static void copy_by_type(Operand* from, SqRef into) {
  if (type_is_aggregate(from->type)) {
    copy_bytes(from->ref, into, type_size(from->type));
  } else {
    store_by_type_val_into(from->type, operand_to_sqref_imm(from), into);
  }
}

static SqRef sqref_for_sym(Sym* sym) {
  if (type_kind(sym->type) == TYPE_FUNC && type_func_flags(sym->type) & TFF_FOREIGN) {
    return sq_ref_extern(cstr_copy(glob.arena, sym->name));
  } else {
    return sq_ref_for_symbol(sym->global);
  }
}

typedef struct NameSymPair {
  Str name;
  Sym sym;
} NameSymPair;

#if 0
static void dump_sym(Sym* sym) {
  switch (sym->kind) {
    case SYM_NONE:
      printf("NONE");
      break;
    case SYM_VAR:
      printf("VAR");
      break;
    case SYM_CONST:
      printf("CONST");
      break;
    case SYM_FUNC:
      printf("FUNC");
      break;
    case SYM_TYPE:
      printf("TYPE");
      break;
    case SYM_MODULE:
      printf("MODULE");
      break;
    default:
      printf("???");
      break;
  }
  printf("\n");
}

static void dump_scope(Scope* scope) {
  if (scope->is_module) {
    if (scope->is_full_dict) {
      DictRawIter iter = dict_iter(&scope->sym_dict, sizeof(NameSymPair));
      printf("%zu entries, full map:\n", scope->sym_dict.size);
      int i = 0;
      NameSymPair* nsp = dict_rawiter_get(&iter);
      for (;;) {
        Str key = nsp->name;
        printf("  %d: %.*s: ", i++, (int)str_len(key), str_raw_ptr(key));
        dump_sym(&nsp->sym);
        nsp = dict_rawiter_next(&iter, sizeof(NameSymPair));
        if (!nsp) {
          break;
        }
      }
    } else {
      SmallFlatNameSymMap* nm = &scope->flat_map;
      int count = nm->num_entries;
      printf("%d entries (small flat map):\n", count);
      for (int i = 0; i < count; ++i) {
        printf("  %d: %.*s: ", i, (int)str_len(nm->names[i]), str_raw_ptr(nm->names[i]));
        dump_sym(&nm->syms[i]);
      }
    }
  } else {
    printf("todo; non-module dump\n");
  }
}
#endif

// Returns pointer into dict where Sym is stored by value, probably bad idea.
static Sym* sym_new(SymKind kind, Str name, Type type) {
  ASSERT(tu.cur_scope);
  ASSERT(!str_is_none(name));
  if (tu.cur_scope->is_full_dict) {
    NameSymPair nsp = {.name = name,
                      .sym = {
                          .kind = kind,
                          .name = name,
                          .type = type,
                      }};
    DictInsert res = dict_insert(&tu.cur_scope->sym_dict, &nsp, start_str_hash_func,
                                start_str_eq_func, sizeof(NameSymPair), _Alignof(NameSymPair));
    return &((NameSymPair*)dict_rawiter_get(&res.iter))->sym;
  } else {
    SmallFlatNameSymMap* nm = &tu.cur_scope->flat_map;
    int count = nm->num_entries;

    if (count == COUNTOFI(nm->names)) {
      // flat_map is full, 'rehash' into full dict

      // Can't immediately put into cur_scope because the flat_map and
      // dict_sym are a union.
      ASSERT(!tu.cur_scope->is_module);
      DictImpl new_dict = dict_new(tu.var_scope_arena, COUNTOFI(nm->names) * 4,
                                   sizeof(NameSymPair), _Alignof(NameSymPair));
      for (int i = 0; i < count; ++i) {
        NameSymPair nsp = {.name = nm->names[i], .sym = nm->syms[i]};
        dict_insert(&new_dict, &nsp, start_str_hash_func, start_str_eq_func, sizeof(NameSymPair),
                    _Alignof(NameSymPair));
      }

      // Now flat_map is dead, overrwrite with the dict and update the bool to
      // indicate we have a full dict.
      tu.cur_scope->is_full_dict = true;
      tu.cur_scope->sym_dict = new_dict;

      // Call the other branch to actually insert the new sym.
      return sym_new(kind, name, type);
    }

    nm->names[nm->num_entries] = name;
    nm->syms[nm->num_entries] = (Sym){.kind = kind, .name = name, .type = type};
    Sym* ret = &nm->syms[nm->num_entries++];
    return ret;
  }
}

typedef struct NameTypeSymP {
  Str name;
  Type type;
  Sym* sym;
} NameTypeSymP;

static size_t nametypesymp_hash_func(void* p) {
  NameTypeSymP* ntsp = (NameTypeSymP*)p;
  size_t hash = 0;
  dict_hash_write(&hash, (void*)str_raw_ptr(ntsp->name), str_len(ntsp->name));
  // Not the best hash for Type, but I think it's valid.
  dict_hash_write(&hash, (void*)&ntsp->type, sizeof(Type));
  return hash;
}

static bool nametypesymp_eq_func(void* void_a, void* void_b) {
  NameTypeSymP* ntsp_a = (NameTypeSymP*)void_a;
  NameTypeSymP* ntsp_b = (NameTypeSymP*)void_b;
  if (!str_eq(ntsp_a->name, ntsp_b->name)) {
    return false;
  }
  return type_eq(ntsp_a->type, ntsp_b->type);
}

// TODO: type_as_str being used for real work, not errors here, and shouldn't be
// because it's expensive. should at least be doing a type_as_str_into to build
// the name so that all the intermediates aren't unnecessarily getting intern'd,
// etc.

static Sym* gen_array___str__(Type type) {
  Type subtype = type_array_subtype(type);
  size_t count = type_array_count(type);
  Str full_name = memfn_name_from_type_name(
      str_internf("Array_%s_%lu", type_as_str(subtype), count), glob.static_str___str__);

  sq_func_start(sq_linkage_default, tu.sq_type_str, cstr_copy(glob.arena, full_name));

  SqRef self = sq_func_param(sq_type_long);

  uint64_t subtype_size = type_size(subtype);

  Sym* sub_str_func = lookup_memfn(subtype, glob.static_str___str__);

  SqRef ret = sq_i_call4(
      tu.sq_type_str, sq_ref_extern("Array$__str__"), (SqCallArg){sq_type_long, self},
      (SqCallArg){sq_type_long, sq_const_int(count)},
      (SqCallArg){sq_type_long, sq_const_int(subtype_size)},
      (SqCallArg){sq_type_long, sub_str_func ? sqref_for_sym(sub_str_func) : sq_const_int(0)});
  sq_i_ret(ret);
  SqSymbol str_func = sq_func_end();

  Type param_types[] = {type_ptr(type_array(subtype, count))};
  Type functype = type_function(param_types, COUNTOF(param_types), type_str, TFF_MEMFN);

  Sym* funcsym = sym_new(SYM_FUNC, full_name, functype);
  funcsym->global = str_func;
  funcsym->scope_decl = SSD_DECLARED_GLOBAL;

  sq_itemctx_activate(tu.cur_scope->func_item_ctx);
  return funcsym;
}

// on [N]T def bool __contains__(self, T item):
//   tmp = item
//   return Array$__contains__(self, N, &tmp, sizeof(T), &T::__eq__)
static Sym* gen_array___contains__(Type type) {
  Type subtype = type_array_subtype(type);
  size_t count = type_array_count(type);
  Str full_name = memfn_name_from_type_name(
      str_internf("Array_%s_%lu", type_as_str(subtype), count), glob.static_str___contains__);

  sq_func_start(sq_linkage_default, sq_type_ubyte, cstr_copy(glob.arena, full_name));

  SqRef self = sq_func_param(sq_type_long);

  SqType sqsubtype = type_to_sqtype(subtype);
  SqRef item = sq_func_param(sqsubtype);

  uint64_t subtype_size = type_size(subtype);

  Sym* sub_eq_func = lookup_memfn(subtype, glob.static_str___eq__);

  // This is needed to pass the address, but also accomplishes sign extension if
  // e.g. -4i32 is passed to an i64 method.
  SqRef tmp = sq_i_alloc8(sq_const_int(subtype_size));
  store_by_type_val_into(subtype, item, tmp);

  SqRef ret = sq_i_call5(
      sq_type_ubyte, sq_ref_extern("Array$__contains__"), (SqCallArg){sq_type_long, self},
      (SqCallArg){sq_type_long, sq_const_int(count)}, (SqCallArg){sq_type_long, tmp},
      (SqCallArg){sq_type_long, sq_const_int(subtype_size)},
      (SqCallArg){sq_type_long, sub_eq_func ? sqref_for_sym(sub_eq_func) : sq_const_int(0)});
  sq_i_ret(ret);
  SqSymbol contains_func = sq_func_end();

  Type param_types[] = {type_ptr(type_array(subtype, count)), subtype};
  Type functype = type_function(param_types, COUNTOF(param_types), type_bool, TFF_MEMFN);

  Sym* funcsym = sym_new(SYM_FUNC, full_name, functype);
  funcsym->global = contains_func;
  funcsym->scope_decl = SSD_DECLARED_GLOBAL;

  sq_itemctx_activate(tu.cur_scope->func_item_ctx);
  return funcsym;
}

// on []T def append(self, T item):
//     tmp = item
//     List$append(self, &tmp, sizeof(T))
static Sym* gen_list_append(Type type) {
  Type subtype = type_list_subtype(type);
  Str full_name = memfn_name_from_type_name(str_internf("List_%s", type_as_str(subtype)),
                                            str_intern_len("append", 6));

  sq_func_start(sq_linkage_default, sq_type_void, cstr_copy(glob.arena, full_name));

  SqRef self = sq_func_param(sq_type_long);

  SqType sqsubtype = type_to_sqtype(subtype);
  SqRef item = sq_func_param(sqsubtype);

  uint64_t subtype_size = type_size(subtype);

  // This is needed to pass the address, but also accomplishes sign extension if
  // e.g. -4i32 is passed to an i64 method.
  SqRef tmp = sq_i_alloc8(sq_const_int(subtype_size));
  store_by_type_val_into(subtype, item, tmp);

  sq_i_call3(sq_type_void, sq_ref_extern("List$append"), (SqCallArg){sq_type_long, self},
             (SqCallArg){sq_type_long, tmp}, (SqCallArg){sq_type_long, sq_const_int(subtype_size)});
  sq_i_ret_void();
  SqSymbol append_func = sq_func_end();

  Type param_types[] = { type_ptr(type_list(subtype)), subtype };
  Type functype = type_function(param_types, COUNTOF(param_types), type_void, TFF_MEMFN);

  Sym* funcsym = sym_new(SYM_FUNC, full_name, functype);
  funcsym->global = append_func;
  funcsym->scope_decl = SSD_DECLARED_GLOBAL;

  sq_itemctx_activate(tu.cur_scope->func_item_ctx);

  return funcsym;
}

// on []T def __contains__(self, T item):
//     tmp = item
//     List$__contains__(self, &tmp, sizeof(T), &T::__eq__)
static Sym* gen_list___contains__(Type type) {
  Type subtype = type_list_subtype(type);
  Str full_name = memfn_name_from_type_name(str_internf("List_%s", type_as_str(subtype)),
                                            glob.static_str___contains__);

  sq_func_start(sq_linkage_default, sq_type_void, cstr_copy(glob.arena, full_name));

  SqRef self = sq_func_param(sq_type_long);

  SqType sqsubtype = type_to_sqtype(subtype);
  SqRef item = sq_func_param(sqsubtype);

  uint64_t subtype_size = type_size(subtype);

  // This is needed to pass the address, but also accomplishes sign extension if
  // e.g. -4i32 is passed to an i64 method.
  SqRef tmp;
  if (type_is_aggregate(subtype)) {
    tmp = item;
  } else {
    tmp = sq_i_alloc8(sq_const_int(subtype_size));
    store_by_type_val_into(subtype, item, tmp);
  }

  Sym* sub_eq_func = lookup_memfn(subtype, glob.static_str___eq__);

  sq_i_call4(
      sq_type_void, sq_ref_extern("List$__contains__"), (SqCallArg){sq_type_long, self},
      (SqCallArg){sq_type_long, tmp}, (SqCallArg){sq_type_long, sq_const_int(subtype_size)},
      (SqCallArg){sq_type_long, sub_eq_func ? sqref_for_sym(sub_eq_func) : sq_const_int(0)});
  sq_i_ret_void();
  SqSymbol contains_func = sq_func_end();

  Type param_types[] = { type_ptr(type_list(subtype)), subtype };
  Type functype = type_function(param_types, COUNTOF(param_types), type_void, TFF_MEMFN);

  Sym* funcsym = sym_new(SYM_FUNC, full_name, functype);
  funcsym->global = contains_func;
  funcsym->scope_decl = SSD_DECLARED_GLOBAL;

  sq_itemctx_activate(tu.cur_scope->func_item_ctx);

  return funcsym;
}

// on []T def str __str__(self):
//     List$__str__(self, sizeof(T), &T::__str__)
static Sym* gen_list___str__(Type type) {
  Type subtype = type_list_subtype(type);
  Str full_name = memfn_name_from_type_name(str_internf("List_%s", type_as_str(subtype)),
                                            glob.static_str___str__);

  sq_func_start(sq_linkage_default, tu.sq_type_str, cstr_copy(glob.arena, full_name));

  SqRef self = sq_func_param(sq_type_long);

  uint64_t subtype_size = type_size(subtype);

  Sym* sub_str_func = lookup_memfn(subtype, glob.static_str___str__);

  SqRef ret = sq_i_call3(
      tu.sq_type_str, sq_ref_extern("List$__str__"), (SqCallArg){sq_type_long, self},
      (SqCallArg){sq_type_long, sq_const_int(subtype_size)},
      (SqCallArg){sq_type_long, sub_str_func ? sqref_for_sym(sub_str_func) : sq_const_int(0)});
  sq_i_ret(ret);
  SqSymbol str_func = sq_func_end();

  Type param_types[] = {type_ptr(type_list(subtype))};
  Type functype = type_function(param_types, COUNTOF(param_types), type_str, TFF_MEMFN);

  Sym* funcsym = sym_new(SYM_FUNC, full_name, functype);
  funcsym->global = str_func;
  funcsym->scope_decl = SSD_DECLARED_GLOBAL;

  sq_itemctx_activate(tu.cur_scope->func_item_ctx);
  return funcsym;
}

typedef struct GenericThunkCreators {
  const char* name;
  Sym* (*ensure_gen_thunk)(Type);
} GenericThunkCreators;

static GenericThunkCreators generic_array_functions[] = {
    {"__contains__", gen_array___contains__},
    {"__str__", gen_array___str__},
};

static GenericThunkCreators generic_list_functions[] = {
    {"__contains__", gen_list___contains__},
    {"__str__", gen_list___str__},
    {"append", gen_list_append},
};

static Sym* lookup_memfn(Type type, Str name) {
  TypeKind kind = type_kind(type);
  switch (kind) {
    case TYPE_ARRAY:
    case TYPE_LIST:
    case TYPE_DICT: {
      NameTypeSymP ntsp_lookup = {name, type, NULL};
      DictRawIter iter =
          dict_find(&glob.generics_thunk_cache, &ntsp_lookup, nametypesymp_hash_func,
                    nametypesymp_eq_func, sizeof(NameTypeSymP));
      NameTypeSymP* nstp = (NameTypeSymP*)dict_rawiter_get(&iter);
      if (nstp) {
        return nstp->sym;
      }

      Sym* new_func = NULL;
      switch (kind) {
        case TYPE_ARRAY:
          for (int i = 0; i < COUNTOFI(generic_array_functions); ++i) {
            if (strncmp(generic_array_functions[i].name, str_raw_ptr(name), str_len(name)) == 0) {
              new_func = generic_array_functions[i].ensure_gen_thunk(type);
              break;
            }
          }
          break;
        case TYPE_LIST:
          for (int i = 0; i < COUNTOFI(generic_list_functions); ++i) {
            if (strncmp(generic_list_functions[i].name, str_raw_ptr(name), str_len(name)) == 0) {
              new_func = generic_list_functions[i].ensure_gen_thunk(type);
              break;
            }
          }
          break;
        case TYPE_DICT:
          error("TODO: polymorphic dict memfns");
          break;
        default:
          error("internal error");
      }

      NameTypeSymP ntsp_insert = {name, type, new_func};
      DictInsert res =
          dict_insert(&glob.generics_thunk_cache, &ntsp_insert, nametypesymp_hash_func,
                      nametypesymp_eq_func, sizeof(NameTypeSymP), _Alignof(NameTypeSymP));
      ASSERT(res.inserted);
      return new_func;
    }

    default:
      // non-polymorphic below
      break;
  }

  Str memfn_name = memfn_name_from_type(type, name);
  Sym* sym;
  // TODO: this probably doesn't need to be fully recursive, just look at globals?
  ScopeResult scope_result = scope_lookup_recursive(memfn_name, &sym);
  if (scope_result == SCOPE_RESULT_UNDEFINED) {
    return NULL;
  } else if (scope_result == SCOPE_RESULT_GLOBAL && sym->kind == SYM_FUNC) {
    return sym;
  } else {
    error("internal error: lookup_memfn");
  }
}

static void initialize_aggregate(SqRef base_addr, Type type) {
  size_t size = type_size(type);
  if (type_kind(type) == TYPE_STRUCT && type_struct_has_initializer(type)) {
    SqSymbol init_sym = type_struct_initializer_sym(type);
    copy_bytes(sq_ref_for_symbol(init_sym), base_addr, size);
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
    ASSERT(type_size(type) == 16);
    if (initial_value) {
      SqRef init = operand_to_sqref_lval(initial_value);
      copy_bytes(init, new->ref, type_size(type));
    } else {
      sq_i_storel(sq_const_int(0), new->ref);
      sq_i_storel(sq_const_int(0), sq_i_add(sq_type_long, new->ref, sq_const_int(8)));
    }
  } else if (type_kind(type) == TYPE_RANGE) {
    ASSERT(initial_value);
    ASSERT(type_size(type) == 24);
    new->ref = sq_i_alloc8(sq_const_int(type_size(type)));
    SqRef init = operand_to_sqref_lval(initial_value);
    copy_bytes(init, new->ref, type_size(type));
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
    if (initial_value) {
      store_by_type_val_into(type, operand_to_sqref_imm(initial_value), new->ref);
    } else {
      store_by_type_val_into(type, sq_const_int(0), new->ref);
    }
  }
  new->scope_decl = SSD_DECLARED_LOCAL;
  return new;
}

static Sym* make_global(SymKind kind, Str name, Type type, Val* initial_value) {
  Sym* new = sym_new(kind, name, type);
  sq_data_start(sq_linkage_default, cstr_copy(glob.arena, name));
  if (initial_value) {
    switch (type_kind(type)) {
      case TYPE_BOOL:
        sq_data_byte((uint8_t)initial_value->b);
        break;
      case TYPE_U8:
        sq_data_byte(initial_value->u8);
        break;
      case TYPE_I8:
        sq_data_byte((uint8_t)initial_value->i8);
        break;
      case TYPE_U16:
        sq_data_half(initial_value->u16);
        break;
      case TYPE_I16:
        sq_data_half((uint16_t)initial_value->i16);
        break;
      case TYPE_U32:
        sq_data_word(initial_value->u32);
        break;
      case TYPE_I32:
        sq_data_word((uint32_t)initial_value->i32);
        break;
      case TYPE_U64:
        sq_data_long(initial_value->u64);
        break;
      case TYPE_I64:
        sq_data_long((uint64_t)initial_value->i64);
        break;
      default:
        error("internal error: unexpected global const init.");
    }
  } else {
    for (size_t i = 0; i < type_size(type); ++i) {
      sq_data_byte(0);
    }
  }
  new->global = sq_data_end();
  new->scope_decl = SSD_DECLARED_GLOBAL;

  if (tu.cur_scope->is_function) {
    sq_itemctx_activate(tu.cur_scope->func_item_ctx);
  }

  return new;
}

static Sym* make_param(Str name, Type type, int index) {
  Sym* new = sym_new(SYM_VAR, name, type);
  // Parameters are values, not variables.
  new->ref = sq_func_param_named(type_to_sqtype(type),
#if BUILD_DEBUG
                                 cstr_copy(glob.arena, name)
#else
                                 NULL
#endif
  );
  new->scope_decl = SSD_DECLARED_PARAMETER;
  return new;
}

static void enter_function_scope(Sym* funcsym) {
  tu.cur_scope = &tu.scopes[tu.num_scopes++];
  tu.cur_scope->func_sym = funcsym;
  tu.cur_scope->num_exit_calls = 0;
  tu.cur_scope->num_iteration_datas = 0;
  tu.cur_scope->upval_map.num_upvals = 0;
  tu.cur_scope->arena_pos = arena_pos(tu.var_scope_arena);
  tu.cur_scope->is_function = true;
  tu.cur_scope->is_module = false;
  tu.cur_scope->is_full_dict = false;
  flat_name_map_init(&tu.cur_scope->flat_map);
}

static void enter_module_scope(void) {
  tu.impscope = arena_push(glob.arena, sizeof(ImportedModuleScope), _Alignof(ImportedModuleScope));
  tu.impscope->syms = dict_new(glob.arena, 1 << 8, sizeof(ImportedSymbol), _Alignof(ImportedSymbol));

  ASSERT(tu.num_scopes == 0);
  tu.cur_scope = &tu.scopes[tu.num_scopes++];
  memset(tu.cur_scope, 0, sizeof(Scope));
  tu.cur_scope->arena_pos = arena_pos(tu.var_scope_arena);
  tu.cur_scope->is_module = true;
  tu.cur_scope->is_full_dict = true;
  tu.cur_scope->sym_dict =
      dict_new(tu.var_scope_arena, 1 << 10, sizeof(NameSymPair), _Alignof(NameSymPair));
}

static void leave_scope(void) {
  arena_pop_to(tu.var_scope_arena, tu.cur_scope->arena_pos);
  --tu.num_scopes;
  ASSERT(tu.num_scopes >= 0);
  if (tu.num_scopes == 0) {
    tu.cur_scope = NULL;
  } else {
    tu.cur_scope = &tu.scopes[tu.num_scopes - 1];
  }
}

static void enter_function(Sym* sym,
                           Str param_names[MAX_FUNC_PARAMS],
                           Type param_types[MAX_FUNC_PARAMS]) {
  bool is_nested = tu.num_scopes > 1;  // Module, parent.
  if (is_nested) {
    ASSERT(tu.scopes[tu.num_scopes - 1].is_function);
    ASSERT(tu.scopes[0].is_module);
  }

  enter_function_scope(sym);

  // TODO: need to figure out what we want to do here
  //SqLinkage linkage =
      //str_eq(sym->name, glob.static_str_main) ? sq_linkage_export : sq_linkage_default;
  SqLinkage linkage = sq_linkage_export;

  Type ret_type = type_func_return_type(sym->type);

  tu.cur_scope->func_item_ctx =
      sq_func_start(linkage, type_to_sqtype(ret_type), cstr_copy(glob.arena, sym->name));

  uint32_t num_params = type_func_num_params(sym->type);
  Sym* param_syms[MAX_FUNC_PARAMS];
  for (uint32_t i = 0; i < num_params; ++i) {
    param_syms[i] = make_param(param_names[i], type_func_param(sym->type, i), i);
  }

  if (is_nested) {
    ASSERT(str_eq(param_syms[0]->name, glob.static_str_up));
    ASSERT(type_kind(param_syms[0]->type) == TYPE_PTR);
    ASSERT(type_eq(type_ptr_subtype(param_syms[0]->type), type_void));
    tu.cur_scope->upval_base = param_syms[0]->ref;
  }
}

static void leave_function(void) {
  Type ret_type = type_func_return_type(tu.cur_scope->func_sym->type);
  if (type_eq(ret_type, type_void)) {
    sq_i_ret_void();
  } else {
    // TODO: this is ugly. in the case that there's
    //   if blah:
    //     ...
    //     return 1
    // a new block is started by parse_block() when the child block (that does
    // 'return 1' is parsed, otherwise, the higher level that's doing a jmp to
    // the next block of work of the 'if' will cause a sqbe error because it's
    // ret-then-jmp. but that also means if there's always an open unused block
    // at the end after a final return. so we just return 0 here to make sqbe
    // happy otherwise sqbe errors that the final block misses a jmp. but this
    // always completely dead code so it doesn't matter either way. Make it a
    // weird constant instead of 0 just to ensure we're not accidentally using
    // it!
    sq_i_ret(sq_const_int(0xbad));
  }

  tu.cur_scope->func_sym->global = sq_func_end();
  tu.cur_scope->func_item_ctx = (SqItemCtx){0};

  bool is_nested = tu.num_scopes > 2;  // Module, parent function, current function.
  if (is_nested) {
    ASSERT(tu.scopes[tu.num_scopes - 1].is_function);
    ASSERT(tu.scopes[tu.num_scopes - 2].is_function);
    ASSERT(tu.scopes[0].is_module);
  }

  if (is_nested) {
    // This is pointing to the nested one, but we have to set cur_scope to the
    // parent one, so that codegen goes to it.
    UpvalMap* inner_uvm = &tu.cur_scope->upval_map;
    Sym* child_func = tu.cur_scope->func_sym;
    tu.cur_scope = &tu.scopes[tu.num_scopes - 2];
    UpvalMap* parent_uvm = &tu.cur_scope->upval_map;
    sq_itemctx_activate(tu.cur_scope->func_item_ctx);

    SqRef upval_data = sq_i_alloc8(sq_const_int(inner_uvm->alloc_size));
    child_func->ref2 = upval_data;

    for (int i = 0; i < inner_uvm->num_upvals; ++i) {
      Upval* uv = &inner_uvm->upvals[i];
      switch (uv->scope_result) {
        case SCOPE_RESULT_GLOBAL:
        case SCOPE_RESULT_UNDEFINED:
          error("internal error, unexpected scope_result in upval capture");
        case SCOPE_RESULT_LOCAL: {
          SqRef val = load_by_type_from(uv->type, uv->ref);
          store_by_type_val_into(uv->type, val,
                                 sq_i_add(sq_type_long, upval_data, sq_const_int(uv->offset)));
          break;
        }
        case SCOPE_RESULT_PARAMETER: {
          store_by_type_val_into(uv->type, uv->ref,
                                 sq_i_add(sq_type_long, upval_data, sq_const_int(uv->offset)));
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
                                 cstr_copy(glob.arena, parent_uv->name),
                                 cstr_copy(glob.arena, tu.cur_scope->func_sym->name),
                                 cstr_copy(glob.arena, uv->name),
                                 cstr_copy(glob.arena, child_func->name));
                                 */
              ASSERT(tu.cur_scope->upval_base.u);
              SqRef val =
                  load_by_type_from(uv->type, sq_i_add(sq_type_long, tu.cur_scope->upval_base,
                                                       sq_const_int(parent_uv->offset)));
              store_by_type_val_into(uv->type, val,
                                     sq_i_add(sq_type_long, upval_data, sq_const_int(uv->offset)));
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
  tu.tokbuf.cursor.prev_kind = tu.tokbuf.cursor.cur_kind;
  if (tu.tokbuf.num_peeks > 0) {
    tu.tokbuf.cursor.cur_kind = tu.tokbuf.token_peeks[--tu.tokbuf.num_peeks].kind;
    tu.tokbuf.cursor.token_index = tu.tokbuf.token_peeks[tu.tokbuf.num_peeks].index;
#if BUILD_DEBUG
    if (glob.verbose > 2) {
      base_writef_stderr("token %s (buffered) index=%d\n",
                         token_enum_name(tu.tokbuf.cursor.cur_kind), tu.tokbuf.cursor.token_index);
    }
#endif
    return;
  } else {
    ++tu.tokbuf.cursor.token_index;
    ASSERT(tu.tokbuf.cursor.token_index < tu.tokbuf.num_tokens);
    tu.tokbuf.cursor.cur_kind =
        token_categorize(tu.tokbuf.token_offsets[tu.tokbuf.cursor.token_index],
                         tu.tokbuf.token_offsets[tu.tokbuf.cursor.token_index + 1]);
  }

  if (tu.tokbuf.cursor.cur_kind == TOK_NL) {
    goto again;
  }
  if (tu.tokbuf.cursor.cur_kind == TOK_NEWLINE_BLANK) {
    tu.tokbuf.cursor.cur_kind = TOK_NEWLINE;
  } else if (tu.tokbuf.cursor.cur_kind >= TOK_NEWLINE_INDENT_0 && tu.tokbuf.cursor.cur_kind <= TOK_NEWLINE_INDENT_40) {
    int n = (tu.tokbuf.cursor.cur_kind - TOK_NEWLINE_INDENT_0) * 4;
    if (n > tu.tokbuf.indent_levels[tu.tokbuf.num_indents - 1]) {
      tu.tokbuf.cursor.cur_kind = TOK_NEWLINE;
      tu.tokbuf.indent_levels[tu.tokbuf.num_indents++] = n;
      tu.tokbuf.token_peeks[tu.tokbuf.num_peeks++] =
          (PeekToken){TOK_INDENT, tu.tokbuf.cursor.token_index};
    } else if (n < tu.tokbuf.indent_levels[tu.tokbuf.num_indents - 1]) {
      tu.tokbuf.cursor.cur_kind = TOK_NEWLINE;
      while (tu.tokbuf.num_indents > 1 && tu.tokbuf.indent_levels[tu.tokbuf.num_indents - 1] > n) {
        tu.tokbuf.token_peeks[tu.tokbuf.num_peeks++] =
            (PeekToken){TOK_DEDENT, tu.tokbuf.cursor.token_index};
        --tu.tokbuf.num_indents;
      }
    } else {
      tu.tokbuf.cursor.cur_kind = TOK_NEWLINE;
    }
  }

#if BUILD_DEBUG
  if (glob.verbose > 2) {
    base_writef_stderr("token %s index=%d\n", token_enum_name(tu.tokbuf.cursor.cur_kind),
                       tu.tokbuf.cursor.token_index);
  }
#endif
}

static bool match(TokenKind tok_kind) {
  if (tu.tokbuf.cursor.cur_kind != tok_kind) {
    return false;
  }
  advance();
  return true;
}

static bool check(TokenKind tok_kind) {
  return tu.tokbuf.cursor.cur_kind == tok_kind;
}

static bool peek(TokenKind tok_kind) {
  TokenCursor old = tu.tokbuf.cursor;
  advance();

  bool result = tu.tokbuf.cursor.cur_kind == tok_kind;

  // semi-retreat, but keep categorization by buffering it.
  tu.tokbuf.token_peeks[tu.tokbuf.num_peeks++] =
      (PeekToken){tu.tokbuf.cursor.cur_kind, tu.tokbuf.cursor.token_index};
  tu.tokbuf.cursor = old;

  return result;
}

static bool peek2(TokenKind tok_kind1, TokenKind tok_kind2) {
  TokenCursor old = tu.tokbuf.cursor;

  advance();
  bool result = tu.tokbuf.cursor.cur_kind == tok_kind1;
  PeekToken first = {tu.tokbuf.cursor.cur_kind, tu.tokbuf.cursor.token_index};

  advance();
  result = result && tu.tokbuf.cursor.cur_kind == tok_kind2;

  tu.tokbuf.token_peeks[tu.tokbuf.num_peeks++] =
      (PeekToken){tu.tokbuf.cursor.cur_kind, tu.tokbuf.cursor.token_index};
  tu.tokbuf.token_peeks[tu.tokbuf.num_peeks++] = first;

  tu.tokbuf.cursor = old;

  return result;
}

static void consume(TokenKind tok_kind, const char* message) {
  if (tu.tokbuf.cursor.cur_kind == tok_kind) {
    advance();
    return;
  }
  error_offset(cur_offset(), message);
}

static void consumef(TokenKind tok_kind, const char* fmt, ...) {
  if (tu.tokbuf.cursor.cur_kind == tok_kind) {
    advance();
    return;
  }

  va_list args;
  va_start(args, fmt);
  size_t n = 1 + vsnprintf(NULL, 0, fmt, args);
  va_end(args);
  char* str = malloc(n);  // just a simple malloc because we're going to base_exit() momentarily.
  va_start(args, fmt);
  vsnprintf(str, n, fmt, args);
  va_end(args);
  error_offset(cur_offset(), str);
}

static Str gensym_var_name(void) {
  ++glob.uniq_counter;
  return str_internf("tmp_%d", glob.uniq_counter);
}

static SqRef emit_string_obj(StrView str) {
  ++glob.uniq_counter;

  sq_data_start(sq_linkage_default,
                cstr_copy(glob.arena, str_internf("strdat_%d", glob.uniq_counter)));
  for (uint32_t i = 0; i < str.size; ++i) {
    sq_data_byte(str.data[i]);
  }
  sq_data_byte(0);
  SqSymbol string_data = sq_data_end();

  sq_data_start(sq_linkage_default,
                cstr_copy(glob.arena, str_internf("strobj_%d", glob.uniq_counter)));
  sq_data_ref(string_data, 0);
  sq_data_long(str.size);
  SqSymbol string_obj = sq_data_end();

  sq_itemctx_activate(tu.cur_scope->func_item_ctx);
  return sq_ref_for_symbol(string_obj);
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
  ++tu.evaluating_const;
  Operand expr = parse_expression(&type_u64);
  if (!op_is_const(expr)) {
    error("Expected constant expression.");
  }
  --tu.evaluating_const;
  return expr;
}

static Type basic_tok_to_type[NUM_TOKEN_KINDS] = {
    [TOK_BOOL] = type_bool,        //
    [TOK_BYTE] = type_u8,          //
    [TOK_CODEPT] = type_i32,       //
    [TOK_DOUBLE] = type_double,    //
    [TOK_F32] = type_float,        //
    [TOK_F64] = type_double,       //
    [TOK_FLOAT] = type_float,      //
    [TOK_I16] = type_i16,          //
    [TOK_I32] = type_i32,          //
    [TOK_I64] = type_i64,          //
    [TOK_I8] = type_i8,            //
    [TOK_INT] = type_i32,          //
    [TOK_STR] = type_str,          //
    [TOK_U16] = type_u16,          //
    [TOK_U32] = type_u32,          //
    [TOK_U64] = type_u64,          //
    [TOK_U8] = type_u8,            //
    [TOK_UINT] = type_u32,         //
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
  } else if (type_is_arithmetic(dest) && type_is_arithmetic(src)) {
    // TODO: This would make sense, but have to have small things work
    // automatically somehow, e.g.
    //   u64 u = 123
    // wouldn't compile because 123 is i32 without an explicit suffix, which is
    // annoying -- should be able to have the RHS know what it's expecting to be
    // and return the right type if it fits?
    //&& type_rank(dest) >= type_rank(src) && type_signs_match(dest, src)) {
    return true;
  } else if (type_eq(src, type_codept) && type_eq(dest, type_str)) {
    return true;
  } else if (memcmp(operand, &glob.op_null_ptr, sizeof(Operand)) == 0) {
    return true;
  // TODO: various pointer, etc.
  } else {
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

    if (type_eq(operand->type, type_codept) && type_eq(type, type_str)) {
      // hacky codept to str conversion, maybe should require this in code
      // rather than making automatic. mostly for `ch in "abc"`.
      Sym* sym = lookup_memfn(type_codept, glob.static_str___str__);
      ASSERT(sym);
      *operand = operand_rvalue_imm(
          type_str, sq_i_call1(tu.sq_type_str, sqref_for_sym(sym),
                               (SqCallArg){sq_type_long, operand_to_sqref_lval(operand)}));
    } else if (op_is_const(*operand)) {
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

static ImportedSymbol* look_up_imported_symbol(Str package_name,
                                             Str sym_name,
                                             const char* expected_kind) {
  ImportNameAndModule inam = {package_name, {0}};
  DictRawIter iter = dict_find(tu.tokbuf.import_dict, &inam, start_str_hash_func, start_str_eq_func,
                               sizeof(ImportNameAndModule));
  ImportNameAndModule* pinam = (ImportNameAndModule*)dict_rawiter_get(&iter);
  if (!pinam) {
    error("internal error; no module");
  }
  ImportedModuleScope* scope = module_get_scope(pinam->module);
  ImportedSymbol is = {.name = sym_name};
  iter =
      dict_find(&scope->syms, &is, start_str_hash_func, start_str_eq_func, sizeof(ImportedSymbol));
  ImportedSymbol* pis = (ImportedSymbol*)dict_rawiter_get(&iter);
  if (!pis) {
    if (expected_kind) {
      errorf("%s '%.*s' not found in imported package '%.*s'.", expected_kind, (int)str_len(sym_name),
            str_raw_ptr(sym_name), (int)str_len(package_name), str_raw_ptr(package_name));
    }
    // otherwise allow NULL return for not found
  }
  return pis;
}

static Type look_up_imported_type(Str package_name, Str type_name) {
  ImportedSymbol* pis = look_up_imported_symbol(package_name, type_name, "Type");
  if (pis->kind != SYM_TYPE) {
    error("internal error; not type");
  }
  return pis->type;
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

  if (tu.tokbuf.cursor.cur_kind >= TOK_BOOL && tu.tokbuf.cursor.cur_kind <= TOK_UINT) {
    Type t = basic_tok_to_type[tu.tokbuf.cursor.cur_kind];
    ASSERT(!type_is_none(t));
    advance();
    return t;
  }

  if (check(TOK_IDENT_IMPORT) && peek2(TOK_DOT, TOK_IDENT_TYPE)) {
    advance();
    Str package_name = str_from_previous();
    advance();
    advance();
    Str type_name = str_from_previous();

    return look_up_imported_type(package_name, type_name);
  }

  if (match(TOK_IDENT_TYPE)) {
    Sym* sym;
    Str type_name = str_from_previous();
    ScopeResult scope_result = scope_lookup_recursive(type_name, &sym);
    if (scope_result == SCOPE_RESULT_UNDEFINED) {
      errorf("Undefined type %s.", cstr_copy(glob.arena, type_name));
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
    out_names[num_params] = glob.static_str_up;
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

static void consume_block_header(const char* for_what) {
  consumef(TOK_COLON, "Expect ':' to start %s.", for_what);
  consumef(TOK_NEWLINE, "Expect newline after ':' to start %s.", for_what);
  skip_newlines();
  consumef(TOK_INDENT, "Expect indent to to start %s.", for_what);
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
static uint64_t scan_int(StrView num, bool allow_suffix, Type* suffix) {
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
  const TokenKind tok = tu.tokbuf.cursor.cur_kind;
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

static Operand parse_invalid_trailing_comment(bool can_assign, Type* expected) {
  error("Trailing comments not allowed.");
  return operand_none;
}

static Operand parse_alignof(bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
  return operand_none;
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

static unsigned long highest_bit_set(unsigned long long val) {
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

static unsigned long long eval_binary_op_ull(TokenKind op,
                                             unsigned long long left,
                                             unsigned long long right) {
  switch (op) {
    case TOK_STAR: {
      unsigned long long result;
      if (
#if COMPILER_MSVC
          _mul_overflow_u64(left, right, &result)
#else
          __builtin_umulll_overflow(left, right, &result)
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
      unsigned long long required_bits = highest_bit_set(left) + right + 1;
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
      unsigned long long result;
      if (
#if COMPILER_MSVC
          _add_overflow_u64(0, left, right, &result)
#else
          __builtin_uaddll_overflow(left, right, &result)
#endif
      ) {
        errorf("%llu added to %llu overflows.", right, left);
      }
      return result;
    }
    case TOK_MINUS: {
      unsigned long long result;
      if (
#if COMPILER_MSVC
          _sub_overflow_u64(0, left, right, &result)
#else
          __builtin_usubll_overflow(left, right, &result)
#endif
      ) {
        errorf("%llu subtracted from %llu overflows.", right, left);
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
        errorf("%lld multiplied by %lld overflows.", left, right);
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
        errorf("%lld shifted left by %lld requires %lld bits.", left, right, required_bits);
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
        errorf("%lld added to %lld overflows.", right, left);
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
  TokenKind op = tu.tokbuf.cursor.prev_kind;
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
    return operand_none;
  }
}

static Operand parse_bool_literal(bool can_assign, Type* expected) {
  ASSERT(tu.tokbuf.cursor.prev_kind == TOK_FALSE || tu.tokbuf.cursor.prev_kind == TOK_TRUE);
  return operand_const(type_bool, (Val){.b = tu.tokbuf.cursor.prev_kind == TOK_FALSE ? 0 : 1});
}

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
      if (type_is_aggregate(arg.type)) {
        arg_values[num_args].value = operand_to_sqref_lval(&arg);
      } else {
        arg_values[num_args].value = operand_to_sqref_imm(&arg);
      }
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

static Operand parse_compound_literal_given_type(Type lit_type, bool can_assign, Type* expected) {
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
    store_by_type_val_into(field_type, operand_to_sqref_imm(&field_values[i]),
                           sq_i_add(sq_type_long, base_addr, sq_const_int(field_offset)));
  }

  return operand_rvalue_local_addr(lit_type, base_addr);
}

static Operand parse_module_name_prefix(bool can_assign, Type* expected) {
  Str package_name = str_from_previous();
  consume(TOK_DOT, "Expecting '.' after package name.");
  if (match(TOK_IDENT_VAR)) {
    Str var_or_func_name = str_from_previous();
    ImportedSymbol* sym = look_up_imported_symbol(package_name, var_or_func_name, "Variable");
    SqRef ref = sq_ref_extern(sym->extern_name);
    if (type_kind(sym->type) == TYPE_FUNC) {
      return operand_rvalue_global_addr(sym->type, ref);
    } else {
      return operand_lvalue_global_addr(sym->type, ref);
    }
  } else if (match(TOK_IDENT_TYPE)) {
    Str type_name = str_from_previous();
    Type type = look_up_imported_type(package_name, type_name);
    return parse_compound_literal_given_type(type, can_assign, expected);
  } else {
    error("todo; module name prefix other");
  }
}

static Operand parse_compound_literal(bool can_assign, Type* expected) {
  Type lit_type;
  Str type_name = str_from_previous();

  Sym* sym;
  ScopeResult scope_result = scope_lookup_recursive(type_name, &sym);
  if (scope_result == SCOPE_RESULT_UNDEFINED) {
    errorf("Undefined type %s.", cstr_copy(glob.arena, type_name));
  } else if (scope_result == SCOPE_RESULT_GLOBAL && sym->kind == SYM_TYPE) {
    lit_type = sym->type;
    if (type_kind(lit_type) != TYPE_STRUCT) {
      errorf("Cannot construct compound literal of type %s.", type_as_str(lit_type));
    }
  } else {
    error("TODO: unhandled case in compound literal.");
  }

  return parse_compound_literal_given_type(lit_type, can_assign, expected);
}

static Operand parse_dict_literal(bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
  return operand_none;
}

static Operand parse_dot(Operand left, bool can_assign, Type* expected) {
  Str name = parse_name("Expect property name after '.'.");
  uint32_t name_offset = prev_offset();

  if (can_assign && match_assignment()) {
    while (type_kind(left.type) == TYPE_PTR) {
      left = operand_lvalue_local(type_ptr_subtype(left.type), operand_to_sqref_imm(&left));
    }

    if (type_kind(left.type) == TYPE_STRUCT) {
      uint32_t field_offset;
      Type field_type;
      if (type_struct_find_field_by_name(left.type, name, &field_type, &field_offset)) {
        Operand rhs_value = parse_expression(expected);

        if (!convert_operand(&rhs_value, field_type)) {
          errorf_offset(name_offset, "Cannot assign type %s to field '%s' which is type %s.",
                        type_as_str(rhs_value.type), cstr_copy(glob.arena, name),
                        type_as_str(field_type));
        }
        store_by_type_val_into(
            field_type, operand_to_sqref_imm(&rhs_value),
            sq_i_add(sq_type_long, operand_to_sqref_lval(&left), sq_const_int(field_offset)));
        return operand_none;
      } else {
        errorf_offset(name_offset, "'%s' is not a field of type %s.", cstr_copy(glob.arena, name),
                      cstr_copy(glob.arena, type_struct_decl_name(left.type)));
      }
    } else {
      error("todo; assigning to unexpected thing");
    }
  } else {
    Operand new_left = left;
    while (type_kind(new_left.type) == TYPE_PTR) {
      new_left =
          operand_lvalue_local(type_ptr_subtype(new_left.type), operand_to_sqref_imm(&new_left));
    }
    if (type_kind(new_left.type) == TYPE_STRUCT) {
      uint32_t field_offset;
      Type field_type;
      if (type_struct_find_field_by_name(new_left.type, name, &field_type, &field_offset)) {
        return operand_lvalue_local(
            field_type,
            sq_i_add(sq_type_long, operand_to_sqref_lval(&new_left), sq_const_int(field_offset)));
      }

      // Not an error yet; could be a memfn below.
    }

    Sym* func_sym = lookup_memfn(new_left.type, name);
    if (!func_sym) {
      errorf("Undefined member function %s on type %s.", cstr_copy(glob.arena, name),
             type_as_str(new_left.type));
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
    TypeKind left_type_kind = type_kind(left.type);
    if (type_is_basic(left.type) || left_type_kind == TYPE_STRUCT || left_type_kind == TYPE_LIST) {
      self_ptr = operand_to_sqref_lval(&left);
    } else if (left_type_kind == TYPE_PTR &&
               (type_kind(type_ptr_subtype(left.type)) == TYPE_STRUCT ||
                type_kind(type_ptr_subtype(left.type)) == TYPE_LIST)) {
      self_ptr = operand_to_sqref_imm(&left);
    } else {
      error("TODO: self ptr");
    }
    return operand_rvalue_global_addr_bound(func_sym->type, sqref_for_sym(func_sym), self_ptr);
  }

  ASSERT(false && "todo");
  return operand_none;
}

static Operand parse_grouping(bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
  return operand_none;
}
static Operand parse_in_or_not_in(Operand left, bool can_assign, Type* expected) {
  TokenKind op = tu.tokbuf.cursor.prev_kind;

  bool negated = false;
  Operand rhs;
  if (op == TOK_IN) {
    Rule* rule = get_rule(op);
    rhs = parse_precedence(rule->prec_for_infix + 1, expected);
  } else if (op == TOK_NOT && match(TOK_IN)) {
    Rule* rule = get_rule(tu.tokbuf.cursor.prev_kind);
    rhs = parse_precedence(rule->prec_for_infix + 1, expected);
    negated = true;
  } else {
    error("Expected 'in' or 'not in'.");
  }
  Sym* sym = lookup_memfn(rhs.type, glob.static_str___contains__);
  if (sym) {
    // TODO: need to move all the off-brand calls to a common location so they
    // get argument conversion properly (esp sign extension)
    if (!convert_operand(&left, type_func_param(sym->type, 1))) {
      errorf("Can't convert %s to %s.\n", type_as_str(left.type),
             type_as_str(type_func_param(sym->type, 1)));
    }

    SqRef arg =
        type_is_aggregate(left.type) ? operand_to_sqref_lval(&left) : operand_to_sqref_imm(&left);

    Operand res = operand_rvalue_imm(
        type_bool, sq_i_call2(sq_type_word, sqref_for_sym(sym),
                              (SqCallArg){sq_type_long, operand_to_sqref_lval(&rhs)},
                              (SqCallArg){type_to_sqtype(left.type), arg}));
    if (negated) {
      return operand_rvalue_imm(
          type_bool, sq_i_ceqw(sq_type_word, operand_to_sqref_imm(&res), sq_const_int(0)));
    } else {
      return res;
    }
  } else {
    errorf("Type %s does not define __contains__.", type_as_str(rhs.type));
  }
}

// TODO: This breaks error messages, and so does the other str_process_escapes()
// I guess.
static char* get_fmt_string_literal(void) {
  StrView strview = get_strview_for_offsets(prev_offset(), cur_offset());
  StrView inside_quotes = {strview.data + 1, strview.size - 2};
  if (memchr(strview.data, '\\', strview.size) != NULL) {  // worthwhile?
    // Mutates source buffer!
    uint32_t new_len = str_process_escapes((char*)inside_quotes.data, inside_quotes.size);
    if (new_len == 0) {
      error("Invalid string escape.");
    }
    inside_quotes.size = new_len;
  }

  // Need nul termination for the fmtlex scanner, so dup here.
  char* copy = arena_push(glob.arena, inside_quotes.size + 1, 1);
  memcpy(copy, inside_quotes.data, inside_quotes.size);
  copy[inside_quotes.size] = 0;
  return copy;
}

#if 0
static const char* fmtlex_token_kind_name(FmtTokenKind kind) {
  switch (kind) {
    case FMTTOK_LITERAL:
      return "LITERAL";
    case FMTTOK_LBRACE:
      return "LBRACE";
    case FMTTOK_RBRACE:
      return "RBRACE";
    case FMTTOK_FIELD_NAME:
      return "FIELD_NAME";
    case FMTTOK_CONVERSION:
      return "CONVERSION";
    case FMTTOK_COLON:
      return "COLON";
    case FMTTOK_FORMAT_SPEC:
      return "FORMAT_SPEC";
    case FMTTOK_DOT:
      return "DOT";
    case FMTTOK_LBRACKET:
      return "LBRACKET";
    case FMTTOK_RBRACKET:
      return "RBRACKET";
    case FMTTOK_ESCAPED_BRACE:
      return "ESCAPED_BRACE";
    case FMTTOK_EOF:
      return "EOF";
    case FMTTOK_ERROR:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}
#endif

// The format string is currently required to be a constant so this can be more
// like interpolation than runtime error-checked. Once the format language is
// somewhat nice, should implement a separate path that takes a runtime str and
// implements formatting in rt instead.
static Operand parse_fmt(bool can_assign, Type* expected) {
  consume(TOK_LPAREN, "Expect '(' before fmt.");
  uint32_t string_offset = cur_offset();
  if (!match(TOK_STRING_QUOTED)) {
    error_offset(string_offset, "Expecting constant str as first argument to fmt.");
  }
  char *inside_quotes = get_fmt_string_literal();

  Operand args[MAX_FMT_ARGS];
  int num_args = 0;
  while (!match(TOK_RPAREN)) {
    consume(TOK_COMMA, "Expect ',' between fmt values.");
    args[num_args++] = parse_expression(NULL);
  }

  Sym* buf = make_local_and_alloc(SYM_VAR, gensym_var_name(), type_list(type_u8), NULL);
  // TODO: List$reserve(list, len(inside_quotes) + num_args*K, sizeof(u8));
  // or maybe a StackList + copy into final result str.


  fmtlex_start(inside_quotes);
  FmtToken tok;
  bool in_braces = false;
  int index = 0;
  typedef enum IndexingState {
    IS_DEFAULT,
    IS_AUTOMATIC,
    IS_MANUAL,
    IS_MANUAL_REQUIRED,
  } IndexingState;
  IndexingState indexing_state = IS_DEFAULT;
  for (;;) {
    tok = fmtlex_next();
    //printf("TOK: %s\n", fmtlex_token_kind_name(tok.kind));
    if (tok.kind == FMTTOK_LITERAL || tok.kind == FMTTOK_ESCAPED_BRACE) {
      SqRef str = emit_string_obj(tok.data);
      sq_i_call2(sq_type_void, sq_ref_extern("AppendToStringBufferList"),
                 (SqCallArg){sq_type_long, buf->ref}, (SqCallArg){sq_type_long, str});
    } else if (tok.kind == FMTTOK_LBRACE) {
      if (in_braces) {
        error_offset(string_offset, "todo; Nested braces?");
      }
      in_braces = true;
      if (indexing_state == IS_MANUAL) {
        indexing_state = IS_MANUAL_REQUIRED;
      }
    } else if (tok.kind == FMTTOK_RBRACE) {
      in_braces = false;
      if (indexing_state == IS_MANUAL_REQUIRED) {
        error_offset_delta(string_offset, tok.data.data - inside_quotes + 1,
                           "Can't switch to automatic index after manual index.");
      }
      if (index >= num_args) {
        errorf_offset_delta(string_offset, tok.data.data - inside_quotes + 1,
                            "Trying to use argument %d, but only %d provided.", index + 1,
                            num_args);
      }
      Sym* item_str_func = lookup_memfn(args[index].type, glob.static_str___str__);
      if (!item_str_func) {
        errorf_offset(string_offset, "Don't know how to convert type %s to string for fmt.",
                      type_as_str(args[index].type));
      }
      SqRef as_str = sq_i_call1(tu.sq_type_str, sqref_for_sym(item_str_func),
                                (SqCallArg){sq_type_long, operand_to_sqref_lval(&args[index])});
      sq_i_call2(sq_type_void, sq_ref_extern("AppendToStringBufferList"),
                 (SqCallArg){sq_type_long, buf->ref}, (SqCallArg){sq_type_long, as_str});
      if (indexing_state == IS_DEFAULT) {
        indexing_state = IS_AUTOMATIC;
      }
      if (indexing_state == IS_AUTOMATIC) {
        ++index;
      }
    } else if (tok.kind == FMTTOK_FIELD_NAME) {
      if (indexing_state != IS_DEFAULT && indexing_state != IS_MANUAL_REQUIRED) {
        error_offset_delta(string_offset, tok.data.data - inside_quotes + 1,
                           "Can't switch to manual index after automatic index.");
      }
      indexing_state = IS_MANUAL;
      // TODO: named fields, only handling numerical right now
      index = scan_int(tok.data, false, NULL);
    } else if (tok.kind == FMTTOK_EOF) {
      break;
    } else if (tok.kind == FMTTOK_ERROR) {
      error_offset(string_offset, "Parse error in fmt string.");
    } else {
      error("todo; unhandled fmttok");
    }
  }

  // Reinterpret []u8 as Str since the header is the same. This probably is a
  // bad idea and something will break.
  return operand_rvalue_imm(type_str, buf->ref);
}

static Operand parse_len(bool can_assign, Type* expected) {
  consume(TOK_LPAREN, "Expect '(' after len.");
  Operand len_of = parse_precedence(PREC_OR, NULL);
  consume(TOK_RPAREN, "Expect ')' after len.");
  switch (type_kind(len_of.type)) {
    case TYPE_ARRAY:
      return operand_const(type_u64, (Val){.u64 = type_array_count(len_of.type)});
    case TYPE_LIST:
    case TYPE_STR:
      return operand_rvalue_imm(
          type_u64, sq_i_load(sq_type_long, sq_i_add(sq_type_long, len_of.ref, sq_const_int(8))));
    case TYPE_DICT:
      error("TODO: len impl");
    default:
      errorf("Cannot use len on type %s.", type_as_str(len_of.type));
  }
}

static bool scan_to_determine_if_comprehension(TokenCursor* original, TokenCursor* at_for) {
  ASSERT(tu.tokbuf.num_peeks == 0);

  *original = tu.tokbuf.cursor;
  original->paren_level = token_get_continuation_paren_level();

  // We start the scan after the starting [.
  int square_bracket_count = 1;
  for (;;) {
    if (tu.tokbuf.cursor.cur_kind == TOK_LSQUARE) {
      ++square_bracket_count;
    } else if (tu.tokbuf.cursor.cur_kind == TOK_RSQUARE) {
      --square_bracket_count;
      if (square_bracket_count == 0) {
        tu.tokbuf.cursor = *original;
        token_restore_continuation_paren_level(original->paren_level);
        return false;
      }
    } else if (tu.tokbuf.cursor.cur_kind == TOK_FOR) {
      *at_for = tu.tokbuf.cursor;
      return true;
    } else if (tu.tokbuf.cursor.cur_kind == TOK_NEWLINE || tu.tokbuf.cursor.cur_kind == TOK_EOF) {
      error("Expecting ']' to end list literal or comprehension.");
    }

    tu.tokbuf.cursor.prev_kind = tu.tokbuf.cursor.cur_kind;
    ++tu.tokbuf.cursor.token_index;
    ASSERT(tu.tokbuf.cursor.token_index < tu.tokbuf.num_tokens);
    tu.tokbuf.cursor.cur_kind =
        token_categorize(tu.tokbuf.token_offsets[tu.tokbuf.cursor.token_index],
                         tu.tokbuf.token_offsets[tu.tokbuf.cursor.token_index + 1]);
    ASSERT(tu.tokbuf.cursor.cur_kind != TOK_NEWLINE_BLANK);
    ASSERT(tu.tokbuf.cursor.cur_kind < TOK_NEWLINE_INDENT_0 ||
           tu.tokbuf.cursor.cur_kind > TOK_NEWLINE_INDENT_40);
  }
}

typedef enum IterationKind {
  ITK_UNKNOWN = 0,
  ITK_ARRAY,
  ITK_LIST,
  ITK_STR,
  ITK_RANGE,
} IterationKind;

struct IterationData {
  IterationKind kind;
  Sym* itsym;
  SqBlock loop_start;
  SqBlock loop_continue;   // Used for continue
  SqBlock loop_done;       // Used for normal exit and break
  int exit_call_mark_for_break_continue;
  Type it_type;
  union {
    struct {
      SqRef ptr;
      SqRef end;
    } CONTIG;
    struct {
      SqRef stop;
      SqRef step;
      SqRef is_neg;
    } RANGE;
  };
};

static IterationData iteration_prolog(Str it, Operand* over) {
  IterationData itd = {0};
  if (type_kind(over->type) == TYPE_ARRAY) {
    itd.kind = ITK_ARRAY;
    itd.it_type = type_array_subtype(over->type);
    itd.itsym = make_local_and_alloc(SYM_VAR, it, itd.it_type, NULL);
    itd.CONTIG.ptr = sq_i_alloc8(sq_const_int(8));
    itd.CONTIG.end = sq_i_alloc8(sq_const_int(8));
    sq_i_storel(over->ref, itd.CONTIG.ptr);
    sq_i_storel(sq_i_add(sq_type_long, over->ref,
                         sq_const_int(type_array_count(over->type) * type_size(itd.it_type))),
                itd.CONTIG.end);
  } else if (type_kind(over->type) == TYPE_LIST) {
    itd.kind = ITK_LIST;
    itd.it_type = type_list_subtype(over->type);
    itd.itsym = make_local_and_alloc(SYM_VAR, it, itd.it_type, NULL);
    itd.CONTIG.ptr = sq_i_alloc8(sq_const_int(8));
    itd.CONTIG.end = sq_i_alloc8(sq_const_int(8));
    SqRef base = sq_i_load(sq_type_long, over->ref);
    sq_i_storel(base, itd.CONTIG.ptr);
    SqRef count = sq_i_load(sq_type_long, sq_i_add(sq_type_long, over->ref, sq_const_int(8)));
    sq_i_storel(sq_i_add(sq_type_long, base,
                         sq_i_mul(sq_type_long, sq_const_int(type_size(itd.it_type)), count)),
                itd.CONTIG.end);
  } else if (type_kind(over->type) == TYPE_STR) {
    itd.kind = ITK_STR;
    itd.it_type = type_codept;
    itd.itsym = make_local_and_alloc(SYM_VAR, it, itd.it_type, NULL);
    itd.CONTIG.ptr = sq_i_alloc8(sq_const_int(8));
    itd.CONTIG.end = sq_i_alloc8(sq_const_int(8));
    SqRef base = sq_i_load(sq_type_long, over->ref);
    sq_i_storel(base, itd.CONTIG.ptr);
    SqRef count = sq_i_load(sq_type_long, sq_i_add(sq_type_long, over->ref, sq_const_int(8)));
    sq_i_storel(sq_i_add(sq_type_long, base, count), itd.CONTIG.end);
  } else if (type_eq(over->type, type_range)) {
    itd.kind = ITK_RANGE;
    itd.it_type = type_i64;
    itd.itsym = make_local_and_alloc(SYM_VAR, it, itd.it_type, NULL);

    SqRef astart = over->ref;
    SqRef astop = sq_i_add(sq_type_long, over->ref, sq_const_int(8));
    SqRef astep = sq_i_add(sq_type_long, over->ref, sq_const_int(16));

    SqRef start = sq_i_load(sq_type_long, astart);
    itd.RANGE.stop = sq_i_load(sq_type_long, astop);
    itd.RANGE.step = sq_i_load(sq_type_long, astep);

    itd.RANGE.is_neg = sq_i_csltl(sq_type_long, itd.RANGE.step, sq_const_int(0));

    // TODO: This probably needs work if the Range isn't trivial, start
    // should be using the Operand expr or something maybe
    sq_i_storel(start, itd.itsym->ref);
  } else {
    errorf("Can't iterate over type %s.", type_as_str(over->type));
  }

  itd.loop_start = sq_block_declare_and_start();
  itd.loop_continue = sq_block_declare();
  itd.exit_call_mark_for_break_continue = tu.cur_scope->num_exit_calls;

  SqBlock block_body = sq_block_declare();
  itd.loop_done = sq_block_declare();

  if (itd.kind == ITK_ARRAY || itd.kind == ITK_LIST) {
    SqRef cur = sq_i_load(sq_type_long, itd.CONTIG.ptr);
    SqRef end = sq_i_load(sq_type_long, itd.CONTIG.end);
    SqRef cmp = sq_i_csltl(sq_type_long, cur, end);
    sq_i_jnz(cmp, block_body, itd.loop_done);
    sq_block_start(block_body);

    store_by_type_val_into(itd.it_type, load_by_type_from(itd.it_type, cur), itd.itsym->ref);
  } else if (itd.kind == ITK_STR) {
    SqRef cur = sq_i_load(sq_type_long, itd.CONTIG.ptr);
    SqRef end = sq_i_load(sq_type_long, itd.CONTIG.end);
    SqRef cmp = sq_i_csltl(sq_type_long, cur, end);
    sq_i_jnz(cmp, block_body, itd.loop_done);
    sq_block_start(block_body);

    // TODO: utf8. This is writing to a charpt (correct), but just iterating over
    // bytes; need to decode code points here.

    store_by_type_val_into(itd.it_type, load_by_type_from(type_u8, cur), itd.itsym->ref);
  } else if (itd.kind == ITK_RANGE) {
    SqRef cur = sq_i_load(sq_type_long, itd.itsym->ref);

    SqBlock block_neg_step = sq_block_declare();
    SqBlock block_pos_step = sq_block_declare();
    SqBlock block_cont = sq_block_declare();

    // (is_neg ? cur > stop : cur < stop)
    sq_i_jnz(itd.RANGE.is_neg, block_neg_step, block_pos_step);

    sq_block_start(block_neg_step);
    sq_i_jnz(sq_i_csgtl(sq_type_long, cur, itd.RANGE.stop), block_cont, itd.loop_done);

    sq_block_start(block_pos_step);
    sq_i_jnz(sq_i_csltl(sq_type_long, cur, itd.RANGE.stop), block_cont, itd.loop_done);

    sq_block_start(block_cont);
  } else {
    error("internal error: unhandled case in iter prolog");
  }

  return itd;
}

static void iteration_epilog(IterationData itd) {
  sq_block_start(itd.loop_continue);

  if (itd.kind == ITK_ARRAY || itd.kind == ITK_LIST) {
    sq_i_storel(sq_i_add(sq_type_long, sq_i_load(sq_type_long, itd.CONTIG.ptr),
                         sq_const_int(type_size(itd.it_type))),
                itd.CONTIG.ptr);
  } else if (itd.kind == ITK_STR) {
    // TODO: utf8
    sq_i_storel(sq_i_add(sq_type_long, sq_i_load(sq_type_long, itd.CONTIG.ptr), sq_const_int(1)),
                itd.CONTIG.ptr);
  } else if (itd.kind == ITK_RANGE) {
    SqRef it_val = sq_i_load(sq_type_long, itd.itsym->ref);
    SqRef inc = sq_i_add(sq_type_long, it_val, itd.RANGE.step);
    sq_i_storel(inc, itd.itsym->ref);
  } else {
    error("internal error: unhandled case in iter epilog");
  }

  sq_i_jmp(itd.loop_start);
  sq_block_start(itd.loop_done);
}

static Operand parse_list_comprehension(TokenCursor original, TokenCursor at_for, Type* expected) {
  tu.tokbuf.cursor = at_for;
  consume(TOK_FOR, "Expect 'for' to start list comprehension.");
  Str it = parse_name("Expect iterator name of list comprehension.");
  // TODO: other forms for enumerate
  consume(TOK_IN, "Expect 'in'.");
  Operand over = parse_expression(NULL);
  if (check(TOK_FOR)) {
    error("todo; multiple for in list compr");
  }
  bool have_condition = check(TOK_IF);

  // In general, we have to assume a slice output here because even if iterating
  // over an array, it could be filtered, so we can't know the number of
  // outputs. So this only creates an array if |expected| is provided
  // explicitly. (Not sure this case is worth it over just always returning a
  // slice and writing array versions as loops for cases where the allocation
  // has to be avoided.)
  if (expected && type_kind(*expected) == TYPE_ARRAY &&
      type_array_count(*expected) == type_array_count(over.type) && !have_condition) {
    // TODO: enter a full function scope here? or some third non-module,
    // non-function type of scope?
    // I think it has to be equivalent to a nested function, because the iterator
    // shadows.
    enter_function_scope(NULL);

    Type subtype = type_array_subtype(*expected);
    SqRef arr_base = sq_i_alloc8(sq_const_int(type_size(subtype) * type_array_count(over.type)));
    SqRef store_ptr = sq_i_alloc8(sq_const_int(8));
    sq_i_storel(arr_base, store_ptr);

    IterationData itd = iteration_prolog(it, &over);
    ASSERT(type_eq(itd.it_type, subtype));

    TokenCursor after_clauses = tu.tokbuf.cursor;
    tu.tokbuf.cursor = original;

    Operand elem = parse_expression(NULL);

    // Store elem into created array.
    // This only works because we're assuming it's an array, and no filter; just
    // assign to the same index in the created array as in the source array.
    store_by_type_val_into(subtype, operand_to_sqref_imm(&elem),
                           sq_i_load(sq_type_long, store_ptr));
    sq_i_storel(sq_i_add(sq_type_long, sq_i_load(sq_type_long, store_ptr),
                         sq_const_int(type_size(subtype))),
                store_ptr);

    iteration_epilog(itd);

    leave_scope();

    tu.tokbuf.cursor = after_clauses;

    return operand_rvalue_imm(type_array(subtype, type_array_count(over.type)), arr_base);
  } else {
    // General slice case.

    // TODO: same question as above
    enter_function_scope(NULL);

    // Ugly: We haven't parsed the iteration expression yet, so we don't know
    // the result type, so we just allocate a zero-initialized block of the
    // correct size for a List object, which will work with appending because by
    // the type we actually have an element to append we'll know the type of the
    // element.
    // 'i32' isn't important, as all list objs are the same size.
    size_t untyped_list_size = type_size(type_list(type_i32));
    SqRef untyped_list = sq_i_alloc8(sq_const_int(untyped_list_size));
    SqRef memset_func = sq_ref_extern("memset");
    sq_i_call3(sq_type_void, memset_func, (SqCallArg){sq_type_long, untyped_list},
               (SqCallArg){sq_type_word, sq_const_int(0)},
               (SqCallArg){sq_type_long, sq_const_int(untyped_list_size)});

    IterationData itd = iteration_prolog(it, &over);

    SqBlock true_block = sq_block_declare();
    SqBlock false_block = sq_block_declare();
    if (have_condition) {
      consume(TOK_IF, "Expect 'if'.");
      Operand cond = parse_expression(NULL);
      if (!type_is_condition(cond.type)) {
        errorf("Result of condition expression cannot be type %s.", type_as_str(cond.type));
      }

      sq_i_jnz(operand_to_sqref_imm(&cond), true_block, false_block);
      sq_block_start(true_block);
    }

    TokenCursor after_clauses = tu.tokbuf.cursor;
    tu.tokbuf.cursor = original;

    Operand elem = parse_expression(NULL);


    // TODO: This is very bad, being inside the loop.
    Sym* lval = make_local_and_alloc(SYM_VAR, gensym_var_name(), elem.type, NULL);
    copy_by_type(&elem, lval->ref);

    SqRef list_append_func = sq_ref_extern("List$append");

    // TODO: I can't come up with a case yet where promotion is needed, but it
    // seems like it might be here.
    sq_i_call3(sq_type_void, list_append_func, (SqCallArg){sq_type_long, untyped_list},
               (SqCallArg){sq_type_long, lval->ref},
               (SqCallArg){sq_type_long, sq_const_int(type_size(elem.type))});

    sq_block_start(false_block);

    iteration_epilog(itd);

    leave_scope();

    tu.tokbuf.cursor = after_clauses;

    return operand_rvalue_imm(type_list(elem.type), untyped_list);
  }
}

static Operand parse_list_literal(Type* expected) {
  OpVec elems;
  opv_init(&elems, glob.arena);

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
    copy_by_type(&first_item, arr_base);
    for (int i = 1; i < elems.size; ++i) {
      Operand next_item = opv_at(&elems, i);
      if (!convert_operand(&next_item, first_item.type)) {
        errorf("List item %d is of type %s which does not match type %s of first element.", i + 1,
               type_as_str(next_item.type), type_as_str(first_item.type));
      }
      SqRef target = sq_i_add(sq_type_long, arr_base, sq_const_int(type_size(first_item.type) * i));
      copy_by_type(&next_item, target);
    }
    if (expected && type_kind(*expected) == TYPE_LIST) {
      // TODO: worse to make the array on the stack first if it's big?
      size_t list_size = type_size(*expected);
      SqRef list_obj = sq_i_alloc8(sq_const_int(list_size));
      initialize_aggregate(list_obj, *expected);
      sq_i_call4(sq_type_void, sq_ref_extern("List$copy_from_array"),
                 (SqCallArg){sq_type_long, list_obj}, (SqCallArg){sq_type_long, arr_base},
                 (SqCallArg){sq_type_long, sq_const_int(elems.size)},
                 (SqCallArg){sq_type_long, sq_const_int(type_size(first_item.type))});
      return operand_rvalue_imm(type_list(first_item.type), list_obj);
    } else {
      return operand_rvalue_imm(type_array(first_item.type, elems.size), arr_base);
    }
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
  return glob.op_null_ptr;
}

static Operand parse_int_literal(bool allow_suffix) {
  Type suffix = {0};
  StrView view = get_strview_for_offsets(prev_offset(), cur_offset());
  ASSERT(view.size > 0);
  while (view.data[view.size - 1] == ' ') {
    --view.size;
  }
  uint64_t val = scan_int(view, allow_suffix, &suffix);
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

static double scan_fractional_part_of_double(StrView num) {
  char* copy = arena_push(glob.arena, num.size + 2, 1);
  copy[0] = '.';
  memcpy(copy + 1, num.data, num.size);
  copy[num.size + 1] = 0;
  char* end;
  return strtod(copy, &end);
}

// The simd lexer can't handle contextually distinguishing between '.' in the
// context of separating field or package access vs. being the decimal point in
// a floating point number. A previous version used '`' as the decimal separator
// to avoid this problem, but it just felt too ugly to use in practice. Using
// comma for a decimal separator also would have problems because of separating
// function call arguments. A single quote would also be confusing (looks like
// digit grouping). Semi-colon is maybe plausible as a separator (sort of a
// combination of North American and European styles?) but still feels a bit
// hokey to have to separate that way (in particular for us aged C-like-rs).
//
// So! Failing a way to properly lex floats, we defer the problem to the parser
// and turn a sequence like [int dot int] into a double const instead of a u64.
//
// This is currently somewhat too flexible, and will allow things like
// "0xabcu64.34" to be a float, but maybe something like that is useful (?)
// especially if we we want a direct writing down of floats by writing IEEE-754
// formatted hex value.
//
// This also sucks in that it'll accept "1 . 4". Hrm.
static Operand parse_number(bool can_assign, Type* expected) {
  Operand integer_part = parse_int_literal(true);
  if (!check(TOK_DOT)) {
    return integer_part;
  }

  // Some form of floating point number now.

  advance();

  double final = (double)integer_part.val.i64;
  if (check(TOK_INT_LITERAL)) {
    advance();
    final += scan_fractional_part_of_double(get_strview_for_offsets(prev_offset(), cur_offset()));
  } else {
    // Just "1.", nothing to add fractionally.
  }
  return operand_const(type_double, (Val){.d = final});
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
  errorf_offset(name_offset, "'%s' is not a field of type %s.", cstr_copy(glob.arena, field),
                cstr_copy(glob.arena, type_struct_decl_name(type)));
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

static Operand parse_range_literal(bool can_assign, Type* expected) {
  consume(TOK_LPAREN, "Expect '(' after range.");
  Operand first = parse_precedence(PREC_OR, &type_i64);
  if (!convert_operand(&first, type_i64)) {
    errorf("Cannot convert type %s to i64.", type_as_str(first.type));
  }

  Operand second = operand_none;
  Operand third = operand_none;
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
  consume(TOK_LPAREN, "Expecting '(' after sizeof.");
  Type type = parse_type();
  if (type_is_none(type)) {
    error("TODO: sizeof expr");
  }
  consume(TOK_RPAREN, "Expect ')' after sizeof.");
  return operand_const(type_u64, (Val){.u64=type_size(type)});
}

static Operand parse_string(bool can_assign, Type* expected) {
  StrView strview = get_strview_for_offsets(prev_offset(), cur_offset());
  StrView inside_quotes = {strview.data + 1, strview.size - 2};
  while (inside_quotes.data[inside_quotes.size] != '"') {
    --inside_quotes.size;
  }
  if (memchr(strview.data, '\\', strview.size) != NULL) {  // worthwhile?
    // Mutates source buffer!
    uint32_t new_len = str_process_escapes((char*)inside_quotes.data, inside_quotes.size);
    if (new_len == 0) {
      error("Invalid string escape.");
    }
    inside_quotes.size = new_len;
  }
  return operand_rvalue_global_addr(type_str, emit_string_obj(inside_quotes));
}

static Operand parse_string_interpolate(bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
  return operand_none;
}

static Operand parse_subscript(Operand left, bool can_assign, Type* expected) {
  SqRef target_addr;
  Type subtype;

  TypeKind left_type_kind = type_kind(left.type);
  // TODO: type_generic_subtype for array/list/str/ptr maybe

  if (match(TOK_COLON)) {
    if (check(TOK_RSQUARE)) {  // [:]
      // slice(left, NULL, NULL);
      // Not really that useful? Maybe just disallow to avoid thinking it's
      // solving anything to do with memory allocation.
      error("TODO: [:]");
    } else {  // [:x]
      // slice(left, NULL, parse_expression())
      if (left_type_kind == TYPE_LIST) {
        subtype = type_list_subtype(left.type);
        Operand subscript = parse_expression(NULL);
        if (!type_is_integer(subscript.type)) {
          errorf("Cannot subscript using type %s.", type_as_str(subscript.type));
        }
        consume(TOK_RSQUARE, "Expecting ']' to end slicing expression.");
        return operand_rvalue_imm(
            type_list(subtype),
            sq_i_call4(tu.sq_type_list, sq_ref_extern("List$slice_from_list"),
                       (SqCallArg){sq_type_long, left.ref},
                       (SqCallArg){sq_type_long, sq_const_int(type_size(subtype))},
                       (SqCallArg){sq_type_long, sq_const_int(0)},
                       (SqCallArg){sq_type_long, operand_to_sqref_imm(&subscript)}));
      } else {
        error("TODO: [:x] for other type");
      }
    }
  } else {
    Operand subscript = parse_expression(NULL);
    if (!type_is_integer(subscript.type)) {
      errorf("Cannot subscript using type %s.", type_as_str(subscript.type));
    }
    if (match(TOK_COLON)) {
      if (match(TOK_RSQUARE)) {  // [x:]
        // slice(left, subscript, NULL)
        if (left_type_kind == TYPE_LIST) {
          subtype = type_list_subtype(left.type);
          return operand_rvalue_imm(
              type_list(subtype),
              sq_i_call4(tu.sq_type_list, sq_ref_extern("List$slice_from_list"),
                         (SqCallArg){sq_type_long, left.ref},
                         (SqCallArg){sq_type_long, sq_const_int(type_size(subtype))},
                         (SqCallArg){sq_type_long, operand_to_sqref_imm(&subscript)},
                         (SqCallArg){sq_type_long, sq_const_int(INT64_MAX)}));
        } else {
          error("TODO: [x:] for other type");
        }
      } else {  // [x:y]
        // slice(left, subscript, parse_expression());
        if (left_type_kind == TYPE_LIST) {
          subtype = type_list_subtype(left.type);
          Operand subscript2 = parse_expression(NULL);
          if (!type_is_integer(subscript2.type)) {
            errorf("Cannot subscript using type %s.", type_as_str(subscript2.type));
          }
          consume(TOK_RSQUARE, "Expecting ']' to end slicing expression.");
          return operand_rvalue_imm(
              type_list(subtype),
              sq_i_call4(tu.sq_type_list, sq_ref_extern("List$slice_from_list"),
                         (SqCallArg){sq_type_long, left.ref},
                         (SqCallArg){sq_type_long, sq_const_int(type_size(subtype))},
                         (SqCallArg){sq_type_long, operand_to_sqref_imm(&subscript)},
                         (SqCallArg){sq_type_long, operand_to_sqref_imm(&subscript2)}));
        }
        error("TODO: [x:y] for other type");
      }
    } else {
      // Regular subscript.
      switch (left_type_kind) {
        case TYPE_ARRAY:
        case TYPE_LIST:
        case TYPE_PTR:
        case TYPE_STR: {
          if (left_type_kind == TYPE_ARRAY) {
            subtype = type_array_subtype(left.type);
            target_addr = sq_i_add(sq_type_long, left.ref,
                                   sq_i_mul(sq_type_long, sq_const_int(type_size(subtype)),
                                            operand_to_sqref_imm(&subscript)));
          } else if (left_type_kind == TYPE_LIST) {
            subtype = type_list_subtype(left.type);
            SqRef arr_base = sq_i_load(sq_type_long, left.ref);
            target_addr = sq_i_add(sq_type_long, arr_base,
                                   sq_i_mul(sq_type_long, sq_const_int(type_size(subtype)),
                                            operand_to_sqref_imm(&subscript)));
          } else if (left_type_kind == TYPE_PTR) {
            subtype = type_ptr_subtype(left.type);
            target_addr = sq_i_add(sq_type_long, operand_to_sqref_imm(&left),
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
    store_by_type_val_into(rhs.type, operand_to_sqref_imm(&rhs), target_addr);
    return operand_none;
  } else {
    return operand_lvalue_local(subtype, target_addr);
  }
}

static Operand parse_typeid(bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
  return operand_none;
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
  TokenKind op_kind = tu.tokbuf.cursor.prev_kind;
  uint32_t expr_offset = cur_offset();
  if (op_kind == TOK_CAST) {
    Type type = parse_type();
    if (type_is_none(type)) {
      error("Expected type for cast.");
    }
    Operand expr = parse_precedence(PREC_UNARY, expected);
    if (!is_castable(&expr, type)) {
      errorf("Cannot cast %s to %s.", type_as_str(expr.type), type_as_str(type));
    }
    if (tu.evaluating_const) {
      error("Constant evaluation for cast not implemented yet.");
    }
    if (!cast_operand(&expr, type)) {
      error("internal error: failed to cast?");
    }
    return operand_rvalue_imm(type, operand_to_sqref_imm(&expr));
  } else {
    Operand expr = parse_precedence(PREC_UNARY, expected);
    if (op_kind == TOK_MINUS) {
      if (op_is_const(expr)) {
        return operand_const(expr.type, eval_unary_op(op_kind, expr.type, expr.val));
      } else {
        ASSERT(false && "todo");
        return operand_none;
#if 0
      return operand_rvalue_imm(expr.type,
                                ir_NEG(type_to_ir_type(expr.type), operand_to_irref_imm(&expr)));
#endif
      }
    } else if (op_kind == TOK_NOT) {
      if (op_is_const(expr)) {
        if (type_eq(expr.type, type_bool)) {
          // Not really any point to is_condition I don't think, and makes the
          // eval more complicated.
          return operand_const(type_bool, (Val){.b = expr.val.b == 0});
        } else {
          errorf("Type %s cannot be used in a boolean not.", type_as_str(expr.type));
        }
      } else {
        if (type_is_condition(expr.type)) {
          return operand_rvalue_imm(
              expr.type, sq_i_ceqw(sqbasetype_from_type(expr.type), operand_to_sqref_imm(&expr),
                                   sq_const_int(0)));
        } else {
          errorf("Type %s cannot be used in a boolean not.", type_as_str(expr.type));
        }
      }
    } else if (op_kind == TOK_AMPERSAND) {
      if (!op_is_lval(expr)) {
        error_offset(expr_offset, "Can't take the address of non-lvalue.");
      }
      return operand_rvalue_imm(type_ptr(expr.type), expr.ref);
#if 0
    return operand_rvalue_imm(type_ptr(expr.type), ir_VADDR(expr.ref));
#endif
    } else {
      error("unary operator not implemented");
    }
  }
}

static Sym* find_in_scope(Scope* scope, Str name) {
  if (BRANCH_UNLIKELY(scope->is_full_dict)) {
    DictRawIter iter = dict_find(&scope->sym_dict, &name, start_str_hash_func, start_str_eq_func,
                                 sizeof(NameSymPair));
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
  Scope* cur_scope = tu.cur_scope;
  for (;;) {
    ScopeResult res = scope_lookup_single(cur_scope, name, crossed_function, sym);
    if (res != SCOPE_RESULT_UNDEFINED) {
      return res;
    }
    // otherwise keep going upwards

    if (cur_scope->is_function) {
      crossed_function = true;
    }

    if (cur_scope == &tu.scopes[0]) {
      break;
    }
    ASSERT(cur_scope >= &tu.scopes[0] &&
           cur_scope <= &tu.scopes[tu.num_scopes - 1]);
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

  ASSERT(scope >= &tu.scopes[1] && scope <= &tu.scopes[tu.num_scopes - 1]);
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
  SqRef val = load_by_type_from(type, sq_i_add(sq_type_long, scope->upval_base,
                                               sq_const_int(uvm->upvals[upval_index].offset)));
  return operand_rvalue_imm(type, val);
}

static Operand load_value(ScopeResult scope_result, Sym* sym, Str var_name) {
  switch (scope_result) {
    case SCOPE_RESULT_LOCAL:
      if (type_kind(sym->type) == TYPE_FUNC) {
        if (type_func_is_nested(sym->type)) {
          return operand_bound_local_function(sym->type, sqref_for_sym(sym), sym->ref2);
        } else {
          return operand_rvalue_global_addr(sym->type, sqref_for_sym(sym));
        }
      } else {
        if (sym->scope_decl == SSD_DECLARED_GLOBAL) {
          return operand_lvalue_global_addr(sym->type, sqref_for_sym(sym));
        } else {
          return operand_lvalue_local(sym->type, sym->ref);
        }
      }
    case SCOPE_RESULT_PARAMETER: {
      // Paramters aren't mutable and they're sqbe values, not variables with
      // stack space.
      return operand_rvalue_imm(sym->type, sym->ref);
    }
    case SCOPE_RESULT_GLOBAL: {
      if (sym->kind == SYM_MODULE) {
        return operand_const(sym->type, (Val){.m = sym->module});
      } else {
        if (type_kind(sym->type) == TYPE_FUNC) {
          // Doesn't make sense in our use for GLOBAL to be bound I don't think.
          return operand_rvalue_global_addr(sym->type, sqref_for_sym(sym));
        } else {
          return operand_lvalue_global_addr(sym->type, sqref_for_sym(sym));
        }
      }
    }
    case SCOPE_RESULT_UPVALUE: {
      // We already did a scope_lookup() so we know the in the current function,
      // we need to reference this value through $up.
      Operand value = find_or_create_upval(tu.cur_scope, var_name, sym);
      return value;
    }
    case SCOPE_RESULT_UNDEFINED: {
      errorf("Undefined reference to '%s'.", cstr_copy(glob.arena, var_name));
    }
  }
}

static Operand parse_variable(bool can_assign, Type* expected) {
  if (tu.evaluating_const) {
    error("Expression is not constant.");
  }

  Str target = str_from_previous();
  Sym* sym = NULL;
  ScopeResult scope_result = scope_lookup_recursive(target, &sym);
  if (can_assign && match_assignment()) {
    TokenKind eq_kind = tu.tokbuf.cursor.prev_kind;
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
          store_by_type_val_into(op.type, operand_to_sqref_imm(&op), sym->ref);
          return operand_none;
        } else {
          error_offset(eq_offset, "Unhandled assignment type.");
        }
      }
      case SCOPE_RESULT_UNDEFINED:
      case SCOPE_RESULT_GLOBAL: {
        if (tu.cur_scope->is_function) {
          ASSERT(!tu.cur_scope->is_module);

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
            return operand_none;
          } else {
            error_offset(eq_offset,
                         "Cannot use an augmented assignment when implicitly declaring a local.");
          }
        } else {
          ASSERT(tu.cur_scope->is_module);
          ASSERT(!tu.cur_scope->is_function);
          ASSERT(eq_kind == TOK_EQ);
          if (scope_result == SCOPE_RESULT_UNDEFINED) {
            // Global variable declaration without a type.
            Operand op = const_expression();
            if (!op_is_const(op)) {
              error("Global initializers must be constants.");
            }
            make_global(SYM_VAR, target, op.type, &op.val);
            return operand_none;
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
    //printf("looking %s\n", cstr_copy(glob.arena, target));
    //dump_scope(tu.mod_scope);
    return load_value(scope_result, sym, target);
  }
}

static Operand parse_constant(bool can_assign, Type* expected) {
  Str target = str_from_previous();
  Sym* sym = NULL;
  ScopeResult scope_result = scope_lookup_recursive(target, &sym);
  if (can_assign && match_assignment()) {
    TokenKind eq_kind = tu.tokbuf.cursor.prev_kind;
    TokenKind eq_offset = prev_offset();
    switch (scope_result) {
      case SCOPE_RESULT_LOCAL: {
        error("Cannot assign a new value to a constant.");
      }
      case SCOPE_RESULT_UNDEFINED:
      case SCOPE_RESULT_GLOBAL: {
        if (tu.cur_scope->is_function) {
          ASSERT(!tu.cur_scope->is_module);

          ASSERT((scope_result == SCOPE_RESULT_UNDEFINED && !sym) ||
                 (scope_result == SCOPE_RESULT_GLOBAL && sym));
          // If a local wasn't found, then implicitly create and initialize it.
          // (If it was found in the global scope, then it's not relevant for
          // assignment because a `global blah` will be found as a local with a
          // scope_decl of GLOBAL instead.)
          if (eq_kind == TOK_EQ) {
            // Local variable declaration without a type.
            Operand op = const_expression();
            Sym* sym = make_local_and_alloc(SYM_CONST, target, op.type, &op);
            sym->val = op.val;
            return operand_none;
          } else {
            error_offset(eq_offset,
                         "Cannot use an augmented assignment when declaring a constant.");
          }
        } else {
          ASSERT(tu.cur_scope->is_module);
          ASSERT(!tu.cur_scope->is_function);
          ASSERT(eq_kind == TOK_EQ);
          if (scope_result == SCOPE_RESULT_UNDEFINED) {
            // Global variable declaration without a type.
            Operand op = const_expression();
            if (!op_is_const(op)) {
              error("Global initializers must be constants.");
            }
            Sym* sym = make_global(SYM_CONST, target, op.type, &op.val);
            sym->val = op.val;
            return operand_none;
          } else {
            ASSERT(scope_result == SCOPE_RESULT_GLOBAL);
            error("Cannot assign a new value to a constant.");
          }
        }
      }

      case SCOPE_RESULT_PARAMETER: {
        error("internal error; const func param");
      }

      case SCOPE_RESULT_UPVALUE: {
        error("internal error; const upval");
      }
    }
  } else {
    return operand_const(sym->type, sym->val);
  }
}

// Has to match the order in tokens.inc.
static Rule rules[NUM_TOKEN_KINDS] = {
    {NULL, NULL, PREC_NONE},                            // TOK_INVALID
    {parse_invalid_trailing_comment, NULL, PREC_NONE},  // TOK_INVALID_TRAILING_COMMENT
    {NULL, NULL, PREC_NONE},                            // TOK_EOF
    {NULL, NULL, PREC_NONE},                            // TOK_INDENT
    {NULL, NULL, PREC_NONE},                            // TOK_DEDENT
    {NULL, NULL, PREC_NONE},                            // TOK_NEWLINE

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
    {NULL, NULL, PREC_NONE},                                    // TOK_FLOAT_LITERAL
    {parse_fmt, NULL, PREC_NONE},                               // TOK_FMT
    {NULL, NULL, PREC_NONE},                                    // TOK_FOR
    {NULL, NULL, PREC_NONE},                                    // TOK_FOREIGN
    {NULL, parse_binary, PREC_COMPARISON},                      // TOK_GEQ
    {NULL, NULL, PREC_NONE},                                    // TOK_GLOBAL
    {NULL, parse_binary, PREC_COMPARISON},                      // TOK_GT
    {parse_variable, NULL, PREC_NONE},                          // TOK_IDENT_VAR
    {parse_compound_literal, NULL, PREC_NONE},                  // TOK_IDENT_TYPE
    {parse_constant, NULL, PREC_NONE},                          // TOK_IDENT_CONST
    {parse_module_name_prefix, NULL, PREC_NONE},                // TOK_IDENT_IMPORT
    {NULL, NULL, PREC_NONE},                                    // TOK_IDENT_DECORATOR
    {NULL, NULL, PREC_NONE},                                    // TOK_IF
    {NULL, NULL, PREC_NONE},                                    // TOK_IMPORT
    {NULL, parse_in_or_not_in, PREC_COMPARISON},                // TOK_IN
    {parse_number, NULL, PREC_NONE},                            // TOK_INT_LITERAL
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
    {parse_range_literal, NULL, PREC_NONE},                     // TOK_RANGE
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
    {NULL, NULL, PREC_NONE},  // TOK_CODEPT
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
  PrefixFn prefix_rule = get_rule(tu.tokbuf.cursor.prev_kind)->prefix;
  if (!prefix_rule) {
    errorf("Expect expression after prefix %s.", token_enum_name(tu.tokbuf.cursor.prev_kind));
  }

  bool can_assign = precedence <= PREC_ASSIGNMENT;
  Operand left = prefix_rule(can_assign, expected);

  while (precedence <= get_rule(tu.tokbuf.cursor.cur_kind)->prec_for_infix) {
    advance();
    InfixFn infix_rule = get_rule(tu.tokbuf.cursor.prev_kind)->infix;
    if (!infix_rule) {
      errorf("Expect expression after infix %s.", token_enum_name(tu.tokbuf.cursor.prev_kind));
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
    errorf_cur("Expect newline after %s statement.", after_what);
  }
}

static Operand if_statement_cond_helper(void) {
  Operand cond = parse_expression(NULL);
  if (!type_is_condition(cond.type)) {
    errorf("Result of condition expression cannot be type %s.", type_as_str(cond.type));
  }
  consume_block_header("if/elif");
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
    parse_block();
    sq_i_jmp(after_block);

    sq_block_start(false_block);
    if (match(TOK_ELSE)) {
      consume_block_header("else");
      parse_block();
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
    TypeKind expr_type = type_kind(expr.type);
    if (type_eq(expr.type, type_range) || expr_type == TYPE_ARRAY || expr_type == TYPE_LIST ||
        expr_type == TYPE_STR) {
      IterationData itd = iteration_prolog(it_name, &expr);

      // TODO: maybe move this into iteration_prolog, but not needed for
      // comprehensions, so maybe it makes more sense here.
      tu.cur_scope->iteration_datas[tu.cur_scope->num_iteration_datas++] = &itd;

      consume_block_header("for");
      parse_block();
      iteration_epilog(itd);

      --tu.cur_scope->num_iteration_datas;

    } else {
      errorf("Unhandled for/in over type %s.", type_as_str(expr.type));
    }
  }
}

static void break_statement(void) {
  consume(TOK_NEWLINE, "Expecting newline after 'break'.");
  if (tu.cur_scope->num_iteration_datas == 0) {
    error("Cannot 'break' outside of loop.");
  }
  IterationData* itd = tu.cur_scope->iteration_datas[tu.cur_scope->num_iteration_datas - 1];

  for (int i = tu.cur_scope->num_exit_calls - 1; i >= itd->exit_call_mark_for_break_continue;
       --i) {
    ExitCall* ec = &tu.cur_scope->exit_call_stack[i];
    sq_i_call1(sq_type_void, ec->func, (SqCallArg){sq_type_long, ec->obj});
  }

  sq_i_jmp(itd->loop_done);
  sq_block_declare_and_start();
}

static void continue_statement(void) {
  consume(TOK_NEWLINE, "Expecting newline after 'continue'.");
  if (tu.cur_scope->num_iteration_datas == 0) {
    error("Cannot 'continue' outside of loop.");
  }
  IterationData* itd = tu.cur_scope->iteration_datas[tu.cur_scope->num_iteration_datas - 1];

  for (int i = tu.cur_scope->num_exit_calls - 1; i >= itd->exit_call_mark_for_break_continue;
       --i) {
    ExitCall* ec = &tu.cur_scope->exit_call_stack[i];
    sq_i_call1(sq_type_void, ec->func, (SqCallArg){sq_type_long, ec->obj});
  }

  sq_i_jmp(itd->loop_continue);
  sq_block_declare_and_start();
}

//   with EXPR as TARGET:
//       BODY
//
// is the same as
//
//   manager = EXPR
//   ent = &type(manager).__enter__
//   ext = &type(manager).__exit__
//   TARGET = ent(manager)
//   try:
//       BODY
//   finally:
//       ext(manager)
//
// (except that we don't actually have try/finally, so it's that idea captured
// in normal control flow if there's a break/return/etc.)
//
static void with_statement(void) {
  Operand wobj = parse_expression(NULL);
  if (check(TOK_AS)) {
    error("todo; with as");
  }

  Sym* enter_func = lookup_memfn(wobj.type, glob.static_str___enter__);
  if (!enter_func) {
    errorf("Type %s does not define an __enter__ for being used in 'with'.",
           type_as_str(wobj.type));
  }
  Sym* exit_func = lookup_memfn(wobj.type, glob.static_str___exit__);
  if (!exit_func) {
    errorf("Type %s does not define an __exit__ for being used in 'with'.",
           type_as_str(wobj.type));
  }

  sq_i_call1(/*todo*/ sq_type_void, sqref_for_sym(enter_func),
             (SqCallArg){sq_type_long, operand_to_sqref_lval(&wobj)});

  push_exit_call(sqref_for_sym(exit_func), operand_to_sqref_lval(&wobj));

  // TODO: bind return to the 'as' target

  consume_block_header("with");

  parse_block();

  // TODO: handle break/continue!

  ExitCall* ec = &tu.cur_scope->exit_call_stack[--tu.cur_scope->num_exit_calls];
  sq_i_call1(sq_type_void, ec->func, (SqCallArg){sq_type_long, ec->obj});
}

static void print_statement(void) {
  Operand val = parse_expression(NULL);

  if (type_eq(val.type, type_str)) {
      sq_i_call1(sq_type_void, sq_ref_extern("PrintStr"),
                 (SqCallArg){sq_type_long, operand_to_sqref_lval(&val)});
  } else {
    // If __str__ exists for the type, call it, and then print the result.
    Sym* sym = lookup_memfn(val.type, glob.static_str___str__);
    if (sym) {
      Operand as_str = operand_rvalue_imm(
          type_str, sq_i_call1(tu.sq_type_str, sqref_for_sym(sym),
                               (SqCallArg){sq_type_long, operand_to_sqref_lval(&val)}));

      sq_i_call1(sq_type_void, sq_ref_extern("PrintStr"),
                 (SqCallArg){sq_type_long, operand_to_sqref_lval(&as_str)});
    } else {
      errorf("Don't know how to print type %s.", type_as_str(val.type));
    }
  }
  expect_end_of_statement("print");
}

static void check_statement(void) {
  Operand cond = parse_expression(NULL);
  if (!type_is_condition(cond.type)) {
    errorf("Result of check expression cannot be type %s.", type_as_str(cond.type));
  }
  ASSERT(type_kind(cond.type) == TYPE_BOOL && "todo, other types");

  SqBlock fail_block = sq_block_declare();
  SqBlock after_block = sq_block_declare();

  sq_i_jnz(operand_to_sqref_imm(&cond), after_block, fail_block);

  sq_block_start(fail_block);
  // TODO: file/line would be nice!
  sq_i_call0(sq_type_void, sq_ref_extern("CheckFailed"));

  sq_block_start(after_block);
  expect_end_of_statement("check");
}

static LastStatementType parse_block(void) {
  LastStatementType lst = LST_NON_RETURN;
  while (!check(TOK_DEDENT)) {
    lst = parse_statement(/*toplevel=*/false);
    if (lst != LST_NON_RETURN) {
      sq_block_declare_and_start();
      break;
    }
    skip_newlines();
  }

  consume(TOK_DEDENT, "Expect end of block.");
  return lst;
}

// TODO: decorators
static Sym* def_statement(void) {
  Type return_type = parse_type();
  if (type_is_none(return_type)) {
    return_type = type_void;
  }
  uint32_t function_start_offset = cur_offset();
  Str name = parse_name("Expect function name.");
  consume(TOK_LPAREN, "Expect '(' after function name.");

  Type param_types[MAX_FUNC_PARAMS];
  Str param_names[MAX_FUNC_PARAMS];
  bool is_nested = tu.num_scopes > 1;
  if (is_nested) {
    ASSERT(tu.scopes[tu.num_scopes - 1].is_function);
    ASSERT(tu.scopes[0].is_module);
  }
  uint32_t num_params =
      parse_func_params(is_nested, /*memfn_self=*/NULL, (Str){0}, param_types, param_names);

  consume_block_header("function body");

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

  return funcsym;
}

static Sym* foreign_statement(void) {
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
  return funcsym;
}

static Sym* on_statement(void) {
  Type on_type;
  Str on_type_name;
  if (tu.tokbuf.cursor.cur_kind >= TOK_BOOL && tu.tokbuf.cursor.cur_kind <= TOK_UINT) {
    on_type = basic_tok_to_type[tu.tokbuf.cursor.cur_kind];
    on_type_name = type_decl_name(on_type);
    advance();
  } else if (check(TOK_IDENT_TYPE)) {
    advance();
    on_type_name = str_from_previous();
    Sym* sym;
    ScopeResult scope_result = scope_lookup_recursive(on_type_name, &sym);
    if (scope_result == SCOPE_RESULT_UNDEFINED) {
      errorf("Undefined type %s.", cstr_copy(glob.arena, on_type_name));
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

  if (!is_foreign) {
    consume_block_header("function body");
  }

  TypeFuncFlags flags = TFF_MEMFN;
  if (is_foreign) {
    flags |= TFF_FOREIGN;
  }
  Type functype = type_function(param_types, num_params, return_type, flags);

  ASSERT(str_eq(on_type_name, type_decl_name(on_type)));
  Str full_name = memfn_name_from_type_name(on_type_name, func_name);
  Sym* funcsym = sym_new(SYM_FUNC, full_name, functype);
  funcsym->scope_decl = SSD_DECLARED_GLOBAL;

  if (!is_foreign) {
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

  return funcsym;
}

static Sym* struct_statement() {
  Str name = parse_type_name("Expect struct type name.");
  consume_block_header("struct");

  sq_type_struct_start(cstr_copy(glob.arena, name), 0);

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

    sq_type_add_field(type_to_sqtype(field_type));

    Str field_name = parse_name("Expect struct field name.");
    for (uint32_t i = 0; i < num_fields; ++i) {
      if (str_eq(field_names[i], field_name)) {
        errorf("Duplicate struct field name '%s'.", cstr_copy(glob.arena, field_name));
      }
    }
    field_names[num_fields] = field_name;

    if (match(TOK_EQ)) {
      // TODO: need to support compound_literal is_const evaluation here
      field_initializers[num_fields] = const_expression();
      have_initializers = true;
    } else {
      field_initializers[num_fields] = operand_none;
    }

    ++num_fields;

    do {
      consume(TOK_NEWLINE, "Expect newline after struct field.");
    } while (check(TOK_NEWLINE));
  }
  consume(TOK_DEDENT, "Expecting dedent after struct definition.");

  SqType sqtype = sq_type_struct_end();

  Type strukt = type_new_struct(name, num_fields, field_names, field_types, have_initializers);
  type_struct_set_sqtype(strukt, sqtype);

  if (have_initializers) {
    // Because we need to zero init fields, build this as if it was jitting into
    // a memory structure, and then use byte emission to build the data object.

    uint8_t* blob = arena_push(glob.arena, type_size(strukt), type_align(strukt));
    memset(blob, 0, type_size(strukt));
    for (uint32_t i = 0; i < num_fields; ++i) {
      if (!op_is_null(field_initializers[i])) {
        if (!op_is_const(field_initializers[i])) {
          errorf("Expecting constant initializer for field %s.",
                 cstr_copy(glob.arena, field_names[i]));
        }
        Type field_type = type_struct_field_type(strukt, i);
        if (!convert_operand(&field_initializers[i], field_type)) {
          errorf("Cannot convert initializer to type %s.", type_as_str(field_type));
        }
        // TODO: not 100% certain this is not copying garbage from the val field
        // out of the range of the size of type if it gets convert_operand'd.
        memcpy(blob + type_struct_field_offset(strukt, i), &field_initializers[i].val,
               type_size(field_type));
      }
    }

    sq_data_start(sq_linkage_default, cstr_copy(glob.arena, name));  // "_init"+name?
    for (uint32_t i = 0; i < type_size(strukt); ++i) {
      sq_data_byte(blob[i]);
    }
    SqSymbol init_sym = sq_data_end();

    type_struct_set_initializer_symbol(strukt, init_sym);

    ASSERT(!tu.cur_scope->is_function);
  }
  Sym* new = sym_new(SYM_TYPE, name, strukt);
  new->scope_decl = SSD_DECLARED_GLOBAL;
  return new;
}

static void parse_global_variable_statement(Type type) {
  Str name = parse_name("Expect variable or typed variable name.");

  Sym* sym = NULL;
  ScopeResult scope_result = scope_lookup_recursive(name, &sym);
  bool have_init;
  ASSERT(!type_is_none(type));
  have_init = match(TOK_EQ);
  uint32_t eq_offset = prev_offset();

  if (scope_result == SCOPE_RESULT_UNDEFINED) {
    if (have_init) {
      Operand op = const_expression();
      if (!op_is_const(op)) {
        error("Global initializers must be constants.");
      }
      if (!convert_operand(&op, type)) {
        errorf_offset(eq_offset, "Initializer cannot be converted from type %s to declared type %s.",
                      type_as_str(op.type), type_as_str(type));
      }
      make_global(SYM_VAR, name, type, &op.val);
    } else {
      make_global(SYM_VAR, name, type, NULL);
    }
  } else {
    ASSERT(scope_result == SCOPE_RESULT_GLOBAL);
    error("Cannot re-initialize an existing global.");
  }

  expect_end_of_statement("global variable declaration");
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
  for (int i = tu.cur_scope->num_exit_calls - 1; i >= 0; --i) {
    ExitCall* ec = &tu.cur_scope->exit_call_stack[i];
    sq_i_call1(sq_type_void, ec->func, (SqCallArg){sq_type_long, ec->obj});
  }

  Type func_ret = type_func_return_type(tu.cur_scope->func_sym->type);
  ASSERT(!type_is_none(func_ret));
  Operand op = operand_none;
  if (!type_eq(func_ret, type_void)) {
    op = parse_expression(NULL);
    if (!convert_operand(&op, func_ret)) {
      errorf("Cannot convert type %s to expected return type %s.", type_as_str(op.type),
             type_as_str(func_ret));
    }

    if (type_is_aggregate(func_ret)) {
      sq_i_ret(operand_to_sqref_lval(&op));
    } else {
      sq_i_ret(operand_to_sqref_imm(&op));
    }
    return LST_RETURN_VALUE;
  } else {
    consume(TOK_NEWLINE, "Expected newline after return in function with no return type.");
    sq_i_ret_void();
    return LST_RETURN_VOID;
  }
}

static void global_statement(void) {
  Str name = parse_name("Expect variable name after global.");

  Sym* sym;
  ScopeResult scope_result = scope_lookup_single(&tu.scopes[0], name, true, &sym);
  if (scope_result != SCOPE_RESULT_GLOBAL) {
    errorf("Undefined global '%.*s'.", str_len(name), str_raw_ptr(name));
  }
  Sym* new = sym_new(SYM_VAR, name, sym->type);
  new->scope_decl = SSD_DECLARED_GLOBAL;
  new->global = sym->global;
}

static void insert_into_impscope(Sym* sym) {
  ImportedSymbol is = {.name = sym->name, .kind = sym->kind, .type = sym->type};
  switch (sym->kind) {
    case SYM_VAR:
    case SYM_FUNC:
      // TODO: decorator or whatever for source name vs. external name
      is.extern_name = cstr_copy(glob.arena, sym->name);
      break;
    case SYM_CONST:
      error("todo;");
      break;
    case SYM_TYPE:
      // nothing extra
      break;
    case SYM_MODULE:
      error("todo; allow?");
      break;
    default:
      error("internal error");
  }

  DictInsert res =
      dict_insert(&tu.impscope->syms, &is, start_str_hash_func, start_str_eq_func,
                  sizeof(ImportedSymbol), _Alignof(ImportedSymbol));
  if (!res.inserted) {
    errorf("Duplicate top-level definition of '%.*s'.", (int)str_len(sym->name),
           str_raw_ptr(sym->name));
  }
}

static LastStatementType parse_statement(bool toplevel) {
  LastStatementType lst = LST_NON_RETURN;

  skip_newlines();

  // TODO: de-dupe this mess.
  switch (tu.tokbuf.cursor.cur_kind) {
    case TOK_IMPORT:
      advance();
      error("imports must appear before other declarations.");
      break;
    case TOK_DEF: {
      advance();
      Sym* sym = def_statement();
      if (toplevel) {
        insert_into_impscope(sym);
      }
      break;
    }
    case TOK_FOREIGN: {
      advance();
      if (!toplevel) error("foreign statement only allowed at top level.");
      Sym* sym = foreign_statement();
      insert_into_impscope(sym);
      break;
    }
    case TOK_ON: {
      advance();
      if (!toplevel) error("on statement only allowed at top level.");
      Sym* sym = on_statement();
      insert_into_impscope(sym);
      break;
    }
    case TOK_STRUCT:
      advance();
      if (!toplevel) error("struct statement only allowed at top level.");
      Sym* sym = struct_statement();
      insert_into_impscope(sym);
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
    case TOK_BREAK:
      advance();
      if (toplevel) error("break statement not allowed at top level.");
      break_statement();
      break;
    case TOK_CONTINUE:
      advance();
      if (toplevel) error("continue statement not allowed at top level.");
      continue_statement();
      break;
    case TOK_WITH:
      advance();
      if (toplevel) error("with statement not allowed at top level.");
      with_statement();
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
    case TOK_CHECK:
      advance();
      if (toplevel) error("todo; check statement at top level should be valid for consts.");
      check_statement();
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
        if (toplevel) {
          parse_global_variable_statement(var_type);
        } else {
          parse_variable_statement(var_type);
        }
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

static void declare_rt_foreign_memfn0(Type on, Type return_type, Str name) {
  Str memfn_name = memfn_name_from_type(on, name);
  Type param_types[] = { type_ptr(on) };
  Type functype =
      type_function(param_types, COUNTOF(param_types), return_type, TFF_MEMFN | TFF_FOREIGN);
  Sym* funcsym = sym_new(SYM_FUNC, memfn_name, functype);
  funcsym->scope_decl = SSD_DECLARED_GLOBAL;
}

static void declare_rt_foreign_memfn1(Type on, Type return_type, Str name, Type arg0) {
  Str memfn_name = memfn_name_from_type(on, name);
  Type param_types[] = { type_ptr(on), arg0 };
  Type functype =
      type_function(param_types, COUNTOF(param_types), return_type, TFF_MEMFN | TFF_FOREIGN);
  Sym* funcsym = sym_new(SYM_FUNC, memfn_name, functype);
  funcsym->scope_decl = SSD_DECLARED_GLOBAL;
}

static void declare_all_rt_foreigns(void) {
  declare_rt_foreign_memfn1(type_str, type_str, str_intern("join"), type_list(type_str));
  declare_rt_foreign_memfn1(type_str, type_bool, glob.static_str___eq__, type_str);
  declare_rt_foreign_memfn1(type_str, type_bool, glob.static_str___contains__, type_str);

  declare_rt_foreign_memfn0(type_bool, type_str, glob.static_str___str__);
  declare_rt_foreign_memfn0(type_codept, type_str, glob.static_str___str__);
  declare_rt_foreign_memfn0(type_i8, type_str, glob.static_str___str__);
  declare_rt_foreign_memfn0(type_u8, type_str, glob.static_str___str__);
  declare_rt_foreign_memfn0(type_i16, type_str, glob.static_str___str__);
  declare_rt_foreign_memfn0(type_u16, type_str, glob.static_str___str__);
  declare_rt_foreign_memfn0(type_i32, type_str, glob.static_str___str__);
  declare_rt_foreign_memfn0(type_u32, type_str, glob.static_str___str__);
  declare_rt_foreign_memfn0(type_i64, type_str, glob.static_str___str__);
  declare_rt_foreign_memfn0(type_u64, type_str, glob.static_str___str__);
  declare_rt_foreign_memfn0(type_float, type_str, glob.static_str___str__);
  declare_rt_foreign_memfn0(type_double, type_str, glob.static_str___str__);
  declare_rt_foreign_memfn0(type_str, type_str, glob.static_str___str__);
  declare_rt_foreign_memfn0(type_range, type_str, glob.static_str___str__);

  declare_rt_foreign_memfn1(type_range, type_bool, glob.static_str___contains__, type_i64);
}

static void parse_one_time_initialization_impl(Arena* main_arena, int verbose) {
  type_init(main_arena);
  glob.arena = main_arena;
  glob.generics_thunk_cache = dict_new(glob.arena, 128, sizeof(NameSymPair), _Alignof(NameSymPair));

  glob.static_str_main = str_intern_len("main", 4);
  glob.static_str___str__ = str_intern_len("__str__", 7);
  glob.static_str___contains__ = str_intern_len("__contains__", 12);
  glob.static_str___enter__ = str_intern_len("__enter__", 9);
  glob.static_str___eq__ = str_intern_len("__eq__", 6);
  glob.static_str___exit__ = str_intern_len("__exit__", 8);
  glob.static_str_ret = str_intern_len("$ret", 4);
  glob.static_str_up = str_intern_len("$up", 3);

  glob.op_null_ptr = operand_const(type_ptr(type_void), (Val){.p = 0});

  glob.uniq_counter = 0;

  glob.verbose = verbose;
}

static void parse_scan_for_imports(Str load_filename, ReadFileResult file) {
  TokenizedBuffer tb = {
      .filename = load_filename,
      .file_contents = (const char*)file.buffer,
      // In the case of "a.a." the worst case for offsets is the same as the number
      // of characters in the buffer.
      .token_offsets = (uint32_t*)base_mem_large_alloc(file.allocated_size * sizeof(uint32_t)),
      .cursor = (TokenCursor){-1, 0, 0, 0},
      .num_peeks = 0,
      .num_indents = 1,
      .import_dict = arena_push(glob.arena, sizeof(DictImpl), _Alignof(DictImpl)),
  };
  *tb.import_dict =
      dict_new(glob.arena, 32, sizeof(ImportNameAndModule), _Alignof(ImportNameAndModule)),
  tb.indent_levels[0] = 0;
  tb.num_tokens = lex_indexer(file.buffer, file.allocated_size, tb.token_offsets);
  token_init(file.buffer, tb.import_dict);
  if (glob.verbose > 2) {
    token_dump_offsets(tb.num_tokens, tb.token_offsets, file.file_size);
  }
  tu.tokbuf = tb;

  advance();

  for (;;) {
    skip_newlines();
    if (match(TOK_IMPORT)) {
      uint32_t string_offset = cur_offset();
      if (!match(TOK_STRING_QUOTED)) {
        error_offset(string_offset, "Expecting constant str as import argument.");
      }
      StrView strview = get_strview_for_offsets(prev_offset(), cur_offset());
      StrView inside_quotes = {strview.data + 1, strview.size - 2};
      while (inside_quotes.data[inside_quotes.size] != '"') {
        --inside_quotes.size;
      }

      // Need to push and pop around import, because main compiler uses `tu` directly.
      tb = tu.tokbuf;
      Scope* globscope = tu.cur_scope;
      ASSERT(tu.num_scopes == 1);

      Module newmod = module_add(inside_quotes);
      if (module_is_in_error(newmod)) {
        Str path = module_load_path(newmod);
        errorf("Couldn't open import, looking for '%.*s'.", (int)str_len(path), str_raw_ptr(path));
      }

      // Restore current module.
      tu.tokbuf = tb;
      tu.cur_scope = globscope;
      tu.num_scopes = 1;
      token_init(file.buffer, tb.import_dict);

      consume(TOK_NEWLINE, "Expecting newline after import.");

      //printf("import as: '%s'\n", cstr_copy(glob.arena, module_import_as(newmod)));

      Str import_as = module_import_as(newmod);
      ImportNameAndModule inam = {import_as, newmod};
      dict_insert(tb.import_dict, &inam, start_str_hash_func, start_str_eq_func,
                  sizeof(ImportNameAndModule), _Alignof(ImportNameAndModule));

      Sym* sym = sym_new(SYM_MODULE, import_as, type_module);
      sym->module = newmod;
      sym->scope_decl = SSD_DECLARED_GLOBAL;
    } else {
      break;
    }
  }
}

static void parse_impl(Arena* temp_arena, Module module) {
  tu.var_scope_arena = temp_arena;
  tu.num_scopes = 0;
  tu.cur_scope = NULL;
  tu.evaluating_const = 0;

  enter_module_scope();

  parse_scan_for_imports(module_load_path(module), module_read_file_result(module));

  SqConfiguration config = SQ_CONFIGURATION_DEFAULT;
  //config.target = SQ_TARGET_AMD64_APPLE;
  const char* output_copy = cstr_copy(temp_arena, module_output_path(module));
  config.output = fopen(output_copy, "wb");
  if (!config.output) {
    base_writef_stderr("Couldn't open '%s' for output.\n", output_copy);
    base_exit(1);
  }
  config.output_function = sqbe_callback_output_function;
  if (glob.verbose == 2) {
    config.debug_flags = "PT";
  } else if (glob.verbose > 3) {
    config.debug_flags = "PMNCFKAILSRT";
  }
  sq_init(&config);

  sq_type_struct_start("str", 8);
  sq_type_add_field(sq_type_long); // data
  sq_type_add_field(sq_type_long); // len
  tu.sq_type_str = sq_type_struct_end();

  sq_type_struct_start("list", 8);
  sq_type_add_field(sq_type_long); // data
  sq_type_add_field(sq_type_long); // size
  sq_type_add_field(sq_type_long); // capacity;
  tu.sq_type_list = sq_type_struct_end();

  sq_type_struct_start("range", 8);
  sq_type_add_field(sq_type_long); // start
  sq_type_add_field(sq_type_long); // stop
  sq_type_add_field(sq_type_long); // step
  tu.sq_type_range = sq_type_struct_end();


  declare_all_rt_foreigns();

  while (tu.tokbuf.cursor.cur_kind != TOK_EOF) {
    parse_statement(/*toplevel=*/true);
  }

  module_set_scope(module, tu.impscope);
  leave_scope();

  sq_shutdown();

  fclose(config.output);
}
