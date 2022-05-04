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

    if (conn->IsConnected()) {
      auto session = std::make_shared<HttpSession>(*this, conn);
      session->Setup();
      LOG_DEBUG << "[Session #" << session->GetId() << "] constructed";
      conn->SetContext(std::move(session));
      LOG_DEBUG << "Connection: " << conn.get() << "; Session: " << session.get();
    }
    else {
      auto session = *kanon::AnyCast<std::shared_ptr<HttpSession>>(conn->GetContext());
      LOG_DEBUG << "[Session #" << session->GetId() << "] destroyed";
      LOG_DEBUG << "Connection: " << conn.get() << "; Session: " << session.get();
      session.reset();
    }

  });

  LOG_DEBUG << "HttpServer " << this << " is created";
}

HttpServer::~HttpServer() noexcept
{
  LOG_FATAL << "HttpServer crash";
}

void HttpServer::EmplaceOffset(uint64_t session, off_t off)
{
  WLockGuard guard(lock_offset_);
  auto success = offset_map_.emplace(session, off);KANON_UNUSED(success);

  KANON_ASSERT(success.second, "Session has been emplaced?");
}

void HttpServer::EraseOffset(uint64_t session)
{
  WLockGuard guard(lock_offset_);
  auto n = offset_map_.erase(session);KANON_UNUSED(n);
}

kanon::optional<off_t> HttpServer::SearchOffset(uint64_t session)
{
  RLockGuard guard(lock_offset_);
  auto iter = offset_map_.find(session);

  if (iter == std::end(offset_map_)) {
    return kanon::make_null_optional<off_t>();
  }

  return kanon::make_optional(iter->second);
}

std::shared_ptr<int> HttpServer::GetFd(std::string const& path)
{
  MutexGuard guard(mutex_);
  
  auto& wptr = fd_map_[path];
  auto sp = wptr.lock();

  if (!sp) {
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) {
      LOG_SYSERROR << "Failed to open " << path;
      return nullptr;
    }

    sp.reset(new int(fd), [path, this](int* p) {
      // p maybe nullptr
      if (p) {
        {
        MutexGuard guard(mutex_);
        auto iter = fd_map_.find(path);
        
        if (iter != fd_map_.end() && iter->second.expired()) {
          fd_map_.erase(iter);
        }
        unix::FDWrapper wrapper(*p);
        }
      }

      delete p;
    });

    wptr = sp;
  }
  
  return sp;
}

} // namespace http
