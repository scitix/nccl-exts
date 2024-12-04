/**
 * Copyright (c) 2024, Scitix Tech PTE. LTD. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <unistd.h>
#include <sys/file.h>
#include <fcntl.h>
#include <string.h>

#include <vector>
#include <string>
#include <fstream>
#include <iostream>

#include "logger.h"
#include "ucommd.h"

namespace ucommd {

class UnetPerfMonitor {
 protected:
  enum State {
    CREATED = 0,
    INITIALIZED,
    ERROR,
  };

 public:
  static constexpr const char* kUnetPid = "pid";
  static constexpr const char* kUnetRank = "rank";
  static constexpr const char* kUnetLocalRank = "local_rank";
  static constexpr const char* kUnetWorldSize = "world_size";
  static constexpr const char* kUnetLocalSize = "local_size";

  static constexpr const char* kUnetIbStats = "unet_ib_stats";
  static constexpr const size_t kUnetIbStatsNum = 1;
  static constexpr const char* kUnetIbCqCount = "cq_count";
  static constexpr const char* kUnetIbQpCount = "qp_count";
  static constexpr const char* kUnetIbMrCount = "mr_count";
  static constexpr const char* kUnetIbCplCount = "cpl_count";
  static constexpr const char* kUnetIbCplErrCount = "cpl_err_count";
  static constexpr const char* kUnetIbFifoPostCount = "fifo_post_count";
  static constexpr const char* kUnetIbFifoRecvCount = "fifo_recv_count";
  static constexpr const char* kUnetIbTxBytes = "tx_bytes";

  static constexpr const char* kUnetBwStats = "unet_bw_stats";
  static constexpr const size_t kUnetBwStatsNum = 1;

 private:
  int pid_ = -1;
  std::string proc_name_;

  int lock_fd_ = -1;
  std::string lock_file_;

  int rank_ = -1;
  int local_rank_ = -1;
  int world_size_ = -1;
  int local_size_ = -1;

  std::string id_{"_"};
  StatsShmPtr shm_unet_ib_{nullptr};
  StatsShmPtr shm_unet_bw_{nullptr};
  StatPtr ib_stat_{nullptr};
  StatPtr bw_stat_{nullptr};

  std::mutex mutex_;
  std::atomic<State> state_;

 public:
  static int bw_offset_;

 public:
  UnetPerfMonitor() : state_(CREATED) {
    const char* disable = getenv("SICL_UCOMMD_STATS_DISABLE");
    if (disable && disable[0] > '0') {
      state_.store(ERROR);
      return;
    }

    pid_ = getpid();

    auto proc_comm = std::ifstream(
        std::string("/proc/") + std::to_string(pid_) + "/comm");
    if (proc_comm.is_open()) {
      char comm[128] = {0};
      proc_comm.getline(comm, sizeof(comm));
      if (proc_comm.good()) proc_name_.assign(comm);
      proc_comm.close();
    }
    if (proc_name_.empty()) {
      proc_name_.assign(program_invocation_short_name);
    }

    lock_file_ = std::string(StatsShm::kRootDir) + StatsShm::kLockPrefix +
        proc_name_ + "." + std::to_string(pid_);

    auto get_from_env = [](const char* torch_env, const char* ompi_env) {
      char* env = nullptr;
      if (((env = getenv(torch_env)) && env[0]) ||
          ((env = getenv( ompi_env)) && env[0])) {
        return atoi(env);
      } else {
        return -1;
      }
    };
    rank_ = get_from_env("RANK", "OMPI_COMM_WORLD_RANK");
    local_rank_ = get_from_env("LOCAL_RANK", "OMPI_COMM_WORLD_LOCAL_RANK");
    world_size_ = get_from_env("WORLD_SIZE", "OMPI_COMM_WORLD_SIZE");
    local_size_ = get_from_env("LOCAL_WORLD_SIZE", "OMPI_COMM_WORLD_LOCAL_SIZE");
  }

  ~UnetPerfMonitor() {
    if (bw_stat_) {
      (void)shm_unet_bw_->freeStat(bw_stat_);
      bw_stat_.reset();
    }
    if (ib_stat_) {
      (void)shm_unet_ib_->freeStat(ib_stat_);
      ib_stat_.reset();
    }
    if (shm_unet_bw_) {
      shm_unet_bw_.reset();
    }
    if (shm_unet_ib_) {
      shm_unet_ib_.reset();
    }
    if (lock_fd_ >= 0) {
      close(lock_fd_); lock_fd_ = -1;
      unlink(lock_file_.c_str());
    }
    state_.store(ERROR);
  }

  int init() {
    std::unique_lock<std::mutex> lock(mutex_);

    auto s = state_.load();
    if (s == INITIALIZED) {
      return 0;
    } else if (s != CREATED) {
      LogWarn("ucommd: unable to initialize unet perf monitor, invalid state (%d)", s);
      return -1;
    }

    int fd = open(lock_file_.c_str(), (O_CREAT | O_TRUNC | O_WRONLY),
        (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
    if (fd == -1) {
      LogWarn("ucommd: unable to initialize unet perf monitor: failed to "
          "open lock file %s, err=%d", lock_file_.c_str(), errno);
      state_.store(ERROR);
      return -1;
    }
    if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
      close(fd);
      if (EWOULDBLOCK != errno) {
        LogWarn("ucommd: unable to initialize unet perf monitor, failed to "
            "lock on fd %d, err=%d", fd, errno);
        state_.store(ERROR);
        return -1;
      }
    }
    lock_fd_ = fd;

    {
      std::vector<std::string> counter_list = {
          kUnetPid, kUnetRank,
          kUnetIbCqCount, kUnetIbQpCount, kUnetIbMrCount,
          kUnetIbCplCount, kUnetIbCplErrCount,
          kUnetIbFifoPostCount, kUnetIbFifoRecvCount,
          kUnetIbTxBytes,
      };
      shm_unet_ib_ = std::make_shared<StatsShm>(id_,
          kUnetIbStats, kUnetIbStatsNum, counter_list);
      if (shm_unet_ib_->init()) {
        LogWarn("ucommd: unable to initialize unet perf monitor, failed to "
            "set up ib stats");
        state_.store(ERROR);
        return -1;
      }
      if (shm_unet_ib_->allocStat(ib_stat_)) {
        LogWarn("ucommd: unable to initialize unet perf monitor, failed to "
            "allocate ib stat");
        state_.store(ERROR);
        return -1;
      }
      if (ib_stat_->set(UNET_PID, pid_)) {
        LogWarn("ucommd: unable to initialize unet perf monitor, failed to "
            "set PID for ib stat");
        state_.store(ERROR);
        return -1;
      }
      if (ib_stat_->set(UNET_RANK, rank_)) {
        LogWarn("ucommd: unable to initialize unet perf monitor, failed to "
            "set RANK for ib stat");
        state_.store(ERROR);
        return -1;
      }
    }

    if (rank_ >= 0) {
      std::vector<std::string> counter_list = {
          kUnetPid,
          kUnetRank, kUnetLocalRank,
          kUnetWorldSize, kUnetLocalSize,
      };
      bw_offset_ = counter_list.size();
      for (int i = 0; i < world_size_; i++) {
        counter_list.push_back("tx_rank" + std::to_string(i));
        counter_list.push_back("cpl_rank" + std::to_string(i));
      }
      shm_unet_bw_ = std::make_shared<StatsShm>(id_,
          kUnetBwStats, kUnetBwStatsNum, counter_list);
      if (shm_unet_bw_->init()) {
        LogWarn("ucommd: unable to initialize unet perf monitor, failed to "
            "set up bw stats");
        state_.store(ERROR);
        return -1;
      }
      if (shm_unet_bw_->allocStat(bw_stat_)) {
        LogWarn("ucommd: unable to initialize unet perf monitor, failed to "
            "allocate bw stat");
        state_.store(ERROR);
        return -1;
      }
      if (bw_stat_->set(kUnetPid, pid_) ||
          bw_stat_->set(kUnetRank, rank_) || bw_stat_->set(kUnetLocalRank, local_rank_) ||
          bw_stat_->set(kUnetWorldSize, world_size_) || bw_stat_->set(kUnetLocalSize, local_size_)) {
        LogWarn("ucommd: unable to initialize unet perf monitor, failed to "
            "set ranking info for bw stat");
        state_.store(ERROR);
        return -1;
      }
    }

    state_.store(INITIALIZED);
    return 0;
  }

  StatPtr getIbStat() const {
    return ib_stat_; }

  StatPtr getBwStat() const {
    return bw_stat_; }

};
int UnetPerfMonitor::bw_offset_ = 5;

int UNET_BW_POST_BYTES_BY_RANK(int rank) {
  return UnetPerfMonitor::bw_offset_ + rank * 2;
}

int UNET_BW_CPL_BYTES_BY_RANK(int rank) {
  return UnetPerfMonitor::bw_offset_ + rank * 2 + 1;
}

static StatPtr get_stat_(const int n) {   // ugly 0 for ib, 1 for bw
  static UnetPerfMonitor unet_perf_;
  if (unet_perf_.init()) return nullptr;
  return !n ? unet_perf_.getIbStat() : unet_perf_.getBwStat();
}

StatPtr getUnetIbStat() {
  return get_stat_(0);
}

StatPtr getUnetBwStat() {
  return get_stat_(1);
}

} // namespace ucommd
