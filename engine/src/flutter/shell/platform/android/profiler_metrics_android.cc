// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/profiler_metrics_android.h"

#include <fstream>
#include <sstream>
#include <string>

#include "flutter/fml/logging.h"

namespace flutter {

ProfileSample ProfilerMetricsAndroid::GenerateSample() {
  ProfileSample sample;
  sample.cpu_usage = std::nullopt;
  sample.memory_usage = MemoryUsage();
  sample.gpu_usage = std::nullopt;
  return sample;
}

std::optional<CpuUsageInfo> ProfilerMetricsAndroid::CpuUsage() {
  return std::nullopt;
}

std::optional<MemoryUsageInfo> ProfilerMetricsAndroid::MemoryUsage() {
  std::ifstream stream("/proc/self/status");
  if (!stream) {
    return std::nullopt;
  }

  std::string line;
  int64_t rss_anon_kb = -1;
  int64_t rss_file_kb = -1;
  int64_t rss_shmem_kb = -1;
  int64_t vm_rss_kb = -1;

  while (std::getline(stream, line)) {
    std::string key;
    int64_t value;
    std::string unit;
    std::istringstream iss(line);
    if (iss >> key >> value >> unit) {
      if (key == "RssAnon:") {
        rss_anon_kb = value;
      } else if (key == "RssFile:") {
        rss_file_kb = value;
      } else if (key == "RssShmem:") {
        rss_shmem_kb = value;
      } else if (key == "VmRSS:") {
        vm_rss_kb = value;
      }
    }
  }

  double dirty_memory_mb = 0.0;
  double owned_shared_memory_mb = 0.0;

  if (rss_anon_kb != -1 && rss_file_kb != -1 && rss_shmem_kb != -1) {
    dirty_memory_mb = static_cast<double>(rss_anon_kb) / 1024.0;
    owned_shared_memory_mb =
        static_cast<double>(rss_file_kb + rss_shmem_kb) / 1024.0;
  } else if (vm_rss_kb != -1) {
    dirty_memory_mb = static_cast<double>(vm_rss_kb) / 1024.0;
    owned_shared_memory_mb = 0.0;
  } else {
    return std::nullopt;
  }

  MemoryUsageInfo info;
  info.dirty_memory_usage = dirty_memory_mb;
  info.owned_shared_memory_usage = owned_shared_memory_mb;
  return info;
}

}  // namespace flutter
