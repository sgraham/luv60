#include "../third_party/sqbe/sqbe.h"

#include "parse.c"

void parse_one_time_initialization_code_gen(Arena* main_arena) {
  parse_one_time_initialization_impl(main_arena);
}

void parse_code_gen(Arena* temp_arena,
                    const char* filename,
                    ReadFileResult file,
                    int verbose,
                    FILE* (*open_output_for)(const char* input)) {
  parse_impl(temp_arena, filename, file, verbose, open_output_for);
}
