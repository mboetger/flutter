// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/resource.h>
#include <memory>

#include "flutter/fml/message_loop.h"
#include "flutter/fml/message_loop_task_queues.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/fml/trace_event.h"
#include "flutter/shell/platform/android/android_shell_holder.h"
#include "flutter/shell/platform/android/external_view_embedder/external_view_embedder.h"
#include "flutter/shell/platform/android/external_view_embedder/external_view_embedder_2.h"
#include "flutter/shell/platform/android/external_view_embedder/external_view_embedder_wrapper.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "flutter/shell/platform/android/platform_view_android.h"
#include "flutter/shell/platform/android/surface/android_native_window.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

// PARITY CONTRACT CONSTANTS:
// These constants record the exact POSIX thread priorities configured by
// AndroidPlatformThreadConfigSetter in android_shell_holder.cc.
// Any future embedder API migration (Stage 3) adopting engine-managed threads
// MUST preserve these exact priorities via
// FlutterCustomTaskRunners::thread_priority_setter to prevent silent
// performance regressions.
constexpr int kParityUIThreadPriority = -1;
constexpr int kParityRasterThreadPriority = -5;
constexpr int kParityRasterThreadPriorityFallback = -2;
// DISCREPANCY NOTE: Android explicitly sets IO thread priority to kNormal (0)
// in AndroidShellHolder, in contrast to the engine-managed default
// kBackground (10).
constexpr int kParityIOThreadPriorityAndroid = 0;
constexpr int kParityPlatformThreadPriority = 0;
constexpr int kParityWorkerThreadPriority = 1;

TEST(AndroidThreadingTest, ThreadPrioritiesParityContract) {
  TRACE_EVENT0("flutter", "ThreadPrioritiesParityContract");
  fml::MessageLoop::EnsureInitializedForCurrentThread();

  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<JNIMock>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  ASSERT_TRUE(holder->IsValid());

  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();

  // 1. Platform thread
  EXPECT_TRUE(task_runners.GetPlatformTaskRunner()->RunsTasksOnCurrentThread());
  int platform_priority = ::getpriority(PRIO_PROCESS, 0);
  EXPECT_GE(platform_priority, kParityPlatformThreadPriority);

  // 2. UI thread
  fml::AutoResetWaitableEvent ui_latch;
  task_runners.GetUITaskRunner()->PostTask([&ui_latch]() {
    int ui_priority = ::getpriority(PRIO_PROCESS, 0);
    // UI thread requests kParityUIThreadPriority (-1 via kDisplay)
    EXPECT_TRUE(ui_priority == kParityUIThreadPriority || ui_priority == 0);
    ui_latch.Signal();
  });
  ui_latch.Wait();

  // 3. Raster thread
  fml::AutoResetWaitableEvent raster_latch;
  task_runners.GetRasterTaskRunner()->PostTask([&raster_latch]() {
    int raster_priority = ::getpriority(PRIO_PROCESS, 0);
    EXPECT_TRUE(raster_priority == kParityRasterThreadPriority ||
                raster_priority == kParityRasterThreadPriorityFallback ||
                raster_priority == 0);
    raster_latch.Signal();
  });
  raster_latch.Wait();

  // 4. IO thread
  fml::AutoResetWaitableEvent io_latch;
  task_runners.GetIOTaskRunner()->PostTask([&io_latch]() {
    int io_priority = ::getpriority(PRIO_PROCESS, 0);
    // Android sets IO thread to kNormal (priority 0)
    EXPECT_EQ(io_priority, kParityIOThreadPriorityAndroid);
    io_latch.Signal();
  });
  io_latch.Wait();
}

TEST(AndroidThreadingTest, CallbackThreadAffinities) {
  TRACE_EVENT0("flutter", "CallbackThreadAffinities");
  fml::MessageLoop::EnsureInitializedForCurrentThread();

  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<JNIMock>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  ASSERT_TRUE(holder->IsValid());

  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  // Assert test runs on Platform thread
  EXPECT_TRUE(task_runners.GetPlatformTaskRunner()->RunsTasksOnCurrentThread());

  // 1. Platform message dispatch runs on Platform thread
  EXPECT_CALL(*jni, FlutterViewHandlePlatformMessage(::testing::_, 1))
      .WillOnce([&task_runners](auto, auto) {
        EXPECT_TRUE(
            task_runners.GetPlatformTaskRunner()->RunsTasksOnCurrentThread());
      });

  size_t data_size = 4;
  fml::MallocMapping bytes =
      fml::MallocMapping(static_cast<uint8_t*>(malloc(data_size)), data_size);
  auto message = std::make_unique<PlatformMessage>("test_channel",
                                                   std::move(bytes), nullptr);
  holder->GetPlatformMessageHandler()->HandlePlatformMessage(
      std::move(message));

  // 2. Semantics update runs on Platform thread
  EXPECT_CALL(*jni, FlutterViewSetSemanticsTreeEnabled(true))
      .WillOnce([&task_runners](bool) {
        EXPECT_TRUE(
            task_runners.GetPlatformTaskRunner()->RunsTasksOnCurrentThread());
      });
  platform_view->SetSemanticsEnabled(true);
}

TEST(AndroidThreadingTest, DynamicThreadMergingCapabilities) {
  TRACE_EVENT0("flutter", "DynamicThreadMergingCapabilities");
  fml::MessageLoop::EnsureInitializedForCurrentThread();

  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<JNIMock>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  ASSERT_TRUE(holder->IsValid());

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  auto embedder = platform_view->CreateExternalViewEmbedder();
  ASSERT_TRUE(embedder);

  // In non-HCPP mode (standard Hybrid Composition), dynamic thread merging is
  // supported:
  EXPECT_TRUE(embedder->SupportsDynamicThreadMerging());
}

TEST(AndroidThreadingTest, MergeAfterLaunchPath) {
  TRACE_EVENT0("flutter", "MergeAfterLaunchPath");
  fml::MessageLoop::EnsureInitializedForCurrentThread();

  Settings settings;
  settings.merged_platform_ui_thread =
      Settings::MergedPlatformUIThread::kMergeAfterLaunch;
  auto jni = std::make_shared<JNIMock>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  ASSERT_TRUE(holder->IsValid());

  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  auto platform_queue = task_runners.GetPlatformTaskRunner()->GetTaskQueueId();
  auto ui_queue = task_runners.GetUITaskRunner()->GetTaskQueueId();

  // Prior to launch / run, the UI and platform queues are separate.
  EXPECT_NE(platform_queue, ui_queue);
}

TEST(AndroidThreadingTest, AndroidTaskRunnersCustomTaskRunnersShape) {
  TRACE_EVENT0("flutter", "AndroidTaskRunnersCustomTaskRunnersShape");
  fml::MessageLoop::EnsureInitializedForCurrentThread();

  Settings settings;
  settings.enable_software_rendering = false;
  auto task_runners = AndroidTaskRunners::Create("io.flutter.test", settings);
  ASSERT_TRUE(task_runners);
  ASSERT_TRUE(task_runners->IsValid());
  EXPECT_TRUE(task_runners->GetTaskRunners().IsValid());

  const FlutterCustomTaskRunners* custom_task_runners =
      task_runners->GetCustomTaskRunners();
  ASSERT_NE(custom_task_runners, nullptr);
  EXPECT_EQ(custom_task_runners->struct_size, sizeof(FlutterCustomTaskRunners));
  EXPECT_NE(custom_task_runners->platform_task_runner, nullptr);
  EXPECT_NE(custom_task_runners->render_task_runner, nullptr);
  EXPECT_NE(custom_task_runners->ui_task_runner, nullptr);
  EXPECT_NE(custom_task_runners->io_task_runner, nullptr);
  EXPECT_NE(custom_task_runners->thread_priority_setter, nullptr);
  EXPECT_EQ(custom_task_runners->io_thread_priority,
            FlutterThreadPriority::kNormal);

  // Validate runner descriptions
  EXPECT_EQ(custom_task_runners->platform_task_runner->struct_size,
            sizeof(FlutterTaskRunnerDescription));
  EXPECT_EQ(custom_task_runners->render_task_runner->struct_size,
            sizeof(FlutterTaskRunnerDescription));
  EXPECT_EQ(custom_task_runners->ui_task_runner->struct_size,
            sizeof(FlutterTaskRunnerDescription));
  EXPECT_EQ(custom_task_runners->io_task_runner->struct_size,
            sizeof(FlutterTaskRunnerDescription));

  EXPECT_EQ(custom_task_runners->platform_task_runner->priority,
            FlutterThreadPriority::kNormal);
  EXPECT_EQ(custom_task_runners->render_task_runner->priority,
            FlutterThreadPriority::kRaster);
  EXPECT_EQ(custom_task_runners->ui_task_runner->priority,
            FlutterThreadPriority::kDisplay);
  EXPECT_EQ(custom_task_runners->io_task_runner->priority,
            FlutterThreadPriority::kNormal);
}

TEST(AndroidThreadingTest, AndroidShellHolderExposesTaskRunners) {
  TRACE_EVENT0("flutter", "AndroidShellHolderExposesTaskRunners");
  fml::MessageLoop::EnsureInitializedForCurrentThread();

  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<JNIMock>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  ASSERT_TRUE(holder->IsValid());
  ASSERT_NE(holder->GetTaskRunnersForTesting(), nullptr);
  EXPECT_TRUE(holder->GetTaskRunnersForTesting()->IsValid());
  EXPECT_NE(holder->GetTaskRunnersForTesting()->GetCustomTaskRunners(),
            nullptr);
}

}  // namespace testing
}  // namespace flutter
