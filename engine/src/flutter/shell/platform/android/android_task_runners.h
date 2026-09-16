// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_TASK_RUNNERS_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_TASK_RUNNERS_H_

#include <memory>
#include <string>

#include "flutter/common/settings.h"
#include "flutter/common/task_runners.h"
#include "flutter/fml/macros.h"
#include "flutter/fml/thread.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

struct ThreadHost;

/// Configures thread priority for Android platform and worker threads according
/// to FlutterThreadPriority.
void AndroidPlatformThreadPrioritySetter(FlutterThreadPriority priority);

/// Adapter function for fml::Thread::ThreadConfig configuring thread names
/// and calling AndroidPlatformThreadPrioritySetter.
void AndroidPlatformThreadConfigSetter(const fml::Thread::ThreadConfig& config);

/// Encapsulates thread creation and task runner management for the Android
/// embedder, exposing both the internal flutter::TaskRunners and the
/// FlutterCustomTaskRunners Embedder API shape.
class AndroidTaskRunners {
 public:
  static std::unique_ptr<AndroidTaskRunners> Create(
      const std::string& thread_label,
      const Settings& settings);

  AndroidTaskRunners(const std::string& thread_label,
                     const Settings& settings,
                     std::shared_ptr<ThreadHost> thread_host);

  ~AndroidTaskRunners();

  bool IsValid() const { return task_runners_.IsValid(); }

  const flutter::TaskRunners& GetTaskRunners() const { return task_runners_; }

  const FlutterCustomTaskRunners* GetCustomTaskRunners() const {
    return &custom_task_runners_;
  }

  std::shared_ptr<ThreadHost> GetThreadHost() const { return thread_host_; }

 private:
  static flutter::TaskRunners MakeTaskRunners(const std::string& thread_label,
                                              const Settings& settings,
                                              const ThreadHost* thread_host);

  void InitializeCustomTaskRunners();

  std::shared_ptr<ThreadHost> thread_host_;
  flutter::TaskRunners task_runners_;
  FlutterTaskRunnerDescription platform_description_ = {};
  FlutterTaskRunnerDescription raster_description_ = {};
  FlutterTaskRunnerDescription ui_description_ = {};
  FlutterTaskRunnerDescription io_description_ = {};
  FlutterCustomTaskRunners custom_task_runners_ = {};

  FML_DISALLOW_COPY_AND_ASSIGN(AndroidTaskRunners);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_TASK_RUNNERS_H_
