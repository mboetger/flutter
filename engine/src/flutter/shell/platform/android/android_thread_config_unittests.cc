// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_thread_config.h"

#include <thread>

#include "flutter/fml/thread.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

TEST(AndroidThreadConfigTest, PriorityConversions) {
  EXPECT_EQ(ToFlutterThreadPriority(fml::Thread::ThreadPriority::kBackground),
            FlutterThreadPriority::kBackground);
  EXPECT_EQ(ToFlutterThreadPriority(fml::Thread::ThreadPriority::kDisplay),
            FlutterThreadPriority::kDisplay);
  EXPECT_EQ(ToFlutterThreadPriority(fml::Thread::ThreadPriority::kRaster),
            FlutterThreadPriority::kRaster);
  EXPECT_EQ(ToFlutterThreadPriority(fml::Thread::ThreadPriority::kNormal),
            FlutterThreadPriority::kNormal);

  EXPECT_EQ(ToFMLThreadPriority(FlutterThreadPriority::kBackground),
            fml::Thread::ThreadPriority::kBackground);
  EXPECT_EQ(ToFMLThreadPriority(FlutterThreadPriority::kDisplay),
            fml::Thread::ThreadPriority::kDisplay);
  EXPECT_EQ(ToFMLThreadPriority(FlutterThreadPriority::kRaster),
            fml::Thread::ThreadPriority::kRaster);
  EXPECT_EQ(ToFMLThreadPriority(FlutterThreadPriority::kNormal),
            fml::Thread::ThreadPriority::kNormal);

  // Out-of-range fallback to kNormal
  EXPECT_EQ(
      ToFlutterThreadPriority(static_cast<fml::Thread::ThreadPriority>(999)),
      FlutterThreadPriority::kNormal);
  EXPECT_EQ(ToFMLThreadPriority(static_cast<FlutterThreadPriority>(999)),
            fml::Thread::ThreadPriority::kNormal);
}

TEST(AndroidThreadConfigTest, AndroidThreadPrioritySetterDoesNotCrash) {
  // Verify calling the priority setter for all priority levels does not crash.
  std::thread t1([]() {
    AndroidThreadPrioritySetter(FlutterThreadPriority::kBackground);
  });
  t1.join();

  std::thread t2(
      []() { AndroidThreadPrioritySetter(FlutterThreadPriority::kDisplay); });
  t2.join();

  std::thread t3(
      []() { AndroidThreadPrioritySetter(FlutterThreadPriority::kRaster); });
  t3.join();

  std::thread t4(
      []() { AndroidThreadPrioritySetter(FlutterThreadPriority::kNormal); });
  t4.join();
}

TEST(AndroidThreadConfigTest, AndroidPlatformThreadConfigSetterDoesNotCrash) {
  std::thread t([]() {
    fml::Thread::ThreadConfig config("test_thread",
                                     fml::Thread::ThreadPriority::kDisplay);
    AndroidPlatformThreadConfigSetter(config);
  });
  t.join();
}

TEST(AndroidThreadConfigTest, MatchesEmbedderFunctionSignature) {
  FlutterCustomTaskRunners custom_task_runners = {};
  custom_task_runners.struct_size = sizeof(FlutterCustomTaskRunners);
  custom_task_runners.thread_priority_setter = &AndroidThreadPrioritySetter;

  EXPECT_NE(custom_task_runners.thread_priority_setter, nullptr);
  // Execute via the struct pointer.
  custom_task_runners.thread_priority_setter(FlutterThreadPriority::kDisplay);
}

}  // namespace testing
}  // namespace flutter
