#include "luv60.h"
#include "test.h"

TEST(Type, Basic) {
  Arena* arena = arena_create(KiB(128), KiB(128));
  type_init(arena);
  type_destroy_for_tests();
  arena_destroy(arena);
}

TEST(Type, FuncNoParam) {
  Arena* arena = arena_create(KiB(128), KiB(128));
  type_init(arena);

  Type a = type_function(NULL, 0, type_i32);
  Type b = type_function(NULL, 0, type_i32);
  EXPECT_EQ(a.u, b.u);
  EXPECT_TRUE(type_eq(a, b));

  Type c = type_function(NULL, 0, type_bool);
  EXPECT_TRUE(!type_eq(a, c));

  Type d = type_function(NULL, 0, type_void);
  EXPECT_TRUE(!type_eq(a, d));
  EXPECT_TRUE(!type_eq(c, d));

  type_destroy_for_tests();
  arena_destroy(arena);
}

TEST(Type, FuncWithBasicParams) {
  Arena* arena = arena_create(KiB(128), KiB(128));
  type_init(arena);

  Type params_a[4] = { type_i32, type_i32, type_bool, type_double };
  Type params_b[4] = { type_i32, type_bool, type_float, type_i32 };
  Type params_c[3] = { type_i32, type_i32, type_bool };

  Type a0 = type_function(params_a, 4, type_void);
  Type a0_again = type_function(params_a, 4, type_void);
  EXPECT_TRUE(type_eq(a0, a0_again));

  Type a1 = type_function(params_a, 4, type_bool); // diff return
  EXPECT_TRUE(!type_eq(a0, a1));

  Type b = type_function(params_b, 4, type_void);
  EXPECT_TRUE(!type_eq(a0, b));  // diff args, same return

  Type c = type_function(params_c, 3, type_void);
  EXPECT_TRUE(!type_eq(a0, c));  // same return, same args up to 3

  type_destroy_for_tests();
  arena_destroy(arena);
}

TEST(Type, CrackFuncs) {
  Arena* arena = arena_create(KiB(128), KiB(128));
  type_init(arena);

  Type params[4] = { type_i32, type_i32, type_bool, type_double };

  Type f = type_function(params, 4, type_float);
  EXPECT_EQ(type_kind(f), TYPE_FUNC);
  EXPECT_EQ(type_func_num_params(f), 4);
  EXPECT_TRUE(type_eq(type_func_return_type(f), type_float));
  EXPECT_TRUE(type_eq(type_func_param(f, 0), type_i32));
  EXPECT_TRUE(type_eq(type_func_param(f, 1), type_i32));
  EXPECT_TRUE(type_eq(type_func_param(f, 2), type_bool));
  EXPECT_TRUE(type_eq(type_func_param(f, 3), type_double));

  type_destroy_for_tests();
  arena_destroy(arena);
}

TEST(Type, Ptr) {
  Arena* arena = arena_create(KiB(128), KiB(128));
  type_init(arena);

  Type a = type_ptr(type_i32);
  Type b = type_ptr(type_i32);
  EXPECT_TRUE(type_eq(a, b));
  Type c = type_ptr(type_bool);
  EXPECT_TRUE(!type_eq(a, c));

  type_destroy_for_tests();
  arena_destroy(arena);
}
