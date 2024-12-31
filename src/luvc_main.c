#include "base.c"

typedef enum TokenKind {
#define TOKEN(n) TOK_##n,
#include "tokens.inc"
#undef TOKEN
  NUM_TOKEN_KINDS,
} TokenKind;

#include "lex.c"
#include "parse.c"

int main() {
  const char* filename = "a.luv";
  ReadFileResult file = base_read_file(filename);
  if (!file.buffer) {
    base_writef_stderr("Couldn't read '%s'\n", filename);
    return 1;
  }

  parse(file);
  return 0;
}
