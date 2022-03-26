#ifndef HTTP_DYNAMIC_REPONSE_INTERFACE_H
#define HTTP_DYNAMIC_REPONSE_INTERFACE_H

#include "kanon/util/noncopyable.h"

#include "common/types.h"
#include "common/http_response.h"

namespace http {

class HttpDynamicResponseInterface {
public:
  HttpDynamicResponseInterface() = default;  
  virtual ~HttpDynamicResponseInterface() = default;
  virtual HttpResponse GenResponseForGet(ArgsMap const& args) = 0;
  virtual HttpResponse GenResponseForPost(std::string const& body) = 0;

private:
  DISABLE_EVIL_COPYABLE(HttpDynamicResponseInterface)
};

} // namspace plugin

#endif // HTTP_DYNAMIC_REPONSE_INTERFACE_H