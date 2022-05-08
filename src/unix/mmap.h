#ifndef KANON_UNIX_MMAP_H
#define KANON_UNIX_MMAP_H

#include <sys/mman.h>
#include <fcntl.h>

#include <kanon/string/string_view.h>

namespace unix {

char* Mmap(int fd, size_t len, int prot, int flags, int offset = 0) noexcept;

char* Mmap(kanon::StringView path, size_t len, int prot, int flags, int offset = 0) noexcept;

bool Munmap(void* addr, size_t len) noexcept;

} // namespace unix

#endif // KANON_UNIX_MMAP_H