bool leak_check_backing_zone = true;

typedef struct Zone {
  void* ptr;
  void* reserve_base;
  void* reserve_end;
  void* commit_end;
} Zone;

#ifdef _WIN32

// Both kernel32.dll, but we don't want windows.h.
extern void* __stdcall VirtualAlloc(void* addr, size_t size, int alloc_type, int protect);
extern int __stdcall VirtualFree(void* addr, size_t size, int free_type);

#define MEM_COMMIT 0x00001000
#define MEM_RESERVE 0x00002000
#define PAGE_READWRITE 0x04
#define MEM_RELEASE 0x00008000

static void* alloc_large_slab(size_t size) {
  return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

static void free_large_slab(void* p, size_t size) {
  VirtualFree(p, 0, MEM_RELEASE);
}

#else

#include <sys/mman.h> // TODO

static void* alloc_large_slab(size_t size) {
  return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
}

static void free_large_slab(void* p, size_t size) {
  munmap(p, size);
}

#endif

#define ZONE_DEFAULT_RESERVE (1024*1024*1024)

#define ALIGN_DOWN(n, a) ((n) & ~((a)-1))
#define ALIGN_UP(n, a) ALIGN_DOWN((n) + (a)-1, (a))

#define PREFIX_SIZE sizeof(size_t)
#define MAX_ALLOC_SIZE 0xffffffffffffff
#define GET_PREFIX_DATA(p) (*(size_t*)(p - PREFIX_SIZE))
#define SET_PREFIX_DATA(p, v) do { (*(size_t*)(p - PREFIX_SIZE)) = v; } while(0);
#define GET_SIZE_FROM_PTR(p) (GET_PREFIX_DATA(p) >> 8)

static Zone zones[16];
static int cur_zone;

static int find_unused_zone(void) {
  for (int i = 0; i < COUNTOF(zones); ++i) {
    if (zones[i].ptr == NULL) {
      return i;
    }
  }
  CheckFailed();
  return 0;
}

int zone_create(void) {
  int ret = find_unused_zone();
  zones[ret].reserve_base = alloc_large_slab(ZONE_DEFAULT_RESERVE);
  zones[ret].ptr = zones[ret].reserve_base;
  zones[ret].reserve_end = zones[ret].reserve_base + ZONE_DEFAULT_RESERVE;
  zones[ret].commit_end = zones[ret].reserve_end;  // TODO: incrementally grow commit!
  printf("ZONE CREATE %d at %p\n", ret, zones[ret].ptr);
  return ret;
}

void zone_destroy(int zone) {
  CHECK(zone >= 0 && zone < COUNTOFI(zones));
  CHECK(zones[zone].ptr != NULL);
  free_large_slab(zones[zone].reserve_base, zones[zone].reserve_end - zones[zone].reserve_base);
  memset(&zones[zone], 0, sizeof(Zone));
}

int zone_set_default(int zone) {
  int ret = cur_zone;
  cur_zone = zone;
  CHECK(cur_zone >= 0 && cur_zone < COUNTOFI(zones));
  return ret;
}

void zone_free(void* ptr) {
  size_t data = GET_PREFIX_DATA(ptr);
#if 1  // TODO: DEBUG
  size_t size = data >> 8;
  memset(ptr, 0xdd, size);
#endif
  data &= ~0xff;
  data |= 0xfe;
  SET_PREFIX_DATA(ptr, data);
}

#define AllocAlign 8

void* zone_malloc(size_t len) {
  size_t len_with_prefix = ALIGN_UP(len + PREFIX_SIZE, AllocAlign);
  if (len_with_prefix >= MAX_ALLOC_SIZE) {
    return NULL;
  }
#if 0
  if (current_zone.ptr + len_with_prefix >= current_zone.commit_end) {
    // TODO: update commit more and update commit_end
  }
#endif
  void* ret = zones[cur_zone].ptr + PREFIX_SIZE;
  zones[cur_zone].ptr += len_with_prefix;
  size_t data = (len << 8) | 0xa1;
  SET_PREFIX_DATA(ret, data);
  printf("ZONE MALLOC %zu bytes -> %p\n", len, ret);
  return ret;
}

void* zone_realloc(void* old, size_t new_size) {
  if (!old) {
    return zone_malloc(new_size);
  }
  size_t old_size = GET_SIZE_FROM_PTR(old);
  if (new_size <= old_size) {
    return old;
  }
  void* p = zone_malloc(new_size);
  memcpy(p, old, old_size);
  zone_free(old);
  return p;
}

bool zone_ptr_is_valid(void* ptr) {
  for (int i = 0; i < COUNTOFI(zones); ++i) {
    if (zones[i].ptr != NULL) {
      if (ptr >= zones[i].reserve_base && ptr < zones[i].commit_end) {
        size_t data = GET_PREFIX_DATA(ptr);
        return (data & 0xff) == 0xa1;
      }
    }
  }
  return false;
}

size_t zone_ptr_alloc_size(void* ptr) {
  size_t data = GET_PREFIX_DATA(ptr);
  return data >> 8;
}

int zone_print_active_blocks(void) {
  int num_found = 0;
  void* p = zones[cur_zone].reserve_base;
  while (p < zones[cur_zone].ptr) {
    void* user_p = p + PREFIX_SIZE;
    size_t size = zone_ptr_alloc_size(user_p);
    if (zone_ptr_is_valid(user_p)) {
      printf("block allocated at %p of size %zu\n", user_p, size);
      ++num_found;
    }
    p += ALIGN_UP(size + PREFIX_SIZE, sizeof(void*));
  }
  return num_found;
}

static void at_exit_handler(void) {
  if (leak_check_backing_zone) {
    cur_zone = 0;
    int count = zone_print_active_blocks();
    if (count) {
      _Exit(0xbd);
    }
  }
}
