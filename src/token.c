#include "luv60.h"

static const unsigned char* token_file_contents;
static int token_continuation_paren_level;

void token_init(const unsigned char* file_contents) {
  token_file_contents = file_contents;
  token_continuation_paren_level = 0;
}

int token_get_continuation_paren_level(void) {
  return token_continuation_paren_level;
}

void token_restore_continuation_paren_level(int level) {
  token_continuation_paren_level = level;
}

TokenKind token_categorize(uint32_t offset) {
  const unsigned char* p = &token_file_contents[offset];
  const unsigned char* q;

#define NEWLINE_INDENT_ADJUST(n)        \
  if (token_continuation_paren_level) { \
    return TOK_NL;                      \
  } else {                              \
    return TOK_NEWLINE_INDENT_##n;      \
  }

#include "categorizer.c"
}

static void print_with_visible_unprintable(char ch) {
  if (ch == '\n') {
    base_writef_stderr("↵\n");
  } else if (ch == ' ') {
    base_writef_stderr("⚬");
  } else if (ch < ' ' || ch >= 0x7f) {
    base_writef_stderr("·");
  } else {
    base_writef_stderr("%c", ch);
  }
}

void token_dump_offsets(uint32_t num_tokens, uint32_t* token_offsets, size_t file_size) {
  base_writef_stderr("TOKEN OFFSETS (%u tokens)\n", num_tokens);
  uint32_t cur_tok = 0;
  for (uint32_t offset = 0; offset < file_size; ++offset) {
    if (token_offsets[cur_tok] == offset) {
      base_writef_stderr("\033[31m");  // red
      ++cur_tok;
    } else {
      base_writef_stderr("\033[0m");  // default
    }
    print_with_visible_unprintable(token_file_contents[offset]);
  }
  base_writef_stderr("\033[0m");
}

static const char* token_names[NUM_TOKEN_KINDS] = {
#define TOKEN(n) #n,
#include "tokens.inc"
#undef TOKEN
};

const char* token_enum_name(TokenKind kind) {
  ASSERT(kind >= 0 && kind < NUM_TOKEN_KINDS);
  return token_names[kind];
}
