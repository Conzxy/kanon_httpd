#include "http_parser.h"
#include "common/http_constant.h"
#include "http2/http_request.h"
#include "http2/http_server2.h"

using namespace http;

HttpParser::ParseResult HttpParser::Parse(kanon::Buffer& buffer, HttpRequest* request) {
  ParseResult ret = kShort;

  if (parse_phase_ == kFinished) {
    LOG_TRACE << "The parse state is reset";
    Reset();
  }

  while (parse_phase_ < kFinished) {
    if (parse_phase_ < kBody) {
      assert(parse_phase_ == kHeaderFields || parse_phase_ == kHeaderLine);

      kanon::StringView line;

      const bool has_line = buffer.FindCrLf(line);

      if (!has_line) {
        break;
      }

      if (parse_phase_ == kHeaderLine) {
        LOG_TRACE << "Start parsing the header line";

        ret = ParseHeaderLine(line, request);
        if (ret == kGood) {
          buffer.AdvanceRead(line.size()+2);
          parse_phase_ = kHeaderFields;
        }

        assert(ret != kShort);
      }
      else if (parse_phase_ == kHeaderFields) {
        LOG_TRACE << "Start parsing headers fields";
        ret = ParseHeaderField(line, request);

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
      SetHeaderMetadata(request);

      if (content_length_ != static_cast<uint64_t>(-1)) {
        LOG_TRACE << "Start extracting the body";
        ret = ExtractBody(buffer, request);

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

HttpParser::ParseResult HttpParser::ParseHeaderLine(StringView line, HttpRequest* request) {
  auto space_pos = line.find(' ');

  if (space_pos == StringView::npos) {
    error_ = {
      HttpStatusCode::k400BadRequest, 
      "The method isn't provided"};

    return kError;
  }

  ParseMethod(line.substr(0, space_pos), request);

  // First, parse the method
  if (request->method == HttpMethod::kNotSupport) {
    // Not a valid method
    error_ = {
      HttpStatusCode::k405MethodNotAllowd, 
      "The method is not supported(Check letters if are uppercase all)"};
    return kError;
  }

  if (request->method == HttpMethod::kPost) {
    request->is_static = false;
  }

  LOG_DEBUG << "Method = " << GetMethodString(request->method);

  line.remove_prefix(space_pos+1);

  // Then, check the URL
  space_pos = line.find(' ');

  if (space_pos == StringView::npos) {
    error_ = {
      HttpStatusCode::k400BadRequest,
      "The URL isn't provided"};
    return kError;
  }

  auto url = line.substr(0, space_pos);

  if (url.size() == 0) {
    error_ = {
      HttpStatusCode::k400BadRequest,
      "The URL is empty"};
    return kError;
  }

  LOG_DEBUG << "The URL = " << url;

  const auto url_size = url.size();
  request->url = url.ToString();

  // FIXME Server no need to consider scheme and host:port ? 

  if (url[0] != '/') {
    error_ = {
      HttpStatusCode::k400BadRequest,
      "The first character of content path is not /"};
    return kError;
  }

  url.remove_prefix(1);

  StringView::size_type slash_pos = StringView::npos;
  StringView directory;

  while ( (slash_pos = url.find('/') ) != StringView::npos) {
    directory = url.substr(0, slash_pos);

    if (directory.empty() || directory == ".." || directory == "." || directory.contains('%')) {
      request->is_complex = true;
    }

    url.remove_prefix(slash_pos+1);
  }

  if (url.contains('?')) {
    request->is_complex = true;
    request->is_static = false;
  }

  line.remove_prefix(url_size+1);

  // Check the http version
  auto http_version = line.substr(0, line.size());

  LOG_DEBUG << "Http version: " << http_version;  

  if (http_version.substr(0, 5).compare("HTTP/") != 0) {
    error_ = {
      HttpStatusCode::k400BadRequest,
      "The HTTP in HTTP version isn't match"};
    return kError;
  }

  http_version.remove_prefix(5);

  auto dot_pos = http_version.find('.');

  if (dot_pos == StringView::npos) {
    error_ = {
      HttpStatusCode::k400BadRequest,
      "The dot of http version isn't provided"};
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
  ParseVersionCode(version_code, request);

  if (request->version == HttpVersion::kNotSupport) {
    error_ = {
      HttpStatusCode::k400BadRequest, 
      "The http version isn't supported"};
    return kError;
  }

  if (request->is_complex) {
    ParseComplexUrl(request);
  }

  return kGood;
}

HttpParser::ParseResult HttpParser::ParseComplexUrl(HttpRequest* request) {
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
  transfer_url.reserve(request->url.size());

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

  for (auto c : request->url) {
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
        
        error_ = {
          HttpStatusCode::k400BadRequest,
          "The first digit of persent-encoding is invalid"};
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
        
        error_ = {
          HttpStatusCode::k400BadRequest,
          "The second digit of persent-encoding is invalid"};
        return kError;
    } // end switch (state)
  } // end for

  if (request->is_static) {
    request->url = std::move(transfer_url);
  } else {
    const auto query_pos = transfer_url.find('?');
    KANON_ASSERT(query_pos != std::string::npos, "The ? must be in the URL");

    request->url = transfer_url.substr(0, query_pos);
    request->query = transfer_url.substr(query_pos+1);
  }

  return kGood;
}

HttpParser::ParseResult HttpParser::ParseHeaderField(StringView header, HttpRequest* request) {
  if (header.empty()) {
    return kGood;
  }

  auto colon_pos = header.find(':');

  if (colon_pos == StringView::npos) {
    error_.msg = "The : of header isn't provided";
    return kError;
  }
  
  LOG_DEBUG << "Header field: [" << header.substr(0, colon_pos) << 
    ": " << header.substr(colon_pos+2) << "]";

  request->headers.emplace(
    header.substr(0, colon_pos).ToString(), 
    header.substr(colon_pos+2).ToString());

  return kShort;
}

HttpParser::ParseResult HttpParser::ExtractBody(Buffer& buffer, HttpRequest* request) {
  if (buffer.GetReadableSize() >= content_length_) {
    LOG_DEBUG << "Buffer readable size = " << buffer.GetReadableSize();
    request->body = buffer.RetrieveAsString(content_length_);
    LOG_DEBUG << "HTTP REQUEST BODY: ";
    LOG_DEBUG << request->body;

    return kGood;
  }

  return kShort;
}


