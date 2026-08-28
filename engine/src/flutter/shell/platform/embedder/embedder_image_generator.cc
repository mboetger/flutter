// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/embedder/embedder_image_generator.h"

#include <cstring>
#include <limits>
#include <utility>

#include "flutter/fml/logging.h"

namespace flutter {

static SkColorType ConvertColorType(FlutterImagePixelFormat format) {
  switch (format) {
    case kFlutterImagePixelFormatRGBA8888:
      return kRGBA_8888_SkColorType;
    case kFlutterImagePixelFormatBGRA8888:
      return kBGRA_8888_SkColorType;
    case kFlutterImagePixelFormatRGBA16F:
      return kRGBA_F16_SkColorType;
    case kFlutterImagePixelFormatUnknown:
    default:
      return kUnknown_SkColorType;
  }
}

static FlutterImagePixelFormat ConvertColorType(SkColorType color_type) {
  switch (color_type) {
    case kRGBA_8888_SkColorType:
      return kFlutterImagePixelFormatRGBA8888;
    case kBGRA_8888_SkColorType:
      return kFlutterImagePixelFormatBGRA8888;
    case kRGBA_F16_SkColorType:
      return kFlutterImagePixelFormatRGBA16F;
    default:
      return kFlutterImagePixelFormatUnknown;
  }
}

static SkAlphaType ConvertAlphaType(FlutterImageAlphaType alpha_type) {
  switch (alpha_type) {
    case kFlutterImageAlphaTypeOpaque:
      return kOpaque_SkAlphaType;
    case kFlutterImageAlphaTypePremul:
      return kPremul_SkAlphaType;
    case kFlutterImageAlphaTypeUnpremul:
      return kUnpremul_SkAlphaType;
    case kFlutterImageAlphaTypeUnknown:
    default:
      return kUnknown_SkAlphaType;
  }
}

static FlutterImageAlphaType ConvertAlphaType(SkAlphaType alpha_type) {
  switch (alpha_type) {
    case kOpaque_SkAlphaType:
      return kFlutterImageAlphaTypeOpaque;
    case kPremul_SkAlphaType:
      return kFlutterImageAlphaTypePremul;
    case kUnpremul_SkAlphaType:
      return kFlutterImageAlphaTypeUnpremul;
    default:
      return kFlutterImageAlphaTypeUnknown;
  }
}

EmbedderImageGenerator::EmbedderImageGenerator(
    FlutterImageGeneratorCallbacks callbacks)
    : callbacks_(callbacks), image_info_(SkImageInfo::MakeUnknown(-1, -1)) {}

EmbedderImageGenerator::~EmbedderImageGenerator() {
  if (callbacks_.destroy != nullptr) {
    callbacks_.destroy(callbacks_.user_data);
  }
}

void EmbedderImageGenerator::EnsureInfo() {
  if (info_fetched_) {
    return;
  }
  info_fetched_ = true;
  flutter_info_.struct_size = sizeof(FlutterImageInfo);
  if (callbacks_.get_info != nullptr &&
      callbacks_.get_info(callbacks_.user_data, &flutter_info_)) {
    image_info_ =
        SkImageInfo::Make(static_cast<int>(flutter_info_.width),
                          static_cast<int>(flutter_info_.height),
                          ConvertColorType(flutter_info_.pixel_format),
                          ConvertAlphaType(flutter_info_.alpha_type));
  } else {
    image_info_ = SkImageInfo::MakeUnknown(-1, -1);
  }
}

const SkImageInfo& EmbedderImageGenerator::GetInfo() {
  EnsureInfo();
  return image_info_;
}

unsigned int EmbedderImageGenerator::GetFrameCount() const {
  const_cast<EmbedderImageGenerator*>(this)->EnsureInfo();
  return flutter_info_.frame_count > 0
             ? static_cast<unsigned int>(flutter_info_.frame_count)
             : 1;
}

unsigned int EmbedderImageGenerator::GetPlayCount() const {
  const_cast<EmbedderImageGenerator*>(this)->EnsureInfo();
  if (flutter_info_.play_count == 0) {
    return 1;
  }
  if (flutter_info_.play_count == std::numeric_limits<size_t>::max() ||
      flutter_info_.play_count == std::numeric_limits<uint32_t>::max()) {
    return kInfinitePlayCount;
  }
  return static_cast<unsigned int>(flutter_info_.play_count);
}

const ImageGenerator::FrameInfo EmbedderImageGenerator::GetFrameInfo(
    unsigned int frame_index) {
  EnsureInfo();
  ImageGenerator::FrameInfo result = {
      .required_frame = std::nullopt,
      .duration = 0,
      .disposal_method = SkCodecAnimation::DisposalMethod::kKeep,
      .disposal_rect = std::nullopt,
      .blend_mode = SkCodecAnimation::Blend::kSrcOver,
  };

  if (callbacks_.get_frame_info != nullptr) {
    FlutterImageFrameInfo frame_info = {};
    frame_info.struct_size = sizeof(FlutterImageFrameInfo);
    if (callbacks_.get_frame_info(callbacks_.user_data, frame_index,
                                  &frame_info)) {
      result.duration = static_cast<unsigned int>(frame_info.duration_millis);
      switch (frame_info.disposal_method) {
        case kFlutterImageDisposalMethodKeep:
          result.disposal_method = SkCodecAnimation::DisposalMethod::kKeep;
          break;
        case kFlutterImageDisposalMethodBackground:
          result.disposal_method =
              SkCodecAnimation::DisposalMethod::kRestoreBGColor;
          break;
        case kFlutterImageDisposalMethodRestorePrevious:
          result.disposal_method =
              SkCodecAnimation::DisposalMethod::kRestorePrevious;
          break;
      }
      switch (frame_info.blend_mode) {
        case kFlutterImageBlendModeSource:
          result.blend_mode = SkCodecAnimation::Blend::kSrc;
          break;
        case kFlutterImageBlendModeSourceOver:
          result.blend_mode = SkCodecAnimation::Blend::kSrcOver;
          break;
      }
    }
  }
  return result;
}

SkISize EmbedderImageGenerator::GetScaledDimensions(float desired_scale) {
  EnsureInfo();
  if (callbacks_.get_scaled_dimensions != nullptr) {
    size_t width = 0;
    size_t height = 0;
    if (callbacks_.get_scaled_dimensions(callbacks_.user_data, desired_scale,
                                         &width, &height) &&
        width > 0 && height > 0) {
      return SkISize::Make(static_cast<int>(width), static_cast<int>(height));
    }
  }
  return image_info_.dimensions();
}

bool EmbedderImageGenerator::GetPixels(
    const SkImageInfo& info,
    void* pixels,
    size_t row_bytes,
    unsigned int frame_index,
    std::optional<unsigned int> prior_frame) {
  if (callbacks_.get_pixels == nullptr || pixels == nullptr) {
    return false;
  }
  FlutterImageInfo desired_info = {};
  desired_info.struct_size = sizeof(FlutterImageInfo);
  desired_info.width = static_cast<size_t>(info.width());
  desired_info.height = static_cast<size_t>(info.height());
  desired_info.pixel_format = ConvertColorType(info.colorType());
  desired_info.alpha_type = ConvertAlphaType(info.alphaType());
  desired_info.frame_count = GetFrameCount();
  desired_info.play_count = GetPlayCount();

  return callbacks_.get_pixels(callbacks_.user_data, &desired_info, pixels,
                               row_bytes, frame_index);
}

}  // namespace flutter
