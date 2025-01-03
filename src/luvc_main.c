#include "base.c"

typedef struct StrView {
  const char* data;
  uint32_t size;
} StrView;

extern void lex_start(const uint8_t* buf, size_t byte_count_rounded_up);
extern void lex_next_block(uint8_t token_kinds[128], uint32_t token_offsets[128]);
extern StrView lex_get_strview(uint32_t from, uint32_t to);

typedef struct Type {
  uint32_t i;
} Type;

#define TYPEKINDS_X \
  X(NONE)           \
  X(VOID)           \
  X(BOOL)           \
  X(U8)             \
  X(I8)             \
  X(U16)            \
  X(I16)            \
  X(U32)            \
  X(I32)            \
  X(U64)            \
  X(I64)            \
  X(ENUM)           \
  X(FLOAT)          \
  X(DOUBLE)         \
  X(STR)            \
  X(RANGE)          \
  X(SLICE)          \
  X(ARRAY)          \
  X(PTR)            \
  X(FUNC)           \
  X(DICT)           \
  X(STRUCT)         \
  X(UNION)          \
  X(CONST)          \
  X(PACKAGE)        \
  X(CHAR) /* only for interop foreign calls */

typedef enum TypeKind {
#define X(x) TYPE_##x,
  TYPEKINDS_X
#undef X
      NUM_TYPE_KINDS,
} TypeKind;

extern void parse(const char* filename, ReadFileResult file);

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

typedef struct ContFixup {
    unsigned char* func_base;
    int32_t offset_of_list_head;
} ContFixup;


typedef struct FuncPrologFixup {
  unsigned char* addr;
} FuncPrologFixup;

typedef struct GenLabel {
  int i;
} GenLabel;

void gen_init(void);
void gen_finish(void);
ContFixup gen_func_entry(void);
void gen_func_exit_and_patch_func_entry(ContFixup* fixup, Type return_type);
void gen_push_number(uint64_t val, Type suffix, ContFixup* cont);
void gen_store_local(uint32_t offset, Type type);
void gen_load_local(uint32_t offset, Type type);
void gen_error(const char* message);

static inline ContFixup snip_make_cont_fixup(unsigned char* function_base) {
    return (ContFixup){function_base, 0};
}
        
static inline void snip_patch_cont_fixup(ContFixup* fixup, unsigned char* target) {
  int32_t cur = fixup->offset_of_list_head;
  while (cur) {
    unsigned char* addr = fixup->func_base + cur;
    int32_t next = *(int32_t*)addr;
    *(int32_t*)addr = target - addr - 4;
    cur = next;
  }
  memset(fixup, 0, sizeof(*fixup));
}

#include "lex.c"
#include "parse.c"
#include "gen.c"

int main() {
  const char* filename = "basic.luv";
  ReadFileResult file = base_read_file(filename);
  if (!file.buffer) {
    base_writef_stderr("Couldn't read '%s'\n", filename);
    return 1;
  }

  parse(filename, file);
  return 0;
}
