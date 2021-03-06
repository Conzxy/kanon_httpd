#include "file.h"

#include <cstdio>
#include <assert.h>
#include <stdexcept>
#include <string.h>

namespace http {

File::File(char const* filename, int mode)
  : fp_(NULL)
  , mode_(mode)
  , eof_(false)
{
  if (false == Open(filename, mode)) {
    std::string buf;
    buf.reserve(strlen(filename) + 32);
    ::snprintf(&*buf.begin(), buf.capacity(),
     "Failed to open file: %s",
      filename);

    throw FileException(buf);
  }
}

File::File(std::string const& filename, int mode)
  : fp_(NULL)
  , mode_(mode)
  , eof_(false)
{  
  if (false == Open(filename, mode)) {
    std::string buf;
    buf.reserve(filename.size() + 32);
    buf = "Failed to open file: ";
    buf += filename;

    throw FileException(buf);
  }
}

File::~File() noexcept
{
  if (fp_ != NULL) {
    ::fclose(fp_);
  }
}

bool File::Open(char const* filename, int mode)
{
  if (mode & kWriteBegin) {
    fp_ = ::fopen(filename, "r+");
  }
  else if (mode & kRead) {
    if (mode & kAppend) {
      fp_ = ::fopen(filename, "a+");
    }
    else if (mode & kTruncate) {
      fp_ = ::fopen(filename, "w+");
    }
    else {
      fp_ = ::fopen(filename, "r");
    }
  }
  else if (mode & kAppend) {
    fp_ = ::fopen(filename, "a");
  }
  else if (mode & kTruncate) {
    fp_ = ::fopen(filename, "w");
  }

  return fp_ != NULL;
}

size_t File::Read(void* buf, size_t len)
{
  // According the description of fread,
  // this is maybe not necessary.
  // But regardless of whether it does or not something like readn,
  // We should make sure read complete even if there is no short read occurred.

  auto p = reinterpret_cast<char*>(buf);
  size_t remaining = len;
  size_t readn = 0;

  while (remaining > 0) {
    readn = ::fread(p, 1, remaining, fp_);

    if (readn < len) {
      // Error occcurred or eof
      if (::feof(fp_) != 0) {
        remaining -= readn;
        eof_ = true;
        break;
      }
      else if (::ferror(fp_) != 0){
        if (errno == EINTR) {
          continue;
        }
      }

      return -1;
    }
    else {
      p += readn;
      remaining -= readn;
    }
  }

  return len - remaining;
}

bool File::ReadLine(std::string& line, const bool need_newline)
{
  char buf[4096];
  char* ret = NULL;
  size_t n = 0;

  line.clear();

  do {
      ret = ::fgets(buf, sizeof buf, fp_);

      if (ret == NULL) {
        if (::ferror(fp_)) {
          if (errno == EINTR) {
            continue;
          }
        }
        else if (::feof(fp_)) {
          eof_ = true;
        }

        return false;
      }

      assert(ret == buf);
      n = strlen(buf);

      if (n >= 1 && buf[n-1] == '\n') {
        if (!need_newline) {
          if (n >= 2 && buf[n-2] == '\r') {
            line.append(buf, n - 2);
          }
          else {
            line.append(buf, n - 1);
          }
        } else {
          line.append(buf, n);
        }
        break;
      }

      line.append(buf, n);

  } while (n == (sizeof(buf) - 1));

  return true;
}

size_t File::GetSize() noexcept
{
  SeekEnd(0);
  const auto ret = GetCurrentPosition();
  Rewind();

  return ret;
}
} // namespace http
