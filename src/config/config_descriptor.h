#ifndef KANON_CONFIG_DESCRIPTOR_H
#define KANON_CONFIG_DESCRIPTOR_H

#include <unordered_map>

#include <kanon/util/noncopyable.h>
#include <kanon/string/string_view.h>
#include <kanon/util/optional.h>

#include "util/file.h"

namespace config {

class ConfigDescriptor : kanon::noncopyable {
  using ParameterMap = std::unordered_map<std::string, std::string>;
public:
  explicit ConfigDescriptor(kanon::StringView const& filename);
  ~ConfigDescriptor() noexcept;

  void Read();
  kanon::optional<std::string> GetParameter(std::string const& key);

  // For debugging
  ParameterMap const& GetParameters() const noexcept
  { return paras_; }

private:
  http::File file_;
  std::string filename_;
  ParameterMap paras_;
};

} // namespace config

#endif // KANON_CONFIG_DESCRIPTOR_H