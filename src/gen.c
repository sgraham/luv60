#define GEN_MAX_STACK_SIZE 256
typedef struct VSVal {
  Type type;
} VSVal;
static VSVal gen_vstack[GEN_MAX_STACK_SIZE];
static int gen_num_vstack = 0;

#define GEN_CODE_SEG_SIZE (64<<20)
static unsigned char* gen_code;
static unsigned char* gen_p;

static void gen_error(const char* message) {
  base_writef_stderr("%s:<offset %d>: error: %s\n", p_cur_filename, p_cur_offset, message);
  base_exit(1);
}

#include "snippets.c"

void gen_init(void) {
  gen_code =
      VirtualAlloc(NULL, GEN_CODE_SEG_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
  gen_p = gen_code;
}

void gen_finish(void) {
  FILE* f = fopen("code.raw", "wb");
  fwrite(gen_code, 1, gen_p - gen_code, f);
  fclose(f);
  // `ndisasm -b64 code.raw`

  DWORD old_protect;
  VirtualProtect(gen_code, GEN_CODE_SEG_SIZE, PAGE_EXECUTE_READ, &old_protect);
  int rv = ((int (*)())gen_code)();
  printf("returned: %d\n", rv);
}

void gen_push_number(uint64_t val, Type suffix) {
  gen_p = snip_const_i32_fallthrough(gen_num_vstack, gen_p, val);
  gen_vstack[gen_num_vstack++] = (VSVal){suffix.i == TYPE_NONE ? (Type){TYPE_I64} : suffix};
}

// type can be TYPE_VOID for no return.
void gen_return(Type type) {
  if (type.i == TYPE_VOID) {
    // gen_p = snip_return_void(gen_num_vstack, gen_p);
    abort();
  } else {
    ASSERT(gen_num_vstack);
    VSVal top = gen_vstack[--gen_num_vstack];
    // convert_or_error(top, type);
    (void)top;
    snip_return(gen_num_vstack, gen_p);
  }
}
