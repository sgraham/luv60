#include "luv60.h"

TokenizedBuffer parse_scan_for_imports(const char* filename, ReadFileResult file, int verbose) {
  TokenizedBuffer tb = {
      .filename = filename,
      .file_contents = (const char*)file.buffer,
      // In the case of "a.a." the worst case for offsets is the same as the number
      // of characters in the buffer.
      .token_offsets = (uint32_t*)base_mem_large_alloc(file.allocated_size * sizeof(uint32_t)),
      .cursor = (TokenCursor){-1, 0, 0, 0},
  };
  tb.num_tokens = lex_indexer(file.buffer, file.allocated_size, tb.token_offsets);
  token_init(file.buffer);
  if (verbose > 1) {
    token_dump_offsets(tb.num_tokens, tb.token_offsets, file.file_size);
  }
  return tb;
}
