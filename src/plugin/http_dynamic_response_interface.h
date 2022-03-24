#ifndef HTTP_DYNAMIC_REPONSE_INTERFACE_H
#define HTTP_DYNAMIC_REPONSE_INTERFACE_H

#include "kanon/util/noncopyable.h"

#include "common/http_response.h"

namespace plugin {


class HttpDynamicResponseInterface {
 public:
  DISABLE_EVIL_COPYABLE(HttpDynamicResponseInterface)

  virtual http::HttpResponse GenResponse();
  
};

} // namspace plugin

#endif // HTTP_DYNAMIC_REPONSE_INTERFACE_H