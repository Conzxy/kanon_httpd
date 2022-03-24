#include "common/http_constant.h"
#include <gtest/gtest.h>
#include <kanon/log/logger.h>

#define private public

#include "http2/http_session.h"


using namespace http;

class HttpSessionTest : public ::testing::Test {
 protected:
  void SetUp() override
  {
    session_.reset(new HttpSession());
  }

  std::unique_ptr<HttpSession> session_;
};

TEST_F(HttpSessionTest, header_line) {
  kanon::Buffer buffer;
  // Scenario:
  // Client request static contents, e.g. a html page
  buffer.Append("GET /kanon_http/html/index.html HTTP/1.0");
  EXPECT_EQ(session_->ParseHeaderLine(buffer.ToStringView()), HttpSession::kGood);
  EXPECT_EQ(session_->is_complex_, false);
  EXPECT_EQ(session_->is_static_, true);
  EXPECT_EQ(session_->method_, HttpMethod::kGet);
  EXPECT_EQ(session_->version_, HttpVersion::kHttp10);
  EXPECT_EQ(session_->url_, "/kanon_http/html/index.html");

  // Scenario:
  // Client request dynamic contents
  EXPECT_EQ(
    session_->ParseHeaderLine("POST /kanon_http/contents/adder?a=100&b=100 HTTP/1.1"),
    HttpSession::kGood);
  EXPECT_EQ(session_->is_static_, false);
  EXPECT_EQ(session_->is_complex_, true);
  EXPECT_EQ(session_->method_, HttpMethod::kPost);
  EXPECT_EQ(session_->version_, HttpVersion::kHttp11);
  EXPECT_EQ(session_->url_, "/kanon_http/contents/adder");
  EXPECT_EQ(session_->query_, "a=100&b=100");

  // Scenario:
  // URL format
  EXPECT_EQ(
    session_->ParseHeaderLine("HEAD /kanon_http/contents/%20?a=%31%30%30&b=%31%30%32 HTTP/1.1"),
    HttpSession::kGood);
  
  EXPECT_EQ(session_->is_static_, false);
  EXPECT_EQ(session_->is_complex_, true);
  EXPECT_EQ(session_->method_, HttpMethod::kHead);
  EXPECT_EQ(session_->version_, HttpVersion::kHttp11);
  EXPECT_EQ(session_->url_, "/kanon_http/contents/ ");
  EXPECT_EQ(session_->query_, "a=100&b=102");
}

TEST_F(HttpSessionTest, error_handling_of_header_line) {
  // Scenario:
  // Empty string
  EXPECT_EQ(
    session_->ParseHeaderLine(""),
    HttpSession::kError);

  LOG_DEBUG << "The error message for empty string" << session_->meta_error_.second;

  EXPECT_EQ(
    session_->ParseHeaderLine("      "),
    HttpSession::kError);

  LOG_DEBUG << "The error message for spaces string" << session_->meta_error_.second;

  EXPECT_EQ(
    session_->ParseHeaderLine("XXX /xxx HTTP/1.0"),
    HttpSession::kError
  );

  EXPECT_EQ(session_->meta_error_.first, HttpStatusCode::k405MethodNotAllowd);
  LOG_DEBUG << "The error message for invalid method" << session_->meta_error_.second;

  EXPECT_EQ(
    session_->ParseHeaderLine("GET /xx HTTP/2.0"),
    HttpSession::kError);

  EXPECT_EQ(session_->meta_error_.first, HttpStatusCode::k400BadRequest);
  LOG_DEBUG << "The error message for not supported http version code";
}

TEST_F(HttpSessionTest, header_fields) {
  // Scenario:
  // Single header field

  EXPECT_EQ(
    session_->ParseHeaderField("Connection: Close"),
    HttpSession::kShort);

  auto iter = session_->headers_.find("Connection");

  EXPECT_NE(iter, session_->headers_.end());
  EXPECT_EQ(iter->second, "Close");

  // Scenario:
  // Mutilple header fields
  session_->Reset();

  kanon::Buffer request{};

  request.Append(
    "GET /xxx HTTP/1.0\r\n"
    "Connection: Close\r\n"
    "Host: xxxx\r\n"
    "Agent: Google Browser\r\n"
    "\r\n");

  EXPECT_EQ(
    session_->Parse(request),
    HttpSession::kGood
  );

  LOG_DEBUG << request.GetReadableSize();
  EXPECT_TRUE(!request.HasReadable());
  EXPECT_EQ(session_->url_, "/xxx");

  auto& headers = session_->headers_;

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

TEST_F(HttpSessionTest, DISABLE_error_handling_of_header_fields) {

}

int main() {
  testing::InitGoogleTest();

  return RUN_ALL_TESTS();
}