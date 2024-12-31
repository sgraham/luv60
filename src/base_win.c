static ReadFileResult base_read_file(const char* filename) {
  SECURITY_ATTRIBUTES sa = {sizeof(sa), 0, 0};
  HANDLE file = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, &sa,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  if (file == INVALID_HANDLE_VALUE) {
    return (ReadFileResult){0};
  }
  LARGE_INTEGER size;
  GetFileSizeEx(file, &size);

  size_t to_alloc = ALIGN_UP(size.QuadPart + 64, PAGE_SIZE);
  unsigned char* read_buf = VirtualAlloc(NULL, to_alloc, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  if (!read_buf) {
    return (ReadFileResult){0};
  }

  DWORD bytes_read = 0;
  if (!ReadFile(file, read_buf, size.QuadPart, &bytes_read, NULL)) {
    VirtualFree(read_buf, 0, MEM_RELEASE);
    return (ReadFileResult){0};
  }
  if (bytes_read != size.QuadPart) {
    VirtualFree(read_buf, 0, MEM_RELEASE);
    return (ReadFileResult){0};
  }
  CloseHandle(file);

  return (ReadFileResult){read_buf, size.QuadPart, to_alloc};
}

static void base_exit(int rc) {
  ExitProcess(rc);
}
