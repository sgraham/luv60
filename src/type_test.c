#include "luv60.h"
#include "test.h"

TEST(Type, Basic) {
  type_init();
  type_destroy_for_tests();
}

TEST(Type, Func) {
  type_init();

  Type a = type_function(NULL, 0, (Type){TYPE_I32});
  Type b = type_function(NULL, 0, (Type){TYPE_I32});
  EXPECT_EQ(a.i, b.i);

  type_destroy_for_tests();
}
