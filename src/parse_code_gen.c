#define ENABLE_CODE_GEN 1

#include "../third_party/ir/ir.h"
#include "../third_party/ir/ir_builder.h"

#undef _ir_CTX
#define _ir_CTX (&parser.cur_scope->ctx)

#include "parse.c"

void* parse_code_gen(Arena* main_arena,
                     Arena* temp_arena,
                     const char* filename,
                     ReadFileResult file,
                     void* (*get_extern)(StrView),
                     int verbose,
                     bool ir_only,
                     int opt_level) {
  return parse_impl(main_arena, temp_arena, filename, file, get_extern, verbose, ir_only,
                    opt_level);
}
