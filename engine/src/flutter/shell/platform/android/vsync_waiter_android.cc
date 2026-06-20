// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/vsync_waiter_android.h"

#include <cmath>
#include <utility>

#include "flutter/common/task_runners.h"
#include "flutter/fml/logging.h"
#include "flutter/fml/platform/android/jni_util.h"
#include "flutter/fml/platform/android/scoped_java_ref.h"
#include "flutter/fml/trace_event.h"
#include "impeller/toolkit/android/choreographer.h"

namespace flutter {

static std::atomic_uint g_refresh_rate_ = 60;

VsyncWaiterAndroid::VsyncWaiterAndroid(const flutter::TaskRunners& task_runners)
    : VsyncWaiter(task_runners) {}

VsyncWaiterAndroid::~VsyncWaiterAndroid() = default;

// |VsyncWaiter|
void VsyncWaiterAndroid::AwaitVSync() {
  std::weak_ptr<VsyncWaiter> weak_this = shared_from_this();
  fml::TaskRunner::RunNowOrPostTask(
      task_runners_.GetUITaskRunner(), [weak_this = std::move(weak_this)]() {
        const auto& choreographer =
            impeller::android::Choreographer::GetInstance();
        choreographer.PostFrameCallback([weak_this](auto time) {
          auto shared_this = weak_this.lock();
          if (!shared_this) {
            return;
          }
          auto time_ns =
              std::chrono::time_point_cast<std::chrono::nanoseconds>(time)
                  .time_since_epoch()
                  .count();

          auto frame_time = fml::TimePoint::FromEpochDelta(
              fml::TimeDelta::FromNanoseconds(time_ns));
          auto now = fml::TimePoint::Now();
          if (frame_time > now) {
            frame_time = now;
          }
          auto target_time = frame_time + fml::TimeDelta::FromNanoseconds(
                                              1000000000.0 / g_refresh_rate_);

          TRACE_EVENT2_INT("flutter", "PlatformVsync", "frame_start_time",
                           frame_time.ToEpochDelta().ToMicroseconds(),
                           "frame_target_time",
                           target_time.ToEpochDelta().ToMicroseconds());

          shared_this->FireCallback(frame_time, target_time);
        });
      });
}

// static
void VsyncWaiterAndroid::OnUpdateRefreshRate(JNIEnv* env,
                                             jclass jcaller,
                                             jfloat refresh_rate) {
  FML_DCHECK(refresh_rate > 0);
  g_refresh_rate_ = static_cast<uint>(refresh_rate);
}

// static
bool VsyncWaiterAndroid::Register(JNIEnv* env) {
  static const JNINativeMethod methods[] = {
      {
          .name = "nativeUpdateRefreshRate",
          .signature = "(F)V",
          .fnPtr = reinterpret_cast<void*>(&OnUpdateRefreshRate),
      }};

  jclass clazz = env->FindClass("io/flutter/embedding/engine/FlutterJNI");

  if (clazz == nullptr) {
    return false;
  }

  return env->RegisterNatives(clazz, methods, std::size(methods)) == 0;
}

}  // namespace flutter
