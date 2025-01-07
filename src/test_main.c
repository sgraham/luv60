#include "test.h"

Test* tests_;
Test* cur_test_;

int main(int argc, char** argv) {
  bool quiet = false;
  bool verbose = false;
  if (argc >= 2 && argv[1][0] == '-' && argv[1][1] == 'q') {
    quiet = true;
  }
  if (argc >= 2 && argv[1][0] == '-' && argv[1][1] == 'v') {
    verbose = true;
  }

  int tests_passed = 0;
  int tests_failed = 0;
  for (Test* t = tests_; t; t = t->next) {
    cur_test_ = t;
    if (!quiet) {
      if (verbose) {
        printf("[RUN %s]\n", t->name);
      } else {
        printf("\r[RUN %s]\x1B[K", t->name);
      }
    }
    t->testp();
    if (cur_test_->failed) {
      ++tests_failed;
    } else {
      ++tests_passed;
    }
  }

  int result;

  const char* green = "\033[32m";
  const char* red = "\033[31m";
  const char* reset = "\033[0m";

  if (tests_failed == 0) {
    if (!quiet) {
      if (verbose) {
        printf("\nAll %s%d%s unit tests passed\n", green, tests_passed, reset);
      } else {
        printf("\rAll %s%d%s unit tests passed\x1B[K\n", green, tests_passed, reset);
      }
    }
    result = 0;
  } else {
    printf("\n\nTEST FAILURES: %s%d%s passed, %s%d%s failed\n", green, tests_passed, reset, red,
           tests_failed, reset);
    result = 1;
  }

  return result;
}
