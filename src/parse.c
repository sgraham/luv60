#include "luv60.h"

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
} Val;

typedef enum SymScopeDecl {
  SSD_NONE,
  SSD_ASSUMED_GLOBAL,
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
    ir_ref ref;
    struct { // kind == SYM_FUNC
      void* addr;
      ir_ref ref2;  // upvals
    };
  };
  SymScopeDecl scope_decl;
} Sym;

#define MAX_FUNC_NESTING 16
#define MAX_VARIABLE_SCOPES 32
#define MAX_FUNC_PARAMS 32
#define MAX_STRUCT_FIELDS 64
#define MAX_PENDING_CONDS 32
#define MAX_UPVALS 32

typedef struct PendingCond {
  ir_ref iftrue;
} PendingCond;

typedef struct UpvalMap {
  Str names[MAX_UPVALS];
  Sym* sources[MAX_UPVALS];
  uint32_t offsets[MAX_UPVALS];
  int num_upvals;
  uint32_t alloc_size;
} UpvalMap;

typedef struct FuncData {
  Sym* sym;
  Sym* return_slot;
  PendingCond pending_conds[MAX_PENDING_CONDS];
  int num_pending_conds;
  ir_ctx ctx;
  uint64_t arena_saved_pos;
  UpvalMap upval_map;
  ir_ref upval_base;
} FuncData;

typedef struct SmallFlatNameSymMap {
  Str names[16];
  Sym syms[16];
  int num_entries;
} SmallFlatNameSymMap;

static void flat_name_map_init(SmallFlatNameSymMap* nm) {
  nm->num_entries = 0;
}

typedef struct VarScope VarScope;
struct VarScope {
  union {
    DictImpl sym_dict;  // when is_full_dict
    SmallFlatNameSymMap flat_map;
  };
  uint64_t arena_pos;
  bool is_function;
  bool is_module;
  bool is_full_dict;
};

typedef struct Parser {
  Arena* arena;
  Arena* var_scope_arena;
  const char* cur_filename;

  const char* file_contents;
  uint32_t num_tokens;
  uint32_t* token_offsets;
  uint32_t cur_token_index;

  TokenKind cur_kind;
  TokenKind prev_kind;
  TokenKind token_buffer[16];
  int num_buffered_tokens;
  int continuation_paren_level;
  int indent_levels[12];  // This is the maximum possible in lexer.
  int num_indents;

  FuncData funcdatas[MAX_FUNC_NESTING];
  int num_funcdatas;
  VarScope varscopes[MAX_VARIABLE_SCOPES];
  int num_varscopes;

  FuncData* cur_func;
  VarScope* cur_var_scope;

  void* main_func_entry;
  int verbose;
  bool ir_only;
  int opt_level;
  ir_code_buffer code_buffer;
} Parser;

static Parser parser;

#define OPK_BIT_CONST 0x1
#define OPK_BIT_RVAL_REF 0x2
#define OPK_BIT_LVAL_REF 0x4
#define OPK_BIT_ADDR 0x8
#define OPK_BIT_SECOND_REF 0x10

typedef enum OpKind {
  OPK_CONST = OPK_BIT_CONST,
  OPK_REF_RVAL = OPK_BIT_RVAL_REF,
  OPK_REF_RVAL_ADDR = OPK_BIT_RVAL_REF | OPK_BIT_ADDR,
  OPK_REF_RVAL_ADDR_BOUND_FUNC = OPK_BIT_SECOND_REF | OPK_BIT_RVAL_REF | OPK_BIT_ADDR,
  OPK_REF_LVAL_ADDR = OPK_BIT_LVAL_REF | OPK_BIT_ADDR,
} OpKind;

typedef struct Operand {
  OpKind kind;
  Type type;
  union {
    Val val;
    ir_ref ref;
  };
  ir_ref ref2; // Extra ref, used for fat function pointers (addr & $up)
} Operand;

static inline FORCE_INLINE bool op_is_const(Operand op) {
  return op.kind == OPK_CONST;
}

static inline FORCE_INLINE bool op_is_addr(Operand op) {
  return op.kind & OPK_BIT_ADDR;
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

static ir_type type_to_ir_type(Type type) {
  if (type_is_aggregate(type)) {
    return IR_U64;
  }
  switch (type_kind(type)) {
    case TYPE_VOID:
      return IR_VOID;
    case TYPE_BOOL:
      return IR_BOOL;
    case TYPE_U8:
      return IR_U8;
    case TYPE_U16:
      return IR_U16;
    case TYPE_U32:
      return IR_U32;
    case TYPE_U64:
      return IR_U64;
    case TYPE_I8:
      return IR_I8;
    case TYPE_I16:
      return IR_I16;
    case TYPE_I32:
      return IR_I32;
    case TYPE_I64:
      return IR_I64;
    case TYPE_DOUBLE:
      return IR_DOUBLE;
    case TYPE_FLOAT:
      return IR_FLOAT;
    case TYPE_PTR:
      if (type_kind(type_ptr_subtype(type)) == TYPE_VOID) {
        return IR_U64;
      }
    default:
      base_writef_stderr("type_to_ir_type: %s\n", type_as_str(type));
      ASSERT(false && "todo");
      abort();
  }
}

typedef enum ScopeResult {
  SCOPE_RESULT_GLOBAL,
  SCOPE_RESULT_UNDEFINED,
  SCOPE_RESULT_LOCAL,
  SCOPE_RESULT_PARAMETER,
  SCOPE_RESULT_UPVALUE,
  NUM_SCOPE_RESULTS,
} ScopeResult;

static ScopeResult scope_lookup(Str name, Sym** sym, SymScopeDecl* scope_decl);

#if 0
static Operand operand_sym(Type type, LqSymbol lqsym) {
  return (Operand){.type = type, .opkind = OpKindSymbol, .lqsym = lqsym};
}
#endif

static Operand operand_lvalue(Type type, ir_ref ref) {
  return (Operand){.kind = OPK_REF_LVAL_ADDR, .type = type, .ref = ref};
}

static Operand operand_rvalue_addr(Type type, ir_ref ref) {
  return (Operand){.kind = OPK_REF_RVAL_ADDR, .type = type, .ref = ref};
}

static Operand operand_rvalue_addr_bound_function(Type type, ir_ref ref, ir_ref ref2) {
  return (Operand){.kind = OPK_REF_RVAL_ADDR_BOUND_FUNC, .type = type, .ref = ref, .ref2 = ref2};
}

static Operand operand_rvalue_imm(Type type, ir_ref ref) {
  return (Operand){.kind = OPK_REF_RVAL, .type = type, .ref = ref};
}

static Operand operand_const(Type type, Val val) {
  return (Operand){.kind = OPK_CONST, .type = type, .val = val};
}

static inline uint32_t cur_offset(void) {
  return parser.token_offsets[parser.cur_token_index];
}

static inline uint32_t prev_offset(void) {
  return parser.token_offsets[parser.cur_token_index - 1];
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

static ir_ref operand_to_irref_imm(Operand* op) {
  switch (op->kind) {
    case OPK_CONST: {
      switch (type_kind(op->type)) {
        case TYPE_BOOL:
          return ir_CONST_BOOL(op->val.b);
        case TYPE_U8:
          return ir_CONST_U8(op->val.u8);
        case TYPE_I8:
          return ir_CONST_I8(op->val.i8);
        case TYPE_U16:
          return ir_CONST_U16(op->val.u16);
        case TYPE_I16:
          return ir_CONST_I16(op->val.i16);
        case TYPE_U32:
          return ir_CONST_U32(op->val.u32);
        case TYPE_I32:
          return ir_CONST_I32(op->val.i32);
        case TYPE_U64:
          return ir_CONST_U64(op->val.u64);
        case TYPE_I64:
          return ir_CONST_I64(op->val.i64);
        default:
          error("internal error: unexpected const type.");
      }
    }
    case OPK_REF_RVAL:
      return op->ref;
    case OPK_REF_RVAL_ADDR:
    case OPK_REF_RVAL_ADDR_BOUND_FUNC:  // assume something else will load ref2
    case OPK_REF_LVAL_ADDR:
      if (type_is_aggregate(op->type)) {
        // TODO: This seems questionable.
        return op->ref;
      }
      return ir_VLOAD(type_to_ir_type(op->type), op->ref);
  }
}

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
  ASSERT(parser.cur_var_scope);
  if (parser.cur_var_scope->is_full_dict) {
    NameSymPair nsp = {.name = name,
                      .sym = {
                          .kind = kind,
                          .name = name,
                          .type = type,
                      }};
    DictInsert res = dict_insert(&parser.cur_var_scope->sym_dict, &nsp, namesym_hash_func,
                                namesym_eq_func, sizeof(NameSymPair), _Alignof(NameSymPair));
    return &((NameSymPair*)dict_rawiter_get(&res.iter))->sym;
  } else {
    SmallFlatNameSymMap* nm = &parser.cur_var_scope->flat_map;
    int count = nm->num_entries;

    if (count == COUNTOFI(nm->names)) {
      // flat_map is full, 'rehash' into full dict

      // Can't immediately put into cur_var_scope because the flat_map and
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
      parser.cur_var_scope->is_full_dict = true;
      parser.cur_var_scope->sym_dict = new_dict;

      // Call the other branch to actually insert the new sym.
      return sym_new(kind, name, type);
    }

    nm->names[nm->num_entries] = name;
    nm->syms[nm->num_entries] = (Sym){.kind = kind, .name = name, .type = type};
    Sym* ret = &nm->syms[nm->num_entries++];
    return ret;
  }
}

static void print_i32_impl(int32_t val) {
  printf("%d\n", val);
}

static void print_i32(Operand* op) {
  ir_ref addr = ir_CONST_ADDR(print_i32_impl);
  ir_CALL_1(IR_VOID, addr, operand_to_irref_imm(op));
}

static void print_bool_impl(uint8_t val) {
  printf("%s\n", val ? "true" : "false");
}

static void print_bool(Operand* op) {
  ir_ref addr = ir_CONST_ADDR(print_bool_impl);
  ir_CALL_1(IR_VOID, addr, operand_to_irref_imm(op));
}

static void print_str_impl(RuntimeStr str) {
  printf("%.*s\n", (int)str.length, str.data);
}

static void print_str(Operand* op) {
  // This is really !(AARCH64 || SYSV): 16 byte argument passed as pointer
  // rather than two words.
#if OS_WINDOWS
  ir_ref addr = ir_CONST_ADDR(print_str_impl);
  ir_CALL_1(IR_VOID, addr, operand_to_irref_imm(op));
#else
  ir_ref addr = ir_CONST_ADDR(print_str_impl);
  ir_ref data = ir_LOAD(IR_ADDR, op->ref);
  ir_ref size = ir_LOAD(IR_I64, ir_ADD_OFFSET(op->ref, 8));
  ir_CALL_2(IR_VOID, addr, data, size);
#endif
}

static void print_range_impl(RuntimeRange range) {
  if (range.step == 1) {
    printf("range(%" PRIi64 ", %" PRIi64 ")\n", range.start, range.stop);
  } else {
    printf("range(%" PRIi64 ", %" PRIi64 ", %" PRIi64 ")\n", range.start, range.stop, range.step);
  }
}

static void print_range(Operand* op) {
  ir_ref addr = ir_CONST_ADDR(print_range_impl);
  ir_CALL_1(IR_VOID, addr, operand_to_irref_imm(op));
}

static void initialize_aggregate(ir_ref base_addr, Type type) {
  size_t size = type_size(type);
  if (type_struct_has_initializer(type)) {
    ir_ref memcpy_addr = ir_CONST_ADDR(memcpy);
    ir_ref default_blob = ir_CONST_ADDR(type_struct_initializer_blob(type));
    ir_CALL_3(IR_VOID, memcpy_addr, base_addr, default_blob, ir_CONST_U64(size));
  } else {
    ir_ref memset_addr = ir_CONST_ADDR(memset);
    ir_CALL_3(IR_VOID, memset_addr, base_addr, ir_CONST_U8(0), ir_CONST_U64(size));
  }
}

static Sym* make_local_and_alloc(SymKind kind, Str name, Type type, Operand* initial_value) {
  Sym* new = sym_new(kind, name, type);
  // TODO: figure out str/range
  if (type_is_aggregate(type) && type_kind(type) != TYPE_STR && type_kind(type) != TYPE_RANGE) {
    if (initial_value) {
      if (!type_eq(initial_value->type, type)) {
        errorf("Cannot initialize aggregate type %s with type %s.",
               type_as_str(initial_value->type), type_as_str(type));
      }
      // We're stealing this value, I think it's OK though for aggregates, and
      // they have to have been just created (?). Alternatively, memcpy to a new
      // alloca, but I think that probably won't be optimized away.
      new->ref = initial_value->ref;
      initial_value->ref = 0;
    } else {
      uint32_t size = type_size(type);
      new->ref = ir_ALLOCA(ir_CONST_U64(size));
      initialize_aggregate(new->ref, type);
    }
  } else {
    new->ref = ir_VAR(type_to_ir_type(type),
#if BUILD_DEBUG
                      cstr_copy(parser.arena, name)
#else
                      ""
#endif
    );
    if (initial_value) {
      ir_VSTORE(new->ref, operand_to_irref_imm(initial_value));
    } else {
      ir_val irval;
      irval.u64 = 0;
      ir_VSTORE(new->ref, ir_const(_ir_CTX, irval, type_to_ir_type(type)));
    }
  }
  new->scope_decl = SSD_DECLARED_LOCAL;
  return new;
}

static Sym* make_param(Str name, Type type, int index) {
  Sym* new = sym_new(SYM_VAR, name, type);
  new->ref = ir_PARAM(type_to_ir_type(type),
#if BUILD_DEBUG
                      cstr_copy(parser.arena, name),
#else
                      "",
#endif
                      index + 1);
  new->scope_decl = SSD_DECLARED_PARAMETER;
  return new;
}

static void enter_scope(bool is_module, bool is_function) {
  parser.cur_var_scope = &parser.varscopes[parser.num_varscopes++];
  parser.cur_var_scope->arena_pos = arena_pos(parser.var_scope_arena);
  parser.cur_var_scope->is_function = is_function;
  parser.cur_var_scope->is_module = is_module;
  parser.cur_var_scope->is_full_dict = !is_function;
  if (parser.cur_var_scope->is_full_dict) {
    parser.cur_var_scope->sym_dict =
        dict_new(parser.var_scope_arena, 1 << 20, sizeof(NameSymPair), _Alignof(NameSymPair));
  } else {
    flat_name_map_init(&parser.cur_var_scope->flat_map);
  }
}

static void leave_scope(void) {
  arena_pop_to(parser.var_scope_arena, parser.cur_var_scope->arena_pos);
  --parser.num_varscopes;
  ASSERT(parser.num_varscopes >= 0);
  if (parser.num_varscopes == 0) {
    parser.cur_var_scope = NULL;
  } else {
    parser.cur_var_scope = &parser.varscopes[parser.num_varscopes - 1];
  }
}

static void enter_function(Sym* sym,
                           Str param_names[MAX_FUNC_PARAMS],
                           Type param_types[MAX_FUNC_PARAMS]) {
  bool is_nested = parser.num_funcdatas > 0;

  parser.cur_func = &parser.funcdatas[parser.num_funcdatas++];
  parser.cur_func->sym = sym;
  parser.cur_func->arena_saved_pos = arena_pos(arena_ir);
  parser.cur_func->upval_map.num_upvals = 0;
  enter_scope(/*is_module=*/false, /*is_function=*/true);

  ir_consistency_check();

  uint32_t opts = parser.opt_level ? IR_OPT_FOLDING : 0;
  if (parser.opt_level == 2) {
    opts |= IR_OPT_MEM2SSA;
  }
  ir_init(_ir_CTX, IR_FUNCTION | opts, 4096, 4096);
  parser.cur_func->ctx.code_buffer = &parser.code_buffer;
  ir_START();

  uint32_t num_params = type_func_num_params(sym->type);
  Sym* param_syms[MAX_FUNC_PARAMS];
  for (uint32_t i = 0; i < num_params; ++i) {
    param_syms[i] = make_param(param_names[i], type_func_param(sym->type, i), i);
  }

  // Allocation of the VAR for return_slot must be after all PARAMs.
  // https://github.com/dstogov/ir/issues/103.
  Type ret_type = type_func_return_type(sym->type);
  parser.cur_func->ctx.ret_type = type_to_ir_type(ret_type);
  if (type_eq(ret_type, type_void)) {
    parser.cur_func->return_slot = NULL;
  } else {
    parser.cur_func->return_slot =
        make_local_and_alloc(SYM_VAR, str_intern_len("$ret", 4), ret_type, NULL);
  }

  if (is_nested) {
    ASSERT(str_eq(param_syms[0]->name, str_intern_len("$up", 3)));
    ASSERT(type_kind(param_syms[0]->type) == TYPE_PTR);
    ASSERT(type_eq(type_ptr_subtype(param_syms[0]->type), type_void));
    parser.cur_func->upval_base = param_syms[0]->ref;
  }
}

static void leave_function(void) {
  Type ret_type = type_func_return_type(parser.cur_func->sym->type);
  if (type_eq(ret_type, type_void)) {
    ir_RETURN(IR_UNUSED);
  } else {
    ir_RETURN(ir_VLOAD(type_to_ir_type(ret_type), parser.cur_func->return_slot->ref));
  }

  if (parser.verbose) {
    ir_save(_ir_CTX, -1, stderr);
#if BUILD_DEBUG
    FILE* f = fopen("tmp.dot", "wb");
    ir_dump_dot(_ir_CTX, cstr_copy(parser.arena, parser.cur_func->sym->name), f);
    base_writef_stderr("Wrote tmp.dot\n");
    fclose(f);
#endif
  }

#if BUILD_DEBUG
  if (!ir_check(_ir_CTX)) {
    base_writef_stderr("ir_check failed, not compiling\n");
    base_exit(1);
  }
#endif

#if ENABLE_CODE_GEN
  if (!parser.ir_only) {
    size_t size = 0;
    void* entry = ir_jit_compile(_ir_CTX, /*opt=*/parser.opt_level, &size);
    if (entry) {
      if (parser.verbose) {
        fprintf(stderr, "=> codegen to %zu bytes at %p for '%s'\n", size, entry,
                cstr_copy(parser.arena, parser.cur_func->sym->name));
#  if !OS_WINDOWS  // TODO: don't have capstone or ir_disasm on win32 right now
        ir_disasm_init();
        ir_disasm(cstr_copy(parser.arena, parser.cur_func->sym->name), entry, size, false, _ir_CTX,
                  stderr);
        ir_disasm_free();
#  endif
      }
      if (str_eq(parser.cur_func->sym->name, str_intern("main"))) {
        parser.main_func_entry = entry;
      }
    } else {
      base_writef_stderr("compilation failed '%s'\n",
                         cstr_copy(parser.arena, parser.cur_func->sym->name));
    }
    parser.cur_func->sym->addr = entry;
  }
#endif

  ir_free(_ir_CTX);

  arena_pop_to(arena_ir, parser.cur_func->arena_saved_pos);

  // This is pointing the nested one, but we have to "pop" it to make the
  // capture code be generated in the parent, so we're pointing at a the
  // "deallocated" UpvalMap (not really, just num_funcdatas decremented) from
  // here until the end of this function.
  UpvalMap* uvm = &parser.cur_func->upval_map;
  Sym* child_func = parser.cur_func->sym;
  bool is_nested = parser.num_funcdatas > 1;

  --parser.num_funcdatas;
  ASSERT(parser.num_funcdatas >= 0);
  if (parser.num_funcdatas == 0) {
    parser.cur_func = NULL;
  } else {
    parser.cur_func = &parser.funcdatas[parser.num_funcdatas - 1];
  }

  if (is_nested) {
    ir_ref upval_data = ir_ALLOCA(ir_CONST_U64(uvm->alloc_size));
    child_func->ref2 = upval_data;
    for (int i = 0; i < uvm->num_upvals; ++i) {
      // TODO: aggregates, etc.
      Sym* sym = uvm->sources[i];
      if (type_kind(sym->type) == TYPE_FUNC) {
        ir_STORE(ir_ADD_OFFSET(upval_data, uvm->offsets[i]), ir_CONST_ADDR(sym->addr));
      } else {
        // This isn't an Operand because they were only evaluated in the context
        // of the nested function, not the parent function, so we need to do
        // another reference in this context. Doing another repeated
        // scope_lookup() on the name wouldn't be correct because the inner
        // function could do a `global`.
        // TODO: VLOAD isn't correct for aggregates
        // ... Hmm, it's a bit more complicated than that for higher levels of
        // nesting. In nested_two_levels.luv, when we pop out of inner, the sym
        // we've tried to store in UpvalMap has ir_refs that are valid in the
        // outer-most function (main), but not in middle() where we need to do
        // the capture to inner()s upval block.
        //
        // So, even though middle() doesn't directly reference |x|, it needs to
        // capture |x| in its upvals so that the capture at the defintion point
        // of inner() can capture both |x| (now a copy of a copy) and |y|, the
        // local definition in middle().
        //
        // Additionally, the lookup style won't be the same, the capture of |x| at
        // middle()s capture point will be a VLOAD, but the capture of |x| at inner()s
        // capture point will need to do what load_upval() does to load it from
        // middle()s upval block.
        //
        // When we load_upval() of |x| in inner() we need to also walk up the
        // function stack and load_upval() in each of the parent frames until we
        // reach the function where the thing being looked up was defined so
        // that it's propagated inwards. (actually, the function before the one
        // where it was defined, obviously it's already defined there.)
        //
        // It seems like the lookup time is where we should figure out the
        // correct method of actually loading the value at capture time too,
        // otherwise we're re-deriving it here awkwardly.
        if (sym->scope_decl == SSD_DECLARED_PARAMETER) {
          ir_STORE(ir_ADD_OFFSET(upval_data, uvm->offsets[i]), sym->ref);
        } else if (sym->scope_decl == SSD_DECLARED_LOCAL) {
          ir_STORE(ir_ADD_OFFSET(upval_data, uvm->offsets[i]),
                   ir_VLOAD(type_to_ir_type(sym->type), sym->ref));
        } else {
          error("Unhandled scope type in upval capture.");
        }
      }
    }
  }

  leave_scope();
}

static void advance(void) {
again:
  parser.prev_kind = parser.cur_kind;
  if (parser.num_buffered_tokens > 0) {
    parser.cur_kind = parser.token_buffer[--parser.num_buffered_tokens];
#if BUILD_DEBUG
    if (parser.verbose > 1) {
      base_writef_stderr("token %s (buffered)\n", token_enum_name(parser.cur_kind));
    }
#endif
    return;
  } else {
    ++parser.cur_token_index;
    ASSERT(parser.cur_token_index < parser.num_tokens);
    parser.cur_kind = token_categorize(parser.token_offsets[parser.cur_token_index]);
  }

  if (parser.cur_kind == TOK_NL) {
    goto again;
  }
  if (parser.cur_kind == TOK_NEWLINE_BLANK) {
    parser.cur_kind = TOK_NEWLINE;
  } else if (parser.cur_kind >= TOK_NEWLINE_INDENT_0 && parser.cur_kind <= TOK_NEWLINE_INDENT_40) {
    int n = (parser.cur_kind - TOK_NEWLINE_INDENT_0) * 4;
    if (n > parser.indent_levels[parser.num_indents - 1]) {
      parser.cur_kind = TOK_NEWLINE;
      parser.indent_levels[parser.num_indents++] = n;
      parser.token_buffer[parser.num_buffered_tokens++] = TOK_INDENT;
    } else if (n < parser.indent_levels[parser.num_indents - 1]) {
      parser.cur_kind = TOK_NEWLINE;
      while (parser.num_indents > 1 && parser.indent_levels[parser.num_indents - 1] > n) {
        parser.token_buffer[parser.num_buffered_tokens++] = TOK_DEDENT;
        --parser.num_indents;
      }
    } else {
      parser.cur_kind = TOK_NEWLINE;
    }
  }

#if BUILD_DEBUG
  if (parser.verbose > 1) {
      base_writef_stderr("token %s\n", token_enum_name(parser.cur_kind));
  }
#endif
}

static bool match(TokenKind tok_kind) {
  if (parser.cur_kind != tok_kind) {
    return false;
  }
  advance();
  return true;
}

static bool check(TokenKind tok_kind) {
  return parser.cur_kind == tok_kind;
}

static bool peek(TokenKind tok_kind) {
  TokenKind old_cur = parser.cur_kind;
  TokenKind old_prev = parser.prev_kind;
  advance();

  bool result = parser.cur_kind == tok_kind;

  // semi-retreat, but keep categorization by buffering it.
  parser.token_buffer[parser.num_buffered_tokens++] = parser.cur_kind;
  parser.cur_kind = old_cur;
  parser.prev_kind = old_prev;

  return result;
}

static void consume(TokenKind tok_kind, const char* message) {
  if (parser.cur_kind == tok_kind) {
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
// probably reasoanble for more complex expressions though because then it's
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
  } else if (type_is_arithmetic(dest) && type_is_arithmetic(src)){
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
      // TODO: floats
      // TODO: enums
      TypeKind from_type_kind = type_kind(operand->type);
      TypeKind to_type_kind = type_kind(type);
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
    } else {
      ir_ref ref_to_adjust;
      if (op_is_addr(*operand)) {
        ref_to_adjust = operand_to_irref_imm(operand);
        operand->kind = OPK_REF_RVAL;
      } else {
        ref_to_adjust = operand->ref;
      }

      if (type_size(operand->type) > type_size(type)) {
        operand->ref = ir_TRUNC(type_to_ir_type(type), ref_to_adjust);
      } else if (type_size(operand->type) < type_size(type)) {
        if (type_is_signed(type)) {
          operand->ref = ir_SEXT(type_to_ir_type(type), ref_to_adjust);
        } else {
          operand->ref = ir_ZEXT(type_to_ir_type(type), ref_to_adjust);
        }
      } else {
        // This is int-to-int, probably not necessary? Not sure.
        operand->ref = ir_BITCAST(type_to_ir_type(type), ref_to_adjust);
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
      error("Expecting type of array or slice.");
    }
    if (count == 0) {
      error("todo; slice");
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

  if (parser.cur_kind >= TOK_BOOL && parser.cur_kind <= TOK_UINT) {
    Type t = basic_tok_to_type[parser.cur_kind];
    ASSERT(!type_is_none(t));
    advance();
    return t;
  }

  if (match(TOK_IDENT_TYPE)) {
    Sym* sym;
    SymScopeDecl scope_decl;
    Str type_name = str_from_previous();
    ScopeResult scope_result = scope_lookup(type_name, &sym, &scope_decl);
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
                                  Type out_types[MAX_FUNC_PARAMS],
                                  Str out_names[MAX_FUNC_PARAMS]) {
  uint32_t num_params = 0;
  bool require_more = false;
  if (is_nested) {
    out_types[num_params] = type_ptr(type_void);
    out_names[num_params] = str_intern_len("$up", 3);
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
  const TokenKind tok = parser.cur_kind;
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
  ir_ref lcond = ir_IF(operand_to_irref_imm(&left));
  ir_IF_FALSE(lcond);
  ir_ref if_false = ir_END();
  ir_IF_TRUE(lcond);

  Operand right = parse_precedence(PREC_OR, &type_bool);
  if (!type_is_condition(right.type)) {
    errorf("Right-hand side of or cannot be type %s.", type_as_str(right.type));
  }
  ir_ref if_true = ir_END();
  ir_ref rcond = operand_to_irref_imm(&right);
  ir_MERGE_2(if_false, if_true);
  ir_ref result = ir_PHI_2(IR_BOOL, ir_CONST_BOOL(false), rcond);
  return operand_rvalue_imm(type_bool, result);
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

static unsigned long long eval_binary_op_ull(ir_op op,
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

static long long eval_binary_op_ll(ir_op op, long long left, long long right) {
  switch (op) {
    case IR_MUL: {
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
    case IR_DIV:
      if (right == 0) {
        error("Divide by zero.");
        return 0;
      }
      return left / right;
    case IR_MOD:
      if (right == 0) {
        error("Divide by zero.");
        return 0;
      }
      return left % right;
    case IR_AND:
      return left & right;
    case IR_SHL: {
      long long required_bits = highest_bit_set(left) + right + 1;
      if (required_bits > 64) {
        errorf("%llu shifted left by %llu requires %llu bits.", left, right, required_bits);
      }
      return left << right;
    }
    case IR_SHR:
      error("internal error: SHR on signed.");
    case IR_SAR:
      return left >> right;
    case IR_ADD: {
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
    case IR_SUB: {
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
    case IR_OR:
      return left | right;
    case IR_XOR:
      return left ^ right;
    case IR_EQ:
      return left == right;
    case IR_NE:
      return left != right;
    case IR_LT:
      return left < right;
    case IR_LE:
      return left <= right;
    case IR_GT:
      return left > right;
    case IR_GE:
      return left >= right;
    default:
      error("internal error: unexpected const ir_op.");
  }
}

static Val eval_binary_op(ir_op op, Type type, Val left, Val right) {
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

static Operand resolve_binary_op(ir_op op, Operand left, Operand right, uint32_t loc) {
  ASSERT(type_eq(left.type, right.type));
  // It didn't really seem worth doing constant eval, but it's needed for array
  // sizes in particular, so we do some constant propagation through is_const
  // Operands.
  if (op_is_const(left) && op_is_const(right)) {
    return operand_const(left.type, eval_binary_op(op, left.type, left.val, right.val));
  } else {
    ir_type irt = type_to_ir_type(left.type);
    ir_ref result =
        ir_BINARY_OP(op, irt, operand_to_irref_imm(&left), operand_to_irref_imm(&right));
    return operand_rvalue_imm(left.type, result);
  }
}

static Operand resolve_cmp_op(ir_op op, Operand left, Operand right, uint32_t loc) {
  ASSERT(type_eq(left.type, right.type));
  if (op_is_const(left) && op_is_const(right)) {
    return operand_const(left.type, eval_binary_op(op, left.type, left.val, right.val));
  } else  {
    ir_ref result =
        ir_CMP_OP(op, operand_to_irref_imm(&left), operand_to_irref_imm(&right));
    return operand_rvalue_imm(type_bool, result);
  }
}

static Operand resolve_binary_arithmetic_op(ir_op op, Operand left, Operand right, uint32_t loc) {
  unify_arithmetic_operands(&left, &right);
  return resolve_binary_op(op, left, right, loc);
}

static Operand resolve_binary_cmp_op(ir_op op, Operand left, Operand right, uint32_t loc) {
  unify_arithmetic_operands(&left, &right);
  return resolve_cmp_op(op, left, right, loc);
}

static Operand parse_binary(Operand left, bool can_assign, Type* expected) {
  // Remember the operator.
  TokenKind op = parser.prev_kind;
  uint32_t op_offset = prev_offset();

  // Compile the right operand.
  Rule* rule = get_rule(op);
  Operand rhs = parse_precedence(rule->prec_for_infix + 1, expected);

  typedef struct IrOpPair {
    ir_op sign;
    ir_op unsign;
  } IrOpPair;
  static IrOpPair tok_to_cmp_op[NUM_TOKEN_KINDS] = {
      [TOK_EQEQ] = {IR_EQ, IR_EQ},
      [TOK_BANGEQ] = {IR_NE, IR_NE},
      [TOK_LEQ] = {IR_LE, IR_ULE},
      [TOK_LT] = {IR_LT, IR_ULT},
      [TOK_GEQ] = {IR_GE, IR_UGE},
      [TOK_GT] = {IR_GT, IR_UGT},
  };
  typedef struct IrOpAndErr {
    ir_op op;
    const char* err_msg;
  } IrOpAndErr;
  static IrOpAndErr tok_to_bin_op[NUM_TOKEN_KINDS] = {
      [TOK_PLUS] = {IR_ADD, "Cannot add %s to %s"},
      [TOK_MINUS] = {IR_SUB, "TODO %s %s"},
      [TOK_STAR] = {IR_MUL, "TODO %s %s"},
      [TOK_SLASH] = {IR_DIV, "TODO %s %s"},
      [TOK_PERCENT] = {IR_MOD, "TODO %s %s"},
      //[TOK_TILDE] = {IR_NOT,  // TODO
      [TOK_PIPE] = {IR_OR, "TODO %s %s"},
      [TOK_AMPERSAND] = {IR_AND, "TODO %s %s"},
      [TOK_CARET] = {IR_XOR, "TODO %s %s"},
      [TOK_LSHIFT] = {IR_SHL, "TODO %s %s"},
      // TOK_RSHIFT handled below to do SHR vs SAR
  };
  if (tok_to_cmp_op[op].sign /*anything nonzero in slot*/) {
    if (type_is_arithmetic(left.type) && type_is_arithmetic(rhs.type)) {
      if (!type_signs_match(left.type, rhs.type)) {
        errorf_offset(op_offset, "Comparison with different signs: %s and %s.",
                      type_as_str(left.type), type_as_str(rhs.type));
      } else {
        bool is_signed = type_is_signed(left.type);
        return resolve_binary_cmp_op(is_signed ? tok_to_cmp_op[op].sign : tok_to_cmp_op[op].unsign,
                                     left, rhs, op_offset);
      }
    } else {
      errorf_offset(op_offset, "Cannot compare %s and %s.", type_as_str(left.type),
                    type_as_str(rhs.type));
    }
    ir_op irop = type_is_unsigned(left.type) ? tok_to_cmp_op[op].unsign : tok_to_cmp_op[op].sign;
    ir_ref cmp = ir_CMP_OP(irop, operand_to_irref_imm(&left), operand_to_irref_imm(&rhs));
    return operand_rvalue_imm(type_bool, cmp);
  } else if (tok_to_bin_op[op].err_msg /* anything nonzero in slot*/) {
    if (type_is_arithmetic(left.type) && type_is_arithmetic(rhs.type)) {
      return resolve_binary_arithmetic_op(tok_to_bin_op[op].op, left, rhs, op_offset);
    } else {
      // TODO: special case str here

      errorf_offset(op_offset, tok_to_bin_op[op].err_msg, type_as_str(left.type),
                    type_as_str(rhs.type));
    }
  } else if (op == TOK_RSHIFT) {
    ir_op irop = type_is_unsigned(left.type) ? IR_SHR : IR_SAR;
    ir_ref result = ir_BINARY_OP(irop, type_to_ir_type(left.type), operand_to_irref_imm(&left),
                                 operand_to_irref_imm(&rhs));
    return operand_rvalue_imm(left.type, result);
  } else {
    ASSERT(false && "todo");
    return operand_null;
  }
}

static Operand parse_bool_literal(bool can_assign, Type* expected) {
  ASSERT(parser.prev_kind == TOK_FALSE || parser.prev_kind == TOK_TRUE);
  return operand_const(type_bool, (Val){.b = parser.prev_kind == TOK_FALSE ? 0 : 1});
}

static Operand parse_call(Operand left, bool can_assign, Type* expected) {
  if (can_assign && match_assignment()) {
    CHECK(false && "todo; returning address i think");
  }
  if (type_kind(left.type) != TYPE_FUNC) {
    errorf("Expected function type, but type is %s.", type_as_str(left.type));
  }
  bool is_nested = type_func_is_nested(left.type);
  ir_ref arg_values[MAX_FUNC_PARAMS];
  uint32_t num_args = 0;

  if (is_nested) {
    ASSERT(op_has_ref2(left));
    arg_values[0] = left.ref2;
    ++num_args;
  }

  if (!check(TOK_RPAREN)) {
    for (;;) {
      if (num_args >= type_func_num_params(left.type)) {
        errorf("Passing >= %d arguments to function, but it expects %d.", num_args + 1,
               type_func_num_params(left.type));
      }
      uint32_t arg_offset = cur_offset();
      Type param_type = type_func_param(left.type, num_args);
      Operand arg = parse_precedence(PREC_OR, &param_type);
      if (!convert_operand(&arg, param_type)) {
        errorf_offset(arg_offset, "Call argument %d is type %s, but function expects type %s.",
                      num_args + 1, type_as_str(arg.type), type_as_str(param_type));
      }
      arg_values[num_args] = operand_to_irref_imm(&arg);
      ++num_args;
      if (!match(TOK_COMMA)) {
        break;
      }
    }
  }
  consume(TOK_RPAREN, "Expect ')' after arguments.");
  Type ret_type = type_func_return_type(left.type);
  return operand_rvalue_imm(ret_type,
                            ir_CALL_N(type_to_ir_type(ret_type), left.ref, num_args, arg_values));
}

static Operand parse_compound_literal(bool can_assign, Type* expected) {
  Type lit_type;
  Str type_name = str_from_previous();

  Sym* sym;
  SymScopeDecl scope_decl;
  ScopeResult scope_result = scope_lookup(type_name, &sym, &scope_decl);
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
  ir_ref base_addr = ir_ALLOCA(ir_CONST_U64(lit_size));
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
    ir_STORE(ir_ADD_OFFSET(base_addr, field_offset), operand_to_irref_imm(&field_values[i]));
  }

  return operand_rvalue_addr(lit_type, base_addr);
}

static Operand parse_dict_literal(bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
  return operand_null;
}

static Operand parse_dot(Operand left, bool can_assign, Type* expected) {
  // TODO: package, and maybe const or types after . ?
  Str name = parse_name("Expect property name after '.'.");
  uint32_t name_offset = prev_offset();

  if (can_assign && match_assignment()) {
    if (type_kind(left.type) == TYPE_STRUCT) {
      uint32_t field_offset;
      Type field_type;
      if (type_struct_find_field_by_name(left.type, name, &field_type, &field_offset)) {
        Operand rhs_value = parse_expression(expected);
        ir_STORE(ir_ADD_OFFSET(operand_to_irref_imm(&left), field_offset),
                              operand_to_irref_imm(&rhs_value));
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
    // TODO: ptr following down left

    if (type_kind(left.type) == TYPE_STRUCT) {
      uint32_t field_offset;
      Type field_type;
      if (type_struct_find_field_by_name(left.type, name, &field_type, &field_offset)) {
        ir_ref ref = ir_LOAD(type_to_ir_type(field_type),
                             ir_ADD_OFFSET(operand_to_irref_imm(&left), field_offset));
        return operand_rvalue_imm(field_type, ref);
      }

      // Not an error yet; could be a memfn below.
    }

    // TODO: package, union, special memfn on array/slice/dict, general memfn
    errorf("Cannot get field from type.");
  }
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
  ASSERT(false && "not implemented");
  return operand_null;
}

static Operand parse_list_literal_or_compr(bool can_assign, Type* expected) {
  OpVec elems;
  opv_init(&elems, parser.arena);

  if (!match(TOK_RSQUARE)) {
    for (;;) {
      if (elems.size > 0 && check(TOK_RSQUARE)) {
        // Allow trailing comma.
        break;
      }

      Operand elem = parse_expression(NULL);
      opv_append(&elems, elem);

      if (match(TOK_FOR)) {
        ASSERT(false && "todo; list compr");
        abort();
      } else if (!match(TOK_COMMA)) {
        break;
      }
    }
    consume(TOK_RSQUARE, "Expect ']' to terminate list.");
  }

  if (elems.size == 0) {
    if (!expected) {
      error("Cannot deduce type of empty list with no explicit type on left-hand side.");
    }
    if (type_kind(*expected) == TYPE_SLICE || type_kind(*expected) == TYPE_ARRAY) {
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
    ir_ref arr_base = ir_ALLOCA(ir_CONST_U64(type_size(first_item.type) * elems.size));
    ir_STORE(arr_base, operand_to_irref_imm(&first_item));
    for (int i = 1; i < elems.size; ++i) {
      Operand next_item = opv_at(&elems, i);
      if (!convert_operand(&next_item, first_item.type)) {
        errorf("List item %d is of type %s which does not match type %s of first element.", i + 1,
               type_as_str(next_item.type), type_as_str(first_item.type));
      }
      ir_STORE(ir_ADD_OFFSET(arr_base, type_size(first_item.type) * i),
               operand_to_irref_imm(&next_item));
    }
    return operand_rvalue_imm(type_array(first_item.type, elems.size), arr_base);
  }
}

static Operand parse_null_literal(bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
  return operand_null;
}

static Operand parse_number(bool can_assign, Type* expected) {
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
  ir_ref lcond = ir_IF(operand_to_irref_imm(&left));
  ir_IF_TRUE(lcond);
  ir_ref if_true = ir_END();
  ir_IF_FALSE(lcond);

  Operand right = parse_precedence(PREC_OR, &type_bool);
  if (!type_is_condition(right.type)) {
    errorf("Right-hand side of or cannot be type %s.", type_as_str(right.type));
  }
  ir_ref if_false = ir_END();
  ir_ref rcond = operand_to_irref_imm(&right);
  ir_MERGE_2(if_true, if_false);
  ir_ref result = ir_PHI_2(IR_BOOL, ir_CONST_BOOL(true), rcond);
  return operand_rvalue_imm(type_bool, result);
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

  ir_ref range = ir_ALLOCA(ir_CONST_U32(sizeof(RuntimeRange)));
  ir_ref astart = range;
  ir_ref astop = ir_ADD_A(range, ir_CONST_ADDR(sizeof(int64_t)));
  ir_ref astep = ir_ADD_A(range, ir_CONST_ADDR(2 * sizeof(int64_t)));
  if (type_is_none(second.type)) {
    // range(0, first, 1)
    ir_STORE(astart, ir_CONST_I64(0));
    ir_STORE(astop, operand_to_irref_imm(&first));
    ir_STORE(astep, ir_CONST_I64(1));
  } else {
    // range(first, second, third || 1)
    ir_STORE(astart, operand_to_irref_imm(&first));
    ir_STORE(astop, operand_to_irref_imm(&second));
    if (type_is_none(third.type)) {
      ir_STORE(astep, ir_CONST_I64(1));
    } else {
      ir_STORE(astep, operand_to_irref_imm(&third));
    }
  }
  return operand_rvalue_addr(type_range, range);
}

static Operand parse_sizeof(bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
  return operand_null;
}

static ir_ref emit_string_obj(StrView str) {
  // TODO: I think IR doesn't do much with data? So the str bytes can go into
  // the intern table, and then the Str object probably needs a data segment
  // that lives with the code segment that we shove all these into.
  RuntimeStr* p = arena_push(parser.arena, sizeof(RuntimeStr) + str.size + 1, _Alignof(RuntimeStr));
  uint8_t* strp = (uint8_t*)(((RuntimeStr*)p) + 1);
  p->data = strp;
  memcpy(strp, str.data, str.size);
  p->length = str.size;
  return ir_CONST_ADDR(p);
}

static Operand parse_string(bool can_assign, Type* expected) {
  StrView strview = get_strview_for_offsets(prev_offset(), cur_offset());
  StrView inside_quotes = {strview.data + 1, strview.size - 2};
  if (memchr(strview.data, '\\', strview.size) != NULL) {  // worthwhile?
    // Mutates sourcse buffer!
    uint32_t new_len = str_process_escapes((char*)inside_quotes.data, inside_quotes.size);
    if (new_len == 0) {
      error("Invalid string escape.");
    }
    inside_quotes.size = new_len;
    return operand_rvalue_addr(type_str, emit_string_obj(inside_quotes));
  } else {
    return operand_rvalue_addr(type_str, emit_string_obj(inside_quotes));
  }
}

static Operand parse_string_interpolate(bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
  return operand_null;
}

static Operand parse_subscript(Operand left, bool can_assign, Type* expected) {
  ir_ref target_addr;
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
        case TYPE_SLICE:
        case TYPE_PTR:
        case TYPE_STR: {
          if (!type_is_integer(subscript.type)) {
            errorf("Cannot subscript using type %s.", type_as_str(subscript.type));
          }
          if (left_type_kind == TYPE_ARRAY) {
            ASSERT(op_is_addr(left));
            subtype = type_array_subtype(left.type);
            target_addr = ir_ADD_A(left.ref, ir_MUL_I64(ir_CONST_I64(type_size(subtype)),
                                                        operand_to_irref_imm(&subscript)));
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

  ASSERT(target_addr);
  ASSERT(!type_is_none(subtype));
  if (can_assign && match_assignment()) {
    Operand rhs = parse_expression(NULL); // TODO: do type here
    if (!convert_operand(&rhs, subtype)) {
      errorf("Cannot store type %s into %s.", type_as_str(rhs.type), type_as_str(left.type));
    }
    ir_STORE(target_addr, operand_to_irref_imm(&rhs));
    return operand_null;
  } else {
    return operand_rvalue_imm(subtype, ir_LOAD(type_to_ir_type(subtype), target_addr));
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
  TokenKind op_kind = parser.prev_kind;
  Operand expr = parse_precedence(PREC_UNARY, expected);
  if (op_kind == TOK_MINUS) {
    if (op_is_const(expr)) {
      return operand_const(expr.type, eval_unary_op(op_kind, expr.type, expr.val));
    } else {
      return operand_rvalue_imm(expr.type,
                                ir_NEG(type_to_ir_type(expr.type), operand_to_irref_imm(&expr)));
    }
  } else if (op_kind == TOK_NOT) {
    // TODO: const eval
    if (type_is_condition(expr.type)) {
      return operand_rvalue_imm(
          expr.type, ir_NOT(type_to_ir_type(expr.type), operand_to_irref_imm(&expr)));
    } else {
      errorf("Type %s cannot be used in a boolean not.", type_as_str(expr.type));
    }
  } else {
    error("unary operator not implemented");
  }
}

static Sym* find_in_scope(VarScope* scope, Str name) {
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
static ScopeResult scope_lookup(Str name, Sym** sym, SymScopeDecl* scope_decl) {
  *scope_decl = SSD_NONE;
  *sym = NULL;
  bool crossed_function = false;
  VarScope* cur_scope = parser.cur_var_scope;
  for (;;) {
    Sym* found_sym = find_in_scope(cur_scope, name);
    if (found_sym) {
      SymScopeDecl sd = found_sym->scope_decl;
      if (sd == SSD_ASSUMED_GLOBAL) {
        *scope_decl = SSD_ASSUMED_GLOBAL;
        return SCOPE_RESULT_UNDEFINED;
      } else if (sd == SSD_DECLARED_NONLOCAL) {
        *scope_decl = sd;
        ASSERT(false); abort();
        return SCOPE_RESULT_UPVALUE;
      } else if (sd == SSD_DECLARED_GLOBAL) {
        *scope_decl = sd;
        *sym = found_sym;
        return SCOPE_RESULT_GLOBAL;
      } else if (sd == SSD_DECLARED_PARAMETER) {
        *scope_decl = sd;
        *sym = found_sym;
        if (crossed_function) {
          return SCOPE_RESULT_UPVALUE;
        }
        return SCOPE_RESULT_PARAMETER;
      } else {
        *sym = found_sym;
        if (cur_scope->is_module) {
          return SCOPE_RESULT_GLOBAL;
        } else if (crossed_function) {
          return SCOPE_RESULT_UPVALUE;
        } else {
          return SCOPE_RESULT_LOCAL;
        }
      }
    }

    if (cur_scope->is_function) {
      crossed_function = true;
    }

    if (cur_scope == &parser.varscopes[0]) {
      break;
    }
    ASSERT(cur_scope >= &parser.varscopes[0] &&
           cur_scope <= &parser.varscopes[parser.num_varscopes - 1]);
    cur_scope--;  // parent
  }

  return SCOPE_RESULT_UNDEFINED;
}

static Operand load_upval(Str name, Sym* sym) {
  ASSERT(!str_is_none(name));
  ASSERT(sym);
  UpvalMap* uvm = &parser.cur_func->upval_map;
  int upval_index;
  for (upval_index = uvm->num_upvals - 1; upval_index >= 0; --upval_index) {
    if (str_eq(name, uvm->names[upval_index])) {
      ASSERT(sym == uvm->sources[upval_index]);  // i think?
      break;
    }
  }
  if (upval_index < 0) {
    // Didn't find it in the existing map, add a reference and then return it.
    if (uvm->num_upvals >= COUNTOFI(uvm->names)) {
      error("Too many upvals.");
    }
    upval_index = uvm->num_upvals++;

    uvm->alloc_size = ALIGN_UP(uvm->alloc_size, type_align(sym->type));

    uvm->names[upval_index] = name;
    uvm->sources[upval_index] = sym;
    uvm->offsets[upval_index] = uvm->alloc_size;

    uvm->alloc_size = type_size(sym->type);
  }

  Type type = uvm->sources[upval_index]->type;
  return operand_rvalue_imm(
      type, ir_LOAD(type_to_ir_type(type),
                    ir_ADD_OFFSET(parser.cur_func->upval_base, uvm->offsets[upval_index])));
}


static Operand parse_variable(bool can_assign, Type* expected) {
  Str target = str_from_previous();
  SymScopeDecl scope_decl = SSD_NONE;
  Sym* sym = NULL;
  ScopeResult scope_result = scope_lookup(target, &sym, &scope_decl);
  if (can_assign && match_assignment()) {
    TokenKind eq_kind = parser.prev_kind;
    TokenKind eq_offset = prev_offset();
    if (scope_result == SCOPE_RESULT_UNDEFINED && scope_decl == SSD_ASSUMED_GLOBAL) {
      errorf("Local variable '%s' referenced before assignment.", cstr_copy(parser.arena, target));
    } else if (scope_result == SCOPE_RESULT_LOCAL) {
      ASSERT(sym);
      Operand op = parse_expression(NULL);
      if (!convert_operand(&op, sym->type)) {
        errorf("Cannot assign type %s to type %s.", type_as_str(op.type), type_as_str(sym->type));
      }
      if (eq_kind == TOK_EQ) {
        ir_VSTORE(sym->ref, operand_to_irref_imm(&op));
        return operand_null;
      } else {
        error_offset(eq_offset, "Unhandled assignment type.");
      }
      return operand_null;
    } else if (scope_result != SCOPE_RESULT_LOCAL && scope_decl == SSD_NONE) {
      if (eq_kind == TOK_EQ) {
        // Variable declaration without a type.
        Operand op = parse_expression(NULL);
        make_local_and_alloc(SYM_VAR, target, op.type, &op);
        return operand_null;
      } else {
        error_offset(eq_offset,
                     "Cannot use an augmented assignment when implicitly declaring a local.");
      }
    } else if (scope_result == SCOPE_RESULT_PARAMETER) {
      error_offset(eq_offset, "Function parameters are immutable.");
    }
  } else {
    if (scope_result == SCOPE_RESULT_LOCAL) {
      if (type_kind(sym->type) == TYPE_FUNC) {
        if (type_func_is_nested(sym->type)) {
          return operand_rvalue_addr_bound_function(sym->type, ir_CONST_ADDR(sym->addr), sym->ref2);
        } else {
          return operand_rvalue_addr(sym->type, ir_CONST_ADDR(sym->addr));
        }
      } else {
        return operand_lvalue(sym->type, sym->ref);
      }
    } else if (scope_result == SCOPE_RESULT_PARAMETER) {
      return operand_rvalue_imm(sym->type, sym->ref);
    } else if (scope_result == SCOPE_RESULT_GLOBAL) {
      if (type_kind(sym->type) == TYPE_FUNC) {
        // Doesn't make sense in our use for GLOBAL to be bound I don't think.
        return operand_rvalue_addr(sym->type, ir_CONST_ADDR(sym->addr));
      } else {
        ASSERT(false && "global var");
        //return operand_lvalue(sym->type, gen_mir_op_reg(sym->ir_reg));
      }
    } else if (scope_result == SCOPE_RESULT_UPVALUE) {
      return load_upval(target, sym);
    } else {
      errorf("Undefined reference to '%s'.", cstr_copy(parser.arena, target));
    }
  }

  errorf("internal error in '%s'", __FUNCTION__);
  return operand_null;
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
  PrefixFn prefix_rule = get_rule(parser.prev_kind)->prefix;
  if (!prefix_rule) {
    errorf("Expect expression after prefix %s.", token_enum_name(parser.prev_kind));
  }

  bool can_assign = precedence <= PREC_ASSIGNMENT;
  Operand left = prefix_rule(can_assign, expected);

  while (precedence <= get_rule(parser.cur_kind)->prec_for_infix) {
    advance();
    InfixFn infix_rule = get_rule(parser.prev_kind)->infix;
    if (!infix_rule) {
      errorf("Expect expression after infix %s.", token_enum_name(parser.prev_kind));
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
    ir_ref cond = ir_IF(operand_to_irref_imm(&opcond));
    ir_IF_TRUE(cond);
    LastStatementType lst = parse_block();
    ir_ref iftrue = ir_END();
    if (lst == LST_RETURN_VALUE || lst == LST_RETURN_VOID) {
      ir_IF_FALSE(cond);
      // Push that we're in the FALSE block, with END of iftrue
      // When we get to the end of the outer block, END this false
      // and the MERGE iftrue, iffalse
      ASSERT(parser.cur_func->num_pending_conds < COUNTOFI(parser.cur_func->pending_conds));
      parser.cur_func->pending_conds[parser.cur_func->num_pending_conds++] = (PendingCond){iftrue};
    } else {
      bool no_more = false;
      ir_IF_FALSE(cond);
      if (match(TOK_ELSE)) {
        consume(TOK_COLON, "Expect ':' to start else.");
        consume(TOK_NEWLINE, "Expect newline after ':' to start else.");
        consume(TOK_INDENT, "Expect indent to start else.");
        LastStatementType lst = parse_block();
        if (lst == LST_RETURN_VALUE || lst == LST_RETURN_VOID) {
          ASSERT(false && "todo: return in else");
        }
        no_more = true;
      }
      ir_ref otherwise = ir_END();
      ir_MERGE_2(iftrue, otherwise);
      if (no_more) {
        break;
      }
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
      ASSERT(op_is_addr(expr));
      ir_ref astart = expr.ref;
      ir_ref astop = ir_ADD_A(expr.ref, ir_CONST_ADDR(sizeof(int64_t)));
      ir_ref astep = ir_ADD_A(expr.ref, ir_CONST_ADDR(2 * sizeof(int64_t)));

      ir_ref start = ir_LOAD_I64(astart);
      ir_ref stop = ir_LOAD_I64(astop);
      ir_ref step = ir_LOAD_I64(astep);

      ir_ref is_neg = ir_LT(step, ir_CONST_I64(0));

      // TODO: This probably needs work if the Range isn't trivial, start
      // should be using the Operand expr or something maybe
      Sym* it = make_local_and_alloc(SYM_VAR, it_name, type_i64, NULL);
      ir_VSTORE(it->ref, start);

      ir_ref loop = ir_LOOP_BEGIN(ir_END());

      ir_ref cur = ir_VLOAD(IR_I64, it->ref);
      // (is_neg ? cur > stop : cur < stop)
      ir_ref cond = ir_IF(ir_COND(IR_BOOL, is_neg, ir_GT(cur, stop), ir_LT(cur, stop)));
      ir_IF_TRUE(cond);

      consume(TOK_COLON, "Expect ':' to start for.");
      consume(TOK_NEWLINE, "Expect newline after ':' to start for.");
      consume(TOK_INDENT, "Expect indent to start for.");
      LastStatementType lst = parse_block();
      ASSERT(lst == LST_NON_RETURN && "todo; return from loop");

      ir_ref itval = ir_VLOAD(IR_I64, it->ref);
      ir_ref inc = ir_ADD_I64(itval, step);
      ir_VSTORE(it->ref, inc);

      ir_MERGE_SET_OP(loop, 2, ir_LOOP_END());
      ir_IF_FALSE(cond);
    } else {
      errorf("Unhandled for/in over type %s.", type_as_str(expr.type));
    }
  }
}

static void print_statement(void) {
  Operand val = parse_expression(NULL);
  (void)val;
  if (type_eq(val.type, type_str)) {
    print_str(&val);
  } else if (type_eq(val.type, type_range)) {
    print_range(&val);
  } else if (type_eq(val.type, type_bool)) {
    print_bool(&val);
  } else if (convert_operand(&val, type_i32)) {
    print_i32(&val);
  } else {
    errorf("TODO: don't know how to print type %s.", type_as_str(val.type));
  }
  expect_end_of_statement("print");
}

static LastStatementType parse_block(void) {
  LastStatementType lst = LST_NON_RETURN;
  while (!check(TOK_DEDENT)) {
    lst = parse_statement(/*toplevel=*/false);
    skip_newlines();
  }

  if (parser.cur_func->num_pending_conds) {
    PendingCond cond = parser.cur_func->pending_conds[--parser.cur_func->num_pending_conds];
    ir_ref other = ir_END();
    ir_MERGE_2(cond.iftrue, other);
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
  bool is_nested = parser.num_funcdatas > 0;
  uint32_t num_params = parse_func_params(is_nested, param_types, param_names);

  consume(TOK_COLON, "Expect ':' before function body.");
  consume(TOK_NEWLINE, "Expect newline before function body. (TODO: single line)");
  skip_newlines();
  consume(TOK_INDENT, "Expect indent before function body. (TODO: single line)");

  Type functype = type_function(param_types, num_params, return_type, is_nested);

  Sym* funcsym = sym_new(SYM_FUNC, name, functype);
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
        memcpy(blob + type_struct_field_offset(strukt, i), &field_initializers[i].val,
               type_size(field_type));
      }
    }
    type_struct_set_initializer_blob(strukt, blob);
  }
  sym_new(SYM_TYPE, name, strukt);
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
  Type func_ret = type_func_return_type(parser.cur_func->sym->type);
  ASSERT(!type_is_none(func_ret));
  Operand op = operand_null;
  if (!type_eq(func_ret, type_void)) {
    op = parse_expression(NULL);
    if (!convert_operand(&op, func_ret)) {
      errorf("Cannot convert type %s to expected return type %s.", type_as_str(op.type),
             type_as_str(func_ret));
    }
    ir_VSTORE(parser.cur_func->return_slot->ref, operand_to_irref_imm(&op));
    return LST_RETURN_VALUE;
  } else {
    consume(TOK_NEWLINE, "Expected newline after return in function with no return type.");
    return LST_RETURN_VOID;
  }
}

static LastStatementType parse_statement(bool toplevel) {
  LastStatementType lst = LST_NON_RETURN;

  skip_newlines();

  switch (parser.cur_kind) {
    case TOK_DEF:
      advance();
      def_statement();
      break;
    case TOK_STRUCT:
      advance();
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

static void* parse_impl(Arena* main_arena,
                   Arena* temp_arena,
                   const char* filename,
                   ReadFileResult file,
                   int verbose,
                   bool ir_only,
                   int opt_level) {
  type_init(main_arena);

  parser.arena = main_arena;
  parser.var_scope_arena = temp_arena;
  // In the case of "a.a." the worst case for offsets is the same as the number
  // of characters in the buffer.
  parser.token_offsets = (uint32_t*)base_mem_large_alloc(file.allocated_size * sizeof(uint32_t));
  parser.file_contents = (const char*)file.buffer;
  parser.cur_filename = filename;
  parser.cur_token_index = 0;
  parser.num_funcdatas = 0;
  parser.num_varscopes = 0;
  parser.cur_func = NULL;
  parser.cur_var_scope = NULL;
  parser.cur_token_index = -1;
  parser.indent_levels[0] = 0;
  parser.num_indents = 1;
  parser.num_buffered_tokens = 0;
  parser.main_func_entry = NULL;
  parser.verbose = verbose;
  parser.ir_only = ir_only;
  parser.opt_level = opt_level;

#if ENABLE_CODE_GEN
  size_t code_buffer_size = MiB(512);
  parser.code_buffer.start = ir_mem_mmap(code_buffer_size);
  ASSERT(parser.code_buffer.start);
  ir_mem_unprotect(parser.code_buffer.start, code_buffer_size);
  parser.code_buffer.end = (uint8_t*)parser.code_buffer.start + code_buffer_size;
  parser.code_buffer.pos = parser.code_buffer.start;
#endif

  enter_scope(/*is_module=*/true, /*is_function=*/false);

  parser.num_tokens = lex_indexer(file.buffer, file.allocated_size, parser.token_offsets);
  token_init(file.buffer);
  if (parser.verbose > 1) {
    token_dump_offsets(parser.num_tokens, parser.token_offsets, file.file_size);
  }
  advance();

  while (parser.cur_kind != TOK_EOF) {
    parse_statement(/*toplevel=*/true);
  }

#if ENABLE_CODE_GEN
  ir_mem_protect(parser.code_buffer.start, code_buffer_size);
#endif

  return parser.main_func_entry;
}
