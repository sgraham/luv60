#include "luv60.h"
//#include "dict.h"

#if 0
#define VEC_NAME ModuleVec
#define VEC_T Module
#define VEC_PREFIX modv_
#define VEC_NUM_SHORT 16
#include "vec_impl.h"
#endif

typedef enum ModuleState {
  MS_ERROR,
  MS_PENDING,
  MS_LOADED,
} ModuleState;

typedef struct ModuleData {
  ModuleState state;
  Str path;
  //ModuleVec imports;
  //DictImpl syms;
} ModuleData;

#define MAX_NUM_MODULES 1024
static Arena* arena_;
static Arena* parse_temp_arena_;
static ModuleData* modules_;
static size_t num_modules_;
static const char* source_dir_;
static const char* output_dir_;
static int verbose_;
static bool syntax_only_;

void module_init(Arena* arena,
                 Arena* parse_temp_arena,
                 const char* source_dir,
                 const char* output_dir,
                 int verbose,
                 bool syntax_only) {
  arena_ = arena;
  parse_temp_arena_ = parse_temp_arena;
  num_modules_ = 0;
  size_t bytes = sizeof(ModuleData) * MAX_NUM_MODULES;
  modules_ = arena_push(arena_, bytes, _Alignof(ModuleData));
  memset(modules_, 0, bytes);
  source_dir_ = source_dir;
  output_dir_ = output_dir;
  verbose_ = verbose;
  syntax_only_ = syntax_only;

  if (syntax_only_) {
    parse_one_time_initialization_syntax_check(arena_);
  } else {
    parse_one_time_initialization_code_gen(arena_);
  }
}

static Module alloc_module(const char* full_path, ModuleState module_state) {
  uint32_t index = num_modules_++;
  ModuleData* md = &modules_[index];
  CHECK(index < MAX_NUM_MODULES);
  md->path = str_intern_len(full_path, strlen(full_path));
  md->state = module_state;
  return (Module){index};
}

bool module_is_in_error(Module module) {
  ASSERT(module.u < num_modules_);
  ModuleData* md = &modules_[module.u];
  return md->state == MS_ERROR;
}

Str module_path(Module module) {
  ASSERT(module.u < num_modules_);
  ModuleData* md = &modules_[module.u];
  return md->path;
}

static FILE* open_output_callback(const char* input) {
  // This is assumed based on module_add().
  size_t source_dir_len = strlen(source_dir_);
  CHECK(strncmp(input, source_dir_, source_dir_len) == 0);
  CHECK(input[source_dir_len] == '/');
  const char* rest = &input[source_dir_len + 1];
  size_t rest_len = strlen(rest);
  CHECK(rest_len > 4);
  CHECK(strcmp(&rest[rest_len - 4], ".luv") == 0);
  rest_len -= 4;
  // slash, .s, and nul
  size_t output_full_path_len = strlen(output_dir_) + 1 + rest_len + 2 + 1;
  char* output_full_path = arena_push(arena_, output_full_path_len, 1);
  sprintf(output_full_path, "%s/%.*s.s", output_dir_, (int)rest_len, rest);
  path_without_slashes_in_place(&output_full_path[strlen(output_dir_) + 1]);
  FILE* f = fopen(output_full_path, "wb");
  if (!f) {
    base_writef_stderr("Couldn't open '%s' for output.\n", output_full_path);
    base_exit(1);
  }
  return f;
}

// depth-first is right, and there's no cycles
// entry adds main module, walk the import tree depth first
// import statements cause module import
// when done, they create the global package sym in the importer
// each module is .luv to single .s

Module module_add(StrView basename) {
  // 1 for slash, 5 for ".luv\0"
  size_t len_full_path = strlen(source_dir_) + 1 + basename.size + 5;
  char* full_path = arena_push(arena_, len_full_path, 1);
  sprintf(full_path, "%s/%.*s.luv", source_dir_, (int)basename.size, basename.data);
  ReadFileResult file = base_read_file(full_path);
  if (!file.buffer) {
    return alloc_module(full_path, MS_ERROR);
  }

  if (syntax_only_) {
    parse_syntax_check(parse_temp_arena_, full_path, file, verbose_);
  } else {
    parse_code_gen(parse_temp_arena_, full_path, file, verbose_, open_output_callback);
  }

  return alloc_module(full_path, MS_PENDING);
}
