#ifndef KANON_CONFIG_DESCRIPTOR_H
#define KANON_CONFIG_DESCRIPTOR_H

#include <unordered_map>

#include <kanon/util/noncopyable.h>
#include <kanon/string/string_view.h>

#include "util/file.h"

namespace config {

class ConfigDescriptor : kanon::noncopyable {
public:
  explicit ConfigDescriptor(kanon::StringView const& filename);
  ~ConfigDescriptor() noexcept;

  void Read();

  kanon::StringView GetField(std::string const& key);
private:
  http::File file_;
  std::string filename_;
  std::unordered_map<std::string, std::string> fields_;
};

} // namespace config

#endif // KANON_CONFIG_DESCRIPTOR_H