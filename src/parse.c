typedef struct Str {
  uint32_t i;
} Str;

typedef struct TypeData {
  uint32_t ksa;  // 8 kind, 6 align, 18 size
} TypeData;

static TypeData p_typedata[16<<20];
static int p_num_typedata;

static char p_strings[16<<20];
static int p_string_insert = 1;

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

static void p_set_typedata_raw(uint32_t index, uint32_t data) {
  ASSERT(p_typedata[index].ksa == 0);
  ASSERT(index < COUNTOF(p_typedata));
  p_typedata[index].ksa = data;
}

static uint64_t p_pack_type(TypeKind kind, uint32_t size, uint32_t align) {
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

typedef struct Sym {
  SymKind kind;
  Str name;
  Type type;
  Val val;
  int stack_offset;
  bool is_local;
} Sym;

static const char* p_cur_filename;
static uint8_t p_token_kinds[128] = {0};
static uint32_t p_token_offsets[128] = {0};
static int p_cur_token_index = 0;
static TokenKind p_cur_kind;
static uint32_t p_cur_offset;
static TokenKind p_previous_kind;
static uint32_t p_previous_offset;

#define P_MAX_LOCALS 256
typedef struct FuncData {
  Type return_type;
  Str name;
  FuncParams params;

  int locals_offset;
  GenFixup locals_fixup;
  Sym locals[P_MAX_LOCALS];
  int num_locals;
} FuncData;
static FuncData p_cur_func;

#if 0
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
#endif

static void p_error(const char* message) {
  base_writef_stderr("%s:<offset %d>: error: %s\n", p_cur_filename, p_cur_offset, message);
  base_exit(1);
}

static Type make_type_ptr(uint32_t offset, Type subtype) {
  return (Type){0};
}

static Type make_type_array(uint32_t offset, uint64_t count, Type subtype) {
  return (Type){0};
}

static void p_advance(void) {
  ++p_cur_token_index;
  while (!p_token_kinds[p_cur_token_index]) {
    p_cur_token_index = 0;
    lex_next_block(p_token_kinds, p_token_offsets);
  }
  p_previous_kind = p_cur_kind;
  p_previous_offset = p_cur_offset;
  p_cur_kind = p_token_kinds[p_cur_token_index];
  p_cur_offset = p_token_offsets[p_cur_token_index];
}

static bool p_match(TokenKind tok_kind) {
  if (p_cur_kind != tok_kind)
    return false;
  p_advance();
  return true;
}

static bool p_check(TokenKind kind) {
  return p_cur_kind == kind;
}

static void p_consume(TokenKind tok_kind, const char* message) {
  if (p_cur_kind == tok_kind) {
    p_advance();
    return;
  }
  p_error(message);
}

static uint64_t p_const_expression(void) {
  return 0;
}

static Type p_basic_tok_to_type[NUM_TOKEN_KINDS] = {
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

static Type p_type(void) {
  if (p_match(TOK_STAR)) {
    return make_type_ptr(p_cur_offset, p_type());
  }
  if (p_match(TOK_LSQUARE)) {
    int count = 0;
    if (!p_check(TOK_RSQUARE)) {
      count = p_const_expression();
    }
    p_consume(TOK_RSQUARE, "Expect ']' to close array type.");
    Type elem = p_type();
    if (!elem.i) {
      p_error("Expecting type of array or slice.");
    }
    return make_type_array(p_cur_offset, count, elem);
  }

  if (p_match(TOK_LBRACE)) {
    abort();
  }

  if (p_match(TOK_DEF)) {
    abort();
  }

  if (p_cur_kind >= TOK_BOOL && p_cur_kind <= TOK_UINT) {
    Type t = p_basic_tok_to_type[p_cur_kind];
    ASSERT(t.i != 0);
    p_advance();
    return t;
  }
  return (Type){0};
}

static Str p_name(const char* err) {
  p_consume(TOK_IDENT_VAR, err);
  StrView view = lex_get_strview(p_previous_offset, p_cur_offset);
  ASSERT(view.size > 0);
  while (view.data[view.size - 1] == ' ') --view.size;
  memcpy(&p_strings[p_string_insert], view.data, view.size);
  int ret = p_string_insert;
  p_string_insert += view.size;
  return (Str){ret};
}

static FuncParams p_func_params(void) {
  FuncParams ret = {0};
  bool require_more = false;
  while (require_more || !p_check(TOK_RPAREN)) {
    require_more = false;
    Type param_type = p_type();
    if (!param_type.i) {
      p_advance();
      p_error("Expect function parameter.");
    }
    Str param_name = p_name("Expect parameter name.");
    ASSERT(param_name.i);
    ret.types[ret.num_params] = param_type;
    ret.names[ret.num_params] = param_name;
    ++ret.num_params;

    if (p_check(TOK_RPAREN)) {
      break;
    }
    p_consume(TOK_COMMA, "Expect ',' between function parameters.");
    require_more = true;
  }
  p_consume(TOK_RPAREN, "Expect ')' after function parameters.");
  return ret;
}

static void p_statement(bool toplevel);

static void p_enter_function(Type return_type, Str name, FuncParams params) {
  p_cur_func.return_type = return_type;
  p_cur_func.name = name;
  p_cur_func.params = params;
  p_cur_func.locals_offset = 0;
  p_cur_func.locals_fixup = gen_func_entry();
}

static void p_leave_function(void) {
  gen_func_exit_and_patch_func_entry(p_cur_func.locals_offset + 32, p_cur_func.locals_fixup);
  //gen_return(p_cur_func.return_type);
  p_cur_func.locals_fixup = (GenFixup){0};
  p_cur_func.return_type.i = 0;
  p_cur_func.name.i = 0;
  p_cur_func.params = (FuncParams){0};
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
      p_error("Integer literal overflow.");
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

static void p_number(bool can_assign) {
  Type suffix = {0};
  uint64_t val = scan_int(lex_get_strview(p_previous_offset, p_cur_offset), &suffix);

  gen_push_number(val, suffix);
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

static void p_precedence(Precedence precedence) {
  p_advance();
  bool can_assign = precedence <= PREC_ASSIGNMENT;

  // XXX
  if (p_previous_kind == TOK_INT_LITERAL) {
    p_number(can_assign);
  }
}

static void p_expression(void) {
  p_precedence(PREC_LOWEST);
}

static bool p_func_body_only_statement(void) {
  if (p_match(TOK_RETURN)) {
    ASSERT(p_cur_func.return_type.i);
    if (p_cur_func.return_type.i != TYPE_VOID) {
      p_expression();
    }
    gen_return(p_cur_func.return_type);
    return true;
  }

  return false;
}

static void p_block(void) {
  while (!p_check(TOK_DEDENT) && !p_check(TOK_EOF)) {
    p_statement(/*toplevel=*/false);
    while (p_match(TOK_NEWLINE)) {
    }
  }
  p_consume(TOK_DEDENT, "Expect end of block.");
}

// TODO: decorators
static void p_def(void) {
  Type return_type = p_type();
  Str name = p_name("Expect function name.");
  p_consume(TOK_LPAREN, "Expect '(' after function name.");
  FuncParams params = p_func_params();
  p_consume(TOK_COLON, "Expect ':' before function body.");
  p_consume(TOK_NEWLINE, "Expect newline before function body. (TODO: single line)");
  while (p_match(TOK_NEWLINE)) {
  }
  p_consume(TOK_INDENT, "Expect indented function body.");

  p_enter_function(return_type, name, params);
  p_block();
  p_leave_function();
}

static void p_var(Type type) {
  Str name = p_name("Expect variable or typed variable name.");
  ASSERT(name.i);

  bool have_init;
  if (!type.i) {
    p_consume(TOK_EQ, "Expect initializer for variable without type.");
    p_expression();
    have_init = true;
  } else {
    have_init = p_match(TOK_EQ);
    if (have_init) {
      p_expression();
    }
  }

  ASSERT(p_cur_func.name.i);
  ASSERT(p_cur_func.num_locals < COUNTOF(p_cur_func.locals));
  Sym* new = &p_cur_func.locals[p_cur_func.num_locals++];
  new->kind = SYM_VAR;
  new->name = name;
  new->type = type;
  new->is_local = true;
  // TODO: align, size, etc etc etc
  new->stack_offset = p_cur_func.locals_offset + 32;
  p_cur_func.locals_offset += 8;
  new->val.p = 0;
  if (have_init) {
    gen_store_local(new->stack_offset, type);
  }

  p_consume(TOK_NEWLINE, "Expect newline after variable declaration.");
}

static bool p_anywhere_statement(void) {
  if (p_match(TOK_DEF)) {
    p_def();
    return true;
  }

  Type var_type = p_type();
  if (var_type.i) {
    p_var(var_type);
    return true;
  }

  p_expression();
  p_consume(TOK_NEWLINE, "Expect newline.");
  return true;
}

static void p_statement(bool toplevel) {
  while (p_match(TOK_NEWLINE)) {
  }

  if (!toplevel) {
    if (p_func_body_only_statement()) {
      return;
    }
  }

  p_anywhere_statement();
}

static void init_types(void) {
  p_set_typedata_raw(TYPE_VOID, p_pack_type(TYPE_VOID, 0, 0));
  p_set_typedata_raw(TYPE_BOOL, p_pack_type(TYPE_BOOL, 1, 1));
  p_set_typedata_raw(TYPE_U8, p_pack_type(TYPE_U8, 1, 1));
  p_set_typedata_raw(TYPE_I8, p_pack_type(TYPE_I8, 1, 1));
  p_set_typedata_raw(TYPE_U16, p_pack_type(TYPE_U16, 2, 2));
  p_set_typedata_raw(TYPE_I16, p_pack_type(TYPE_I16, 2, 2));
  p_set_typedata_raw(TYPE_U32, p_pack_type(TYPE_U32, 4, 4));
  p_set_typedata_raw(TYPE_I32, p_pack_type(TYPE_I32, 4, 4));
  p_set_typedata_raw(TYPE_U64, p_pack_type(TYPE_U64, 8, 8));
  p_set_typedata_raw(TYPE_I64, p_pack_type(TYPE_I64, 8, 8));
  p_set_typedata_raw(TYPE_FLOAT, p_pack_type(TYPE_FLOAT, 4, 4));
  p_set_typedata_raw(TYPE_DOUBLE, p_pack_type(TYPE_DOUBLE, 8, 8));
  p_set_typedata_raw(TYPE_STR, p_pack_type(TYPE_STR, 16, 8));
  p_set_typedata_raw(TYPE_RANGE, p_pack_type(TYPE_STR, 24, 8));
  p_num_typedata = NUM_TYPE_KINDS;
}

void parse(const char* filename, ReadFileResult file) {
  init_types();

  gen_init();

  p_cur_filename = filename;
  p_cur_token_index = 0;
  lex_start(file.buffer, file.allocated_size);
  p_advance();

  while (p_cur_kind != TOK_EOF) {
    p_statement(/*toplevel=*/true);
  }

  gen_finish();
}
