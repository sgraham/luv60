#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNTOF(a) (sizeof(a)/sizeof(a[0]))

void CheckFailed(void) {
  fprintf(stderr, "check failed!\n");
  exit(127);
}

typedef struct Str {
  const char* data;
  int64_t size;
} Str;

typedef struct Range {
  int64_t start;
  int64_t stop;
  int64_t step;
} Range;

typedef struct List {
  unsigned char* data;
  uint64_t size;
  uint64_t capacity;
} List;

static Str str_copy_cstr(const char* cstr) {
  // Note, no NUL, not sure if this will be annoying in practice.
  size_t size = strlen(cstr);
  Str ret = {malloc(size), size};
  memcpy((void*)ret.data, cstr, size);
  return ret;
}

static Str str_const_cstr(const char* cstr) {
  size_t size = strlen(cstr);
  return (Str){cstr, size};
}

void PrintStr(Str* str) {
  printf("%.*s\n", (int)str->size, str->data);
}

void List$reserve(List* list, uint64_t capacity, uint64_t item_size) {
  if (capacity <= list->capacity) {
    return;
  }
  while (list->capacity < capacity) {
    list->capacity = list->capacity > 0 ? list->capacity * 2 : 16;
  }
  list->data = realloc(list->data, list->capacity * item_size);
}

static void append_to_string_buffer_list(List* sb, Str* str) {
  List$reserve(sb, sb->size + str->size, sizeof(unsigned char));
  memcpy(&sb->data[sb->size], str->data, str->size);
  sb->size += str->size;
}

Str str$join(Str* str, List* strings) {
  List string_buffer = {0};
  for (size_t i = 0; i < strings->size; ++i) {
    Str* item = (Str*)&strings->data[i * sizeof(Str)];
    append_to_string_buffer_list(&string_buffer, item);
    if (i < strings->size - 1) {
      append_to_string_buffer_list(&string_buffer, str);
    }
  }

  return (Str){(const char*)string_buffer.data, string_buffer.size};
}

#define RT_CHECK(cond) if (!(cond)) { fprintf(stderr, "%s\n", #cond); CheckFailed(); }

#if 0
// The QBE %env is stashed in RAX on x64, or in x9 on aarch64. This is used to
// pass additional data to the type-erased implementations of the generic
// functions without needing to generate more complex thunks for each function
// per type instantiation.

#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64)

#  ifdef _MSC_VER

// This is a pretty hokey implementation, might need to make a "0xC3 ret"
// somewhere in the code segment instead that we can extern here instead so that
// we can be sure that the C compiler doesn't inline/opt/whatever this and make
// it not retrieve RAX.
#    pragma warning(push)
#    pragma warning(disable : 4716)
uint64_t _impl_NothingRetrieveRAX(void) {}
#    pragma warning(pop)

#    define GET_ENV_DATA(into) \
      { into = _impl_NothingRetrieveRAX(); }

#  else
#    error port non-msvc win
#  endif

#elif defined(__aarch64__)

#  define GET_ENV_DATA(into) asm volatile("mov %0, x9" : "=r"(into)::);

#else
#  error port
#endif

#define LOAD_LIST_DATA() uint64_t item_size; GET_ENV_DATA(item_size)
#endif

void List$free(List* list) {
  free(list->data);
  list->data = NULL;
  list->size = 0;
  list->capacity = 0;
}

uint64_t List$len(List* list) {
  return list->size;
}

void List$append(List* list, void* item, uint64_t item_size) {
  List$reserve(list, list->size + 1, item_size);
  memcpy(&list->data[list->size * item_size], item, item_size);
  list->size++;
}

void List$insert(List* list, int64_t index, void* item, uint64_t item_size) {
  RT_CHECK(index >= 0 && "todo; negative index");
  List$reserve(list, list->size + 1, item_size);
  void* at_index = &list->data[index * item_size];
  void* after_index = &list->data[(index + 1) * item_size];
  memmove(after_index, at_index, (list->size - index) * item_size);
  memcpy(at_index, item, item_size);
  list->size++;
}

Str i8$__str__(int8_t* self) {
  char buf[40];
  snprintf(buf, sizeof(buf), "%d", *self);
  return str_copy_cstr(buf);
}

Str u8$__str__(uint8_t* self) {
  char buf[40];
  snprintf(buf, sizeof(buf), "%d", *self);
  return str_copy_cstr(buf);
}

Str i16$__str__(int16_t* self) {
  char buf[40];
  snprintf(buf, sizeof(buf), "%d", *self);
  return str_copy_cstr(buf);
}

Str u16$__str__(uint16_t* self) {
  char buf[40];
  snprintf(buf, sizeof(buf), "%u", *self);
  return str_copy_cstr(buf);
}

Str i32$__str__(int32_t* self) {
  char buf[40];
  snprintf(buf, sizeof(buf), "%d", *self);
  return str_copy_cstr(buf);
}

Str u32$__str__(uint32_t* self) {
  char buf[40];
  snprintf(buf, sizeof(buf), "%u", *self);
  return str_copy_cstr(buf);
}

Str i64$__str__(int64_t* self) {
  char buf[80];
  snprintf(buf, sizeof(buf), "%lld", *self);
  return str_copy_cstr(buf);
}

Str u64$__str__(uint64_t* self) {
  char buf[80];
  snprintf(buf, sizeof(buf), "%llu", *self);
  return str_copy_cstr(buf);
}

Str str$__str__(Str* self) {
  return *self;
}

Str bool$__str__(bool* self) {
  return *self ? str_const_cstr("true") : str_const_cstr("false");
}

Str codept$__str__(uint32_t* self) {
  char buf[80]; // TODO: utf8
  snprintf(buf, sizeof(buf), "%c", (char)*self);
  return str_copy_cstr(buf);
}

Str float$__str__(float* self) {
  char buf[256];
  snprintf(buf, sizeof(buf), "%f", *self);
  return str_copy_cstr(buf);
}

Str double$__str__(double* self) {
  char buf[256];
  snprintf(buf, sizeof(buf), "%f", *self);
  return str_copy_cstr(buf);
}

Str range$__str__(Range* range) {
  char buf[256];
  if (range->step == 1) {
    snprintf(buf, sizeof(buf), "range(%lld, %lld)", range->start, range->stop);
  } else {
    snprintf(buf, sizeof(buf), "range(%lld, %lld, %lld)", range->start, range->stop, range->step);
  }
  return str_copy_cstr(buf);
}

// subtype___str__ might be null if there's none defined.
Str Array$__str__(unsigned char* arr_base,
                  uint64_t arr_count,
                  uint64_t item_size,
                  Str (*subtype___str__)(void* item)) {
  List string_buffer = {0};
  append_to_string_buffer_list(&string_buffer, &(Str){"[", 1});
  for (size_t i = 0; i < arr_count; ++i) {
    if (!subtype___str__) {
      append_to_string_buffer_list(&string_buffer, &(Str){"???", 3});
    } else {
      Str tmp = subtype___str__(&arr_base[i * item_size]);
      append_to_string_buffer_list(&string_buffer, &tmp);
    }
    if (i < arr_count - 1) {
      append_to_string_buffer_list(&string_buffer, &(Str){", ", 2});
    }
  }
  append_to_string_buffer_list(&string_buffer, &(Str){"]", 1});

  return (Str){(const char*)string_buffer.data, string_buffer.size};
}

// subtype___str__ might be null if there's none defined.
Str List$__str__(List* list, uint64_t item_size, Str (*subtype___str__)(void* item)) {
  return Array$__str__(list->data, list->size, item_size, subtype___str__);
}

void List$unchecked_get(List* list, int64_t index, void* into, uint64_t item_size) {
  RT_CHECK(index >= 0 && "todo; negative index");
  memcpy(into, &list->data[index * item_size], item_size);
}

#if 0
// eq_func can be null, which then instead uses memcmp. If provided it must be
// the typed function, not the erased.
bool List$__contains__(List* list, void* item, uint64_t item_size, bool (*eq_func)(void*, void*)) {
}
#endif

#if 0
void List$__contains__()
void List$__getitem__()
void List$__iter__()
void List$__len__()
void List$__repr__()
void List$__reversed__()
void List$__setitem__()
void List$__str__()
void List$extend()
void List$insert()
void List$pop()
void List$sort()
#endif
