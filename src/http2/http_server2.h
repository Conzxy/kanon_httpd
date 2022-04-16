#ifndef KANON_HTTP_SERVER2_H
#define KANON_HTTP_SERVER2_H

#include <unordered_map>

#include <kanon/net/user_server.h>
#include <kanon/util/optional.h>

namespace http {

class HttpSession;

class HttpServer : public kanon::TcpServer {
  friend class HttpSession;
public:
  HttpServer(EventLoop* loop, InetAddr const& addr);

  ~HttpServer() noexcept;
private:
  void EmplaceOffset(HttpSession* session, off_t off);
  void EraseOffset(HttpSession* session);
  kanon::optional<off_t> SearchOffset(HttpSession* session);

  // Cache factory method
  // @see Modern Effective C++ Item 21
  std::shared_ptr<int> GetFd(std::string const& path);

  kanon::MutexLock mutex_;
  std::unordered_map<std::string, std::weak_ptr<int>> fd_map_;
  std::unordered_map<HttpSession*, off_t> offset_map_;
};

} // namespace http

#endif // KANON_HTTP_SERVER2_H