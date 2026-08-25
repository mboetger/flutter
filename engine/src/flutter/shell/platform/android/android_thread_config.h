// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_THREAD_CONFIG_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_THREAD_CONFIG_H_

#include "flutter/fml/thread.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

// Converts an FML thread priority to the corresponding FlutterThreadPriority.
FlutterThreadPriority ToFlutterThreadPriority(
    fml::Thread::ThreadPriority priority);

// Converts a FlutterThreadPriority to the corresponding FML thread priority.
fml::Thread::ThreadPriority ToFMLThreadPriority(FlutterThreadPriority priority);

// Standard Android thread priority setter conforming to the
// FlutterCustomTaskRunners::thread_priority_setter function pointer signature.
//
// Sets the Linux/Android thread nice value and CPU affinity based on the
// specified FlutterThreadPriority.
void AndroidThreadPrioritySetter(FlutterThreadPriority priority);

// Thread config setter callback for fml::Thread and flutter::ThreadHost.
// Configures the thread name and priority on Android.
void AndroidPlatformThreadConfigSetter(const fml::Thread::ThreadConfig& config);

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_THREAD_CONFIG_H_
