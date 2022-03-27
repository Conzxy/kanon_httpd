#include "common/parse_args.h"
#include "common/types.h"

#include <kanon/log/logger.h>

using namespace kanon;

namespace http {

ArgsMap ParseArgs(StringView query) {
  StringView::size_type equal_pos = StringView::npos;
  StringView::size_type and_pos = StringView::npos;
  std::string key;
  std::string val;

  ArgsMap kvs;

  LOG_TRACE << "Start parsing the query args...";

  for (;;) {
    equal_pos = query.find('=');
    and_pos = query.find('&');

    // and_pos == npos is also ok
    kvs.emplace(
      query.substr_range(0, equal_pos).ToString(),
      query.substr_range(equal_pos+1, and_pos).ToString());

    if (and_pos == StringView::npos) break;

    query.remove_prefix(and_pos+1);
  }

  LOG_TRACE << "The query args have been parsed";

  LOG_DEBUG << "The query args map is following: ";

  for (auto const& kv : kvs) {
    LOG_DEBUG << "[" << kv.first << ": " << kv.second << "]";
  }

  return kvs;
}

} // namespace http