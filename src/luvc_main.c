#include "luv60.h"

int main() {
  const char* filename = "basic.luv";
  ReadFileResult file = base_read_file(filename);
  if (!file.buffer) {
    base_writef_stderr("Couldn't read '%s'\n", filename);
    return 1;
  }

  parse(filename, file);
  return 0;
}
