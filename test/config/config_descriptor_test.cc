#include <gtest/gtest.h>

#include <kanon/log/logger.h>

#define private public

#include "config/config_descriptor.h"


using namespace config;

TEST(config_descriptor_test, read) {
  ConfigDescriptor config("/root/kanon_http/test/config/kanon_http_conf");

  config.Read();

  auto& h = config.fields_;

  for (auto const& head : h) {
    LOG_DEBUG << head.first << ": " << head.second;
  }

  EXPECT_EQ(config.GetField("HomePagePath").ToString(), "/root/kanon_http/html");
  EXPECT_EQ(config.GetField("HomePage").ToString(), "index.html");
}

int main()
{
  ::testing::InitGoogleTest();

  return RUN_ALL_TESTS(); 
}