// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_PROFILER_METRICS_ANDROID_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_PROFILER_METRICS_ANDROID_H_

#include <optional>

#include "flutter/fml/macros.h"
#include "flutter/shell/profiling/sampling_profiler.h"

namespace flutter {

class ProfilerMetricsAndroid {
 public:
  ProfilerMetricsAndroid() = default;

  ProfileSample GenerateSample();

 private:
  std::optional<CpuUsageInfo> CpuUsage();
  std::optional<MemoryUsageInfo> MemoryUsage();

  FML_DISALLOW_COPY_AND_ASSIGN(ProfilerMetricsAndroid);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_PROFILER_METRICS_ANDROID_H_
