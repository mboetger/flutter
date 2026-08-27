// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <thread>

#include "flutter/fml/thread.h"
#include "flutter/shell/platform/android/android_thread_config.h"
#include "flutter/shell/platform/embedder/embedder.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

TEST(AndroidThreadConfig, NiceValueMapping) {
  EXPECT_EQ(AndroidGetNiceValue(FlutterThreadPriority::kBackground), 10);
  EXPECT_EQ(AndroidGetNiceValue(FlutterThreadPriority::kDisplay), -1);
  EXPECT_EQ(AndroidGetNiceValue(FlutterThreadPriority::kRaster), -5);
  EXPECT_EQ(AndroidGetNiceValue(FlutterThreadPriority::kNormal), 0);
}

TEST(AndroidThreadConfig, CpuAffinityMapping) {
  EXPECT_EQ(AndroidGetCpuAffinity(FlutterThreadPriority::kBackground),
            fml::CpuAffinity::kEfficiency);
  EXPECT_EQ(AndroidGetCpuAffinity(FlutterThreadPriority::kDisplay),
            fml::CpuAffinity::kNotEfficiency);
  EXPECT_EQ(AndroidGetCpuAffinity(FlutterThreadPriority::kRaster),
            fml::CpuAffinity::kNotEfficiency);
  EXPECT_EQ(AndroidGetCpuAffinity(FlutterThreadPriority::kNormal),
            fml::CpuAffinity::kNotPerformance);
}

TEST(AndroidThreadConfig, FmlToFlutterThreadPriorityConversion) {
  EXPECT_EQ(ToFlutterThreadPriority(fml::Thread::ThreadPriority::kBackground),
            FlutterThreadPriority::kBackground);
  EXPECT_EQ(ToFlutterThreadPriority(fml::Thread::ThreadPriority::kDisplay),
            FlutterThreadPriority::kDisplay);
  EXPECT_EQ(ToFlutterThreadPriority(fml::Thread::ThreadPriority::kRaster),
            FlutterThreadPriority::kRaster);
  EXPECT_EQ(ToFlutterThreadPriority(fml::Thread::ThreadPriority::kNormal),
            FlutterThreadPriority::kNormal);
}

TEST(AndroidThreadConfig, CanSetThreadPriorityOnDedicatedThread) {
  const std::vector<FlutterThreadPriority> priorities = {
      FlutterThreadPriority::kBackground,
      FlutterThreadPriority::kDisplay,
      FlutterThreadPriority::kRaster,
      FlutterThreadPriority::kNormal,
  };

  for (auto priority : priorities) {
    std::thread worker([priority]() {
      // Must execute without crashing or throwing assertions.
      AndroidSetThreadPriority(priority);
    });
    worker.join();
  }
}

TEST(AndroidThreadConfig, FallbackHandling) {
  auto out_of_range_flutter_priority = static_cast<FlutterThreadPriority>(9999);
  EXPECT_EQ(AndroidGetNiceValue(out_of_range_flutter_priority), 0);
  EXPECT_EQ(AndroidGetCpuAffinity(out_of_range_flutter_priority),
            fml::CpuAffinity::kNotPerformance);

  auto out_of_range_fml_priority =
      static_cast<fml::Thread::ThreadPriority>(9999);
  EXPECT_EQ(ToFlutterThreadPriority(out_of_range_fml_priority),
            FlutterThreadPriority::kNormal);
}

TEST(AndroidThreadConfig, FmlThreadConfigSetterIntegration) {
  fml::Thread::ThreadConfig config("test_android_thread",
                                   fml::Thread::ThreadPriority::kDisplay);

  std::thread worker(
      [&config]() { AndroidPlatformThreadConfigSetter(config); });
  worker.join();
}

TEST(AndroidThreadConfig, FlutterCustomTaskRunnersCompatibility) {
  FlutterCustomTaskRunners task_runners = {};
  task_runners.struct_size = sizeof(FlutterCustomTaskRunners);
  task_runners.thread_priority_setter = &AndroidSetThreadPriority;

  EXPECT_NE(task_runners.thread_priority_setter, nullptr);
  // Verify calling via function pointer
  task_runners.thread_priority_setter(FlutterThreadPriority::kNormal);
}

}  // namespace testing
}  // namespace flutter
