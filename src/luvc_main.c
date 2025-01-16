#include "luv60.h"

static void usage(void) {
  base_writef_stderr("usage: luvc [-v] file.luv file.s\n");
  base_exit(1);
}

int main(int argc, char** argv) {
  if (argc < 3) {
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

  const char* output_name = argv[verbose ? 3 : 2];
  FILE* into = fopen(output_name, "wb");
  if (!into) {
    base_writef_stderr("Couldn't open '%s'\n", output_name);
    return 1;
  }
  lq_init(LQ_TARGET_DEFAULT, into, verbose ? "PMNCFAILSRT" : "");

  parse(filename, file);

  lq_shutdown();
  return 0;
}
