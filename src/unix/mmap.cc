#include "unix/mmap.h"

#include <unistd.h>

using namespace kanon;

namespace unix {

char* Mmap(int fd, size_t len, int prot, int flags, int offset) noexcept {
  char* addr = reinterpret_cast<char*>(::mmap(NULL, len, prot, flags, fd, offset));

  if (addr == MAP_FAILED) {
    return nullptr;
  }

  ::close(fd);
  return addr;
}

char* Mmap(StringView path, size_t len, int prot, int flags, int offset) noexcept {
  int fd = ::open(path.data(), O_RDONLY);

  return Mmap(fd, len, prot, flags, offset);
}

bool Munmap(void *addr, size_t len) noexcept {
  return ::munmap(addr, len) == 0;
}

} // namespace mmap