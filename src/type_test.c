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

  Type a = type_function(NULL, 0, type_i32, TFF_NONE);
  Type b = type_function(NULL, 0, type_i32, TFF_NONE);
  EXPECT_EQ(a.u, b.u);
  EXPECT_TRUE(type_eq(a, b));

  Type c = type_function(NULL, 0, type_bool, TFF_NONE);
  EXPECT_TRUE(!type_eq(a, c));

  Type d = type_function(NULL, 0, type_void, TFF_NONE);
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

  Type a0 = type_function(params_a, 4, type_void, TFF_NONE);
  Type a0_again = type_function(params_a, 4, type_void, TFF_NONE);
  EXPECT_TRUE(type_eq(a0, a0_again));

  Type a1 = type_function(params_a, 4, type_bool, TFF_NONE); // diff return
  EXPECT_TRUE(!type_eq(a0, a1));

  Type b = type_function(params_b, 4, type_void, TFF_NONE);
  EXPECT_TRUE(!type_eq(a0, b));  // diff args, same return

  Type c = type_function(params_c, 3, type_void, TFF_NONE);
  EXPECT_TRUE(!type_eq(a0, c));  // same return, same args up to 3

  type_destroy_for_tests();
  arena_destroy(arena);
}

TEST(Type, FuncNested) {
  Arena* arena = arena_create(KiB(128), KiB(128));
  type_init(arena);

  Type params[1] = { type_i32 };

  Type a0 = type_function(params, 1, type_void, TFF_NESTED);
  Type a0_again = type_function(params, 1, type_void, TFF_NESTED);
  EXPECT_TRUE(type_eq(a0, a0_again));

  Type a1 = type_function(params, 1, type_void, TFF_NONE);
  EXPECT_TRUE(!type_eq(a0, a1));

  EXPECT_EQ(type_func_is_nested(a0), true);
  EXPECT_EQ(type_func_is_nested(a1), false);

  // nested is just a flag, other code has to make arg0 a void* and do something
  // useful with the flag on the call.
  EXPECT_EQ(type_func_num_params(a0), 1);
  EXPECT_EQ(type_func_num_params(a1), 1);

  type_destroy_for_tests();
  arena_destroy(arena);
}

TEST(Type, FuncMemfn) {
  Arena* arena = arena_create(KiB(128), KiB(128));
  type_init(arena);

  Type params[1] = { type_i32 };

  Type a0 = type_function(params, 1, type_void, TFF_MEMFN);
  Type a0_again = type_function(params, 1, type_void, TFF_MEMFN);
  EXPECT_TRUE(type_eq(a0, a0_again));

  Type a1 = type_function(params, 1, type_void, TFF_NESTED);
  EXPECT_TRUE(!type_eq(a0, a1));

  Type a2 = type_function(params, 1, type_void, TFF_NONE);
  EXPECT_TRUE(!type_eq(a0, a2));

  EXPECT_EQ(type_func_is_memfn(a0), true);
  EXPECT_EQ(type_func_is_memfn(a1), false);
  EXPECT_EQ(type_func_is_memfn(a2), false);

  // memfn is just a flag, other code has to bind self.
  EXPECT_EQ(type_func_num_params(a0), 1);
  EXPECT_EQ(type_func_num_params(a1), 1);

  EXPECT_TRUE(type_func_flags(a0) & TFF_MEMFN);

  type_destroy_for_tests();
  arena_destroy(arena);
}

TEST(Type, CrackFuncs) {
  Arena* arena = arena_create(KiB(128), KiB(128));
  type_init(arena);

  Type params[4] = { type_i32, type_i32, type_bool, type_double };

  Type f = type_function(params, 4, type_float, TFF_NONE);
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

TEST(Type, StructBasic) {
  Arena* arena = arena_create(KiB(128), KiB(128));
  type_init(arena);

  Str name = str_intern("Testy");
  Str names[3] = {str_intern("a"), str_intern("b"), str_intern("c")};
  Type types[3] = {type_i32, type_bool, type_i32};

  Type strukt = type_new_struct(name, 3, names, types, false);
  Type strukt2 = type_new_struct(name, 3, names, types, false);
  EXPECT_TRUE(!type_eq(strukt, strukt2));

  EXPECT_EQ(type_struct_num_fields(strukt), 3);

  EXPECT_TRUE(str_eq(type_struct_field_name(strukt, 0), str_intern("a")));
  EXPECT_TRUE(str_eq(type_struct_field_name(strukt, 1), str_intern("b")));
  EXPECT_TRUE(str_eq(type_struct_field_name(strukt, 2), str_intern("c")));

  EXPECT_TRUE(type_eq(type_struct_field_type(strukt, 0), type_i32));
  EXPECT_TRUE(type_eq(type_struct_field_type(strukt, 1), type_bool));
  EXPECT_TRUE(type_eq(type_struct_field_type(strukt, 2), type_i32));

  EXPECT_EQ(type_struct_field_offset(strukt, 0), 0);
  EXPECT_EQ(type_struct_field_offset(strukt, 1), 4);
  EXPECT_EQ(type_struct_field_offset(strukt, 2), 8);

  EXPECT_EQ(type_size(strukt), 12);
  EXPECT_EQ(type_align(strukt), 4);

  type_destroy_for_tests();
  arena_destroy(arena);
}

TEST(Type, StructInitializer) {
  Arena* arena = arena_create(KiB(128), KiB(128));
  type_init(arena);

  Str name = str_intern("Testy");
  Str names[3] = {str_intern("a"), str_intern("b"), str_intern("c")};
  Type types[3] = {type_i32, type_bool, type_i32};

  struct Hacky {
    int a;
    bool b;
    int c;
  };
  struct Hacky hacky = {44, true, 13};
  Type strukt = type_new_struct(name, 3, names, types, true);
  type_struct_set_initializer_blob(strukt, &hacky);
  Type strukt2 = type_new_struct(name, 3, names, types, false);
  EXPECT_TRUE(!type_eq(strukt, strukt2));

  EXPECT_EQ(type_struct_has_initializer(strukt), true);
  EXPECT_EQ(type_struct_has_initializer(strukt2), false);
  EXPECT_EQ(type_struct_initializer_blob(strukt), &hacky);

  EXPECT_EQ(type_struct_num_fields(strukt), 3);

  EXPECT_TRUE(str_eq(type_struct_field_name(strukt, 0), str_intern("a")));
  EXPECT_TRUE(str_eq(type_struct_field_name(strukt, 1), str_intern("b")));
  EXPECT_TRUE(str_eq(type_struct_field_name(strukt, 2), str_intern("c")));

  EXPECT_TRUE(type_eq(type_struct_field_type(strukt, 0), type_i32));
  EXPECT_TRUE(type_eq(type_struct_field_type(strukt, 1), type_bool));
  EXPECT_TRUE(type_eq(type_struct_field_type(strukt, 2), type_i32));

  EXPECT_EQ(type_struct_field_offset(strukt, 0), 0);
  EXPECT_EQ(type_struct_field_offset(strukt, 1), 4);
  EXPECT_EQ(type_struct_field_offset(strukt, 2), 8);

  EXPECT_EQ(type_size(strukt), 12);
  EXPECT_EQ(type_align(strukt), 4);

  type_destroy_for_tests();
  arena_destroy(arena);
}

TEST(Type, ArrayBasic) {
  Arena* arena = arena_create(KiB(128), KiB(128));
  type_init(arena);

  Type a0 = type_array(type_i32, 16);
  Type a1 = type_array(type_i32, 16);
  EXPECT_TRUE(type_eq(a0, a1));

  Type a2 = type_array(type_i32, 17);
  EXPECT_TRUE(!type_eq(a0, a2));
  EXPECT_TRUE(!type_eq(a1, a2));

  Type a3 = type_array(type_u32, 16);
  EXPECT_TRUE(!type_eq(a0, a3));
  EXPECT_TRUE(!type_eq(a1, a3));
  EXPECT_TRUE(!type_eq(a2, a3));

  EXPECT_TRUE(type_eq(type_array_subtype(a0), type_i32));
  EXPECT_TRUE(type_eq(type_array_subtype(a1), type_i32));
  EXPECT_TRUE(type_eq(type_array_subtype(a2), type_i32));
  EXPECT_TRUE(type_eq(type_array_subtype(a3), type_u32));

  EXPECT_EQ(type_array_count(a0), 16);
  EXPECT_EQ(type_array_count(a1), 16);
  EXPECT_EQ(type_array_count(a2), 17);
  EXPECT_EQ(type_array_count(a3), 16);

  type_destroy_for_tests();
  arena_destroy(arena);
}

TEST(Type, ListBasic) {
  Arena* arena = arena_create(KiB(128), KiB(128));
  type_init(arena);

  Type a0 = type_list(type_i32);
  Type a1 = type_list(type_i32);
  EXPECT_TRUE(type_eq(a0, a1));

  Type a2 = type_list(type_u32);
  EXPECT_TRUE(!type_eq(a0, a2));
  EXPECT_TRUE(!type_eq(a1, a2));

  type_destroy_for_tests();
  arena_destroy(arena);
}
