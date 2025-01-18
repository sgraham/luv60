#include "luv60.h"

static void parse_commandline(int argc,
                              char** argv,
                              Str* input,
                              bool* verbose) {
#if 0
                              LqTarget* target) {
#endif
  int i = 1;
  *verbose = false;
  *input = (Str){0};
  while (i < argc) {
    if (strcmp(argv[i], "-v") == 0) {
      *verbose = true;
      ++i;
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
  str_intern_pool_init();

  Str input;
  bool verbose;
  parse_commandline(argc, argv, &input, &verbose);

  ReadFileResult file = base_read_file(cstr(input));
  if (!file.buffer) {
    base_writef_stderr("Couldn't read '%s'\n", cstr(input));
    return 1;
  }

  void* entry = parse(cstr(input), file);
  if (entry) {
    int rc = ((int (*)())entry)();
    if (verbose) {
      printf("main() returned %d\n", rc);
    }
  }

  return 0;
}
