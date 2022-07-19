#ifndef _KANON_HTTPD_HTTP_ERROR_H_
#define _KANON_HTTPD_HTTP_ERROR_H_

#include <string>

#include "common/http_constant.h"

namespace http {

struct HttpError {
  HttpStatusCode code;
  std::string msg="";
};

} // http

#endif // _KANON_HTTPD_HTTP_ERROR_H_
