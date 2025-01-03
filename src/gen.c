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

void gen_init(void) {
  gen_code =
      VirtualAlloc(NULL, GEN_CODE_SEG_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
  gen_p = gen_code;
}

#include "snippets.c"

void gen_finish(void) {
#if 1
  FILE* f = fopen("code.raw", "wb");
  fwrite(gen_code, 1, gen_p - gen_code, f);
  fclose(f);
  system("ndisasm -b64 code.raw");
#else
  DWORD old_protect;
  VirtualProtect(gen_code, GEN_CODE_SEG_SIZE, PAGE_EXECUTE_READ, &old_protect);
  asm("movq %rsp, %r13; subq $8, %r13");
  int rv = ((int (*)())gen_code)();
  printf("returned: %d\n", rv);
#endif
}

// In the maximal case, we need this big mess to start a function (when the
// amount of stack space required exceeds a page, and when the rip-relative
// location of __chkstk is a far offset away (>2G). So that we're not always
// paying for this, but still allowing for simple fixups, we insert a nop sled
// first that will leave enough size for this version if the function ends up
// needing a large amount of local stack space. But otherwise, we can just leave
// it and fix the minimal version. We could also special case a "medium" version
// if necessary.
//
// Maximal:
//  0000000000000000: B8 38 40 00 00     mov         eax,4038h
//  0000000000000005: 49 BB 00 00 00 00  mov         r11,offset __chkstk
//                    00 00 00 00
//  000000000000000F: 41 FF D3           call        r11
//  0000000000000012: 48 29 C4           sub         rsp,rax
//
// Minimal (same number of bytes with long nops):
//   0000000000000000: 
//   0000000000000000: 48 83 EC 38        sub         rsp,38h
//
//static uint8_t gen_nop8[] = { 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 };
//static uint8_t gen_nop9[] = { 0x66, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 };

ContFixup gen_func_entry(void) {
  ContFixup ret = snip_make_cont_fixup(gen_p);
  snip_func_entry_fallthrough(gen_num_vstack, &gen_p);
  return ret;
}

// Big version:
// 000000000000005D: 48 81 C4 38 40 00  add         rsp,4038h
//                   00
//
// Small version:
// 0000000000000036: 48 83 C4 38        add         rsp,38h
void gen_func_exit_and_patch_func_entry(ContFixup* fixup, Type return_type) {
  snip_patch_cont_fixup(fixup, gen_p);

  if (return_type.i == TYPE_VOID) {
    snip_return_void_fixup(gen_num_vstack, &gen_p);
  } else {
    ASSERT(gen_num_vstack);
    VSVal top = gen_vstack[--gen_num_vstack];
    // convert_or_error(top, return_type);
    (void)top;
    snip_return_fixup(gen_num_vstack, &gen_p);
  }
}

#if 0
void gen_return(Type return_type, ContFixup* fixup) {
}
#endif

void gen_push_number(uint64_t val, Type suffix, ContFixup* cont) {
  snip_const_i32_fixup(gen_num_vstack, &gen_p, val, cont);
  gen_vstack[gen_num_vstack++] = (VSVal){suffix.i == TYPE_NONE ? (Type){TYPE_I64} : suffix};
}

void gen_store_local(uint32_t offset, Type type) {
  // TODO: type!
  VSVal top = gen_vstack[--gen_num_vstack];
  // convert_or_error(top, type);
  (void)top;
  snip_store_local_i32_fallthrough(gen_num_vstack, &gen_p, offset);
}

void gen_load_local(uint32_t offset, Type type) {
  snip_load_local_i32_fallthrough(gen_num_vstack, &gen_p, offset);
  gen_vstack[gen_num_vstack++] = (VSVal){type};
}
