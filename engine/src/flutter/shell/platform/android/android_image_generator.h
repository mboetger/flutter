// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_IMAGE_GENERATOR_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_IMAGE_GENERATOR_H_

#include <jni.h>
#include <memory>
#include <vector>

#include "flutter/fml/macros.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/fml/task_runner.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

class AndroidImageGenerator {
 public:
  explicit AndroidImageGenerator(std::vector<uint8_t> data);
  ~AndroidImageGenerator();

  bool GetImageInfo(FlutterImageInfo* info_out);
  bool GetPixels(const FlutterImageInfo* info, void* pixels, size_t row_bytes);

  void DecodeImage();

  static bool Register(JNIEnv* env);

  static void NativeImageHeaderCallback(JNIEnv* env,
                                        jclass jcaller,
                                        jlong generator_pointer,
                                        int width,
                                        int height);

 private:
  std::vector<uint8_t> data_;
  std::vector<uint8_t> software_decoded_data_;
  int width_ = -1;
  int height_ = -1;

  fml::ManualResetWaitableEvent header_decoded_latch_;
  fml::ManualResetWaitableEvent fully_decoded_latch_;

  void DoDecodeImage();

  FML_DISALLOW_COPY_ASSIGN_AND_MOVE(AndroidImageGenerator);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_IMAGE_GENERATOR_H_
