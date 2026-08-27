// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_THREAD_CONFIG_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_THREAD_CONFIG_H_

#include "flutter/fml/cpu_affinity.h"
#include "flutter/fml/thread.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

/// Maps a FlutterThreadPriority to the corresponding Android nice value.
int AndroidGetNiceValue(FlutterThreadPriority priority);

/// Maps a FlutterThreadPriority to the corresponding CPU affinity request.
fml::CpuAffinity AndroidGetCpuAffinity(FlutterThreadPriority priority);

/// Sets the calling thread's priority and CPU affinity for the given
/// FlutterThreadPriority on Android.
///
/// Matches the signature of FlutterCustomTaskRunners::thread_priority_setter.
void AndroidSetThreadPriority(FlutterThreadPriority priority);

/// Converts an fml::Thread::ThreadPriority to FlutterThreadPriority.
FlutterThreadPriority ToFlutterThreadPriority(
    fml::Thread::ThreadPriority priority);

/// Configures thread name and priority for an FML thread on Android.
/// Suitable for passing to fml::ThreadHost::ThreadHostConfig.
void AndroidPlatformThreadConfigSetter(const fml::Thread::ThreadConfig& config);

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_THREAD_CONFIG_H_
