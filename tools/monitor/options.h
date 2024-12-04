/**
 * Copyright (c) 2024, Scitix Tech PTE. LTD. All rights reserved.
 *
 * See LICENSE file in the root directory of this source tree for terms.
 */

#pragma once

#include <getopt.h>
#include <vector>
#include <sstream>

namespace ucommd {

struct Options {
 private:
  static constexpr const int kOpt = 15917;

 protected:
  static constexpr const struct option opts[] = {
    { "pid",              required_argument,    0,    'p'     },
    { "show-bw",          no_argument,          0,    kOpt    },
    { "sample-interval",  required_argument,    0,    kOpt+1  },
    { "help",             no_argument,          0,    'h'     },
    { 0,                  0,                    0,     0      },
  };
  static constexpr const char* optstr = "p:h";

 public:
  std::vector<int> pids;
  bool bw_print = false;
  long long interval_ns = -1;

 public:
  int parseArgs(int argc, char* argv[]) {
    int c, index = -1;
    while ((c = getopt_long(argc, argv, optstr, opts, &index)) != -1) {
      switch (c) {
      case 'p':
        { std::istringstream iss(optarg);
        char c; int pid;
        while (iss >> pid) {
          pids.push_back(pid);
          iss >> c;
        } }
        break;
      case kOpt:
        bw_print = true;
        break;
      case kOpt+1:
        interval_ns = atoll(optarg);
        break;
      case 'h':
      case '?':
      default:
        usage();
        return 1;
      }
    }
    return 0;
  }

 protected:
  static inline void usage() {}

 public:
  Options() {}
  ~Options() {}
};

} // namespace ucommd
