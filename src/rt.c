#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>

#define COUNTOF(a) (sizeof(a)/sizeof(a[0]))
#define COUNTOFI(a) ((int)(sizeof(a)/sizeof(a[0])))
#define RT_CHECK(cond) if (!(cond)) { fprintf(stderr, "%s\n", #cond); CheckFailed(); }


#if defined(_WIN32)

void* memmem(const void* haystack, size_t haystacklen, const void* needle, size_t needlelen) {
  const unsigned char* h = haystack;
  const unsigned char* n = needle;

  // Edge cases
  if (needlelen == 0) {
    return (void*)haystack;  // Empty needle matches at start
  }

  if (needlelen > haystacklen) {
    return NULL;  // Needle can't fit in haystack
  }

  // Search through haystack
  size_t search_len = haystacklen - needlelen + 1;

  for (size_t i = 0; i < search_len; i++) {
    // Check if needle matches at current position
    size_t j;
    for (j = 0; j < needlelen; j++) {
      if (h[i + j] != n[j]) {
        break;
      }
    }

    // If we checked all bytes and they matched
    if (j == needlelen) {
      return (void*)(h + i);
    }
  }

  return NULL;  // Not found
}

#endif // _WIN32

void CheckFailed(void) {
  fprintf(stderr, "check failed!\n");
  exit(127);
}

static uint64_t base_timer_now(void) {
  return clock_gettime_nsec_np(CLOCK_UPTIME_RAW) / 1000;
}

#include "rt_zone.c"

// These are scratch zones for ^func.
static Zone* scratch_zones[1024];
static int scratch_zone_depth;

// This is the currently active zone, often one of the above, but also
// user-created Zones for longer lived allocations.
static Zone* zone_stack[1024];
static int zone_stack_pos;

void RtPreMain(void) {
  //uint64_t before = base_timer_now();

  for (int i = 0; i < COUNTOFI(scratch_zones); ++i) {
    scratch_zones[i] = zone_create();
  }
  scratch_zone_depth = 0;
  zone_stack_pos = 0;

  //uint64_t after = base_timer_now();
  //printf("%lld nanos\n", after - before);
}

void ZonePush(Zone* zone) {
  RT_CHECK(zone_stack_pos < COUNTOFI(zone_stack));
  printf("zone push %p\n", zone);
  zone_stack[zone_stack_pos++] = zone;
}

void ZonePop(void) {
  RT_CHECK(zone_stack_pos > 0);
  printf("zone pop %p\n", zone_stack[zone_stack_pos - 1]);
  zone_pop_to(zone_stack[zone_stack_pos - 1], 0);
  zone_stack_pos--;
}

void ZoneEnterFunction(void) {
  RT_CHECK(scratch_zone_depth < COUNTOFI(scratch_zones));
  Zone* z = scratch_zones[scratch_zone_depth++];
  zone_guard_read_write(z);
  ZonePush(z);
  printf("zone enter %d\n", scratch_zone_depth);
}

void ZoneExitFunction(void) {
  printf("zone exit %d\n", scratch_zone_depth);
  RT_CHECK(scratch_zone_depth > 0);
  RT_CHECK(zone_stack_pos > 0);
  RT_CHECK(scratch_zones[scratch_zone_depth - 1] == zone_stack[zone_stack_pos - 1]);
  scratch_zone_depth--;
  ZonePop();
  zone_guard_no_access(scratch_zones[scratch_zone_depth]);
}

Zone* ZoneTop(void) {
  return zone_stack[zone_stack_pos - 1];
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
  Str ret = {zone_push(ZoneTop(), size, 8), size};
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
    if (list->capacity == UINT64_MAX) {
      CheckFailed();
    }
    return;
  }
  while (list->capacity < capacity) {
    list->capacity = list->capacity > 0 ? list->capacity * 2 : 16;
  }

  unsigned char* newptr = zone_push(ZoneTop(), list->capacity * item_size, 8 /* todo */);
  memcpy(newptr, list->data, list->size * item_size);
  list->data = newptr;
}

void AppendToStringBufferList(List* sb, Str* str) {
  List$reserve(sb, sb->size + str->size, sizeof(unsigned char));
  memcpy(&sb->data[sb->size], str->data, str->size);
  sb->size += str->size;
}

bool str$__eq__(Str* self, Str* other) {
  if (self->size != other->size) {
    return false;
  }
  return memcmp(self->data, other->data, self->size) == 0;
}

Str str$join(Str* str, List* strings) {
  List string_buffer = {0};
  for (size_t i = 0; i < strings->size; ++i) {
    Str* item = (Str*)&strings->data[i * sizeof(Str)];
    AppendToStringBufferList(&string_buffer, item);
    if (i < strings->size - 1) {
      AppendToStringBufferList(&string_buffer, str);
    }
  }

  return (Str){(const char*)string_buffer.data, string_buffer.size};
}

bool str$__contains__(Str* self, Str other) {
  // TODO: probably unicode utf8 blah blah
  return memmem(self->data, self->size, other.data, other.size) != NULL;
}

#if 0
void List$free(List* list) {
  free(list->data);
  list->data = NULL;
  list->size = 0;
  list->capacity = 0;
}
#endif

uint64_t List$len(List* list) {
  return list->size;
}

void List$append(List* list, void* item, uint64_t item_size) {
  List$reserve(list, list->size + 1, item_size);
  memcpy(&list->data[list->size * item_size], item, item_size);
  list->size++;
}

List List$slice_from_array(void* arr_base, size_t arr_count, uint64_t item_size) {
  return (List){arr_base, arr_count, UINT64_MAX};
}

// TODO: this is like a list.extend() but that should really take an iterator,
// not a contiguous block like this.
void List$copy_from_array(List* list,
                          void* arr_base,
                          size_t arr_count,
                          uint64_t item_size) {
  List$reserve(list, list->size + arr_count, item_size);
  memcpy(&list->data[list->size * item_size], arr_base, arr_count * item_size);
  list->size += arr_count;
}

List List$slice_from_list(List* list, uint64_t item_size, int64_t start, int64_t end) {
  if (start < 0) {
    start = list->size + start;
  }
  if (start < 0) {
    start = 0;
  }
  if (start > list->size) {
    start = list->size;
  }

  if (end < 0) {
    end = list->size + end;
  }
  if (end < 0) {
    end = 0;
  }
  if (end > list->size) {
    end = list->size;
  }

  if (end < start) {
    end = start;
  }

  return (List){&list->data[start * item_size], end - start, UINT64_MAX};
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

// subtype___eq__ might be null if there's none defined.
bool Array$__contains__(unsigned char* arr_base,
                        uint64_t arr_count,
                        void* item,
                        uint64_t item_size,
                        bool (*subtype___eq__)(void* self, void* other)) {
  for (uint64_t i = 0; i < arr_count; ++i) {
    bool eq;
    void* a = &arr_base[i * item_size];
    if (!subtype___eq__) {
      eq = memcmp(a, item, item_size) == 0;
    } else {
      eq = subtype___eq__(a, item);
    }
    if (eq) {
      return true;
    }
  }

  return false;
}

bool List$__contains__(List* list,
                       void* item,
                       uint64_t item_size,
                       bool (*subtype___eq__)(void* self, void* other)) {
  return Array$__contains__(list->data, list->size, item, item_size, subtype___eq__);
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

bool range$__contains__(Range* range, int64_t value) {
  // Step cannot be zero
  if (range->step == 0) {
    return false;
  }

  // Check if value is outside the range bounds
  if (range->step > 0) {
    // Positive step: range goes from start (inclusive) to stop (exclusive)
    if (value < range->start || value >= range->stop) {
      return false;
    }
  } else {
    // Negative step: range goes from start (inclusive) down to stop (exclusive)
    if (value > range->start || value <= range->stop) {
      return false;
    }
  }

  // Check if value is on the correct stride
  // value = start + n * step, where n >= 0
  // So: (value - start) must be divisible by step
  int64_t diff = value - range->start;

  // If step divides diff evenly, and the quotient is non-negative, value is in range
  if (diff % range->step == 0) {
    return true;
  }

  return false;
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
  AppendToStringBufferList(&string_buffer, &(Str){"[", 1});
  for (size_t i = 0; i < arr_count; ++i) {
    if (!subtype___str__) {
      AppendToStringBufferList(&string_buffer, &(Str){"???", 3});
    } else {
      Str tmp = subtype___str__(&arr_base[i * item_size]);
      AppendToStringBufferList(&string_buffer, &tmp);
    }
    if (i < arr_count - 1) {
      AppendToStringBufferList(&string_buffer, &(Str){", ", 2});
    }
  }
  AppendToStringBufferList(&string_buffer, &(Str){"]", 1});

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
