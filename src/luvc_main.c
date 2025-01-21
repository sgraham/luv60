#include "luv60.h"

static void parse_commandline(int argc,
                              char** argv,
                              Str* input,
                              bool* verbose,
                              bool* syntax_only,
                              bool* return_main_rc,
                              int* opt_level) {
  int i = 1;
  *verbose = false;
  *return_main_rc = false;
  *syntax_only = false;
  *input = (Str){0};
  *opt_level = 1;
  while (i < argc) {
    if (strcmp(argv[i], "-v") == 0) {
      *verbose = true;
      ++i;
    } else if (strcmp(argv[i], "--main-rc") == 0) {
      *return_main_rc = true;
      ++i;
    } else if (strcmp(argv[i], "--syntax-only") == 0) {
      *syntax_only = true;
      ++i;
    } else if (strcmp(argv[i], "--opt") == 0) {
      *opt_level = atoi(argv[i+1]);
      if (*opt_level < 0 || *opt_level > 2) {
        base_writef_stderr("Valid optimization levels are 0, 1, 2.\n");
        base_exit(1);
      }
      i += 2;
    } else {
      if (!str_is_none(*input)) {
        base_writef_stderr("Can only specify a single input file.\n");
        base_exit(1);
      }
      *input = str_intern(argv[i]);
      ++i;
    }
  }

  if (str_is_none(*input)) {
    base_writef_stderr("No input file specified.\n");
    base_exit(1);
  }
}

int main(int argc, char** argv) {
  Arena* main_arena = arena_create(MiB(256), KiB(128));
  Arena* parse_temp_arena = arena_create(MiB(256), KiB(128));
  Arena* str_arena = arena_create(MiB(256), KiB(128));
  arena_ir = arena_create(MiB(256), KiB(128));

  str_intern_pool_init(str_arena);

  Str input;
  bool verbose;
  bool syntax_only;
  bool return_main_rc;
  int opt_level;
  parse_commandline(argc, argv, &input, &verbose, &syntax_only, &return_main_rc, &opt_level);

  ReadFileResult file = base_read_file(cstr(input));
  if (!file.buffer) {
    base_writef_stderr("Couldn't read '%s'\n", cstr(input));
    return 1;
  }

  if (syntax_only) {
    parse_syntax_check(main_arena, parse_temp_arena, cstr(input), file, verbose, opt_level);
    return 0;
  } else {
    void* entry =
        parse_code_gen(main_arena, parse_temp_arena, cstr(input), file, verbose, opt_level);
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
