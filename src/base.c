#include "base_config.h"
#include "base_core.h"

#if OS_WINDOWS
#include "base_some_windows.h"
#endif

#if ARCH_X64
#include <intrin.h>
#endif

typedef struct ReadFileResult {
  unsigned char* buffer;
  size_t file_size;
  size_t allocated_size;
} ReadFileResult;

static void base_writef_stderr(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}

#if OS_WINDOWS
#  include "base_win.c"
#elif OS_MAC
#  include "base_mac.c"
#else
#  error port
#endif
