#include "luv60.h"

static void usage(void) {
  base_writef_stderr("usage: luvc [-v] file.luv\n");
  base_exit(1);
}

int main(int argc, char** argv) {
  if (argc < 2) {
    usage();
  }

  bool verbose = strcmp(argv[1], "-v") == 0;

  str_intern_pool_init();

  const char* filename = argv[verbose ? 2 : 1];
  ReadFileResult file = base_read_file(filename);
  if (!file.buffer) {
    base_writef_stderr("Couldn't read '%s'\n", filename);
    return 1;
  }

  gen_mir_init(verbose);

  parse(filename, file);

  int rc = gen_mir_finish();
  if (verbose) {
    printf("main() returned %d\n", rc);
  }
  return rc;
}
