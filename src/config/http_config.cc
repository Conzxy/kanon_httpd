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

static void SetBoolParameter(kanon::optional<std::string> const& val, bool& para)
{
  if (val && *val == "true") {
    para = true;
  } else {
    para = false;
  }
}

void SetConfigParameters(const std::string &config_name)
{
  ConfigDescriptor cd(config_name);

  LOG_INFO << "Start parsing the configuration file...";
  cd.Read();

  SetStringParameter(cd.GetParameter("HomePagePath"), g_config.homepage_path);
  SetStringParameter(cd.GetParameter("Host"), g_config.hostname);
  SetStringParameter(cd.GetParameter("RootPath"), g_config.root_path);
  SetBoolParameter(cd.GetParameter("UseMmap"), g_config.use_mmap);

  LOG_INFO << "The configuration file has been parsed";
  LOG_INFO << "[HomePagePath: " << g_config.homepage_path << "]";
  LOG_INFO << "[Host: " << g_config.hostname << "]";
  LOG_INFO << "[RootPath: " << g_config.root_path << "]";
  LOG_INFO << "[UseMmap: " << g_config.use_mmap << "]";
}

} // namespace http