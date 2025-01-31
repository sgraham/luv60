#include "luv60.h"

// Cannot use Str as it's not initialized yet.
static void parse_commandline(int argc,
                              char** argv,
                              char** input,
                              int* verbose,
                              bool* syntax_only,
                              bool* ir_only,
                              bool* return_main_rc,
                              bool* register_test_helpers,
                              int* opt_level) {
  int i = 1;
  *verbose = 0;
  *return_main_rc = false;
  *syntax_only = false;
  *ir_only = false;
  *input = NULL;
  *register_test_helpers = false;
  *opt_level = 1;
  while (i < argc) {
    if (strcmp(argv[i], "-v") == 0) {
      *verbose = 1;
      ++i;
    } else if (strcmp(argv[i], "-vv") == 0) {
      *verbose = 2;
      ++i;
    } else if (strcmp(argv[i], "--main-rc") == 0) {
      *return_main_rc = true;
      ++i;
    } else if (strcmp(argv[i], "--syntax-only") == 0) {
      *syntax_only = true;
      ++i;
    } else if (strcmp(argv[i], "--ir-only") == 0) {
      *ir_only = true;
      ++i;
    } else if (strcmp(argv[i], "--internal-register-test-helpers") == 0) {
      *register_test_helpers = true;
      ++i;
    } else if (strcmp(argv[i], "--opt") == 0) {
      *opt_level = atoi(argv[i+1]);
      if (*opt_level < 0 || *opt_level > 2) {
        base_writef_stderr("Valid optimization levels are 0, 1, 2.\n");
        base_exit(1);
      }
      i += 2;
    } else {
      if (*input) {
        base_writef_stderr("Can only specify a single input file.\n");
        base_exit(1);
      }
      *input = argv[i];
      ++i;
    }
  }

  if (*ir_only && *syntax_only) {
    base_writef_stderr("--ir-only and --syntax-only don't make sense together.\n");
    base_exit(1);
  }

  if (!*input) {
    base_writef_stderr("No input file specified.\n");
    base_exit(1);
  }
}

struct Stuff4 {
  int x, y, z, w;
};

static struct Stuff4 testhelper_returns_stuff4(void) {
  return (struct Stuff4){99, 97, 87, 66};
}

static void testhelper_takes_stuff4(struct Stuff4 s) {
  printf("s.x: %d\n", s.x);
  printf("s.y: %d\n", s.y);
  printf("s.z: %d\n", s.z);
  printf("s.w: %d\n", s.w);
}

static struct Stuff4 testhelper_takes_and_returns_stuff4(struct Stuff4 a, struct Stuff4 b) {
  printf("a.x: %d\n", a.x);
  printf("a.y: %d\n", a.y);
  printf("a.z: %d\n", a.z);
  printf("a.w: %d\n", a.w);
  printf("b.x: %d\n", b.x);
  printf("b.y: %d\n", b.y);
  printf("b.z: %d\n", b.z);
  printf("b.w: %d\n", b.w);
  return (struct Stuff4){2, 3, 5, 8};
}

static void* get_testhelper_addresses(StrView name) {
#define EXPORT_FUNC(x)                          \
  if (strncmp(name.data, #x, name.size) == 0) { \
    return x;                                   \
  }
  EXPORT_FUNC(testhelper_returns_stuff4);
  EXPORT_FUNC(testhelper_takes_stuff4);
  EXPORT_FUNC(testhelper_takes_and_returns_stuff4);
  return NULL;
}

int main(int argc, char** argv) {
  Arena* main_arena = arena_create(MiB(256), KiB(128));
  Arena* parse_temp_arena = arena_create(MiB(256), KiB(128));
  Arena* str_arena = arena_create(MiB(256), KiB(128));
  arena_ir = arena_create(MiB(256), KiB(128));

  char* input;
  int verbose;
  bool syntax_only;
  bool ir_only;
  bool return_main_rc;
  bool register_test_helpers;
  int opt_level;
  parse_commandline(argc, argv, &input, &verbose, &syntax_only, &ir_only, &return_main_rc,
                    &register_test_helpers, &opt_level);

  ReadFileResult file = base_read_file(input);
  if (!file.buffer) {
    base_writef_stderr("Couldn't read '%s'\n", input);
    return 1;
  }

  str_intern_pool_init(str_arena, (char*)file.buffer, file.file_size);

  if (syntax_only) {
    parse_syntax_check(main_arena, parse_temp_arena, input, file, NULL, verbose, ir_only,
                       opt_level);
    return 0;
  } else {
    void* entry = parse_code_gen(main_arena, parse_temp_arena, input, file,
                                 register_test_helpers ? get_testhelper_addresses : NULL, verbose,
                                 ir_only, opt_level);
    int rc = 0;
    if (entry) {
      int entry_returned = ((int (*)())entry)();
      if (verbose) {
        printf("main() returned %d\n", entry_returned);
      }
      if (return_main_rc) {
        rc = entry_returned;
      }
    }

    return rc;
  }
}
