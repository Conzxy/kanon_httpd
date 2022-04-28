#include "http2/http_server2.h"

#include "kanon/log/async_log.h"
#include "util/cmdline_parse.h"
#include "config/http_options.h"
#include "config/http_config.h"

using namespace http;
using namespace kanon;

int main(int argc, char* argv[])
{
  HttpOptions options;

  if (ParseCmdLine(argc, argv, options)) {
    SetConfigParameters(options.config_name);


    if (options.log_way == "file") {
      LOG_INFO << "The log files are stored in the " << options.log_dir << " directory";

      SetupAsyncLog("httpd_kanon", 2 * 1024 * 1024, options.log_dir);
    }

    EventLoop loop;
    InetAddr addr(options.port);

    HttpServer server(&loop, addr);

    server.SetLoopNum(options.thread_num);
    server.StartRun();

    loop.StartLoop();
  }

  return 0;
}
