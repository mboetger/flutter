// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_VULKAN_UTILS_H_
#define FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_VULKAN_UTILS_H_

#include <optional>

#include "flutter/fml/macros.h"
#include "impeller/renderer/backend/vulkan/context_vk.h"
#include "impeller/renderer/backend/vulkan/texture_source_vk.h"

namespace flutter {

class WrappedTextureSourceVK final : public impeller::TextureSourceVK {
 public:
  WrappedTextureSourceVK(const impeller::ContextVK& context,
                         const impeller::TextureDescriptor& desc,
                         impeller::vk::Image image);

  ~WrappedTextureSourceVK() override;

  bool IsValid() const { return is_valid_; }

  impeller::vk::Image GetImage() const override { return image_; }

  impeller::vk::ImageView GetImageView() const override {
    return image_view_.get();
  }

  impeller::vk::ImageView GetRenderTargetView() const override {
    return image_view_.get();
  }

  bool IsSwapchainImage() const override { return false; }

 private:
  impeller::vk::Image image_;
  impeller::vk::UniqueImageView image_view_;
  bool is_valid_ = false;

  FML_DISALLOW_COPY_AND_ASSIGN(WrappedTextureSourceVK);
};

std::optional<impeller::PixelFormat> VulkanFormatToImpellerPixelFormat(
    uint32_t format);

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_VULKAN_UTILS_H_
