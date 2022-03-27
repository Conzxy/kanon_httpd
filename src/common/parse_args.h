#ifndef KANON_HTTP_PARSE_ARGS_H
#define KANON_HTTP_PARSE_ARGS_H

#include <kanon/string/string_view.h>

#include "common/types.h"

namespace http {

ArgsMap ParseArgs(kanon::StringView query);

}
#endif