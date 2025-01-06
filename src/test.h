#pragma once

#include "luv60.h"

typedef struct Test {
  struct Test* next;
  const char* name;
  void (*testp)(void);
  bool failed;
} Test;

extern Test* tests_;
extern Test* cur_test_;

#if COMPILER_MSVC
#pragma section(".CRT$XCU", read)
#define TEST(fixn, testn)                                                                          \
  static void fixn##_##testn(void);                                                                \
  static void fixn##_##testn##_testreg(void);                                                      \
  Test fixn##_##testn##_test_data = {                                                              \
      .name = #fixn "." #testn, .testp = &fixn##_##testn, .failed = false};                        \
  __declspec(allocate(".CRT$XCU")) void (*fixn##_##testn##_regp)(void) = fixn##_##testn##_testreg; \
  __pragma(comment(                                                                                \
      linker, "/include:" #fixn "_" #testn "_regp")) static void fixn##_##testn##_testreg(void) {  \
    fixn##_##testn##_test_data.next = tests_;                                                      \
    tests_ = &fixn##_##testn##_test_data;                                                          \
  }                                                                                                \
  static void fixn##_##testn(void)
#elif COMPILER_CLANG || COMPILER_GCC
#define TEST(fixn, testn)                                                   \
  static void fixn##_##testn(void);                                         \
  Test fixn##_##testn##_test_data = {                                       \
      .name = #fixn "." #testn, .testp = &fixn##_##testn, .failed = false}; \
  __attribute__((constructor)) static void fixn##_##testn##_testreg(void) { \
    fixn##_##testn##_test_data.next = tests_;                               \
    tests_ = &fixn##_##testn##_test_data;                                   \
  }                                                                         \
  static void fixn##_##testn(void)
#endif

#define EXPECT_TRUE(cond)                                                                     \
  do {                                                                                        \
    if (!(cond)) {                                                                            \
      cur_test_->failed = true;                                                               \
      fprintf(stderr, "\n\nFAILURE at %s:%d\nCondition:\n%s\n\n", __FILE__, __LINE__, #cond); \
    }                                                                                         \
  } while (0)

#define EXPECT_EQ(a, b) EXPECT_TRUE((a) == (b))
#define EXPECT_STREQ(a, b) EXPECT_TRUE(strcmp(a, b) == 0);
