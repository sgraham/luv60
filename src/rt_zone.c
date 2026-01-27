#define ZONE_HEADER_SIZE 128
typedef struct Zone {
  uint64_t original_commit_size;
  uint64_t original_reserve_size;
  uint64_t cur_pos;
  uint64_t cur_commit;
  uint64_t cur_reserve;
} Zone;

_Static_assert(sizeof(Zone) < ZONE_HEADER_SIZE, "Zone too large");

#ifdef _WIN32

// Both kernel32.dll, but we don't want windows.h.
extern void* __stdcall VirtualAlloc(void* addr, size_t size, int alloc_type, int protect);
extern int __stdcall VirtualFree(void* addr, size_t size, int free_type);

#define MEM_COMMIT 0x00001000
#define MEM_RESERVE 0x00002000
#define PAGE_READWRITE 0x04
#define MEM_RELEASE 0x00008000

uint64_t base_page_size(void) {
  SYSTEM_INFO sysInfo;
  GetSystemInfo(&sysInfo);
  return sysInfo.dwPageSize;
}

static void* base_mem_reserve(uint64_t size) {
  return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
}

static bool base_mem_commit(void* ptr, uint64_t size) {
  return VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != 0;
}

static void base_mem_release(void* ptr, uint64_t size) {
  VirtualFree(ptr, 0, MEM_RELEASE);
}

#else

#include <sys/mman.h>
#include <unistd.h>

static uint64_t base_page_size(void) {
  return sysconf(_SC_PAGE_SIZE);
}

void* base_mem_reserve(uint64_t size) {
  void* result = mmap(0, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (result == MAP_FAILED) {
    result = NULL;
  }
  return result;
}

bool base_mem_commit(void* ptr, uint64_t size) {
  mprotect(ptr, size, PROT_READ|PROT_WRITE);
  return true;
}

void base_mem_guard_no_access(void* ptr, uint64_t size) {
  mprotect(ptr, size, PROT_NONE);
}

void base_mem_guard_read_write(void* ptr, uint64_t size) {
  mprotect(ptr, size, PROT_READ|PROT_WRITE);
}

void base_mem_release(void* ptr, uint64_t size) {
  munmap(ptr, size);
}

#endif

#define ZONE_DEFAULT_RESERVE (128*1024*1024)
#define ZONE_DEFAULT_COMMIT (16*1024)

#define ALIGN_DOWN(n, a) ((n) & ~((a)-1))
#define ALIGN_UP(n, a) ALIGN_DOWN((n) + (a)-1, (a))

#define MIN(x, y) ((x) <= (y) ? (x) : (y))
#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#define CLAMP_MAX(x, max) MIN(x, max)
#define CLAMP_MIN(x, min) MAX(x, min)

Zone* zone_create(void) {
  // TODO: probably make these args
  uint64_t provided_reserve_size = ZONE_DEFAULT_RESERVE;
  uint64_t provided_commit_size = ZONE_DEFAULT_COMMIT;

  uint64_t commit_size = ALIGN_UP(provided_commit_size, base_page_size());
  uint64_t reserve_size = ALIGN_UP(provided_reserve_size, base_page_size());

  void* base = base_mem_reserve(reserve_size);
  RT_CHECK(base);
  RT_CHECK(base_mem_commit(base, commit_size));

  Zone* zone = base;
  zone->original_commit_size = provided_commit_size;
  zone->original_reserve_size = provided_reserve_size;
  zone->cur_pos = ZONE_HEADER_SIZE;
  zone->cur_commit = commit_size;
  zone->cur_reserve = reserve_size;

  // TODO: ASAN

  //fprintf(stderr, "ZONE %p, %" PRIu64 " reserve %" PRIu64 " commit\n", zone, reserve_size, commit_size);
  return zone;
}

void zone_destroy(Zone* zone) {
  base_mem_release(zone, zone->cur_reserve);
}

void* zone_push(Zone* zone, uint64_t size, uint64_t align) {
  uint64_t pos_pre = ALIGN_UP(zone->cur_pos, align);
  uint64_t pos_post = pos_pre + size;
  //fprintf(stderr, "ZONE %p, at %" PRIu64 ", push %" PRIu64 "\n", zone, zone->cur_pos, size);

  // Extend committed range, if necessary.
  if (zone->cur_commit < pos_post) {
    uint64_t commit_post_aligned = pos_post + zone->original_commit_size - 1;
    commit_post_aligned -= commit_post_aligned % zone->original_commit_size;
    uint64_t commit_post_clamped = CLAMP_MAX(commit_post_aligned, zone->cur_reserve);
    uint64_t commit_size = commit_post_clamped - zone->cur_commit;
    uint8_t* commit_ptr = (uint8_t*)zone + zone->cur_commit;
    base_mem_commit(commit_ptr, commit_size);
    zone->cur_commit = commit_post_clamped;
  }

  void* result = NULL;
  if (zone->cur_commit >= pos_post) {
    result = (uint8_t*)zone + pos_pre;
    zone->cur_pos = pos_post;
  }

  RT_CHECK(result != NULL);

  // TODO: ASAN

  return result;
}

uint64_t zone_pos(Zone* zone) {
  return zone->cur_pos;
}

void zone_guard_no_access(Zone* zone) {
  uint8_t* ptr = (uint8_t*)zone;
  base_mem_guard_no_access(ptr, ALIGN_DOWN(zone->cur_commit, base_page_size()));
}

void zone_guard_read_write(Zone* zone) {
  uint8_t* ptr = (uint8_t*)zone;
  base_mem_guard_read_write(ptr, base_page_size());
  base_mem_guard_read_write(ptr, ALIGN_DOWN(zone->cur_commit, base_page_size()));
}

void zone_pop_to(Zone* zone, uint64_t pos) {
  uint64_t cpos = CLAMP_MIN(ZONE_HEADER_SIZE, pos);
  zone->cur_pos = cpos;
  //memset(((uint8_t*)zone + zone->cur_pos), 0xdd, 128);
  //fprintf(stderr, "ZONE %p, pop to %" PRIu64 "\n", zone, zone->cur_pos);
  //  TODO: ASAN
}
