#ifndef KANON_HTTP_SESSION_H
#define KANON_HTTP_SESSION_H

#include "kanon/util/noncopyable.h"
#include "kanon/net/user_server.h"
#include <kanon/string/string_view.h>

#include "common/types.h"
#include "common/http_response.h"
#include "common/http_constant.h"
#include "util/file.h"


namespace http {

class HttpSession : kanon::noncopyable {
private:
  using FilePtr = std::shared_ptr<File>;

  enum ParsePhase {
    kHeaderLine = 0,
    kHeaderFields,
    kFinished,
  };

  enum ParseResult {
    kGood = 0,
    kShort,
    kError,
  };

public:
  HttpSession();

  explicit HttpSession(kanon::TcpConnectionPtr const& conn);
  ~HttpSession() noexcept;

  // For debugging 
  void GetErrorResponse() const
  { GetClientError(meta_error_.first, meta_error_.second); }
private:
  using MetaError = std::pair<HttpStatusCode, kanon::StringView>;

  void OnMessage(TcpConnectionPtr const& conn, Buffer& buffer, TimeStamp recv);

  // void SendFile(FilePtr const& file);
  // void ServeFile(TcpConnectionPtr const& conn);
  // static void LastWriteComplete(kanon::TcpConnectionPtr const& conn);
  // static void CloseConnection(kanon::TcpConnectionPtr const& conn);

  ParseResult Parse(kanon::Buffer& buffer);

  ParseResult ParseHeaderLine(kanon::StringView header_line);
  ParseResult ParseComplexUrl();

  ParseResult ParseHeaderField(kanon::StringView header);
  void ParseMethod(kanon::StringView method) noexcept;
  void ParseVersionCode(int version_code) noexcept;

  template<size_t N>
  void FillMetaError(HttpStatusCode code, char const (&msg)[N]);

  template<size_t N>
  void FillMetaErrorOfBadRequest(char const (&msg)[N])
  { FillMetaError(HttpStatusCode::k400BadRequest, msg); }

  void Reset();

  // void SendErrorResponse(HttpStatusCode code, kanon::StringView msg);
  // void SendErrorResponse(HttpStatusCode code)
  // { SendErrorResponse(code, kanon::MakeStringView("")); }

  // HttpServer* server_;
  static char const kHomePage_[];
  static char const kRootPath_[];
  static char const kHtmlPath_[];
  static char const kHost_[]; 
  static constexpr int32_t kFileBufferSize_ = 1 << 16;

  kanon::TcpConnection* conn_;

  ParsePhase parse_phase_;
  MetaError meta_error_;

  bool is_static_;
  bool is_complex_;
  std::string url_;
  HeaderMap headers_;
  std::string query_;

  HttpMethod method_; 
  HttpVersion version_;

};

template<size_t N>
void HttpSession::FillMetaError(HttpStatusCode code, char const (&msg)[N])
{
  meta_error_ = MetaError(code, kanon::MakeStringView(msg));
}

} // namespace http

#endif // KANON_HTTP_SESSION_H