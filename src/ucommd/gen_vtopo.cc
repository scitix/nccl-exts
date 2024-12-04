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
#include <dirent.h>
#include <cpuid.h>
#include <sys/stat.h>
#include <string.h>

#include <set>
#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <algorithm>

#include "ucommd.h"

namespace ucommd {

class GenVTopo {
 public:
  GenVTopo() {
    const char* disable = getenv("SICL_UCOMMD_VTOPO_DISABLE");
    if (disable && disable[0] > '0') return;
    const char* topo_file_env = getenv("NCCL_TOPO_FILE");
    if (topo_file_env && topo_file_env[0]) return;

    auto product_name = std::ifstream("/sys/devices/virtual/dmi/id/product_name");
    auto board_name = std::ifstream("/sys/devices/virtual/dmi/id/board_name");
    if (product_name.is_open() && board_name.is_open()) {
      char pname[32] = {0};
      char bname[32] = {0};
      product_name.getline(pname, 32);
      board_name.getline(bname, 32);
      if (!product_name.good() || !board_name.good() ||
          (std::string(pname).rfind("NF5688-M7", 0) && std::string(bname).rfind("NF5688-M7", 0))) {
        return;
      }
    } else {
      return;
    }

    union {
      struct {
        unsigned stepping_id:4;
        unsigned model_id:4;
        unsigned family_id:4;
        unsigned processor_type:2;
        unsigned resv0:2;
        unsigned ext_model_id:4;
        unsigned ext_family_id:8;
        unsigned resv1:4;
      };
      uint32_t val;
    } cpuid;
    unsigned unused;
    __cpuid(1, cpuid.val, unused, unused, unused);
    int model_id = cpuid.model_id + (cpuid.ext_model_id << 4);
    int family_id = cpuid.family_id + (cpuid.ext_family_id << 4);
    if (family_id != 6 || model_id != 143) return;

    DIR* dir = opendir("/sys/bus/pci/drivers/nvidia");
    if (dir) {
      struct dirent *entry;
      while ((entry = readdir(dir))) {
        if (entry->d_name[0] != '0') continue;
        const auto nvdev = std::string(entry->d_name);
        auto dev_class = std::ifstream(
            std::string("/sys/bus/pci/drivers/nvidia/") + nvdev + "/class");
        if (dev_class.is_open()) {
          char dclass[16] = {0};
          dev_class.getline(dclass, 16);
          if (dev_class.good() &&
             (std::string("0x030200").compare(dclass) == 0 ||
              std::string("0x030000").compare(dclass) == 0)) {
            nvdevs_.push_back(nvdev);
          }
          dev_class.close();
        }
      }
      closedir(dir);
    }
    if (nvdevs_.empty() || nvdevs_.size() != 8) return;

    /*DIR**/ dir = opendir("/sys/class/infiniband");
    if (dir) {
      struct dirent *entry;
      while ((entry = readdir(dir))) {
        if ((strcmp(entry->d_name, ".") == 0) ||
            (strcmp(entry->d_name, "..") == 0)) {
          continue;
        }
        const auto ibdev = std::string(entry->d_name);
        if ([&ibdev] {
            bool is_ib = false;
            auto node_type = std::ifstream(
                std::string("/sys/class/infiniband/") + ibdev + "/node_type");
            if (node_type.is_open()) {
              char ntype = node_type.get();
              if (node_type.good()) is_ib = '1' <= ntype && ntype <= '3';
              node_type.close();
            }
            return is_ib;
          }() &&
          [&ibdev] {
            bool is_cx6 = false;
            auto hca_type = std::ifstream(
                std::string("/sys/class/infiniband/") + ibdev + "/hca_type");
            if (hca_type.is_open()) {
              char htype[8] = {0};
              hca_type.getline(htype, 8);
              if (hca_type.good()) {
                is_cx6 = std::string("MT4123").compare(htype) == 0 ||
                         std::string("MT4125").compare(htype) == 0 ||
                         std::string("MT4129").compare(htype) == 0 ||
                         std::string("MT4131").compare(htype) == 0 ||
                         std::string("MT4124").compare(htype) == 0;
              }
              hca_type.close();
            }
            return is_cx6;
          }()) {
          ibdevs_.push_back(ibdev);
        }
      }
      closedir(dir);
    }
    if (ibdevs_.empty() || ibdevs_.size() != 4) return;

    std::sort(nvdevs_.begin(), nvdevs_.end());
    for (auto& nvdev : nvdevs_) {
      const auto nvdev_path = std::string("/sys/bus/pci/drivers/nvidia/") + nvdev;
      auto real_path = realpath(nvdev_path.c_str(), nullptr);
      if (!real_path) {
        continue;
      }
      auto rpath = std::string(real_path);
      const std::string sys_dev_0 = "/sys/devices/pci0000:";
      const std::string sys_dev_1 = "/sys/devices/pci0001:";
      if (rpath.compare(0, sys_dev_0.size(), sys_dev_0) &&
          rpath.compare(0, sys_dev_1.size(), sys_dev_1)) {
        continue;
      }
      auto up_port = rpath.substr(sizeof("/sys/devices/pci0000:00/0000:00:00.0"), strlen("0000:00:00.0"));
      upp_devs_[up_port].push_back(nvdev);
    }
    if (nvdevs_.size() != upp_devs_.size()) {
      upp_devs_.clear();
      return;
    }

    std::sort(ibdevs_.begin(), ibdevs_.end());
    for (auto& ibdev : ibdevs_) {
      const auto ibdev_path = std::string("/sys/class/infiniband/") + ibdev + "/device";
      auto real_path = realpath(ibdev_path.c_str(), nullptr);
      if (!real_path) {
        continue;
      }
      auto rpath = std::string(real_path);
      const std::string sys_dev_0 = "/sys/devices/pci0000:";
      const std::string sys_dev_1 = "/sys/devices/pci0001:";
      if (rpath.compare(0, sys_dev_0.size(), sys_dev_0) &&
          rpath.compare(0, sys_dev_1.size(), sys_dev_1)) {
        continue;
      }
      auto up_port = rpath.substr(sizeof("/sys/devices/pci0000:00/0000:00:00.0"), strlen("0000:00:00.0"));
      upp_devs_[up_port].push_back(
          rpath.substr(rpath.length() - strlen("0000:00:00.0"), strlen("0000:00:00.0")));
    }
    if (nvdevs_.size() != upp_devs_.size()) {
      upp_devs_.clear();
      return;
    }

    for (const auto& upd : upp_devs_) {
      auto device = std::ifstream(std::string("/sys/bus/pci/devices/") + upd.first + "/device");
      if (device.is_open()) {
        char dev[32] = {0};
        device.getline(dev, 32);
        if (!device.good() || strcmp(dev, "0xc030")) {
          upp_devs_.clear();
          return;
        }
      } else {
        upp_devs_.clear();
        return;
      }
    }
    for (auto it = upp_devs_.begin(); it != upp_devs_.end(); std::advance(it, 2)) {
      if (!(it->second.size() == 2 || std::next(it)->second.size() == 2)) {
        upp_devs_.clear();
        return;
      }
    }
  }

  ~GenVTopo() {
    if (!topo_file_.empty()) {
      (void)remove(topo_file_.c_str());
    }
    topo_file_.clear();
    upp_devs_.clear();
    ibdevs_.clear();
    nvdevs_.clear();
  }

  void gen() {
    if (upp_devs_.empty()) return;

    std::map<int, std::map<std::string, std::set<std::string>>> topo;
    for (auto it = upp_devs_.begin(); it != upp_devs_.end(); std::advance(it, 2)) {
      auto get_numa_node = [](const std::string bdf) {
        auto numa_node = std::ifstream(std::string("/sys/bus/pci/devices/") + bdf + "/numa_node");
        if (numa_node.is_open()) {
          char nn[8] = {0};
          numa_node.getline(nn, 8);
          if (numa_node.good()) {
            return atoi(nn);
          }
        }
        return 0;
      };
      auto iit = std::next(it);
      if (it->second.size() == 2) {
        auto numa_node = get_numa_node(it->first);
        topo[numa_node][it->first].insert(it->second.at(0));
        topo[numa_node][it->first].insert(it->second.at(1));
        topo[numa_node][it->first].insert(iit->second.at(0));
      } else {
        auto numa_node = get_numa_node(iit->first);
        topo[numa_node][iit->first].insert(it->second.at(0));
        topo[numa_node][iit->first].insert(iit->second.at(0));
        topo[numa_node][iit->first].insert(iit->second.at(1));
      }
    }

    int status = mkdir("/tmp/.ucommd", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    if (status && [] {
        if (errno == EEXIST) {
          struct stat info;
          return stat("/tmp/.ucommd", &info) || (info.st_mode & S_IFDIR) == 0;
        }
        return true;
      }()) {
      return;
    }

    if (topo_file_.empty()) {
      topo_file_ = std::string("/tmp/.ucommd/.vtopo_") + std::to_string(getpid()) + ".xml";
    }

    FILE* fp = fopen(topo_file_.c_str(), "w");
    if (fp) {
      fprintf(fp, "<system version=\"1\">\n");
      for (const auto& node_topo : topo) {
        auto cpumap = std::ifstream(std::string("/sys/devices/system/node/node") + std::to_string(node_topo.first) + "/cpumap");
        char affinity[128] = {0};
        if (cpumap.is_open()) cpumap.getline(affinity, 128);
        else affinity[0] = 'f';
        fprintf(fp, "  <cpu numaid=\"%d\" affinity=\"%s\" arch=\"x86_64\" vendor=\"GenuineIntel\" familyid=\"6\" modelid=\"143\">\n",
            node_topo.first, affinity);

        auto get_info = [](const std::string bdf, const std::string attr) {
          auto info = std::ifstream(std::string("/sys/bus/pci/devices/") + bdf + "/" + attr);
          if (info.is_open()) {
            char out[16] = {0};
            info.getline(out, 16);
            if (info.good()) {
              return std::string(out);
            }
          }
          return std::string("unknown");
        };
        for (const auto& pcie_sw_topo : node_topo.second) {
          const auto& busid = pcie_sw_topo.first;
          fprintf(fp,  "    <pci busid=\"%s\" class=\"%s\" vendor=\"%s\" device=\"%s\" subsystem_vendor=\"%s\" subsystem_device=\"%s\" link_speed=\"%s\" link_width=\"%s\">\n",
              busid.c_str(),
              get_info(busid, "class").c_str(),
              get_info(busid, "vendor").c_str(),
              get_info(busid, "device").c_str(),
              get_info(busid, "subsystem_vendor").c_str(),
              get_info(busid, "subsystem_device").c_str(),
              get_info(busid, "current_link_speed").c_str(),
              get_info(busid, "current_link_width").c_str());
          for (const auto& device : pcie_sw_topo.second) {
            fprintf(fp, "      <pci busid=\"%s\" class=\"%s\" vendor=\"%s\" device=\"%s\" subsystem_vendor=\"%s\" subsystem_device=\"%s\" link_speed=\"%s\" link_width=\"%s\"/>\n",
                device.c_str(),
                get_info(device, "class").c_str(),
                get_info(device, "vendor").c_str(),
                get_info(device, "device").c_str(),
                get_info(device, "subsystem_vendor").c_str(),
                get_info(device, "subsystem_device").c_str(),
                get_info(device, "current_link_speed").c_str(),
                get_info(device, "current_link_width").c_str());
          }
          fprintf(fp,  "    </pci>\n");
        }
        fprintf(fp, "  </cpu>\n");
      }
      fprintf(fp, "</system>\n");
      fflush(fp);
      fclose(fp);
      setenv("NCCL_TOPO_FILE", topo_file_.c_str(), 0);
    }

    return;
  }

 private:
  std::map<std::string, std::vector<std::string>> upp_devs_;
  std::vector<std::string> nvdevs_;
  std::vector<std::string> ibdevs_;

  std::string topo_file_;
};

void tryGenVTopo() {
  static GenVTopo vtopo_;
  static pthread_once_t once_ = PTHREAD_ONCE_INIT;
  pthread_once(&once_, [](){vtopo_.gen();});
  return;
}

} // namespace ucommd
