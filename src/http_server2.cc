#include "http_server2.h"

namespace http {


HttpServer::HttpServer(EventLoop* loop, InetAddr const& listen_addr)
  : TcpServer(loop, listen_addr, "HttpServer")
{
}

} // namespace http