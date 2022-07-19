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
#include "http2/http_parser.h"
#include "http2/http_request.h"
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
  , keep_alive_timer_id_()
  , connection_timer_id_()
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
  
  HttpParser parser;
  HttpRequest request;
  HttpParser::ParseResult ret;

  if ( (ret = parser.Parse(buffer, &request) ) == HttpParser::kGood) {
    if (request.url == "/") {
      request.url += g_config.homepage_path;
    }
    request.url.insert(0, g_config.root_path.data(), g_config.root_path.size());
    LOG_DEBUG << "content_path = " << request.url;

    switch (request.method) {
      case HttpMethod::kGet:
        if (request.is_static) {
          ServeFile(request);
        }
        else {
          ServeDynamicContent(request);
        }
      break;

      case HttpMethod::kPost:
        ServeDynamicContent(request);
      break;

      default:
        NotImplementation(request);
    }      
  }

  if (ret == HttpParser::kError) {
    error_ = std::move(parser.error());
    SendErrorResponse();
  }
}

void HttpSession::ServeFile(HttpRequest const& req)
{
  Stat stat;
  bool success = stat.Open(req.url);

  if (SetErrorOfStat(stat, success)) {
    return ;
  }

  HttpResponse response(true);

  off_t file_size = stat.GetFileSize();
  cur_filesize_ = file_size;

  LOG_DEBUG << "file_size = " << file_size;

  response.AddHeaderLine(HttpStatusCode::k200OK, HttpVersion::kHttp11)
          .AddContentType(req.url)
          .AddHeader("Content-Length", std::to_string(file_size));

  if (req.is_keep_alive) {
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
    fd = server_->GetFd(req.url);
  
    if (!fd) {
      SetErrorOfGetFdOrGetAddr(req);
      return ;
    }
    
    char tmp_buf[kFileBufferSize_];
    readn = ::pread(*fd, tmp_buf, kFileBufferSize_ - response.GetBuffer().GetReadableSize(), 0);
    buf = tmp_buf;
  } else {
    addr = server_->GetAddr(req.url, file_size);

    if (!addr) {
      SetErrorOfGetFdOrGetAddr(req);
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
      conn_->SetWriteCompleteCallback([&req, fd, session = this](TcpConnectionPtr const& conn) {
        KANON_UNUSED(conn);
        return session->SendFile(fd, req);
      });
    } else {
      conn_->SetWriteCompleteCallback([&req, addr, session = this](TcpConnectionPtr const& conn) {
        KANON_UNUSED(conn);
        return session->SendFileOfMmap(addr, req);
      });

    }

    conn_->Send(response.GetBuffer());
  } else {
    LOG_DEBUG << "File has been sent";

    SetLastWriteComplete(req);
    conn_->Send(response.GetBuffer());
  }
  
}

bool HttpSession::SendFile(std::shared_ptr<int> const& fd, HttpRequest const& req)
{
  char buf[kFileBufferSize_];

  auto readn = ::pread(*fd, buf, sizeof buf, cache_filesize_);
  
  LOG_DEBUG << "readn = " << readn;
  LOG_DEBUG << "The offset = " << cache_filesize_;

  if (readn < 0) {
    LOG_ERROR << "pread error";
    error_ = {HttpStatusCode::k500InternalServerError, "pread() error"};
    SendErrorResponse();
    return true;
  } else if (readn > 0) {
    cache_filesize_ += readn;
    if (cur_filesize_ == cache_filesize_) {
      LOG_DEBUG << "File has been sent";

      SetLastWriteComplete(req); 
      conn_->Send(buf, readn);

      return !conn_->GetOutputBuffer()->HasReadable();
    } else {
      conn_->Send(buf, readn);
      return false;
    }
      
  } else {
    LOG_DEBUG << "File has been sent";

    conn_->SetWriteCompleteCallback(WriteCompleteCallback());
    CloseConnection(req);
    return true;
  }
}

bool HttpSession::SendFileOfMmap(std::shared_ptr<char*> const& addr, HttpRequest const& req) {
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
    SetLastWriteComplete(req);
    conn_->Send(*guard + cache_filesize_, left);

    return !conn_->GetOutputBuffer()->HasReadable();
  }
}

void HttpSession::ServeDynamicContent(HttpRequest const& req)
{
  plugin::PluginLoader<HttpDynamicResponseInterface> loader;

  assert(!req.is_static);

  LOG_DEBUG << "query = " << req.query;
  auto error = loader.Open(req.url);

  if (error) {
    LOG_SYSERROR << "Failed to open shared object: " << req.url;
    LOG_SYSERROR << "Error Message: " << *error;
    error_ = {HttpStatusCode::k404NotFound, "The page is not found"};
    SendErrorResponse();
    return ;
  }

  auto create_func = loader.GetCreateFunc();

  std::unique_ptr<HttpDynamicResponseInterface> generator(create_func());
  generator->SetConnection(conn_);
  generator->SetVersion(req.version);

  HttpResponse first(false);

  assert(req.version != HttpVersion::kNotSupport);
  first.AddHeaderLine(HttpStatusCode::k200OK, req.version);
  
  if (req.is_keep_alive) {
    first.AddHeader("Connection", "Keep-Alive");
  }

  if (req.method == HttpMethod::kPost) {
    generator->GenResponseForPost(req.body, first);
  }
  else if (req.method == HttpMethod::kGet) {
    generator->GenResponseForGet(ParseArgs(req.query), first);
  }
}

void HttpSession::CloseConnection(HttpRequest const& req)
{
  if (req.is_keep_alive) {
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
  LOG_DEBUG << "Error: (" << GetStatusCode(error_.code)
    << ", " << GetStatusCodeString(error_.code)
    << ", " << error_.msg << ")";

  conn_->Send(GetClientError(
    error_.code, error_.msg).GetBuffer());

  CancelKeepAliveTimer();
  conn_->ShutdownWrite();
}

void HttpSession::NotImplementation(HttpRequest const& req)
{
  LOG_TRACE << "The service of " << GetMethodString(req.method) << " is not implemeted";

  conn_->Send(GetClientError(
    HttpStatusCode::k501NotImplemeted,
    "The service is not implemeted").GetBuffer());
  conn_->ShutdownWrite();
}

void HttpSession::CancelKeepAliveTimer()
{
  if (keep_alive_timer_id_) {
    conn_->GetLoop()->CancelTimer(*keep_alive_timer_id_);
    keep_alive_timer_id_ = kanon::optional<TimerId>();
    LOG_DEBUG << "The keep-alive timer is canceled";
  }
}

void HttpSession::CancelConnectionTimeoutTimer()
{
  if (connection_timer_id_) {
    conn_->GetLoop()->CancelTimer(*connection_timer_id_);
    connection_timer_id_ = kanon::optional<TimerId>();

    LOG_DEBUG << "The connection-timeout timer is canceled";
  }
}

void HttpSession::SetErrorOfGetFdOrGetAddr(HttpRequest const& req) {
  if (errno != 0) {
    error_ = {HttpStatusCode::k500InternalServerError, 
              "The pape does exists, but error occurred in server"};
    LOG_SYSERROR << "Failed to open the file: " << req.url
        << "(But it exists)";
  }
  else {
    error_ = {HttpStatusCode::k500InternalServerError, 
               "Unknown error"};
    LOG_ERROR << "Unknown error of GetFd()";
  }
  
  SendErrorResponse();
}

bool HttpSession::SetErrorOfStat(Stat& stat, bool success) {
  if (!success) {
    if (errno == ENOENT) {
      error_ = {HttpStatusCode::k404NotFound,
                "The file does not exist"};
    } else if (errno == EACCES) {
      error_ = {HttpStatusCode::k403Forbidden,
                "No permission to access the file"};
    } else {
      error_ = {HttpStatusCode::k500InternalServerError,
                "Can't get any infomation of the file"};
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

    error_ = {HttpStatusCode::k403Forbidden,
              "Can't read requested file"};
    SendErrorResponse();
    return true;
  }

  return false;

}

void HttpSession::SetLastWriteComplete(HttpRequest const& req) {
  conn_->SetWriteCompleteCallback([session = this, &req] (TcpConnectionPtr const& conn) {
    (void)conn;
    session->CloseConnection(req);
    return true;
  });
}

} // namespace http
