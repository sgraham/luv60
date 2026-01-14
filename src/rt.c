#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void CheckFailed(void) {
  fprintf(stderr, "check failed!\n");
  exit(127);
}

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

typedef struct List {
  unsigned char* data;
  uint64_t size;
  uint64_t capacity;
} List;

void List$reserve(List* list, uint64_t capacity, uint64_t item_size) {
  if (capacity <= list->capacity) {
    return;
  }
  while (list->capacity < capacity) {
    list->capacity = list->capacity > 0 ? list->capacity * 2 : 16;
  }
  list->data = realloc(list->data, list->capacity * item_size);
}

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

void List$unchecked_get(List* list, uint64_t index, void* into, uint64_t item_size) {
  memcpy(into, &list->data[index * item_size], item_size);
}

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
