#ifndef KANON_PLUGIN_H
#define KANON_PLUGIN_H

#include <dlfcn.h>
#include <string>

#include <kanon/util/optional.h>
#include <kanon/util/noncopyable.h>

namespace plugin {

template<typename T>
class PluginLoader {
  using CreateFunc = T*(*)();
public:
  PluginLoader();
  explicit PluginLoader(std::string const& plugin_name);
  ~PluginLoader() noexcept;

  kanon::optional<std::string> Open(std::string const& plugin_name);
  CreateFunc GetCreateFunc();
private:
  void* handle_;

  DISABLE_EVIL_COPYABLE(PluginLoader);
};


} // namespace plugin

#include "plugin_loader_impl.h"

#endif // 