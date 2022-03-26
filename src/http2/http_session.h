#ifndef KANON_HTTP_SESSION_H
#define KANON_HTTP_SESSION_H

#include <kanon/util/noncopyable.h>
#include <kanon/util/optional.h>
#include <kanon/net/user_server.h>
#include <kanon/net/timer/timer_id.h>
#include <kanon/string/string_view.h>
#include <kanon/thread/atomic_counter.h>

#include "common/types.h"
#include "common/http_response.h"
#include "common/http_constant.h"
#include "util/file.h"


namespace http {

class HttpServer;

class HttpSession : kanon::noncopyable {
private:
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

  explicit HttpSession(HttpServer& server, kanon::TcpConnectionPtr const& conn);
  ~HttpSession() noexcept;

  // For debugging 
  void GetErrorResponse() const
  { GetClientError(meta_error_.first, meta_error_.second); }

  uint32_t GetId() const noexcept
  { return id_; }

private:
  using MetaError = std::pair<HttpStatusCode, kanon::StringView>;

  void OnMessage(TcpConnectionPtr const& conn, Buffer& buffer, TimeStamp recv);

  // Static contents
  void ServeFile();
  void SendFile(int fd);

  // Dynamic contents
  void ServeDynamicContent();
  ArgsMap SplitArgs();

  void CloseConnection();
  void NotImplementation();

  // Parse http request
  ParseResult Parse(kanon::Buffer& buffer);
  ParseResult ParseHeaderLine(kanon::StringView header_line);
  ParseResult ParseComplexUrl();
  ParseResult ParseHeaderField(kanon::StringView header);
  void ParseMethod(kanon::StringView method) noexcept;
  void ParseVersionCode(int version_code) noexcept;
  void Reset();

  // Error handling
  template<size_t N>
  void FillMetaError(HttpStatusCode code, char const (&msg)[N]);

  template<size_t N>
  void FillMetaErrorOfBadRequest(char const (&msg)[N])
  { FillMetaError(HttpStatusCode::k400BadRequest, msg); }

  void SendErrorResponse();

  HttpServer* server_;
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
  bool is_keep_alive_;

  kanon::optional<kanon::TimerId> timer_id_;

  // For debugging
  uint32_t id_;
  static kanon::AtomicCounter32 counter_;

  static constexpr int32_t kFileBufferSize_ = 1 << 16;
};

template<size_t N>
void HttpSession::FillMetaError(HttpStatusCode code, char const (&msg)[N])
{
  meta_error_ = MetaError(code, kanon::MakeStringView(msg));
}

} // namespace http

#endif // KANON_HTTP_SESSION_H