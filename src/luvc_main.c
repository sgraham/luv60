#include "luv60.h"

#define VEC_NAME ByteVec
#define VEC_T char
#define VEC_PREFIX bv_
#define VEC_NUM_SHORT 1024
#include "vec_impl.h"

#define VEC_NAME CstrVec
#define VEC_T char*
#define VEC_PREFIX cstrv_
#define VEC_NUM_SHORT 16
#include "vec_impl.h"

// Cannot use Str as it's not initialized yet. TODO: probably just initialize
// earlier, not sure why I didn't before.
static void parse_commandline(int argc,
                              char** argv,
                              char** input,
                              char** output_dir,
                              int* verbose,
                              bool* syntax_only,
                              CstrVec* with_c) {
  int i = 1;
  *verbose = 0;
  *syntax_only = false;
  *input = NULL;
  *output_dir = NULL;
  while (i < argc) {
    if (strcmp(argv[i], "-v") == 0) {
      *verbose = 1;
      ++i;
    } else if (strcmp(argv[i], "-vv") == 0) {
      *verbose = 2;
      ++i;
    } else if (strcmp(argv[i], "-vvv") == 0) {
      *verbose = 3;
      ++i;
    } else if (strcmp(argv[i], "--syntax-only") == 0) {
      *syntax_only = true;
      ++i;
    } else if (strcmp(argv[i], "--with-c") == 0) {
      cstrv_append(with_c, argv[i + 1]);
      i += 2;
    } else if (strcmp(argv[i], "-o") == 0) {
      if (*output_dir) {
        base_writef_stderr("Can only specify a single output_dir directory.\n");
        base_exit(1);
      }
      *output_dir = argv[i + 1];
      i += 2;
    } else {
      if (*input) {
        base_writef_stderr("Can only specify a single main input file.\n");
        base_exit(1);
      }
      *input = argv[i];
      ++i;
    }
  }

  if (!*input) {
    base_writef_stderr("No main input file specified.\n");
    base_exit(1);
  }
  if (!*output_dir && !*syntax_only && !*verbose) {
    base_writef_stderr("No output directory specified.\n");
    base_exit(1);
  }
}

static void append_str(ByteVec* bv, Str s) {
  size_t len = str_len(s);
  for (size_t i = 0; i < len; ++i) {
    bv_append(bv, str_raw_ptr(s)[i]);
  }
}

static void append_cstr(ByteVec* bv, const char* s) {
  size_t len = strlen(s);
  for (size_t i = 0; i < len; ++i) {
    bv_append(bv, s[i]);
  }
}

int assemble_and_link(Arena* arena,
                      Module main,
                      const char* output_dir,
                      const char* basename,
                      CstrVec* with_c,
                      int verbose) {
  ByteVec cmd;
  bv_init(&cmd, arena);

#if OS_WINDOWS
  // TODO
  append_cstr(&cmd, "\"C:\\Program Files\\LLVM\\bin\\clang.exe\" ");
#else
  append_cstr(&cmd, "clang ");
#endif

  for (size_t i = 0; i < module_num_modules(); ++i) {
    Module m = module_get_module_by_index(i);
    Str dot_s = module_output_path(m);
    append_str(&cmd, dot_s);
    append_cstr(&cmd, " ");
  }

  for (int i = 0; i < cstrv_size(with_c); ++i) {
    append_cstr(&cmd, cstrv_at(with_c, i));
    append_cstr(&cmd, " ");
  }

  append_cstr(&cmd, "-o ");
  append_cstr(&cmd, output_dir);
  append_cstr(&cmd, "/");
  append_cstr(&cmd, basename);
#if OS_WINDOWS
  append_cstr(&cmd, ".exe");
#endif

  bv_append(&cmd, 0);
  if (verbose) {
    base_writef_stderr("Running:\n  %s\n", bv_dataptr(&cmd));
  }
  system(bv_dataptr(&cmd));

  return 0;
}

int main(int argc, char** argv) {
  Arena* main_arena = arena_create(MiB(256), KiB(128));
  Arena* parse_temp_arena = arena_create(MiB(256), KiB(128));
  Arena* str_arena = arena_create(MiB(256), KiB(128));

  char* input;
  char* output_dir;
  int verbose;
  bool syntax_only;
  CstrVec with_c;
  cstrv_init(&with_c, main_arena);
  cstrv_append(&with_c, "src/rt.c");  // TODO

  parse_commandline(argc, argv, &input, &output_dir, &verbose, &syntax_only, &with_c);

  str_intern_pool_init(str_arena);

  const char* source_dir;
  char* filename;
  path_split(main_arena, input, &source_dir, &filename);
  path_trim_extension_if_exists(filename, ".luv");

  module_init(main_arena, parse_temp_arena, source_dir, output_dir,
              verbose, syntax_only);
  Module main = module_add((StrView){filename, strlen(filename)});
  if (module_is_in_error(main)) {
    Str path = module_load_path(main);
    base_writef_stderr("Couldn't load main module, looking for '%.*s'.\n",
        (int)str_len(path), str_raw_ptr(path));
  }

  if (verbose > 1) {
    base_writef_stderr("Not assembling and linking, no .s generated.\n");
  } else {
    return assemble_and_link(main_arena, main, output_dir, filename, &with_c, verbose);
  }
}
