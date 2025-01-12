#include "luv60.h"

#include "../third_party/mir/mir.h"
#include "../third_party/mir/mir-gen.h"
#include "../third_party/mir/real-time.h"
#include <inttypes.h>

static MIR_context_t ctx;
static MIR_module_t main_module;
static Str entry_point_str;
static MIR_item_t current_func;
static MIR_item_t entry_point;
static bool verbose_;

_Static_assert(sizeof(IROp) == sizeof(MIR_op_t), "opaque size wrong");

static void do_mir_test() {
  double start_time;
  const int64_t n_iter = 10000000;

  MIR_item_t func;
  MIR_label_t fin, cont;
  MIR_reg_t ARG1, R2;
  MIR_type_t res_type;

  res_type = MIR_T_I64;
  func = MIR_new_func(ctx, "loop", 1, &res_type, 1, MIR_T_I64, "arg1");
  R2 = MIR_new_func_reg(ctx, func->u.func, MIR_T_I64, "count");
  ARG1 = MIR_reg(ctx, "arg1", func->u.func);
  fin = MIR_new_label(ctx);
  cont = MIR_new_label(ctx);
  MIR_append_insn(ctx, func,
                  MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, R2), MIR_new_int_op(ctx, 0)));
  MIR_append_insn(ctx, func,
                  MIR_new_insn(ctx, MIR_BGE, MIR_new_label_op(ctx, fin), MIR_new_reg_op(ctx, R2),
                               MIR_new_reg_op(ctx, ARG1)));
  MIR_append_insn(ctx, func, cont);
  MIR_append_insn(ctx, func,
                  MIR_new_insn(ctx, MIR_ADD, MIR_new_reg_op(ctx, R2), MIR_new_reg_op(ctx, R2),
                               MIR_new_int_op(ctx, 1)));
  MIR_append_insn(ctx, func,
                  MIR_new_insn(ctx, MIR_BLT, MIR_new_label_op(ctx, cont), MIR_new_reg_op(ctx, R2),
                               MIR_new_reg_op(ctx, ARG1)));
  MIR_append_insn(ctx, func, fin);
  MIR_append_insn(ctx, func, MIR_new_ret_insn(ctx, 1, MIR_new_reg_op(ctx, R2)));
  MIR_finish_func(ctx);
  if (main_module != NULL) {
    MIR_finish_module(ctx);
  }

  fprintf(stderr, "++++++ Loop before simplification:\n");
  MIR_output(ctx, stderr);

  MIR_gen_init(ctx);
  MIR_gen_set_optimize_level(ctx, 1);
  MIR_gen_set_debug_file(ctx, NULL);
  MIR_gen_set_debug_level(ctx, 0);

  MIR_load_module(ctx, main_module);
  // MIR_link(ctx, MIR_set_gen_interface, NULL);

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

void gen_mir_init(bool verbose) {
  ctx = MIR_init();
  main_module = MIR_new_module(ctx, "main");

  verbose_ = verbose;
  MIR_gen_init(ctx);
  MIR_gen_set_optimize_level(ctx, 1);
  if (verbose) {
    MIR_gen_set_debug_file(ctx, stderr);
    MIR_gen_set_debug_level(ctx, 0);
  } else {
    MIR_gen_set_debug_file(ctx, NULL);
    MIR_gen_set_debug_level(ctx, -1);
  }

  entry_point_str = str_intern("main");
  (void)do_mir_test;
}

static MIR_type_t type_to_mir_type(Type type) {
  switch (type_kind(type)) {
    case TYPE_U64:
    case TYPE_U32:
    case TYPE_U16:
    case TYPE_U8:
    case TYPE_BOOL:
      return MIR_T_U64;
    case TYPE_I64:
    case TYPE_I32:
    case TYPE_I16:
    case TYPE_I8:
      return MIR_T_I64;
    default:
      base_writef_stderr("%s: %s\n", __FUNCTION__, type_as_str(type));
      ASSERT(false && "todo");
      abort();
  }
}

static bool is_aggregate_type(Type type) {
  switch (type_kind(type)) {
    case TYPE_U64:
    case TYPE_I64:
    case TYPE_U32:
    case TYPE_I32:
    case TYPE_U16:
    case TYPE_I16:
    case TYPE_U8:
    case TYPE_I8:
    case TYPE_BOOL:
    case TYPE_FUNC:
      return false;
    case TYPE_STR:
    case TYPE_RANGE:
      return true;
    default:
      base_writef_stderr("%s: %s\n", __FUNCTION__, type_as_str(type));
      ASSERT(false && "todo");
      abort();
  }
}

#define OP_OPAQUE_TO_MIR(o) *((MIR_op_t*)o.data)
#define OP_MIR_TO_OPAQUE(m) *((IROp*)&m)

#define REG_OPAQUE_TO_MIR(o) ((MIR_reg_t)o.u)
#define REG_MIR_TO_OPAQUE(m) ((IRReg){m})

IRFunc gen_mir_start_function(Str name,
                              Type return_type,
                              int num_args,
                              Type* param_types,
                              Str* param_names) {
  ASSERT(!current_func);

  MIR_var_t* vars = alloca(sizeof(MIR_var_t) * num_args);
  for (int i = 0; i < num_args; ++i) {
    vars[i].type = type_to_mir_type(param_types[i]);
    vars[i].name = cstr(param_names[i]);
  }
  MIR_type_t rettype = type_to_mir_type(return_type);
  MIR_item_t func = MIR_new_func_arr(ctx, cstr(name), 1, &rettype, num_args, vars);

  current_func = func;
  if (str_eq(name, entry_point_str)) {
    entry_point = func;
  }

  Str proto_name = str_internf("%s$proto", cstr(name));
  MIR_item_t proto = MIR_new_proto_arr(ctx, cstr(proto_name), 1, &rettype, num_args, vars);
  return (IRFunc){(IRModuleItem)func, (IRModuleItem)proto};
}

IRReg gen_mir_make_temp(Str name, Type type) {
  // TODO: check that it doesn't match t%d as apparently those are reserved?
  if (is_aggregate_type(type)) {
    ASSERT(false && "todo; aggregate");
    abort();
  } else {
    MIR_reg_t reg = MIR_new_func_reg(ctx, current_func->u.func, type_to_mir_type(type), cstr(name));
    return REG_MIR_TO_OPAQUE(reg);
  }
}

IRReg gen_mir_get_func_param(Str name, Type type) {
  MIR_reg_t reg = MIR_reg(ctx, cstr(name), current_func->u.func);
  // TODO: sanity check with type?
  (void)type;
  return REG_MIR_TO_OPAQUE(reg);
}

IROp gen_mir_op_const(uint64_t val, Type type) {
  switch (type_kind(type)) {
    case TYPE_U64:
    case TYPE_I64:
    case TYPE_U32:
    case TYPE_I32:
    case TYPE_U16:
    case TYPE_I16:
    case TYPE_U8:
    case TYPE_I8:
    case TYPE_BOOL: {
      MIR_op_t mret = MIR_new_uint_op(ctx, val);
      return OP_MIR_TO_OPAQUE(mret);
    }
    default:
      ASSERT(false && "todo");
      abort();
  }
}

IROp gen_mir_op_str_const(Str str) {
  ASSERT(false && "str const");
  abort();
}

IROp gen_mir_op_reg(IRReg reg) {
  MIR_op_t mret = MIR_new_reg_op(ctx, REG_OPAQUE_TO_MIR(reg));
  return OP_MIR_TO_OPAQUE(mret);
}

void gen_mir_instr_return(IROp val, Type type) {
  MIR_insn_t insn;
  if (type_eq(type, type_void)) {
    insn = MIR_new_ret_insn(ctx, 0);
  } else {
    insn = MIR_new_ret_insn(ctx, 1, val);
  }
  MIR_append_insn(ctx, current_func, insn);
}

void gen_mir_instr_store(IRReg lhs, Type type, IROp val) {
  MIR_insn_t insn = MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, REG_OPAQUE_TO_MIR(lhs)), val);
  MIR_append_insn(ctx, current_func, insn);
}

void gen_mir_instr_call(IRFunc func,
                        Type return_type,
                        IRReg retreg,
                        uint32_t num_args,
                        IROp* arg_values) {
  const bool is_void = type_kind(return_type) == TYPE_VOID;
  uint32_t extra = is_void ? 2 : 3;
  MIR_op_t* all_ops = alloca(sizeof(MIR_op_t) * (num_args + extra));
  all_ops[0] = MIR_new_ref_op(ctx, (MIR_item_t)func.proto);
  all_ops[1] = MIR_new_ref_op(ctx, (MIR_item_t)func.func);
  if (!is_void) {
    all_ops[2] = MIR_new_reg_op(ctx, REG_OPAQUE_TO_MIR(retreg));
  }
  for (uint32_t i = 0; i < num_args; ++i) {
    all_ops[i + extra] = OP_OPAQUE_TO_MIR(arg_values[i]);
  }
  MIR_insn_t insn = MIR_new_insn_arr(ctx, MIR_CALL, num_args + extra, all_ops);
  MIR_append_insn(ctx, current_func, insn);
}

void gen_mir_end_current_function(void) {
  ASSERT(current_func);
  if (verbose_) {
    MIR_output(ctx, stderr);
  }
  MIR_finish_func(ctx);
  current_func = NULL;
}

int gen_mir_finish(void) {
  MIR_load_module(ctx, main_module);
  MIR_link(ctx, MIR_set_gen_interface, NULL);

  if (!entry_point) {
    base_writef_stderr("No 'main' defined.");
    base_exit(1);
  }
  int rc = ((int(*)())entry_point->addr)();
  return rc;
}
