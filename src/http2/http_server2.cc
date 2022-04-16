#include "http_server2.h"

#include <fcntl.h>

#include <kanon/util/any.h>
#include <kanon/util/macro.h>
#include <kanon/util/optional.h>

#include "unix/fd_wrapper.h"
#include "http_session.h"

using namespace kanon;

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

  // KANON_ASSERT(n == 1, "The <session, offset> must be erased only once");
}

kanon::optional<off_t> HttpServer::SearchOffset(HttpSession* session)
{
  auto iter = offset_map_.find(session);

  if (iter == std::end(offset_map_)) {
    return kanon::make_null_optional<off_t>();
  }

  return kanon::make_optional(iter->second);
}

std::shared_ptr<int> HttpServer::GetFd(std::string const& path)
{
  decltype(fd_map_)::iterator iter;

  MutexGuard guard(mutex_);
  iter = fd_map_.find(path);

  std::shared_ptr<int> ret(nullptr);

  if (iter != std::end(fd_map_)) {
    ret = iter->second.lock();

    assert(ret);
  }
  else {
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) {
      LOG_SYSERROR << "Failed to open " << path;
      return nullptr;
    }

    ret = std::shared_ptr<int>(new int(fd), [path, this](int* p) {
      {
        MutexGuard guard(mutex_);
        auto iter = fd_map_.find(path);

        assert(iter != std::end(fd_map_));
        fd_map_.erase(iter);
      }

      unix::FDWrapper wrapper(*p);
      delete p;
    });

    fd_map_.emplace(path, ret);
  }

  return ret;
}

} // namespace http