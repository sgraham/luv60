#include "luv60.h"

static const unsigned char* token_file_contents;
static int token_continuation_paren_level;

void token_init(const unsigned char* file_contents) {
  token_file_contents = file_contents;
  token_continuation_paren_level = 0;
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
