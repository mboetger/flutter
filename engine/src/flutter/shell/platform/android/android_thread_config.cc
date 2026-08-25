// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_thread_config.h"

#include <sys/resource.h>
#include <sys/time.h>

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

fml::Thread::ThreadPriority ToFMLThreadPriority(
    FlutterThreadPriority priority) {
  switch (priority) {
    case FlutterThreadPriority::kBackground:
      return fml::Thread::ThreadPriority::kBackground;
    case FlutterThreadPriority::kDisplay:
      return fml::Thread::ThreadPriority::kDisplay;
    case FlutterThreadPriority::kRaster:
      return fml::Thread::ThreadPriority::kRaster;
    case FlutterThreadPriority::kNormal:
    default:
      return fml::Thread::ThreadPriority::kNormal;
  }
}

void AndroidThreadPrioritySetter(FlutterThreadPriority priority) {
  switch (priority) {
    case FlutterThreadPriority::kBackground: {
      fml::RequestAffinity(fml::CpuAffinity::kEfficiency);
      if (::setpriority(PRIO_PROCESS, 0, 10) != 0) {
        FML_LOG(ERROR) << "Failed to set background task runner priority";
      }
      break;
    }
    case FlutterThreadPriority::kDisplay: {
      fml::RequestAffinity(fml::CpuAffinity::kNotEfficiency);
      if (::setpriority(PRIO_PROCESS, 0, -1) != 0) {
        FML_LOG(ERROR) << "Failed to set display/UI task runner priority";
      }
      break;
    }
    case FlutterThreadPriority::kRaster: {
      fml::RequestAffinity(fml::CpuAffinity::kNotEfficiency);
      // Android describes -8 as "most important display threads, for
      // compositing the screen and retrieving input events". Conservatively
      // set the raster thread to slightly lower priority than it.
      if (::setpriority(PRIO_PROCESS, 0, -5) != 0) {
        // Defensive fallback. Depending on the OEM, it may not be possible
        // to set priority to -5.
        if (::setpriority(PRIO_PROCESS, 0, -2) != 0) {
          FML_LOG(ERROR) << "Failed to set raster task runner priority";
        }
      }
      break;
    }
    case FlutterThreadPriority::kNormal:
    default:
      fml::RequestAffinity(fml::CpuAffinity::kNotPerformance);
      if (::setpriority(PRIO_PROCESS, 0, 0) != 0) {
        FML_LOG(ERROR) << "Failed to set normal task runner priority";
      }
      break;
  }
}

void AndroidPlatformThreadConfigSetter(
    const fml::Thread::ThreadConfig& config) {
  fml::Thread::SetCurrentThreadName(config);
  AndroidThreadPrioritySetter(ToFlutterThreadPriority(config.priority));
}

}  // namespace flutter
