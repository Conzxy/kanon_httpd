#include "http/http_response.h"

#include <iostream>
#include <kanon/log/logger.h>

IMPORT_NAMESPACE( http );

int main() {
  auto response = GetClientError(HttpStatusCode::k400BadRequest, "Bad Request");

  LOG_DEBUG << "xxxx"; 
  auto str = response.GetBuffer().RetrieveAllAsString();

  std::cout << str;
}
