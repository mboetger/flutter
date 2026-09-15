// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "flutter/fml/message_loop.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/fml/trace_event.h"
#include "flutter/shell/platform/android/android_shell_holder.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "flutter/shell/platform/android/platform_view_android.h"
#include "flutter/shell/platform/android/surface/android_native_window.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

class PlatformViewAndroidSurfaceLifecycleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    fml::MessageLoop::EnsureInitializedForCurrentThread();
  }

  std::unique_ptr<AndroidShellHolder> CreateShellHolder() {
    Settings settings;
    settings.enable_software_rendering = false;
    auto jni = std::make_shared<JNIMock>();
    return std::make_unique<AndroidShellHolder>(
        settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  }

  fml::RefPtr<AndroidNativeWindow> CreateFakeWindow() {
    return fml::MakeRefCounted<AndroidNativeWindow>(nullptr,
                                                    /*is_fake_window=*/true);
  }
};

// Characterization: NotifyCreated sets up native window on raster thread
// and validates surface readiness.
TEST_F(PlatformViewAndroidSurfaceLifecycleTest, NotifyCreatedValidSurface) {
  TRACE_EVENT0("flutter", "NotifyCreatedValidSurface");
  auto holder = CreateShellHolder();
  ASSERT_NE(holder, nullptr);
  ASSERT_TRUE(holder->IsValid());

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  EXPECT_TRUE(task_runners.GetPlatformTaskRunner()->RunsTasksOnCurrentThread());

  auto window = CreateFakeWindow();
  platform_view->NotifyCreated(window);

  fml::AutoResetWaitableEvent latch;
  task_runners.GetRasterTaskRunner()->PostTask([&latch, &task_runners]() {
    EXPECT_TRUE(task_runners.GetRasterTaskRunner()->RunsTasksOnCurrentThread());
    latch.Signal();
  });
  latch.Wait();
}

// Characterization: NotifyDestroyed tears down on-screen context cleanly
// without UAF.
TEST_F(PlatformViewAndroidSurfaceLifecycleTest,
       NotifyDestroyedReleasesGpuResources) {
  TRACE_EVENT0("flutter", "NotifyDestroyedReleasesGpuResources");
  auto holder = CreateShellHolder();
  auto platform_view = holder->GetPlatformView();
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();

  auto window = CreateFakeWindow();
  platform_view->NotifyCreated(window);

  EXPECT_TRUE(task_runners.GetPlatformTaskRunner()->RunsTasksOnCurrentThread());
  platform_view->NotifyDestroyed();

  fml::AutoResetWaitableEvent latch;
  task_runners.GetRasterTaskRunner()->PostTask([&latch, &task_runners]() {
    EXPECT_TRUE(task_runners.GetRasterTaskRunner()->RunsTasksOnCurrentThread());
    latch.Signal();
  });
  latch.Wait();
}

// Characterization: Background and return cycle (NotifyCreated ->
// NotifyDestroyed -> NotifyCreated).
TEST_F(PlatformViewAndroidSurfaceLifecycleTest, BackgroundAndForegroundCycle) {
  TRACE_EVENT0("flutter", "BackgroundAndForegroundCycle");
  auto holder = CreateShellHolder();
  auto platform_view = holder->GetPlatformView();
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();

  // Activity foreground: initial surface creation.
  auto window1 = CreateFakeWindow();
  platform_view->NotifyCreated(window1);

  // Activity background: surface destroyed.
  platform_view->NotifyDestroyed();

  // Activity resume: new surface window attached.
  auto window2 = CreateFakeWindow();
  platform_view->NotifyCreated(window2);

  fml::AutoResetWaitableEvent latch;
  task_runners.GetRasterTaskRunner()->PostTask([&latch, &task_runners]() {
    EXPECT_TRUE(task_runners.GetRasterTaskRunner()->RunsTasksOnCurrentThread());
    latch.Signal();
  });
  latch.Wait();
}

// Characterization: NotifySurfaceWindowChanged replaces window and schedules
// frame.
TEST_F(PlatformViewAndroidSurfaceLifecycleTest,
       NotifySurfaceWindowChangedMidFrame) {
  TRACE_EVENT0("flutter", "NotifySurfaceWindowChangedMidFrame");
  auto holder = CreateShellHolder();
  auto platform_view = holder->GetPlatformView();
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();

  auto window1 = CreateFakeWindow();
  platform_view->NotifyCreated(window1);

  // Window change replaces native window and schedules a frame.
  auto window2 = CreateFakeWindow();
  platform_view->NotifySurfaceWindowChanged(window2);

  fml::AutoResetWaitableEvent latch;
  task_runners.GetRasterTaskRunner()->PostTask([&latch, &task_runners]() {
    EXPECT_TRUE(task_runners.GetRasterTaskRunner()->RunsTasksOnCurrentThread());
    latch.Signal();
  });
  latch.Wait();
}

// Characterization: NotifyDestroyed with frame/task in flight on the raster
// thread.
TEST_F(PlatformViewAndroidSurfaceLifecycleTest,
       NotifyDestroyedWithFrameInFlight) {
  TRACE_EVENT0("flutter", "NotifyDestroyedWithFrameInFlight");
  auto holder = CreateShellHolder();
  auto platform_view = holder->GetPlatformView();
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();

  auto window = CreateFakeWindow();
  platform_view->NotifyCreated(window);

  fml::AutoResetWaitableEvent in_flight_started;
  fml::AutoResetWaitableEvent in_flight_can_finish;
  task_runners.GetRasterTaskRunner()->PostTask([&]() {
    in_flight_started.Signal();
    in_flight_can_finish.Wait();
  });

  in_flight_started.Wait();
  in_flight_can_finish.Signal();

  platform_view->NotifyDestroyed();

  fml::AutoResetWaitableEvent latch;
  task_runners.GetRasterTaskRunner()->PostTask([&latch, &task_runners]() {
    EXPECT_TRUE(task_runners.GetRasterTaskRunner()->RunsTasksOnCurrentThread());
    latch.Signal();
  });
  latch.Wait();
}

// CHARACTERIZATION: Double NotifyCreated without intervening NotifyDestroyed.
// Current behavior: replaces the surface window on the raster thread without
// crash.
TEST_F(PlatformViewAndroidSurfaceLifecycleTest,
       DoubleNotifyCreatedIdempotency) {
  TRACE_EVENT0("flutter", "DoubleNotifyCreatedIdempotency");
  auto holder = CreateShellHolder();
  auto platform_view = holder->GetPlatformView();

  auto window1 = CreateFakeWindow();
  platform_view->NotifyCreated(window1);

  auto window2 = CreateFakeWindow();
  platform_view->NotifyCreated(window2);
}

// CHARACTERIZATION: Double NotifyDestroyed without intervening NotifyCreated.
// Current behavior: idempotent, calls TeardownOnScreenContext safely.
TEST_F(PlatformViewAndroidSurfaceLifecycleTest,
       DoubleNotifyDestroyedIdempotency) {
  TRACE_EVENT0("flutter", "DoubleNotifyDestroyedIdempotency");
  auto holder = CreateShellHolder();
  auto platform_view = holder->GetPlatformView();

  auto window = CreateFakeWindow();
  platform_view->NotifyCreated(window);

  platform_view->NotifyDestroyed();
  platform_view->NotifyDestroyed();
}

}  // namespace testing
}  // namespace flutter
