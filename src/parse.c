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
    void* addr;  // kind == SYM_FUNC
  };
  SymScopeDecl scope_decl;
} Sym;

#define MAX_FUNC_NESTING 16
#define MAX_VARIABLE_SCOPES 32
#define MAX_FUNC_PARAMS 32
#define MAX_PENDING_CONDS 32

typedef struct PendingCond {
  ir_ref iftrue;
} PendingCond;

typedef struct FuncData {
  Sym* sym;
  Sym* return_slot;
  PendingCond pending_conds[MAX_PENDING_CONDS];
  int num_pending_conds;
  ir_ctx ctx;
  uint64_t arena_saved_pos;
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
  bool verbose;
  bool ir_only;
  int opt_level;
  ir_code_buffer code_buffer;
} Parser;

static Parser parser;

typedef struct Operand {
  Type type;
  ir_ref ref;
  bool ref_is_addr;
  bool is_lvalue;
  bool is_const;
} Operand;

static Operand operand_null;

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
  return (Operand){
      .type = type, .ref = ref, .ref_is_addr = true, .is_lvalue = true, .is_const = false};
}

static Operand operand_rvalue(Type type, ir_ref ref) {
  return (Operand){
      .type = type, .ref = ref, .ref_is_addr = true, .is_lvalue = false, .is_const = false};
}

static Operand operand_rvalue_imm(Type type, ir_ref ref) {
  return (Operand){
      .type = type, .ref = ref, .ref_is_addr = false, .is_lvalue = false, .is_const = false};
}

static Operand operand_const(Type type, ir_ref ref) {
  return (Operand){.type = type, .ref = ref, .ref_is_addr = false, .is_const = true};
}

#if 0
void store_into_operand(ir_ref into, Operand* op) {
}
#endif

static ir_ref load_operand_if_necessary(Operand* op) {
  if (op->ref_is_addr && !type_is_aggregate(op->type)) {
    return ir_VLOAD(type_to_ir_type(op->type), op->ref);
  } else {
    return op->ref;
  }
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
    nm->names[nm->num_entries] = name;
    nm->syms[nm->num_entries] = (Sym){.kind = kind, .name = name, .type = type};
    Sym* ret = &nm->syms[nm->num_entries++];
    ASSERT(nm->num_entries < COUNTOFI(nm->names) && "todo; 'rehash' into is_full_dict");
    return ret;
  }
}

static void init_module_globals(void) {
}

static void print_i32_impl(int32_t val) {
  printf("%d\n", val);
}

static void print_i32(Operand* op) {
  ir_ref addr = ir_CONST_ADDR(print_i32_impl);
  ir_CALL_1(IR_VOID, addr, load_operand_if_necessary(op));
}

static void print_str_impl(RuntimeStr str) {
  printf("%.*s\n", (int)str.length, str.data);
}

static void print_str(Operand* op) {
  // This is really !(AARCH64 || SYSV): 16 byte argument passed as pointer
  // rather than two words.
#if OS_WINDOWS
  ir_ref addr = ir_CONST_ADDR(print_str_impl);
  ir_CALL_1(IR_VOID, addr, load_operand_if_necessary(op));
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
  ir_CALL_1(IR_VOID, addr, load_operand_if_necessary(op));
}

static Sym* make_local_and_alloc(SymKind kind, Str name, Type type) {
  Sym* new = sym_new(kind, name, type);
  new->ref = ir_VAR(type_to_ir_type(type),
#if BUILD_DEBUG
                    cstr_copy(parser.arena, name)
#else
                    ""
#endif
  );
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
  parser.cur_func = &parser.funcdatas[parser.num_funcdatas++];
  parser.cur_func->sym = sym;
  parser.cur_func->arena_saved_pos = arena_pos(arena_ir);
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
  for (uint32_t i = 0; i < num_params; ++i) {
    make_param(param_names[i], type_func_param(sym->type, i), i);
  }

  // Allocation of the VAR for return_slot must be after all PARAMs.
  // https://github.com/dstogov/ir/issues/103.
  Type ret_type = type_func_return_type(sym->type);
  parser.cur_func->ctx.ret_type = type_to_ir_type(ret_type);
  if (type_eq(ret_type, type_void)) {
    parser.cur_func->return_slot = NULL;
  } else {
    parser.cur_func->return_slot = make_local_and_alloc(SYM_VAR, str_intern("$ret"), ret_type);
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
        fprintf(stderr, "=> codegen to %zu bytes for '%s'\n", size,
                cstr_copy(parser.arena, parser.cur_func->sym->name));
#if  1//if !OS_WINDOWS  // TODO: don't have capstone or ir_disasm on win32 right now
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

  --parser.num_funcdatas;
  ASSERT(parser.num_funcdatas >= 0);
  if (parser.num_funcdatas == 0) {
    parser.cur_func = NULL;
  } else {
    parser.cur_func = &parser.funcdatas[parser.num_funcdatas - 1];
  }

  leave_scope();
}

static void advance(void) {
again:
  parser.prev_kind = parser.cur_kind;
  if (parser.num_buffered_tokens > 0) {
    parser.cur_kind = parser.token_buffer[--parser.num_buffered_tokens];
    // TODO: will need to revisit if we add peek().
    ASSERT(parser.cur_kind == TOK_INDENT || parser.cur_kind == TOK_DEDENT);
#if BUILD_DEBUG
    if (parser.verbose) {
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
  if (parser.verbose) {
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

static bool check(TokenKind kind) {
  return parser.cur_kind == kind;
}

static void consume(TokenKind tok_kind, const char* message) {
  if (parser.cur_kind == tok_kind) {
    advance();
    return;
  }
  error_offset(cur_offset(), message);
}

static uint64_t const_expression(void) {
  return 0;
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

static Type parse_type(void) {
  if (match(TOK_STAR)) {
    return type_ptr(parse_type());
  }
  if (match(TOK_LSQUARE)) {
    int count = 0;
    if (!check(TOK_RSQUARE)) {
      count = const_expression();
    }
    consume(TOK_RSQUARE, "Expect ']' to close array type.");
    Type elem = parse_type();
    if (type_is_none(elem)) {
      error("Expecting type of array or slice.");
    }
    ASSERT(false && "todo");
    (void)count;
    abort();
    //return type_array(cur_offset(), count, elem);
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
    }
  }

  return type_none;
}

static uint32_t parse_func_params(Type out_types[MAX_FUNC_PARAMS], Str out_names[MAX_FUNC_PARAMS]) {
  uint32_t num_params = 0;
  bool require_more = false;
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

static bool is_convertible(Operand* operand, Type dest) {
  Type src = operand->type;
  if (type_eq(dest, src)) {
    return true;
  } else if (type_kind(dest) == TYPE_VOID) {
    return true;
  } else if (type_is_arithmetic(dest) && type_is_arithmetic(src)) {
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

static bool cast_operand(Operand* operand, Type type) {
  if (type_kind(operand->type) != type_kind(type)) {
    if (!is_castable(operand, type)) {
      return false;
    }

    if (operand->is_const) {
      // TODO: save val into op and convert here
    }

    ir_ref ref_to_adjust;
    if (operand->ref_is_addr) {
      ref_to_adjust = load_operand_if_necessary(operand);
      operand->is_lvalue = false;
      operand->ref_is_addr = false;
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
#if 0
    // Not sure about this, need to handle i16 being stored and then cast to
    // i32, so there's a full proper i32 to load.
    if (operand->opkind == OpKindLocal) {
      LqRef copy = alloc_for_type(type);
      store_for_type(copy, type, value_of(operand));
      operand->lqref = copy;
    }
#endif
  }

  operand->type = type;
  return true;
}

static bool convert_operand(Operand* operand, Type type) {
  if (is_convertible(operand, type)) {
    cast_operand(operand, type);
    return true;
  }
  return false;
}

typedef enum LastStatementType {
  LST_NON_RETURN,
  LST_RETURN_VOID,
  LST_RETURN_VALUE,
} LastStatementType;

static LastStatementType parse_statement(bool toplevel);
static Operand parse_expression(Type* expected);
static LastStatementType parse_block(void);

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
  ASSERT(false && "not implemented");
  return operand_null;
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
    } else if (type_is_signed(left->type) && left->is_const &&
               type_rank(right->type) >= type_rank(left->type)) {
      // i32 == u64 -- only want do do this when the thing we're casting is
      // const so that overflow can be checked.
      cast_operand(left, right->type);
    } else if (type_is_signed(right->type) && right->is_const &&
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

static Operand resolve_binary_op(ir_op op, Operand left, Operand right, uint32_t loc) {
  ASSERT(type_eq(left.type, right.type));
  if (left.is_const && right.is_const) {
    ASSERT(false && "todo; const eval");
    abort();
    //return operand_const(left.type, eval_binary_op(op, left.type, left.val, right.val, loc));
  } else {
    ir_type irt = type_to_ir_type(left.type);
    ir_ref result =
        ir_BINARY_OP(op, irt, load_operand_if_necessary(&left), load_operand_if_necessary(&right));
    return operand_rvalue_imm(left.type, result);
  }
}

static Operand resolve_binary_arithmetic_op(ir_op op, Operand left, Operand right, uint32_t loc) {
  unify_arithmetic_operands(&left, &right);
  return resolve_binary_op(op, left, right, loc);
}

static Operand parse_binary(Operand left, bool can_assign, Type* expected) {
  // Remember the operator.
  TokenKind op = parser.prev_kind;
  uint32_t op_offset = prev_offset();

  // Compile the right operand.
  Rule* rule = get_rule(op);
  Operand rhs = parse_precedence(rule->prec_for_infix + 1, expected);

  typedef struct IrOpPairAndErr {
    ir_op sign;
    ir_op unsign;
    const char* err_msg;
  } IrOpPairAndErr;
  static IrOpPairAndErr tok_to_cmp_op[NUM_TOKEN_KINDS] = {
      [TOK_EQEQ] = {IR_EQ, IR_EQ, "TODO %s %s"},
      [TOK_BANGEQ] = {IR_NE, IR_NE, "TODO %s %s"},
      [TOK_LEQ] = {IR_LE, IR_ULE, "TODO %s %s"},
      [TOK_LT] = {IR_LT, IR_ULT, "TODO %s %s"},
      [TOK_GEQ] = {IR_GE, IR_UGE, "TODO %s %s"},
      [TOK_GT] = {IR_GT, IR_UGT, "TODO %s %s"},
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
  if (tok_to_cmp_op[op].err_msg /*anything nonzero in slot*/) {
    ir_op irop = type_is_unsigned(left.type) ? tok_to_cmp_op[op].unsign : tok_to_cmp_op[op].sign;
    ir_ref cmp = ir_CMP_OP(irop, load_operand_if_necessary(&left), load_operand_if_necessary(&rhs));
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
    ir_ref result = ir_BINARY_OP(irop, type_to_ir_type(left.type), load_operand_if_necessary(&left),
                                 load_operand_if_necessary(&rhs));
    return operand_rvalue_imm(left.type, result);
  } else {
    ASSERT(false && "todo");
    return operand_null;
  }
}

static Operand parse_bool_literal(bool can_assign, Type* expected) {
  ASSERT(parser.prev_kind == TOK_FALSE || parser.prev_kind == TOK_TRUE);
  return operand_const(type_bool, ir_CONST_BOOL(parser.prev_kind == TOK_FALSE ? 0 : 1));
}

static Operand parse_call(Operand left, bool can_assign, Type* expected) {
  if (can_assign && match_assignment()) {
    CHECK(false && "todo; returning address i think");
  }
  if (type_kind(left.type) != TYPE_FUNC) {
    errorf("Expected function type, but type is %s.", type_as_str(left.type));
  }
  ir_ref arg_values[MAX_FUNC_PARAMS];
  uint32_t num_args = 0;
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
      arg_values[num_args] = arg.ref;
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
  ASSERT(false && "not implemented");
  return operand_null;
}
static Operand parse_dict_literal(bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
  return operand_null;
}
static Operand parse_dot(Operand left, bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
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
  ASSERT(false && "not implemented");
  return operand_null;
}
static Operand parse_list_literal_or_compr(bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
  return operand_null;
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
  Type type = type_none;
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

  ir_val irval;
  irval.u64 = val;
  return operand_const(type, ir_const(_ir_CTX, irval, type_to_ir_type(type)));
}

static Operand parse_offsetof(bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
  return operand_null;
}

static Operand parse_or(Operand left, bool can_assign, Type* expected) {
  if (!type_is_condition(left.type)) {
    errorf("Left-hand side of or cannot be type %s.", type_as_str(left.type));
  }
  ir_ref lcond = ir_IF(load_operand_if_necessary(&left));
  ir_IF_TRUE(lcond);
  ir_ref if_true = ir_END();
  ir_IF_FALSE(lcond);

  Operand right = parse_precedence(PREC_OR, &type_bool);
  if (!type_is_condition(right.type)) {
    errorf("Right-hand side of or cannot be type %s.", type_as_str(right.type));
  }
  ir_ref if_false = ir_END();
  ir_ref rcond = load_operand_if_necessary(&right);
  ir_MERGE_2(if_true, if_false);
  ir_ref result = ir_PHI_2(IR_BOOL, ir_CONST_BOOL(true), rcond);
  return operand_const(type_bool, result);
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
    ir_STORE(astop, load_operand_if_necessary(&first));
    ir_STORE(astep, ir_CONST_I64(1));
  } else {
    // range(first, second, third || 1)
    ir_STORE(astart, load_operand_if_necessary(&first));
    ir_STORE(astop, load_operand_if_necessary(&second));
    if (type_is_none(third.type)) {
      ir_STORE(astep, ir_CONST_I64(1));
    } else {
      ir_STORE(astep, load_operand_if_necessary(&third));
    }
  }
  return operand_rvalue(type_range, range);
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
    return operand_rvalue(type_str, emit_string_obj(inside_quotes));
  } else {
    return operand_rvalue(type_str, emit_string_obj(inside_quotes));
  }
}

static Operand parse_string_interpolate(bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
  return operand_null;
}
static Operand parse_subscript(Operand left, bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
  return operand_null;
}
static Operand parse_typeid(bool can_assign, Type* expected) {
  ASSERT(false && "not implemented");
  return operand_null;
}

static Operand parse_unary(bool can_assign, Type* expected) {
  TokenKind op_kind = parser.prev_kind;
  Operand expr = parse_precedence(PREC_UNARY, expected);
  if (op_kind == TOK_MINUS) {
    return operand_rvalue_imm(expr.type,
                              ir_NEG(type_to_ir_type(expr.type), load_operand_if_necessary(&expr)));
  } else if (op_kind == TOK_NOT) {
    if (type_is_condition(expr.type)) {
      return operand_rvalue_imm(
          expr.type, ir_NOT(type_to_ir_type(expr.type), load_operand_if_necessary(&expr)));
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

static Operand parse_variable(bool can_assign, Type* expected) {
  Str target = str_from_previous();
  SymScopeDecl scope_decl = SSD_NONE;
  Sym* sym = NULL;
  ScopeResult scope_result = scope_lookup(target, &sym, &scope_decl);
  if (can_assign && match_assignment()) {
    TokenKind eq_kind = parser.prev_kind;
    TokenKind eq_offset = prev_offset();
    (void)eq_kind;
    (void)eq_offset;
    if (scope_result == SCOPE_RESULT_UNDEFINED && scope_decl == SSD_ASSUMED_GLOBAL) {
      errorf("Local variable '%s' referenced before assignment.", cstr_copy(parser.arena, target));
    } else if (scope_result == SCOPE_RESULT_LOCAL) {
      ASSERT(sym);
      Operand op = parse_expression(NULL);
      if (!convert_operand(&op, sym->type)) {
        errorf("Cannot assign type %s to type %s.", type_as_str(op.type), type_as_str(sym->type));
      }
      if (eq_kind == TOK_EQ) {
        ir_VSTORE(sym->ref, load_operand_if_necessary(&op));
      } else {
        error_offset(eq_offset, "Unhandled assignment type.");
      }
      return operand_null;
    } else if (scope_result != SCOPE_RESULT_LOCAL && scope_decl == SSD_NONE) {
      if (eq_kind == TOK_EQ) {
        // Variable declaration without a type.
        Operand op = parse_expression(NULL);
        Sym* new = make_local_and_alloc(SYM_VAR, target, op.type);
        ir_VSTORE(new->ref, load_operand_if_necessary(&op));
        return operand_null;
      } else {
        error_offset(eq_offset,
                     "Cannot use an augmented assignment when implicitly declaring a local.");
      }
    }
  } else {
    if (scope_result == SCOPE_RESULT_LOCAL) {
      return operand_lvalue(sym->type, sym->ref);
    } else if (scope_result == SCOPE_RESULT_PARAMETER) {
      return operand_rvalue_imm(sym->type, sym->ref);
    } else if (scope_result == SCOPE_RESULT_GLOBAL) {
      if (type_kind(sym->type) == TYPE_FUNC) {
        return operand_rvalue(sym->type, ir_CONST_ADDR(sym->addr));
      } else {
        ASSERT(false && "global var");
        //return operand_lvalue(sym->type, gen_mir_op_reg(sym->ir_reg));
      }
    } else if (scope_result == SCOPE_RESULT_UPVALUE) {
      // identify upvals
      // figure out how many up into parents
      // determine all neccessary values referenced in child
      // - need to parse body to get those hmm
      error("todo, upval");
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
    error("Expect expression.");
  }

  bool can_assign = precedence <= PREC_ASSIGNMENT;
  Operand left = prefix_rule(can_assign, expected);

  while (precedence <= get_rule(parser.cur_kind)->prec_for_infix) {
    advance();
    InfixFn infix_rule = get_rule(parser.prev_kind)->infix;
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
    ir_ref cond = ir_IF(load_operand_if_necessary(&opcond));
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
      ASSERT(expr.ref_is_addr);
      ir_ref astart = expr.ref;
      ir_ref astop = ir_ADD_A(expr.ref, ir_CONST_ADDR(sizeof(int64_t)));
      ir_ref astep = ir_ADD_A(expr.ref, ir_CONST_ADDR(2 * sizeof(int64_t)));

      ir_ref start = ir_LOAD_I64(astart);
      ir_ref stop = ir_LOAD_I64(astop);
      ir_ref step = ir_LOAD_I64(astep);

      ir_ref is_neg = ir_LT(step, ir_CONST_I64(0));

      Sym* it = make_local_and_alloc(SYM_VAR, it_name, type_i64);
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
  } else if (convert_operand(&val, type_i32)) {
    print_i32(&val);
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
  uint32_t num_params = parse_func_params(param_types, param_names);

  consume(TOK_COLON, "Expect ':' before function body.");
  consume(TOK_NEWLINE, "Expect newline before function body. (TODO: single line)");
  skip_newlines();
  consume(TOK_INDENT, "Expect indent before function body. (TODO: single line)");

  Type functype = type_function(param_types, num_params, return_type);

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

static void parse_variable_statement(Type type) {
  Str name = parse_name("Expect variable or typed variable name.");
  ASSERT(name.i);

  bool have_init;
  ASSERT(!type_is_none(type));
  have_init = match(TOK_EQ);
  if (have_init) {
    Operand op = parse_expression(&type);
    if (!convert_operand(&op, type)) {
      errorf("Initializer cannot be converted from type %s to declared type %s.",
             type_as_str(op.type), type_as_str(type));
    }
    Sym* new = make_local_and_alloc(SYM_VAR, name, op.type);
    ir_VSTORE(new->ref, op.ref);
  } else {
    Sym* new = make_local_and_alloc(SYM_VAR, name, type);
    ir_val irval;
    irval.u64 = 0;
    ir_VSTORE(new->ref, ir_const(_ir_CTX, irval, type_to_ir_type(type)));
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
    ir_VSTORE(parser.cur_func->return_slot->ref, load_operand_if_necessary(&op));
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
                   bool verbose,
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

  init_module_globals();
  enter_scope(/*is_module=*/true, /*is_function=*/false);

  parser.num_tokens = lex_indexer(file.buffer, file.allocated_size, parser.token_offsets);
  token_init(file.buffer);
  if (parser.verbose) {
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
