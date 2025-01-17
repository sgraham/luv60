#include "luv60.h"

static void parse_commandline(int argc,
                              char** argv,
                              Str* input,
                              Str* output,
                              bool* verbose,
                              LqTarget* target) {
  int i = 1;
  *verbose = false;
  *target = LQ_TARGET_DEFAULT;
  *input = (Str){0};
  *output = (Str){0};
  while (i < argc) {
    if (strcmp(argv[i], "-v") == 0) {
      *verbose = true;
      ++i;
    } else if (strcmp(argv[i], "-t") == 0) {
      if (i < argc - 1) {
        ++i;
        if (strcmp(argv[i], "amd64_apple") == 0) *target = LQ_TARGET_AMD64_APPLE;
        else if (strcmp(argv[i], "amd64_sysv") == 0) *target = LQ_TARGET_AMD64_SYSV;
        else if (strcmp(argv[i], "amd64_win") == 0) *target = LQ_TARGET_AMD64_WINDOWS;
        else if (strcmp(argv[i], "arm64") == 0) *target = LQ_TARGET_ARM64;
        else if (strcmp(argv[i], "arm64_apple") == 0) *target = LQ_TARGET_ARM64_APPLE;
        else if (strcmp(argv[i], "rv64") == 0) *target = LQ_TARGET_RV64;
        else {
          base_writef_stderr("Unexpected target '%s'.\n", argv[i]);
          base_exit(1);
        }
        ++i;
      } else {
        base_writef_stderr("Expecting target name after -t.");
        base_exit(1);
      }
    } else if (strcmp(argv[i], "-o") == 0) {
      if (!str_is_none(*output)) {
        base_writef_stderr("Can only specify a single output file.\n");
        base_exit(1);
      }
      if (i < argc - 1) {
        *output = str_intern(argv[i + 1]);
        i += 2;
      } else {
        base_writef_stderr("Expecting output name after -o.");
        base_exit(1);
      }
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
  if (str_is_none(*output)) {
    base_writef_stderr("No output file specified.\n");
    base_exit(1);
  }
}

int main(int argc, char** argv) {
  str_intern_pool_init();

  Str input;
  Str output;
  bool verbose;
  LqTarget target;
  parse_commandline(argc, argv, &input, &output, &verbose, &target);

  ReadFileResult file = base_read_file(cstr(input));
  if (!file.buffer) {
    base_writef_stderr("Couldn't read '%s'\n", cstr(input));
    return 1;
  }

  FILE* into = fopen(cstr(output), "wb");
  if (!into) {
    base_writef_stderr("Couldn't open '%s'\n", cstr(output));
    return 1;
  }
  lq_init(target, into, verbose ? "PMNCFAILSRT" : "");

  parse(cstr(input), file);

  lq_shutdown();
  return 0;
}
