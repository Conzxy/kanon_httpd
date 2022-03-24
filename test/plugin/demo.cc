#include "demo.h"

namespace plugin {

double PluginDemo::f()
{
  return 11.11;
}

extern "C" {
  PluginDemoBase* CreateObject() {
    return new PluginDemo();
  }
}

}