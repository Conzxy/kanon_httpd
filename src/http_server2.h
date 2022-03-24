#ifndef KANON_HTTP_SERVER2_H
#define KANON_HTTP_SERVER2_H

#include "kanon/net/user_server.h"

namespace http {

class HttpServer : public kanon::TcpServer {
public:
  // Default port is 80
  explicit HttpServer(EventLoop* loop)
    : HttpServer(loop, InetAddr(80))
  { 
  }

  HttpServer(EventLoop* loop, InetAddr const& listen_addr);



};

} // namespace http

#endif // KANON_HTTP_SERVER2_H