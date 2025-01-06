#include "luv60.h"

int main(int argc, char** argv) {
#if BUILD_DEBUG
  base_writef_stderr("warning: this is a debug build, probably not a useful benchmark binary.\n");
#endif

  const char* filename = FILENAME;
  ReadFileResult file = base_read_file(filename);
  if (!file.buffer) {
    base_writef_stderr("Couldn't read '%s'\n", filename);
    return 1;
  }

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
  done:;
    base_writef_stderr("done lex %zu tokens\n", count);
  }
}
