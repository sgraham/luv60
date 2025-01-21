#include "luv60.h"

#if !OS_WINDOWS
#error
#endif

#include <windows.h>

int base_writef_stderr(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int ret = vfprintf(stderr, fmt, args);
  va_end(args);
  return ret;
}

uint64_t base_page_size(void) {
  SYSTEM_INFO sysInfo;
  GetSystemInfo(&sysInfo);
  return sysInfo.dwPageSize;
}

void *base_mem_reserve(uint64_t size) {
  return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
}

bool base_mem_commit(void* ptr, uint64_t size) {
  return VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != 0;
}

void base_mem_decommit(void* ptr, uint64_t size) {
  VirtualFree(ptr, size, MEM_DECOMMIT);
}

void base_mem_release(void* ptr, uint64_t size) {
  VirtualFree(ptr, 0, MEM_RELEASE);
}

ReadFileResult base_read_file(Arena* arena, const char* filename) {
  SECURITY_ATTRIBUTES sa = {sizeof(sa), 0, 0};
  HANDLE file = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, &sa,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  if (file == INVALID_HANDLE_VALUE) {
    return (ReadFileResult){0};
  }
  LARGE_INTEGER size;
  GetFileSizeEx(file, &size);

  size_t page_size = base_page_size();
  size_t to_alloc = ALIGN_UP(size.QuadPart + 64, page_size);
  unsigned char* read_buf = arena_push(arena, to_alloc, page_size);
  if (!read_buf) {
    CloseHandle(file);
    return (ReadFileResult){0};
  }

  DWORD bytes_read = 0;
  if (!ReadFile(file, read_buf, size.QuadPart, &bytes_read, NULL)) {
    return (ReadFileResult){0};
  }
  CloseHandle(file);

  if (bytes_read != size.QuadPart) {
    return (ReadFileResult){0};
  }

  return (ReadFileResult){read_buf, size.QuadPart, to_alloc};
}

NORETURN void base_exit(int rc) {
  ExitProcess(rc);
}

static uint64_t microsecond_resolution_;

void base_timer_init(void) {
  microsecond_resolution_ = 1;
  LARGE_INTEGER large_int_resolution;
  if (QueryPerformanceFrequency(&large_int_resolution)) {
    microsecond_resolution_ = large_int_resolution.QuadPart;
  }
}

uint64_t base_timer_now(void) {
  uint64_t result = 0;
  LARGE_INTEGER large_int_counter;
  if (QueryPerformanceCounter(&large_int_counter)) {
    result = (large_int_counter.QuadPart * (1000000)) / microsecond_resolution_;
  }
  return result;
}
