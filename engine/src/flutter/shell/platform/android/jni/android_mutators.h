// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_JNI_ANDROID_MUTATORS_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_JNI_ANDROID_MUTATORS_H_

#include <vector>
#include "flutter/flow/embedded_views.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"

namespace flutter {

std::vector<AndroidMutator> ToAndroidMutators(const MutatorsStack& mutators_stack);

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_JNI_ANDROID_MUTATORS_H_
