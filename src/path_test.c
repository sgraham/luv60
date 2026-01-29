#include "luv60.h"
#include "test.h"

TEST(Path, Split) {
  Arena* arena = arena_create(KiB(128), KiB(128));

  const char* dir;
  const char* file;
  path_split(arena, "this/is_stuff\\and.luv", &dir, &file);
  EXPECT_STREQ(dir, "this/is_stuff");
  EXPECT_STREQ(file, "and.luv");

  arena_destroy(arena);
}

TEST(Path, RemoveSlashes) {
  char stuff[] = "this/is\\stuff\\and/things";
  path_without_slashes_in_place(stuff);
  EXPECT_STREQ(stuff, "this.is.stuff.and.things");
}

TEST(Path, RemoveExtension) {
  char stuff[] = "this/is/something.luv";
  path_trim_extension_if_exists(stuff, ".luv");
  EXPECT_STREQ(stuff, "this/is/something");

  char stuff2[] = "this/is/something.lu";
  path_trim_extension_if_exists(stuff2, ".luv");
  EXPECT_STREQ(stuff2, "this/is/something.lu");

  char stuff3[] = "a.luv";
  path_trim_extension_if_exists(stuff3, ".luv");
  EXPECT_STREQ(stuff3, "a");
}
