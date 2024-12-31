#pragma once

#if defined(__clang__)

#  define COMPILER_CLANG 1

#  if defined(_WIN32)
#    define OS_WINDOWS 1
#  elif defined(__gnu_linux__) || defined(__linux__)
#    define OS_LINUX 1
#  elif defined(__APPLE__) && defined(__MACH__)
#    define OS_MAC 1
#  else
#    error port
#  endif

#  if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64)
#    define ARCH_X64 1
#  elif defined(__aarch64__)
#    define ARCH_ARM64 1
#  else
#    error port
#  endif

#elif defined(_MSC_VER)

#  define COMPILER_MSVC 1

#  if defined(_WIN32)
#    define OS_WINDOWS 1
#  else
#    error port
#  endif

#  if defined(_M_AMD64)
#    define ARCH_X64 1
#  elif defined(_M_ARM64)
#    define ARCH_ARM64 1
#  else
#    error port
#  endif

#else

#  error Compiler not supported.

#endif

#if !defined(BUILD_DEBUG)
#  define BUILD_DEBUG 1
#endif

#if !defined(ARCH_X64)
#  define ARCH_X64 0
#endif
#if !defined(ARCH_ARM64)
#  define ARCH_ARM64 0
#endif
#if !defined(COMPILER_MSVC)
#  define COMPILER_MSVC 0
#endif
#if !defined(COMPILER_CLANG)
#  define COMPILER_CLANG 0
#endif
#if !defined(OS_WINDOWS)
#  define OS_WINDOWS 0
#endif
#if !defined(OS_LINUX)
#  define OS_LINUX 0
#endif
#if !defined(OS_MAC)
#  define OS_MAC 0
#endif
