#include "http_server2.h"

#include <fcntl.h>

#include <kanon/util/any.h>
#include <kanon/util/macro.h>
#include <kanon/util/optional.h>

#include "unix/fd_wrapper.h"
#include "unix/mmap.h"

#include "http2/http_session.h"

using namespace kanon;
using namespace unix;

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
    }
    else {
      // Here, Don't use:
      //   auto session = *kanon::AnyCast<std::shared_ptr<HttpSession>>(conn->GetContext());
      //   session.reset();
      // This do nothing, the ref-count no change.
      //   session --> connection
      //   connection --> session (dead lock)
      // Solution:
      // 1. Use auto&
      // 2. Set empty context to desctory shared_ptr
      //    conn->SetContext();
      auto& session = *kanon::AnyCast<std::shared_ptr<HttpSession>>(conn->GetContext());
      LOG_DEBUG << "[Session #" << session->GetId() << "] destroyed";
      conn->SetWriteCompleteCallback(WriteCompleteCallback());
      session->Teardown();
      LOG_DEBUG << "ref-count = " << session.use_count();

      session.reset();
    }

  });

  LOG_DEBUG << "HttpServer " << this << " is created";
}

HttpServer::~HttpServer() noexcept
{
}

std::shared_ptr<int> HttpServer::GetFd(std::string const& path)
{
  /* 
   * The race condition between GetFd() and deleter:
   * Call the deleter but lock is preempted by other thread calling GetFd()
   * 1. Deleter must ensure remove entry when the ref-count == 0
   *    Otherwise, deleter may remove a new entry with same key which created in 
   *    other thread, then Call the GetFd() will get different entry and all state is missing.
   *    +---------------------+---------------------+
   *    | Thread 1            | Thread2             |
   *    | deleter C           |                     |
   *    |                     | GetFd() A           |
   *    | find success        |                     |
   *    | remove A            |                     | <--- Must check ref-count!
   *    |                     | GetFd() B(new entry)|
   *    +---------------------+---------------------+
   *    Solution:
   *    Use std::weak_ptr::expired()
   *    Don't use std::weak_ptr::lock()
   * 2. Don't use assert(find != end) in the deleter
   *    +---------------------+---------------------+
   *    | Thread 1            | Thread2             |
   *    | deleter A           |                     |
   *    |                     | GetFd() B           |
   *    |                     | deleter B           |
   *    | assert failed       |                     |
   *    +---------------------+---------------------+
   *    The scenario maybe occurred when GetFd() don't switch to other thread
   */
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

        delete p;
      }
    });

    wptr = sp;
  }
  
  return sp;
}

std::shared_ptr<char*> HttpServer::GetAddr(std::string const& pathname, size_t len) {
  MutexGuard guard(mutex_addr_);

  auto& wp = addr_map_[pathname];
  auto sp = wp.lock();

  if (!sp) {
    int fd = ::open(pathname.c_str(), O_RDONLY);
    if (fd < 0) {
      LOG_SYSERROR << "Failed to open " << pathname;
      return nullptr;
    }

    auto addr = Mmap(fd, len, PROT_READ, MAP_PRIVATE);

    if (!addr) {
      LOG_SYSERROR << "Failed to call mmap() with " << pathname;
      ::close(fd);
      return nullptr;
    }

    sp.reset(new char*(addr), [this, pathname, len](char** p) {
      if (p) {
        {
          MutexGuard guard(mutex_addr_);

          auto iter = addr_map_.find(pathname);

          if (iter != addr_map_.end() && iter->second.expired()) {
            addr_map_.erase(iter);
          }

          Munmap(*p, len);
        }

        delete p;      
      }
    });

    wp = sp;
  }

  return sp;
}


} // namespace http
