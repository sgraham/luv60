#include "luv60.h"

#if !OS_MAC
#error
#endif

int base_writef_stderr(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int ret = vfprintf(stderr, fmt, args);
  va_end(args);
  return ret;
}

unsigned char* base_large_alloc_rw(size_t size) {
  const size_t align = 64;
  size_t aligned_size = ALIGN_UP(size, align);
  return aligned_alloc(align, aligned_size);
}

void base_large_alloc_free(void* ptr) {
  free(ptr);
}

ReadFileResult base_read_file(const char* filename) {
  FILE* f = fopen(filename, "rb");
  if (!f) {
    return (ReadFileResult){0};
  }

  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  rewind(f);

  size_t to_alloc = ALIGN_UP(len + 64, PAGE_SIZE);
  unsigned char* read_buf = base_large_alloc_rw(to_alloc);
  if (!read_buf) {
    fclose(f);
    return (ReadFileResult){0};
  }

  size_t bytes_read = fread(read_buf, 1, len, f);
  fclose(f);

  if (bytes_read != len) {
    base_large_alloc_free(read_buf);
    return (ReadFileResult){0};
  }

  return (ReadFileResult){read_buf, len, to_alloc};
}

NORETURN void base_exit(int rc) {
  exit(rc);
}
