#include "luv60.h"

static void usage(void) {
  base_writef_stderr("usage: luvc file.luv file.ssa\n");
  base_exit(1);
}

int main(int argc, char** argv) {
  if (argc != 3) {
    usage();
  }

  str_intern_pool_init();

  const char* filename = argv[1];
  ReadFileResult file = base_read_file(filename);
  if (!file.buffer) {
    base_writef_stderr("Couldn't read '%s'\n", filename);
    return 1;
  }

  gen_ssa_init();

  parse(filename, file);

  gen_ssa_finish(argv[2]);
  return 0;
}
