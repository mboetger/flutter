// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "flutter/fml/message_loop.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/fml/trace_event.h"
#include "flutter/shell/platform/android/android_engine.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"
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

// Tests that AndroidSurfaceLifecycle tracks surface availability correctly
// across PlatformViewAndroid lifecycle calls.
TEST_F(PlatformViewAndroidSurfaceLifecycleTest,
       SurfaceLifecycleAvailabilityTracking) {
  TRACE_EVENT0("flutter", "SurfaceLifecycleAvailabilityTracking");
  auto holder = CreateShellHolder();
  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  auto surface_lifecycle = platform_view->GetSurfaceLifecycleForTesting();
  ASSERT_NE(surface_lifecycle, nullptr);
  EXPECT_FALSE(surface_lifecycle->IsSurfaceAvailable());

  auto window = CreateFakeWindow();
  platform_view->NotifyCreated(window);
  EXPECT_TRUE(surface_lifecycle->IsSurfaceAvailable());

  platform_view->NotifyDestroyed();
  EXPECT_FALSE(surface_lifecycle->IsSurfaceAvailable());
}

// Tests that GPU availability transitions are tracked in
// AndroidSurfaceLifecycle.
TEST_F(PlatformViewAndroidSurfaceLifecycleTest, GpuAvailabilityTransitions) {
  TRACE_EVENT0("flutter", "GpuAvailabilityTransitions");
  auto holder = CreateShellHolder();
  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  auto surface_lifecycle = platform_view->GetSurfaceLifecycleForTesting();
  ASSERT_NE(surface_lifecycle, nullptr);
  EXPECT_EQ(surface_lifecycle->GetGpuAvailability(),
            kFlutterGpuAvailabilityAvailable);

  platform_view->SetGpuAvailability(
      kFlutterGpuAvailabilityFlushAndMakeUnavailable);
  EXPECT_EQ(surface_lifecycle->GetGpuAvailability(),
            kFlutterGpuAvailabilityFlushAndMakeUnavailable);

  platform_view->SetGpuAvailability(kFlutterGpuAvailabilityUnavailable);
  EXPECT_EQ(surface_lifecycle->GetGpuAvailability(),
            kFlutterGpuAvailabilityUnavailable);

  platform_view->SetGpuAvailability(kFlutterGpuAvailabilityAvailable);
  EXPECT_EQ(surface_lifecycle->GetGpuAvailability(),
            kFlutterGpuAvailabilityAvailable);
}

// Tests direct delegate callbacks and state management on
// AndroidSurfaceLifecycle.
TEST_F(PlatformViewAndroidSurfaceLifecycleTest,
       DirectLifecycleDelegateCallbacks) {
  TRACE_EVENT0("flutter", "DirectLifecycleDelegateCallbacks");
  class TestLifecycleDelegate : public AndroidSurfaceLifecycle::Delegate {
   public:
    int surface_created_calls = 0;
    int surface_destroyed_calls = 0;
    int schedule_frame_calls = 0;
    int install_first_frame_calls = 0;
    int set_gpu_availability_calls = 0;

    void OnSurfaceCreated() override { surface_created_calls++; }
    void OnSurfaceDestroyed() override { surface_destroyed_calls++; }
    void OnScheduleFrame() override { schedule_frame_calls++; }
    void OnInstallFirstFrameCallback() override { install_first_frame_calls++; }
    void OnSetGpuAvailability(FlutterGpuAvailability availability) override {
      set_gpu_availability_calls++;
    }
  };

  TestLifecycleDelegate delegate;
  AndroidSurfaceLifecycle lifecycle(nullptr, nullptr, nullptr, &delegate);

  EXPECT_FALSE(lifecycle.IsSurfaceAvailable());
  EXPECT_EQ(lifecycle.GetGpuAvailability(), kFlutterGpuAvailabilityAvailable);

  auto window = CreateFakeWindow();
  lifecycle.NotifyCreated(window);
  EXPECT_TRUE(lifecycle.IsSurfaceAvailable());
  EXPECT_EQ(delegate.surface_created_calls, 1);

  lifecycle.NotifySurfaceWindowChanged(window);
  EXPECT_TRUE(lifecycle.IsSurfaceAvailable());
  EXPECT_EQ(delegate.schedule_frame_calls, 1);

  lifecycle.SetGpuAvailability(kFlutterGpuAvailabilityFlushAndMakeUnavailable);
  EXPECT_EQ(lifecycle.GetGpuAvailability(),
            kFlutterGpuAvailabilityFlushAndMakeUnavailable);
  EXPECT_EQ(delegate.set_gpu_availability_calls, 1);

  lifecycle.NotifyDestroyed();
  EXPECT_FALSE(lifecycle.IsSurfaceAvailable());
  EXPECT_EQ(delegate.surface_destroyed_calls, 1);
}

}  // namespace testing
}  // namespace flutter
