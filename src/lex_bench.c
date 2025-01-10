#include "luv60.h"

int main(int argc, char** argv) {
#if BUILD_DEBUG
  base_writef_stderr("warning: this is a debug build, probably not a useful benchmark binary.\n");
#endif

  const char* filename = "a.luv";//FILENAME;
  ReadFileResult file = base_read_file(filename);
  if (!file.buffer) {
    base_writef_stderr("Couldn't read '%s'\n", filename);
    return 1;
  }

  ASSERT(file.allocated_size <= UINT32_MAX);

  // In the worst case of input "x.x.", the token_offsets has the same number of
  // elements as the number of bytes in the input.
  uint32_t* token_offsets = (uint32_t*)base_large_alloc_rw(file.allocated_size * sizeof(uint32_t));
  uint32_t count =
      lex_indexer((const uint8_t*)file.buffer, (uint32_t)file.allocated_size, token_offsets);

#if 0
  for (uint32_t i = 0; i < count; ++i) {
    TokenKind kind = lex_categorize(file.buffer, token_offsets[i], token_offsets[i + 1]);
    if (kind == EOF) {
      break;
    }
  }
#endif

#if 0
  /*for (int i = 0; i < 50; ++i)*/ {
    uint8_t token_kinds[66] = {0};
    uint32_t token_offsets[66] = {0};
    size_t count = 0;
    lex_start((const uint8_t*)file.buffer, file.allocated_size);
    for (;;) {
      lex_next_block(token_kinds, token_offsets);
      for (int i = 0; i < 66; ++i) {
        TokenKind kind = token_kinds[i];
        if (kind == TOK_EOF) {
          goto done;
        }
        if (kind) {
          ++count;
        } else {
          break;
        }
      }
    }
  }
#endif
  base_writef_stderr("done lex %zu tokens\n", count);
}
