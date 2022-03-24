#include "config/config_descriptor.h"

#include <kanon/log/logger.h>
#include <kanon/string/string_view.h>

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
    LOG_DEBUG << "config line: " << line;
    auto colon_pos = line.find(':');
    auto comment_pos = line.find('#');

    if (comment_pos != std::string::npos) {
      continue;
    }

    if (colon_pos != std::string::npos) {
      // Read config field
      auto space_pos = line.find(' ', colon_pos+2);

      if (space_pos != std::string::npos) {
        fields_.emplace(line.substr(0, colon_pos), line.substr(colon_pos+2, space_pos));
      }
      else {
        fields_.emplace(line.substr(0, colon_pos), line.substr(colon_pos+2));
      }
    }
  }

  if (!file_.IsEof()) {
    LOG_SYSERROR << "Failed to read one line from " << filename_;
  }
}

kanon::StringView ConfigDescriptor::GetField(std::string const& key)
{
  auto iter = fields_.find(key);

  if (iter == std::end(fields_)) {
    return MakeStringView("");
  }

  return iter->second;
}

} // namespace config