#pragma once

typedef struct ReadFileResult {
  unsigned char* buffer;
  size_t file_size;
  size_t allocated_size;
} ReadFileResult;

int base_writef_stderr(const char* fmt, ...);
uint64_t base_page_size(void);
void *base_mem_reserve(uint64_t size);
bool base_mem_commit(void* ptr, uint64_t size);
void *base_mem_large_alloc(uint64_t size);
void base_mem_decommit(void* ptr, uint64_t size);
void base_mem_release(void* ptr, uint64_t size);
ReadFileResult base_read_file(const char* filename);
NORETURN void base_exit(int rc);
void base_timer_init(void);
uint64_t base_timer_now(void);
