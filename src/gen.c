#define GEN_MAX_STACK_SIZE 256
typedef struct VSVal {
  Type type;
  int reg;
} VSVal;
static VSVal gen_vstack[GEN_MAX_STACK_SIZE];
static int gen_num_vstack = 0;

//#include "snippets.c"

void gen_push_number(uint64_t val, TypeKind suffix) {
  snip_load_const(gen_num_vstack, val);
}

// type can be TYPE_VOID for no return.
void gen_return(Type type) {
  if (type.i == TYPE_VOID) {
    //snip_return_void[gen_num_vstack]();
  } else {
    ASSERT(gen_num_vstack);
    VSVal top = gen_vstack[--gen_num_vstack];
    // convert_or_error(top, type);
    snip_return(gen_num_vstack);
  }
}
