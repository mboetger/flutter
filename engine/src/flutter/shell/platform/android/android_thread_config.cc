// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_thread_config.h"

#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

#include "flutter/fml/logging.h"

namespace flutter {

int AndroidGetNiceValue(FlutterThreadPriority priority) {
  switch (priority) {
    case FlutterThreadPriority::kBackground:
      return 10;
    case FlutterThreadPriority::kDisplay:
      return -1;
    case FlutterThreadPriority::kRaster:
      return -5;
    case FlutterThreadPriority::kNormal:
    default:
      return 0;
  }
}

fml::CpuAffinity AndroidGetCpuAffinity(FlutterThreadPriority priority) {
  switch (priority) {
    case FlutterThreadPriority::kBackground:
      return fml::CpuAffinity::kEfficiency;
    case FlutterThreadPriority::kDisplay:
    case FlutterThreadPriority::kRaster:
      return fml::CpuAffinity::kNotEfficiency;
    case FlutterThreadPriority::kNormal:
    default:
      return fml::CpuAffinity::kNotPerformance;
  }
}

void AndroidSetThreadPriority(FlutterThreadPriority priority) {
  fml::RequestAffinity(AndroidGetCpuAffinity(priority));

  switch (priority) {
    case FlutterThreadPriority::kBackground: {
      if (::setpriority(PRIO_PROCESS, 0, AndroidGetNiceValue(priority)) != 0) {
        FML_LOG(ERROR) << "Failed to set IO task runner priority";
      }
      break;
    }
    case FlutterThreadPriority::kDisplay: {
      if (::setpriority(PRIO_PROCESS, 0, AndroidGetNiceValue(priority)) != 0) {
        FML_LOG(ERROR) << "Failed to set UI task runner priority";
      }
      break;
    }
    case FlutterThreadPriority::kRaster: {
      // Android describes -8 as "most important display threads, for
      // compositing the screen and retrieving input events". Conservatively
      // set the raster thread to slightly lower priority than it.
      if (::setpriority(PRIO_PROCESS, 0, AndroidGetNiceValue(priority)) != 0) {
        // Defensive fallback. Depending on the OEM, it may not be possible
        // to set priority to -5.
        if (::setpriority(PRIO_PROCESS, 0, -2) != 0) {
          FML_LOG(ERROR) << "Failed to set raster task runner priority";
        }
      }
      break;
    }
    default:
      if (::setpriority(PRIO_PROCESS, 0, AndroidGetNiceValue(priority)) != 0) {
        FML_LOG(ERROR) << "Failed to set priority";
      }
      break;
  }
}

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

void AndroidPlatformThreadConfigSetter(
    const fml::Thread::ThreadConfig& config) {
  fml::Thread::SetCurrentThreadName(config);
  AndroidSetThreadPriority(ToFlutterThreadPriority(config.priority));
}

}  // namespace flutter
