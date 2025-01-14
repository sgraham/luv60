#include "luv60.h"

#include "dict.h"

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
  IRRef irref;
  SymScopeDecl scope_decl;
} Sym;

typedef struct FuncData {
  Sym* sym;
} FuncData;

#define MAX_FUNC_NESTING 16
#define MAX_VARIABLE_SCOPES 32
#define MAX_FUNC_PARAMS 32

typedef struct VarScope VarScope;
struct VarScope {
  DictImpl sym_dict;
  bool is_function;
  bool is_module;
};

typedef struct Parser {
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
} Parser;

static Parser parser;

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

typedef struct NameSymPair {
  Str name;
  Sym sym;
} NameSymPair;

static size_t namesym_hash_func(void* vnsp) {
  NameSymPair* nsp = (NameSymPair*)vnsp;
  size_t hash = 0;
  dict_hash_write(&hash, &nsp->name, sizeof(Str));
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
  NameSymPair nsp = {.name = name,
                     .sym = {
                         .kind = kind,
                         .name = name,
                         .type = type,
                     }};
  DictInsert res = dict_insert(&parser.cur_var_scope->sym_dict, &nsp, namesym_hash_func,
                               namesym_eq_func, sizeof(NameSymPair), _Alignof(NameSymPair));
  return &((NameSymPair*)dict_rawiter_get(&res.iter))->sym;
}

static Sym* make_local_and_alloc(SymKind kind, Str name, Type type) {
  Sym* new = sym_new(kind, name, type);
  new->irref = gen_ssa_make_temp(type);
  new->scope_decl = SSD_DECLARED_LOCAL;
  gen_ssa_alloc_local(type_size(type), type_align(type), new->irref);
  return new;
}

static Sym* make_param(Str name, Type type) {
  Sym* new = sym_new(SYM_VAR, name, type);
  new->irref = gen_ssa_make_temp(type);
  new->scope_decl = SSD_DECLARED_PARAMETER;
  return new;
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

static void enter_scope(bool is_module, bool is_function) {
  parser.cur_var_scope = &parser.varscopes[parser.num_varscopes++];
  parser.cur_var_scope->sym_dict =
      dict_new(is_module ? (1 << 24) : 16, sizeof(NameSymPair), _Alignof(NameSymPair));
  parser.cur_var_scope->is_function = is_function;
  parser.cur_var_scope->is_module = is_module;
}

static void leave_scope(void) {
  --parser.num_varscopes;
  ASSERT(parser.num_varscopes >= 0);
  if (parser.num_varscopes == 0) {
    parser.cur_var_scope = NULL;
  } else {
    parser.cur_var_scope = &parser.varscopes[parser.num_varscopes - 1];
  }
}

static IRRef enter_function(Sym* sym, Str param_names[MAX_FUNC_PARAMS]) {
  parser.cur_func = &parser.funcdatas[parser.num_funcdatas++];
  parser.cur_func->sym = sym;
  enter_scope(/*is_module=*/false, /*is_function=*/true);

  uint32_t num_params = type_func_num_params(sym->type);
  IRRef refs[MAX_FUNC_PARAMS];
  for (uint32_t i = 0; i < num_params; ++i) {
    refs[i] = make_param(param_names[i], type_func_param(sym->type, i))->irref;
  }
  return gen_ssa_start_function(sym->name, type_func_return_type(sym->type), num_params, refs);
}

static void leave_function(void) {
  // gen_resolve_label(done);
  // gen_func_exit_and_patch_func_entry(&cur_func.func_exit_cont, cur_func.return_type);
  gen_ssa_end_function();

  --parser.num_funcdatas;
  ASSERT(parser.num_funcdatas >= 0);
  if (parser.num_funcdatas == 0) {
    parser.cur_func = NULL;
  } else {
    parser.cur_func = &parser.funcdatas[parser.num_funcdatas - 1];
  }
  leave_scope();
}

static Type make_type_ptr(uint32_t offset, Type subtype) {
  return (Type){0};
}

static Type make_type_array(uint32_t offset, uint64_t count, Type subtype) {
  return (Type){0};
}

static void advance(void) {
again:
  parser.prev_kind = parser.cur_kind;
  if (parser.num_buffered_tokens > 0) {
    parser.cur_kind = parser.token_buffer[--parser.num_buffered_tokens];
    // TODO: will need to revisit if we add peek().
    ASSERT(parser.cur_kind == TOK_INDENT || parser.cur_kind == TOK_DEDENT);
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

static Type parse_type(void) {
  if (match(TOK_STAR)) {
    return make_type_ptr(cur_offset(), parse_type());
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
    return make_type_array(cur_offset(), count, elem);
  }

  if (match(TOK_LBRACE)) {
    abort();
  }

  if (match(TOK_DEF)) {
    abort();
  }

  if (parser.cur_kind >= TOK_BOOL && parser.cur_kind <= TOK_UINT) {
    Type t = basic_tok_to_type[parser.cur_kind];
    ASSERT(!type_is_none(t));
    advance();
    return t;
  }

  return type_none;
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

typedef struct Operand {
  Type type;
  IRRef irref;
  bool is_lvalue;
  bool is_const;
} Operand;

static Operand operand_null;

static Operand operand_lvalue(Type type, IRRef irref) {
  return (Operand){.type = type, .irref = irref, .is_lvalue = true};
}

static Operand operand_rvalue(Type type, IRRef irref) {
  return (Operand){.type = type, .irref = irref};
}

static Operand operand_const(Type type, IRRef irref) {
  return (Operand){.type = type, .irref = irref, .is_const = true};
}

static bool is_arithmetic_type(Type type) {
  return type_kind(type) >= TYPE_U8 && type_kind(type) <= TYPE_DOUBLE;
}

static bool is_integer_type(Type type) {
  return type_kind(type) >= TYPE_U8 && type_kind(type) <= TYPE_ENUM;
}

static bool is_ptr_like_type(Type type) {
  return type_kind(type) == TYPE_PTR || type_kind(type) == TYPE_FUNC;
}

#if 0
static bool is_floating_type(Type type) {
  return type.i >= TYPE_FLOAT && type.i <= TYPE_DOUBLE;
}
#endif

static bool is_condition_type(Type type) {
  switch (type_kind(type)) {
    case TYPE_BOOL:
    case TYPE_STR:
    case TYPE_SLICE:
    case TYPE_PTR:
      return true;
    default:
      return false;
  }
}

static bool is_convertible(Operand* operand, Type dest) {
  Type src = operand->type;
  if (type_eq(dest, src)) {
    return true;
  } else if (type_kind(dest) == TYPE_VOID) {
    return true;
  } else if (is_arithmetic_type(dest) && is_arithmetic_type(src)) {
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
    return is_ptr_like_type(src) || is_integer_type(src);
  } else if (is_integer_type(dest)) {
    return is_ptr_like_type(src);
  } else if (is_integer_type(src)) {
    return is_ptr_like_type(dest);
  } else if (is_ptr_like_type(dest) && is_ptr_like_type(src)) {
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
  }

  operand->type = type;
  return true;
}

static bool convert_operand(Operand* operand, Type type) {
  if (is_convertible(operand, type)) {
    cast_operand(operand, type);
    operand->is_lvalue = false;
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
static Operand parse_expression(void);
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

typedef Operand (*PrefixFn)(bool can_assign);
typedef Operand (*InfixFn)(Operand left, bool can_assign);

typedef struct Rule {
  PrefixFn prefix;
  InfixFn infix;
  Precedence prec_for_infix;
} Rule;

static Rule* get_rule(TokenKind tok_kind);
static Operand parse_precedence(Precedence precedence);

static Operand parse_alignof(bool can_assign) {
  ASSERT(false && "not implemented");
  return operand_null;
}
static Operand parse_and(Operand left, bool can_assign) {
  ASSERT(false && "not implemented");
  return operand_null;
}

static Operand parse_binary(Operand left, bool can_assign) {
  // Remember the operator.
  TokenKind op = parser.prev_kind;

  // Compile the right operand.
  Rule* rule = get_rule(op);
  Operand rhs = parse_precedence(rule->prec_for_infix + 1);
  (void)rhs;

  // TODO: gen binary lhs op rhs
  static IRIntCmp tok_to_int_cmp_signed[NUM_TOKEN_KINDS] = {
      [TOK_EQEQ] = IIC_EQ,    //
      [TOK_BANGEQ] = IIC_NE,  //
      [TOK_LEQ] = IIC_SLE,    //
      [TOK_LT] = IIC_SLT,     //
      [TOK_GEQ] = IIC_SGE,    //
      [TOK_GT] = IIC_SGT,     //
  };
  if (tok_to_int_cmp_signed[op]) {
    return operand_rvalue(type_bool,
                          gen_ssa_int_comparison(tok_to_int_cmp_signed[op], left.irref, rhs.irref));
  } else if (op == TOK_PLUS) {
    return operand_rvalue(type_i32, gen_ssa_add(left.irref, rhs.irref));
  } else if (op == TOK_STAR) {
    return operand_rvalue(type_i32, gen_ssa_mul(left.irref, rhs.irref));
  } else {
    ASSERT(false && "todo");
    return operand_null;
  }
}

static Operand parse_bool_literal(bool can_assign) {
  ASSERT(parser.prev_kind == TOK_FALSE || parser.prev_kind == TOK_TRUE);
  return operand_const(type_bool, gen_ssa_const(parser.prev_kind == TOK_FALSE ? 0 : 1, type_bool));
}

static Operand parse_call(Operand left, bool can_assign) {
  if (can_assign && match_assignment()) {
    CHECK(false && "todo; returning address i think");
  }
  if (type_kind(left.type) != TYPE_FUNC) {
    errorf("Expected function type, but type is %s.", type_as_str(left.type));
  }
  Type arg_types[MAX_FUNC_PARAMS];
  IRRef arg_values[MAX_FUNC_PARAMS];
  uint32_t num_args = 0;
  if (!check(TOK_RPAREN)) {
    for (;;) {
      if (num_args >= type_func_num_params(left.type)) {
        errorf("Passing >= %d arguments to function, but it expects %d.", num_args + 1,
               type_func_num_params(left.type));
      }
      uint32_t arg_offset = cur_offset();
      Type param_type = type_func_param(left.type, num_args);
      Operand arg = parse_precedence(PREC_OR);
      if (!convert_operand(&arg, param_type)) {
        errorf_offset(arg_offset, "Call argument %d is type %s, but function expects type %s.",
                      num_args + 1, type_as_str(arg.type), type_as_str(param_type));
      }
      arg_types[num_args] = arg.type;
      arg_values[num_args] = arg.irref;
      ++num_args;
      if (!match(TOK_COMMA)) {
        break;
      }
    }
  }
  consume(TOK_RPAREN, "Expect ')' after arguments.");
  return operand_rvalue(
      type_func_return_type(left.type),
      gen_ssa_call(type_func_return_type(left.type), left.irref, num_args, arg_types, arg_values));
}

static Operand parse_compound_literal(bool can_assign) {
  ASSERT(false && "not implemented");
  return operand_null;
}
static Operand parse_dict_literal(bool can_assign) {
  ASSERT(false && "not implemented");
  return operand_null;
}
static Operand parse_dot(Operand left, bool can_assign) {
  ASSERT(false && "not implemented");
  return operand_null;
}
static Operand parse_grouping(bool can_assign) {
  ASSERT(false && "not implemented");
  return operand_null;
}
static Operand parse_in_or_not_in(Operand left, bool can_assign) {
  ASSERT(false && "not implemented");
  return operand_null;
}
static Operand parse_len(bool can_assign) {
  ASSERT(false && "not implemented");
  return operand_null;
}
static Operand parse_list_literal_or_compr(bool can_assign) {
  ASSERT(false && "not implemented");
  return operand_null;
}
static Operand parse_null_literal(bool can_assign) {
  ASSERT(false && "not implemented");
  return operand_null;
}

static Operand parse_number(bool can_assign) {
  Type suffix = {0};
  uint64_t val = scan_int(get_strview_for_offsets(prev_offset(), cur_offset()), &suffix);
  Type type = type_is_none(suffix) ? type_u64 : suffix;
  return operand_const(type, gen_ssa_const(val, type));
}

static Operand parse_offsetof(bool can_assign) {
  ASSERT(false && "not implemented");
  return operand_null;
}
static Operand parse_or(Operand left, bool can_assign) {
  ASSERT(false && "not implemented");
  return operand_null;
}

static Operand parse_range(bool can_assign) {
  consume(TOK_LPAREN, "Expect '(' after range.");
  Operand first = parse_precedence(PREC_OR);
  if (!convert_operand(&first, type_i64)) {
    errorf("Cannot convert type %s to i64.", type_as_str(first.type));
  }

  Operand second = operand_null;
  Operand third = operand_null;
  if (match(TOK_COMMA)) {
    second = parse_precedence(PREC_OR);
    if (!convert_operand(&second, type_i64)) {
      errorf("Cannot convert type %s to i64.", type_as_str(second.type));
    }
    if (match(TOK_COMMA)) {
      third = parse_precedence(PREC_OR);
      if (!convert_operand(&third, type_i64)) {
        errorf("Cannot convert type %s to i64.", type_as_str(third.type));
      }
    }
  }
  consume(TOK_RPAREN, "Expect ')' after range.");

  IRRef range = gen_ssa_make_temp(type_range);
  if (type_is_none(second.type)) {
    // range(0, first, 1)
    gen_ssa_store_field(range, 0, type_i64, gen_ssa_const(0, type_i64));
    gen_ssa_store_field(range, type_size(type_i64), type_i64, first.irref);
    gen_ssa_store_field(range, type_size(type_i64) * 2, type_i64, gen_ssa_const(1, type_i64));
  } else {
    // range(first, second, third || 1)
    gen_ssa_store_field(range, 0, type_i64, first.irref);
    gen_ssa_store_field(range, type_size(type_i64), type_i64, second.irref);
    if (type_is_none(third.type)) {
      gen_ssa_store_field(range, type_size(type_i64) * 2, type_i64, gen_ssa_const(1, type_i64));
    } else {
      gen_ssa_store_field(range, type_size(type_i64) * 2, type_i64, third.irref);
    }
  }
  return operand_rvalue(type_range, range);
}

static Operand parse_sizeof(bool can_assign) {
  ASSERT(false && "not implemented");
  return operand_null;
}

static Operand parse_string(bool can_assign) {
  StrView strview = get_strview_for_offsets(prev_offset(), cur_offset());
  if (memchr(strview.data, '\\', strview.size) != NULL) {
    Str str = str_process_escapes(strview.data + 1, strview.size - 2);
    if (str.i == 0) {
      error("Invalid string escape.");
    }
    return operand_const(type_str, gen_ssa_string_constant(str));
  } else {
    return operand_const(
        type_str, gen_ssa_string_constant(str_intern_len(strview.data + 1, strview.size - 2)));
  }
}

static Operand parse_string_interpolate(bool can_assign) {
  ASSERT(false && "not implemented");
  return operand_null;
}
static Operand parse_subscript(Operand left, bool can_assign) {
  ASSERT(false && "not implemented");
  return operand_null;
}
static Operand parse_typeid(bool can_assign) {
  ASSERT(false && "not implemented");
  return operand_null;
}

static Operand parse_unary(bool can_assign) {
  TokenKind op_kind = parser.prev_kind;
  Operand expr = parse_precedence(PREC_UNARY);
  if (op_kind == TOK_MINUS) {
    return operand_rvalue(expr.type, gen_ssa_neg(expr.irref));
  } else {
    error("unary operator not implemented");
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

static Sym* find_in_scope(VarScope* scope, Str name) {
  DictRawIter iter =
      dict_find(&scope->sym_dict, &name, namesym_hash_func, namesym_eq_func, sizeof(NameSymPair));
  NameSymPair* nsp = (NameSymPair*)dict_rawiter_get(&iter);
  if (!nsp) {
    return NULL;
  }
  return &nsp->sym;
}

// Returns pointer into sym_dict (where Sym is stored by value), probably a bad idea.
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
        abort();
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

static Operand parse_variable(bool can_assign) {
  Str target = str_from_previous();
  SymScopeDecl scope_decl = SSD_NONE;
  Sym* sym = NULL;
  ScopeResult scope_result = scope_lookup(target, &sym, &scope_decl);
  if (can_assign && match_assignment()) {
    TokenKind eq_kind = parser.prev_kind;
    TokenKind eq_offset = prev_offset();
    if (scope_result == SCOPE_RESULT_UNDEFINED && scope_decl == SSD_ASSUMED_GLOBAL) {
      errorf("Local variable '%s' referenced before assignment.", cstr(target));
    } else if (scope_result == SCOPE_RESULT_LOCAL) {
      ASSERT(sym);
      Operand op = parse_expression();
      if (!convert_operand(&op, sym->type)) {
        errorf("Cannot assign type %s to type %s.", type_as_str(op.type), type_as_str(sym->type));
      }
      if (eq_kind == TOK_EQ) {
        ///| store sym->irref, i64, op.irref
        gen_ssa_store(sym->irref, op.type, op.irref);
#if 0
      } else if (eq_kind == TOK_PLUSEQ) {
        gen_ssa_store(sym->irref, op.type,
                      gen_ssa_add(gen_ssa_load(sym->irref, op.type), op.irref));
#endif
      } else {
        error_offset(eq_offset, "Unhandled assignment type.");
      }
      return operand_null;
    } else if (scope_result != SCOPE_RESULT_LOCAL && scope_decl == SSD_NONE) {
      if (eq_kind == TOK_EQ) {
        // Variable declaration without a type.
        Operand op = parse_expression();
        Sym* new = make_local_and_alloc(SYM_VAR, target, op.type);
        gen_ssa_store(new->irref, op.type, op.irref);
        return operand_null;
      } else {
        error_offset(eq_offset,
                     "Cannot use an augmented assignment when implicitly declaring a local.");
      }
    }
  } else {
    if (scope_result == SCOPE_RESULT_LOCAL) {
      return operand_lvalue(sym->type, gen_ssa_load(sym->irref, sym->type));
    } else if (scope_result == SCOPE_RESULT_PARAMETER) {
      // TODO: & of parameter won't work, maybe just disallow without in-source copy?
      return operand_rvalue(sym->type, sym->irref);
    } else if (scope_result == SCOPE_RESULT_GLOBAL) {
      return operand_lvalue(sym->type, sym->irref);
    } else if (scope_result == SCOPE_RESULT_UPVALUE) {
      // identify upvals
      // figure out how many up into parents
      // determine all neccessary values referenced in child
      // - need to parse body to get those hmm
      error("todo, upval");
    } else {
      errorf("Undefined reference to '%s'.\n", cstr(target));
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

static Operand parse_precedence(Precedence precedence) {
  advance();
  PrefixFn prefix_rule = get_rule(parser.prev_kind)->prefix;
  if (!prefix_rule) {
    error("Expect expression.");
  }

  bool can_assign = precedence <= PREC_ASSIGNMENT;
  Operand left = prefix_rule(can_assign);

  while (precedence <= get_rule(parser.cur_kind)->prec_for_infix) {
    advance();
    InfixFn infix_rule = get_rule(parser.prev_kind)->infix;
    left = infix_rule(left, can_assign);
  }

  if (can_assign && match_assignment()) {
    error("Invalid assignment target.");
  }

  return left;
}

static Operand parse_expression(void) {
  return parse_precedence(PREC_LOWEST);
}

static void expect_end_of_statement(const char* after_what) {
  if (!match(TOK_NEWLINE)) {
    errorf("Expect newline after %s statement.", after_what);
  }
}

static Operand if_statement_cond_helper(void) {
  Operand cond = parse_expression();
  if (!is_condition_type(cond.type)) {
    errorf("Result of condition expression cannot be type %s.", type_as_str(cond.type));
  }
  consume(TOK_COLON, "Expect ':' to start if/elif.");
  consume(TOK_NEWLINE, "Expect newline after ':' to start if/elif.");
  consume(TOK_INDENT, "Expect indent to start if/elif.");
  return cond;
}

static void if_statement(void) {
  IRBlock after = gen_ssa_make_block_name();

  do {
    Operand cond = if_statement_cond_helper();
    IRBlock then = gen_ssa_make_block_name();
    IRBlock next = gen_ssa_make_block_name();
    gen_ssa_jump_cond(cond.irref, then, next);

    gen_ssa_start_block(then);
    if (parse_block() == LST_NON_RETURN) {
      gen_ssa_jump(after);
    }
    gen_ssa_start_block(next);
  } while (match(TOK_ELIF));

  if (match(TOK_ELSE)) {
    consume(TOK_COLON, "Expect ':' to start else.");
    consume(TOK_NEWLINE, "Expect newline after ':' to start else.");
    consume(TOK_INDENT, "Expect indent to start else.");
    if (parse_block() == LST_NON_RETURN) {
      gen_ssa_jump(after);
    }
  }

  gen_ssa_start_block(after);
}

static void for_statement(void) {
  // Can be:
  // 1. no condition
  // 2. condition only, while style
  // 3. iter in expr
  // 4. *ptr in expr
  // 5. index, iter enumerate expr
  // 6. index, *ptr enumerate expr

  IRBlock after = gen_ssa_make_block_name();
  IRBlock top = gen_ssa_make_block_name();
  IRBlock body = gen_ssa_make_block_name();
  IRBlock tail = gen_ssa_make_block_name();

  if (check(TOK_COLON)) {
    // Nothing, case 1:
  } else {  // if (check(TOK_IDENT_VAR) && peek(2, TOK_IN)) {
    // Case 3.
    Str it_name = parse_name("Expect iterator name.");
    consume(TOK_IN, "Expect 'in'.");
    Operand expr = parse_expression();
    if (type_eq(expr.type, type_range)) {
      IRRef step = gen_ssa_load_field(expr.irref, 16 /*.step*/, type_i64);
      IRRef is_neg = gen_ssa_int_comparison(IIC_SLT, step, gen_ssa_const(0, type_i64));

      Sym* it = make_local_and_alloc(SYM_VAR, it_name, type_i64);

      gen_ssa_store(it->irref, type_i64, gen_ssa_load_field(expr.irref, 0 /*.start*/, type_i64));

      IRRef stop = gen_ssa_load_field(expr.irref, 8 /*.stop*/, type_i64);

      gen_ssa_start_block(top);

      IRRef cur_val = gen_ssa_load(it->irref, type_i64);

      // is_neg ? (it > stop) : (it < stop)
      IRBlock is_neg_cmp = gen_ssa_make_block_name();
      IRBlock is_pos_cmp = gen_ssa_make_block_name();
      gen_ssa_jump_cond(is_neg, is_neg_cmp, is_pos_cmp);

      gen_ssa_start_block(is_neg_cmp);
      IRRef should_cont_neg = gen_ssa_int_comparison(IIC_SGT, cur_val, stop);
      gen_ssa_jump_cond(should_cont_neg, body, after);

      gen_ssa_start_block(is_pos_cmp);
      IRRef should_cont_pos = gen_ssa_int_comparison(IIC_SLT, cur_val, stop);
      gen_ssa_jump_cond(should_cont_pos, body, after);

      // block goes in here

      gen_ssa_start_block(tail);
      IRRef cur = gen_ssa_load(it->irref, type_i64);
      IRRef incremented = gen_ssa_add(cur, step);
      gen_ssa_store(it->irref, type_i64, incremented);
      gen_ssa_jump(top);
    } else {
      errorf("Unhandled for/in over type %s.", type_as_str(expr.type));
    }
  }

  consume(TOK_COLON, "Expect ':' to start for.");
  consume(TOK_NEWLINE, "Expect newline after ':' to start for.");
  consume(TOK_INDENT, "Expect indent to start for.");
  gen_ssa_start_block(body);
  parse_block();
  gen_ssa_jump(tail);

  gen_ssa_start_block(after);
}

static void print_statement(void) {
  Operand val = parse_expression();
  if (type_eq(val.type, type_str)) {
    gen_ssa_print_str(val.irref);
  } else if (type_eq(val.type, type_range)) {
    gen_ssa_print_range(val.irref);
  } else if (convert_operand(&val, type_i32)) {
    gen_ssa_print_i32(val.irref);
  }
  expect_end_of_statement("print");
}

static bool parse_func_body_only_statement(LastStatementType* lst) {
  *lst = LST_NON_RETURN;

  if (match(TOK_IF)) {
    if_statement();
    return true;
  }
  if (match(TOK_FOR)) {
    for_statement();
    return true;
  }
  if (match(TOK_PRINT)) {
    print_statement();
    return true;
  }

  if (match(TOK_RETURN)) {
    Type func_ret = type_func_return_type(parser.cur_func->sym->type);
    ASSERT(!type_is_none(func_ret));
    Operand op = operand_null;
    if (!type_eq(func_ret, type_void)) {
      op = parse_expression();
      *lst = LST_RETURN_VALUE;
    } else {
      *lst = LST_RETURN_VOID;
    }

    gen_ssa_return(op.irref, func_ret);
    return true;
  }

  return false;
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
static void parse_def_statement(void) {
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
  consume(TOK_INDENT, "Expect indent before function body. (TODO: single line)");
  skip_newlines();

  Type functype = type_function(param_types, num_params, return_type);

  Sym* funcsym = sym_new(SYM_FUNC, name, functype);
  funcsym->irref = enter_function(funcsym, param_names);
  LastStatementType lst = parse_block();
  if (lst == LST_NON_RETURN && !type_eq(type_void, type_func_return_type(functype))) {
    errorf_offset(function_start_offset,
                  "Function returns %s, but there is no return at the end of the body.",
                  type_as_str(type_func_return_type(functype)));
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
    Operand op = parse_expression();
    if (!convert_operand(&op, type)) {
      errorf("Initializer cannot be converted from type %s to declared type %s.",
             type_as_str(op.type), type_as_str(type));
    }
    Sym* new = make_local_and_alloc(SYM_VAR, name, op.type);
    gen_ssa_store(new->irref, op.type, op.irref);
  }

  expect_end_of_statement("variable declaration");
}

static bool parse_anywhere_statement(void) {
  if (match(TOK_DEF)) {
    parse_def_statement();
    return true;
  }

  Type var_type = parse_type();
  if (!type_is_none(var_type)) {
    parse_variable_statement(var_type);
    return true;
  }

  parse_expression();
  expect_end_of_statement("top level");
  return true;
}

static LastStatementType parse_statement(bool toplevel) {
  skip_newlines();
  if (!toplevel) {
    LastStatementType lst;
    if (parse_func_body_only_statement(&lst)) {
      return lst;
    }
  }

  parse_anywhere_statement();
  skip_newlines();
  return LST_NON_RETURN;  // Return isn't possible unless in func.
}

void parse(const char* filename, ReadFileResult file) {
  type_init();

  // In the case of "a.a." the worst case for offsets is the same as the number
  // of characters in the buffer.
  parser.token_offsets = (uint32_t*)base_large_alloc_rw(file.allocated_size * sizeof(uint32_t));
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
  enter_scope(/*is_module=*/true, /*is_function=*/false);

  parser.num_tokens = lex_indexer(file.buffer, file.allocated_size, parser.token_offsets);
  token_init(file.buffer);
  advance();

  while (parser.cur_kind != TOK_EOF) {
    parse_statement(/*toplevel=*/true);
  }
}
