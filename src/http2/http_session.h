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
#include "unix/stat.h"
#include "http_error.h"
#include "http_request.h"

namespace http {

class HttpServer;

class HttpSession : kanon::noncopyable {
 public:
  HttpSession();

  HttpSession(HttpServer& server, kanon::TcpConnectionPtr const& conn);
  ~HttpSession() noexcept;

  // For debugging 
  uint32_t GetId() const noexcept
  { return id_; }

  void Setup();
  void Teardown();
private:

  void OnMessage(TcpConnectionPtr const& conn, Buffer& buffer, TimeStamp recv);

  // Static contents
  void ServeFile(HttpRequest const& request);
  bool SendFile(std::shared_ptr<int> const& fd, HttpRequest const& request);
  bool SendFileOfMmap(std::shared_ptr<char*> const& addr, HttpRequest const& request);

  // Dynamic contents
  void ServeDynamicContent(HttpRequest const& request);

  void SetLastWriteComplete(HttpRequest const& request);
  void CloseConnection(HttpRequest const& request);
  void NotImplementation(HttpRequest const& request);

  // Log 
 
  void LogError();
  void LogClose();
  void LogRequest(HttpRequest const& request);

  // Error handling

  void SetErrorOfGetFdOrGetAddr(HttpRequest const& request);
  bool SetErrorOfStat(unix::Stat& stat, bool success);

  void SendErrorResponse();

  // Timer control
  void CancelConnectionTimeoutTimer();
  void CancelKeepAliveTimer();

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
   * Error metadata, used to construct error response
   */
  HttpError error_{.code=HttpStatusCode::k400BadRequest};

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

  uint64_t cur_filesize_ = 0; 
  uint64_t cache_filesize_ = 0;

  // For debugging
  uint32_t id_;
  static kanon::AtomicCounter32 counter_;

  static constexpr int32_t kFileBufferSize_ = 1 << 16;
};

} // namespace http

#endif // KANON_HTTP_SESSION_H
