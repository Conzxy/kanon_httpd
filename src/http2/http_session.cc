#include "http_session.h"

#include <unistd.h>
#include <fcntl.h>

#include <kanon/log/logger.h>
#include <kanon/net/buffer.h>
#include <kanon/net/callback.h>
#include <kanon/string/string_view.h>
#include <kanon/util/macro.h>
#include <kanon/util/ptr.h>


#include "common/http_response.h"
#include "config/http_config.h"
#include "common/http_constant.h"
#include "plugin/plugin_loader.h"
#include "plugin/http_dynamic_response_interface.h"
#include "unix/fd_wrapper.h"

#include "http_server2.h"

using namespace kanon;
using namespace unix;

namespace http {

AtomicCounter32 HttpSession::counter_(1);

HttpSession::HttpSession()
  : server_(nullptr)
  , conn_(nullptr)
  , parse_phase_(kHeaderLine)
  , meta_error_()
  , is_static_(true)
  , is_complex_(false)
  , method_(HttpMethod::kNotSupport)
  , version_(HttpVersion::kNotSupport)
  , timer_id_()
  , id_(counter_.GetAndAdd(1))
{
}

HttpSession::HttpSession(HttpServer& server, TcpConnectionPtr const& conn)
  : HttpSession()
{
  server_ = &server;
  conn_ = kanon::GetPointer(conn);

  conn_->SetMessageCallback(std::bind(
    &HttpSession::OnMessage, this, kanon::_1, kanon::_2, kanon::_3));
}

HttpSession::~HttpSession() noexcept
{
}

void HttpSession::OnMessage(TcpConnectionPtr const& conn, Buffer& buffer, TimeStamp recv_time)
{
  KANON_UNUSED(conn);
  KANON_UNUSED(recv_time);
  // FIXME Implementation timeout disconnection

  if (server_->timer_id_) {
    conn->GetLoop()->CancelTimer(*server_->timer_id_);
    server_->timer_id_.SetInvalid();

    LOG_DEBUG << "The connection-timeout timer is reset";
  }

  if (timer_id_) {
    conn->GetLoop()->CancelTimer(*timer_id_);
    timer_id_.SetInvalid();

    LOG_DEBUG << "The keep-alive timer is reset";
  }

  ParseResult ret;
  if ( ( ret = Parse(buffer) ) == kGood) {
    if (url_ == "/") {
      url_ += g_config.homepage_path;
    }
    url_.insert(0, g_config.root_path.data(), g_config.root_path.size());
    LOG_DEBUG << "content_path = " << url_;

    auto iter = headers_.find("Connection");
    if (iter != std::end(headers_) && 
        !::strncasecmp(iter->second.c_str(), "keep-alive", iter->second.size())) {
      is_keep_alive_ = true;
    }

    switch (method_) {
      case HttpMethod::kGet:
        if (is_static_) {
          ServeFile();
        }
        else {
          ServeDynamicContent();
        }
      break;

      case HttpMethod::kPost:
        ServeDynamicContent();        
      break;

      default:
        NotImplementation();
    }      
  }

  if (ret == kError) {
    SendErrorResponse();
  }
}

void HttpSession::ServeFile()
{
  const int fd = ::open(url_.c_str(), O_RDONLY);

  if (fd < 0) {
    if (errno == ENOENT) {
      FillMetaError(
        HttpStatusCode::k404NotFound, 
        "The file does not exist");
    }
    else {
      FillMetaError(
        HttpStatusCode::k500InternalServerError, 
        "The page does exists, but error occurred in server");
      LOG_SYSERROR << "Failed to open the file: " << url_
          << "(But it exists)";
    }
    
    SendErrorResponse();
    return ;
  }

  HttpResponse response(true);

  off_t file_size = ::lseek(fd, 0, SEEK_END);
  ::lseek(fd, 0, SEEK_SET);

  LOG_DEBUG << "file_size = " << file_size;

  response.AddHeaderLine(HttpStatusCode::k200OK, HttpVersion::kHttp11)
          .AddHeader("Content-Length", std::to_string(file_size));

  if (is_keep_alive_) {
    response.AddHeader("Connection", "Keep-Alive");
  }
  
  response.AddBlackLine();

  server_->EmplaceOffset(this, 0);

  conn_->SetWriteCompleteCallback([fd, this](TcpConnectionPtr const& conn) {
    SendFile(fd);
  });

  LOG_DEBUG << "response: " << response.GetBuffer().ToStringView();
  conn_->Send(response.GetBuffer());
  
}

void HttpSession::SendFile(int fd)
{
  char buf[kFileBufferSize_];

  auto off = server_->SearchOffset(this);
  assert(off);
  auto readn = ::pread(fd, buf, sizeof buf, *off);

  if (readn != 0) {
    LOG_TRACE << "Sending file...";
    server_->offset_map_[this] += readn;
    conn_->Send(buf, readn);
  }
  else {
    LOG_TRACE << "File has been sent";
    FDWrapper wrapper(fd);
    server_->EraseOffset(this);

    conn_->SetWriteCompleteCallback(WriteCompleteCallback());

  }

  CloseConnection();
}

void HttpSession::ServeDynamicContent()
{
  plugin::PluginLoader<HttpDynamicResponseInterface> loader;

  assert(!is_static_);

  LOG_DEBUG << "query = " << query_;
  auto error = loader.Open(url_);

  if (error) {
    LOG_SYSERROR << "Failed to open shared object " << url_;
    FillMetaError(HttpStatusCode::k404NotFound, "The page is not found");
    SendErrorResponse();
    return ;
  }

  auto create_func = loader.GetCreateFunc();

  std::unique_ptr<HttpDynamicResponseInterface> response(create_func());

  if (method_ == HttpMethod::kPost) {
    NotImplementation();
  }
  else if (method_ == HttpMethod::kGet) {
    // The Content-Length is set by plugin
    HttpResponse first(true);
    first.AddHeaderLine(HttpStatusCode::k200OK, HttpVersion::kHttp11);
    
    if (is_keep_alive_) {
      first.AddHeader("Connection", "Keep-Alive");
    }
  
    conn_->Send(first.GetBuffer());
    conn_->Send(response->GenResponseForGet(SplitArgs()).GetBuffer());
  }

  CloseConnection();
}

ArgsMap HttpSession::SplitArgs()
{
  StringView::size_type equal_pos = StringView::npos;
  StringView::size_type and_pos = StringView::npos;
  std::string key;
  std::string val;

  ArgsMap kvs;

  StringView query = query_;

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

void HttpSession::CloseConnection()
{
  if (is_keep_alive_) {
    LOG_DEBUG << "Keep-Alive connection will keep 10s if no new message coming";
    timer_id_ = conn_->GetLoop()->RunAfter([this]() {
      conn_->ShutdownWrite();
    }, 10);
  }
  else {
    LOG_DEBUG << "Non-Keep-Alive(Close) Connection will be closed at immediately";
    conn_->ShutdownWrite();
  }
}

void HttpSession::SendErrorResponse()
{
  LOG_TRACE << "Error occurred, send error page to client";
  conn_->Send(GetClientError(
    meta_error_.first, meta_error_.second).GetBuffer());
  conn_->ShutdownWrite();
}

void HttpSession::NotImplementation()
{
  LOG_TRACE << "The service of " << GetMethodString(method_) << " is not implemetation";

  conn_->Send(GetClientError(
    HttpStatusCode::k501NotImplemeted,
    "The service is not implemeted").GetBuffer());
  conn_->ShutdownWrite();
}

auto HttpSession::Parse(kanon::Buffer& buffer) -> ParseResult
{
  StringView line;
  ParseResult ret;

  if (parse_phase_ == kFinished) {
    LOG_TRACE << "The parse state is reset";
    Reset();
  }

  while (parse_phase_ < kFinished) {
    const bool has_line = buffer.FindCrLf(line);

    if (!has_line) {
      break;
    }

    switch (parse_phase_) {
      case kHeaderLine:
        LOG_TRACE << "Start parsing the header line";

        ret = ParseHeaderLine(line);
        if (ret == kGood) {
          parse_phase_ = kHeaderFields;
        }
        break;
      case kHeaderFields:
        LOG_TRACE << "Start parsing headers fields";
        ret = ParseHeaderField(line);

        if (ret == kGood) {
          parse_phase_ = kFinished;
        }

        break;
      case kFinished:
        break;
    }

    buffer.AdvanceRead(line.size()+2);

    if (ret == kError) {
      return kError;
    }
    else if (parse_phase_ == kFinished) {
      LOG_TRACE << "The http request has been parsed";
      return kGood;
    }
  }

  return kShort;
}

auto HttpSession::ParseHeaderLine(kanon::StringView line) -> ParseResult
{
  auto space_pos = line.find(' ');

  if (space_pos == StringView::npos) {
    FillMetaErrorOfBadRequest("The method isn't provided");
    return kError;
  }

  ParseMethod(line.substr(0, space_pos));

  // First, parse the method
  if (method_ == HttpMethod::kNotSupport) {
    // Not a valid method
    FillMetaError(
      HttpStatusCode::k405MethodNotAllowd, 
      "The method is not supported(Check letters if are uppercase all)");
    return kError;
  }

  LOG_DEBUG << "Method = " << GetMethodString(method_);

  line.remove_prefix(space_pos+1);

  // Then, check the URL
  space_pos = line.find(' ');

  if (space_pos == StringView::npos) {
    FillMetaError(
      HttpStatusCode::k400BadRequest,
      "The URL isn't provided");
    return kError;
  }

  auto url = line.substr(0, space_pos);

  if (url.size() == 0) {
    FillMetaErrorOfBadRequest("The URL is empty");
    return kError;
  }

  LOG_DEBUG << "The URL = " << url;

  const auto url_size = url.size();
  url_ = url.ToString();

  // FIXME Server no need to consider scheme and host:port ? 

  if (url[0] != '/') {
    FillMetaErrorOfBadRequest("The first character of content path is not /");
    return kError;
  }

  url.remove_prefix(1);

  StringView::size_type slash_pos = StringView::npos;
  StringView directory;

  while ( (slash_pos = url.find('/') ) != StringView::npos) {
    directory = url.substr(0, slash_pos);

    if (directory.empty() || directory == ".." || directory == "." || directory.contains('%')) {
      is_complex_ = true;
    }

    url.remove_prefix(slash_pos+1);
  }

  if (url.contains('?')) {
    is_complex_ = true;
    is_static_ = false;
  }

  line.remove_prefix(url_size+1);

  // Check the http version
  auto http_version = line.substr(0, line.size());

  LOG_DEBUG << "Http version: " << http_version;  

  if (http_version.substr(0, 5).compare("HTTP/") != 0) {
    FillMetaErrorOfBadRequest("The HTTP in HTTP version isn't match");
    return kError;
  }

  http_version.remove_prefix(5);

  auto dot_pos = http_version.find('.');

  if (dot_pos == StringView::npos) {
    FillMetaErrorOfBadRequest("The dot of http version isn't provided");
    return kError;
  }

  // <major digit>.<minor digit>
  int version_code = 0;
  int version_digit = 0;
  for (size_t i = 0; i < dot_pos; ++i) {
    version_digit = version_digit * 10 + http_version[i] - '0';
  }

  version_code = version_digit * 100;
  version_digit = 0;

  for (auto i = dot_pos + 1; i < http_version.size(); ++i) {
    version_digit = version_digit * 10 + http_version[i] - '0';
  }

  version_code += version_digit;

  LOG_DEBUG << "Versioncode = " << version_code;
  ParseVersionCode(version_code);

  if (version_ == HttpVersion::kNotSupport) {
    FillMetaErrorOfBadRequest("The http version isn't supported");
    return kError;
  }

  if (!is_static_) {
    ParseComplexUrl();
  }

  return kGood;
}

void HttpSession::ParseMethod(kanon::StringView method) noexcept
{
  if (!method.compare("GET")) {
    method_ = HttpMethod::kGet;
  } 
  else if (!method.compare("POST")) {
    method_ = HttpMethod::kPost;
  } 
  else if (!method.compare("PUT")) {
    method_ = HttpMethod::kPut;
  } 
  else if (!method.compare("HEAD")) {
    method_ = HttpMethod::kHead;
  } 
  else {
    method_ = HttpMethod::kNotSupport;
  }
}

auto HttpSession::ParseComplexUrl() -> ParseResult
{
  /**
   * The following cases are complex:
   * 1. /./
   * 2. /../
   * 3. /?query_string
   * 4. % Hex Hex
   * 5. // (mutil slash)
   * 
   * To process such complex url and avoid multi traverse, 
   * use state machine to parse it, determine the next state
   * in the current character.
   * 
   * Detailed explanation:
   * If we use trivial traverse can handle one case only. 
   * Instead, state machine will store the state and process
   * the character and go to next state. According the state,
   * we can determine the best choice so avoid multi traverse.
   */
  std::string transfer_url;
  transfer_url.reserve(url_.size());

  enum ComplexUrlState {
    kUsual = 0,
    kQuery,
    kSlash,
    kDot,
    kDoubleDot,
    kPersentFirst,
    kPersentSecond,
  } state = kUsual;

  // Because there exists such case:
  // /% Hex Hex
  // And also %Hex Hex maybe . or %,
  // we should back to kSlash state
  auto persent_trap = state; // trap persent state to back previous state

  unsigned char decode_persent = 0; // Hexadecimal digit * 2 after %, i.e. % Hex Hex

  LOG_TRACE << "Start parsing the complex URL";

  for (auto c : url_) {
    switch (state) {
      case ComplexUrlState::kUsual:
        switch (c) {
        case '/':
          state = kSlash;
          transfer_url += c;
          break;
        case '%':
          persent_trap = state;
          state = ComplexUrlState::kPersentFirst;
          break;
        case '?':
          transfer_url += c;
          break;
        default:
          transfer_url += c;
          break;
        }
        
        break;
      
      case ComplexUrlState::kSlash:
        switch (c) {
        // multi slash, just ignore
        case '/':
          break;
        case '.':
          state = ComplexUrlState::kDot;
          break;
        case '%':
          persent_trap = state;
          state = ComplexUrlState::kPersentFirst;
          break;
        default:
          transfer_url += c;
          state = ComplexUrlState::kUsual;
          break;
        }
        break;

      case ComplexUrlState::kDot:
        switch (c) {
        case '/':
          // /./
          // Discard ./
          state = ComplexUrlState::kSlash;
          break;
        case '.':
          state = ComplexUrlState::kDoubleDot;
          break;
        case '%':
          persent_trap = state;
          state = ComplexUrlState::kPersentFirst;
          break;
        default:
          transfer_url += c;
          state = ComplexUrlState::kUsual;
          break;
        }
        break;

      case ComplexUrlState::kDoubleDot:
        switch (c) {
        // A/B/../ ==> A/
        case '/':
          transfer_url.erase(transfer_url.rfind('/', transfer_url.size() - 2) + 1);

          state = ComplexUrlState::kSlash;
          break;
        case '%':
          persent_trap = state;
          state = ComplexUrlState::kPersentFirst;
          break; 

        default:
          state = ComplexUrlState::kUsual;
          transfer_url += c;
          break;
        }
        break;
      
      case ComplexUrlState::kPersentFirst:
        if (std::isdigit(c)) {
          decode_persent = c - '0';
          state = ComplexUrlState::kPersentSecond;
          break;
        }
        
        c = c | 0x20; // to lower case letter

        if (std::isdigit(c)) {
          decode_persent = c - 'a' + 10;
          state = ComplexUrlState::kPersentSecond;
          break;
        }

        FillMetaErrorOfBadRequest("The first digit of persent-encoding is invalid");
        return kError;
        break; 

      case ComplexUrlState::kPersentSecond:
        if (std::isdigit(c)) {
          decode_persent = (decode_persent << 4) + c - '0';
          state = persent_trap;
          transfer_url += (char)decode_persent;
          break;
        }
        
        c = c | 0x20;

        if (std::isdigit(c)) {
          decode_persent = (decode_persent << 4) + c - 'a' + 10;
          state = persent_trap;
          transfer_url += (char)decode_persent;
          break;
        }

        FillMetaErrorOfBadRequest("The second digit of persent-encoding is invalid");
        return kError;
    } // end switch (state)
  } // end for

  if (is_static_) {
    url_ = std::move(transfer_url);
  }
  else {
    const auto query_pos = transfer_url.find('?');
    KANON_ASSERT(query_pos != std::string::npos, "The ? must be in the URL");

    url_ = transfer_url.substr(0, query_pos);
    query_ = transfer_url.substr(query_pos+1);
  }

  return kGood;
}

auto HttpSession::ParseHeaderField(StringView header) -> ParseResult
{
  if (header.empty()) {
    return kGood;
  }

  auto colon_pos = header.find(':');

  if (colon_pos == StringView::npos) {
    FillMetaErrorOfBadRequest("The : of header isn't provided");
    return kError;
  }
  
  LOG_DEBUG << "Header field: [" << header.substr(0, colon_pos) << 
    ": " << header.substr(colon_pos+2) << "]";

  headers_.emplace(
    header.substr(0, colon_pos).ToString(), 
    header.substr(colon_pos+2).ToString());

  return kShort;
}

void HttpSession::ParseVersionCode(int version_code) noexcept
{
  switch (version_code) {
    case 101:
      version_ = HttpVersion::kHttp11;
      break;
    case 100:
      version_ = HttpVersion::kHttp10;
      break;
    default:
      version_ = HttpVersion::kNotSupport;
  }
}

void HttpSession::Reset()
{
  parse_phase_ = kHeaderLine;
  is_static_   = true;
  is_complex_  = false;
  method_      = HttpMethod::kNotSupport;
  version_     = HttpVersion::kNotSupport;
  headers_.clear();
  // No need to reset the url_ and query_
  // we can overwrite it and reuse the heap 
  // has allocated
}

} // namespace http
