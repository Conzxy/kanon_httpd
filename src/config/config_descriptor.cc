#include "config/config_descriptor.h"

#include <kanon/log/logger.h>
#include <kanon/string/string_view.h>
#include <kanon/util/optional.h>

using namespace kanon;
using namespace http;

namespace config {

ConfigDescriptor::ConfigDescriptor(StringView const& filename)
 : filename_(filename.ToString())
{
  if (!file_.Open(filename_.data(), File::kRead)) {
    LOG_SYSERROR << "Failed to open config file: [" << filename_ << "]";
    return ;
  }
}

ConfigDescriptor::~ConfigDescriptor() noexcept
{
}

void ConfigDescriptor::Read()
{
  std::string line;
  line.reserve(4096);

  while (file_.ReadLine(line, false)) {
    auto colon_pos = line.find(':');
    auto comment_pos = line.find('#');

    if (comment_pos != std::string::npos) {
      continue;
    }

    if (colon_pos != std::string::npos) {
      // Read config field
      auto space_pos = line.find(' ', colon_pos+2);

      if (space_pos != std::string::npos) {
        paras_.emplace(line.substr(0, colon_pos), line.substr(colon_pos+2, space_pos));
      }
      else {
        paras_.emplace(line.substr(0, colon_pos), line.substr(colon_pos+2));
      }
    }
  }

  if (!file_.IsEof()) {
    LOG_SYSERROR << "Failed to read one line from " << filename_;
  }
}

kanon::optional<std::string> 
ConfigDescriptor::GetParameter(std::string const& key)
{
  auto iter = paras_.find(key);

  if (iter == std::end(paras_)) {
    return kanon::make_null_optional<std::string>();
  }

  return kanon::make_optional(std::move(iter->second));
}

} // namespace config