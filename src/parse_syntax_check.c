#define SQBE_NOOP

#include "../third_party/sqbe/sqbe.h"

#pragma GCC diagnostic ignored "-Wunused-variable"

#include "parse.c"

void parse_one_time_initialization_syntax_check(Arena* main_arena, int verbose, bool obj_output) {
  parse_one_time_initialization_impl(main_arena, verbose, obj_output);
}

void parse_syntax_check(Arena* temp_arena, Module module) {
  parse_impl(temp_arena, module);
}
