// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_IMAGE_GENERATOR_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_IMAGE_GENERATOR_H_

#include <jni.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "flutter/fml/macros.h"
#include "flutter/fml/mapping.h"
#include "flutter/fml/memory/ref_ptr.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/fml/task_runner.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

namespace testing {
FML_TEST_CLASS(AndroidImageGenerator, HeaderDecodeDimensionMismatch);
}

class AndroidImageGenerator {
 private:
  explicit AndroidImageGenerator(std::vector<uint8_t> buffer);

 public:
  ~AndroidImageGenerator();

  bool GetInfo(FlutterImageGeneratorInfo* info_out);

  bool GetPixels(const FlutterImageGeneratorInfo* info,
                 void* pixels,
                 size_t row_bytes);

  void DecodeImage();

  static bool Register(JNIEnv* env);

  static bool CreateGenerator(const uint8_t* data,
                              size_t data_size,
                              const fml::RefPtr<fml::TaskRunner>& task_runner,
                              FlutterImageGenerator* generator_out);

  static void NativeImageHeaderCallback(JNIEnv* env,
                                        jclass jcaller,
                                        jlong generator_pointer,
                                        int width,
                                        int height);

 private:
  FML_FRIEND_TEST(testing::AndroidImageGenerator,
                  HeaderDecodeDimensionMismatch);

  std::vector<uint8_t> data_;
  std::unique_ptr<fml::Mapping> software_decoded_data_;

  int width_ = -1;
  int height_ = -1;

  /// Blocks until the header of the image has been decoded and the image
  /// dimensions have been determined.
  fml::ManualResetWaitableEvent header_decoded_latch_;

  /// Blocks until the image has been fully decoded.
  fml::ManualResetWaitableEvent fully_decoded_latch_;

  static std::shared_ptr<AndroidImageGenerator> MakeForTesting(
      int width,
      int height,
      std::unique_ptr<fml::Mapping> decoded_data);

  void DoDecodeImage();

  bool IsValidImageData();

  FML_DISALLOW_COPY_ASSIGN_AND_MOVE(AndroidImageGenerator);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_IMAGE_GENERATOR_H_
