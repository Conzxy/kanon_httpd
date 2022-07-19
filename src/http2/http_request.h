#ifndef _KANON_HTTPD_HTTP_REQUEST_H_
#define _KANON_HTTPD_HTTP_REQUEST_H_

#include <string>
#include <kanon/util/optional.h>
#include <kanon/net/timer/timer_id.h>

#include "common/http_constant.h"
#include "common/types.h"

namespace http {

struct HttpRequest {
  /*
   * The metadata for parsing header line of a http request
   */

  bool is_static = true; /** Static page */
  bool is_complex = false; /** Complex URL, e.g. %Hex Hex */
  std::string url; /** The URL part */
  std::string query; /** query string */
  HttpMethod method = HttpMethod::kNotSupport; /** method of header line */
  HttpVersion version = HttpVersion::kNotSupport; /** version code of header line */

  /**
   * Store the header fields
   */
  HeaderMap headers;

  /**
   * Store the body of a http request
   */
  std::string body;

  bool is_keep_alive = false; /** Determine if a keep-alive connection */
};

} // http

#endif 
