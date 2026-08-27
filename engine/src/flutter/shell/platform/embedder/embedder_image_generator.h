// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_IMAGE_GENERATOR_H_
#define FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_IMAGE_GENERATOR_H_

#include <memory>
#include <mutex>
#include <optional>

#include "flutter/fml/macros.h"
#include "flutter/lib/ui/painting/image_generator.h"
#include "flutter/shell/platform/embedder/embedder.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace flutter {

class EmbedderImageGenerator : public ImageGenerator {
 public:
  static std::shared_ptr<EmbedderImageGenerator> Make(
      const FlutterImageDecoder& decoder,
      sk_sp<SkData> data);

  EmbedderImageGenerator(FlutterImageGeneratorInstance instance,
                         const FlutterImageGenerator& generator,
                         sk_sp<SkData> data);

  ~EmbedderImageGenerator() override;

  // |ImageGenerator|
  const SkImageInfo& GetInfo() override;

  // |ImageGenerator|
  unsigned int GetFrameCount() const override;

  // |ImageGenerator|
  unsigned int GetPlayCount() const override;

  // |ImageGenerator|
  const ImageGenerator::FrameInfo GetFrameInfo(
      unsigned int frame_index) override;

  // |ImageGenerator|
  SkISize GetScaledDimensions(float desired_scale) override;

  // |ImageGenerator|
  bool GetPixels(
      const SkImageInfo& info,
      void* pixels,
      size_t row_bytes,
      unsigned int frame_index = 0,
      std::optional<unsigned int> prior_frame = std::nullopt) override;

 private:
  FlutterImageGeneratorInstance instance_;
  FlutterImageGenerator generator_;
  sk_sp<SkData> data_;
  SkImageInfo image_info_;
  FlutterImageInfo embedder_image_info_ = {};
  bool info_fetched_ = false;
  mutable std::mutex info_mutex_;

  void EnsureInfoFetched();

  FML_DISALLOW_COPY_AND_ASSIGN(EmbedderImageGenerator);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_IMAGE_GENERATOR_H_
