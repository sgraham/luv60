#include "../third_party/sqbe/sqbe.h"

#include "parse.c"

void parse_one_time_initialization_code_gen(Arena* main_arena) {
  parse_one_time_initialization_impl(main_arena);
}

void parse_code_gen(Arena* temp_arena, TokenizedBuffer tokbuf, int verbose, FILE* out_file) {
  parse_impl(temp_arena, tokbuf, verbose, out_file);
}
