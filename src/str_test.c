#include "luv60.h"
#include "test.h"

TEST(Str, Intern) {
  str_intern_pool_init();

  char a[] = "hello";
  EXPECT_STREQ(a, cstr(str_intern(a)));
  EXPECT_EQ(str_intern(a).i, str_intern(a).i);
  EXPECT_EQ(str_intern(cstr(str_intern(a))).i, str_intern(a).i);
  char b[] = "hello";
  EXPECT_TRUE(a != b);
  EXPECT_EQ(str_intern(a).i, str_intern(b).i);
  char c[] = "hello!";
  EXPECT_TRUE(str_intern(a).i != str_intern(c).i);
  char d[] = "hell";
  EXPECT_TRUE(str_intern(a).i != str_intern(d).i);

  str_intern_pool_destroy_for_tests();
}

TEST(Str, Internf) {
  str_intern_pool_init();

  Str x = str_internf("hi%d", 44);
  EXPECT_STREQ(cstr(x), "hi44");

  Str y = str_intern("hi44");
  EXPECT_EQ(x.i, y.i);
  EXPECT_TRUE(str_eq(x, y));

  str_intern_pool_destroy_for_tests();
}
