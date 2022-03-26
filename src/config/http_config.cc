#include "http_config.h"

#include <kanon/log/logger.h>

#include "config/config_descriptor.h"

using namespace config;

namespace http {

HttpConfig g_config{};

static void SetStringParameter(kanon::optional<std::string>&& val, std::string& para)
{
  if (val) {
    para = std::move(*val);
  }
}
void SetConfigParameters(const std::string &config_name)
{
  ConfigDescriptor cd(config_name);

  LOG_TRACE << "Start parsing the configuration file...";
  cd.Read();

  SetStringParameter(cd.GetParameter("HomePagePath"), g_config.homepage_path);
  SetStringParameter(cd.GetParameter("Host"), g_config.hostname);
  SetStringParameter(cd.GetParameter("RootPath"), g_config.root_path);

  LOG_TRACE << "The configuration file has been parsed";
}

} // namespace http