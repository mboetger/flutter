// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/embedder/embedder_image_generator.h"

#include <utility>

namespace flutter {

std::shared_ptr<EmbedderImageGenerator> EmbedderImageGenerator::Make(
    const FlutterImageDecoder& decoder,
    sk_sp<SkData> data) {
  if (!data || data->isEmpty()) {
    return nullptr;
  }
  if (!decoder.create_generator) {
    return nullptr;
  }
  FlutterImageGeneratorInstance instance = nullptr;
  if (!decoder.create_generator(static_cast<const uint8_t*>(data->data()),
                                data->size(), &instance, decoder.user_data)) {
    return nullptr;
  }
  if (!instance) {
    return nullptr;
  }
  return std::make_shared<EmbedderImageGenerator>(instance, decoder.generator,
                                                  std::move(data));
}

EmbedderImageGenerator::EmbedderImageGenerator(
    FlutterImageGeneratorInstance instance,
    const FlutterImageGenerator& generator,
    sk_sp<SkData> data)
    : instance_(instance),
      generator_(generator),
      data_(std::move(data)),
      image_info_(SkImageInfo::MakeUnknown(-1, -1)) {}

EmbedderImageGenerator::~EmbedderImageGenerator() {
  if (instance_ && generator_.destroy) {
    generator_.destroy(instance_, generator_.user_data);
    instance_ = nullptr;
  }
}

void EmbedderImageGenerator::EnsureInfoFetched() {
  std::lock_guard<std::mutex> lock(info_mutex_);
  if (info_fetched_) {
    return;
  }
  info_fetched_ = true;
  if (generator_.get_image_info) {
    embedder_image_info_.struct_size = sizeof(FlutterImageInfo);
    if (generator_.get_image_info(instance_, &embedder_image_info_,
                                  generator_.user_data)) {
      image_info_ = SkImageInfo::Make(
          embedder_image_info_.width, embedder_image_info_.height,
          kRGBA_8888_SkColorType, kPremul_SkAlphaType);
      return;
    }
  }
  image_info_ = SkImageInfo::MakeUnknown(-1, -1);
}

const SkImageInfo& EmbedderImageGenerator::GetInfo() {
  EnsureInfoFetched();
  return image_info_;
}

unsigned int EmbedderImageGenerator::GetFrameCount() const {
  const_cast<EmbedderImageGenerator*>(this)->EnsureInfoFetched();
  return embedder_image_info_.frame_count > 0 ? embedder_image_info_.frame_count
                                              : 1;
}

unsigned int EmbedderImageGenerator::GetPlayCount() const {
  const_cast<EmbedderImageGenerator*>(this)->EnsureInfoFetched();
  if (embedder_image_info_.repetition_count == 0) {
    return (embedder_image_info_.frame_count > 1) ? kInfinitePlayCount : 1;
  }
  return embedder_image_info_.repetition_count;
}

const ImageGenerator::FrameInfo EmbedderImageGenerator::GetFrameInfo(
    unsigned int frame_index) {
  if (generator_.get_frame_info) {
    FlutterImageFrameInfo frame_info = {};
    frame_info.struct_size = sizeof(FlutterImageFrameInfo);
    if (generator_.get_frame_info(instance_, frame_index, &frame_info,
                                  generator_.user_data)) {
      SkCodecAnimation::DisposalMethod disposal =
          SkCodecAnimation::DisposalMethod::kKeep;
      switch (frame_info.disposal_method) {
        case kFlutterImageDisposalMethodRestoreBackground:
          disposal = SkCodecAnimation::DisposalMethod::kRestoreBGColor;
          break;
        case kFlutterImageDisposalMethodRestorePrevious:
          disposal = SkCodecAnimation::DisposalMethod::kRestorePrevious;
          break;
        case kFlutterImageDisposalMethodKeep:
        default:
          disposal = SkCodecAnimation::DisposalMethod::kKeep;
          break;
      }
      SkCodecAnimation::Blend blend =
          (frame_info.blend_mode == kFlutterImageBlendModeSrc)
              ? SkCodecAnimation::Blend::kSrc
              : SkCodecAnimation::Blend::kSrcOver;
      return ImageGenerator::FrameInfo{
          .required_frame = std::nullopt,
          .duration = frame_info.duration_millis,
          .disposal_method = disposal,
          .disposal_rect = std::nullopt,
          .blend_mode = blend,
      };
    }
  }
  return ImageGenerator::FrameInfo{
      .required_frame = std::nullopt,
      .duration = 0,
      .disposal_method = SkCodecAnimation::DisposalMethod::kKeep,
      .disposal_rect = std::nullopt,
      .blend_mode = SkCodecAnimation::Blend::kSrcOver,
  };
}

SkISize EmbedderImageGenerator::GetScaledDimensions(float desired_scale) {
  if (generator_.get_scaled_dimensions) {
    uint32_t sw = 0;
    uint32_t sh = 0;
    if (generator_.get_scaled_dimensions(instance_, desired_scale, &sw, &sh,
                                         generator_.user_data)) {
      return SkISize::Make(sw, sh);
    }
  }
  return GetInfo().dimensions();
}

bool EmbedderImageGenerator::GetPixels(
    const SkImageInfo& info,
    void* pixels,
    size_t row_bytes,
    unsigned int frame_index,
    std::optional<unsigned int> prior_frame) {
  if (!generator_.get_pixels || !pixels) {
    return false;
  }
  EnsureInfoFetched();
  FlutterImageInfo requested_info = {};
  requested_info.struct_size = sizeof(FlutterImageInfo);
  requested_info.width = info.width();
  requested_info.height = info.height();
  requested_info.frame_count = embedder_image_info_.frame_count;
  requested_info.repetition_count = embedder_image_info_.repetition_count;

  return generator_.get_pixels(instance_, &requested_info, pixels, row_bytes,
                               frame_index, generator_.user_data);
}

}  // namespace flutter
