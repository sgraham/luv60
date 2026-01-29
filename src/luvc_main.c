#include "luv60.h"

// Cannot use Str as it's not initialized yet.
static void parse_commandline(int argc,
                              char** argv,
                              char** input,
                              char** output_dir,
                              int* verbose,
                              bool* syntax_only) {
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
    } else if (strcmp(argv[i], "--syntax-only") == 0) {
      *syntax_only = true;
      ++i;
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

int main(int argc, char** argv) {
  Arena* main_arena = arena_create(MiB(256), KiB(128));
  Arena* parse_temp_arena = arena_create(MiB(256), KiB(128));
  Arena* str_arena = arena_create(MiB(256), KiB(128));

  char* input;
  char* output_dir;
  int verbose;
  bool syntax_only;
  parse_commandline(argc, argv, &input, &output_dir, &verbose, &syntax_only);

  str_intern_pool_init(str_arena);

  module_init(main_arena, parse_temp_arena, /* TODO: basepath of input */ ".", output_dir,
              verbose, syntax_only);
  module_add((StrView){input, strlen(input)});
#if 0

  parse_scan_for_imports(input, file, verbose);
  TokenizedBuffer tokbuf = parse_scan_for_imports(input, file, verbose);

  if (syntax_only) {
    parse_syntax_check(parse_temp_arena, tokbuf, verbose);
  } else {
    FILE* out_file = NULL;
    if (!verbose) {
      out_file = fopen(output, "wb");
      if (!out_file) {
        base_writef_stderr("Couldn't open '%s' for output.\n", out_file);
        return 1;
      }
    }
    parse_code_gen(parse_temp_arena, tokbuf, verbose, out_file);
  }
#endif
  return 0;
}
