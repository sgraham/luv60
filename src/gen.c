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
// When the ContFixup is passed into a snip_XYZ_fixup function, the four
// bytes of the REL32 for the CONT stores the current head of the list,
// and the location where we just wrote becomes the head.
//
// So, for the first to-be-fixed, a 0 is written into the REL32 location,
// and the address/offset of the place we wrote is saved into the
// ContFixup->offset_of_list_head (e.g. let's say the zero was written at
// offset 22, so 22 is saved into offset_of_list_head).
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
      // If the next thing to write is where we're targetting, and the location
      // that we're patching is a jump that would have a zero offset (i.e.
      // fallthrough), then adjust our current write position and target
      // backwards for subsequent fixups and generation to avoid the unnecessary
      // fallthrough jmp.
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

void gen_finish_and_dump(void) {
  FILE* f = fopen("code.raw", "wb");
  fwrite(gen_code, 1, gen_p - gen_code, f);
  fclose(f);
  system("ndisasm -b64 code.raw");
}

int gen_finish_and_run(void) {
  base_set_protection_rx(gen_code, GEN_CODE_SEG_SIZE);
  __asm__("movq %rsp, %r13; subq $8, %r13");
  return ((int (*)())gen_code)();
}

ContFixup gen_func_entry(void) {
  ContFixup ret = snip_make_cont_fixup(gen_p);
  snip_func_entry(gen_num_vstack, &gen_p, /*CONT0=*/NULL);
  return ret;
}

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

void gen_push_number(uint64_t val, Type suffix, ContFixup* cont) {
  snip_const_i32(gen_num_vstack, &gen_p, val, cont);
  gen_vstack[gen_num_vstack++] = (VSVal){suffix.i == TYPE_NONE ? (Type){TYPE_I64} : suffix};
}

void gen_add(ContFixup* cont) {
  VSVal a = gen_vstack[--gen_num_vstack];
  VSVal b = gen_vstack[--gen_num_vstack];
  (void)a;
  (void)b;
  // TODO: types!
  snip_add_i32(gen_num_vstack, &gen_p, cont);
  gen_vstack[gen_num_vstack++] = (VSVal){a.type};
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
