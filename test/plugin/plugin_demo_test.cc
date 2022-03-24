#include "demo.h"

#include <memory>
#include <dlfcn.h>
#include <iostream>

using namespace plugin;

using CreateFunc = PluginDemoBase*(*)();

int main()
{
  void* handle = dlopen("./libdemo", RTLD_LAZY);

  auto create_func = reinterpret_cast<CreateFunc>(dlsym(handle, "CreateObject"));

  std::unique_ptr<PluginDemoBase> p(create_func());

  std::cout << p->f() << std::endl;
}