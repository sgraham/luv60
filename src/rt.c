#include <stdint.h>

#if _WIN32
void* memcpy(void* dest, const void* src, unsigned long long count);
extern void* realloc(void* ptr, unsigned long long new_size);
#else
void* memcpy(void* dest, const void* src, unsigned long count);
extern void* realloc(void* ptr, unsigned long new_size);
#endif

typedef struct List {
  unsigned char* data;
  uint64_t count;
  uint64_t capacity;
} List;

void List_append(List* list, uint64_t item_size, void* item) {
  if (list->count >= list->capacity) {
    list->capacity = list->capacity > 0 ? list->capacity * 2 : 16;
    list->data = realloc(list->data, list->capacity * item_size);
  }
  memcpy(&list->data[list->count * item_size], item, item_size);
  list->count++;
}

void List_print(List* list, void (*subtype_repr)(void*)) {
}
