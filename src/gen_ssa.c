#include "luv60.h"

static FILE* outf;

typedef struct IRFuncData {
  Str name;
} IRFuncData;
static IRFuncData funcs[1<<20];
static uint32_t num_funcs;
static Str main_func_name;
static int tmp_counter;

static IRRef gentmp(void) {
  return (IRRef){tmp_counter++};
}

static const char* irref_as_str(IRRef ref) {
  char buf[128];
  sprintf(buf, "%%t%d", ref.i);
  return _strdup(buf);
}

void gen_ssa_init(const char* filename) {
  num_funcs = 0;
  tmp_counter = 0;
  main_func_name = str_intern_len("main", 4);
  outf = fopen(filename, "wb");

#if 0
  fprintf(outf, "%s",
"function w $add(w %a, w %b) {              # Define a function add\n"
"@start\n"
"  %c =w add %a, %b                   # Adds the 2 arguments\n"
"  ret %c                             # Return the result\n"
"}\n"
"export function w $main() {                # Main function\n"
"@start\n"
"  %r =w call $add(w 1, w 1)          # Call add(1, 1)\n"
"  call $printf(l $fmt, ..., w %r)    # Show the result\n"
"  ret 0\n"
"}\n"
"data $fmt = { b \"One and one make %d!\\n\", b 0 }\n");
#endif

}

static const char* type_to_qbe_name(Type type) {
  return "w";
}

IRFunc gen_ssa_start_function(Str name,
                              Type return_type,
                              int num_params,
                              Type* param_types,
                              Str* param_names) {
  const char* export = "";
  if (name.i == main_func_name.i) {
    export = "export ";
  }
  fprintf(outf, "%sfunction %s $%s(", export, type_to_qbe_name(return_type), cstr(name));
  for (int i = 0; i < num_params; ++i) {
    fprintf(outf, "%s %%%s%s", type_to_qbe_name(param_types[i]), cstr(param_names[i]),
            i < num_params - 1 ? ", " : "");
  }
  fprintf(outf, ") {\n");
  fprintf(outf, "@start\n");

  IRFunc ret = {num_funcs};
  funcs[num_funcs++].name = name;
  return ret;
}

IRRef gen_ssa_const(uint64_t val, Type type) {
  (void)type; // TODO
  IRRef ret = gentmp();
  fprintf(outf, "  %s =w copy %lld\n", irref_as_str(ret), val);
  return ret;
}

void gen_ssa_return(IRRef val, Type type) {
  if (type.i == TYPE_VOID) {
    fprintf(outf, "  ret\n");
  } else {
    fprintf(outf, "  ret %s\n", irref_as_str(val));
  }
}

IRRef gen_ssa_alloc_local(Type type) {
  IRRef ret = gentmp();
  fprintf(outf, "  %s =l alloc4 4\n", irref_as_str(ret));
  return ret;
}

void gen_ssa_store(IRRef into, Type type, IRRef val) {
  fprintf(outf, "  storew %s, %s\n", irref_as_str(val), irref_as_str(into));
}

IRRef gen_ssa_load(IRRef from, Type type) {
  IRRef ret = gentmp();
  fprintf(outf, "  %s =w loadw %s\n", irref_as_str(ret), irref_as_str(from));
  return ret;
}

IRRef gen_ssa_add(IRRef a, IRRef b) {
  IRRef ret = gentmp();
  fprintf(outf, "  %s =w add %s, %s\n", irref_as_str(ret), irref_as_str(a), irref_as_str(b));
  return ret;
}

void gen_ssa_end_function(void) {
  fprintf(outf, "}\n");
}

void gen_ssa_finish(void) {
  fclose(outf);
}
