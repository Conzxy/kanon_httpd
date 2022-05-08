#ifndef HTTP_DYNAMIC_REPONSE_INTERFACE_H
#define HTTP_DYNAMIC_REPONSE_INTERFACE_H

#include <kanon/util/noncopyable.h>
#include <kanon/net/tcp_connection.h>

#include "common/types.h"
#include "common/http_response.h"

namespace http {

class HttpDynamicResponseInterface {
public:
  HttpDynamicResponseInterface() = default;  
  virtual ~HttpDynamicResponseInterface() = default;
  virtual void GenResponseForGet(ArgsMap const& args, HttpResponse& response) = 0;
  virtual void GenResponseForPost(std::string const& body, HttpResponse& response) = 0;

  void SetVersion(HttpVersion ver) noexcept { version_ = ver; }
  void SetConnection(kanon::TcpConnectionPtr const& conn) { conn_ = conn; }

protected:
  kanon::TcpConnectionPtr conn_;
  HttpVersion version_; 
  DISABLE_EVIL_COPYABLE(HttpDynamicResponseInterface)
};

} // namspace plugin

#endif // HTTP_DYNAMIC_REPONSE_INTERFACE_H