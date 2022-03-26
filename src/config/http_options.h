#ifndef KANON_HTTP_CONFIG_HTTP_OPTIONS_H
#define KANON_HTTP_CONFIG_HTTP_OPTIONS_H

#include <string>

namespace http {

struct HttpOptions {
  std::string log_way;
  std::string log_dir;
  std::string config_name;
  uint16_t port;
  uint16_t thread_num;
};

} // namespac http

#endif //