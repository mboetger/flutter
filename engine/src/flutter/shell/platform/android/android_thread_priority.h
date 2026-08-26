// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_THREAD_PRIORITY_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_THREAD_PRIORITY_H_

#include "flutter/fml/thread.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

// Android nice priority levels range from -20 (highest priority) to 19 (lowest
// priority).

// Priority 10 represents standard background task priority
// (THREAD_PRIORITY_BACKGROUND in Android SDK).
constexpr int kBackgroundThreadNicePriority = 10;

// Priority -4 represents Android display/UI thread priority
// (THREAD_PRIORITY_DISPLAY in Android SDK).
constexpr int kDisplayThreadNicePriority = -4;

// Priority -5 gives display-level compositor priority to the raster thread.
constexpr int kRasterThreadPrimaryNicePriority = -5;

// Priority -2 is a conservative fallback if -5 is disallowed by OEM process
// limits.
constexpr int kRasterThreadFallbackNicePriority = -2;

// Priority 0 represents standard normal thread nice priority
// (THREAD_PRIORITY_DEFAULT in Android SDK).
constexpr int kNormalThreadNicePriority = 0;

// Priority 1 gives workers slightly lower priority than interactive UI tasks.
constexpr int kWorkerThreadNicePriority = 1;

//----------------------------------------------------------------------------
/// @brief      Converts an internal FML thread priority to the public
///             Embedder API FlutterThreadPriority enum.
///
/// @param[in]  priority  The FML thread priority.
///
/// @return     The corresponding FlutterThreadPriority.
///
FlutterThreadPriority ToFlutterThreadPriority(
    fml::Thread::ThreadPriority priority);

//----------------------------------------------------------------------------
/// @brief      Sets the thread priority and CPU affinity for the current thread
///             based on the requested FlutterThreadPriority.
///
/// @param[in]  priority  The requested thread priority.
///
void AndroidPlatformThreadPrioritySetter(FlutterThreadPriority priority);

//----------------------------------------------------------------------------
/// @brief      Sets the thread name and priority for an FML thread on Android.
///
/// @param[in]  config  The thread configuration containing name and priority.
///
void AndroidPlatformThreadConfigSetter(const fml::Thread::ThreadConfig& config);

//----------------------------------------------------------------------------
/// @brief      Sets the current thread priority to worker thread priority.
///
void AndroidConfigureWorkerThreadPriority();

//----------------------------------------------------------------------------
/// @brief      Helper to construct a FlutterCustomTaskRunners struct populated
///             with the Android thread priority setter callback.
///
/// @param[in]  platform_task_runner  Optional custom platform task runner.
/// @param[in]  render_task_runner    Optional custom render/raster task runner.
/// @param[in]  ui_task_runner        Optional custom UI task runner.
///
/// @return     A populated FlutterCustomTaskRunners struct.
///
FlutterCustomTaskRunners CreateAndroidCustomTaskRunners(
    const FlutterTaskRunnerDescription* platform_task_runner = nullptr,
    const FlutterTaskRunnerDescription* render_task_runner = nullptr,
    const FlutterTaskRunnerDescription* ui_task_runner = nullptr);

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_THREAD_PRIORITY_H_
