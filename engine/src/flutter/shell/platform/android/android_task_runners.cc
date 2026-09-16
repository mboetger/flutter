// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include "flutter/shell/platform/android/android_task_runners.h"

#include <pthread.h>
#include <sys/resource.h>
#include <sys/time.h>

#include <utility>

#include "flutter/fml/cpu_affinity.h"
#include "flutter/fml/logging.h"
#include "flutter/fml/message_loop.h"
#include "flutter/shell/common/thread_host.h"

namespace flutter {

void AndroidPlatformThreadPrioritySetter(FlutterThreadPriority priority) {
  switch (priority) {
    case FlutterThreadPriority::kBackground: {
      fml::RequestAffinity(fml::CpuAffinity::kEfficiency);
      if (::setpriority(PRIO_PROCESS, 0, 10) != 0) {
        FML_LOG(ERROR) << "Failed to set IO task runner priority";
      }
      break;
    }
    case FlutterThreadPriority::kDisplay: {
      fml::RequestAffinity(fml::CpuAffinity::kNotEfficiency);
      if (::setpriority(PRIO_PROCESS, 0, -1) != 0) {
        FML_LOG(ERROR) << "Failed to set UI task runner priority";
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
        FML_LOG(ERROR) << "Failed to set priority";
      }
      break;
  }
}

void AndroidPlatformThreadConfigSetter(
    const fml::Thread::ThreadConfig& config) {
  // set thread name
  fml::Thread::SetCurrentThreadName(config);
  // set thread priority
  FlutterThreadPriority priority = FlutterThreadPriority::kNormal;
  switch (config.priority) {
    case fml::Thread::ThreadPriority::kBackground:
      priority = FlutterThreadPriority::kBackground;
      break;
    case fml::Thread::ThreadPriority::kDisplay:
      priority = FlutterThreadPriority::kDisplay;
      break;
    case fml::Thread::ThreadPriority::kRaster:
      priority = FlutterThreadPriority::kRaster;
      break;
    default:
      priority = FlutterThreadPriority::kNormal;
      break;
  }
  AndroidPlatformThreadPrioritySetter(priority);
}

std::unique_ptr<AndroidTaskRunners> AndroidTaskRunners::Create(
    const std::string& thread_label,
    const Settings& settings) {
  auto mask = ThreadHost::Type::kRaster | ThreadHost::Type::kIo;
  if (settings.merged_platform_ui_thread !=
      Settings::MergedPlatformUIThread::kEnabled) {
    mask |= ThreadHost::Type::kUi;
  }

  flutter::ThreadHost::ThreadHostConfig host_config(
      thread_label, mask, AndroidPlatformThreadConfigSetter);
  host_config.ui_config = fml::Thread::ThreadConfig(
      flutter::ThreadHost::ThreadHostConfig::MakeThreadName(
          flutter::ThreadHost::Type::kUi, thread_label),
      fml::Thread::ThreadPriority::kDisplay);
  host_config.raster_config = fml::Thread::ThreadConfig(
      flutter::ThreadHost::ThreadHostConfig::MakeThreadName(
          flutter::ThreadHost::Type::kRaster, thread_label),
      fml::Thread::ThreadPriority::kRaster);
  host_config.io_config = fml::Thread::ThreadConfig(
      flutter::ThreadHost::ThreadHostConfig::MakeThreadName(
          flutter::ThreadHost::Type::kIo, thread_label),
      fml::Thread::ThreadPriority::kNormal);

  auto thread_host = std::make_shared<ThreadHost>(host_config);
  return std::make_unique<AndroidTaskRunners>(thread_label, settings,
                                              std::move(thread_host));
}

flutter::TaskRunners AndroidTaskRunners::MakeTaskRunners(
    const std::string& thread_label,
    const Settings& settings,
    const ThreadHost* thread_host) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();
  fml::RefPtr<fml::TaskRunner> platform_runner =
      fml::MessageLoop::GetCurrent().GetTaskRunner();
  fml::RefPtr<fml::TaskRunner> raster_runner;
  fml::RefPtr<fml::TaskRunner> ui_runner;
  fml::RefPtr<fml::TaskRunner> io_runner;

  if (thread_host) {
    if (thread_host->raster_thread) {
      raster_runner = thread_host->raster_thread->GetTaskRunner();
    }
    if (settings.merged_platform_ui_thread ==
        Settings::MergedPlatformUIThread::kEnabled) {
      ui_runner = platform_runner;
    } else if (thread_host->ui_thread) {
      ui_runner = thread_host->ui_thread->GetTaskRunner();
    }
    if (thread_host->io_thread) {
      io_runner = thread_host->io_thread->GetTaskRunner();
    }
  }

  return flutter::TaskRunners(thread_label, platform_runner, raster_runner,
                              ui_runner, io_runner);
}

AndroidTaskRunners::AndroidTaskRunners(const std::string& thread_label,
                                       const Settings& settings,
                                       std::shared_ptr<ThreadHost> thread_host)
    : thread_host_(std::move(thread_host)),
      task_runners_(
          MakeTaskRunners(thread_label, settings, thread_host_.get())) {
  InitializeCustomTaskRunners();
}

AndroidTaskRunners::~AndroidTaskRunners() = default;

void AndroidTaskRunners::InitializeCustomTaskRunners() {
  auto runs_task_on_current_thread_cb = [](void* user_data) -> bool {
    if (!user_data) {
      return false;
    }
    return static_cast<fml::TaskRunner*>(user_data)->RunsTasksOnCurrentThread();
  };

  auto post_task_cb = [](FlutterTask task, uint64_t target_time_nanos,
                         void* user_data) {
    if (!user_data) {
      return;
    }
    auto* runner = static_cast<fml::TaskRunner*>(user_data);
    auto target_time = fml::TimePoint::FromEpochDelta(
        fml::TimeDelta::FromNanoseconds(target_time_nanos));
    runner->PostTaskForTime([task]() { (void)task; }, target_time);
  };

  platform_description_.struct_size = sizeof(FlutterTaskRunnerDescription);
  platform_description_.user_data = task_runners_.GetPlatformTaskRunner().get();
  platform_description_.runs_task_on_current_thread_callback =
      runs_task_on_current_thread_cb;
  platform_description_.post_task_callback = post_task_cb;
  platform_description_.identifier = 1;
  platform_description_.priority = FlutterThreadPriority::kNormal;
  platform_description_.thread_priority_setter =
      &AndroidPlatformThreadPrioritySetter;
  platform_description_.thread_priority_setter_with_user_data = nullptr;

  raster_description_.struct_size = sizeof(FlutterTaskRunnerDescription);
  raster_description_.user_data = task_runners_.GetRasterTaskRunner().get();
  raster_description_.runs_task_on_current_thread_callback =
      runs_task_on_current_thread_cb;
  raster_description_.post_task_callback = post_task_cb;
  raster_description_.identifier = 2;
  raster_description_.priority = FlutterThreadPriority::kRaster;
  raster_description_.thread_priority_setter =
      &AndroidPlatformThreadPrioritySetter;
  raster_description_.thread_priority_setter_with_user_data = nullptr;

  ui_description_.struct_size = sizeof(FlutterTaskRunnerDescription);
  ui_description_.user_data = task_runners_.GetUITaskRunner().get();
  ui_description_.runs_task_on_current_thread_callback =
      runs_task_on_current_thread_cb;
  ui_description_.post_task_callback = post_task_cb;
  ui_description_.identifier =
      (task_runners_.GetUITaskRunner() == task_runners_.GetPlatformTaskRunner())
          ? 1
          : 3;
  ui_description_.priority = FlutterThreadPriority::kDisplay;
  ui_description_.thread_priority_setter = &AndroidPlatformThreadPrioritySetter;
  ui_description_.thread_priority_setter_with_user_data = nullptr;

  io_description_.struct_size = sizeof(FlutterTaskRunnerDescription);
  io_description_.user_data = task_runners_.GetIOTaskRunner().get();
  io_description_.runs_task_on_current_thread_callback =
      runs_task_on_current_thread_cb;
  io_description_.post_task_callback = post_task_cb;
  io_description_.identifier = 4;
  io_description_.priority = FlutterThreadPriority::kNormal;
  io_description_.thread_priority_setter = &AndroidPlatformThreadPrioritySetter;
  io_description_.thread_priority_setter_with_user_data = nullptr;

  custom_task_runners_.struct_size = sizeof(FlutterCustomTaskRunners);
  custom_task_runners_.platform_task_runner = &platform_description_;
  custom_task_runners_.render_task_runner = &raster_description_;
  custom_task_runners_.ui_task_runner = &ui_description_;
  custom_task_runners_.io_task_runner = &io_description_;
  custom_task_runners_.thread_priority_setter =
      &AndroidPlatformThreadPrioritySetter;
  custom_task_runners_.thread_priority_setter_with_user_data = nullptr;
  custom_task_runners_.user_data = nullptr;
  custom_task_runners_.io_thread_priority = FlutterThreadPriority::kNormal;
}

}  // namespace flutter
