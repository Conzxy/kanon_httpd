#ifndef KANON_HTTP_SESSION_H
#define KANON_HTTP_SESSION_H

#include <kanon/net/buffer.h>
#include <kanon/net/callback.h>
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

class HttpSession : 
  public std::enable_shared_from_this<HttpSession>,
  kanon::noncopyable {
private:
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

public:
  HttpSession();

  explicit HttpSession(HttpServer& server, kanon::TcpConnectionPtr const& conn);
  ~HttpSession() noexcept;

  // For debugging 
  void GetErrorResponse() const
  { GetClientError(meta_error_.first, meta_error_.second); }

  uint32_t GetId() const noexcept
  { return id_; }

  void Setup();
private:
  using MetaError = std::pair<HttpStatusCode, kanon::StringView>;

  void OnMessage(TcpConnectionPtr const& conn, Buffer& buffer, TimeStamp recv);

  // Static contents
  void ServeFile();
  bool SendFile(int fd);

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
  ParseResult ExtractBody(kanon::Buffer& buffer);
  void ParseMethod(kanon::StringView method) noexcept;
  void ParseVersionCode(int version_code) noexcept;
  void Reset();
  void SetHeaderMetadata();

  uint64_t Str2U64(std::string const& str);

  // Error handling
  template<size_t N>
  void FillMetaError(HttpStatusCode code, char const (&msg)[N]);
  template<size_t N>
  void FillMetaErrorOfBadRequest(char const (&msg)[N])
  { FillMetaError(HttpStatusCode::k400BadRequest, msg); }
  void SendErrorResponse();

  // Timer control
  void CancelConnectionTimeoutTimer();
  void CancelKeepAliveTimer();

  // Data
  /**
   * To access the data structure maintained by server
   */
  HttpServer* server_;

  /**
   * A session in fact is a connection
   * To simplify the function parameter
   */
  TcpConnectionPtr conn_;

  /**
   * Main state machine metadata
   */
  ParsePhase parse_phase_;

  /**
   * Error metadata, used to construct error response
   */
  MetaError meta_error_;

  /**
   * The metadata for parsing header line of a http request
   */
  bool is_static_; /** Static page */
  bool is_complex_; /** Complex URL, e.g. %Hex Hex */
  std::string url_; /** The URL part */
  std::string query_; /** query string */
  HttpMethod method_; /** method of header line */
  HttpVersion version_; /** version code of header line */

  /**
   * Store the header fields
   */
  HeaderMap headers_;

  /**
   * Store the body of a http request
   */
  std::string body_;

  /** 
   * To support long connection to reuse this connection,
   * set a timer, it will close this connection if 
   * peer don't send any message in 10s.
   */
  kanon::optional<kanon::TimerId> keep_alive_timer_id_;

  /** 
   * If a new connection is established and don't send any
   * message in 60s,  just close it.
   */
  kanon::optional<kanon::TimerId> connection_timer_id_;

  bool is_keep_alive_; /** Determine if a keep-alive connection */

  /**
   * Due to short read, we should cache content length
   * in case search the fields in headers_
   */
  uint64_t content_length_;
  
  uint64_t cur_filesize_ = 0; 
  uint64_t cache_filesize_ = 0;
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
