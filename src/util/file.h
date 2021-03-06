#ifndef KANON_HTTP_FILE_DESCRIPTOR_H
#define KANON_HTTP_FILE_DESCRIPTOR_H

#include <stdio.h>
#include <string>
#include <stddef.h>
#include <stdexcept>

#include "kanon/util/noncopyable.h"

namespace http {

struct FileException : std::runtime_error {
  explicit FileException(char const* msg)
    : std::runtime_error(msg)
  {
  }

  explicit FileException(std::string const& msg) 
    : std::runtime_error(msg)
  {
  }
};

class File : kanon::noncopyable {
public:
  enum OpenMode {
    kRead = 0x1,
    kWriteBegin = 0x2,
    kTruncate = 0x4,
    kAppend = 0x8,
  };

  File()
    : fp_(NULL)
    , mode_(0)
    , eof_(false)
  { }

  File(char const* filename, int mode);
  File(std::string const& filename, int mode);

  bool Open(std::string const& filename, int mode)
  {
    return Open(filename.data(), mode);
  }

  bool Open(char const* filename, int mode);

  ~File() noexcept;

  size_t Read(void* buf, size_t len);

  template<size_t N>
  size_t Read(char(&buf)[N])
  {
    return Read(buf, N);
  }

  bool ReadLine(std::string& line, const bool need_newline = true);

  void Write(char const* buf, size_t len) noexcept {
    ::fwrite(buf, 1, len, fp_);
  }

  bool IsValid() const noexcept { return fp_ != NULL; }

  bool IsEof() const noexcept { return eof_; }

  void Reset() noexcept { eof_ = false; }
  void Rewind() noexcept { ::rewind(fp_); }

  void SeekCurrent(long offset) noexcept { Seek(offset, SEEK_CUR); }
  void SeekBegin(long offset) noexcept { Seek(offset, SEEK_SET); }
  void SeekEnd(long offset) noexcept { Seek(offset, SEEK_END); }
  long GetCurrentPosition() noexcept { return ::ftell(fp_); }

  size_t GetSize() noexcept;

  static const size_t kInvalidReturn = static_cast<size_t>(-1);
private:
  void Seek(long offset, int whence) noexcept { ::fseek(fp_, offset, whence); }

  FILE* fp_;
  int mode_;
  bool eof_;
};


} // namespace http

#endif // KANON_HTTP_FILE_DESCRIPTOR_H
