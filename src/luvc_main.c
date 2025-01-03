#include "luv60.h"

static void usage(void) {
  base_writef_stderr("usage: luvc {-run|-dump} file.luv\n");
  base_exit(1);
}

int main(int argc, char** argv) {
  if (argc != 3) {
    usage();
  }
  bool do_run = strcmp(argv[1], "-run") == 0;
  bool do_dump = strcmp(argv[1], "-dump") == 0;
  if (!do_run && !do_dump) {
    usage();
  }

  str_intern_pool_init();

  const char* filename = argv[2];
  ReadFileResult file = base_read_file(filename);
  if (!file.buffer) {
    base_writef_stderr("Couldn't read '%s'\n", filename);
    return 1;
  }

  gen_init();
  parse(filename, file);
  if (do_dump) {
    gen_finish_and_dump();
  }
  if (do_run) {
    return gen_finish_and_run();
  }
  return 0;
}
