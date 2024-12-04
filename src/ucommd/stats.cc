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
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <sstream>

#include "logger.h"
#include "stats.h"

namespace ucommd {

Stat::Stat(int id, Counter* base, std::vector<std::string> counter_list)
    : id_(id), base_(base) {
  size_t i = 0;
  while (i < counter_list.size()) {
    counter_by_name_.emplace(counter_list[i], (Counter*)(base_ + i));
    i++;
  }
  num_counters_ = i;
}

Stat::~Stat() {
  // clear();
  counter_by_name_.clear();
  num_counters_ = 0;
  base_ = nullptr;
  id_ = -1;
}

int Stat::add(const std::string& name, size_t val) {
  auto it = counter_by_name_.find(name);
  if (it == counter_by_name_.end()) {
    LogWarn("ucommd: counter %s not found", name.c_str());
    return -1;
  }
  __atomic_add_fetch(it->second, val, __ATOMIC_SEQ_CST);
  return 0;
}

int Stat::add(const size_t i, size_t val) {
  // check index?
  __atomic_add_fetch(base_ + i, val, __ATOMIC_SEQ_CST);
  return 0;
}

int Stat::sub(const std::string& name, size_t val) {
  auto it = counter_by_name_.find(name);
  if (it == counter_by_name_.end()) {
    LogWarn("ucommd: counter %s not found", name.c_str());
    return -1;
  }
  __atomic_sub_fetch(it->second, val, __ATOMIC_SEQ_CST);
  return 0;
}

int Stat::sub(const size_t i, size_t val) {
  __atomic_sub_fetch(base_ + i, val, __ATOMIC_SEQ_CST);
  return 0;
}

int Stat::inc(const std::string& name) {
  auto it = counter_by_name_.find(name);
  if (it == counter_by_name_.end()) {
    LogWarn("ucommd: counter %s not found", name.c_str());
    return -1;
  }
  __atomic_add_fetch(it->second, 1, __ATOMIC_SEQ_CST);
  return 0;
}

int Stat::inc(const size_t i) {
  __atomic_add_fetch(base_ + i, 1, __ATOMIC_SEQ_CST);
  return 0;
}

int Stat::dec(const std::string& name) {
  auto it = counter_by_name_.find(name);
  if (it == counter_by_name_.end()) {
    LogWarn("ucommd: counter %s not found", name.c_str());
    return -1;
  }
  __atomic_sub_fetch(it->second, 1, __ATOMIC_SEQ_CST);
  return 0;
}

int Stat::dec(const size_t i) {
  __atomic_sub_fetch(base_ + i, 1, __ATOMIC_SEQ_CST);
  return 0;
}

int Stat::get(const std::string& name, size_t& val) const {
  auto it = counter_by_name_.find(name);
  if (it == counter_by_name_.end()) {
    LogWarn("ucommd: counter %s not found", name.c_str());
    return -1;
  }
  val = __atomic_load_n(it->second, __ATOMIC_SEQ_CST);
  return 0;
}

int Stat::get(const size_t i, size_t& val) const {
  val = __atomic_load_n(base_ + i, __ATOMIC_SEQ_CST);
  return 0;
}

int Stat::set(const std::string& name, size_t val) {
  auto it = counter_by_name_.find(name);
  if (it == counter_by_name_.end()) {
    LogWarn("ucommd: counter %s not found", name.c_str());
    return -1;
  }
  __atomic_store_n(it->second, val, __ATOMIC_SEQ_CST);
  return 0;
}

int Stat::set(const size_t i, size_t val) {
  __atomic_store_n(base_ + i, val, __ATOMIC_SEQ_CST);
  return 0;
}

StatsShm::StatsShm(std::string id, std::string name,
    size_t num_stats, std::vector<std::string> counter_list)
    : id_(id), name_(name),
      num_counters_(counter_list.size()),
      num_stats_(num_stats),
      counter_list_(counter_list),
      state_(CREATED) {
  std::unique_lock<std::mutex> lock(mutex_);
  for (size_t i = 0; i < num_stats_; i++) {
    stats_available_.push(i);
  }
}

StatsShm::~StatsShm() {
  state_.store(ERROR);
  std::unique_lock<std::mutex> lock(mutex_);
  std::queue<int>().swap(stats_available_);
  stats_group_.clear();
  memset(meta_, 0, meta_safe_.total_size);
  (void)munmap(meta_, meta_safe_.total_size);
  (void)shm_unlink(shm_.c_str());
}

void* StatsShm::create_shm(size_t size) {
  mode_t mode_mask = umask(0);

  std::stringstream shm_path;
  shm_path << kPrefix << id_ << '.' << name_ << '.' << getpid() << '.' << this;
  shm_ = shm_path.str();

  int fd = shm_open(shm_.c_str(), (O_CREAT | O_RDWR), (S_IRWXU | S_IRWXG | S_IRWXO));
  if (fd == -1) {
    LogWarn("ucommd: unable to create stats %s, failed to open shm %s (%s)",
        name_.c_str(), shm_.c_str(), strerror(errno));
    (void)umask(mode_mask);
    return nullptr;
  }

  struct stat statbuf;
  memset(&statbuf, 0, sizeof(statbuf));
  auto path = std::string(kRootDir) + shm_;
  if (stat(path.c_str(), &statbuf) == 0) {
    if ((size_t)statbuf.st_size < size) {
      if (ftruncate(fd, size) == -1) {
        LogWarn("ucommd: failed to resize %s (%s)", path.c_str(), strerror(errno));
        (void)shm_unlink(shm_.c_str());
        (void)umask(mode_mask);
        return nullptr;
      }
    }
  } else {
    LogWarn("ucommd: failed to get info of %s (%s)", path.c_str(), strerror(errno));
    (void)shm_unlink(shm_.c_str());
    (void)umask(mode_mask);
    return nullptr;
  }

  void* addr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED) {
    LogWarn("ucommd: unable to mmap stats %s, failed to mmap shm %s (%s)",
        name_.c_str(), shm_.c_str(), strerror(errno));
    (void)shm_unlink(shm_.c_str());
    (void)umask(mode_mask);
    return nullptr;
  }

  (void)close(fd);
  (void)umask(mode_mask);
  return addr;
}

int StatsShm::init() {
  std::unique_lock<std::mutex> lock(mutex_);

  auto s = state_.load();
  if (s == INITIALIZED) {
    return 0;
  } else if (s != CREATED) {
    LogWarn("ucommd: unable to initialize stats %s, invalid state (%d)",
        name_.c_str(), s);
    return -1;
  }

  std::stringstream timestamp;
  timestamp << time(nullptr);

  auto meta_size  = sizeof(Meta);
  auto desc_size  = sizeof(Desc) * num_counters_;
  auto stats_size = sizeof(size_t) * num_counters_ * num_stats_;
  auto total_size = meta_size + desc_size + stats_size;

  auto addr = create_shm(total_size);
  if (addr == nullptr) {
    LogWarn("ucommd: failed to create shm for stats %s", name_.c_str());
    state_.store(ERROR);
    return -1;
  }
  memset(addr, 0, total_size);
  LogDebug("ucommd: stats %s shm %s created and mmapped",
      name_.c_str(), shm_.c_str());

  strncpy(meta_safe_.shm, shm_.c_str(), NAME_LEN_MAX - 1);
  strncpy(meta_safe_.time, timestamp.str().c_str(), NAME_LEN_MAX - 1);
  strncpy(meta_safe_.name, name_.c_str(), NAME_LEN_MAX - 1);
  meta_safe_.counter_num = num_counters_;
  meta_safe_.stat_num   = num_stats_;
  meta_safe_.meta_size  = meta_size;
  meta_safe_.desc_size  = desc_size;
  meta_safe_.stats_size = stats_size;
  meta_safe_.total_size = total_size;

  meta_ = (struct Meta*)addr;
  memcpy(meta_, &meta_safe_, sizeof(struct Meta));

  desc_ = (struct Desc*)(meta_ + 1);
  for (size_t i = 0; i < counter_list_.size(); i++) {
    strncpy((desc_ + i)->name, counter_list_[i].c_str(), DESC_LEN_MAX - 1);
  }

  stats_ = (Counter*)(desc_ + num_counters_);
  if ((uint64_t)stats_ % 8 != 0) {
    LogWarn("ucommd: unable to initialize stats %s, invalid alignment",
        name_.c_str());
    state_.store(ERROR);
    return -1;
  }

  LogInfo("ucommd: stats %s initialized", name_.c_str());
  state_.store(INITIALIZED);
  return 0;
}

int StatsShm::allocStat(StatPtr& stat) {
  std::unique_lock<std::mutex> lock(mutex_);

  auto s = state_.load();
  if (s != INITIALIZED) {
    LogWarn("ucommd: unable to allocate stat from %s, invalid state (%d)",
        name_.c_str(), s);
    return -1;
  }

  if (stats_available_.empty()) {
    LogWarn("ucommd: unable to allocate stat from %s, stats exhausted",
        name_.c_str());
    return -1;
  }
  auto id = stats_available_.front();
  stats_available_.pop();
  stat = std::make_shared<Stat>(id,
      stats_ + (id * num_counters_), counter_list_);
  stat->clear();
  stats_group_.insert(stat);

  return 0;
}

int StatsShm::freeStat(StatPtr& stat) {
  std::unique_lock<std::mutex> lock(mutex_);

  auto s = state_.load();
  if (s != INITIALIZED) {
    LogWarn("ucommd: unable to free stat to %s: invalid state (%d)",
        name_.c_str(), s);
    return -1;
  }

  if (stats_group_.count(stat) == 1) {
    stat->clear();
    stats_group_.erase(stat);
    stats_available_.push(stat->getId());
  } else {
    LogWarn("ucommd: unable to free stat %d, unknown to %s",
        stat->getId(), name_.c_str());
    return -1;
  }

  return 0;
}

} // namespace ucommd
