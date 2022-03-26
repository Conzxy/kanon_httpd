#include <memory>

#include <kanon/log/logger.h>

#include "plugin/plugin_loader.h"
#include "plugin/http_dynamic_response_interface.h"

using namespace plugin;
using namespace http;

int main()
{
  PluginLoader<HttpDynamicResponseInterface> plugin;

  auto error = plugin.Open("/root/kanon_httpd/resources/contents/adder");

  if (!error) {
    auto func = plugin.GetCreateFunc();
    std::unique_ptr<HttpDynamicResponseInterface> adder(func());
    
    LOG_INFO << adder->GenResponseForGet(ArgsMap()).GetBuffer().ToStringView();
  }
  else {
    LOG_SYSERROR << *error;

  }

  return 0;
}