/**
 * Copyright (c) 2024, Scitix Tech PTE. LTD. All rights reserved.
 */

#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <stdlib.h>
#include <string.h>

#include <chrono>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>

#include "stats.h"
#include "logger.h"
#include "monitor.h"

namespace ucommd {

Monitor::Monitor(Options opts) : opts_(opts) {
  (void)check_ucommd_pids_stats();
}

Monitor::~Monitor() {
  ucommd_pids_.clear();
  lock_pids_.clear();
  for (const auto& stats : stats_records_) {
    for (const auto& info : stats.second) {
      (void)munmap(info.addr, info.size);
    }
  }
  stats_records_.clear();
}

void Monitor::check_ucommd_pids_stats() {
  if (get_lock_pids() || check_ucommd_pids()) {
    LogWarn("unable to get pid(s) of running ucommd processes ...");
  }
  if (scan_stats()) {
    LogWarn("unable to get ucommd stats ...");
  }
  (void)clean_stats();
}

int Monitor::get_lock_pids() {
  lock_pids_.clear();
  auto proc_lock = std::ifstream("/proc/locks");
  if (proc_lock.is_open()) {
    char lock[1024] = {0};
    while (proc_lock.getline(lock, sizeof(lock))) {
      std::stringstream ss(lock);
      for (int i = 0; i < 4; i++) {
        std::string tmp;
        ss >> tmp;
      }
      int pid; ss >> pid;
      lock_pids_.insert(pid);
      ss.str(""); ss.clear();
      memset(lock, 0, sizeof(lock));
    }
    proc_lock.close();
    return 0;
  }
  return -1;
}

int Monitor::check_ucommd_pids() {
  ucommd_pids_.clear();

  auto& pids = opts_.pids;
  if (!pids.empty()) {
    for (auto it = pids.begin(); it != pids.end(); ) {
      if (lock_pids_.find(*it) != lock_pids_.end()) {
        ucommd_pids_.insert(*it);
        it++;
      } else {
        it = pids.erase(it);
      }
    }
  }
  if (!ucommd_pids_.empty()) {
    return 0;
  }

  DIR* dir = opendir(StatsShm::kRootDir);
  if (dir) {
    struct dirent* entry;
    while ((entry = readdir(dir))) {
      // e.g., /dev/shm/.ucommd_lock.ucommd_perf.2763
      auto lock_file_name = std::string(entry->d_name);
      if (entry->d_type == DT_REG && !lock_file_name.rfind(StatsShm::kLockPrefix, 0)) {
        std::vector<std::string> lock_file;
        std::istringstream iss(lock_file_name);
        for (std::string str; std::getline(iss, str, '.'); ) {
          lock_file.push_back(str);
        }
        auto pid = atoi(lock_file.back().c_str());
        if (lock_pids_.find(pid) != lock_pids_.end()) {
          ucommd_pids_.insert(pid);
        }
      }
    }
    closedir(dir);
  } else return -1;

  for (auto& stats : stats_records_) {
    for (size_t i = 0; i < stats.second.size(); i++) {
      std::vector<std::string> stats_file;
      std::istringstream iss(stats.second[i].shm);
      for (std::string str; std::getline(iss, str, '.'); ) {
        stats_file.push_back(str);
      }
      if (lock_pids_.find(std::stoi(stats_file[4])) == lock_pids_.end()) {
        (void)munmap(stats.second[i].addr, stats.second[i].size);
        stats.second.erase(stats.second.begin() + i);
        i--;
      }
    }
  }
  return 0;
}

int Monitor::scan_stats() {
  DIR* dir = opendir(StatsShm::kRootDir);
  if (dir) {
    struct dirent* entry;
    while ((entry = readdir(dir))) {
      // e.g., /dev/shm/.ucommd_stats.test.ucommd_stats.2763.0x5650a769c260
      //                    prefix     id      name      pid
      //                       1        2        3        4
      auto stats_file_name = std::string(entry->d_name);
      if (entry->d_type == DT_REG && !stats_file_name.rfind(StatsShm::kPrefix, 0)) {
        std::vector<std::string> stats_file;
        std::istringstream iss(stats_file_name);
        for (std::string str; std::getline(iss, str, '.'); ) {
          stats_file.push_back(str);
        }
        auto pid = atoi(stats_file[4].c_str());
        if (ucommd_pids_.find(pid) != ucommd_pids_.end()) {
          auto& stats_name = stats_file[3];
          if ([&] {
              if (stats_records_.find(stats_name) != stats_records_.end())
                for (const auto& info : stats_records_[stats_name])
                  if (info.shm == stats_file_name) return false;
              return true; }()) {
            StatsShmInfo info(stats_file_name);
            if (!check_stats(info)) {
              stats_records_[stats_name].emplace_back(info);
            }
          }
          // stats_records_[stats_file[3]].emplace_back(entry->d_name);
        }
      }
    }
    closedir(dir);
    return 0;
  }
  return -1;
}

int Monitor::check_stats(StatsShmInfo& info) {
  if (info.shm.empty()) {
    LogDebug("unexpected null shm name ...");
    return 1;
  }

  const auto file_path = std::string(StatsShm::kRootDir) + info.shm;
  struct stat st_buff;
  if (stat(file_path.c_str(), &st_buff) == -1) {
    LogDebug("unable to get info of %s, err=%d", file_path.c_str(), errno);
    return -1;
  }
  auto size = st_buff.st_size;
  if (size == -1) {
    LogDebug("invalid size=%d of %s", st_buff.st_size, file_path.c_str());
    return 1;
  }
  auto fd = open(file_path.c_str(), O_RDONLY, (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
  if (fd == -1) {
    LogDebug("failed to open shm file %s, err=%d", file_path.c_str(), errno);
    return 1;
  }
  void* addr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED) {
    LogDebug("failed to mmap shm file %s, err=%d", file_path.c_str(), errno);
    close(fd);
    return -1;
  }
  close(fd);

  auto meta = (StatsShm::Meta*)addr;
  if (info.shm.compare(meta->shm)) {
    LogWarn("unmatched shm name: %s .vs %s", info.shm.c_str(), meta->shm);
  }
  info.time = (time_t)std::stoi(meta->time);
  info.addr = addr;
  info.size = size;

  auto desc = (StatsShm::Desc*)(meta + 1);
  for (int i = 0; i < meta->counter_num; i++) {
    info.desc.push_back((desc+i)->name);
  }

  info.data = (volatile size_t*)(desc + meta->counter_num);
  info.counter_num = meta->counter_num;
  info.stat_num = meta->stat_num;

  return 0;
}

void Monitor::clean_stats() {
  for (auto it = stats_records_.begin(); it != stats_records_.end(); ) {
    for (auto iit = it->second.begin(); iit != it->second.end(); ) {
      if (access((std::string(StatsShm::kRootDir) + iit->shm).c_str(), F_OK)) {
        (void)munmap(iit->addr, iit->size);
        iit = it->second.erase(iit);
      } else {
        ++iit;
      }
    }
    if (it->second.empty()) {
      it = stats_records_.erase(it);
    } else {
      ++it;
    }
  }
}

void Monitor::bw_print_func() {
  static size_t world_size_ = 0, local_world_size_ = 0;
  static std::unordered_map<int, std::unordered_map<int, std::vector<size_t>>> unprinted_;
  static std::unordered_map<int, std::unordered_map<int, size_t>> prev_post_map_, prev_cql_map_;
  static std::unordered_map<int, int> local2globalrank_, localrank2pid_;
  static size_t now_get_ns_, prev_get_ns_, duration_, prev_print_ns_;
  static auto human_string = [](size_t bytes)->std::string {
    static std::string units[5] = {"B", "KiB", "MiB", "GiB", "TiB"};
    int index = 0;
    while (bytes > 10240 && index < 4) {
      bytes /= 1024;
      index++;
    }
    return std::to_string(bytes) + units[index];
  };

  now_get_ns_ = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
  duration_ = now_get_ns_ - prev_get_ns_;
  if (duration_ < 1000000) return;
  prev_get_ns_ = now_get_ns_;
  for (const auto& stats : stats_records_) {
    if (stats.first != "unet_bw_stats") {
      continue;
    }

    for (const auto& info : stats.second) {
      for (size_t i = 0; i < info.stat_num; i++) {
        auto base = info.data + i * info.counter_num;
        int pid = base[0];
        int rank = base[1];
        int local_rank = base[2];
        if (world_size_ != base[3] || local_world_size_ != base[4]) {
          world_size_ = base[3];
          local_world_size_ = base[4];
          unprinted_.clear();
          prev_post_map_.clear();
          prev_cql_map_.clear();
          local2globalrank_.clear();
          localrank2pid_.clear();
        }
        local2globalrank_[local_rank] = rank;
        localrank2pid_[local_rank] = pid;

        for (size_t i = 0; i < world_size_; i++) {
          if (prev_cql_map_[local_rank][i] == base[2 * i + 6]) {
            continue;
          }
          unprinted_[local_rank][i].push_back((base[2 * i + 6] - prev_cql_map_[local_rank][i]) * 1000000000 / duration_);
          prev_cql_map_[local_rank][i] = base[2 * i + 6];
        }
      }
    }
  }

  if ((now_get_ns_ - prev_print_ns_) > 1000000000) {
    prev_print_ns_ = now_get_ns_;

    std::cout << std::endl << "=========================" << std::endl << std::flush;
    print_real_time();
    for (size_t i = 0; i < local_world_size_; i++) {
      std::cout << "[" << i << "][" << local2globalrank_[i] << "][" << localrank2pid_[i] << "]"
                << "[tx_thruput_to_rank] ";
      for (size_t j = 0; j < world_size_; j++) {
        if (!unprinted_[i][j].empty()) {
          size_t total_thruput = 0, total_num = 0;
          // for (auto& e : unprinted_[i][j]) max_bw = std::max(max_bw, e);
          for (auto& e : unprinted_[i][j]) {
            total_thruput += e; total_num++;
          }
          std::cout << "[" << j << "]=" << human_string(total_thruput / total_num) << "/s ";
        }
      }
      std::cout << std::endl << std::flush;
    }
    unprinted_.clear();
  }
}

int Monitor::run() {
  struct itimerspec spec;
  memset(&spec, 0, sizeof(spec));
  if (opts_.interval_ns < 0) {
    spec.it_interval.tv_sec = opts_.bw_print ? 0 : 1;
    spec.it_interval.tv_nsec = opts_.bw_print ? 10 * 1000 * 1000 : 0;
  } else {
    spec.it_interval.tv_sec = opts_.interval_ns / 1000000000;
    spec.it_interval.tv_nsec = opts_.interval_ns % 1000000000;
  }
  spec.it_value.tv_nsec = 10 * 1000 * 1000;

  int fd = timerfd_create(CLOCK_MONOTONIC, 0);
  if (fd == -1) {
    LogError("failed to create timerfd: err=%d", errno);
    return -1;
  }
  if (timerfd_settime(fd, 0, &spec, NULL) == -1) {
    LogError("failed to set timer: err=%d", errno);
    return -1;
  }

  size_t t = 0;
  long long check_pid_interval =
      (10LL * 1000000000LL) / (spec.it_interval.tv_sec * 1000000000LL + spec.it_interval.tv_nsec);
  while (1) {
    if ([&fd] {
        size_t to;
        int ret = read(fd, &to, sizeof(size_t));
        if (ret < 0) return (errno == EAGAIN) ? 0 : ret;
        return 0; }()) {
      LogError("failed to wait for timer ...");
      close(fd);
      return -1;
    }

    if (!(++t%check_pid_interval)) (void)check_ucommd_pids_stats();

    if (opts_.bw_print) {
      bw_print_func();
      continue;
    }

    std::cout << std::endl << "=========================" << std::endl << std::flush;
    print_real_time();

    if (ucommd_pids_.empty() || stats_records_.empty()) {
      LogInfo("none ucommd pids or stats found, is ucommd application running?");
      if (t%60) (void)check_ucommd_pids_stats();
      continue;
    }

    for (const auto& stats : stats_records_) {
      const auto& stats_name = stats.first;
      if (stats_name.find("unet_bw_stats") != std::string::npos) {
        continue;
      }
      for (size_t i = 0; i < stats_name.length(); i++) {
        std::cout << "-"; (void)i;
      }
      std::cout << std::endl << stats_name << std::endl << std::flush;

      std::vector<size_t> width;
      for (const auto& desc : stats.second.at(0).desc) {
        if (desc.find("bytes") != std::string::npos) {
          width.push_back(20);
        } else if (desc.length() <= 8) {
          width.push_back(8);
        } else if (8 < desc.length() && desc.length() <= 12) {
          width.push_back(12);
        } else if (12 < desc.length() && desc.length() <= 16) {
          width.push_back(16);
        }
      }

      const auto& desc = stats.second.at(0).desc;
      for (size_t i = 0; i < desc.size(); i++) {
        std::cout << std::setw(width.at(i)) << desc[i] << "    ";
      }
      std::cout << std::endl << std::flush;

      for (const auto& info : stats.second) {
        for (size_t i = 0; i < info.stat_num; i++) {
          auto base = info.data + i * info.counter_num;
          if (*base == 0) continue;
          for (size_t j = 0; j < info.counter_num; j++) {
          //std::cout << std::setw(width.at(j)) << __atomic_load_n(base + j, __ATOMIC_SEQ_CST) << "    ";
            std::cout << std::setw(width.at(j)) << *((volatile size_t*)(base + j)) << "    ";
          }
          std::cout << std::endl << std::flush;
        }
      }
    }
  }

  close(fd);
  return 0;
}

void Monitor::print_real_time() {
  time_t real_time = time(nullptr);
  struct tm *p = localtime(&real_time);
  char timestamp[32] = {0};
  strftime(timestamp, 32, "%Y-%m-%d | %H:%M:%S %Z\n", p);
  std::cout << timestamp;
}

} // namespace ucommd
