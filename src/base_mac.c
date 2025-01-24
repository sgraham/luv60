#include "luv60.h"

#if !OS_MAC
#error
#endif

#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

int base_writef_stderr(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int ret = vfprintf(stderr, fmt, args);
  va_end(args);
  return ret;
}

uint64_t base_page_size(void) {
  return sysconf(_SC_PAGE_SIZE);
}

void base_timer_init(void) {
}

uint64_t base_timer_now(void) {
  return clock_gettime_nsec_np(CLOCK_UPTIME_RAW) / 1000;
}

void* base_mem_reserve(uint64_t size) {
  void* result = mmap(0, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (result == MAP_FAILED) {
    result = NULL;
  }
  return result;
}

bool base_mem_commit(void* ptr, uint64_t size) {
  mprotect(ptr, size, PROT_READ|PROT_WRITE);
  return true;
}

void* base_mem_large_alloc(uint64_t size) {
  void* result = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (result == MAP_FAILED) {
    result = NULL;
  }
  return result;
}

void base_mem_decommit(void* ptr, uint64_t size) {
  madvise(ptr, size, MADV_DONTNEED);
  mprotect(ptr, size, PROT_NONE);
}

void base_mem_release(void* ptr, uint64_t size) {
  munmap(ptr, size);
}

ReadFileResult base_read_file(const char* filename) {
  FILE* f = fopen(filename, "rb");
  if (!f) {
    return (ReadFileResult){0};
  }

  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  rewind(f);

  size_t page_size = base_page_size();
  size_t to_alloc = ALIGN_UP(len + 64, page_size);
  unsigned char* read_buf = base_mem_large_alloc(to_alloc);
  if (!read_buf) {
    fclose(f);
    return (ReadFileResult){0};
  }

  size_t bytes_read = fread(read_buf, 1, len, f);
  fclose(f);

  if (bytes_read != len) {
    base_mem_release(read_buf, to_alloc);
    return (ReadFileResult){0};
  }

  return (ReadFileResult){read_buf, len, to_alloc};
}

NORETURN void base_exit(int rc) {
  exit(rc);
}
