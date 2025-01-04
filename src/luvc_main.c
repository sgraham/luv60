#include "luv60.h"

static void usage(void) {
  base_writef_stderr("usage: luvc [-dump] file.luv\n");
  base_exit(1);
}

int main(int argc, char** argv) {
  if (argc != 2 && argc != 3) {
    usage();
  }
  bool do_dump = strcmp(argv[1], "-dump") == 0;

  str_intern_pool_init();

  const char* filename = argv[argc - 1];
  ReadFileResult file = base_read_file(filename);
  if (!file.buffer) {
    base_writef_stderr("Couldn't read '%s'\n", filename);
    return 1;
  }

  gen_init();

  parse(filename, file);

  if (do_dump) {
    gen_finish_and_dump();
    base_writef_stderr("[not running]\n");
    return 0;
  }

  int rc = gen_finish_and_run();
  printf("ret: %d\n", rc);
  return rc;
}
