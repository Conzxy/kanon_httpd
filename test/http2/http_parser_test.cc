#include <gtest/gtest.h>
#include <kanon/log/logger.h>

#define private public
#include "http2/http_parser.h"
#include "http2/http_request.h"

using namespace http;

TEST(http_parser, header_line) {
  kanon::Buffer buffer;
  HttpParser parser;
  HttpRequest request;
  // Scenario:
  // Client request static contents, e.g. a html page
  buffer.Append("GET /kanon_http/html/index.html HTTP/1.0");
  EXPECT_EQ(parser.ParseHeaderLine(buffer.ToStringView(), &request), HttpParser::kGood);
  EXPECT_EQ(request.is_complex, false);
  EXPECT_EQ(request.is_static, true);
  EXPECT_EQ(request.method, HttpMethod::kGet);
  EXPECT_EQ(request.version, HttpVersion::kHttp10);
  EXPECT_EQ(request.url, "/kanon_http/html/index.html");

  // Scenario:
  // Client request dynamic contents
  HttpRequest request2;
  EXPECT_EQ(
    parser.ParseHeaderLine("POST /kanon_http/contents/adder?a=100&b=100 HTTP/1.1", &request2),
    HttpParser::kGood);
  EXPECT_EQ(request2.is_static, false);
  EXPECT_EQ(request2.is_complex, true);
  EXPECT_EQ(request2.method, HttpMethod::kPost);
  EXPECT_EQ(request2.version, HttpVersion::kHttp11);
  EXPECT_EQ(request2.url, "/kanon_http/contents/adder");
  EXPECT_EQ(request2.query, "a=100&b=100");

  // Scenario:
  // URL format
  HttpRequest request3;
  EXPECT_EQ(
    parser.ParseHeaderLine("HEAD /kanon_http/contents/%20?a=%31%30%30&b=%31%30%32 HTTP/1.1", &request3),
    HttpParser::kGood);
  
  EXPECT_EQ(request3.is_static, false);
  EXPECT_EQ(request3.is_complex, true);
  EXPECT_EQ(request3.method, HttpMethod::kHead);
  EXPECT_EQ(request3.version, HttpVersion::kHttp11);
  EXPECT_EQ(request3.url, "/kanon_http/contents/ ");
  EXPECT_EQ(request3.query, "a=100&b=102");
}

TEST(http_parser, error_handling_of_header_line) {
  // Scenario:
  // Empty string
  HttpParser parser;
  HttpRequest request;
  EXPECT_EQ(
    parser.ParseHeaderLine("", &request),
    HttpParser::kError);

  LOG_DEBUG << "The error message for empty string" << parser.error_.msg;

  EXPECT_EQ(
    parser.ParseHeaderLine("      ", &request),
    HttpParser::kError);

  LOG_DEBUG << "The error message for spaces string" << parser.error_.msg;

  EXPECT_EQ(
    parser.ParseHeaderLine("XXX /xxx HTTP/1.0", &request),
    HttpParser::kError
  );

  EXPECT_EQ(parser.error_.code, HttpStatusCode::k405MethodNotAllowd);
  LOG_DEBUG << "The error message for invalid method" << parser.error_.msg;

  EXPECT_EQ(
    parser.ParseHeaderLine("GET /xx HTTP/2.0", &request),
    HttpParser::kError);

  EXPECT_EQ(parser.error_.code, HttpStatusCode::k400BadRequest);
  LOG_DEBUG << "The error message for not supported http version code";
}

TEST(http_parser, header_fields) {
  // Scenario:
  // Single header field
  HttpParser parser;
  HttpRequest request;

  EXPECT_EQ(
    parser.ParseHeaderField("Connection: Close", &request),
    HttpParser::kShort);

  auto iter = request.headers.find("Connection");

  EXPECT_NE(iter, request.headers.end());
  EXPECT_EQ(iter->second, "Close");

  // Scenario:
  // Mutilple header fields
  kanon::Buffer buffer;

  buffer.Append(
    "GET /xxx HTTP/1.0\r\n"
    "Connection: Close\r\n"
    "Host: xxxx\r\n"
    "Agent: Google Browser\r\n"
    "\r\n");

  EXPECT_EQ(
    parser.Parse(buffer, &request),
    HttpParser::kGood
  );

  LOG_DEBUG << buffer.GetReadableSize();
  EXPECT_TRUE(!buffer.HasReadable());
  EXPECT_EQ(request.url, "/xxx");

  auto& headers = request.headers;

  iter = headers.find("Connection");
  EXPECT_NE(iter, headers.end());
  EXPECT_EQ(iter->second, "Close");

  iter = headers.find("Host");
  EXPECT_NE(iter, headers.end());
  EXPECT_EQ(iter->second, "xxxx");

  iter = headers.find("Agent");
  EXPECT_NE(iter, headers.end());
  EXPECT_EQ(iter->second, "Google Browser");
}


int main() {
  testing::InitGoogleTest();

  return RUN_ALL_TESTS();
}
