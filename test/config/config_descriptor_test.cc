#include <gtest/gtest.h>

#include <kanon/log/logger.h>

#include "config/config_descriptor.h"


using namespace config;

TEST(config_descriptor_test, read) {
  ConfigDescriptor config("/root/kanon_http/bin/kanon_http.conf");

  config.Read();

  auto const& h = config.GetParameters();

  for (auto const& head : h) {
    LOG_DEBUG << head.first << ": " << head.second;
  }

  EXPECT_EQ(*config.GetParameter("HomePagePath"), "/root/kanon_http/html");
  EXPECT_EQ(*config.GetParameter("HomePageName"), "index.html");
  EXPECT_EQ(*config.GetParameter("Host"), "47.99.92.230");
  EXPECT_EQ(*config.GetParameter("RootPath"), "/root/kanon_http");
  EXPECT_EQ(*config.GetParameter("Port"), "80");
}

int main()
{
  ::testing::InitGoogleTest();

  return RUN_ALL_TESTS(); 
}