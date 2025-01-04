#include "luv60.h"

#define GEN_MAX_STACK_SIZE 256
typedef struct VSVal {
  Type type;
} VSVal;
static VSVal gen_vstack[GEN_MAX_STACK_SIZE];
static int gen_num_vstack = 0;

static unsigned char* cur_function_base;

#if BUILD_DEBUG
#define GEN_LOG(x) base_writef_stderr("GEN: " #x "\n")
#else
#define GEN_LOG(x)
#endif

#define GEN_CODE_SEG_SIZE (64<<20)
static unsigned char* gen_code;
static unsigned char* gen_p;

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
// patch_cont_fixup() loads offset_of_list_head, gets the offset
// saved there, writes the real REL32 fixup to the target, and repeats
// until it loads a zero in the REL32, which means it walked to the end
// of the list.
#if BUILD_DEBUG
static ContFixup make_cont_fixup(unsigned char* function_base, const char* debug_name) {
  return (ContFixup){function_base, 0, debug_name};
}
#else
static ContFixup make_cont_fixup(unsigned char* function_base) {
  return (ContFixup){function_base, 0};
}
#endif
        
static void patch_cont_fixup(ContFixup* fixup, unsigned char* target) {
#if BUILD_DEBUG
  base_writef_stderr("PATCH: patching %s to 0x%X\n", fixup->debug_name, target - fixup->func_base);
#endif
  int32_t cur = fixup->offset_of_list_head;
  while (cur) {
#if BUILD_DEBUG
    base_writef_stderr("PATCH:  fixup at 0x%X\n", cur);
#endif
    unsigned char* addr = fixup->func_base + cur;
    int32_t next = *(int32_t*)addr;
    int32_t rel = target - addr - 4;
    /*
    if (rel == 0 && target == gen_p && gen_p[-5] == 0xe9) {
      // If the next thing to write is where we're targetting, and the location
      // that we're patching is a jump that would have a zero offset (i.e.
      // fallthrough), then adjust our current write position and target
      // backwards for subsequent fixups and generation to avoid the unnecessary
      // fallthrough jmp.
#if BUILD_DEBUG
      base_writef_stderr("PATCH:  fallthrough, rewinding gen_p (0x%x) by 5\n",
                         gen_p - fixup->func_base);
#endif
      gen_p -= 5;
      target = gen_p;
#if BUILD_DEBUG
      base_writef_stderr("PATCH:  -> now 0x%x\n", gen_p - fixup->func_base);
#endif
    } else*/ {
      *(int32_t*)addr = rel;
    }
    cur = next;
  }
  memset(fixup, 0, sizeof(*fixup));
}

static void vstack_dump(void) {
  base_writef_stderr("[");
  for (int i = gen_num_vstack - 1; i >= 0; --i) {
    base_writef_stderr("%s", typekind_names[gen_vstack[i].type.i]);
    if (i > 0) base_writef_stderr(" ");
  }
  base_writef_stderr("]\n");
}

static void vstack_push(Type t) {
  // XXX only works for basic types that have Type == typeid
  base_writef_stderr("push %s, now ", typekind_names[t.i]);
  gen_vstack[gen_num_vstack++] = (VSVal){t};
  vstack_dump();
}

static VSVal vstack_pop(void) {
  VSVal ret = gen_vstack[--gen_num_vstack];
  // XXX only works for basic types that have Type == typeid
  base_writef_stderr("pop %s, now ", typekind_names[ret.type.i]);
  vstack_dump();
  return ret;
}

void gen_init(void) {
  GEN_LOG("init");
  gen_code = base_large_alloc_rwx(GEN_CODE_SEG_SIZE);
  gen_p = gen_code;
}

#include "snippets.c"

void gen_finish_and_dump(void) {
  GEN_LOG("finish_and_dump");
  FILE* f = fopen("code.raw", "wb");
  fwrite(gen_code, 1, gen_p - gen_code, f);
  fclose(f);
  system("ndisasm -b64 code.raw");
}

int gen_finish_and_run(void) {
  GEN_LOG("finish_and_run");
  base_set_protection_rx(gen_code, GEN_CODE_SEG_SIZE);
  __asm__("movq %rsp, %r13; subq $8, %r13");
  return ((int (*)())gen_code)();
}

void gen_func_entry(void) {
  GEN_LOG("func_entry");
  cur_function_base = gen_p;
  snip_func_entry(gen_num_vstack, &gen_p, /*CONT0=*/NULL);
}

void gen_func_return(ContFixup* result, Type return_type) {
  GEN_LOG("func_return");
  gen_resolve_label(result);
  if (return_type.i == TYPE_VOID) {
    snip_return_void(gen_num_vstack, &gen_p);
  } else {
    ASSERT(gen_num_vstack);
    VSVal top = vstack_pop();
    // convert_or_error(top, return_type);
    (void)top;
    snip_return(gen_num_vstack, &gen_p);
  }
}

ContFixup gen_make_label_impl(
#if BUILD_DEBUG
    const char* debug_name
#else
    void
#endif
    ) {
  ASSERT(cur_function_base);
#if BUILD_DEBUG
  return make_cont_fixup(cur_function_base, debug_name);
#else
  return make_cont_fixup(cur_function_base);
#endif
}

void gen_resolve_label(ContFixup* fixup) {
  patch_cont_fixup(fixup, gen_p);
}

void gen_if(ContFixup* cond, ContFixup* then, ContFixup* els) {
  GEN_LOG("if");
  gen_resolve_label(cond);
  vstack_pop();
  // TODO: type!
  snip_if_then_else(gen_num_vstack, &gen_p, then, els);
}

void gen_jump(ContFixup* to) {
  GEN_LOG("jump");
  snip_jump(gen_num_vstack, &gen_p, to);
}

void gen_push_number(uint64_t val, Type suffix, ContFixup* cont) {
  GEN_LOG("push_number");
  snip_const_i32(gen_num_vstack, &gen_p, val, cont);
  vstack_push(suffix.i == TYPE_NONE ? (Type){TYPE_I64} : suffix);
}

void gen_add(ContFixup* cont) {
  GEN_LOG("add");
  VSVal a = vstack_pop();
  VSVal b = vstack_pop();
  (void)a;
  (void)b;
  // TODO: types!
  snip_add_i32(gen_num_vstack, &gen_p, cont);
  vstack_push(a.type);
}

void gen_store_local(uint32_t offset, Type type) {
  GEN_LOG("store_local");
  // TODO: type!
  VSVal top = vstack_pop();
  // convert_or_error(top, type);
  (void)top;
  snip_store_local_i32(gen_num_vstack, &gen_p, offset, /*CONT0=*/NULL);
}

void gen_load_local(uint32_t offset, Type type, ContFixup* fixup) {
  GEN_LOG("load_local");
  snip_load_local_i32(gen_num_vstack, &gen_p, offset, /*CONT0=*/fixup);
  vstack_push(type);
}
