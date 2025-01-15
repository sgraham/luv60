// SwissTable-like implementation for dicts. See
//   https://abseil.io/about/design/swisstables
//   https://github.com/abseil/abseil-cpp/blob/master/absl/container/internal/raw_hash_set.h
//   https://www.youtube.com/watch?v=ncHmEUmJZf4
// for the gory and interesting details.
//
// Other non-C++ implementations, better and more flexible than this one:
//   https://github.com/google/cwisstable
//   https://github.com/rust-lang/hashbrown
//
// This implementation is basically a port of a combination of the original C++
// code and cwisstable, starting with the logic and layout, and down to the
// function names and asserts and even comments. It's adapted for: 1) C instead
// of C++, and 2) integrating with the type information and policy information
// we have available, and (less so) 3) some small tweaks to work with tcc.
//
// For |capacity| slots, the backing array looks like:
//
// DictControlByte ctrl[capacity];
// DictControlByte sentinel;                     // these two are used to simplify
// DictControlByte clones[DictGroupWidth - 1];  // unaligned lookups
// char padding[N];  // to get to alignof(T)
// char slots[capacity * sizeof(T)];
//
// DictControlByte has 4 major states that determine the state of the slot, and
// help walk the probe chains more efficiently.
//
//    empty: 0b10000000
//  deleted: 0b11111110
//   in-use: 0b0hhhhhhh  // |h| is the bottom 7 bits of T's hash
// sentinel: 0b11111111
typedef int8_t DictControlByte;
#define DCB_Empty (INT8_C(-128))
#define DCB_Deleted (INT8_C(-2))
#define DCB_Sentinel (INT8_C(-1))

#if ARCH_X64
#  define HAVE_SSE2 1
#endif

// If we have SSE2, groups can be 16 wide, otherwise 8 byte bit-twiddling on
// uint64_t. TODO: I'm sure there's something that could be better on arm64.
#if HAVE_SSE2

#  include <emmintrin.h>

typedef __m128i DictGroup;
#  define DictGroupWidth ((size_t)16)
#  define DictGroupShift 0

#else

typedef uint64_t DictGroup;
#  define DictGroupWidth ((size_t)8)
#  define DictGroupShift 3

#endif

// A valid capacity is a non-zero integer 2^m - 1.
static inline bool dict_is_valid_capacity(size_t n) {
  return ((n + 1) & n) == 0 && n > 0;
}

static inline uint32_t dict_trailing_zeros64(uint64_t x) {
#if __clang__ || __TINYC__
  return __builtin_ctzll(x);
#elif _MSC_VER
#  error todo
#else
  uint32_t c = 63;
  x &= ~x + 1;
  // clang-format off
  if (x & 0x00000000FFFFFFFF) c -= 32;
  if (x & 0x0000FFFF0000FFFF) c -= 16;
  if (x & 0x00FF00FF00FF00FF) c -= 8;
  if (x & 0x0F0F0F0F0F0F0F0F) c -= 4;
  if (x & 0x3333333333333333) c -= 2;
  if (x & 0x5555555555555555) c -= 1;
  // clang-format on
  return c;
#endif
}

static inline uint32_t dict_leading_zeros64(uint64_t x) {
#if __clang__ || __TINYC__
  return x == 0 ? 64 : __builtin_clzll(x);
#elif _MSC_VER
#  error todo
#endif
}

#define dict_leading_zeros(x) \
  (dict_leading_zeros64(x) - (uint32_t)((sizeof(unsigned long long) - sizeof(x)) * 8))

#define dict_trailing_zeros(x) (dict_trailing_zeros64(x))

// A abstract bitmask, such as that emitted by a SIMD instruction.
//
// Specifically, this type implements a simple bitset whose representation is
// controlled by |width| and |shift|. |width| is the number of abstract bits in
// the bitset, while |shift| is the log-base-two of the width of an abstract
// bit in the representation.
//
// For example, when |width| is 16 and |shift| is zero, this is just an
// ordinary 16-bit bitset occupying the low 16 bits of |mask|. When |width| is
// 8 and |shift| is 3, abstract bits are represented as the bytes |0x00| and
// |0x80|, and it occupies all 64 bits of the bitmask.
typedef struct DictBitMask {
  // The mask, in the representation specified by |width| and |shift|.
  uint64_t mask;
  // The number of abstract bits in the mask.
  uint32_t width;
  // The log-base-two width of an abstract bit.
  uint32_t shift;
} DictBitMask;

#define dict_group_bitmask(x)                     \
  (DictBitMask) {                                 \
    (uint64_t)(x), DictGroupWidth, DictGroupShift \
  }

// Return the number of trailing zero abstract bits.
static inline uint32_t dict_bitmask_trailing_zeros(DictBitMask* self) {
  return dict_trailing_zeros(self->mask) >> self->shift;
}

static inline uint32_t dict_bitmask_leading_zeros(DictBitMask* self) {
  uint32_t total_significant_bits = self->width << self->shift;
  uint32_t extra_bits = sizeof(self->mask) * 8 - total_significant_bits;
  return (uint32_t)(dict_leading_zeros(self->mask << extra_bits)) >> self->shift;
}

static inline uint32_t dict_bitmask_lowest_bit_set(const DictBitMask* self) {
  return dict_trailing_zeros(self->mask) >> self->shift;
}

static inline bool dict_bitmask_next(DictBitMask* self, uint32_t* bit) {
  if (self->mask == 0) {
    return false;
  }

  *bit = dict_bitmask_lowest_bit_set(self);
  self->mask &= (self->mask - 1);
  return true;
}

#define dict_rotate_left(x, bits) (((x) << bits) | ((x) >> (sizeof(x) * 8 - bits)))

_Static_assert(sizeof(size_t) == 8, "64 bit hash required");

// https://github.com/cbreeden/fxhash
static inline void dict_hash_write(size_t* state, void* val, size_t len) {
  const size_t seed = (size_t)(UINT64_C(0x517cc1b727220a95));
  const uint32_t rotate = 5;

  const char* p = (const char*)val;
  size_t st = *state;
  while (len > 0) {
    size_t word = 0;
    size_t to_read = len >= sizeof(st) ? sizeof(st) : len;
    memcpy(&word, p, to_read);

    st = dict_rotate_left(st, rotate);
    st ^= word;
    st *= seed;

    len -= to_read;
    p += to_read;
  }
  *state = st;
}

#if HAVE_SSE2

static inline DictGroup dict_group_new(DictControlByte* pos) {
  return _mm_loadu_si128((DictGroup*)pos);
}

// Returns a bitmask representing the positions of slots that match h2_hash.
static inline DictBitMask dict_group_match(DictGroup* self, uint8_t h2_hash) {
  return dict_group_bitmask(_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_set1_epi8(h2_hash), *self)));
}

// Returns a bitmask representing the positions of empty slots.
static inline DictBitMask dict_group_match_empty(DictGroup* self) {
  // TODO: SSSE3 version
  return dict_group_match(self, DCB_Empty);
}

// Returns a bitmask representing the positions of empty or deleted slots.
static inline DictBitMask dict_group_match_empty_or_deleted(DictGroup* self) {
  DictGroup special = _mm_set1_epi8((uint8_t)DCB_Sentinel);
  return dict_group_bitmask(_mm_movemask_epi8(_mm_cmpgt_epi8(special, *self)));
}

static inline uint32_t dict_group_count_leading_empty_or_deleted(DictGroup* self) {
  DictGroup special = _mm_set1_epi8((uint8_t)DCB_Sentinel);
  return dict_trailing_zeros((uint32_t)(_mm_movemask_epi8(_mm_cmpgt_epi8(special, *self)) + 1));
}

static inline void dict_group_convert_special_to_empty_and_full_to_deleted(DictGroup* self,
                                                                           DictControlByte* dst) {
  DictGroup msbs = _mm_set1_epi8((char)-128);
  DictGroup x126 = _mm_set1_epi8(126);
  // TODO: SSSE3 version
  DictGroup zero = _mm_setzero_si128();
  DictGroup special_mask = _mm_cmpgt_epi8(zero, *self);
  DictGroup res = _mm_or_si128(msbs, _mm_andnot_si128(special_mask, x126));
  _mm_storeu_si128((DictGroup*)dst, res);
}

#else

static inline DictGroup dict_group_new(DictControlByte* pos) {
  DictGroup val;
  memcpy(&val, pos, sizeof(val));
  return val;
}

static inline DictBitMask dict_group_match(DictGroup* self, uint8_t h2_hash) {
  // For the technique, see:
  // http://graphics.stanford.edu/~seander/bithacks.html##ValueInWord
  // (Determine if a word has a byte equal to n).
  //
  // Caveat: there are false positives but:
  // - they only occur if there is a real match
  // - they never occur on Empty, Deleted, Sentinel
  // - they will be handled gracefully by subsequent checks in code
  //
  // Example:
  //   v = 0x1716151413121110
  //   hash = 0x12
  //   retval = (v - lsbs) & ~v & msbs = 0x0000000080800000
  uint64_t msbs = 0x8080808080808080ULL;
  uint64_t lsbs = 0x0101010101010101ULL;
  uint64_t x = *self ^ (lsbs * h2_hash);
  return dict_group_bitmask((x - lsbs) & ~x & msbs);
}

static inline DictBitMask dict_group_match_empty(DictGroup* self) {
  uint64_t msbs = 0x8080808080808080ULL;
  return dict_group_bitmask((*self & (~*self << 6)) & msbs);
}

static inline DictBitMask dict_group_match_empty_or_deleted(DictGroup* self) {
  uint64_t msbs = 0x8080808080808080ULL;
  return dict_group_bitmask((*self & (~*self << 7)) & msbs);
}

static inline uint32_t dict_group_count_leading_empty_or_deleted(DictGroup* self) {
  uint64_t gaps = 0x00FEFEFEFEFEFEFEULL;
  return (dict_trailing_zeros(((~*self & (*self >> 7)) | gaps) + 1) + 7) >> 3;
}

static inline void dict_group_convert_special_to_empty_and_full_to_deleted(DictGroup* self,
                                                                           DictControlByte* dst) {
  uint64_t msbs = 0x8080808080808080ULL;
  uint64_t lsbs = 0x0101010101010101ULL;
  uint64_t x = *self & msbs;
  uint64_t res = (~x + (x >> 7)) & ~lsbs;
  memcpy(dst, &res, sizeof(res));
}

#endif  // SSE2

#define dict_num_cloned_bytes() (DictGroupWidth - 1)

// Applies the following mapping to every byte in the control array:
//   * Deleted -> Empty
//   * Empty -> Empty
//   * _ -> Deleted
//
// Preconditions: `dict_is_valid_capacity(capacity)`,
// `ctrl[capacity]` == `Sentinel`, `ctrl[i] != Sentinel for i < capacity`.
static void dict_convert_deleted_to_empty_and_full_to_deleted(DictControlByte* ctrl,
                                                              size_t capacity) {
  ASSERT(ctrl[capacity] == DCB_Sentinel);  // bad ctrl value
  ASSERT(dict_is_valid_capacity(capacity));

  for (DictControlByte* pos = ctrl; pos < ctrl + capacity; pos += DictGroupWidth) {
    DictGroup g = dict_group_new(pos);
    dict_group_convert_special_to_empty_and_full_to_deleted(&g, pos);
  }
  memcpy(ctrl + capacity + 1, ctrl, dict_num_cloned_bytes());
  ctrl[capacity] = DCB_Sentinel;
}

// Sets |ctrl| to {Empty, ..., Empty, Sentinel}, marking it all as deleted.
static inline void dict_reset_ctrl(size_t capacity,
                                   DictControlByte* ctrl,
                                   void* slots,
                                   size_t slot_size) {
  memset(ctrl, DCB_Empty, capacity + 1 + dict_num_cloned_bytes());
  ctrl[capacity] = DCB_Sentinel;
}

static inline void dict_set_ctrl(size_t i,
                                 DictControlByte h,
                                 size_t capacity,
                                 DictControlByte* ctrl,
                                 void* slots,
                                 size_t slot_size) {
  ASSERT(i < capacity);

  // This is intentionally branchless. If |i < Width|, it will write to the
  // cloned bytes as well as the "real" byte; otherwise, it will store |h|
  // twice.
  size_t mirrored_i =
      ((i - dict_num_cloned_bytes()) & capacity) + (dict_num_cloned_bytes() & capacity);
  ctrl[i] = h;
  ctrl[mirrored_i] = h;
}

_Static_assert((DCB_Empty & DCB_Deleted & DCB_Sentinel & 0x80) != 0,
               "Special markers need to have the MSB to make checking for them efficient");

_Static_assert(DCB_Empty < DCB_Sentinel && DCB_Deleted < DCB_Sentinel,
               "DCB_Empty and DCB_Deleted must be smaller than "
               "DCB_Sentinel to make the SIMD test of is_empty_or_deleted() efficient");

_Static_assert(DCB_Sentinel == -1,
               "DCB_Sentinel must be -1 to elide loading it from memory into SIMD "
               "registers (pcmpeqd xmm, xmm)");

_Static_assert(DCB_Empty == -128,
               "DCB_Empty must be -128 to make the SIMD check for its "
               "existence efficient (psignb xmm, xmm)");

_Static_assert((~(DCB_Empty) & ~(DCB_Deleted) & (DCB_Sentinel) & 0x7F) != 0,
               "DCB_Empty and DCB_Deleted must share an unset bit that is not "
               "shared by DCB_Sentinel to make the scalar test for "
               "mask_empty_or_deleted() efficient");

_Static_assert(DCB_Deleted == (-2),
               "DCB_Deleted must be -2 to make the implementation of "
               "convert_special_to_empty_and_in_use_to_deleted efficient");

static inline DictControlByte* dict_empty_group() {
  _Alignas(16) static const DictControlByte ret[16] = {
      DCB_Sentinel, DCB_Empty, DCB_Empty, DCB_Empty, DCB_Empty, DCB_Empty, DCB_Empty, DCB_Empty,
      DCB_Empty,    DCB_Empty, DCB_Empty, DCB_Empty, DCB_Empty, DCB_Empty, DCB_Empty, DCB_Empty,
  };
  return (DictControlByte*)ret;
}

// Returns a hash seed.
//
// The seed consists of the ctrl pointer, which adds enough entropy to ensure
// non-determinism of iteration order in most cases.
static inline size_t dict_hash_seed(DictControlByte* ctrl) {
  // The low bits of the pointer have little or no entropy because of
  // alignment. We shift the pointer to try to use higher entropy bits. A
  // good number seems to be 12 bits, because that aligns with page size.
  return ((uintptr_t)ctrl) >> 12;
}

// Extracts the H1 portion of a hash: the high 57 bits mixed with a per-table
// salt.
static inline size_t dict_h1(size_t hash, DictControlByte* ctrl) {
  return (hash >> 7) ^ dict_hash_seed(ctrl);
}

// Extracts the H2 portion of a hash: the low 7 bits, which can be used as
// control byte.
static inline uint8_t dict_h2(size_t hash) {
  return hash & 0x7F;
}

static inline bool dict_is_empty(DictControlByte c) {
  return c == DCB_Empty;
}

static inline bool dict_is_in_use(DictControlByte c) {
  return c >= 0;
}

static inline bool dict_is_deleted(DictControlByte c) {
  return c == DCB_Deleted;
}

static inline bool dict_is_empty_or_deleted(DictControlByte c) {
  return c < DCB_Sentinel;
}

#define dict_assert_is_valid(ctrl) ASSERT(ctrl == NULL || dict_is_in_use(*(ctrl)))
#define dict_assert_is_in_use(ctrl) ASSERT(ctrl != NULL && dict_is_in_use(*(ctrl)))

typedef size_t (*DictKeyHashFunc)(void*);
typedef bool (*DictKeyEqFunc)(void*, void*);

static inline size_t dict_normalize_capacity(size_t n) {
  return n ? SIZE_MAX >> dict_leading_zeros(n) : 1;
}

// General notes on capacity/growth methods below:
// - We use 7/8th as maximum load factor. For 16-wide groups, that gives an
//   average of two empty slots per group.
// - For (capacity+1) >= DictGroupWidth, growth is 7/8*capacity.
// - For (capacity+1) < DictGroupWidth, growth == capacity. In this case, we
//   never need to probe (the whole table fits in one group) so we don't need a
//   load factor less than 1.

// Given |capacity|, applies the load factor. Returns the maximum number of
// values we should put into the table before a rehash.
static inline size_t dict_capacity_to_growth(size_t capacity) {
  ASSERT(dict_is_valid_capacity(capacity));
  if (DictGroupWidth == 8 && capacity == 7) {
    // x-x/8 does not work when x==7.
    return 6;
  }
  return capacity - capacity / 8;
}

// Given growth, unapplies the load factor to find out how large the capacity
// should be to stay within the load factor.
static inline size_t dict_growth_to_lower_bound_capacity(size_t growth) {
  if (DictGroupWidth == 8 && growth == 7) {
    // x+(x-1)/7 does not work when x==7.
    return 8;
  }
  return growth + (size_t)((((int64_t)growth) - 1) / 7);
}

// The memory block has |capacity + 1 + num_cloned_bytes()| control bytes
// followed by |capacity| slots which are aligned to |slot_align|. This gets the
// offset of where the slots begin in the block.
static inline size_t dict_slot_offset(size_t capacity, size_t slot_align) {
  ASSERT(dict_is_valid_capacity(capacity));
  size_t num_control_bytes = capacity + 1 + dict_num_cloned_bytes();
  return (num_control_bytes + slot_align - 1) & (~slot_align + 1);
}

// Total size of the backing memory block.
static inline size_t dict_alloc_size(size_t capacity, size_t slot_size, size_t slot_align) {
  return dict_slot_offset(capacity, slot_align) + capacity * slot_size;
}

typedef struct DictImpl {
  DictControlByte* ctrl;  // base of backing array
  char* slots;            // pointer into backing array to the actual slots
  size_t size;            // number of in-use slots
  size_t capacity;        // total number of slots
  size_t growth_left;     // how many more slots can be filled before rehash
                          // See dict_capacity_to_growth().
} DictImpl;

// An iterator into a SwissTable.
//
// dict_rawiter_get() will be null when complete.
//
// Invariants:
// - |ctrl| and |slot| are always in sync (i.e., the pointed to control byte
//   corresponds to the pointed to slot), or both are null. |dict| may be null
//   in the latter case.
// - |ctrl| always points to a full slot.
typedef struct DictRawIter {
  DictImpl* dict;
  DictControlByte* ctrl;
  char* slot;
} DictRawIter;

// Fixes up ctrl to point to the next in-use, but advancing ctrl and slot until
// they reach one. If a sentinel is hit, null out both.
static inline void dict_rawiter_skip_empty_or_deleted(DictRawIter* self, size_t slot_size) {
  while (dict_is_empty_or_deleted(*self->ctrl)) {
    DictGroup g = dict_group_new(self->ctrl);
    uint32_t shift = dict_group_count_leading_empty_or_deleted(&g);
    self->ctrl += shift;
    self->slot += shift * slot_size;
  }

  if (BRANCH_UNLIKELY(*self->ctrl == DCB_Sentinel)) {
    self->ctrl = NULL;
    self->slot = NULL;
  }
}

// Creates a valid iterator starting at the |index|th slot.
static inline DictRawIter dict_iter_at(DictImpl* self, size_t index, size_t slot_size) {
  DictRawIter iter = {self, self->ctrl + index, self->slots + index * slot_size};
  dict_rawiter_skip_empty_or_deleted(&iter, slot_size);
  dict_assert_is_valid(iter.ctrl);
  return iter;
}

static inline void* dict_rawiter_get(DictRawIter* self) {
  dict_assert_is_valid(self->ctrl);
  if (self->slot == NULL) {
    return NULL;
  }
  return self->slot;
}

// Advances the iterator and returns the result of dict_rawiter_get().
static inline void* dict_rawiter_next(DictRawIter* self, size_t slot_size) {
  dict_assert_is_in_use(self->ctrl);
  ++self->ctrl;
  self->slot += slot_size;

  dict_rawiter_skip_empty_or_deleted(self, slot_size);
  return dict_rawiter_get(self);
}

static inline void dict_reset_growth_left(DictImpl* self) {
  self->growth_left = dict_capacity_to_growth(self->capacity) - self->size;
}

static inline void dict_initialize_slots(DictImpl* self, size_t slot_size, size_t slot_align) {
  ASSERT(self->capacity > 0);
  ASSERT(slot_align <= 64);  // This is what base_large_alloc_rw ensures.
  void* mem = base_large_alloc_rw(dict_alloc_size(self->capacity, slot_size, slot_align));
  ASSERT(mem);
  self->ctrl = mem;
  self->slots = mem + dict_slot_offset(self->capacity, slot_align);
  dict_reset_ctrl(self->capacity, self->ctrl, self->slots, slot_size);
  dict_reset_growth_left(self);
}

static inline DictImpl dict_new(size_t capacity, size_t slot_size, size_t slot_align) {
  DictImpl ret = {.ctrl = dict_empty_group()};
  if (capacity != 0) {
    ret.capacity = dict_normalize_capacity(capacity);
    dict_initialize_slots(&ret, slot_size, slot_align);
  }
  return ret;
}

static inline void dict_destroy(DictImpl* self) {
  if (self->capacity == 0) {
    return;
  }
  base_large_alloc_free(self->ctrl);
  *self = (DictImpl){0};
}

typedef struct DictProbeSeq {
  size_t mask;
  size_t offset;
  size_t index;
} DictProbeSeq;

// Creates a new probe sequence using |hash| as the initial value of the
// sequence and |mask| (usually the capacity of the table) as the mask to apply
// to each value in the progression.
static inline DictProbeSeq dict_probeseq_new(size_t hash, size_t mask) {
  return (DictProbeSeq){.mask = mask, .offset = hash & mask};
}

// Returns the slot |i| indices ahead of |self| within the bounds expressed by
// |mask|.
static inline size_t dict_probeseq_offset(DictProbeSeq* self, size_t i) {
  return (self->offset + i) & self->mask;
}

static inline void dict_probeseq_next(DictProbeSeq* self) {
  self->index += DictGroupWidth;
  self->offset += self->index;
  self->offset &= self->mask;
}

static inline DictProbeSeq dict_probeseq_start(DictControlByte* ctrl,
                                               size_t hash,
                                               size_t capacity) {
  return dict_probeseq_new(dict_h1(hash, ctrl), capacity);
}

typedef struct DictFindInfo {
  size_t offset;
  size_t probe_length;
} DictFindInfo;

static inline DictFindInfo dict_find_first_non_full(DictControlByte* ctrl,
                                                    size_t hash,
                                                    size_t capacity) {
  DictProbeSeq seq = dict_probeseq_start(ctrl, hash, capacity);
  while (true) {
    DictGroup g = dict_group_new(ctrl + seq.offset);
    DictBitMask mask = dict_group_match_empty_or_deleted(&g);
    if (mask.mask) {
      return (DictFindInfo){dict_probeseq_offset(&seq, dict_bitmask_trailing_zeros(&mask)),
                            seq.index};
    }
    dict_probeseq_next(&seq);
    ASSERT(seq.index <= capacity);  // table should not be full.
  }
}

static inline void dict_resize(DictImpl* self,
                               size_t new_capacity,
                               DictKeyHashFunc hash_func,
                               size_t slot_size,
                               size_t slot_align) {
  ASSERT(dict_is_valid_capacity(new_capacity));

  DictControlByte* old_ctrl = self->ctrl;
  char* old_slots = self->slots;
  size_t old_capacity = self->capacity;
  self->capacity = new_capacity;
  dict_initialize_slots(self, slot_size, slot_align);

  // size_t total_probe_length = 0;
  for (size_t i = 0; i != old_capacity; ++i) {
    if (dict_is_in_use(old_ctrl[i])) {
      size_t hash = hash_func(old_slots + i * slot_size);
      DictFindInfo target = dict_find_first_non_full(self->ctrl, hash, self->capacity);
      size_t new_i = target.offset;
      // total_probe_length += target.probe_length;
      dict_set_ctrl(new_i, dict_h2(hash), self->capacity, self->ctrl, self->slots, slot_size);
      memcpy(self->slots + new_i * slot_size, old_slots + i * slot_size, slot_size);
    }
  }

  if (old_capacity) {
    base_large_alloc_free(old_ctrl);
  }
}

static inline DictRawIter dict_find(DictImpl* self,
                                    void* key,
                                    DictKeyHashFunc hash_func,
                                    DictKeyEqFunc eq_func,
                                    size_t slot_size) {
  size_t hash = hash_func(key);
  DictProbeSeq seq = dict_probeseq_start(self->ctrl, hash, self->capacity);
  while (true) {
    DictGroup g = dict_group_new(self->ctrl + seq.offset);
    DictBitMask match = dict_group_match(&g, dict_h2(hash));
    uint32_t i;
    while (dict_bitmask_next(&match, &i)) {
      char* slot = self->slots + dict_probeseq_offset(&seq, i) * slot_size;
      if (BRANCH_LIKELY(eq_func(key, slot))) {
        return dict_iter_at(self, dict_probeseq_offset(&seq, i), slot_size);
      }
    }
    if (BRANCH_LIKELY(dict_group_match_empty(&g).mask)) {
      return (DictRawIter){0};
    }
    dict_probeseq_next(&seq);
    ASSERT(seq.index <= self->capacity);  // table should not be full.
  }
}

typedef struct DictInsert {
  DictRawIter iter;
  bool inserted;  // true if it was inserted, false if it was already there.
} DictInsert;

typedef struct DictPrepareInsert {
  size_t index;
  bool inserted;
} DictPrepareInsert;

static void dict_drop_deletes_without_resize(DictImpl* self,
                                             DictKeyHashFunc hash_func,
                                             size_t slot_size,
                                             size_t slot_align) {
  ASSERT(dict_is_valid_capacity(self->capacity));
  // Algorithm:
  // - mark all DELETED slots as EMPTY
  // - mark all FULL slots as DELETED
  // - for each slot marked as DELETED
  //     hash = Hash(element)
  //     target = find_first_non_full(hash)
  //     if target is in the same group
  //       mark slot as FULL
  //     else if target is EMPTY
  //       transfer element to target
  //       mark slot as EMPTY
  //       mark target as FULL
  //     else if target is DELETED
  //       swap current element with target element
  //       mark target as FULL
  //       repeat procedure for current slot with moved from element (target)
  dict_convert_deleted_to_empty_and_full_to_deleted(self->ctrl, self->capacity);
  // size_t total_probe_length = 0;

  _Alignas(16) char temp_slot[1024];
  ASSERT(slot_size < sizeof(temp_slot));
  ASSERT(slot_align < 16);

  for (size_t i = 0; i < self->capacity; ++i) {
    if (!dict_is_deleted(self->ctrl[i])) {
      continue;
    }

    char* old_slot = self->slots + i * slot_size;
    size_t hash = hash_func(old_slot);

    DictFindInfo target = dict_find_first_non_full(self->ctrl, hash, self->capacity);
    size_t new_i = target.offset;
    // total_probe_length += target.probe_length;

    char* new_slot = self->slots + new_i * slot_size;

    // Verify if the old and new i fall within the same group wrt the hash.
    // If they do, we don't need to move the object as it falls already in the
    // best probe we can.
    size_t probe_offset = dict_probeseq_start(self->ctrl, hash, self->capacity).offset;
#define dict_probe_index(pos) (((pos - probe_offset) & self->capacity) / DictGroupWidth)

    // Element doesn't move.
    if (BRANCH_LIKELY(dict_probe_index(new_i) == dict_probe_index(i))) {
      dict_set_ctrl(i, dict_h2(hash), self->capacity, self->ctrl, self->slots, slot_size);
      continue;
    }
    if (dict_is_empty(self->ctrl[new_i])) {
      // Transfer element to the right spot.
      dict_set_ctrl(new_i, dict_h2(hash), self->capacity, self->ctrl, self->slots, slot_size);
      memcpy(new_slot, old_slot, slot_size);
      dict_set_ctrl(i, DCB_Empty, self->capacity, self->ctrl, self->slots, slot_size);
    } else {
      ASSERT(dict_is_deleted(self->ctrl[new_i]));
      dict_set_ctrl(new_i, dict_h2(hash), self->capacity, self->ctrl, self->slots, slot_size);

      // Until we are done rehashing, DELETED marks previously FULL slots.
      // Swap i and new_i elements.

      memcpy(temp_slot, old_slot, slot_size);
      memcpy(old_slot, new_slot, slot_size);
      memcpy(new_slot, temp_slot, slot_size);
      --i;  // repeat
    }
#undef dict_probe_index

    dict_reset_growth_left(self);
  }
}

static inline void dict_rehash_and_grow_if_necessary(DictImpl* self,
                                                     DictKeyHashFunc hash_func,
                                                     size_t slot_size,
                                                     size_t slot_align) {
  if (self->capacity == 0) {
    dict_resize(self, 1, hash_func, slot_size, slot_align);
  } else if (self->capacity > DictGroupWidth &&
             // Do these calculations in 64-bit to avoid overflow.
             self->size * UINT64_C(32) <= self->capacity * UINT64_C(25)) {
    // There's a very long comment in the original that I'm just going to assume
    // is right and not put it here.
    dict_drop_deletes_without_resize(self, hash_func, slot_size, slot_align);
  } else {
    dict_resize(self, self->capacity * 2 + 1, hash_func, slot_size, slot_align);
  }
}

// Given the hash of a value not currently in the table, finds the next viable
// slot index to insert it at.
static size_t dict_prepare_insert(DictImpl* self,
                                  size_t hash,
                                  DictKeyHashFunc hash_func,
                                  size_t slot_size,
                                  size_t slot_align) {
  DictFindInfo target = dict_find_first_non_full(self->ctrl, hash, self->capacity);
  if (BRANCH_UNLIKELY(self->growth_left == 0 && !dict_is_deleted(self->ctrl[target.offset]))) {
    dict_rehash_and_grow_if_necessary(self, hash_func, slot_size, slot_align);
    target = dict_find_first_non_full(self->ctrl, hash, self->capacity);
  }
  ++self->size;
  self->growth_left -= dict_is_empty(self->ctrl[target.offset]);
  dict_set_ctrl(target.offset, dict_h2(hash), self->capacity, self->ctrl, self->slots, slot_size);
  return target.offset;
}

// Attempts to find key in the table. If it isn't found, returns where to insert
// it instead.
static inline DictPrepareInsert dict_find_or_prepare_insert(DictImpl* self,
                                                            void* key,
                                                            DictKeyHashFunc hash_func,
                                                            DictKeyEqFunc eq_func,
                                                            size_t slot_size,
                                                            size_t slot_align) {
  // TODO dict_prefetch_heap_block(self)
  size_t hash = hash_func(key);
  DictProbeSeq seq = dict_probeseq_start(self->ctrl, hash, self->capacity);
  while (true) {
    DictGroup g = dict_group_new(self->ctrl + seq.offset);
    DictBitMask match = dict_group_match(&g, dict_h2(hash));
    uint32_t i;
    while (dict_bitmask_next(&match, &i)) {
      size_t idx = dict_probeseq_offset(&seq, i);
      char* slot = self->slots + idx * slot_size;
      if (BRANCH_LIKELY(eq_func(key, slot))) {
        return (DictPrepareInsert){idx, false};
      }
    }
    if (BRANCH_LIKELY(dict_group_match_empty(&g).mask)) {
      break;
    }
    dict_probeseq_next(&seq);
    ASSERT(seq.index <= self->capacity);  // table should not be full.
  }
  return (DictPrepareInsert){dict_prepare_insert(self, hash, hash_func, slot_size, slot_align),
                             true};
}

static inline void dict_dump(DictImpl* self, size_t slot_size) {
  fprintf(stderr, "ptr: %p, len: %zu, cap: %zu, growth_left: %zu\n", self->ctrl, self->size,
          self->capacity, self->growth_left);
  if (self->capacity == 0) {
    return;
  }

  size_t ctrl_bytes = self->capacity + dict_num_cloned_bytes();
  for (size_t i = 0; i <= ctrl_bytes; ++i) {
    fprintf(stderr, "[%4zu] %p / ", i, &self->ctrl[i]);
    switch (self->ctrl[i]) {
      case DCB_Sentinel:
        fprintf(stderr, " Sentinel: //\n");
        continue;
      case DCB_Empty:
        fprintf(stderr, "   Empty:");
        break;
      case DCB_Deleted:
        fprintf(stderr, "   Deleted:");
        break;
      default:
        fprintf(stderr, " H2(0x%02x)", self->ctrl[i]);
        break;
    }

    if (i >= self->capacity) {
      fprintf(stderr, ": <mirrored>\n");
    }

    char* slot = self->slots + i * slot_size;
    fprintf(stderr, ": %p /", slot);
    for (size_t j = 0; j < slot_size; ++j) {
      fprintf(stderr, " %02x", (unsigned char)slot[j]);
    }
    fprintf(stderr, "\n");
  }
}

static inline DictInsert dict_insert(DictImpl* self,
                                     void* val,
                                     DictKeyHashFunc hash_func,
                                     DictKeyEqFunc eq_func,
                                     size_t slot_size,
                                     size_t slot_align) {
  DictPrepareInsert res =
      dict_find_or_prepare_insert(self, val, hash_func, eq_func, slot_size, slot_align);

  if (res.inserted) {
    void* slot = self->slots + res.index * slot_size;
    memcpy(slot, val, slot_size);
  }

  return (DictInsert){dict_iter_at(self, res.index, slot_size), res.inserted};
}

// Inserts |val| into the table if it isn't already present.
//
// This function does not perform insertion; it behaves exactly like
// dict_insert() up until it would actually copy the new element, instead
// returning a valid iterator pointing to uninitialized data.
//
// This allows, for example, lazily constructing the parts of the element that
// do not figure into the hash or equality.
//
// If this function returns true in inserted, the caller has *no choice*
// but to insert, i.e., they may not change their minds at that point.
static inline DictInsert dict_deferred_insert(DictImpl* self,
                                              void* key,
                                              DictKeyHashFunc hash_func,
                                              DictKeyEqFunc eq_func,
                                              size_t slot_size,
                                              size_t slot_align) {
  DictPrepareInsert res =
      dict_find_or_prepare_insert(self, key, hash_func, eq_func, slot_size, slot_align);
  return (DictInsert){dict_iter_at(self, res.index, slot_size), res.inserted};
}

static inline bool dict_contains(DictImpl* self,
                                 void* key,
                                 DictKeyHashFunc hash_func,
                                 DictKeyEqFunc eq_func,
                                 size_t slot_size) {
  return dict_find(self, key, hash_func, eq_func, slot_size).slot != NULL;
}

// Erases the value pointed to by |it|, but doesn't touch the slot memory.
static inline void dict_erase_meta_only(DictRawIter it, size_t slot_size) {
  ASSERT(dict_is_in_use(*it.ctrl));  // dangling iter
  --it.dict->size;
  size_t index = (size_t)(it.ctrl - it.dict->ctrl);
  size_t index_before = (index - DictGroupWidth) & it.dict->capacity;
  DictGroup g_after = dict_group_new(it.ctrl);
  DictBitMask empty_after = dict_group_match_empty(&g_after);
  DictGroup g_before = dict_group_new(it.dict->ctrl + index_before);
  DictBitMask empty_before = dict_group_match_empty(&g_before);

  // Count how many consecutive non empties we have to the right and to the left
  // of |it|. If the sum is >= GroupWidth then there is at least one probe
  // window that might have seen a full group.
  bool was_never_full = empty_before.mask && empty_after.mask &&
                        (size_t)(dict_bitmask_trailing_zeros(&empty_after) +
                                 dict_bitmask_leading_zeros(&empty_before)) < DictGroupWidth;

  dict_set_ctrl(index, was_never_full ? DCB_Empty : DCB_Deleted, it.dict->capacity, it.dict->ctrl,
                it.dict->slots, slot_size);
  it.dict->growth_left += was_never_full;
}

static inline void dict_erase_at(DictRawIter it, size_t slot_size) {
  dict_assert_is_in_use(it.ctrl);
  dict_erase_meta_only(it, slot_size);
}

static inline bool dict_erase(DictImpl* self,
                              void* key,
                              DictKeyHashFunc hash_func,
                              DictKeyEqFunc eq_func,
                              size_t slot_size) {
  DictRawIter it = dict_find(self, key, hash_func, eq_func, slot_size);
  if (it.slot == NULL) {
    return false;
  }
  dict_erase_at(it, slot_size);
  return true;
}

static inline void dict_rehash(DictImpl* self,
                               size_t n,
                               DictKeyHashFunc hash_func,
                               size_t slot_size,
                               size_t slot_align) {
  if (n == 0 && self->capacity == 0) {
    return;
  }
  if (n == 0 && self->size == 0) {
    dict_destroy(self);
    return;
  }

  // bitor is a faster way of doing max here. We will round up to the next
  // power-of-2-minus-1, so bitor is good enough.
  size_t m = dict_normalize_capacity(n | dict_growth_to_lower_bound_capacity(self->size));
  // n == 0 unconditionally rehashes.
  if (n == 0 || m > self->capacity) {
    dict_resize(self, m, hash_func, slot_size, slot_align);
  }
}

// Ensures that at least |n| more elements can be inserted without a resize
// (although this function my itself resize and rehash the table).
static inline void dict_reserve(DictImpl* self,
                                size_t n,
                                DictKeyHashFunc hash_func,
                                size_t slot_size,
                                size_t slot_align) {
  if (n <= self->size + self->growth_left) {
    return;
  }

  n = dict_normalize_capacity(dict_growth_to_lower_bound_capacity(n));
  dict_resize(self, n, hash_func, slot_size, slot_align);
}

static inline DictRawIter dict_iter(DictImpl* self, size_t slot_size) {
  return dict_iter_at(self, 0, slot_size);
}
