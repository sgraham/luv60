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

void str_intern_pool_destroy_for_tests(void) {
  base_large_alloc_free(str_intern_pool);
  dict_destroy(&str_map);
}

static uint32_t str_alloc_uninitialized_pool_bytes(uint32_t bytes) {
  uint32_t start = str_insert_location;
  ASSERT(str_insert_location + bytes < STR_POOL_SIZE);
  str_insert_location += bytes;
  return start;
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
  uint32_t start = str_alloc_uninitialized_pool_bytes(allocate_bytes);
  memcpy(&str_intern_pool[start], &len, sizeof(len));
  memcpy(&str_intern_pool[start + sizeof(uint32_t)], ptr, len);
  str_intern_pool[start + sizeof(uint32_t) + len] = 0;
  Str str = {start + sizeof(uint32_t)};

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

Str str_intern(const char* ptr) {
  return str_intern_len(ptr, strlen(ptr));
}

static char hex_digits[] = {
    ['0'] = 0,  ['1'] = 1,  ['2'] = 2,  ['3'] = 3,  ['4'] = 4,  ['5'] = 5,  ['6'] = 6,  ['7'] = 7,
    ['8'] = 8,  ['9'] = 9,  ['a'] = 10, ['b'] = 11, ['c'] = 12, ['d'] = 13, ['e'] = 14, ['f'] = 15,
    ['A'] = 10, ['B'] = 11, ['C'] = 12, ['D'] = 13, ['E'] = 14, ['F'] = 15,
};

Str str_process_escapes(const char* ptr, uint32_t len) {
  uint32_t allocate_bytes = len + sizeof(uint32_t) + 1;
  uint32_t start = str_alloc_uninitialized_pool_bytes(allocate_bytes);
  char* into_start = &str_intern_pool[start + sizeof(uint32_t)];
  char* into = into_start;
  for (const char* e = ptr; e != ptr + len; ++e) {
    if (e[0] == '\\') {
      switch (e[1]) {
        case 'n': *into++ = '\n'; ++e; break;
        case 't': *into++ = '\t'; ++e; break;
        case 'r': *into++ = '\r'; ++e; break;
        case '\\': *into++ = '\\'; ++e; break;
        case '"': *into++ = '"'; ++e; break;
        case 'x':
          if (e + 3 >= ptr + len) {
            goto escape_error;
          }
          *into++ = (char)(hex_digits[(int)e[2]] * 16 + hex_digits[(int)e[3]]);
          e += 3;
          break;
        default:
escape_error:
          return (Str){0};
      }
    } else {
      *into++ = *e;
    }
  }
  uint32_t final_len = into - into_start;
  memcpy(&str_intern_pool[start], &final_len, sizeof(final_len));
  *into++ = 0;
  ASSERT(final_len <= allocate_bytes);
  str_insert_location -= allocate_bytes - (final_len + sizeof(uint32_t));
  return (Str){start + sizeof(uint32_t)};
}
