#define EXPAND_(p, rest) p ## rest
#define EXPAND(p, rest) EXPAND_(p, rest)
#define VEC_NAME_JOIN(rest) EXPAND(VEC_PREFIX, rest)

typedef struct VEC_NAME {
  union {
    VEC_T* data;
    VEC_T short_data[VEC_NUM_SHORT];
  };
  Arena* arena;
  int64_t size;
  int64_t capacity;
} VEC_NAME;

static inline FORCE_INLINE void VEC_NAME_JOIN(free)(VEC_NAME* vec) {
  vec->size = 0;
}

static inline FORCE_INLINE int64_t VEC_NAME_JOIN(size)(VEC_NAME* vec) {
  return vec->size;
}

static void VEC_NAME_JOIN(ensure_capacity)(VEC_NAME* vec, int64_t size) {
  if (size <= vec->capacity) {
    return;
  }

  // capacity starts at short_data len, so always moving to allocated if
  // growing.
  VEC_T* new = arena_push(vec->arena, sizeof(VEC_T) * size, _Alignof(VEC_T));
  VEC_T* old = vec->capacity <= COUNTOFI(vec->short_data) ? vec->short_data : vec->data;
  memcpy(new, old, sizeof(VEC_T) * vec->size);
  vec->capacity = size;
}

static VEC_T VEC_NAME_JOIN(at)(VEC_NAME* vec, int64_t i) {
  ASSERT(i < vec->size);
  if (vec->capacity <= COUNTOFI(vec->short_data)) {
    return vec->short_data[i];
  }
  return vec->data[i];
}

static void VEC_NAME_JOIN(set)(VEC_NAME* vec, int64_t i, VEC_T op) {
  ASSERT(i < vec->size);
  if (vec->capacity <= COUNTOFI(vec->short_data)) {
    vec->short_data[i] = op;
  } else {
    vec->data[i] = op;
  }
}

static void VEC_NAME_JOIN(append)(VEC_NAME* vec, VEC_T op) {
  VEC_NAME_JOIN(ensure_capacity)(vec, vec->size + 1);
  ++vec->size;
  VEC_NAME_JOIN(set)(vec, vec->size - 1, op);
}

static VEC_T* VEC_NAME_JOIN(dataptr)(VEC_NAME* vec) {
  if (vec->capacity <= COUNTOFI(vec->short_data)) {
    return &vec->short_data[0];
  } else {
    return &vec->data[0];
  }
}

static inline FORCE_INLINE void VEC_NAME_JOIN(init)(VEC_NAME* vec, Arena* arena) {
  vec->size = 0;
  vec->capacity = COUNTOFI(vec->short_data);
  vec->arena = arena;

  (void)VEC_NAME_JOIN(append);
  (void)VEC_NAME_JOIN(at);
  (void)VEC_NAME_JOIN(dataptr);
  (void)VEC_NAME_JOIN(ensure_capacity);
  (void)VEC_NAME_JOIN(free);
  (void)VEC_NAME_JOIN(set);
  (void)VEC_NAME_JOIN(size);
}

#undef VEC_NAME
#undef VEC_T
#undef VEC_PREFIX
#undef VEC_NUM_SHORT
