#include "luv60.h"

// Cannot use Str as it's not initialized yet.
static void parse_commandline(int argc,
                              char** argv,
                              char** input,
                              int* verbose,
                              bool* syntax_only,
                              bool* ir_only,
                              bool* return_main_rc,
                              int* opt_level) {
  int i = 1;
  *verbose = 0;
  *return_main_rc = false;
  *syntax_only = false;
  *ir_only = false;
  *input = NULL;
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
  int opt_level;
  parse_commandline(argc, argv, &input, &verbose, &syntax_only, &ir_only, &return_main_rc,
                    &opt_level);

  ReadFileResult file = base_read_file(input);
  if (!file.buffer) {
    base_writef_stderr("Couldn't read '%s'\n", input);
    return 1;
  }

  str_intern_pool_init(str_arena, (char*)file.buffer, file.file_size);

  if (syntax_only) {
    parse_syntax_check(main_arena, parse_temp_arena, input, file, verbose, ir_only, opt_level);
    return 0;
  } else {
    void* entry =
        parse_code_gen(main_arena, parse_temp_arena, input, file, verbose, ir_only, opt_level);
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
