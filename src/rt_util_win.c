#if OS_WINDOWS

void* memmem(const void* haystack, size_t haystacklen, const void* needle, size_t needlelen) {
  const unsigned char* h = haystack;
  const unsigned char* n = needle;

  // Edge cases
  if (needlelen == 0) {
    return (void*)haystack;  // Empty needle matches at start
  }

  if (needlelen > haystacklen) {
    return NULL;  // Needle can't fit in haystack
  }

  // Search through haystack
  size_t search_len = haystacklen - needlelen + 1;

  for (size_t i = 0; i < search_len; i++) {
    // Check if needle matches at current position
    size_t j;
    for (j = 0; j < needlelen; j++) {
      if (h[i + j] != n[j]) {
        break;
      }
    }

    // If we checked all bytes and they matched
    if (j == needlelen) {
      return (void*)(h + i);
    }
  }

  return NULL;  // Not found
}

#endif
