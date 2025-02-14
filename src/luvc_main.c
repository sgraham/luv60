#include "luv60.h"

// Cannot use Str as it's not initialized yet.
static void parse_commandline(int argc,
                              char** argv,
                              char** input,
                              char** output,
                              int* verbose,
                              bool* syntax_only,
                              bool* ir_only,
                              int* opt_level) {
  int i = 1;
  *verbose = 0;
  *syntax_only = false;
  *ir_only = false;
  *input = NULL;
  *output = NULL;
  *opt_level = 1;
  while (i < argc) {
    if (strcmp(argv[i], "-v") == 0) {
      *verbose = 1;
      ++i;
    } else if (strcmp(argv[i], "-vv") == 0) {
      *verbose = 2;
      ++i;
    } else if (strcmp(argv[i], "--syntax-only") == 0) {
      *syntax_only = true;
      ++i;
    } else if (strcmp(argv[i], "-o") == 0) {
      if (*output) {
        base_writef_stderr("Can only specify a single output file.\n");
        base_exit(1);
      }
      *output = argv[i + 1];
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
  if (!*output) {
    base_writef_stderr("No output file specified.\n");
    base_exit(1);
  }
}

#if 0
struct Stuff4 {
  int x, y, z, w;
};

struct LittleStuff {
  int x;
  float y;
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

static struct LittleStuff testhelper_returns_littlestuff(void) {
  return (struct LittleStuff){123, 3.14f};
}

static void testhelper_takes_littlestuff(struct LittleStuff ls) {
  printf("ls.x: %d\n", ls.x);
  printf("ls.y: %f\n", ls.y);
}

static struct LittleStuff testhelper_takes_and_returns_little_and_big(struct LittleStuff a, struct Stuff4 b, struct LittleStuff c) {
  printf("a.x: %d\n", a.x);
  printf("a.y: %f\n", a.y);
  printf("b.x: %d\n", b.x);
  printf("b.y: %d\n", b.y);
  printf("b.z: %d\n", b.z);
  printf("b.w: %d\n", b.w);
  printf("c.x: %d\n", c.x);
  printf("c.y: %f\n", c.y);
  return (struct LittleStuff){999, 888.0f};
}

static void* get_testhelper_addresses(StrView name) {
#define EXPORT_FUNC(x)                          \
  if (strncmp(name.data, #x, name.size) == 0) { \
    return x;                                   \
  }
  EXPORT_FUNC(testhelper_returns_stuff4);
  EXPORT_FUNC(testhelper_takes_stuff4);
  EXPORT_FUNC(testhelper_takes_and_returns_stuff4);
  EXPORT_FUNC(testhelper_returns_littlestuff);
  EXPORT_FUNC(testhelper_takes_littlestuff);
  EXPORT_FUNC(testhelper_takes_and_returns_little_and_big);
  return NULL;
}
#endif

int main(int argc, char** argv) {
  Arena* main_arena = arena_create(MiB(256), KiB(128));
  Arena* parse_temp_arena = arena_create(MiB(256), KiB(128));
  Arena* str_arena = arena_create(MiB(256), KiB(128));
#if 0
  arena_ir = arena_create(MiB(256), KiB(128));
#endif

  char* input;
  char* output;
  int verbose;
  bool syntax_only;
  bool ir_only;
  int opt_level;
  parse_commandline(argc, argv, &input, &output, &verbose, &syntax_only, &ir_only, &opt_level);

  ReadFileResult file = base_read_file(input);
  if (!file.buffer) {
    base_writef_stderr("Couldn't read '%s'\n", input);
    return 1;
  }

  str_intern_pool_init(str_arena, (char*)file.buffer, file.file_size);

  if (syntax_only) {
    parse_syntax_check(main_arena, parse_temp_arena, input, file, verbose);
  } else {
    FILE* out_file = fopen(output, "wb");
    if (!out_file) {
      base_writef_stderr("Couldn't open '%s' for output.\n", out_file);
      return 1;
    }
    parse_code_gen(main_arena, parse_temp_arena, input, file, verbose, out_file);
  }
  return 0;
}
