/******************************************************************************
 * Copyright 2018 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include "cyber/common/global_data.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <functional>

#include "cyber/common/environment.h"
#include "cyber/common/file.h"
#include "cyber/common/util.h"

namespace apollo {
namespace cyber {
namespace common {

AtomicHashMap<uint64_t, std::string, 512> GlobalData::node_id_map_;
AtomicHashMap<uint64_t, std::string, 256> GlobalData::channel_id_map_;
AtomicHashMap<uint64_t, std::string, 256> GlobalData::service_id_map_;
AtomicHashMap<uint64_t, std::string, 256> GlobalData::task_id_map_;

namespace {
const char* empty_str_ = "";
}  // namespace

GlobalData::GlobalData() {
  InitHostInfo();
  InitConfig();
  process_id_ = getpid();
  process_name_ = "default_" + std::to_string(process_id_);
  is_reality_mode_ = (config_.has_run_mode_conf() &&
                      config_.run_mode_conf().run_mode() ==
                          apollo::cyber::proto::RunMode::MODE_SIMULATION)
                         ? false
                         : true;

  const char* run_mode_val = ::getenv("CYBER_RUN_MODE");
  if (run_mode_val != nullptr) {
    std::string run_mode_str(run_mode_val);
    if (run_mode_str == "simulation") {
      is_reality_mode_ = false;
    }
  }
}

GlobalData::~GlobalData() {}

int GlobalData::ProcessId() const { return process_id_; }

void GlobalData::SetProcessName(const std::string& process_name) {
  process_name_ = process_name;
}
const std::string& GlobalData::ProcessName() const { return process_name_; }

const std::string& GlobalData::HostIp() const { return host_ip_; }

const std::string& GlobalData::HostName() const { return host_name_; }

void GlobalData::EnableSimulationMode() { is_reality_mode_ = false; }

void GlobalData::DisableSimulationMode() { is_reality_mode_ = true; }

bool GlobalData::IsRealityMode() const { return is_reality_mode_; }

void GlobalData::InitHostInfo() {
  char host_name[1024];
  gethostname(host_name, 1024);
  host_name_ = host_name;

  host_ip_ = "127.0.0.1";

  // if we have exported a non-loopback CYBER_IP, we will use it firstly,
  // otherwise, we try to find first non-loopback ipv4 addr.
  const char* ip_env = getenv("CYBER_IP");
  if (ip_env != nullptr) {
    // maybe we need to verify ip_env
    std::string ip_env_str(ip_env);
    std::string starts = ip_env_str.substr(0, 3);
    if (starts != "127") {
      host_ip_ = ip_env_str;
      AINFO << "host ip: " << host_ip_;
      return;
    }
  }

  ifaddrs* ifaddr = nullptr;
  if (getifaddrs(&ifaddr) != 0) {
    AERROR << "getifaddrs failed, we will use 127.0.0.1 as host ip.";
    return;
  }
  for (ifaddrs* ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == nullptr) {
      continue;
    }
    int family = ifa->ifa_addr->sa_family;
    if (family != AF_INET) {
      continue;
    }
    char addr[NI_MAXHOST] = {0};
    if (getnameinfo(ifa->ifa_addr, sizeof(sockaddr_in), addr, NI_MAXHOST, NULL,
                    0, NI_NUMERICHOST) != 0) {
      continue;
    }
    std::string tmp_ip(addr);
    std::string starts = tmp_ip.substr(0, 3);
    if (starts != "127") {
      host_ip_ = tmp_ip;
      break;
    }
  }
  freeifaddrs(ifaddr);
  AINFO << "host ip: " << host_ip_;
}

void GlobalData::InitConfig() {
  auto config_path = GetAbsolutePath(WorkRoot(), "conf/cyber.pb.conf");
  if (!GetProtoFromFile(config_path, &config_)) {
    AERROR << "read cyber global conf failed!";
  }
}

const CyberConfig& GlobalData::Config() const { return config_; }

// TODO(fengkaiwen01) need a better error handling mechanism when collision
// happened
uint64_t GlobalData::RegisterNode(const std::string& node_name) {
  auto id = Hash(node_name);
  if (node_id_map_.Has(id)) {
    std::string* name = nullptr;
    node_id_map_.Get(id, &name);
    if (node_name != *name) {
      AERROR << "Node name collision: " << node_name << " <=> " << *name;
    }
  }
  node_id_map_.Set(id, node_name);
  return id;
}

std::string GlobalData::GetNodeById(uint64_t id) {
  std::string* node_name = nullptr;
  if (node_id_map_.Get(id, &node_name)) {
    return *node_name;
  }
  return empty_str_;
}

// TODO(fengkaiwen01) need a better error handling mechanism when collision
// happened
uint64_t GlobalData::RegisterChannel(const std::string& channel) {
  auto id = Hash(channel);
  if (channel_id_map_.Has(id)) {
    std::string* name = nullptr;
    channel_id_map_.Get(id, &name);
    if (channel != *name) {
      AERROR << "Channel name collision: " << channel << " <=> " << *name;
    }
  }
  channel_id_map_.Set(id, channel);
  return id;
}

std::string GlobalData::GetChannelById(uint64_t id) {
  std::string* channel = nullptr;
  if (channel_id_map_.Get(id, &channel)) {
    return *channel;
  }
  return empty_str_;
}

// TODO(fengkaiwen01) need a better error handling mechanism when collision
// happened
uint64_t GlobalData::RegisterService(const std::string& service) {
  auto id = Hash(service);
  if (service_id_map_.Has(id)) {
    std::string* name = nullptr;
    service_id_map_.Get(id, &name);
    if (service != *name) {
      AERROR << "Service name collision: " << service << " <=> " << *name;
    }
  }
  service_id_map_.Set(id, service);
  return id;
}

std::string GlobalData::GetServiceById(uint64_t id) {
  std::string* service = nullptr;
  if (service_id_map_.Get(id, &service)) {
    return *service;
  }
  return empty_str_;
}

// TODO(fengkaiwen01) need a better error handling mechanism when collision
// happened
uint64_t GlobalData::RegisterTaskName(const std::string& task_name) {
  auto id = Hash(task_name);
  if (task_id_map_.Has(id)) {
    std::string* name = nullptr;
    task_id_map_.Get(id, &name);
    if (task_name != *name) {
      AERROR << "Task name collision: " << task_name << " <=> " << *name;
    }
  }
  task_id_map_.Set(id, task_name);
  return id;
}

std::string GlobalData::GetTaskNameById(uint64_t id) {
  std::string* task_name = nullptr;
  if (task_id_map_.Get(id, &task_name)) {
    return *task_name;
  }
  return empty_str_;
}

}  // namespace common
}  // namespace cyber
}  // namespace apollo