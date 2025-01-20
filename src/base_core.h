#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if COMPILER_MSVC
#  define FORCE_INLINE __forceinline
#elif COMPILER_CLANG || COMPILER_GCC
#  define FORCE_INLINE __attribute__((always_inline))
#endif

#if COMPILER_CLANG
#  define BRANCH_EXPECT(expr, val) __builtin_expect((expr), (val))
#else
#  define BRANCH_EXPECT(expr, val) (expr)
#endif

#define BRANCH_LIKELY(expr) BRANCH_EXPECT(expr, 1)
#define BRANCH_UNLIKELY(expr) BRANCH_EXPECT(expr, 0)

#define MIN(x, y) ((x) <= (y) ? (x) : (y))
#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#define CLAMP_MAX(x, max) MIN(x, max)
#define CLAMP_MIN(x, min) MAX(x, min)

#define IS_POW2(x) (((x) != 0) && ((x) & ((x)-1)) == 0)
#define ALIGN_DOWN(n, a) ((n) & ~((a)-1))
#define ALIGN_UP(n, a) ALIGN_DOWN((n) + (a)-1, (a))
#define ALIGN_DOWN_PTR(p, a) ((void*)ALIGN_DOWN((uintptr_t)(p), (a)))
#define ALIGN_UP_PTR(p, a) ((void*)ALIGN_UP((uintptr_t)(p), (a)))

#define COUNTOF(a) (sizeof(a)/sizeof(a[0]))
#define COUNTOFI(a) ((int)(sizeof(a)/sizeof(a[0])))

#if COMPILER_MSVC
#  define TRAP() __debugbreak()
#elif COMPILER_CLANG || COMPILER_GCC
#  define TRAP() __builtin_trap()
#else
#  error port
#endif

#define CHECK(x)                                                            \
  do {                                                                      \
    if (!(x)) {                                                             \
      fprintf(stderr, "%s:%d: CHECK failed: " #x "\n", __FILE__, __LINE__); \
      TRAP();                                                               \
    }                                                                       \
  } while (0)

#if BUILD_DEBUG
#  define ASSERT(x) CHECK(x)
#else
#  define ASSERT(x) (void)(x)
#endif

#if COMPILER_MSVC
#  define NORETURN __declspec(noreturn)
#elif COMPILER_CLANG || COMPILER_GCC
#  define NORETURN __attribute__((noreturn))
#else
#  error port
#endif

#if COMPILER_CLANG || COMPILER_GCC
#  define WARN_UNUSED __attribute((warn_unused_result))
#elif COMPILER_MSVC
#  define WARN_UNUSED
#  error port
#endif
