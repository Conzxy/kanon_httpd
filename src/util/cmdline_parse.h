#ifndef KANON_UTIL_CMDLINE_PARSE_H
#define KANON_UTIL_CMDLINE_PARSE_H

#include <iostream>
#include <boost/program_options.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>

#include "config/http_options.h"

namespace po = boost::program_options;

inline bool ParseCmdLine(
  int argc, char* argv[], 
  http::HttpOptions& options)
{
  po::options_description od("Allowed options");

  options.port = 80;
  options.config_name = "./.kanon_httpd.conf";
  options.thread_num = 0;
  options.log_way = "terminal";

  std::string home_dir = ::getenv("HOME");
  options.log_dir = home_dir;
  options.log_dir += "/.log";

  od.add_options()
    ("help,h", "help about kanon httpd")
    ("port,p", po::value<uint16_t>(&options.port), "Tcp port number of httpd(default: 80)")
    ("threads,t", po::value<uint16_t>(&options.thread_num), "Threads number of IO thread instead of main thread(default: 0)")
    ("config,c", po::value<std::string>(&options.config_name), "Configuration file path(default: ./.kanon_httpd.conf)")
    ("log,l", po::value<std::string>(&options.log_way), "How to log: terminal, file(default: terminal)")
    ("log_dir,d", po::value<std::string>(&options.log_dir), "The directory where log file store(default: ${HOME}/.log)");


  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, od), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << od << "\n";
    return false;
  }

  return true;
}

#endif // 