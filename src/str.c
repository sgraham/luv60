#include "luv60.h"

#include "dict.h"

#define STR_POOL_SIZE (16<<20)
char* str_intern_pool;
static int str_insert_location;
static DictImpl str_map;

void str_intern_pool_init(void) {
  str_intern_pool = (char*)base_large_alloc_rw(STR_POOL_SIZE);
  str_insert_location = 1;
  str_map = dict_new(128, sizeof(Str), _Alignof(Str));
}

static Str str_alloc_uninitialized_pool_bytes(uint32_t bytes) {
  uint32_t end = str_insert_location + bytes;
  ASSERT(end < STR_POOL_SIZE);
  str_insert_location += bytes;
  return (Str){end};
}

static size_t str_hash_func(void* ss) {
  Str s = *(Str*)ss;
  uint32_t len = str_len(s);
  size_t ret = 0;
  dict_hash_write(&ret, &len, sizeof(len));
  dict_hash_write(&ret, (void*)cstr(s), len);
  return ret;
}

static bool str_eq_func(void* aa, void* bb) {
  Str a = *(Str*)aa;
  Str b = *(Str*)bb;
  uint32_t alen = str_len(a);
  if (alen != str_len(b)) {
    return false;
  }
  return memcmp(cstr(a), cstr(b), alen) == 0;
}

Str str_intern_len(const char* ptr, uint32_t len) {
  // Because the dict only stores intern'd strings, copy the candidate to be
  // inserted into the insert location (with the length header). If we do need
  // to insert, it's already there. If we don't, subtract the allocated amount.
  uint32_t allocate_bytes = len + sizeof(uint32_t) + 1;
  Str str = str_alloc_uninitialized_pool_bytes(allocate_bytes);
  memcpy(&str_intern_pool[str.i - sizeof(uint32_t)], &len, sizeof(len));
  memcpy(&str_intern_pool[str.i], ptr, len);
  str_intern_pool[str.i + len] = 0;

  DictInsert res =
      dict_deferred_insert(&str_map, &str, str_hash_func, str_eq_func, sizeof(Str), _Alignof(Str));
  Str* dictstr = ((Str*)dict_rawiter_get(&res.iter));
  if (res.inserted) {
    dictstr->i = str.i;
    return str;
  } else {
    str_insert_location -= allocate_bytes; 
    return *dictstr;
  }
}
