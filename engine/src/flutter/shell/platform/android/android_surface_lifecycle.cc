// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_surface_lifecycle.h"

#include <utility>

#include "flutter/fml/synchronization/waitable_event.h"

namespace flutter {

AndroidSurfaceLifecycle::AndroidSurfaceLifecycle(
    fml::RefPtr<fml::TaskRunner> raster_task_runner,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    AndroidSurface* android_surface,
    Delegate* delegate)
    : raster_task_runner_(std::move(raster_task_runner)),
      jni_facade_(std::move(jni_facade)),
      android_surface_(android_surface),
      delegate_(delegate) {}

void AndroidSurfaceLifecycle::NotifyCreated(
    fml::RefPtr<AndroidNativeWindow> native_window) {
  if (android_surface_) {
    if (delegate_) {
      delegate_->OnInstallFirstFrameCallback();
    }

    fml::AutoResetWaitableEvent latch;
    fml::TaskRunner::RunNowOrPostTask(
        raster_task_runner_,
        [&latch, surface = android_surface_,
         native_window = std::move(native_window), jni_facade = jni_facade_]() {
          surface->SetNativeWindow(native_window, jni_facade);
          latch.Signal();
        });
    latch.Wait();
  }

  is_surface_available_ = true;

  if (delegate_) {
    delegate_->OnSurfaceCreated();
  }
}

void AndroidSurfaceLifecycle::NotifySurfaceWindowChanged(
    fml::RefPtr<AndroidNativeWindow> native_window) {
  if (android_surface_) {
    fml::AutoResetWaitableEvent latch;
    fml::TaskRunner::RunNowOrPostTask(
        raster_task_runner_,
        [&latch, surface = android_surface_,
         native_window = std::move(native_window), jni_facade = jni_facade_]() {
          surface->TeardownOnScreenContext();
          surface->SetNativeWindow(native_window, jni_facade);
          latch.Signal();
        });
    latch.Wait();
  }

  is_surface_available_ = true;

  if (delegate_) {
    delegate_->OnScheduleFrame();
  }
}

void AndroidSurfaceLifecycle::NotifyDestroyed() {
  if (delegate_) {
    delegate_->OnSurfaceDestroyed();
  }

  if (android_surface_) {
    fml::AutoResetWaitableEvent latch;
    fml::TaskRunner::RunNowOrPostTask(raster_task_runner_,
                                      [&latch, surface = android_surface_]() {
                                        surface->TeardownOnScreenContext();
                                        latch.Signal();
                                      });
    latch.Wait();
  }

  is_surface_available_ = false;
}

void AndroidSurfaceLifecycle::NotifyChanged(const DlISize& size) {
  if (!android_surface_) {
    return;
  }
  fml::AutoResetWaitableEvent latch;
  fml::TaskRunner::RunNowOrPostTask(
      raster_task_runner_, [&latch, surface = android_surface_, size]() {
        surface->OnScreenSurfaceResize(size);
        latch.Signal();
      });
  latch.Wait();
}

void AndroidSurfaceLifecycle::SetGpuAvailability(
    FlutterGpuAvailability availability) {
  gpu_availability_ = availability;
  if (delegate_) {
    delegate_->OnSetGpuAvailability(availability);
  }
}

FlutterGpuAvailability AndroidSurfaceLifecycle::GetGpuAvailability() const {
  return gpu_availability_;
}

bool AndroidSurfaceLifecycle::IsSurfaceAvailable() const {
  return is_surface_available_;
}

}  // namespace flutter
