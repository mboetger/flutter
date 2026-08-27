// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/platform_view_android_adapter.h"

#include <utility>

namespace flutter {

PlatformViewAndroidAdapter::PlatformViewAndroidAdapter(
    PlatformView::Delegate& delegate,
    const flutter::TaskRunners& task_runners,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
    AndroidRenderingAPI rendering_api)
    : PlatformView(delegate, task_runners),
      platform_view_android_(
          std::make_unique<PlatformViewAndroid>(*this,
                                                task_runners,
                                                jni_facade,
                                                rendering_api)) {}

PlatformViewAndroidAdapter::PlatformViewAndroidAdapter(
    PlatformView::Delegate& delegate,
    const flutter::TaskRunners& task_runners,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
    const std::shared_ptr<flutter::AndroidContext>& android_context)
    : PlatformView(delegate, task_runners),
      platform_view_android_(
          std::make_unique<PlatformViewAndroid>(*this,
                                                task_runners,
                                                jni_facade,
                                                android_context)) {}

PlatformViewAndroidAdapter::~PlatformViewAndroidAdapter() = default;

}  // namespace flutter
