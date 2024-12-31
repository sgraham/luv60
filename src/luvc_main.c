#include "base.c"

#include "os.c"

typedef enum TokenKind {
#define TOKEN(n) TOK_##n,
#include "tokens.inc"
#undef TOKEN
  NUM_TOKEN_KINDS,
} TokenKind;

#include "lex.c"
#include "parse.c"

int main() {
  ReadFileResult file = read_file("a.luv");
  if (!file.buffer) {
    fprintf(stderr, "Couldn't read file.\n");
    return 1;
  }

  parse(file);
  return 0;
}
