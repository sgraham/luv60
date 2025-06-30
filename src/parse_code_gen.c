#define ENABLE_CODE_GEN 1

#include "../third_party/sqbe/sqbe.h"

#include "parse.c"

void parse_code_gen(Arena* main_arena,
                     Arena* temp_arena,
                     const char* filename,
                     ReadFileResult file,
                     int verbose,
                     FILE* out_file) {
  parse_impl(main_arena, temp_arena, filename, file, verbose, out_file);
}
