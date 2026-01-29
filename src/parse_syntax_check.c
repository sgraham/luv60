#define SQBE_NOOP

#include "../third_party/sqbe/sqbe.h"

#pragma GCC diagnostic ignored "-Wunused-variable"

#include "parse.c"

void parse_one_time_initialization_syntax_check(Arena* main_arena) {
  parse_one_time_initialization_impl(main_arena);
}

void parse_syntax_check(Arena* temp_arena, const char* filename, ReadFileResult file, int verbose) {
  parse_impl(temp_arena, filename, file, verbose, NULL);
}
