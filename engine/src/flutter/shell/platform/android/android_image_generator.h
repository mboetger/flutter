// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_IMAGE_GENERATOR_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_IMAGE_GENERATOR_H_

#include <jni.h>

#include <optional>

#include "flutter/fml/memory/ref_ptr.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/fml/task_runner.h"
#include "third_party/skia/include/codec/SkCodecAnimation.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSize.h"

namespace flutter {

namespace testing {
FML_TEST_CLASS(AndroidImageGenerator, HeaderDecodeDimensionMismatch);
}

class AndroidImageGenerator {
 private:
  explicit AndroidImageGenerator(sk_sp<SkData> buffer);

 public:
  struct FrameInfo {
    std::optional<unsigned int> required_frame;
    unsigned int duration;
    SkCodecAnimation::DisposalMethod disposal_method;
    std::optional<SkIRect> disposal_rect;
    SkCodecAnimation::Blend blend_mode;
  };

  ~AndroidImageGenerator();

  const SkImageInfo& GetInfo();

  unsigned int GetFrameCount() const;

  unsigned int GetPlayCount() const;

  FrameInfo GetFrameInfo(unsigned int frame_index);

  SkISize GetScaledDimensions(float desired_scale);

  bool GetPixels(const SkImageInfo& info,
                 void* pixels,
                 size_t row_bytes,
                 unsigned int frame_index,
                 std::optional<unsigned int> prior_frame);

  void DecodeImage();

  static bool Register(JNIEnv* env);

  static std::shared_ptr<AndroidImageGenerator> MakeFromData(
      sk_sp<SkData> data,
      const fml::RefPtr<fml::TaskRunner>& task_runner);

  static void NativeImageHeaderCallback(JNIEnv* env,
                                        jclass jcaller,
                                        jlong generator_pointer,
                                        int width,
                                        int height);

 private:
  FML_FRIEND_TEST(testing::AndroidImageGenerator,
                  HeaderDecodeDimensionMismatch);

  sk_sp<SkData> data_;
  sk_sp<SkData> software_decoded_data_;

  SkImageInfo image_info_;

  /// Blocks until the header of the image has been decoded and the image
  /// dimensions have been determined.
  fml::ManualResetWaitableEvent header_decoded_latch_;

  /// Blocks until the image has been fully decoded.
  fml::ManualResetWaitableEvent fully_decoded_latch_;

  static std::shared_ptr<AndroidImageGenerator> MakeForTesting(
      const SkImageInfo& header_info,
      sk_sp<SkData> decoded_data);

  void DoDecodeImage();

  bool IsValidImageData();

  FML_DISALLOW_COPY_ASSIGN_AND_MOVE(AndroidImageGenerator);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_IMAGE_GENERATOR_H_
