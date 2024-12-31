#include "base_config.h"
#include "base_core.h"

#if _WIN32
// TODO: a few extern and defines instead
#include <windows.h>
#endif

typedef struct ReadFileResult {
  unsigned char* buffer;
  size_t file_size;
  size_t allocated_size;
} ReadFileResult;

#if OS_WINDOWS
#  include "base_win.c"
#elif OS_MAC
#  include "base_mac.c"
#else
#  error port
#endif
