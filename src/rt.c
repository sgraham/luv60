#include "shared.h"

#include "rt_util_win.c"
#include "arena.c"
#include "base_win.c"
#include "base_mac.c"
#include "dict.h"

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

static Arena* backing_arena_;
static Arena* current_arena_;

void CheckFailed(void) {
  fprintf(stderr, "check failed!\n");
  exit(127);
}

void PrintStr(Str* str) {
  printf("%.*s\n", (int)str->size, str->data);
}

void RtPreMain(void) {
  backing_arena_ = arena_create(MiB(1024), KiB(64));
  current_arena_ = backing_arena_;
}

static Str str_copy_cstr(const char* cstr) {
  // Note, no NUL, not sure if this will be annoying in practice.
  size_t size = strlen(cstr);
  Str ret = {arena_push(current_arena_, size, 1), size};
  memcpy((void*)ret.data, cstr, size);
  return ret;
}

static Str str_const_cstr(const char* cstr) {
  size_t size = strlen(cstr);
  return (Str){cstr, size};
}

void List$reserve(List* list, uint64_t capacity, uint64_t item_size) {
  if (capacity <= list->capacity) {
    if (list->capacity == UINT64_MAX) {
      CheckFailed();
    }
    return;
  }

  uint64_t old_capacity = list->capacity;

  while (list->capacity < capacity) {
    list->capacity = list->capacity > 0 ? list->capacity * 2 : 16;
  }

  void* new_data = arena_push(current_arena_, list->capacity * item_size, /*align=*/8);
  if (list->data) {
    memcpy(new_data, list->data, old_capacity * item_size);
  }
  list->data = new_data;
}

void AppendToStringBufferList(List* sb, Str* str) {
  List$reserve(sb, sb->size + str->size, sizeof(unsigned char));
  memcpy(&sb->data[sb->size], str->data, str->size);
  sb->size += str->size;
}

bool i8$__eq__(int8_t* self, int8_t* other) {
  return *self == *other;
}

bool u8$__eq__(uint8_t* self, uint8_t* other) {
  return *self == *other;
}

bool i16$__eq__(int16_t* self, int16_t* other) {
  return *self == *other;
}

bool u16$__eq__(uint16_t* self, uint16_t* other) {
  return *self == *other;
}

bool i32$__eq__(int32_t* self, int32_t* other) {
  return *self == *other;
}

bool u32$__eq__(uint32_t* self, uint32_t* other) {
  return *self == *other;
}

bool i64$__eq__(int64_t* self, int64_t* other) {
  return *self == *other;
}

bool u64$__eq__(uint64_t* self, uint64_t* other) {
  return *self == *other;
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
void List$copy_from_array(List* list, void* arr_base, size_t arr_count, uint64_t item_size) {
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
  CHECK(index >= 0 && "todo; negative index");
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

void Dict$ensure_init(DictImpl* into) {
  if (into->arena) {
    return;
  }
  // TODO: slot_size and align are unused if capacity is 0. We need to init the
  // control bytes of the dict, and set the arena. Potentially arrange to make
  // zero-init correct? Pass arena to everything would work, but potentially be
  // confusing. For List too, should it be embedded? In theory list could
  // relocate to a diff arena, not sure if that's useful or confusing for
  // either.
  memset(into, 0, sizeof(*into));
  *into = dict_new(current_arena_, /*capacity=*/0, /*slot_size=*/0, /*slot_align=*/0);
}

void Dict$insert(DictImpl* self,
                 void* key,
                 size_t key_size,
                 void* value,
                 size_t value_size,
                 uint64_t (*hash_func)(void*),
                 bool (*eq_func)(void*, void*)) {
  Dict$ensure_init(self);

  ASSERT(hash_func);
  ASSERT(eq_func);

  char* packed_value = alloca(key_size + value_size);
  memcpy(packed_value, key, key_size);
  memcpy(&packed_value[key_size], value, value_size);
  DictInsert res =
      dict_insert(self, packed_value, hash_func, eq_func, key_size + value_size, /*slot_align=*/8);
  (void)res;
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

static uint64_t memhash(const void* data, size_t len) {
  uint64_t hash = 0;
  dict_hash_write(&hash, data, len);
  return hash;
}

uint64_t bool$__hash__(bool* self) {
  return memhash(self, sizeof(*self));
}

uint64_t codept$__hash__(uint32_t* self) {
  return memhash(self, sizeof(*self));
}

uint64_t i8$__hash__(int8_t* self) {
  return memhash(self, sizeof(*self));
}

uint64_t u8$__hash__(uint8_t* self) {
  return memhash(self, sizeof(*self));
}

uint64_t i16$__hash__(int16_t* self) {
  return memhash(self, sizeof(*self));
}

uint64_t u16$__hash__(uint16_t* self) {
  return memhash(self, sizeof(*self));
}

uint64_t i32$__hash__(int32_t* self) {
  return memhash(self, sizeof(*self));
}

uint64_t u32$__hash__(uint32_t* self) {
  return memhash(self, sizeof(*self));
}

uint64_t i64$__hash__(int64_t* self) {
  return memhash(self, sizeof(*self));
}

uint64_t u64$__hash__(uint64_t* self) {
  return memhash(self, sizeof(*self));
}

uint64_t str$__hash__(Str* self) {
  uint64_t hash = 0;
  dict_hash_write(&hash, self->data, self->size);
  return hash;
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

Str Dict$__str__(DictImpl* dict,
                 uint64_t key_size,
                 uint64_t value_size,
                 Str (*key___str__)(void* item),
                 Str (*value___str__)(void* item)) {
  Dict$ensure_init(dict);

  List string_buffer = {0};
  AppendToStringBufferList(&string_buffer, &(Str){"{", 1});

  uint64_t count = 0;
  DictRawIter iter = dict_iter(dict, key_size + value_size);
  char* item = dict_rawiter_get(&iter);
  while (item) {
    if (!key___str__) {
      AppendToStringBufferList(&string_buffer, &(Str){"???", 3});
    } else {
      Str tmp = key___str__(item);
      AppendToStringBufferList(&string_buffer, &tmp);
    }

    AppendToStringBufferList(&string_buffer, &(Str){": ", 2});

    if (!value___str__) {
      AppendToStringBufferList(&string_buffer, &(Str){"???", 3});
    } else {
      Str tmp = value___str__(&item[key_size]);
      AppendToStringBufferList(&string_buffer, &tmp);
    }

    AppendToStringBufferList(&string_buffer, &(Str){", ", 2});

    item = dict_rawiter_next(&iter, key_size + value_size);
    ++count;
  }

  if (count) {
    string_buffer.size -= 2;
  }
  AppendToStringBufferList(&string_buffer, &(Str){"}", 1});

  return (Str){(const char*)string_buffer.data, string_buffer.size};
}

void List$unchecked_get(List* list, int64_t index, void* into, uint64_t item_size) {
  CHECK(index >= 0 && "todo; negative index");
  memcpy(into, &list->data[index * item_size], item_size);
}
