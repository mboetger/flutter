// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_PLATFORM_VIEW_ANDROID_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_PLATFORM_VIEW_ANDROID_H_

#include <jni.h>

namespace flutter {

//------------------------------------------------------------------------------
/// @brief      Entry point for registering native JNI methods on Android.
///
class PlatformViewAndroid {
 public:
  static bool Register(JNIEnv* env);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_PLATFORM_VIEW_ANDROID_H_
