#include "plugin/plugin_loader.h"

#include <memory>

#include "demo.h"

using namespace plugin;

int main()
{
  PluginLoader<PluginDemoBase> loader;

  const auto has_error = loader.Open("./demo");

  if (has_error) {
    ::fprintf(stderr, "%s", has_error->c_str());
    ::exit(1);
  }

  auto func = loader.GetCreateFunc();

  std::unique_ptr<PluginDemoBase> ptr(func());

  ::printf("%f\n", ptr->f());
}