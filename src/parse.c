#include "luv60.h"

typedef struct TypeData {
  uint32_t ksa;  // 8 kind, 6 align, 18 size
} TypeData;

static TypeData typedata[16<<20];
static int num_typedata;

const char* typekind_names[] = {
#define X(x) #x,
  TYPEKINDS_X
#undef X
};

#if 0
typedef struct TypeDataExtraPtr {
  Type type;
} TypeDataExtraPtr;

typedef struct TypeDataExtraArray {
  Type type;
  uint32_t count;
} TypeDataExtraArray;

typedef struct TypeDataExtraDict {
  Type key;
  Type value;
} TypeDataExtraDict;

typedef struct TypeDataExtraStruct {
  Type type;
  Str name;
  uint32_t offset;
} TypeDataExtraStruct;

typedef struct TypeDataExtraFunc {
  uint32_t flags;
} TypeDataExtraFunc;
#endif

static void set_typedata_raw(uint32_t index, uint32_t data) {
  ASSERT(typedata[index].ksa == 0);
  ASSERT(index < COUNTOF(typedata));
  typedata[index].ksa = data;
}

static uint64_t pack_type(TypeKind kind, uint32_t size, uint32_t align) {
  uint32_t ret = 0;
  ASSERT(kind != 0);
  ASSERT(kind < (1<<8));
  ret |= (uint32_t)kind << 24;
  ASSERT(align < (1<<6) );
  ret |= (uint32_t)kind << 18;
  ASSERT(size < (1<<18));
  ret |= (uint32_t)size;
  return ret;
}

#define P_MAX_FUNC_PARAMS 32
typedef struct FuncParams {
  Type types[P_MAX_FUNC_PARAMS];
  Str names[P_MAX_FUNC_PARAMS];
  int num_params;
} FuncParams;

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
  Str name;
  Type return_type;
  IRFunc ir_func;
} FuncData;

#define MAX_VARS_IN_SCOPE 256
#define MAX_FUNC_NESTING 16
#define MAX_VARIABLE_SCOPES 32

typedef struct VarScope VarScope;
struct VarScope {
  Sym syms[MAX_VARS_IN_SCOPE];
  int num_syms;
  bool is_function;
  bool is_module;
};

typedef struct Parser {
  const char* cur_filename;

  uint8_t token_kinds[128];
  uint32_t token_offsets[128];
  int cur_token_index;

  TokenKind cur_kind;
  uint32_t cur_offset;
  TokenKind prev_kind;
  uint32_t prev_offset;

  FuncData funcdatas[MAX_FUNC_NESTING];
  int num_funcdatas;
  VarScope varscopes[MAX_VARIABLE_SCOPES];
  int num_varscopes;

  FuncData* cur_func;
  VarScope* cur_var_scope;
} Parser;

static Parser parser;

static char* strf(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  size_t n = 1 + vsnprintf(NULL, 0, fmt, args);
  va_end(args);
  char* str = malloc(n);  // TODO: arena
  va_start(args, fmt);
  vsnprintf(str, n, fmt, args);
  va_end(args);
  return str;
}

static void error(const char* message) {
  base_writef_stderr("%s:<offset %d>: error: %s\n", parser.cur_filename, parser.cur_offset,
                     message);
  base_exit(1);
}

static void enter_scope(bool is_module, bool is_function) {
  parser.cur_var_scope = &parser.varscopes[parser.num_varscopes++];
  parser.cur_var_scope->num_syms = 0;
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

static void enter_function(Type return_type, Str name, FuncParams params) {
  parser.cur_func = &parser.funcdatas[parser.num_funcdatas++];
  parser.cur_func->name = name;
  parser.cur_func->return_type = return_type;
  parser.cur_func->ir_func =
      gen_ssa_start_function(name, return_type, params.num_params, params.types, params.names);

  enter_scope(/*is_module=*/false, /*is_function=*/true);
}

static void leave_function(void) {
  //gen_resolve_label(done);
  //gen_func_exit_and_patch_func_entry(&cur_func.func_exit_cont, cur_func.return_type);
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
  ++parser.cur_token_index;
  while (!parser.token_kinds[parser.cur_token_index]) {
    parser.cur_token_index = 0;
    lex_next_block(parser.token_kinds, parser.token_offsets);
  }
  parser.prev_kind = parser.cur_kind;
  parser.prev_offset = parser.cur_offset;
  parser.cur_kind = parser.token_kinds[parser.cur_token_index];
  parser.cur_offset = parser.token_offsets[parser.cur_token_index];
}

static bool match(TokenKind tok_kind) {
  if (parser.cur_kind != tok_kind)
    return false;
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
  error(message);
}

static uint64_t const_expression(void) {
  return 0;
}

static Type basic_tok_to_type[NUM_TOKEN_KINDS] = {
    [TOK_BOOL] = {TYPE_BOOL},      //
    [TOK_BYTE] = {TYPE_U8},        //
    [TOK_CODEPOINT] = {TYPE_I32},  //
    [TOK_DOUBLE] = {TYPE_DOUBLE},  //
    [TOK_F32] = {TYPE_FLOAT},      //
    [TOK_F64] = {TYPE_DOUBLE},     //
    [TOK_FLOAT] = {TYPE_FLOAT},    //
    [TOK_I16] = {TYPE_I16},        //
    [TOK_I32] = {TYPE_I32},        //
    [TOK_I64] = {TYPE_I64},        //
    [TOK_I8] = {TYPE_I8},          //
    [TOK_INT] = {TYPE_I32},        //
    [TOK_STR] = {TYPE_STR},        //
    [TOK_U16] = {TYPE_U16},        //
    [TOK_U32] = {TYPE_U32},        //
    [TOK_U64] = {TYPE_U64},        //
    [TOK_U8] = {TYPE_U8},          //
    [TOK_UINT] = {TYPE_U32},       //

    //[TOK_CONST_CHAR] = TYPE_CONST_CHAR,      //
    //[TOK_CONST_OPAQUE] = TYPE_CONST_OPAQUE,  //
    //[TOK_F16] = TYPE_F16,                    //
    //[TOK_OPAQUE] = TYPE_OPAQUE,              //
    //[TOK_SIZE_T] = TYPE_SIZE_T,              //
};

static Type parse_type(void) {
  if (match(TOK_STAR)) {
    return make_type_ptr(parser.cur_offset, parse_type());
  }
  if (match(TOK_LSQUARE)) {
    int count = 0;
    if (!check(TOK_RSQUARE)) {
      count = const_expression();
    }
    consume(TOK_RSQUARE, "Expect ']' to close array type.");
    Type elem = parse_type();
    if (!elem.i) {
      error("Expecting type of array or slice.");
    }
    return make_type_array(parser.cur_offset, count, elem);
  }

  if (match(TOK_LBRACE)) {
    abort();
  }

  if (match(TOK_DEF)) {
    abort();
  }

  if (parser.cur_kind >= TOK_BOOL && parser.cur_kind <= TOK_UINT) {
    Type t = basic_tok_to_type[parser.cur_kind];
    ASSERT(t.i != 0);
    advance();
    return t;
  }
  return (Type){0};
}

static Str str_from_previous(void) {
  StrView view = lex_get_strview(parser.prev_offset, parser.cur_offset);
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

static FuncParams parse_func_params(void) {
  FuncParams ret = {0};
  bool require_more = false;
  while (require_more || !check(TOK_RPAREN)) {
    require_more = false;
    Type param_type = parse_type();
    if (!param_type.i) {
      advance();
      error("Expect function parameter.");
    }
    Str param_name = parse_name("Expect parameter name.");
    ASSERT(param_name.i);
    ret.types[ret.num_params] = param_type;
    ret.names[ret.num_params] = param_name;
    ++ret.num_params;

    if (check(TOK_RPAREN)) {
      break;
    }
    consume(TOK_COMMA, "Expect ',' between function parameters.");
    require_more = true;
  }
  consume(TOK_RPAREN, "Expect ')' after function parameters.");
  return ret;
}

static void parse_statement(bool toplevel);
static IRRef parse_expression(void);
static void parse_block(void);

static Sym* alloc_local(Str name, Type type) {
  ASSERT(parser.cur_func && parser.cur_var_scope);
  ASSERT(parser.cur_var_scope->num_syms < COUNTOFI(parser.cur_var_scope->syms));
  Sym* new = &parser.cur_var_scope->syms[parser.cur_var_scope->num_syms++];
  new->kind = SYM_VAR;
  new->name = name;
  new->type = type;
  new->scope_decl = SSD_DECLARED_LOCAL;
  new->irref = gen_ssa_alloc_local(type);
  return new;
}

static Sym* get_local(Str name) {
  ASSERT(parser.cur_var_scope);
  if (parser.cur_var_scope->num_syms == 0) {
    return NULL;
  }
  for (int i = parser.cur_var_scope->num_syms; i >= 0; --i) {
    Sym* sym = &parser.cur_var_scope->syms[i];
    if (sym->name.i == name.i) {
      return sym;
    }
  }
  return NULL;
}

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
      *suffix = (Type){TYPE_I8};
    } else if (digits.size == 2 && digits.data[0] == 'u' && digits.data[1] == '8') {
      *suffix = (Type){TYPE_U8};
    } else if (digits.size == 3 && digits.data[0] == 'i' && digits.data[1] == '1' &&
               digits.data[2] == '6') {
      *suffix = (Type){TYPE_I16};
    } else if (digits.size == 3 && digits.data[0] == 'u' && digits.data[1] == '1' &&
               digits.data[2] == '6') {
      *suffix = (Type){TYPE_U16};
    } else if (digits.size == 3 && digits.data[0] == 'i' && digits.data[1] == '3' &&
               digits.data[2] == '2') {
      *suffix = (Type){TYPE_I32};
    } else if (digits.size == 3 && digits.data[0] == 'u' && digits.data[1] == '3' &&
               digits.data[2] == '2') {
      *suffix = (Type){TYPE_U32};
    } else if (digits.size == 3 && digits.data[0] == 'i' && digits.data[1] == '6' &&
               digits.data[2] == '4') {
      *suffix = (Type){TYPE_I64};
    } else if (digits.size == 3 && digits.data[0] == 'u' && digits.data[1] == '6' &&
               digits.data[2] == '4') {
      *suffix = (Type){TYPE_U64};
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
  if (!(tok == TOK_EQ || tok == TOK_PLUSEQ || tok == TOK_MINUSEQ || tok == TOK_STAREQ ||
        tok == TOK_SLASHEQ || tok == TOK_PERCENTEQ || tok == TOK_CARETEQ || tok == TOK_PIPEEQ ||
        tok == TOK_AMPERSANDEQ || tok == TOK_LSHIFTEQ || tok == TOK_RSHIFTEQ)) {
    return false;
  }
  advance();
  return true;
}

typedef IRRef (*PrefixFn)(bool can_assign);
typedef IRRef (*InfixFn)(IRRef left, bool can_assign);

typedef struct Rule {
  PrefixFn prefix;
  InfixFn infix;
  Precedence prec_for_infix;
} Rule;

static Rule* get_rule(TokenKind tok_kind);
static IRRef parse_precedence(Precedence precedence);

static IRRef parse_alignof(bool can_assign) { ASSERT(false && "not implemented"); }
static IRRef parse_and(IRRef left, bool can_assign) { ASSERT(false && "not implemented"); }

static IRRef parse_binary(IRRef left, bool can_assign) {
  // Remember the operator.
  TokenKind op = parser.prev_kind;

  // Compile the right operand.
  Rule* rule = get_rule(op);
  IRRef rhs = parse_precedence(rule->prec_for_infix + 1);
  (void)rhs;

  // TODO: gen binary lhs op rhs
  CHECK(op == TOK_PLUS);
  return gen_ssa_add(left, rhs);
}

static IRRef parse_bool_literal(bool can_assign) { ASSERT(false && "not implemented"); }
static IRRef parse_call(IRRef left, bool can_assign) { ASSERT(false && "not implemented"); }
static IRRef parse_compound_literal(bool can_assign) { ASSERT(false && "not implemented"); }
static IRRef parse_dict_literal(bool can_assign) { ASSERT(false && "not implemented"); }
static IRRef parse_dot(IRRef left, bool can_assign) { ASSERT(false && "not implemented"); }
static IRRef parse_grouping(bool can_assign) { ASSERT(false && "not implemented"); }
static IRRef parse_in_or_not_in(IRRef left, bool can_assign) { ASSERT(false && "not implemented"); }
static IRRef parse_len(bool can_assign) { ASSERT(false && "not implemented"); }
static IRRef parse_list_literal_or_compr(bool can_assign) { ASSERT(false && "not implemented"); }
static IRRef parse_null_literal(bool can_assign) { ASSERT(false && "not implemented"); }

static IRRef parse_number(bool can_assign) {
  Type suffix = {0};
  uint64_t val = scan_int(lex_get_strview(parser.prev_offset, parser.cur_offset), &suffix);
  return gen_ssa_const(val, suffix);
}

static IRRef parse_offsetof(bool can_assign) { ASSERT(false && "not implemented"); }
static IRRef parse_or(IRRef left, bool can_assign) { ASSERT(false && "not implemented"); }
static IRRef parse_range(bool can_assign) { ASSERT(false && "not implemented"); }
static IRRef parse_sizeof(bool can_assign) { ASSERT(false && "not implemented"); }
static IRRef parse_string(bool can_assign) { ASSERT(false && "not implemented"); }
static IRRef parse_string_interpolate(bool can_assign) { ASSERT(false && "not implemented"); }
static IRRef parse_subscript(IRRef left, bool can_assign) { ASSERT(false && "not implemented"); }
static IRRef parse_typeid(bool can_assign) { ASSERT(false && "not implemented"); }
static IRRef parse_unary(bool can_assign) { ASSERT(false && "not implemented"); }

static IRRef parse_variable(bool can_assign) {
  Str target = str_from_previous();
  if (can_assign && match_assignment()) {
    abort();
#if 0
    TokenKind eq = parser.prev_kind;
    parse_expression(NULL);
    ASSERT(eq == TOK_EQ);
    //gen_assignment(target);
#endif
  } else {
    Sym* sym = get_local(target);
    if (!sym) {
      error(strf("Undefined variable '%s'.", cstr(target)));
    }
    ASSERT(sym->scope_decl == SSD_DECLARED_LOCAL);
    return gen_ssa_load(sym->irref, sym->type);
  }

  abort();
  return (IRRef){0};
}

// Has to match the order in tokens.inc.
static Rule rules[NUM_TOKEN_KINDS] = {
    {NULL, NULL, PREC_NONE},  // TOK_INVALID
    {NULL, NULL, PREC_NONE},  // TOK_EOF
    {NULL, NULL, PREC_NONE},  // TOK_INDENT
    {NULL, NULL, PREC_NONE},  // TOK_DEDENT
    {NULL, NULL, PREC_NONE},  // TOK_NEWLINE
    {NULL, NULL, PREC_NONE},  // TOK_NL

    {parse_unary, parse_binary, PREC_BITS},                     // TOK_AMPERSAND
    {NULL, NULL, PREC_ASSIGNMENT},                              // TOK_AMPERSANDEQ
    {parse_alignof, NULL, PREC_NONE},                           // TOK_ALIGNOF
    {parse_unary, NULL, PREC_NONE},                             // TOK_ALLOC
    {NULL, parse_and, PREC_AND},                                // TOK_AND
    {NULL, NULL, PREC_NONE},                                    // TOK_AS
    {NULL, parse_binary, PREC_EQUALITY},                        // TOK_BANGEQ
    {NULL, NULL, PREC_NONE},                                    // TOK_BREAK
    {NULL, parse_binary, PREC_BITS},                            // TOK_CARET
    {NULL, NULL, PREC_ASSIGNMENT},                              // TOK_CARETEQ
    {parse_unary, NULL, PREC_NONE},                             // TOK_CAST
    {NULL, NULL, PREC_NONE},                                    // TOK_CHECK
    {NULL, NULL, PREC_NONE},                                    // TOK_COLON
    {NULL, NULL, PREC_NONE},                                    // TOK_COMMA
    {NULL, NULL, PREC_NONE},                                    // TOK_COMMENT
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
    {NULL, NULL, PREC_NONE},                                    // TOK_HASH
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
    {NULL, NULL, PREC_ASSIGNMENT},                              // TOK_LSHIFTEQ
    {parse_list_literal_or_compr, parse_subscript, PREC_CALL},  // TOK_LSQUARE
    {NULL, parse_binary, PREC_COMPARISON},                      // TOK_LT
    {parse_unary, parse_binary, PREC_TERM},                     // TOK_MINUS
    {NULL, NULL, PREC_NONE},                                    // TOK_MINUSEQ
    {NULL, NULL, PREC_NONE},                                    // TOK_NONLOCAL
    {parse_unary, parse_in_or_not_in, PREC_COMPARISON},         // TOK_NOT
    {parse_null_literal, NULL, PREC_NONE},                      // TOK_NULL
    {parse_offsetof, NULL, PREC_NONE},                          // TOK_OFFSETOF
    {NULL, NULL, PREC_NONE},                                    // TOK_ON
    {NULL, parse_or, PREC_OR},                                  // TOK_OR
    {NULL, NULL, PREC_NONE},                                    // TOK_PASS
    {NULL, parse_binary, PREC_FACTOR},                          // TOK_PERCENT
    {NULL, NULL, PREC_ASSIGNMENT},                              // TOK_PERCENTEQ
    {NULL, parse_binary, PREC_BITS},                            // TOK_PIPE
    {NULL, NULL, PREC_ASSIGNMENT},                              // TOK_PIPEEQ
    {NULL, parse_binary, PREC_TERM},                            // TOK_PLUS
    {NULL, NULL, PREC_NONE},                                    // TOK_PLUSEQ
    {NULL, NULL, PREC_NONE},                                    // TOK_PRINT
    {parse_range, NULL, PREC_NONE},                             // TOK_RANGE
    {NULL, NULL, PREC_NONE},                                    // TOK_RBRACE
    {NULL, NULL, PREC_NONE},                                    // TOK_RELOCATE
    {NULL, NULL, PREC_NONE},                                    // TOK_RETURN
    {NULL, NULL, PREC_NONE},                                    // TOK_RPAREN
    {NULL, parse_binary, PREC_SHIFT},                           // TOK_RSHIFT
    {NULL, NULL, PREC_ASSIGNMENT},                              // TOK_RSHIFTEQ
    {NULL, NULL, PREC_NONE},                                    // TOK_RSQUARE
    {parse_sizeof, NULL, PREC_NONE},                            // TOK_SIZEOF
    {NULL, parse_binary, PREC_FACTOR},                          // TOK_SLASH
    {NULL, NULL, PREC_NONE},                                    // TOK_SLASHEQ
    {NULL, parse_binary, PREC_FACTOR},                          // TOK_STAR
    {NULL, NULL, PREC_NONE},                                    // TOK_STAREQ
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

static IRRef parse_precedence(Precedence precedence) {
  advance();
  PrefixFn prefix_rule = get_rule(parser.prev_kind)->prefix;
  if (!prefix_rule) {
    error("Expect expression.");
  }

  bool can_assign = precedence <= PREC_ASSIGNMENT;
  IRRef left = prefix_rule(can_assign);

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

static IRRef parse_expression(void) {
  return parse_precedence(PREC_LOWEST);
}

static void if_statement(void) {
  //ContFixup cond = gen_make_label("if_cond");
  IRRef cond = parse_expression();
  (void)cond;

  consume(TOK_COLON, "Expect ':' to start if.");
  consume(TOK_NEWLINE, "Expect newline after ':' to start if.");
  consume(TOK_INDENT, "Expect indent to start block after if.");

  //ContFixup then = gen_make_label("if_then");
  //ContFixup els = gen_make_label("if_else");
  //ContFixup after = gen_make_label("if_after");

  //gen_if(&cond, &then, &els);

  //gen_resolve_label(&then);

  parse_block();

  // while (match(TOK_ELIF)) {}

  //gen_resolve_label(&els);

  // if (match(TOK_ELSE)) { }

  //gen_resolve_label(&after);
}

static bool parse_func_body_only_statement(void) {
  if (match(TOK_IF)) {
     if_statement();
     return true;
  }

  if (match(TOK_RETURN)) {
    ASSERT(parser.cur_func->return_type.i);
    IRRef val = {0};
    if (parser.cur_func->return_type.i != TYPE_VOID) {
      val = parse_expression();
    }
    gen_ssa_return(val, parser.cur_func->return_type);
    return true;
  }

  return false;
}

static void parse_block(void) {
  while (!check(TOK_DEDENT) && !check(TOK_EOF)) {
    parse_statement(/*toplevel=*/false);
    while (match(TOK_NEWLINE)) {
    }
  }
  consume(TOK_DEDENT, "Expect end of block.");
  //gen_jump(cont);
}

// TODO: decorators
static void parse_def(void) {
  Type return_type = parse_type();
  Str name = parse_name("Expect function name.");
  consume(TOK_LPAREN, "Expect '(' after function name.");
  FuncParams params = parse_func_params();
  consume(TOK_COLON, "Expect ':' before function body.");
  consume(TOK_NEWLINE, "Expect newline before function body. (TODO: single line)");
  while (match(TOK_NEWLINE)) {
  }
  consume(TOK_INDENT, "Expect indented function body.");

  enter_function(return_type, name, params);
  parse_block();
  leave_function();
}

static void parse_variable_statement(Type type) {
  Str name = parse_name("Expect variable or typed variable name.");
  ASSERT(name.i);

  bool have_init;
  if (!type.i) {
    abort();
#if 0
    consume(TOK_EQ, "Expect initializer for variable without type.");
    parse_expression();
    have_init = true;
#endif
  } else {
    have_init = match(TOK_EQ);
    if (have_init) {
      IRRef val = parse_expression();
      Sym* new = alloc_local(name, type);
      gen_ssa_store(new->irref, type, val);
    }
  }

  consume(TOK_NEWLINE, "Expect newline after variable declaration.");
}

static bool parse_anywhere_statement(void) {
  if (match(TOK_DEF)) {
    parse_def();
    return true;
  }

  Type var_type = parse_type();
  if (var_type.i) {
    parse_variable_statement(var_type);
    return true;
  }

  abort();
 // parse_expression();
  consume(TOK_NEWLINE, "Expect newline.");
  return true;
}

static void parse_statement(bool toplevel) {
  while (match(TOK_NEWLINE)) {
  }

  if (!toplevel) {
    if (parse_func_body_only_statement()) {
      return;
    }
  }

  parse_anywhere_statement();
}

static void init_types(void) {
  set_typedata_raw(TYPE_VOID, pack_type(TYPE_VOID, 0, 0));
  set_typedata_raw(TYPE_BOOL, pack_type(TYPE_BOOL, 1, 1));
  set_typedata_raw(TYPE_U8, pack_type(TYPE_U8, 1, 1));
  set_typedata_raw(TYPE_I8, pack_type(TYPE_I8, 1, 1));
  set_typedata_raw(TYPE_U16, pack_type(TYPE_U16, 2, 2));
  set_typedata_raw(TYPE_I16, pack_type(TYPE_I16, 2, 2));
  set_typedata_raw(TYPE_U32, pack_type(TYPE_U32, 4, 4));
  set_typedata_raw(TYPE_I32, pack_type(TYPE_I32, 4, 4));
  set_typedata_raw(TYPE_U64, pack_type(TYPE_U64, 8, 8));
  set_typedata_raw(TYPE_I64, pack_type(TYPE_I64, 8, 8));
  set_typedata_raw(TYPE_FLOAT, pack_type(TYPE_FLOAT, 4, 4));
  set_typedata_raw(TYPE_DOUBLE, pack_type(TYPE_DOUBLE, 8, 8));
  set_typedata_raw(TYPE_STR, pack_type(TYPE_STR, 16, 8));
  set_typedata_raw(TYPE_RANGE, pack_type(TYPE_STR, 24, 8));
  num_typedata = NUM_TYPE_KINDS;
}

void parse(const char* filename, ReadFileResult file) {
  init_types();

  parser.cur_filename = filename;
  parser.cur_token_index = 0;
  parser.num_funcdatas = 0;
  parser.num_varscopes = 0;
  parser.cur_func = NULL;
  parser.cur_var_scope = NULL;
  enter_scope(/*is_module=*/true, /*is_function=*/false);

  lex_start(file.buffer, file.allocated_size);
  advance();

  while (parser.cur_kind != TOK_EOF) {
    parse_statement(/*toplevel=*/true);
  }
}
