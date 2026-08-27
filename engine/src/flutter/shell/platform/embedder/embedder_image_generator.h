// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_IMAGE_GENERATOR_H_
#define FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_IMAGE_GENERATOR_H_

#include <memory>
#include <optional>

#include "flutter/fml/macros.h"
#include "flutter/lib/ui/painting/image_generator.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

//------------------------------------------------------------------------------
/// @brief An ImageGenerator implementation that wraps
///        FlutterImageGeneratorCallbacks provided through the C Embedder API.
///
class EmbedderImageGenerator final : public ImageGenerator {
 public:
  explicit EmbedderImageGenerator(FlutterImageGeneratorCallbacks callbacks);

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
  FlutterImageGeneratorCallbacks callbacks_;
  bool info_fetched_ = false;
  FlutterImageInfo flutter_info_ = {};
  SkImageInfo image_info_;

  void EnsureInfo();

  FML_DISALLOW_COPY_AND_ASSIGN(EmbedderImageGenerator);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_IMAGE_GENERATOR_H_
