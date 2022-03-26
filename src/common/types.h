#ifndef KANON_HTTP_TYPES_H
#define KANON_HTTP_TYPES_H

#include <unordered_map>

namespace http {

using HeaderMap = std::unordered_map<std::string, std::string>;
using HeaderType = HeaderMap::value_type;
using ArgsMap = std::unordered_map<std::string, std::string>;

} // namespace http

#endif // KANON_HTTP_TYPES_H
