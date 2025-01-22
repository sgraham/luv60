#include "luv60.h"

/* Initial string interning was a DictImpl holding a set of Str, and a large
 * buffer containing all the string data with a uint32_t header containing the
 * size and \0 after the string data for simple/fast cstr(). The Str object was
 * just a u32 containing the index into large buffer.
 *
 * Revised design more suited to compiler usage (maybe?). Str becomes a u64,
 * and:
 * - A zero-length string is disallowed.
 * - Strings of length 1..8 are their "values in little-endian", so e.g. the
 *   intern'd version of "x" is 120, the intern'd version of "def" is 0x666564,
 *   i.e. `('d') | ('e' << 8) | ('f' << 16)`, etc.
 * - Strings of length >= 9 appearing in the original parse buffer (TODO: or
 *   strings of length 8 that have 0x80 in the 8th byte!):
 *   0x8000_0000_0000_0000 | (index & 0x3fffffff) << 32) | (len & 0x3fffffff)
 *   where index is the offset into the original parse buffer, and len is the
 *   strlen. (TODO: These are less often tested for equality, so maybe make
 *   str_eq actually just strncmp? Otherwise do a dict, etc.)
 * - "New" strings:
 *   0xc000_0000_0000_0000 | (index & 0x3fffffff) << 32) | (len & 0x3fffffff)
 *   Are index+len into an aux buffer.
 *
 * The main upside of this representation is that we know the "intern"d value of
 * many identifiers of common length without doing a dict lookup, including all
 * keywords.
 * Drawbacks:
 * - larger representation (64 instead of 32)
 * - some complexity in representing longer strings, esp. "new" ones. (TODO:
 *   Possibly just don't allow new strings for now?)
 */

#define TOP_BIT_U64 (0x8000000000000000ull)
#define SECOND_TOP_BIT_U64 (0x4000000000000000ull)
static char* parse_buffer_;
static size_t parse_buffer_size_;

#define STR_NEW_BUFFER_SIZE (MiB(1))
static char* new_buffer_;
static size_t new_buffer_size_;
static int new_buffer_insert_location_;

static Arena* arena_;

void str_intern_pool_init(Arena* arena, char* parse_buffer, size_t buffer_size) {
  arena_ = arena;
  new_buffer_ = arena_push(arena, STR_NEW_BUFFER_SIZE, 8);
  new_buffer_size_ = STR_NEW_BUFFER_SIZE;
  new_buffer_insert_location_ = 1;
  parse_buffer_ = parse_buffer;
  parse_buffer_size_ = buffer_size;
}

void str_intern_pool_destroy_for_tests(void) {
}

#define MAKE_SHORT_STR_VAL(a, b, c, d, e, f, g, h)          \
  (((uint64_t)(a) << 0ULL) |  /* lowest, first in string */ \
   ((uint64_t)(b) << 8ULL) |  /* */                         \
   ((uint64_t)(c) << 16ULL) | /* */                         \
   ((uint64_t)(d) << 24ULL) | /* */                         \
   ((uint64_t)(e) << 32ULL) | /* */                         \
   ((uint64_t)(f) << 40ULL) | /* */                         \
   ((uint64_t)(g) << 48ULL) | /* */                         \
   ((uint64_t)(h) << 56ULL) /* highest, byte 8 */)

#define MAKE_PARSE_BUFFER_STR_VAL(index, len)               \
  (TOP_BIT_U64 |                          /* top bit */     \
   (((index) & 0x3fffffffull) << 32ull) | /* 30bit index */ \
   (((len) & 0x3fffffffull)) /* 30bit len */)

#define MAKE_NEW_BUFFER_STR_VAL(index, len)                    \
  (TOP_BIT_U64 |                          /* top bit */        \
   SECOND_TOP_BIT_U64 |                   /* second top bit */ \
   (((index) & 0x3fffffffull) << 32ull) | /* 30bit index */    \
   (((len) & 0x3fffffffull)) /* 30bit len */)

Str str_intern_len(const char* p, uint32_t len) {
  switch (len) {
    case 0:
      ASSERT(false && "zero length string not allowed");
      TRAP();
    case 1:
      return (Str){MAKE_SHORT_STR_VAL(p[0], 0, 0, 0, 0, 0, 0, 0)};
    case 2:
      return (Str){MAKE_SHORT_STR_VAL(p[0], p[1], 0, 0, 0, 0, 0, 0)};
    case 3:
      return (Str){MAKE_SHORT_STR_VAL(p[0], p[1], p[2], 0, 0, 0, 0, 0)};
    case 4:
      return (Str){MAKE_SHORT_STR_VAL(p[0], p[1], p[2], p[3], 0, 0, 0, 0)};
    case 5:
      return (Str){MAKE_SHORT_STR_VAL(p[0], p[1], p[2], p[3], p[4], 0, 0, 0)};
    case 6:
      return (Str){MAKE_SHORT_STR_VAL(p[0], p[1], p[2], p[3], p[4], p[5], 0, 0)};
    case 7:
      return (Str){MAKE_SHORT_STR_VAL(p[0], p[1], p[2], p[3], p[4], p[5], p[6], 0)};
    case 8:
      // TODO: bail if 0x80 of p[7] is set
      return (Str){MAKE_SHORT_STR_VAL(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7])};
    default:
      if (p >= parse_buffer_ && p < parse_buffer_ + parse_buffer_size_) {
        ASSERT(len <= 0x3fffffff);
        return (Str){MAKE_PARSE_BUFFER_STR_VAL(p - parse_buffer_, len)};
      }

      uint32_t new_loc = new_buffer_insert_location_;
      new_buffer_insert_location_ += len;
      ASSERT(new_buffer_insert_location_ < new_buffer_size_);
      char* strp = &new_buffer_[new_loc];
      memcpy(strp, p, len);
      return (Str){MAKE_NEW_BUFFER_STR_VAL(new_loc, len)};
  }
}

uint32_t str_len(Str str) {
  if (str.i & TOP_BIT_U64) {
    return (uint32_t)(str.i & 0x3fffffff);
  } else if (str.i <= 0xffull) {
    return 1;
  } else if (str.i <= 0xffffull) {
    return 2;
  } else if (str.i <= 0xffffffull) {
    return 3;
  } else if (str.i <= 0xffffffffull) {
    return 4;
  } else if (str.i <= 0xffffffffffull) {
    return 5;
  } else if (str.i <= 0xffffffffffffull) {
    return 6;
  } else if (str.i <= 0xffffffffffffffull) {
    return 7;
  } else {
    return 8;
  }
}

const char* str_raw_ptr_impl_long_string(Str str) {
  ASSERT(str.i & TOP_BIT_U64);
  if (str.i & SECOND_TOP_BIT_U64) {
    return &new_buffer_[(str.i >> 32ull) & 0x3fffffff];
  } else {
    return &parse_buffer_[(str.i >> 32ull) & 0x3fffffff];
  }
}

bool str_eq_impl_long_strings(Str a, Str b) {
  ASSERT((a.i & TOP_BIT_U64) || (b.i & TOP_BIT_U64)); // One must be a long string here.
  if ((a.i >> 63) ^ (b.i >> 63)) {
    // If one is, and one isn't then they don't match.
    return false;
  } else {
    uint32_t alen = a.i & 0x3fffffff;
    uint32_t blen = b.i & 0x3fffffff;
    if (alen != blen) return false;
    const char* ap = str_raw_ptr(a);
    const char* bp = str_raw_ptr(b);
    // TODO: not actually interning long strings, check to see whether
    // interning them actually performs better or not.
    return strncmp(ap, bp, alen) == 0;
  }
}

Str str_intern(const char* ptr) {
  return str_intern_len(ptr, strlen(ptr));
}

Str str_internf(const char* fmt, ...) {
  // TODO: not intern'd at all, just dup/leaked into the arena!
  va_list args;
  va_start(args, fmt);
  uint32_t len = vsnprintf(NULL, 0, fmt, args);
  va_end(args);

  uint64_t saved_pos = arena_pos(arena_);
  char* strp = arena_push(arena_, len + 1, 1);
  va_start(args, fmt);
  vsnprintf(strp, len + 1, fmt, args);
  va_end(args);

  Str ret = str_intern_len(strp, len);
  arena_pop_to(arena_, saved_pos);

  return ret;
}

static char hex_digits[] = {
    ['0'] = 0,  ['1'] = 1,  ['2'] = 2,  ['3'] = 3,  ['4'] = 4,  ['5'] = 5,  ['6'] = 6,  ['7'] = 7,
    ['8'] = 8,  ['9'] = 9,  ['a'] = 10, ['b'] = 11, ['c'] = 12, ['d'] = 13, ['e'] = 14, ['f'] = 15,
    ['A'] = 10, ['B'] = 11, ['C'] = 12, ['D'] = 13, ['E'] = 14, ['F'] = 15,
};

// Modifies data in place!
uint32_t str_process_escapes(char* ptr, uint32_t len) {
  char* write = ptr;
  uint32_t new_len = len;
  for (const char* read = ptr; read != ptr + len; ++read) {
    if (read[0] == '\\') {
      switch (read[1]) {
        case 'n': *write++ = '\n'; ++read; --new_len; break;
        case 't': *write++ = '\t'; ++read; --new_len; break;
        case 'r': *write++ = '\r'; ++read; --new_len; break;
        case '\\': *write++ = '\\'; ++read; --new_len; break;
        case '"': *write++ = '"'; ++read; --new_len; break;
        case 'x':
          if (read + 3 >= ptr + len) {
            goto escape_error;
          }
          *write++ = (char)(hex_digits[(int)read[2]] * 16 + hex_digits[(int)read[3]]);
          read += 3;
          new_len -= 3;
          break;
        default:
escape_error:
          return 0;
      }
    } else {
      *write++ = *read;
    }
  }
  return new_len;
}
