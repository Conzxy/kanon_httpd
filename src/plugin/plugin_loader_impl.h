#include <kanon/util/optional.h>
#ifndef KANON_PLUGIN_H
#include "plugin_loader.h"
#endif

#include <dlfcn.h>
#include <stdexcept>

#include "util/exception_macro.h"


namespace plugin {

DEFINE_EXCEPTION_FROM_OTHER(PluginLoaderException, std::runtime_error);

template<typename T>
PluginLoader<T>::PluginLoader()
 : handle_(NULL)
{

}

template<typename T>
PluginLoader<T>::PluginLoader(std::string const& plugin_name)
{
  handle_ = ::dlopen(plugin_name.c_str(), RTLD_LAZY);

  if (!handle_) {
    char err_buf[4096];
    ::snprintf(err_buf, sizeof err_buf, 
      "Error occurred in the call of dlopen.\n"
      "Message: %s.\n",
      ::dlerror());

    throw PluginLoaderException(err_buf);
  }
}

template<typename T>
PluginLoader<T>::~PluginLoader() noexcept
{
  if (handle_) {
    ::dlclose(handle_); 
  }
}

template<typename T>
kanon::optional<std::string> PluginLoader<T>::Open(const std::string &plugin_name)
{
  handle_ = ::dlopen(plugin_name.c_str(), RTLD_LAZY);

  if (!handle_) {
    return ::dlerror();
  }

  return kanon::make_null_optional<std::string>();
}

template<typename T>
auto PluginLoader<T>::GetCreateFunc() -> CreateFunc
{
  void* create_func = ::dlsym(handle_, "CreateObject");

  return reinterpret_cast<CreateFunc>(create_func);
}

} // namespace plugin
