#include "../third_party/sqbe/sqbe.h"

#include "parse.c"

void parse_one_time_initialization_code_gen(Arena* main_arena, int verbose, bool obj_output) {
  parse_one_time_initialization_impl(main_arena, verbose, obj_output);
}

void parse_code_gen(Arena* temp_arena, Module module) {
  parse_impl(temp_arena, module);
}
