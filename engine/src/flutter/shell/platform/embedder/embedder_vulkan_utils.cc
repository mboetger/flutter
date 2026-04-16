// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/embedder/embedder_vulkan_utils.h"

#include "flutter/fml/logging.h"
#include "impeller/renderer/backend/vulkan/formats_vk.h"

namespace flutter {

WrappedTextureSourceVK::WrappedTextureSourceVK(
    const impeller::ContextVK& context,
    const impeller::TextureDescriptor& desc,
    impeller::vk::Image image)
    : TextureSourceVK(desc), image_(image) {
  impeller::vk::ImageViewCreateInfo view_info = {};
  view_info.image = image_;
  view_info.viewType = impeller::vk::ImageViewType::e2D;
  view_info.format = impeller::ToVKImageFormat(desc.format);
  view_info.subresourceRange.aspectMask =
      impeller::ToVKImageAspectFlags(desc.format);
  view_info.subresourceRange.levelCount = 1u;
  view_info.subresourceRange.layerCount = 1u;

  auto [result, image_view] =
      context.GetDevice().createImageViewUnique(view_info);
  if (result != impeller::vk::Result::eSuccess) {
    FML_LOG(ERROR) << "Unable to create an image view for wrapped allocation: "
                   << impeller::vk::to_string(result);
    return;
  }
  image_view_ = std::move(image_view);
  is_valid_ = true;
}

WrappedTextureSourceVK::~WrappedTextureSourceVK() = default;

std::optional<impeller::PixelFormat> VulkanFormatToImpellerPixelFormat(
    uint32_t format) {
  impeller::vk::Format vk_format = static_cast<impeller::vk::Format>(format);
  switch (vk_format) {
    case impeller::vk::Format::eR8G8B8A8Unorm:
      return impeller::PixelFormat::kR8G8B8A8UNormInt;
    case impeller::vk::Format::eB8G8R8A8Unorm:
      return impeller::PixelFormat::kB8G8R8A8UNormInt;
    default:
      FML_LOG(ERROR) << "Cannot convert Vulkan format " << format
                     << " to impeller::PixelFormat.";
      return std::nullopt;
  }
}

}  // namespace flutter
