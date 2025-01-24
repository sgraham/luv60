#include "luv60.h"

#include "dict.h"

_Static_assert(NUM_TYPE_KINDS < (1<<8), "Too many TypeKind");

typedef union TypeData {
  struct {
    uint32_t size;
    uint32_t align;
  } BASIC;
  struct {
    uint32_t size;
    uint32_t align;
    Type subtype;
  } PTR;
  struct {
    uint32_t size;
    uint32_t align;
    Type subtype;
    uint32_t count;
  } ARRAY;
  struct {
    uint32_t size;
    uint32_t align;
    Type key;
    Type value;
  } DICT;
  struct {
    Str declname;
    uint32_t size;
    // 1 has_initializer, 7 align, 24 num_fields (could compress align to 3 bits)
    uint32_t has_init_align_and_num_fields;
  } STRUCT;
  struct {
    Type return_type;
    uint32_t num_params;
    uint32_t flags;  // currently unused
    uint32_t unused0; // Should make this param0 type
  } FUNC;

  // TODO: slice, tuple, enum, union
} TypeData;

// Structs were going to fit nicely in 12 bytes, but mucked that up with Str
// changing to be a u64. So, add total_size to array which is questionably
// useful, and others have a bunch of unused fields. But easier to have
// TypeDataExtra not be some weird 12 byte allocation of 16-sized chunks for
// structs.
//
// Ah, nuts, now I need a pointer to the initializer blob in structs, so it
// doesn't fit again. Maybe 12 would have been just as good.

#define WORDS_IN_EXTRA 4
typedef union TypeDataExtra {
  struct {
    Str field_name;
    Type field_type;
    uint32_t field_offset;
  } STRUCT_EXTRA;
  struct {
    void* addr;
  } STRUCT_INITIALIZER;
  struct {
    Type param[WORDS_IN_EXTRA];
  } FUNC_EXTRA;
} TypeDataExtra;

_Static_assert(sizeof(TypeData) == sizeof(uint32_t) * WORDS_IN_EXTRA, "TypeData unexpected size");
_Static_assert(sizeof(TypeDataExtra) == sizeof(uint32_t) * WORDS_IN_EXTRA,
               "TypeDataExtra unexpected size");
_Static_assert(sizeof(TypeData) == sizeof(TypeDataExtra),
               "TypeData and TypeDataExtra have to match");

static TypeData typedata[16<<20];
static int num_typedata;

static DictImpl cached_func_types;
static DictImpl cached_ptr_types;
static DictImpl cached_arr_types;
static Arena* arena_;

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

static void set_builtin_typedata(uint32_t index, uint32_t size, uint32_t align) {
  ASSERT(index < COUNTOF(typedata));
  ASSERT(typedata[index].BASIC.size == 0);
  ASSERT(typedata[index].BASIC.align == 0);
  typedata[index].BASIC.size = size;
  typedata[index].BASIC.align = align;
}

static inline TypeData* type_td(Type t) {
  return &typedata[t.u >> 8];
}

size_t type_size(Type type) {
  switch (type_kind(type)) {
    case TYPE_PTR:
      return 8;
    case TYPE_ARRAY:
      return type_td(type)->ARRAY.size;
      abort();
    case TYPE_DICT:
      ASSERT(false && "todo");
      abort();
    case TYPE_STRUCT:
      return type_td(type)->STRUCT.size;
    case TYPE_FUNC:
      return 8;
    default:
      return type_td(type)->BASIC.size;
  }
}

size_t type_align(Type type) {
  switch (type_kind(type)) {
    case TYPE_PTR:
      return 8;
    case TYPE_ARRAY:
      ASSERT(false && "todo");
      abort();
    case TYPE_DICT:
      ASSERT(false && "todo");
      abort();
    case TYPE_STRUCT:
      return ((type_td(type)->STRUCT.has_init_align_and_num_fields) >> 24) & 0x7f;
    case TYPE_FUNC:
      ASSERT(false && "todo");
      abort();
    default:
      return type_td(type)->BASIC.align;
  }
}

static Type type_alloc(TypeKind kind, int extra, uint32_t* out_rewind_location) {
  *out_rewind_location = num_typedata;
  Type ret = {((num_typedata++) << 8) | kind};
  num_typedata += extra;
  return ret;
}

static size_t functype_hash_func(void* functype) {
  Type t = *(Type*)functype;
  size_t hash = 0;
  TypeData* td = type_td(t);
  size_t typedata_blocks = 1 + ROUND_UP(td->FUNC.num_params, WORDS_IN_EXTRA);
  dict_hash_write(&hash, td, typedata_blocks * sizeof(TypeData));
  return hash;
}

static bool functype_eq_func(void* keyvoid, void* slotvoid) {
  Type k = *(Type*)keyvoid;
  Type s = *(Type*)slotvoid;
  TypeData* ktd = type_td(k);
  TypeData* std = type_td(s);
  if (ktd->FUNC.num_params != std->FUNC.num_params) {
    return false;
  }
  if (!type_eq(ktd->FUNC.return_type, std->FUNC.return_type)) {
    return false;
  }
  size_t extra_typedata_blocks = ROUND_UP(ktd->FUNC.num_params, WORDS_IN_EXTRA);
  return memcmp(ktd + 1, std + 1, extra_typedata_blocks * sizeof(TypeDataExtra)) == 0;
}

Type type_function(Type* params, size_t num_params, Type return_type) {
  uint32_t rewind_location;
  Type func =
      type_alloc(TYPE_FUNC, /*extra=*/ROUND_UP(num_params, WORDS_IN_EXTRA), &rewind_location);

  // Copy it in to a new slot so that the DictImpl can hash/eq them, dealloc by
  // rewinding num_typedata if it turns out this type already exists.
  // TODO: probably could be a little faster with a special dict method that
  // takes a precomputed hash so we could calculate it in place first and then
  // only copy in to typedata if we don't match.
  TypeData* td = type_td(func);
  td->FUNC.num_params = num_params;
  td->FUNC.return_type = return_type;
  /* This would be the typed version, but it's just a memcpy into TypeDataExtra.
  TypeDataExtra* tde = (TypeDataExtra*)(td + 1);
  for (size_t i = 0; i < num_params; ++i) {
    tde->FUNC_EXTRA.param[i % WORDS_IN_EXTRA] = params[i];
    if (i % WORDS_IN_EXTRA == WORDS_IN_EXTRA - 1) {
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
    dicttype->u = func.u;
    return func;
  } else {
    num_typedata = rewind_location;
    return *dicttype;
  }
}

Type type_new_struct(Str name,
                     uint32_t num_fields,
                     Str* field_names,
                     Type* field_types,
                     bool has_initializer) {
  uint32_t unused;
  Type strukt = type_alloc(TYPE_STRUCT, /*extra=*/num_fields + (has_initializer ? 1 : 0), &unused);

  TypeData* td = type_td(strukt);

  uint32_t size = 0;
  uint32_t align = 0;
  uint32_t field_sizes = 0;
  TypeDataExtra* tde = (TypeDataExtra*)(td + 1);
  for (uint32_t i = 0; i < num_fields; ++i) {
    Type t = field_types[i];
    // For {int, bool, int} the second int needs to be natural aligned.
    size = ALIGN_UP(size, type_align(t));
    ASSERT(IS_POW2(type_align(t)));

    tde[i].STRUCT_EXTRA.field_name = field_names[i];
    tde[i].STRUCT_EXTRA.field_type = t;
    tde[i].STRUCT_EXTRA.field_offset = size;

    field_sizes += type_size(t);
    align = MAX(align, type_align(t));
    size = type_size(t) + ALIGN_UP(size, type_align(t));
  }

  size = ALIGN_UP(size, align);
  (void)field_sizes;
  //uint32_t padding = size - field_sizes;

  ASSERT(align <= 0xff);
  //ASSERT(padding <= 0xff);
  ASSERT(num_fields <= 0xffffff);

  td->STRUCT.declname = name;
  td->STRUCT.size = size;
  td->STRUCT.has_init_align_and_num_fields =
      ((has_initializer ? 1 : 0) << 31) | (align & 0x7f) << 24 | (num_fields & 0xffffff);

  return strukt;
}

void type_struct_set_initializer_blob(Type type, void* blob) {
  ASSERT(type_kind(type) == TYPE_STRUCT);
  ASSERT(type_struct_has_initializer(type));
  TypeData* td = type_td(type);
  TypeDataExtra* tde = (TypeDataExtra*)(td + 1);
  TypeDataExtra* init_extra = &tde[type_struct_num_fields(type)];
  init_extra->STRUCT_INITIALIZER.addr = blob;
}

// For TypeDatas that don't have extra entries.
static size_t plaintype_hash_func(void* functype) {
  Type t = *(Type*)functype;
  size_t hash = 0;
  TypeData* td = type_td(t);
  dict_hash_write(&hash, td, sizeof(TypeData));
  return hash;
}

// For TypeDatas that don't have extra entries.
static bool plaintype_eq_func(void* keyvoid, void* slotvoid) {
  Type k = *(Type*)keyvoid;
  Type s = *(Type*)slotvoid;
  TypeData* ktd = type_td(k);
  TypeData* std = type_td(s);
  return memcmp(ktd, std, sizeof(TypeData)) == 0;
}

Type type_ptr(Type subtype) {
  uint32_t rewind_location;
  Type ptr = type_alloc(TYPE_PTR, 0, &rewind_location);
  TypeData* td = type_td(ptr);
  td->PTR.size = 8;
  td->PTR.align = 8;
  td->PTR.subtype = subtype;

  // The dict is only a set of intern'd Type, but we know they're all TYPE_PTR.
  DictInsert res = dict_deferred_insert(&cached_ptr_types, &ptr, plaintype_hash_func,
                                        plaintype_eq_func, sizeof(Type), _Alignof(Type));
  Type* ptrtype = ((Type*)dict_rawiter_get(&res.iter));
  if (res.inserted) {
    ptrtype->u = ptr.u;
    return ptr;
  } else {
    num_typedata = rewind_location;
    return *ptrtype;
  }
}

Type type_array(Type subtype, size_t size) {
  ASSERT(size <= 0xffffffff);
  uint32_t rewind_location;
  Type arr = type_alloc(TYPE_ARRAY, 0, &rewind_location);
  TypeData* td = type_td(arr);
  td->ARRAY.size = size * type_size(subtype);
  td->ARRAY.align = type_align(subtype);
  td->ARRAY.subtype = subtype;
  td->ARRAY.count = size;

  // The dict is only a set of intern'd Type, but we know they're all TYPE_ARRAY.
  DictInsert res = dict_deferred_insert(&cached_arr_types, &arr, plaintype_hash_func,
                                        plaintype_eq_func, sizeof(Type), _Alignof(Type));
  Type* arrtype = ((Type*)dict_rawiter_get(&res.iter));
  if (res.inserted) {
    arrtype->u = arr.u;
    return arr;
  } else {
    num_typedata = rewind_location;
    return *arrtype;
  }
}


// Returned str is either the cstr() of an interned string, or a constant.
// Not fast or memory efficient, should only be used during errors though.
const char* type_as_str(Type type) {
  if (natural_builtin_type_names[type_kind(type)]) {
    return natural_builtin_type_names[type_kind(type)];
  }
  switch (type_kind(type)) {
    case TYPE_FUNC: {
      Type return_type = type_func_return_type(type);
      Str head =
          str_internf("def %s (", type_eq(type_void, return_type) ? "" : type_as_str(return_type));
      uint32_t num = type_func_num_params(type);
      for (uint32_t i = 0; i < num; ++i) {
        head = str_internf("%s%s%s", cstr_copy(arena_, head), type_as_str(type_func_param(type, i)),
                           i < num - 1 ? ", " : "");
      }
      return cstr_copy(arena_, str_internf("%s)", cstr_copy(arena_, head)));
    }
    case TYPE_PTR: {
      Str ptr = str_internf("*%s", type_as_str(type_ptr_subtype(type)));
      return cstr_copy(arena_, ptr);
    }
    case TYPE_ARRAY: {
      return cstr_copy(arena_, str_internf("[%d]%s", type_array_count(type),
                                           type_as_str(type_array_subtype(type))));
    }
    case TYPE_STRUCT: {
      return "TODO: STRUCT";
    }
    default:
      ASSERT(false && "todo");
      return "TODO";
  }
}

void type_init(Arena* arena) {
  arena_ = arena;
  cached_func_types = dict_new(arena, 128, sizeof(Type), _Alignof(Type));
  cached_ptr_types = dict_new(arena, 128, sizeof(Type), _Alignof(Type));
  cached_arr_types = dict_new(arena, 128, sizeof(Type), _Alignof(Type));

  set_builtin_typedata(TYPE_VOID, 0, 1);
  set_builtin_typedata(TYPE_BOOL, 1, 1);
  set_builtin_typedata(TYPE_U8, 1, 1);
  set_builtin_typedata(TYPE_I8, 1, 1);
  set_builtin_typedata(TYPE_U16, 2, 2);
  set_builtin_typedata(TYPE_I16, 2, 2);
  set_builtin_typedata(TYPE_U32, 4, 4);
  set_builtin_typedata(TYPE_I32, 4, 4);
  set_builtin_typedata(TYPE_U64, 8, 8);
  set_builtin_typedata(TYPE_I64, 8, 8);
  set_builtin_typedata(TYPE_FLOAT, 4, 4);
  set_builtin_typedata(TYPE_DOUBLE, 8, 8);
  set_builtin_typedata(TYPE_STR, 16, 8);
  set_builtin_typedata(TYPE_RANGE, 24, 8);
  num_typedata = NUM_TYPE_KINDS;
}

bool type_is_unsigned(Type type) {
  ASSERT(type_is_integer(type));
  switch (type_kind(type)) {
    case TYPE_U8:
    case TYPE_U16:
    case TYPE_U32:
    case TYPE_U64:
      return true;
    case TYPE_I8:
    case TYPE_I16:
    case TYPE_I32:
    case TYPE_I64:
      return false;
    default:
      ASSERT(false);
      TRAP();
  }
}

bool type_is_signed(Type type) {
  ASSERT(type_is_integer(type));
  switch (type_kind(type)) {
    case TYPE_U8:
    case TYPE_U16:
    case TYPE_U32:
    case TYPE_U64:
      return false;
    case TYPE_I8:
    case TYPE_I16:
    case TYPE_I32:
    case TYPE_I64:
      return true;
    default:
      ASSERT(false);
      TRAP();
  }
}

bool type_is_arithmetic(Type type) {
  return type_kind(type) >= TYPE_U8 && type_kind(type) <= TYPE_DOUBLE;
}

bool type_signs_match(Type a, Type b) {
  ASSERT(type_is_arithmetic(a));
  ASSERT(type_is_arithmetic(b));
  return (type_is_signed(a) ^ type_is_signed(b)) == 0;
}

bool type_is_integer(Type type) {
  return type_kind(type) >= TYPE_U8 && type_kind(type) <= TYPE_ENUM;
}

bool type_is_ptr_like(Type type) {
  return type_kind(type) == TYPE_PTR || type_kind(type) == TYPE_FUNC;
}

bool type_is_aggregate(Type type) {
  switch (type_kind(type)) {
    case TYPE_STR:
    case TYPE_RANGE:
    case TYPE_SLICE:
    case TYPE_ARRAY:
    case TYPE_DICT:
    case TYPE_STRUCT:
    case TYPE_UNION:
      return true;
    default:
      return false;
  }
}

bool type_is_condition(Type type) {
  switch (type_kind(type)) {
    case TYPE_BOOL:
    case TYPE_STR:
    case TYPE_SLICE:
    case TYPE_PTR:
      return true;
    default:
      return false;
  }
}

uint32_t type_func_num_params(Type type) {
  ASSERT(type_kind(type) == TYPE_FUNC);
  TypeData* td = type_td(type);
  return td->FUNC.num_params;
}

Type type_func_return_type(Type type) {
  ASSERT(type_kind(type) == TYPE_FUNC);
  TypeData* td = type_td(type);
  return td->FUNC.return_type;
}

Type type_func_param(Type type, uint32_t i){
  ASSERT(type_kind(type) == TYPE_FUNC);
  ASSERT(i < type_func_num_params(type));
  TypeData* td = type_td(type);
  TypeDataExtra* tde = (TypeDataExtra*)(td + 1 + (i / WORDS_IN_EXTRA));
  return tde->FUNC_EXTRA.param[i % WORDS_IN_EXTRA];
}

Type type_ptr_subtype(Type type) {
  ASSERT(type_kind(type) == TYPE_PTR);
  TypeData* td = type_td(type);
  return td->PTR.subtype;
}

Type type_array_subtype(Type type) {
  ASSERT(type_kind(type) == TYPE_ARRAY);
  TypeData* td = type_td(type);
  return td->ARRAY.subtype;
}

uint32_t type_array_count(Type type) {
  ASSERT(type_kind(type) == TYPE_ARRAY);
  TypeData* td = type_td(type);
  return td->ARRAY.count;
}

uint32_t type_struct_num_fields(Type type) {
  ASSERT(type_kind(type) == TYPE_STRUCT);
  TypeData* td = type_td(type);
  return td->STRUCT.has_init_align_and_num_fields & 0xffffff;
}

Str type_struct_decl_name(Type type) {
  ASSERT(type_kind(type) == TYPE_STRUCT);
  TypeData* td = type_td(type);
  return td->STRUCT.declname;
}

bool type_struct_has_initializer(Type type) {
  ASSERT(type_kind(type) == TYPE_STRUCT);
  TypeData* td = type_td(type);
  return (td->STRUCT.has_init_align_and_num_fields & 0x80000000) != 0;
}

void* type_struct_initializer_blob(Type type) {
  ASSERT(type_kind(type) == TYPE_STRUCT);
  ASSERT(type_struct_has_initializer(type));
  TypeData* td = type_td(type);
  TypeDataExtra* tde = (TypeDataExtra*)(td + 1);
  TypeDataExtra* init_extra = &tde[type_struct_num_fields(type)];
  return init_extra->STRUCT_INITIALIZER.addr;
}

Str type_struct_field_name(Type type, uint32_t i) {
  ASSERT(type_kind(type) == TYPE_STRUCT);
  ASSERT(i < type_struct_num_fields(type));
  TypeData* td = type_td(type);
  TypeDataExtra* tde = (TypeDataExtra*)(td + 1);
  return tde[i].STRUCT_EXTRA.field_name;
}

Type type_struct_field_type(Type type, uint32_t i) {
  ASSERT(type_kind(type) == TYPE_STRUCT);
  ASSERT(i < type_struct_num_fields(type));
  TypeData* td = type_td(type);
  TypeDataExtra* tde = (TypeDataExtra*)(td + 1);
  return tde[i].STRUCT_EXTRA.field_type;
}

uint32_t type_struct_field_offset(Type type, uint32_t i) {
  ASSERT(type_kind(type) == TYPE_STRUCT);
  ASSERT(i < type_struct_num_fields(type));
  TypeData* td = type_td(type);
  TypeDataExtra* tde = (TypeDataExtra*)(td + 1);
  return tde[i].STRUCT_EXTRA.field_offset;
}

uint32_t type_struct_field_index_by_name(Type type, Str name) {
  ASSERT(type_kind(type) == TYPE_STRUCT);
  TypeData* td = type_td(type);
  TypeDataExtra* tde = (TypeDataExtra*)(td + 1);
  uint32_t num_fields = type_struct_num_fields(type);
  for (uint32_t i = 0; i < num_fields; ++i) {
    if (str_eq(tde[i].STRUCT_EXTRA.field_name, name)) {
      return i;
    }
  }
  return num_fields;
}

bool type_struct_find_field_by_name(Type type, Str name, Type* out_type, uint32_t* out_offset) {
  ASSERT(type_kind(type) == TYPE_STRUCT);
  TypeData* td = type_td(type);
  TypeDataExtra* tde = (TypeDataExtra*)(td + 1);
  uint32_t num_fields = type_struct_num_fields(type);
  for (uint32_t i = 0; i < num_fields; ++i) {
    if (str_eq(tde[i].STRUCT_EXTRA.field_name, name)) {
      *out_type = tde[i].STRUCT_EXTRA.field_type;
      *out_offset = tde[i].STRUCT_EXTRA.field_offset;
      return true;
    }
  }
  return false;
}

void type_destroy_for_tests(void) {
  dict_destroy(&cached_func_types);
  memset(typedata, 0, sizeof(TypeData) * num_typedata);
  num_typedata = 0;
}
