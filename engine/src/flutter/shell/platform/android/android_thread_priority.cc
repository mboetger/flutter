// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_thread_priority.h"

#include <sys/resource.h>
#include <unistd.h>

#include "flutter/fml/cpu_affinity.h"
#include "flutter/fml/logging.h"

namespace flutter {

FlutterThreadPriority ToFlutterThreadPriority(
    fml::Thread::ThreadPriority priority) {
  switch (priority) {
    case fml::Thread::ThreadPriority::kBackground:
      return FlutterThreadPriority::kBackground;
    case fml::Thread::ThreadPriority::kDisplay:
      return FlutterThreadPriority::kDisplay;
    case fml::Thread::ThreadPriority::kRaster:
      return FlutterThreadPriority::kRaster;
    case fml::Thread::ThreadPriority::kNormal:
    default:
      return FlutterThreadPriority::kNormal;
  }
}

void AndroidPlatformThreadPrioritySetter(FlutterThreadPriority priority) {
  switch (priority) {
    case FlutterThreadPriority::kBackground: {
      fml::RequestAffinity(fml::CpuAffinity::kEfficiency);
      if (::setpriority(PRIO_PROCESS, 0, kBackgroundThreadNicePriority) != 0) {
        FML_LOG(ERROR) << "Failed to set background task runner priority";
      }
      break;
    }
    case FlutterThreadPriority::kDisplay: {
      fml::RequestAffinity(fml::CpuAffinity::kNotEfficiency);
      if (::setpriority(PRIO_PROCESS, 0, kDisplayThreadNicePriority) != 0) {
        FML_LOG(ERROR) << "Failed to set display task runner priority";
      }
      break;
    }
    case FlutterThreadPriority::kRaster: {
      fml::RequestAffinity(fml::CpuAffinity::kNotEfficiency);
      // Android describes -8 as "most important display threads, for
      // compositing the screen and retrieving input events". Conservatively
      // set the raster thread to slightly lower priority than it (-5).
      if (::setpriority(PRIO_PROCESS, 0, kRasterThreadPrimaryNicePriority) !=
          0) {
        // Defensive fallback. Depending on the OEM, it may not be possible
        // to set priority to -5.
        if (::setpriority(PRIO_PROCESS, 0, kRasterThreadFallbackNicePriority) !=
            0) {
          FML_LOG(ERROR) << "Failed to set raster task runner priority";
        }
      }
      break;
    }
    case FlutterThreadPriority::kNormal:
    default: {
      fml::RequestAffinity(fml::CpuAffinity::kNotPerformance);
      if (::setpriority(PRIO_PROCESS, 0, kNormalThreadNicePriority) != 0) {
        FML_LOG(ERROR) << "Failed to set normal priority";
      }
      break;
    }
  }
}

void AndroidPlatformThreadConfigSetter(
    const fml::Thread::ThreadConfig& config) {
  fml::Thread::SetCurrentThreadName(config);
  AndroidPlatformThreadPrioritySetter(ToFlutterThreadPriority(config.priority));
}

void AndroidConfigureWorkerThreadPriority() {
  if (::setpriority(PRIO_PROCESS, gettid(), kWorkerThreadNicePriority) != 0) {
    FML_LOG(ERROR) << "Failed to set Workers task runner priority";
  }
}

FlutterCustomTaskRunners CreateAndroidCustomTaskRunners(
    const FlutterTaskRunnerDescription* platform_task_runner,
    const FlutterTaskRunnerDescription* render_task_runner,
    const FlutterTaskRunnerDescription* ui_task_runner) {
  FlutterCustomTaskRunners custom_task_runners = {};
  custom_task_runners.struct_size = sizeof(FlutterCustomTaskRunners);
  custom_task_runners.platform_task_runner = platform_task_runner;
  custom_task_runners.render_task_runner = render_task_runner;
  custom_task_runners.thread_priority_setter =
      AndroidPlatformThreadPrioritySetter;
  custom_task_runners.ui_task_runner = ui_task_runner;
  return custom_task_runners;
}

}  // namespace flutter
