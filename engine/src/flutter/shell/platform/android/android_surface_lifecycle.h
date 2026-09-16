// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_LIFECYCLE_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_LIFECYCLE_H_

#include <memory>

#include "flutter/fml/macros.h"
#include "flutter/fml/memory/ref_ptr.h"
#include "flutter/fml/task_runner.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/surface/android_native_window.h"
#include "flutter/shell/platform/android/surface/android_surface.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

// Manages the lifecycle of the Android surface and synchronization with
// the raster task runner. Matches the lifecycle shape established in T-0.7
// and T-1.2.
class AndroidSurfaceLifecycle {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnSurfaceCreated() = 0;
    virtual void OnSurfaceDestroyed() = 0;
    virtual void OnScheduleFrame() = 0;
    virtual void OnInstallFirstFrameCallback() = 0;
    virtual void OnSetGpuAvailability(FlutterGpuAvailability availability) {}
  };

  AndroidSurfaceLifecycle(fml::RefPtr<fml::TaskRunner> raster_task_runner,
                          std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
                          AndroidSurface* android_surface,
                          Delegate* delegate);

  ~AndroidSurfaceLifecycle() = default;

  void NotifyCreated(fml::RefPtr<AndroidNativeWindow> native_window);
  void NotifySurfaceWindowChanged(
      fml::RefPtr<AndroidNativeWindow> native_window);
  void NotifyDestroyed();
  void NotifyChanged(const DlISize& size);

  void SetGpuAvailability(FlutterGpuAvailability availability);
  FlutterGpuAvailability GetGpuAvailability() const;

  bool IsSurfaceAvailable() const;

 private:
  fml::RefPtr<fml::TaskRunner> raster_task_runner_;
  std::shared_ptr<PlatformViewAndroidJNI> jni_facade_;
  AndroidSurface* android_surface_ = nullptr;
  Delegate* delegate_ = nullptr;
  bool is_surface_available_ = false;
  FlutterGpuAvailability gpu_availability_ = kFlutterGpuAvailabilityAvailable;

  FML_DISALLOW_COPY_AND_ASSIGN(AndroidSurfaceLifecycle);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_LIFECYCLE_H_
