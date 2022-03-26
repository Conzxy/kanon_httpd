#ifndef KANON_HTTP_CONFIG_H
#define KANON_HTTP_CONFIG_H

#include <string>

namespace http {

struct HttpConfig {
  std::string homepage_path;
  std::string root_path;
  std::string hostname;
  std::string homepage_name;
};

extern HttpConfig g_config;

void SetConfigParameters(std::string const& config_name);

} // namespace http

#endif // 