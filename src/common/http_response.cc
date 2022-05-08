#include "http_response.h"
#include "kanon/util/macro.h"
#include "kanon/util/mem.h"

using namespace kanon;
using namespace std;

namespace http {

Buffer& HttpResponse::GetBuffer() 
{
  if (!known_length_) {
    if (body_.size() != 0) {
      char buf[128];
      MemoryZero(buf);
      ::snprintf(buf, sizeof buf, "%lu", body_.size());
      AddHeader("Content-Length", buf);
    }
    buffer_.Append("\r\n");
    buffer_.Append(body_.data(), body_.size());
  }

  return buffer_;
}

HttpResponse GetClientError(
  HttpStatusCode status_code,
  StringView msg)
{
  HttpResponse response;
  char buf[4096];MemoryZero(buf);

  // Content-Length will compute automatically and append
  return response
    .AddHeaderLine(status_code)
    .AddHeader("Content-Type", "text/html")
    .AddHeader("Connection", "close")
    .AddBlackLine()
    .AddBody("<html>")
    .AddBody("<title>Kanon Error</title>")
    .AddBody("<body bgcolor=\"#ffffff\">")
    .AddBody(buf, sizeof buf, "<h1 align=\"center\">%d %s</h1>\r\n", GetStatusCode(status_code), GetStatusCodeString(status_code))
    // .AddBody(buf, "<font size=\"7\">%d %s</font>\r\n", GetStatusCode(status_code), GetStatusCodeString(status_code))
    .AddBody(buf, sizeof buf, "<p>%s</p>\r\n", msg.data())
    .AddBody("<div>")
    .AddBody(buf, sizeof buf, "<center><hr><em>This is a simple http server(Kanon)</em></center>")
    .AddBody("</div>")
    .AddBody("</body>")
    .AddBody("</html>\r\n");
}

char const* HttpResponse::GetFileType(StringView filename) {
  if (filename.ends_with(".pdf")) {
    return "application/pdf";
  } else if (filename.ends_with(".png")) {
    return "image/png";
  } else if (filename.ends_with(".jpg")) {
    return "image/jpg";
  } else if (filename.ends_with(".gif")) {
    return "image/gif";
  } else if (filename.ends_with(".html")) {
    return "text/html";
  } else if (filename.ends_with(".txt")) {
    return "text/plain";
  } else {
    return "";
  }
}

std::string HttpResponse::Dec2Hex(size_t num) {
  static char const hexs[] = "0123456789ABCDEF";

  int left = 0;
  int mask = (1 << 4) - 1;
  char buf[16];
  int pos = 15;
  std::string ret;

  while (num >= 16) {
    left = num & mask;
    num = num / 16;

    buf[pos--] = hexs[left];
  }

  buf[pos] = hexs[num];

  return std::string(buf + pos, sizeof(buf) - pos);
}

} // namespace http