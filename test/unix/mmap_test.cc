#include "unix/mmap.h"
#include "unix/stat.h"
#include <gtest/gtest.h>

// This is a incorrect approach for using mmap()
// Since the memory area just used once
// \see https://marc.info/?l=linux-kernel&m=95496636207616&w=2
using namespace unix;

auto pathname = "/root/kanon_httpd/resources/db.pdf";
constexpr uint32_t BUFFER_SIZE = 1 << 21;
constexpr int32_t N = 10000;

TEST(mmap_test, read) {

  for (int i = 0; i < N; ++i) {
    char* addr = Mmap(pathname, BUFFER_SIZE, PROT_READ, MAP_PRIVATE);
    char buf[BUFFER_SIZE];
    ::memcpy(buf, addr, BUFFER_SIZE);
    Munmap(addr, BUFFER_SIZE);
  }
}

TEST(mmap_test, plain_read) {
  for (int i = 0; i < N; ++i) {
    auto fd = ::open(pathname, O_RDONLY);
    char buf[BUFFER_SIZE];
    ::read(fd, buf, BUFFER_SIZE);
    ::close(fd);
  }
}

int main() {
  ::testing::InitGoogleTest();

  return RUN_ALL_TESTS();
}