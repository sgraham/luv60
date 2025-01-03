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


typedef struct FuncPrologFixup {
  unsigned char* addr;
} FuncPrologFixup;

typedef struct GenLabel {
  int i;
} GenLabel;

void gen_init(void);
void gen_finish(void);
FuncPrologFixup gen_func_entry(void);
void gen_func_exit_and_patch_func_entry(uint32_t locals_space_required, FuncPrologFixup fixup_loc);
void gen_push_number(uint64_t val, Type suffix);
void gen_store_local(uint32_t offset, Type type);
void gen_return(Type type);
GenLabel gen_declare_label(void);
void gen_jump(GenLabel label);
void gen_fixup_label_to_here(GenLabel label);

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
