#include "luv60.h"

// Cannot use Str as it's not initialized yet.
static void parse_commandline(int argc,
                              char** argv,
                              char** input,
                              char** output,
                              int* verbose,
                              bool* syntax_only) {
  int i = 1;
  *verbose = 0;
  *syntax_only = false;
  *input = NULL;
  *output = NULL;
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
        base_writef_stderr("Can only specify a single output directory.\n");
        base_exit(1);
      }
      *output = argv[i + 1];
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
  if (!*output && !*syntax_only && !*verbose) {
    base_writef_stderr("No output directory specified.\n");
    base_exit(1);
  }
}

int main(int argc, char** argv) {
  Arena* main_arena = arena_create(MiB(256), KiB(128));
  Arena* parse_temp_arena = arena_create(MiB(256), KiB(128));
  Arena* str_arena = arena_create(MiB(256), KiB(128));

  char* input;
  char* output;
  int verbose;
  bool syntax_only;
  parse_commandline(argc, argv, &input, &output, &verbose, &syntax_only);

  ReadFileResult file = base_read_file(input);
  if (!file.buffer) {
    base_writef_stderr("Couldn't read '%s'\n", input);
    return 1;
  }

  str_intern_pool_init(str_arena);

  if (syntax_only) {
    parse_syntax_check(main_arena, parse_temp_arena, input, file, verbose);
  } else {
    FILE* out_file = NULL;
    if (!verbose) {
      out_file = fopen(output, "wb");
      if (!out_file) {
        base_writef_stderr("Couldn't open '%s' for output.\n", out_file);
        return 1;
      }
    }
    parse_code_gen(main_arena, parse_temp_arena, input, file, verbose, out_file);
  }
  return 0;
}
