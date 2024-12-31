static uint8_t token_kinds_[128] = {0};
static uint32_t token_offsets_[128] = {0};
static int cur_token_ = 0;

typedef struct Str {
  const char* p;
} Str;

typedef struct Typespec {
  int a;
} Typespec;

typedef struct FuncParams {
  int a;
} FuncParams;

#define cur_tok_kind() token_kinds_[cur_token_]

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

static void fatal_error(int tokid, const char* message) {
  fprintf(stderr, "<file>:%d: error: %s\n", token_offsets_[tokid], message);
  exit(1);
}

static void advance(void) {
  ++cur_token_;
  while (!token_kinds_[cur_token_]) {
    cur_token_ = 0;
    lex_next_block(token_kinds_, token_offsets_);
  }
}

static bool match(TokenKind tok_kind) {
  if (cur_tok_kind() != tok_kind)
    return false;
  advance();
  return true;
}

static void consume(TokenKind tok_kind, const char* message) {
  if (cur_tok_kind() == tok_kind) {
    advance();
    return;
  }
  fatal_error(cur_token_, message);
}

static Typespec parse_typespec(void) {
  return (Typespec){0};
}

static Str parse_name(const char* err) {
  return (Str){0};
}

static FuncParams parse_func_params(void) {
  return (FuncParams){0};
}

static void enter_function(Typespec return_type, Str name, FuncParams params) {
}

static void parse_block(void) {
}

static void leave_function(void) {
}

// TODO: decorators
static void parse_def(void) {
  Typespec return_type = parse_typespec();
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

static void parse_toplevel(void) {
  while (match(TOK_NEWLINE)) {
  }

  for (;;) {
    if (cur_tok_kind() == TOK_EOF) {
      break;
    }
    if (match(TOK_DEF)) {
      parse_def();
    } else {
      fatal_error(cur_token_, "Unexpected token.");
    }
  }
}

static void parse(ReadFileResult file) {
  cur_token_ = 0;
  lex_start(file.buffer, file.allocated_size);
  
  while (cur_tok_kind() != TOK_EOF) {
    parse_toplevel();
  }
}
