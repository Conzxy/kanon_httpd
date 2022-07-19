#ifndef _KANON_HTTPD_HTTP_PARSER_H_
#define _KANON_HTTPD_HTTP_PARSER_H_

#include <kanon/util/noncopyable.h>
#include <kanon/net/buffer.h>
#include <kanon/log/logger.h>
#include <kanon/string/string_view.h>

#include "common/http_constant.h"
#include "http_request.h"
#include "http_error.h"

namespace http {

using kanon::StringView;
using kanon::Buffer;

class HttpParser : kanon::noncopyable {
 public:
  enum ParsePhase {
    kHeaderLine = 0,
    kHeaderFields,
    kBody,
    kFinished,
  };

  enum ParseResult {
    kGood = 0,
    kShort,
    kError,
  };

  // Parse http request
  ParseResult Parse(Buffer& buffer, HttpRequest* request); 

  HttpError const& error() const noexcept {
    return error_;
  }

  HttpError& error() noexcept {
    return error_;
  }
 private:
  ParseResult ParseHeaderLine(StringView line, HttpRequest* request);

  ParseResult ParseComplexUrl(HttpRequest* request);
  ParseResult ParseHeaderField(StringView header, HttpRequest* request);
  ParseResult ExtractBody(Buffer& buffer, HttpRequest* request);

  void ParseMethod(StringView method, HttpRequest* request) noexcept {
    if (!method.compare("GET")) {
      request->method = HttpMethod::kGet;
    } 
    else if (!method.compare("POST")) {
      request->method = HttpMethod::kPost;
    } 
    else if (!method.compare("PUT")) {
      request->method = HttpMethod::kPut;
    } 
    else if (!method.compare("HEAD")) {
      request->method = HttpMethod::kHead;
    } 
    else {
      request->method = HttpMethod::kNotSupport;
    }
  }

  void ParseVersionCode(int version_code, HttpRequest* request) noexcept {
    switch (version_code) {
      case 101:
        request->version = HttpVersion::kHttp11;
        break;
      case 100:
        request->version = HttpVersion::kHttp10;
        break;
      default:
        request->version = HttpVersion::kNotSupport;
    }
  }

  void Reset() noexcept {
    parse_phase_ = kHeaderLine;
    content_length_ = -1;
  }

  void SetHeaderMetadata(HttpRequest* request) {
    auto iter = request->headers.find("Connection");

    switch (request->version) {
    case HttpVersion::kHttp10:
      // In 1.0, default is close
      if (iter != request->headers.end() && !::strncasecmp(iter->second.c_str(), "keep-alive", iter->second.size())) {
        request->is_keep_alive = true;
        LOG_DEBUG << "The connection is keep-alive";
      } else {
        request->is_keep_alive = false;
        LOG_DEBUG << "The connection is close";
      }
      break;

    case HttpVersion::kHttp11:
      // In 1.1, default is keep-alive
      if (iter != request->headers.end() && !::strncasecmp(iter->second.c_str(), "close", iter->second.size())) {
        request->is_keep_alive = false;
        LOG_DEBUG << "The connection is close";
      } else {
        request->is_keep_alive = true;
        LOG_DEBUG << "The connection is keep-alive";
      }
      break;

    case HttpVersion::kNotSupport:
    default:
      request->is_keep_alive = false;
      LOG_DEBUG << "The connection is keep-alive";

    }

    iter = request->headers.find("Content-Length");

    if (iter != std::end(request->headers)) {
      content_length_ = ::strtoull(iter->second.c_str(), NULL, 10);
      LOG_DEBUG << "The Content-Length = " << content_length_;
    }
  }
  
 private:
  /**
   * Main state machine metadata
   */
  ParsePhase parse_phase_ = kHeaderLine;

  /**
   * Due to short read, we should cache content length
   * in case search the fields in headers_
   */
  uint64_t content_length_ = -1;

  HttpError error_ = { .code = HttpStatusCode::k400BadRequest };
};

} // http

#endif // _KANON_HTTPD_HTTP_PARSER_H_
