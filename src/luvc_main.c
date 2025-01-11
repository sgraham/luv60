#include "luv60.h"
#include <inttypes.h>

#include "../third_party/mir/mir.h"
#include "../third_party/mir/mir-gen.h"
#include "../third_party/mir/real-time.h"

static void usage(void) {
  base_writef_stderr("usage: luvc file.luv file.ssa\n");
  base_exit(1);
}

static void do_mir_test() {
  double start_time;
  const int64_t n_iter = 10000000;
  MIR_context_t ctx = MIR_init();

  MIR_module_t m;
  MIR_item_t func;
  MIR_label_t fin, cont;
  MIR_reg_t ARG1, R2;
  MIR_type_t res_type;

  m = MIR_new_module (ctx, "m");
  res_type = MIR_T_I64;
  func = MIR_new_func (ctx, "loop", 1, &res_type, 1, MIR_T_I64, "arg1");
  R2 = MIR_new_func_reg (ctx, func->u.func, MIR_T_I64, "count");
  ARG1 = MIR_reg (ctx, "arg1", func->u.func);
  fin = MIR_new_label (ctx);
  cont = MIR_new_label (ctx);
  MIR_append_insn (ctx, func,
                   MIR_new_insn (ctx, MIR_MOV, MIR_new_reg_op (ctx, R2), MIR_new_int_op (ctx, 0)));
  MIR_append_insn (ctx, func,
                   MIR_new_insn (ctx, MIR_BGE, MIR_new_label_op (ctx, fin),
                                 MIR_new_reg_op (ctx, R2), MIR_new_reg_op (ctx, ARG1)));
  MIR_append_insn (ctx, func, cont);
  MIR_append_insn (ctx, func,
                   MIR_new_insn (ctx, MIR_ADD, MIR_new_reg_op (ctx, R2), MIR_new_reg_op (ctx, R2),
                                 MIR_new_int_op (ctx, 1)));
  MIR_append_insn (ctx, func,
                   MIR_new_insn (ctx, MIR_BLT, MIR_new_label_op (ctx, cont),
                                 MIR_new_reg_op (ctx, R2), MIR_new_reg_op (ctx, ARG1)));
  MIR_append_insn (ctx, func, fin);
  MIR_append_insn (ctx, func, MIR_new_ret_insn (ctx, 1, MIR_new_reg_op (ctx, R2)));
  MIR_finish_func (ctx);
  if (m != NULL) MIR_finish_module (ctx);

  fprintf(stderr, "++++++ Loop before simplification:\n");
  MIR_output(ctx, stderr);

  MIR_gen_init(ctx);
  MIR_gen_set_optimize_level (ctx, 1);
  MIR_gen_set_debug_file (ctx, NULL);
  MIR_gen_set_debug_level(ctx, 0);

  MIR_load_module(ctx, m);
  //MIR_link(ctx, MIR_set_gen_interface, NULL);

  fprintf(stderr, "++++++ Loop after simplification:\n");
  MIR_output(ctx, stderr);

  //start_time = real_sec_time();
  typedef int64_t (*loop_func)(int64_t);
  start_time = real_sec_time();
  void* p = MIR_gen(ctx, func);
  int64_t res = ((loop_func)p)(n_iter);
  fprintf(stderr, "C interface test (%" PRId64 ") -> %" PRId64 ": %.3f sec\n", n_iter, res,
          real_sec_time() - start_time);
  MIR_finish(ctx);
}

int main(int argc, char** argv) {
  if (argc != 3) {
    usage();
  }

  if (strcmp(argv[1], "-mirtest") == 0) {
    do_mir_test();
    return 0;
  }

  str_intern_pool_init();

  const char* filename = argv[1];
  ReadFileResult file = base_read_file(filename);
  if (!file.buffer) {
    base_writef_stderr("Couldn't read '%s'\n", filename);
    return 1;
  }

  gen_ssa_init();

  parse(filename, file);

  gen_ssa_finish(argv[2]);
  return 0;
}
