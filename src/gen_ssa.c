#include "luv60.h"

static FILE* outf;
static Str main_func_name;

typedef enum IRRefKind {
  REF_TEMP,
  REF_FUNC,
  REF_STR_CONST,
} IRRefKind;

typedef struct IRRefData {
  IRRefKind kind;
  Type type;
  Str str;
} IRRefData;
static IRRefData refs[1<<24];
static int num_refs;

//typedef struct IRBlockData {
//} IRBlockData;
static int num_blocks;

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

IRRef gen_ssa_make_temp(Type type) {
  ASSERT(num_refs < COUNTOFI(refs));
  IRRef ret = {num_refs++};
  IRRefData* data = &refs[ret.i];
  data->kind = REF_TEMP;
  data->type = type;
  if (is_aggregate_type(type)) {
    gen_ssa_alloc_local(type_size(type), type_align(type), ret);
  }
  return ret;
}

static IRRef gen_ssa_make_str_const(Str str) {
  ASSERT(num_refs < COUNTOFI(refs));
  IRRef ret = {num_refs++};
  IRRefData* data = &refs[ret.i];
  data->kind = REF_STR_CONST;
  data->type = type_str;
  data->str = str;
  return ret;
}

IRRef gen_ssa_make_func(Str name, Type type) {
  ASSERT(num_refs < COUNTOFI(refs));
  IRRef ret = {num_refs++};
  IRRefData* data = &refs[ret.i];
  data->kind = REF_FUNC;
  data->type = type;
  data->str = name;
  return ret;
}

IRBlock gen_ssa_make_block_name(void) {
  IRBlock ret = {num_blocks++};
  return ret;
}

void gen_ssa_start_block(IRBlock block) {
  fprintf(outf, "@b%d\n", block.i);
}

static const char* irref_as_str(IRRef ref) {
  char buf[128];
  switch (refs[ref.i].kind) {
    case REF_TEMP:
      sprintf(buf, "%%t%d", ref.i);
      break;
    case REF_FUNC:
      sprintf(buf, "$%s", cstr(refs[ref.i].str));
      break;
    case REF_STR_CONST:
      sprintf(buf, "$str%d", ref.i);
      break;
    default:
      buf[0] = '?';
      buf[1] = 0;
      ASSERT(false && "unhandled ref kind");
  }
  return _strdup(buf);
}

static const char* irblock_as_str(IRBlock block) {
  char buf[128];
  sprintf(buf, "@b%d", block.i);
  return _strdup(buf);
}

void gen_ssa_init(const char* filename) {
  num_refs = 0;
  num_blocks = 0;
  main_func_name = str_intern_len("main", 4);
  outf = fopen(filename, "wb");
  fprintf(outf, "type :Str = { l, l } # data, length\n");
  fprintf(outf, "type :Range = { l, l, l } # start, stop, step\n");
  fprintf(outf, "data $intfmt = { b \"%%d\\n\", z 1 }\n");
  fprintf(outf, "data $strfmt = { b \"%%.*s\\n\", z 1 }\n");
  fprintf(outf, "data $range2fmt = { b \"range(%%lld, %%lld)\\n\", z 1 }\n");
  fprintf(outf, "data $range3fmt = { b \"range(%%lld, %%lld, %%lld)\\n\", z 1 }\n");
}

#if 0
static const char* type_to_qbe_data_type(Type type) {
  switch (type_kind(type)) {
    case TYPE_U64:
    case TYPE_I64:
      return "l";
    case TYPE_U32:
    case TYPE_I32:
      return "w";
    case TYPE_U16:
    case TYPE_I16:
      return "h";
    case TYPE_U8:
    case TYPE_I8:
    case TYPE_BOOL:
      return "b";
    case TYPE_STR:
      return ":Str";
    default:
      ASSERT(false && "todo");
      return "?";
  }
}
#endif

static const char* type_to_qbe_return_type(Type type) {
  switch (type_kind(type)) {
    case TYPE_U64:
    case TYPE_I64:
      return "l";
    case TYPE_U32:
    case TYPE_I32:
    case TYPE_U16:
    case TYPE_I16:
    case TYPE_U8:
    case TYPE_I8:
    case TYPE_BOOL:
      return "w";
    case TYPE_STR:
      return ":Str";
    case TYPE_RANGE:
      return ":Range";
    default:
      ASSERT(false && "todo");
      return "?";
  }
}

static const char* type_to_qbe_reg_type(Type type) {
  switch (type_kind(type)) {
    case TYPE_U64:
    case TYPE_I64:
    case TYPE_STR:
    case TYPE_RANGE:
    case TYPE_FUNC:
      return "l";
    case TYPE_U32:
    case TYPE_I32:
    case TYPE_U16:
    case TYPE_I16:
    case TYPE_U8:
    case TYPE_I8:
    case TYPE_BOOL:
      return "w";
    default:
      ASSERT(false && "todo");
      return "?";
  }
}

static const char* type_to_qbe_store_type(Type type) {
  if (is_aggregate_type(type)) {
    // TODO: assuming this path is for the pointer to the aggregate, bad idea?
    return "l";
  }
  switch (type_kind(type)) {
    case TYPE_U64:
    case TYPE_I64:
      return "l";
    case TYPE_U32:
    case TYPE_I32:
      return "w";
    case TYPE_U16:
    case TYPE_I16:
      return "h";
    case TYPE_U8:
    case TYPE_I8:
    case TYPE_BOOL:
      return "b";
    default:
      ASSERT(false && "todo");
      return "?";
  }
}

static const char* type_to_qbe_load_type(Type type) {
  if (is_aggregate_type(type)) {
    // TODO: assuming this path is for the pointer to the aggregate, bad idea?
    return "l";
  }
  switch (type_kind(type)) {
    case TYPE_U64:
    case TYPE_I64:
    case TYPE_FUNC:
      return "l";
    case TYPE_U32:
    case TYPE_I32:
      return "w";
    case TYPE_U16:
      return "uh";
    case TYPE_I16:
      return "sh";
    case TYPE_U8:
    case TYPE_BOOL:
      return "ub";
    case TYPE_I8:
      return "sb";
    default:
      ASSERT(false && "todo");
      return "?";
  }
}

IRRef gen_ssa_start_function(Str name, Type return_type, int num_params, IRRef* params) {
  const char* export = "";
  if (name.i == main_func_name.i) {
    export = "export ";
  }
  fprintf(outf, "%sfunction %s $%s(", export, type_to_qbe_return_type(return_type), cstr(name));
  for (int i = 0; i < num_params; ++i) {
    fprintf(outf, "%s %s%s", type_to_qbe_reg_type(refs[params[i].i].type), irref_as_str(params[i]),
            i < num_params - 1 ? ", " : "");
  }
  fprintf(outf, ") {\n");
  fprintf(outf, "@start\n");

  return gen_ssa_make_func(name, (Type){0} /* XXX TODO !*/);
}

IRRef gen_ssa_const(uint64_t val, Type type) {
  IRRef ret = gen_ssa_make_temp(type);
  fprintf(outf, "  %s =%s copy %lld\n", irref_as_str(ret), type_to_qbe_reg_type(type), val);
  return ret;
}

IRRef gen_ssa_string_constant(Str str) {
  return gen_ssa_make_str_const(str);
}

void gen_ssa_return(IRRef val, Type type) {
  if (type_eq(type, type_void)) {
    fprintf(outf, "  ret\n");
  } else {
    fprintf(outf, "  ret %s\n", irref_as_str(val));
  }
}

void gen_ssa_alloc_local(size_t size, size_t align, IRRef ref) {
  switch(align) {
    case 1:
    case 2:
    case 4:
      fprintf(outf, "  %s =l alloc4 %zu\n", irref_as_str(ref), size);
      break;
    case 8:
      fprintf(outf, "  %s =l alloc8 %zu\n", irref_as_str(ref), size);
      break;
    case 16:
      fprintf(outf, "  %s =l alloc16 %zu\n", irref_as_str(ref), size);
      break;
    default:
      ASSERT(false && "unhandled alignment");
  }
}

void gen_ssa_store(IRRef into, Type type, IRRef val) {
  fprintf(outf, "  store%s %s, %s\n", type_to_qbe_store_type(type), irref_as_str(val),
          irref_as_str(into));
}

void gen_ssa_store_field(IRRef baseptr, uint32_t offset, Type type, IRRef val) {
  if (offset == 0) {
    gen_ssa_store(baseptr, type, val);
  } else {
    IRRef into_ptr = gen_ssa_make_temp(type_u64);
    fprintf(outf, "  %s =l add %s, %d\n", irref_as_str(into_ptr), irref_as_str(baseptr), offset);
    gen_ssa_store(into_ptr, type, val);
  }
}

IRRef gen_ssa_load(IRRef from, Type type) {
  IRRef ret = gen_ssa_make_temp(type);
  fprintf(outf, "  %s =%s load%s %s\n", irref_as_str(ret), type_to_qbe_reg_type(type),
          type_to_qbe_load_type(type), irref_as_str(from));
  return ret;
}

IRRef gen_ssa_load_field(IRRef baseptr, uint32_t offset, Type type) {
  if (offset == 0) {
    return gen_ssa_load(baseptr, type);
  } else {
    IRRef outof_ptr = gen_ssa_make_temp(type_u64);
    fprintf(outf, "  %s =l add %s, %d\n", irref_as_str(outof_ptr), irref_as_str(baseptr), offset);
    return gen_ssa_load(outof_ptr, type);
  }
}

void gen_ssa_jump(IRBlock block) {
  fprintf(outf, "  jmp %s\n", irblock_as_str(block));
}

void gen_ssa_jump_cond(IRRef cond, IRBlock iftrue, IRBlock iffalse) {
  fprintf(outf, "  jnz %s, %s, %s\n", irref_as_str(cond), irblock_as_str(iftrue),
          irblock_as_str(iffalse));
}

IRRef gen_ssa_add(IRRef a, IRRef b) {
  IRRef ret = gen_ssa_make_temp(refs[a.i].type);
  fprintf(outf, "  %s =%s add %s, %s\n", irref_as_str(ret), type_to_qbe_reg_type(refs[a.i].type),
          irref_as_str(a), irref_as_str(b));
  return ret;
}

IRRef gen_ssa_neg(IRRef v) {
  Type type = refs[v.i].type;  // TODO: type match
  IRRef ret = gen_ssa_make_temp(type);
  fprintf(outf, "  %s =%s neg %s\n", irref_as_str(ret), type_to_qbe_reg_type(type),
          irref_as_str(v));
  return ret;
}

IRRef gen_ssa_mul(IRRef a, IRRef b) {
  IRRef ret = gen_ssa_make_temp(refs[a.i].type);  // TODO: type match
  fprintf(outf, "  %s =w mul %s, %s\n", irref_as_str(ret), irref_as_str(a), irref_as_str(b));
  return ret;
}

static const char* cmp_names[NUM_IR_INT_CMPS] = {
  [IIC_EQ] = "eq",
  [IIC_NE] = "ne",
  [IIC_SLE] = "sle",
  [IIC_SLT] = "slt",
  [IIC_SGE] = "sge",
  [IIC_SGT] = "sgt",
  [IIC_ULE] = "ule",
  [IIC_ULT] = "ult",
  [IIC_UGE] = "uge",
  [IIC_UGT] = "ugt",
};

IRRef gen_ssa_int_comparison(IRIntCmp cmp, IRRef a, IRRef b) {
  IRRef ret = gen_ssa_make_temp(type_bool);
  Type type = refs[a.i].type;  // TODO type match
  fprintf(outf, "  %s =w c%s%s %s, %s\n", irref_as_str(ret), cmp_names[cmp],
          type_to_qbe_reg_type(type), irref_as_str(a), irref_as_str(b));
  return ret;
}

void gen_ssa_print_i32(IRRef i) {
  fprintf(outf, "  call $printf(l $intfmt, ..., w %s)\n", irref_as_str(i));
}

void gen_ssa_print_str(IRRef s) {
  IRRef bytes = gen_ssa_make_temp(type_u64);
  fprintf(outf, "  %s =l loadl %s\n", irref_as_str(bytes), irref_as_str(s));

  IRRef size_ptr = gen_ssa_make_temp(type_u64);
  fprintf(outf, "  %s =l add %s, 8\n", irref_as_str(size_ptr), irref_as_str(s));
  IRRef size = gen_ssa_make_temp(type_u64);
  fprintf(outf, "  %s =l loadl %s\n", irref_as_str(size), irref_as_str(size_ptr));

  fprintf(outf, "  call $printf(l $strfmt, ..., w %s, l %s)\n", irref_as_str(size),
          irref_as_str(bytes));
}

// TODO: make this a function instead of inlining
void gen_ssa_print_range(IRRef s) {
  IRRef first = gen_ssa_make_temp(type_u64);
  fprintf(outf, "  %s =l loadl %s\n", irref_as_str(first), irref_as_str(s));

  IRRef second_ptr = gen_ssa_make_temp(type_u64);
  fprintf(outf, "  %s =l add %s, 8\n", irref_as_str(second_ptr), irref_as_str(s));
  IRRef second = gen_ssa_make_temp(type_u64);
  fprintf(outf, "  %s =l loadl %s\n", irref_as_str(second), irref_as_str(second_ptr));

  IRRef third_ptr = gen_ssa_make_temp(type_u64);
  fprintf(outf, "  %s =l add %s, 16\n", irref_as_str(third_ptr), irref_as_str(s));
  IRRef third = gen_ssa_make_temp(type_u64);
  fprintf(outf, "  %s =l loadl %s\n", irref_as_str(third), irref_as_str(third_ptr));

  IRBlock range2 = gen_ssa_make_block_name();
  IRBlock range3 = gen_ssa_make_block_name();
  IRBlock after = gen_ssa_make_block_name();
  IRRef non_one = gen_ssa_int_comparison(IIC_NE, third, gen_ssa_const(1, type_i64));
  gen_ssa_jump_cond(non_one, range3, range2);

  gen_ssa_start_block(range2);
  fprintf(outf, "  call $printf(l $range2fmt, ..., l %s, l %s)\n", irref_as_str(first),
          irref_as_str(second));
  gen_ssa_jump(after);

  gen_ssa_start_block(range3);
  fprintf(outf, "  call $printf(l $range3fmt, ..., l %s, l %s, l %s)\n", irref_as_str(first),
          irref_as_str(second), irref_as_str(third));
  gen_ssa_jump(after);

  gen_ssa_start_block(after);
}

IRRef gen_ssa_call(Type return_type,
                   IRRef func,
                   int num_args,
                   Type* arg_types,
                   IRRef* arg_values) {
  IRRef ret = gen_ssa_make_temp(return_type);
  fprintf(outf, "  %s =%s call %s(", irref_as_str(ret), type_to_qbe_return_type(return_type),
          irref_as_str(func));
  for (int i = 0; i < num_args; ++i) {
    fprintf(outf, "w %s%s", irref_as_str(arg_values[i]), i < num_args - 1 ? ", " : "");
  }
  fprintf(outf, ")\n");
  return ret;
}

void gen_ssa_end_function(void) {
  fprintf(outf, "}\n\n");
}

void gen_ssa_finish(void) {
  for (int i = 0; i < num_refs; ++i) {
    IRRefData* data = &refs[i];
    if (data->kind == REF_STR_CONST) {
      const char* strname = irref_as_str((IRRef){i});
      fprintf(outf, "data %s_bytes = { b \"", strname);
      for (const char* p = cstr(data->str); p != cstr(data->str) + str_len(data->str); ++p) {
        if (*p >= 0x20) {
          fprintf(outf, "%c", *p);
        } else {
          fprintf(outf, "\\x%02x", *p);
        }
      }
      fprintf(outf, "\", z 1 }\n");
      fprintf(outf, "data %s = { l %s_bytes, l %u }\n", strname, strname, str_len(data->str));
    }
  }
  fclose(outf);
}
