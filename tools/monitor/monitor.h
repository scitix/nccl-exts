/**
 * Copyright (c) 2024, Scitix Tech PTE. LTD. All rights reserved.
 *
 * See LICENSE file in the root directory of this source tree for terms.
 */

#pragma once

#include <time.h>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include "options.h"

namespace ucommd {

class Monitor {
 public:
  Monitor(Options opts);
  ~Monitor();

 public:
  int run();

 protected:
  struct StatsShmInfo {
    std::string shm;
    time_t time;
    void* addr;
    size_t size;
    std::vector<std::string> desc;
    volatile size_t* data;
    size_t counter_num;
    size_t stat_num;
    StatsShmInfo(const std::string& shm_name) :
        shm(shm_name), time(0),
        addr(nullptr), size(0),
        data(nullptr), counter_num{0}, stat_num{0} {}
    ~StatsShmInfo() {
      shm.clear();
      time = 0;
      addr = nullptr;
      size = 0;
      desc.clear();
      data = nullptr;
      counter_num = 0;
      stat_num = 0;
    }
  };

 private:
  void check_ucommd_pids_stats();
  int get_lock_pids();
  int check_ucommd_pids();
  int scan_stats();
  int check_stats(StatsShmInfo& info);
  void clean_stats();

 private:
  void bw_print_func();

 private:
  static void print_real_time();

 private:
  Options opts_;

  std::unordered_set<int> lock_pids_;
  std::unordered_set<int> ucommd_pids_;
  std::unordered_map<std::string, std::vector<struct StatsShmInfo>> stats_records_;
};

} // namespace ucommd
