#include "common/http_response.h"
#include "common/parse_args.h"
#include "plugin/http_dynamic_response_interface.h"

using namespace http;

class Adder : public HttpDynamicResponseInterface {
 public:
  Adder() = default;
  ~Adder() = default;

  HttpResponse GenResponseForGet(const ArgsMap &args) override
  {
    int a = 0;
    int b = 0;

    auto iter = args.find("a");
    if (iter != args.end()) {
      a = ::atoi(iter->second.c_str());
    }

    iter = args.find("b");
    if (iter != args.end()) {
      b = ::atoi(iter->second.c_str());
    }

    HttpResponse response;

    char buf[4096];MemoryZero(buf);

    response.AddHeader("Content-type", "text/html")
            .AddBlackLine()
            .AddBody("<html>")
            .AddBody("<title>adder</title>")
            .AddBody("<body bgcolor=\"#ffffff\">")
            .AddBody("Welcome to add.com\r\n")
            .AddBody(buf, "<p>The answer is: %d + %d = %d</p>\r\n", a, b, a + b)
            .AddBody("<p>Thanks for visiting!</p>\r\n")
            .AddBody("</body>")
            .AddBody("</html>\r\n");

    return response;
  }

  HttpResponse GenResponseForPost(const std::string &body) override
  {
    auto args = ParseArgs(body);

    return GenResponseForGet(args);    
  }
};

extern "C" {
  HttpDynamicResponseInterface* CreateObject()
  {
    return new Adder();
  }
}
