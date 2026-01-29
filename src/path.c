#include "luv60.h"

void path_normalize_to_slash_in_place(char* path) {
  for (char* p = path; *p; ++p) {
    if (*p == '\\') {
      *p = '/';
    }
  }
}

void path_without_slashes_in_place(char* path) {
  for (char* p = path; *p; ++p) {
    if (*p == '/' || *p == '\\') {
      *p = '.';
    }
  }
}

void path_split(Arena* arena, const char* input, const char** source_dir, const char** filename) {
  size_t input_len = strlen(input);
  char* input_copy = arena_push(arena, input_len + 1, 1);
  memcpy(input_copy, input, input_len + 1);
  path_normalize_to_slash_in_place(input_copy);
  char* last_slash = strrchr(input_copy, '/');
  if (!last_slash) {
    *source_dir = ".";
    *filename = input_copy;
  } else {
    *last_slash = 0;
    *source_dir = input_copy;
    *filename = last_slash + 1;
  }
}

void path_trim_extension_if_exists(char* filename, char* ext) {
  CHECK(ext[0] == '.');
  size_t filename_len = strlen(filename);
  size_t ext_len = strlen(ext);
  if (strcmp(&filename[filename_len - ext_len], ext) == 0) {
    filename[filename_len - ext_len] = 0;
  }
}
