#include "http_server2.h"

#include <kanon/util/any.h>
#include <kanon/util/macro.h>
#include <kanon/util/optional.h>

#include "http_session.h"

namespace http {

HttpServer::HttpServer(EventLoop* loop, InetAddr const& addr)
  : TcpServer(loop, addr, "HttpServer")
{
  SetConnectionCallback([this](TcpConnectionPtr const& conn) {
    HttpSession* session = nullptr;

    if (conn->IsConnected()) {
      session = new HttpSession(*this, conn);
      LOG_TRACE << "[Session #" << session->GetId() << "] constructed";
      conn->SetContext(session);
    }
    else {
      session = *kanon::AnyCast<HttpSession*>(conn->GetContext());
      LOG_TRACE << "[Session #" << session->GetId() << "] destroyed";
      delete session; 
    }

  });
}

HttpServer::~HttpServer() noexcept
{
}

void HttpServer::EmplaceOffset(HttpSession* session, off_t off)
{
  auto success = offset_map_.emplace(session, off);KANON_UNUSED(success);

  KANON_ASSERT(success.second, "Session has been emplaced?");
}

void HttpServer::EraseOffset(HttpSession* session)
{
  auto n = offset_map_.erase(session);KANON_UNUSED(n);

  KANON_ASSERT(n == 1, "The <session, offset> must be erased only once");
}

kanon::optional<off_t> HttpServer::SearchOffset(HttpSession* session)
{
  auto iter = offset_map_.find(session);

  if (iter == std::end(offset_map_)) {
    return kanon::make_null_optional<off_t>();
  }

  return kanon::make_optional(iter->second);
}

} // namespace http