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
  asm("movq %rsp, %r13; subq $8, %r13");
  int rv = ((int (*)())gen_code)();
  printf("returned: %d\n", rv);
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
static uint8_t gen_nop8[] = { 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 };
static uint8_t gen_nop9[] = { 0x66, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 };

GenFixup gen_func_entry(void) {
  GenFixup fixup = {gen_p};
  memcpy(gen_p, gen_nop8, 8);
  memcpy(&gen_p[8], gen_nop9, 9);
  gen_p += 17;
  *gen_p++ = 0x48;
  *gen_p++ = 0x83;
  *gen_p++ = 0xec;
  *gen_p++ = 0x00;  // actual location to be patched for minimal fixup
  return fixup;
}

// Big version:
// 000000000000005D: 48 81 C4 38 40 00  add         rsp,4038h
//                   00
//
// Small version:
// 0000000000000036: 48 83 C4 38        add         rsp,38h
void gen_func_exit_and_patch_func_entry(uint32_t locals_space_required, GenFixup fixup_loc) {
  if (locals_space_required <= 255) {
    fixup_loc.addr[20] = (uint8_t)locals_space_required;
    *gen_p++ = 0x48;
    *gen_p++ = 0x83;
    *gen_p++ = 0xc4;
    *gen_p++ = (uint8_t)locals_space_required;
  } else {
    ASSERT(false && "todo");
  }
}

void gen_push_number(uint64_t val, Type suffix) {
  gen_p = snip_const_i32_fallthrough(gen_num_vstack, gen_p, val);
  gen_vstack[gen_num_vstack++] = (VSVal){suffix.i == TYPE_NONE ? (Type){TYPE_I64} : suffix};
}

void gen_store_local(uint32_t offset, Type type) {
  // TODO: type!
  VSVal top = gen_vstack[--gen_num_vstack];
  // convert_or_error(top, type);
  (void)top;
  gen_p = snip_store_local_i32_fallthrough(gen_num_vstack, gen_p, offset);
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
