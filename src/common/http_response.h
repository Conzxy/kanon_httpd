#ifndef KANON_HTTP_RESPONSE_H
#define KANON_HTTP_RESPONSE_H

#include <stdarg.h>

#include "http_constant.h"

#include "kanon/string/string_view.h"
#include "kanon/util/macro.h"
#include "kanon/util/mem.h"
#include "kanon/util/noncopyable.h"
#include "kanon/net/buffer.h"
#include "kanon/util/type.h"

namespace http {

/**
 * Construct a http response
 */
class HttpResponse {
public:
  using Self = HttpResponse;

  /**
   * \param known_length If length is unknown, will computed by HttpResponse
   */
  explicit HttpResponse(const bool known_length = false)
    : buffer_()
    , known_length_(known_length)
  { 
    if (!known_length) {
      body_.reserve(4096);
    }
  }

  Self& AddHeaderLine(HttpStatusCode code, HttpVersion ver) {
    AddVersion(ver);
    AddStatusCodeAndString(code);
    return *this;
  }

  Self& AddHeaderLine(HttpStatusCode code) {
    return AddHeaderLine(code, HttpVersion::kHttp11);    
  }

  /**
   * Add header in response line
   * including its field and content.
   * @param field field name
   * @param content corresponding description
   */
  Self& AddHeader(kanon::StringArg field,
                  kanon::StringArg content) {
    char buf[256];

    ::snprintf(buf, sizeof buf, "%s: %s\r\n", field.data(), content.data());

    buffer_.Append(buf);
    return *this;
  }

  Self& AddContentType(kanon::StringView filename) {
    auto val = GetFileType(filename);
    if (val[0] != 0) {
      return AddHeader("Content-Type", val);
    }

    return *this;
  }

  Self& AddChunkedTransferHeader() {
    // Disable length header
    known_length_ = true;
    chunked = true;
    return AddHeader("Transfer-Encoding", "chunked");
  }

  /**
   * Add blankline after header lines.
   */
  Self& AddBlackLine() {
    if (known_length_) {
      buffer_.Append("\r\n");
    }

    return *this;
  }

  /**
   * Add body contents.
   * @param content body content
   */
  Self& AddBody(kanon::StringView content) {
    return AddBody(content.data(), content.size());
  }

  Self& AddBody(char const* data, size_t len) {
    if (known_length_) {
      buffer_.Append(data, len);
    }
    else {
      body_.insert(body_.end(), data, data + len);
    }

    return *this;
  }

  Self& AddBody(char* buf, size_t len, char const* fmt, ...) {
    va_list vl;
    va_start(vl, fmt);
    auto n = ::vsnprintf(buf, len, fmt, vl); 
    va_end(vl);
    AddBody(kanon::StringView(buf, n));
    return *this;
  }


  Self& AddChunk(char const* data, size_t len) {
    assert(chunked);
    buffer_.Append(Dec2Hex(len));
    buffer_.Append("\r\n");
    buffer_.Append(data, len);
    buffer_.Append("\r\n");
    return *this;
  }

  Self& AddChunk(char* buf, size_t len, char const* fmt, ...) {
    va_list vl;

    va_start(vl, fmt);
    auto n = ::vsnprintf(buf, len, fmt, vl); 
    va_end(vl);

    AddChunk(buf, n);
    return *this;
  }

  Self& AddChunk(kanon::StringView data) {
    return AddChunk(data.data(), data.size());
  }

  size_t GetBodySize() const noexcept { return body_.size(); }
  kanon::Buffer& GetBuffer();

private:
  /**
   * Add version code in response line.
   * Only support HTTP0.0 and HTTP1.1.
   * @param ver valid http version code
   */
  Self& AddVersion(HttpVersion ver) {
    buffer_.Append(GetHttpVersionString(ver));
    return *this;
  }

  /**
   * Add status code in response line
   * and corresponding description string.
   * @param code valid status code
   */ 
  Self& AddStatusCodeAndString(HttpStatusCode code) {
    char buf[255];
    snprintf(buf, sizeof buf, " %d %s\r\n",
        GetStatusCode(code), GetStatusCodeString(code));
    
    buffer_.Append(static_cast<char const*>(buf));
    return *this;
  }

  static std::string Dec2Hex(size_t num);
  static char const* GetFileType(kanon::StringView filename);

  kanon::Buffer buffer_;
  std::vector<char> body_;
  bool known_length_ = false;
  bool chunked = false;
};

HttpResponse GetClientError(
  HttpStatusCode status_code,
  kanon::StringView msg);

} // namespace http

#endif // KANON_HTTP_RESPONSE_H