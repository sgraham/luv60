#include "luv60.h"

#if !OS_WINDOWS
#error
#endif

#include "base_some_windows.h"

int base_writef_stderr(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int ret = vfprintf(stderr, fmt, args);
  va_end(args);
  return ret;
}

unsigned char* base_large_alloc_rw(size_t size) {
  void* p = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  ASSERT(ALIGN_UP((uintptr_t)p, 64) == (uintptr_t)p);
  return p;
}

unsigned char* base_large_alloc_rwx(size_t size) {
  void *p = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
  ASSERT(ALIGN_UP((uintptr_t)p, 64) == (uintptr_t)p);
  return p;
}

void base_set_protection_rx(unsigned char* ptr, size_t size) {
  DWORD old_protect;
  VirtualProtect(ptr, size, PAGE_EXECUTE_READ, &old_protect);
}

void base_large_alloc_free(void* ptr) {
  VirtualFree(ptr, 0, MEM_RELEASE);
}

ReadFileResult base_read_file(const char* filename) {
  SECURITY_ATTRIBUTES sa = {sizeof(sa), 0, 0};
  HANDLE file = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, &sa,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  if (file == INVALID_HANDLE_VALUE) {
    return (ReadFileResult){0};
  }
  LARGE_INTEGER size;
  GetFileSizeEx(file, &size);

  size_t to_alloc = ALIGN_UP(size.QuadPart + 64, PAGE_SIZE);
  unsigned char* read_buf = base_large_alloc_rw(to_alloc);
  if (!read_buf) {
    CloseHandle(file);
    return (ReadFileResult){0};
  }

  DWORD bytes_read = 0;
  if (!ReadFile(file, read_buf, size.QuadPart, &bytes_read, NULL)) {
    base_large_alloc_free(read_buf);
    return (ReadFileResult){0};
  }
  CloseHandle(file);

  if (bytes_read != size.QuadPart) {
    base_large_alloc_free(read_buf);
    return (ReadFileResult){0};
  }

  return (ReadFileResult){read_buf, size.QuadPart, to_alloc};
}

NORETURN void base_exit(int rc) {
  ExitProcess(rc);
}
