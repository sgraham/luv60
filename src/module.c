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
  Str load_path;
  Str output_path;
  ReadFileResult file;
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
  syntax_only_ = syntax_only;

  if (syntax_only_) {
    parse_one_time_initialization_syntax_check(arena_, verbose);
  } else {
    parse_one_time_initialization_code_gen(arena_, verbose);
  }
}

static Module alloc_module(const char* full_path,
                           const char* output_path,
                           ReadFileResult file,
                           ModuleState module_state) {
  uint32_t index = num_modules_++;
  ModuleData* md = &modules_[index];
  CHECK(index < MAX_NUM_MODULES);
  md->load_path = str_intern_len(full_path, strlen(full_path));
  md->output_path = output_path ? str_intern_len(output_path, strlen(output_path)) : (Str){0};
  md->file = file;
  md->state = module_state;
  return (Module){index};
}

static ModuleData* get_module_data(Module module) {
  ASSERT(module.u < num_modules_);
  return &modules_[module.u];
}

bool module_is_in_error(Module module) {
  return get_module_data(module)->state == MS_ERROR;
}

Str module_load_path(Module module) {
  return get_module_data(module)->load_path;
}

Str module_output_path(Module module) {
  return get_module_data(module)->output_path;
}

ReadFileResult module_read_file_result(Module module) {
  return get_module_data(module)->file;
}

size_t module_num_modules(void) {
  return num_modules_;
}

Module module_get_module_by_index(size_t i) {
  ASSERT(i < num_modules_);
  return (Module){i};
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
    return alloc_module(full_path, NULL, file, MS_ERROR);
  }

  // 1 for slash, 3 for ".s\0"
  size_t output_full_path_len = strlen(output_dir_) + 1 + basename.size + 2 + 1;
  char* output_path = arena_push(arena_, output_full_path_len, 1);
  sprintf(output_path, "%s/%.*s.s", output_dir_, (int)basename.size, basename.data);

  Module module = alloc_module(full_path, output_path, file, MS_PENDING);

  if (syntax_only_) {
    parse_syntax_check(parse_temp_arena_, module);
  } else {
    parse_code_gen(parse_temp_arena_, module);
  }

  get_module_data(module)->state = MS_LOADED;

  return module;
}
