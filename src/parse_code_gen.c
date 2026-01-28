#include "../third_party/sqbe/sqbe.h"

#include "parse.c"

void parse_one_time_initialization_code_gen(Arena* arena) {
  parse_one_time_initialization_impl(arena);
}

void parse_code_gen(Arena* temp_arena,
                    const char* filename,
                    ReadFileResult file,
                    int verbose,
                    FILE* out_file) {
  parse_impl(temp_arena, filename, file, verbose, out_file);
}
