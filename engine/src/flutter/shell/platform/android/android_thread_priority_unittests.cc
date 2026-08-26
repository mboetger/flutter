// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_thread_priority.h"

#include <sys/resource.h>
#include <unistd.h>
#include <thread>

#include "flutter/shell/platform/embedder/embedder_thread_host.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

TEST(AndroidThreadPriorityTest, ToFlutterThreadPriorityMapping) {
  EXPECT_EQ(ToFlutterThreadPriority(fml::Thread::ThreadPriority::kBackground),
            FlutterThreadPriority::kBackground);
  EXPECT_EQ(ToFlutterThreadPriority(fml::Thread::ThreadPriority::kDisplay),
            FlutterThreadPriority::kDisplay);
  EXPECT_EQ(ToFlutterThreadPriority(fml::Thread::ThreadPriority::kRaster),
            FlutterThreadPriority::kRaster);
  EXPECT_EQ(ToFlutterThreadPriority(fml::Thread::ThreadPriority::kNormal),
            FlutterThreadPriority::kNormal);
}

TEST(AndroidThreadPriorityTest, AndroidPlatformThreadPrioritySetterExecution) {
  // Test executing priority setters on background threads to avoid perturbing
  // test runner.
  std::thread bg_thread([]() {
    AndroidPlatformThreadPrioritySetter(FlutterThreadPriority::kBackground);
    AndroidPlatformThreadPrioritySetter(FlutterThreadPriority::kDisplay);
    AndroidPlatformThreadPrioritySetter(FlutterThreadPriority::kRaster);
    AndroidPlatformThreadPrioritySetter(FlutterThreadPriority::kNormal);
  });
  bg_thread.join();
}

TEST(AndroidThreadPriorityTest, AndroidPlatformThreadConfigSetterExecution) {
  std::thread test_thread([]() {
    fml::Thread::ThreadConfig config("test_bg_thread",
                                     fml::Thread::ThreadPriority::kBackground);
    AndroidPlatformThreadConfigSetter(config);

    char thread_name[16] = {};
    pthread_getname_np(pthread_self(), thread_name, sizeof(thread_name));
    EXPECT_STREQ(thread_name, "test_bg_thread");
  });
  test_thread.join();
}

TEST(AndroidThreadPriorityTest, AndroidConfigureWorkerThreadPriorityExecution) {
  std::thread worker_thread([]() { AndroidConfigureWorkerThreadPriority(); });
  worker_thread.join();
}

TEST(AndroidThreadPriorityTest, CreateAndroidCustomTaskRunnersDefaults) {
  FlutterCustomTaskRunners runners = CreateAndroidCustomTaskRunners();
  EXPECT_EQ(runners.struct_size, sizeof(FlutterCustomTaskRunners));
  EXPECT_EQ(runners.platform_task_runner, nullptr);
  EXPECT_EQ(runners.render_task_runner, nullptr);
  EXPECT_EQ(runners.ui_task_runner, nullptr);
  EXPECT_EQ(runners.thread_priority_setter,
            AndroidPlatformThreadPrioritySetter);
}

TEST(AndroidThreadPriorityTest,
     CreateAndroidCustomTaskRunnersWithDescriptions) {
  FlutterTaskRunnerDescription platform_desc = {};
  platform_desc.struct_size = sizeof(FlutterTaskRunnerDescription);
  platform_desc.identifier = 1;

  FlutterTaskRunnerDescription render_desc = {};
  render_desc.struct_size = sizeof(FlutterTaskRunnerDescription);
  render_desc.identifier = 2;

  FlutterTaskRunnerDescription ui_desc = {};
  ui_desc.struct_size = sizeof(FlutterTaskRunnerDescription);
  ui_desc.identifier = 3;

  FlutterCustomTaskRunners runners =
      CreateAndroidCustomTaskRunners(&platform_desc, &render_desc, &ui_desc);
  EXPECT_EQ(runners.struct_size, sizeof(FlutterCustomTaskRunners));
  EXPECT_EQ(runners.platform_task_runner, &platform_desc);
  EXPECT_EQ(runners.render_task_runner, &render_desc);
  EXPECT_EQ(runners.ui_task_runner, &ui_desc);
  EXPECT_EQ(runners.thread_priority_setter,
            AndroidPlatformThreadPrioritySetter);
}

TEST(AndroidThreadPriorityTest, EmbedderThreadHostIntegration) {
  bool priority_setter_called = false;
  auto custom_priority_setter = +[](FlutterThreadPriority priority) {
    AndroidPlatformThreadPrioritySetter(priority);
  };

  FlutterCustomTaskRunners custom_runners = {};
  custom_runners.struct_size = sizeof(FlutterCustomTaskRunners);
  custom_runners.thread_priority_setter = custom_priority_setter;

  auto thread_config_callback =
      [&priority_setter_called](const fml::Thread::ThreadConfig& config) {
        fml::Thread::SetCurrentThreadName(config);
        priority_setter_called = true;
        AndroidPlatformThreadConfigSetter(config);
      };

  auto host = EmbedderThreadHost::CreateEmbedderOrEngineManagedThreadHost(
      &custom_runners, thread_config_callback);
  ASSERT_NE(host, nullptr);
  ASSERT_TRUE(host->IsValid());
  EXPECT_TRUE(priority_setter_called);
}

}  // namespace testing
}  // namespace flutter
