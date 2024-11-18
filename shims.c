#include <assert.h>
#include <stdint.h>

#include "shims.h"

#if defined(_WIN32)

#include "safewindows.h"

int getpagesize(void) {
  SYSTEM_INFO si;
  GetSystemInfo(&si);

  DWORD page_size = si.dwPageSize;
  assert(page_size <= INT_MAX);

  return (int)page_size;
}

#endif // defined(_WIN32)