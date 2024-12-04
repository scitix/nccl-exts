/**
 * Copyright (c) 2024, Scitix Tech PTE. LTD. All rights reserved.
 *
 * See LICENSE file in the root directory of this source tree for terms.
 */

#pragma once

#include <queue>
#include <unordered_set>
#include <unordered_map>

#include <mutex>
#include <atomic>
#include <memory>

namespace ucommd {

using Counter = volatile size_t;

class Stat {
 public:
  Stat(int id, Counter* base, std::vector<std::string> counter_list);
  ~Stat();

  int getId() const {
    return id_; }

  int add(const std::string& name, size_t val);
  int add(const size_t i, size_t val);

  int sub(const std::string& name, size_t val);
  int sub(const size_t i, size_t val);

  int inc(const std::string& name);
  int inc(const size_t i);

  int dec(const std::string& name);
  int dec(const size_t i);

  int get(const std::string& name, size_t& val) const;
  int get(const size_t i, size_t& val) const;

  int set(const std::string& name, size_t val);
  int set(const size_t i, size_t val);

  void clear() const {
    for (size_t i = 0; i < num_counters_; i++) {
      __atomic_store_n((Counter*)(base_ + i), 0, __ATOMIC_SEQ_CST);
    }
  }

 private:
  int id_ = -1;
  Counter* base_ = nullptr;
  size_t num_counters_ = 0;
  std::unordered_map<std::string, Counter*> counter_by_name_;
};
using StatPtr = std::shared_ptr<Stat>;

#define PATH_LEN_MAX   256
#define NAME_LEN_MAX   128
#define DESC_LEN_MAX   128

class StatsShm {
 public:
  struct Meta {
    char shm[PATH_LEN_MAX];
    char time[NAME_LEN_MAX];
    char name[NAME_LEN_MAX];
    int counter_num;
    int stat_num;
    int meta_size;
    int desc_size;
    int stats_size;
    int total_size;
  } __attribute__((aligned(8)));

  struct Desc {
    char name[DESC_LEN_MAX];
  } __attribute__((aligned(8)));

  static constexpr const char* kRootDir = "/dev/shm/";
  static constexpr const char* kPrefix = ".ucommd_stats.";
  static constexpr const char* kLockPrefix = ".ucommd_lock.";

 public:
  StatsShm(std::string id, std::string name, size_t num_stats,
      std::vector<std::string> counter_list);
  ~StatsShm();

  int init();

  int allocStat(StatPtr& stat);
  int freeStat(StatPtr& stat);

 protected:
  enum State {
    CREATED = 0,
    INITIALIZED,
    ERROR,
  };

 protected:
  void* create_shm(size_t size);

 private:
  std::string id_;
  std::string name_;
  size_t num_counters_;
  size_t num_stats_;
  std::vector<std::string> counter_list_;

  std::string shm_;
  Meta  meta_safe_;
  Meta* meta_;
  Desc* desc_;
  Counter* stats_;

  std::unordered_set<StatPtr> stats_group_;
  std::queue<int> stats_available_;

  std::mutex mutex_;
  std::atomic<State> state_;
};
using StatsShmPtr = std::shared_ptr<StatsShm>;

} // namespace ucommd
