#include "http2/http_server2.h"

#include "kanon/log/async_log.h"
//#include "util/cmdline_parse.h"
//#include "config/http_options.h"
#include "config/http_config.h"

#include "takina/takina.h"

using namespace http;
using namespace kanon;

struct Options {
  bool log_file;
  std::string log_dir;
  std::string config_name = "./.kanon_httpd.conf";
  int port = 80;
  int thread_num = 0;
};

int main(int argc, char* argv[])
{
  Options options;

  std::string home_dir = ::getenv("HOME");
  options.log_dir = home_dir;
  options.log_dir += "/.log";

  takina::AddUsage("./httpd [options]");
  takina::AddDescription("This is a http server that can process request of static or dynamic page");
  takina::AddSection("Server setting");
  takina::AddOption({"p", "port", "Tcp port number of httpd", "PORT"}, &options.port);
  takina::AddOption({"t", "thread_num", 
                    "Threads number of IO thread. Main thread just accept new connection and other threads handle IO events,(default: 0)",
                    "THREAD_NUMBER"}, &options.thread_num);
  takina::AddOption({"c", "config", "Configuration file path(default: ./.kanon_httpd.conf)", "CONFIG_NAME"}, &options.config_name);
  takina::AddSection("Log setting");
  takina::AddOption({"f", "log_file", "Log to file in asynchronously(default: log to terminal)"}, &options.log_file);
  takina::AddOption({"d", "log_dir", "The directory where log file store(default: ${HOME}/.log)", "LOG_DIRECTORY"}, &options.log_dir);

  std::string err_msg;
  if (takina::Parse(argc, argv, &err_msg)) {
    SetConfigParameters(options.config_name);

    if (options.log_file) {
      LOG_INFO << "The log files are stored in the " << options.log_dir << " directory";

      SetupAsyncLog("httpd_kanon", 2 * 1024 * 1024, options.log_dir);
    }

    EventLoop loop;

    InetAddr addr(options.port);

    HttpServer server(&loop, addr);
    server.SetLoopNum(options.thread_num);
    server.StartRun();

    // loop.SetEdgeTriggerMode();
    loop.StartLoop();
  } else {
    ::printf("Command line parse error: %s\n", err_msg.c_str());    
    ::fflush(stdout);
  }

  return 0;
}
