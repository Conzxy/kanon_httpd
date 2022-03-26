#include "common/http_response.h"

#include <iostream>
#include <kanon/log/logger.h>

using namespace http;

int main() {
  auto response = GetClientError(HttpStatusCode::k400BadRequest, "Bad Request");

  LOG_DEBUG << "xxxx"; 
  auto str = response.GetBuffer().RetrieveAllAsString();

  std::cout << str;
}
