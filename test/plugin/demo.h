#ifndef PLUGIN_DEMO_H
#define PLUGIN_DEMO_H

#include "kanon/util/noncopyable.h"

namespace plugin {

class PluginInterface {
 public:
  
  virtual void DemoApi(int i) = 0;
};

class PluginDemoBase {
public:
  PluginDemoBase() = default;
  virtual ~PluginDemoBase() = default;
  virtual double f() = 0;
};

class PluginDemo : public PluginDemoBase {
 public:
  PluginDemo() = default;
  ~PluginDemo() = default;
  double f() override;
};

extern "C" {
  PluginDemoBase* CreateObject();
}

} // namespace plugin

#endif // PLUGIN_DEMO_H