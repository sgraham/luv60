#include "base.c"

#include "os.c"

typedef enum TokenKind {
#define TOKEN(n) TOK_##n,
#include "tokens.inc"
#undef TOKEN
  NUM_TOKEN_KINDS,
} TokenKind;

#include "lex.c"

int main() {
  ReadFileResult file = read_file("a.luv");
  if (!file.buffer) {
    fprintf(stderr, "Couldn't read file.\n");
    return 1;
  }

  uint8_t token_kinds[128] = {0};
  uint32_t token_offsets[128] = {0};
  size_t count = 0;
  lex_start(file.buffer, file.allocated_size);
  for (;;) {
    lex_next_block(token_kinds, token_offsets);
    for (int i = 0; i < 128; ++i) {
      TokenKind kind = token_kinds[i];
      if (kind == TOK_EOF) {
        goto done;
      }
      if (kind) {
        ++count;
      } else {
        break; // INVALID, next block.
      }
    }
  }
done:;
  fprintf(stderr, "lex'd %zu tokens\n", count);
  return 0;
}
