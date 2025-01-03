#include "luv60.h"

#define GEN_MAX_STACK_SIZE 256
typedef struct VSVal {
  Type type;
} VSVal;
static VSVal gen_vstack[GEN_MAX_STACK_SIZE];
static int gen_num_vstack = 0;

// ContFixups are sort of like a "linked list" of fixups threaded
// through the generated code. The list is started when a caller
// allocates a ContFixup, which is the head of the list starting
// with a null (0) offset.
//
// When the ContFixup is passed into a snip_XXX_fixup function, the four
// bytes of the REL32 for the CONT stores the current head of the list,
// and the location where we just wrote becomes the head.
//
// So, for the first to-be-fixed, a 0 is written into the REL32 location,
// and the address/offset of the place we wrote is saved into the
// ContFixup->offset_of_list_head (e.g. let's say the zero was written at
// offset 22, so 22 is saved into offset_of_list_head.).
//
// Then, for the second to-be-fixed location (at e.g. 61), 22 is
// written to *its* REL32 location, and 61 is saved to offset_of_list_head.
//
// When the actual fixup address is available to be resolved,
// snip_patch_cont_fixup() loads offset_of_list_head, gets the offset
// saved there, writes the real REL32 fixup to the target, and repeats
// until it loads a zero in the REL32, which means it walked to the end
// of the list.

ContFixup snip_make_cont_fixup(unsigned char* function_base) {
    return (ContFixup){function_base, 0};
}
        
void snip_patch_cont_fixup(ContFixup* fixup, unsigned char* target) {
  int32_t cur = fixup->offset_of_list_head;
  while (cur) {
    unsigned char* addr = fixup->func_base + cur;
    int32_t next = *(int32_t*)addr;
    int32_t rel = target - addr - 4;
    if (rel == 0 && target == gen_p && gen_p[-5] == 0xe9) {
      gen_p -= 5;
      target = gen_p;
    } else {
      *(int32_t*)addr = rel;
    }
    cur = next;
  }
  memset(fixup, 0, sizeof(*fixup));
}

#define GEN_CODE_SEG_SIZE (64<<20)
static unsigned char* gen_code;
unsigned char* gen_p;

void gen_init(void) {
  gen_code = base_large_alloc_rwx(GEN_CODE_SEG_SIZE);
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
  base_set_protection_rx(gen_code, GEN_CODE_SEG_SIZE);
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
  snip_func_entry(gen_num_vstack, &gen_p, /*CONT0=*/NULL);
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
    snip_return_void(gen_num_vstack, &gen_p);
  } else {
    ASSERT(gen_num_vstack);
    VSVal top = gen_vstack[--gen_num_vstack];
    // convert_or_error(top, return_type);
    (void)top;
    snip_return(gen_num_vstack, &gen_p);
  }
}

#if 0
void gen_return(Type return_type, ContFixup* fixup) {
}
#endif

void gen_push_number(uint64_t val, Type suffix, ContFixup* cont) {
  snip_const_i32(gen_num_vstack, &gen_p, val, cont);
  gen_vstack[gen_num_vstack++] = (VSVal){suffix.i == TYPE_NONE ? (Type){TYPE_I64} : suffix};
}

void gen_store_local(uint32_t offset, Type type) {
  // TODO: type!
  VSVal top = gen_vstack[--gen_num_vstack];
  // convert_or_error(top, type);
  (void)top;
  snip_store_local_i32(gen_num_vstack, &gen_p, offset, /*CONT0=*/NULL);
}

void gen_load_local(uint32_t offset, Type type, ContFixup* fixup) {
  snip_load_local_i32(gen_num_vstack, &gen_p, offset, /*CONT0=*/fixup);
  gen_vstack[gen_num_vstack++] = (VSVal){type};
}
