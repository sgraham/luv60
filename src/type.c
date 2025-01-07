#include "luv60.h"

#include "dict.h"

typedef struct TypeData {
  uint32_t kaps;  // 8 kind 8 align 8 padding 8 size(for non-array)
  union {
    struct {
      Type subtype;
    } PTR;
    struct {
      Type subtype;
      uint32_t count;
    } ARRAY;
    struct {
      Type key;
      Type value;
    } DICT;
    struct {
      uint32_t TODO;  // count in base, type, name, offset in followers
    } STRUCT;
    struct {
      uint32_t num_params;
      Type return_type;
    } FUNC;
  };
} TypeData;

typedef union TypeDataExtra {
  struct {
    uint32_t TODO;
  } STRUCT_EXTRA;
  struct {
    Type param[3];
  } FUNC_EXTRA;
} TypeDataExtra;

_Static_assert(sizeof(TypeData) == sizeof(uint32_t) * 3, "TypeData unexpected size");
_Static_assert(sizeof(TypeDataExtra) == sizeof(uint32_t) * 3, "TypeDataExtra unexpected size");
_Static_assert(sizeof(TypeData) == sizeof(TypeDataExtra),
               "TypeData and TypeDataExtra have to match");

static TypeData typedata[16<<20];
static int num_typedata;

static DictImpl cached_func_types;

const char* natural_builtin_type_names[NUM_TYPE_KINDS] = {
    [TYPE_VOID] = "opaque",    //
    [TYPE_BOOL] = "bool",      //
    [TYPE_U8] = "u8",          //
    [TYPE_U16] = "u16",        //
    [TYPE_U32] = "u32",        //
    [TYPE_U64] = "u64",        //
    [TYPE_I8] = "i8",          //
    [TYPE_I16] = "i16",        //
    [TYPE_I32] = "i32",        //
    [TYPE_I64] = "i64",        //
    [TYPE_FLOAT] = "float",    //
    [TYPE_DOUBLE] = "double",  //
    [TYPE_STR] = "str",        //
    [TYPE_RANGE] = "Range",    //
    [TYPE_CHAR] = "char",      //
};

static void set_typedata_raw(uint32_t index, uint32_t data) {
  ASSERT(typedata[index].kaps == 0);
  ASSERT(index < COUNTOF(typedata));
  typedata[index].kaps = data;
}

static uint32_t pack_type(TypeKind kind, uint8_t size, uint8_t align, uint8_t padding) {
  ASSERT(kind != 0);
  ASSERT(kind < (1<<8));
  ASSERT(align > 0);
  uint32_t ret = (size << 24) | (padding << 16) | (align << 8) | kind;
  return ret;
}

static Type type_alloc(TypeKind kind, int extra) {
  Type ret = {num_typedata++};
  num_typedata += extra;
  return ret;
}

static size_t functype_hash_func(void* functype) {
  Type t = *(Type*)functype;
  size_t hash = 0;
  TypeData* td = &typedata[t.i];
  size_t typedata_blocks = 1 + (td->FUNC.num_params + 2) / 3;
  dict_hash_write(&hash, &typedata[t.i], typedata_blocks * sizeof(TypeData));
  return hash;
}

static bool functype_eq_func(void* keyvoid, void* slotvoid) {
  Type k = *(Type*)keyvoid;
  Type s = *(Type*)slotvoid;
  TypeData* ktd = &typedata[k.i];
  TypeData* std = &typedata[s.i];
  if (ktd->FUNC.num_params != std->FUNC.num_params) {
    return false;
  }
  if (ktd->FUNC.return_type.i != std->FUNC.return_type.i) {
    return false;
  }
  size_t extra_typedata_blocks = (ktd->FUNC.num_params + 2) / 3;
  return memcmp(ktd + 1, std + 1, extra_typedata_blocks * sizeof(TypeDataExtra)) == 0;
}

Type type_function(Type* params, size_t num_params, Type return_type) {
  Type func = type_alloc(TYPE_FUNC, /*extra=*/(num_params + 2) / 3);

  // Copy it in to a new slot so that the DictImpl can hash/eq them, dealloc by
  // rewinding num_typedata if it turns out this type already exists.
  // TODO: probably could be a little faster with a special dict method that
  // takes a precomputed hash so we could calculate it in place first and then
  // only copy in to typedata if we don't match.
  TypeData* td = &typedata[func.i];
  td->kaps = pack_type(TYPE_FUNC, 8, 8, 0);
  td->FUNC.num_params = num_params;
  td->FUNC.return_type = return_type;
  /* This would be the typed version, but it's just a memcpy into TypeDataExtra.
  TypeDataExtra* tde = (TypeDataExtra*)(td + 1);
  for (size_t i = 0; i < num_params; ++i) {
    tde->FUNC_EXTRA.param[i % 3] = params[i];
    if (i % 3 == 2) {
      ++tde;
    }
  }
  */
  memcpy(td + 1, params, sizeof(Type) * num_params);

  // The dict is only a set of intern'd Type, but we know they're all TYPE_FUNC.
  DictInsert res = dict_deferred_insert(&cached_func_types, &func, functype_hash_func,
                                        functype_eq_func, sizeof(Type), _Alignof(Type));
  Type* dicttype = ((Type*)dict_rawiter_get(&res.iter));
  if (res.inserted) {
    dicttype->i = func.i;
    return func;
  } else {
    // This is a "rewind", assuming that they're linearly allocated and we just
    // got the last one, return the counter to top for the next allocation.
    num_typedata = func.i;
    return *dicttype;
  }
}

// returned str is either the cstr() of an interned string, or a constant.
const char* type_as_str(Type type) {
  if (natural_builtin_type_names[type.i]) {
    return natural_builtin_type_names[type.i];
  } else {
    return "TODO";
  }
}

void type_init(void) {
  cached_func_types = dict_new(128, sizeof(Type), _Alignof(Type));

  set_typedata_raw(TYPE_VOID, pack_type(TYPE_VOID, 0, 1, 0));
  set_typedata_raw(TYPE_BOOL, pack_type(TYPE_BOOL, 1, 1, 0));
  set_typedata_raw(TYPE_U8, pack_type(TYPE_U8, 1, 1, 0));
  set_typedata_raw(TYPE_I8, pack_type(TYPE_I8, 1, 1, 0));
  set_typedata_raw(TYPE_U16, pack_type(TYPE_U16, 2, 2, 0));
  set_typedata_raw(TYPE_I16, pack_type(TYPE_I16, 2, 2, 0));
  set_typedata_raw(TYPE_U32, pack_type(TYPE_U32, 4, 4, 0));
  set_typedata_raw(TYPE_I32, pack_type(TYPE_I32, 4, 4, 0));
  set_typedata_raw(TYPE_U64, pack_type(TYPE_U64, 8, 8, 0));
  set_typedata_raw(TYPE_I64, pack_type(TYPE_I64, 8, 8, 0));
  set_typedata_raw(TYPE_FLOAT, pack_type(TYPE_FLOAT, 4, 4, 0));
  set_typedata_raw(TYPE_DOUBLE, pack_type(TYPE_DOUBLE, 8, 8, 0));
  set_typedata_raw(TYPE_STR, pack_type(TYPE_STR, 16, 8, 0));
  set_typedata_raw(TYPE_RANGE, pack_type(TYPE_STR, 24, 8, 0));
  num_typedata = NUM_TYPE_KINDS;
}

void type_destroy_for_tests(void) {
  dict_destroy(&cached_func_types);
  memset(typedata, 0, sizeof(typedata));
  num_typedata = 0;
}
