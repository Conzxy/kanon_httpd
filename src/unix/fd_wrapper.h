#ifndef KANON_UNIX_FILE_DESCRIPTOR_H
#define KANON_UNIX_FILE_DESCRIPTOR_H

#include <kanon/util/noncopyable.h>
#include <unistd.h>

namespace unix {

// This is a simple fd wrapper
// Just support pread(2) and pwrite(2)
class FDWrapper {
public:
  explicit FDWrapper(int fd) 
   : fd_(fd)
  {

  }

  ~FDWrapper() noexcept 
  {
    ::close(fd_);
  }
private:
  int fd_;
  DISABLE_EVIL_COPYABLE(FDWrapper)
};

} // namespace unix

#endif // 