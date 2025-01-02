// Windows.h is too slow and we only need a few things so just define/extern
// them here.

typedef unsigned long DWORD, *LPDWORD, *PDWORD;
typedef long BOOL;
typedef void VOID, *PVOID, *LPVOID;
typedef void* HANDLE;
typedef const char *LPCSTR, *PCSTR;
typedef unsigned int UINT;
typedef long LONG;
typedef __int64 LONGLONG;
typedef unsigned __int64 ULONGLONG;
typedef unsigned __int64 ULONG_PTR;
typedef __int64 LONG_PTR;
typedef ULONG_PTR SIZE_T, *PSIZE_T;
typedef LONG_PTR SSIZE_T, *PSSIZE_T;
#define WINAPI __stdcall

#define CREATE_NEW 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define TRUNCATE_EXISTING 5

#define GENERIC_READ (0x80000000L)
#define GENERIC_WRITE (0x40000000L)
#define GENERIC_EXECUTE (0x20000000L)
#define GENERIC_ALL (0x10000000L)

#define FILE_SHARE_READ 0x00000001
#define FILE_SHARE_WRITE 0x00000002
#define FILE_SHARE_DELETE 0x00000004

#define FILE_ATTRIBUTE_NORMAL 0x00000080

#define PAGE_NOACCESS 0x01
#define PAGE_READONLY 0x02
#define PAGE_READWRITE 0x04
#define PAGE_WRITECOPY 0x08
#define PAGE_EXECUTE 0x10
#define PAGE_EXECUTE_READ 0x20
#define PAGE_EXECUTE_READWRITE 0x40
#define PAGE_EXECUTE_WRITECOPY 0x80
#define PAGE_GUARD 0x100
#define PAGE_NOCACHE 0x200
#define PAGE_WRITECOMBINE 0x400

#define MEM_COMMIT 0x00001000
#define MEM_RESERVE 0x00002000
#define MEM_DECOMMIT 0x00004000
#define MEM_RELEASE 0x00008000
#define MEM_FREE 0x00010000

#define INVALID_HANDLE_VALUE ((HANDLE)((LONG_PTR)-1))

typedef struct _SECURITY_ATTRIBUTES {
  DWORD nLength;
  LPVOID lpSecurityDescriptor;
  BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

typedef struct _OVERLAPPED {
  ULONG_PTR Internal;
  ULONG_PTR InternalHigh;
  union {
    struct {
      DWORD Offset;
      DWORD OffsetHigh;
    } DUMMYSTRUCTNAME;
    PVOID Pointer;
  } DUMMYUNIONNAME;

  HANDLE hEvent;
} OVERLAPPED, *LPOVERLAPPED;

typedef union _LARGE_INTEGER {
  struct {
    DWORD LowPart;
    LONG HighPart;
  } DUMMYSTRUCTNAME;
  struct {
    DWORD LowPart;
    LONG HighPart;
  } u;
  LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

HANDLE WINAPI CreateFileA(LPCSTR lpFileName,
                          DWORD dwDesiredAccess,
                          DWORD dwShareMode,
                          LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                          DWORD dwCreationDisposition,
                          DWORD dwFlagsAndAttributes,
                          HANDLE hTemplateFile);

BOOL WINAPI ReadFile(HANDLE hFile,
                     LPVOID lpBuffer,
                     DWORD nNumberOfBytesToRead,
                     LPDWORD lpNumberOfBytesRead,
                     LPOVERLAPPED lpOverlapped);

BOOL WINAPI CloseHandle(HANDLE hObject);

BOOL WINAPI GetFileSizeEx(HANDLE hFile, PLARGE_INTEGER lpFileSize);

LPVOID WINAPI VirtualAlloc(LPVOID lpAddress,
                           SIZE_T dwSize,
                           DWORD flAllocationType,
                           DWORD flProtect);

BOOL WINAPI VirtualProtect(LPVOID lpAddress,
                           SIZE_T dwSize,
                           DWORD flNewProtect,
                           PDWORD lpflOldProtect);

BOOL WINAPI VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);

NORETURN VOID WINAPI ExitProcess(UINT uExitCode);
