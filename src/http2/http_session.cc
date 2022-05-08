#include "http_session.h"

#include <strings.h>
#include <unistd.h>
#include <fcntl.h>

#include <kanon/log/logger.h>
#include <kanon/net/buffer.h>
#include <kanon/net/callback.h>
#include <kanon/string/string_view.h>
#include <kanon/util/macro.h>
#include <kanon/util/ptr.h>

#include "common/http_response.h"
#include "common/http_constant.h"
#include "common/parse_args.h"
#include "config/http_config.h"
#include "plugin/plugin_loader.h"
#include "plugin/http_dynamic_response_interface.h"
#include "unix/fd_wrapper.h"
#include "unix/stat.h"

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
  , keep_alive_timer_id_()
  , connection_timer_id_()
  , is_keep_alive_(false)
  , content_length_(-1)
  , id_(counter_.GetAndAdd(1))
{
}

HttpSession::HttpSession(HttpServer& server, TcpConnectionPtr const& conn)
  : HttpSession()
{
  server_ = &server;
  conn_ = conn;
}

void HttpSession::Setup() {
  LOG_DEBUG << "This new established connection will be closed after 60s if no message coming";
  connection_timer_id_ = conn_->GetLoop()->RunAfter([session = this]() {
    session->conn_->ShutdownWrite();
  }, 60);

  conn_->SetMessageCallback(std::bind(
    &HttpSession::OnMessage, this, kanon::_1, kanon::_2, kanon::_3));
}

HttpSession::~HttpSession() noexcept
{
  LOG_DEBUG << "HttpSession " << id_ << " is destroyed";
}

void HttpSession::Teardown() {
  CancelKeepAliveTimer(); 
  CancelConnectionTimeoutTimer();
}

void HttpSession::OnMessage(TcpConnectionPtr const& conn, Buffer& buffer, TimeStamp recv_time)
{
  KANON_UNUSED(conn);
  KANON_UNUSED(recv_time);

  LOG_DEBUG << "The HTTP REQUEST CONTENTS: ";
  LOG_DEBUG << buffer.ToStringView();

  CancelConnectionTimeoutTimer();
  CancelKeepAliveTimer();

  ParseResult ret;
  if ( ( ret = Parse(buffer) ) == kGood) {
    if (url_ == "/") {
      url_ += g_config.homepage_path;
    }
    url_.insert(0, g_config.root_path.data(), g_config.root_path.size());
    LOG_DEBUG << "content_path = " << url_;

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
  Stat stat;
  bool success = stat.Open(url_);

  if (FillErrorOfStat(stat, success)) {
    return ;
  }

  HttpResponse response(true);

  off_t file_size = stat.GetFileSize();
  cur_filesize_ = file_size;

  LOG_DEBUG << "file_size = " << file_size;

  response.AddHeaderLine(HttpStatusCode::k200OK, HttpVersion::kHttp11)
          .AddContentType(url_)
          .AddHeader("Content-Length", std::to_string(file_size));

  if (is_keep_alive_) {
    response.AddHeader("Connection", "Keep-Alive")
            .AddHeader("Keep-Alive", "timeout=5");
  }
  
  response.AddBlackLine();

  LOG_DEBUG << response.GetBuffer().ToStringView();

  char* buf = nullptr;
  ssize_t readn = 0;

  std::shared_ptr<int> fd = nullptr;
  std::shared_ptr<char*> addr = nullptr;

  if (!g_config.use_mmap) {
    fd = server_->GetFd(url_);
  
    if (!fd) {
      FillErrorOfGetFdOrGetAddr();
      return ;
    }
    
    char tmp_buf[kFileBufferSize_];
    readn = ::pread(*fd, tmp_buf, kFileBufferSize_ - response.GetBuffer().GetReadableSize(), 0);
    buf = tmp_buf;
  } else {
    addr = server_->GetAddr(url_, file_size);

    if (!addr) {
      FillErrorOfGetFdOrGetAddr();
      return ;
    }

    buf = *addr;

    readn = kFileBufferSize_ - response.GetBuffer().GetReadableSize();

    if (readn >= file_size) {
      readn = file_size;      
    }
  }

  response.AddBody(buf, readn);

  if (readn < file_size) {
    cache_filesize_ += readn;
    LOG_DEBUG << "Sending file...";

    if (!g_config.use_mmap) {
      conn_->SetWriteCompleteCallback([fd, session = this](TcpConnectionPtr const& conn) {
        KANON_UNUSED(conn);
        return session->SendFile(fd);
      });
    } else {
      conn_->SetWriteCompleteCallback([addr, session = this](TcpConnectionPtr const& conn) {
        KANON_UNUSED(conn);
        return session->SendFileOfMmap(addr);
      });

    }

    conn_->Send(response.GetBuffer());
  } else {
    LOG_DEBUG << "File has been sent";

    SetLastWriteComplete();
    conn_->Send(response.GetBuffer());
  }
  
}

bool HttpSession::SendFile(std::shared_ptr<int> const& fd)
{
  char buf[kFileBufferSize_];

  auto readn = ::pread(*fd, buf, sizeof buf, cache_filesize_);
  
  LOG_DEBUG << "readn = " << readn;
  LOG_DEBUG << "The offset = " << cache_filesize_;

  if (readn < 0) {
    LOG_ERROR << "pread error";
    FillMetaError(HttpStatusCode::k500InternalServerError, "pread() error");
    SendErrorResponse();
    return true;
  } else if (readn > 0) {
    cache_filesize_ += readn;
    if (cur_filesize_ == cache_filesize_) {
      LOG_DEBUG << "File has been sent";

      SetLastWriteComplete(); 
      conn_->Send(buf, readn);

      return !conn_->GetOutputBuffer()->HasReadable();
    } else {
      conn_->Send(buf, readn);
      return false;
    }
      
  } else {
    LOG_DEBUG << "File has been sent";

    conn_->SetWriteCompleteCallback(WriteCompleteCallback());
    CloseConnection();
    return true;
  }
}

bool HttpSession::SendFileOfMmap(std::shared_ptr<char*> const& addr) {
  auto left = cur_filesize_ - cache_filesize_; 

  LOG_DEBUG << "The offset = " << cache_filesize_ << "; left = " << left;

  if (left > kFileBufferSize_) {
    conn_->Send(*addr + cache_filesize_, kFileBufferSize_);
    cache_filesize_ += kFileBufferSize_;
    return false;
  } else {
    LOG_DEBUG << "File has been sent";

    // SetLastWriteComplete() will override the addr
    // Must guard here!
    auto guard = addr;
    SetLastWriteComplete();
    conn_->Send(*guard + cache_filesize_, left);

    return !conn_->GetOutputBuffer()->HasReadable();
  }
}

void HttpSession::ServeDynamicContent()
{
  plugin::PluginLoader<HttpDynamicResponseInterface> loader;

  assert(!is_static_);

  LOG_DEBUG << "query = " << query_;
  auto error = loader.Open(url_);

  if (error) {
    LOG_SYSERROR << "Failed to open shared object: " << url_;
    LOG_SYSERROR << "Error Message: " << *error;
    FillMetaError(HttpStatusCode::k404NotFound, "The page is not found");
    SendErrorResponse();
    return ;
  }

  auto create_func = loader.GetCreateFunc();

  std::unique_ptr<HttpDynamicResponseInterface> generator(create_func());
  generator->SetConnection(conn_);
  generator->SetVersion(version_);

  HttpResponse first(false);

  assert(version_ != HttpVersion::kNotSupport);
  first.AddHeaderLine(HttpStatusCode::k200OK, version_);
  
  if (is_keep_alive_) {
    first.AddHeader("Connection", "Keep-Alive");
  }

  if (method_ == HttpMethod::kPost) {
    generator->GenResponseForPost(body_, first);
  }
  else if (method_ == HttpMethod::kGet) {
    generator->GenResponseForGet(SplitArgs(), first);
  }
}

ArgsMap HttpSession::SplitArgs()
{
  return ParseArgs(query_);
}

void HttpSession::CloseConnection()
{
  if (is_keep_alive_) {
    LOG_DEBUG << "Keep-Alive connection will keep 5s if no new message coming";
    keep_alive_timer_id_ = conn_->GetLoop()->RunAfter([session = this]() {
      session->conn_->ShutdownWrite();
    }, 5);
  }
  else {
    LOG_DEBUG << "Non-Keep-Alive(Close) Connection will be closed at immediately";
    conn_->ShutdownWrite();
  }
}

void HttpSession::SendErrorResponse()
{
  LOG_TRACE << "Error occurred, send error page to client";
  LOG_DEBUG << "Error: (" << GetStatusCode(meta_error_.first)
    << ", " << GetStatusCodeString(meta_error_.first)
    << ", " << meta_error_.second << ")";

  conn_->Send(GetClientError(
    meta_error_.first, meta_error_.second).GetBuffer());

  CancelKeepAliveTimer();
  conn_->ShutdownWrite();
}

void HttpSession::NotImplementation()
{
  LOG_TRACE << "The service of " << GetMethodString(method_) << " is not implemeted";

  conn_->Send(GetClientError(
    HttpStatusCode::k501NotImplemeted,
    "The service is not implemeted").GetBuffer());
  conn_->ShutdownWrite();
}

auto HttpSession::Parse(kanon::Buffer& buffer) -> ParseResult
{
  ParseResult ret = kShort;

  if (parse_phase_ == kFinished) {
    LOG_TRACE << "The parse state is reset";
    Reset();
  }

  while (parse_phase_ < kFinished) {
    if (parse_phase_ < kBody) {
      assert(parse_phase_ == kHeaderFields || parse_phase_ == kHeaderLine);

      StringView line;

      const bool has_line = buffer.FindCrLf(line);

      if (!has_line) {
        break;
      }

      if (parse_phase_ == kHeaderLine) {
        LOG_TRACE << "Start parsing the header line";

        ret = ParseHeaderLine(line);
        if (ret == kGood) {
          buffer.AdvanceRead(line.size()+2);
          parse_phase_ = kHeaderFields;
        }

        assert(ret != kShort);
      }
      else if (parse_phase_ == kHeaderFields) {
        LOG_TRACE << "Start parsing headers fields";
        ret = ParseHeaderField(line);

        if (ret == kGood) {
          parse_phase_ = kBody;
        }

        if (ret != kError) {
          buffer.AdvanceRead(line.size()+2);
        }
      }
    }
    else {
      assert(parse_phase_ == kBody);
      SetHeaderMetadata();

      if (content_length_ != static_cast<uint64_t>(-1)) {
        LOG_TRACE << "Start extracting the body";
        ret = ExtractBody(buffer);

        if (ret == kGood) {
          parse_phase_ = kFinished;
        }
      }
      else {
        parse_phase_ = kFinished;
      }
    }

    if (ret == kError) {
      return kError;
    }

    if (parse_phase_ == kFinished) {
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

  if (method_ == HttpMethod::kPost) {
    is_static_ = false;
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

  if (is_complex_) {
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

auto HttpSession::ExtractBody(Buffer& buffer) -> ParseResult
{
  if (buffer.GetReadableSize() >= content_length_) {
    LOG_DEBUG << "Buffer readable size = " << buffer.GetReadableSize();
    body_ = buffer.RetrieveAsString(content_length_);
    LOG_DEBUG << "HTTP REQUEST BODY: ";
    LOG_DEBUG << body_;

    return kGood;
  }

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
  // No need to reset meta_error_
  is_static_   = true;
  is_complex_  = false;
  method_      = HttpMethod::kNotSupport;
  version_     = HttpVersion::kNotSupport;

  is_keep_alive_ = false;
  content_length_ = -1;
  headers_.clear();
  cache_filesize_ = 0;
  cur_filesize_ = 0;

  CancelConnectionTimeoutTimer();
  CancelKeepAliveTimer();
  // No need to reset the url_ and query_
  // we can overwrite it and reuse the heap 
  // has allocated
}

uint64_t HttpSession::Str2U64(std::string const& str)
{
  return ::strtoull(str.c_str(), NULL, 10);
}

void HttpSession::SetHeaderMetadata()
{
    auto iter = headers_.find("Connection");

    switch (version_) {
    case HttpVersion::kHttp10:
      // In 1.0, default is close
      if (iter != headers_.end() && !::strncasecmp(iter->second.c_str(), "keep-alive", iter->second.size())) {
        is_keep_alive_ = true;
        LOG_DEBUG << "The connection is keep-alive";
      } else {
        is_keep_alive_ = false;
        LOG_DEBUG << "The connection is close";
      }
      break;

    case HttpVersion::kHttp11:
      // In 1.1, default is keep-alive
      if (iter != headers_.end() && !::strncasecmp(iter->second.c_str(), "close", iter->second.size())) {
        is_keep_alive_ = false;
        LOG_DEBUG << "The connection is close";
      } else {
        is_keep_alive_ = true;
        LOG_DEBUG << "The connection is keep-alive";
      }
      break;

    case HttpVersion::kNotSupport:
    default:
      is_keep_alive_ = false;
      LOG_DEBUG << "The connection is keep-alive";

    }

    iter = headers_.find("Content-Length");

    if (iter != std::end(headers_)) {
      content_length_ = Str2U64(iter->second);
      LOG_DEBUG << "The Content-Length = " << content_length_;
    }
}

void HttpSession::CancelKeepAliveTimer()
{
  if (keep_alive_timer_id_) {
    conn_->GetLoop()->CancelTimer(*keep_alive_timer_id_);
    keep_alive_timer_id_.SetInvalid();
    LOG_DEBUG << "The keep-alive timer is canceled";
  }
}

void HttpSession::CancelConnectionTimeoutTimer()
{
  if (connection_timer_id_) {
    conn_->GetLoop()->CancelTimer(*connection_timer_id_);
    connection_timer_id_.SetInvalid();

    LOG_DEBUG << "The connection-timeout timer is canceled";
  }
}

void HttpSession::FillErrorOfGetFdOrGetAddr() {
  if (errno != 0) {
    FillMetaError(
      HttpStatusCode::k500InternalServerError, 
      "The page does exists, but error occurred in server");
    LOG_SYSERROR << "Failed to open the file: " << url_
        << "(But it exists)";
  }
  else {
    FillMetaError(
      HttpStatusCode::k500InternalServerError, 
      "Unknown error");
    LOG_ERROR << "Unknown error of GetFd()";
  }
  
  SendErrorResponse();
}

bool HttpSession::FillErrorOfStat(Stat& stat, bool success) {
  if (!success) {
    if (errno == ENOENT) {
      FillMetaError(
        HttpStatusCode::k404NotFound,
        "The file does not exist");
    } else if (errno == EACCES) {
      FillMetaError(
        HttpStatusCode::k403Forbidden,
        "No permission to access the file");
    } else {
      FillMetaError(
        HttpStatusCode::k500InternalServerError,
        "Can't get any infomation of the file");
    }

    SendErrorResponse();
    return true;
  } 

  if (!stat.IsRegular() || !stat.IsUserR()) {
    if (!stat.IsRegular()) {
      LOG_DEBUG << "This file is not regular";
    }

    if (!stat.IsUserR()) {
      LOG_DEBUG << "This file can't be read";
    }

    FillMetaError(
      HttpStatusCode::k403Forbidden,
      "Can't read requested file");
    SendErrorResponse();
    return true;
  }

  return false;

}

void HttpSession::SetLastWriteComplete() {
  conn_->SetWriteCompleteCallback([session = this] (TcpConnectionPtr const& conn) {
    session->CloseConnection();
    return true;
  });
}

} // namespace http