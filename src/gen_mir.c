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
static MIR_item_t printf_proto;
static MIR_item_t printf_import;
static MIR_item_t memcpy_proto;
static MIR_item_t memcpy_import;
static int gensym_counter;

_Static_assert(sizeof(IROp) == sizeof(MIR_op_t), "opaque size wrong");

static void NORETURN error_function(MIR_error_type_t error_type, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  size_t n = 1 + vsnprintf(NULL, 0, fmt, args);
  va_end(args);
  char* str = malloc(n);  // just a simple malloc because we're going to base_exit() momentarily.
  va_start(args, fmt);
  vsnprintf(str, n, fmt, args);
  va_end(args);
  parse_errorf("somewhere near here, mir reported backend error `%s` (err %d)\n", str, error_type);
}

static const char* gensym(void) {
  return cstr(str_internf("$s%d", gensym_counter++));
}

void gen_mir_init(bool verbose) {
  ctx = MIR_init();
  main_module = MIR_new_module(ctx, "home");

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
  MIR_set_error_func(ctx, error_function);

  MIR_type_t printf_ret_type = MIR_T_I32;
  printf_proto = MIR_new_vararg_proto(ctx, "printf$proto", 1, &printf_ret_type, 0);
  printf_import = MIR_new_import(ctx, "printf");
  MIR_type_t memcpy_ret_type = MIR_T_P;
  memcpy_proto = MIR_new_proto(ctx, "memcpy$proto", 1, &memcpy_ret_type, 3, MIR_T_P, "dest",
                               MIR_T_P, "src", MIR_T_U64, "count");
  memcpy_import = MIR_new_import(ctx, "memcpy");

  entry_point_str = str_intern("main");
}

static MIR_type_t type_to_mir_type(Type type) {
  switch (type_kind(type)) {
    case TYPE_U64:
    case TYPE_U32:
    case TYPE_U16:
    case TYPE_U8:
    case TYPE_BOOL:
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

#define RET_ADDR_NAME "Ret_Addr"

IRFunc gen_mir_start_function(Str name,
                              Type return_type,
                              int num_args,
                              Type* param_types,
                              Str* param_names) {
  ASSERT(!current_func);

  int extra = 0;
  if (is_aggregate_type(return_type)) {
    extra = 1;
  }

  MIR_var_t* vars = alloca(sizeof(MIR_var_t) * (num_args + extra));
  for (int i = 0; i < num_args; ++i) {
    vars[i].type = type_to_mir_type(param_types[i]);
    vars[i].name = cstr(param_names[i]);
  }

  // Aggregate returns go in the args, not the rets.
  size_t nrets = 0;
  MIR_type_t rettype = {0};
  if (extra) {
    vars[num_args].type = MIR_T_RBLK;
    vars[num_args].name = RET_ADDR_NAME;
    vars[num_args].size = type_size(return_type);
  } else {
    rettype = type_to_mir_type(return_type);
  }

  MIR_item_t func = MIR_new_func_arr(ctx, cstr(name), nrets, &rettype, num_args + extra, vars);

  current_func = func;
  if (str_eq(name, entry_point_str)) {
    entry_point = func;
  }

  Str proto_name = str_internf("%s$proto", cstr(name));
  MIR_item_t proto =
      MIR_new_proto_arr(ctx, cstr(proto_name), nrets, &rettype, num_args + extra, vars);
  return (IRFunc){(IRModuleItem)func, (IRModuleItem)proto};
}

IRReg gen_mir_make_temp(Str name, Type type) {
  // TODO: check that it doesn't match t%d as apparently those are reserved?
  if (is_aggregate_type(type)) {
    MIR_reg_t reg = MIR_new_func_reg(ctx, current_func->u.func, MIR_T_I64, cstr(name));
    MIR_append_insn(ctx, current_func,
                    MIR_new_insn(ctx, MIR_ALLOCA, MIR_new_reg_op(ctx, reg),
                                 MIR_new_uint_op(ctx, type_size(type))));
    return REG_MIR_TO_OPAQUE(reg);
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
  MIR_item_t str_item = MIR_new_string_data(ctx, gensym(), (MIR_str_t){str_len(str), cstr(str)});
  MIR_op_t str_op = MIR_new_ref_op(ctx, str_item);
  return OP_MIR_TO_OPAQUE(str_op);
}

IROp gen_mir_op_reg(IRReg reg) {
  MIR_op_t mret = MIR_new_reg_op(ctx, REG_OPAQUE_TO_MIR(reg));
  return OP_MIR_TO_OPAQUE(mret);
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
    if (is_aggregate_type(return_type)) {
    } else {
      all_ops[2] = MIR_new_reg_op(ctx, REG_OPAQUE_TO_MIR(retreg));
    }
  }
  for (uint32_t i = 0; i < num_args; ++i) {
    all_ops[i + extra] = OP_OPAQUE_TO_MIR(arg_values[i]);
  }
  MIR_insn_t insn = MIR_new_insn_arr(ctx, MIR_CALL, num_args + extra, all_ops);
  MIR_append_insn(ctx, current_func, insn);
}

void gen_mir_helper_memcpy(IROp dest, IROp src, size_t size) {
  MIR_op_t args[6];
  args[0] = MIR_new_ref_op(ctx, memcpy_proto);
  args[1] = MIR_new_ref_op(ctx, memcpy_import);
  MIR_reg_t memcpy_return = MIR_new_func_reg(ctx, current_func->u.func, MIR_T_I64, gensym());
  args[2] = MIR_new_reg_op(ctx, memcpy_return);
  args[3] = OP_OPAQUE_TO_MIR(dest);
  args[4] = OP_OPAQUE_TO_MIR(src);
  args[5] = MIR_new_uint_op(ctx, size);
  MIR_append_insn(ctx, current_func, MIR_new_insn_arr(ctx, MIR_CALL, 6, args));
}

void gen_mir_instr_return(IROp val, Type type) {
  MIR_insn_t insn;
  bool is_agg = is_aggregate_type(type);
  if (type_eq(type, type_void) || is_agg) {
    insn = MIR_new_ret_insn(ctx, 0);
    if (is_agg) {
      MIR_reg_t ret_addr_reg = MIR_reg(ctx, RET_ADDR_NAME, current_func->u.func);
      MIR_op_t ret_op = MIR_new_mem_op(ctx, MIR_T_I8, 0, ret_addr_reg, 0, 1);
      gen_mir_helper_memcpy(OP_MIR_TO_OPAQUE(ret_op), OP_MIR_TO_OPAQUE(val), type_size(type));
    }
  } else {
    insn = MIR_new_ret_insn(ctx, 1, val);
  }
  MIR_append_insn(ctx, current_func, insn);
}

void gen_mir_helper_print_int(IROp val) {
  MIR_reg_t ret = MIR_new_func_reg(ctx, current_func->u.func, MIR_T_I64, gensym());
  MIR_append_insn(ctx, current_func,
                  MIR_new_call_insn(ctx, 5, MIR_new_ref_op(ctx, printf_proto),
                                    MIR_new_ref_op(ctx, printf_import), MIR_new_reg_op(ctx, ret),
                                    MIR_new_str_op(ctx, (MIR_str_t){3, "%d\n"}), val));
}

void gen_mir_helper_print_range(IROp val) {
  abort();
}

void gen_mir_helper_print_str(IROp val) {
  MIR_reg_t ret = MIR_new_func_reg(ctx, current_func->u.func, MIR_T_I64, gensym());
  MIR_append_insn(ctx, current_func,
                  MIR_new_call_insn(ctx, 5, MIR_new_ref_op(ctx, printf_proto),
                                    MIR_new_ref_op(ctx, printf_import), MIR_new_reg_op(ctx, ret),
                                    MIR_new_str_op(ctx, (MIR_str_t){3, "%s\n"}), val));
}

void gen_mir_end_current_function(void) {
  ASSERT(current_func);
  if (verbose_) {
    MIR_output(ctx, stderr);
  }
  MIR_finish_func(ctx);
  current_func = NULL;
}

void* import_resolver(const char* name) {
  if (strcmp(name, "printf") == 0) {
    return (void*)&printf;
  }
  if (strcmp(name, "memcpy") == 0) {
    return (void*)&memcpy;
  }
  return NULL;
}

int gen_mir_finish(void) {
  MIR_load_module(ctx, main_module);
  MIR_link(ctx, MIR_set_gen_interface, import_resolver);

  if (!entry_point) {
    base_writef_stderr("No 'main' defined.");
    base_exit(1);
  }
  int rc = ((int (*)())entry_point->addr)();
  return rc;
}
