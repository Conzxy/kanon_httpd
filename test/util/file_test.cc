#include "util/file.h"

#include <string.h>

#include <gtest/gtest.h>
#include <kanon/log/logger.h>

using namespace kanon;
using namespace http;

TEST(file_test, read) {
  try {
    File fd("/root/kanon/example/http/html/kanon.jpg", File::kRead);

    char buf[1 << 16];

    size_t total = 0;
    size_t cnt = 0;
    for (size_t readn = fd.Read(buf); readn != (size_t)-1; readn = fd.Read(buf)) {
      cnt++;
      total += readn;
      // printf("%s", buf);
      memset(buf, 0, sizeof buf);
      if (fd.IsEof()) break;
    }

    printf("\ntotal: %lu, cnt = %lu\n", total, cnt);
    printf("file size: %lu\n", fd.GetSize());
  }
  catch (FileException const& ex) {
    printf("%s", ex.what());
  }
}

TEST(file_test, readline) {
  File file;
  auto ret = file.Open("/root/kanon_http/test/util/test.txt", File::kRead);

  if (!ret) {
    LOG_SYSERROR << "file error";
  }

  std::string line;
  line.reserve(16);

  int count = 0;
  while (file.ReadLine(line)) {
    if (file.IsEof()) break;
    std::cout << count++ << ": " << line;
    line.clear();
  }
}

int main()
{
  ::testing::InitGoogleTest();

  return RUN_ALL_TESTS();
}