// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/embedder/embedder_external_texture_vk.h"

#include <memory>
#include <utility>

#include "flutter/display_list/image/dl_image_skia.h"
#include "flutter/fml/cleanup.h"
#include "flutter/fml/logging.h"

#ifdef SHELL_ENABLE_VULKAN
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkTypes.h"
#endif

#if defined(SHELL_ENABLE_VULKAN) && defined(IMPELLER_SUPPORTS_RENDERING)
#include "impeller/core/formats.h"
#include "impeller/core/texture_descriptor.h"
#include "impeller/display_list/aiks_context.h"
#include "impeller/display_list/dl_image_impeller.h"
#include "impeller/geometry/size.h"
#include "impeller/renderer/backend/vulkan/context_vk.h"
#include "impeller/renderer/backend/vulkan/formats_vk.h"
#include "impeller/renderer/backend/vulkan/texture_source_vk.h"
#include "impeller/renderer/backend/vulkan/texture_vk.h"
#include "impeller/renderer/backend/vulkan/vk.h"
#endif

namespace flutter {

#if defined(SHELL_ENABLE_VULKAN) && defined(IMPELLER_SUPPORTS_RENDERING)
class ExternalTextureSourceVK final : public impeller::TextureSourceVK {
 public:
  ExternalTextureSourceVK(impeller::TextureDescriptor desc,
                          impeller::vk::Image image,
                          impeller::vk::UniqueImageView image_view,
                          fml::closure destruction_callback)
      : TextureSourceVK(desc),
        image_(image),
        image_view_(std::move(image_view)),
        destruction_callback_(std::move(destruction_callback)) {}

  ~ExternalTextureSourceVK() override {
    if (destruction_callback_) {
      destruction_callback_();
    }
  }

  impeller::vk::Image GetImage() const override { return image_; }

  impeller::vk::ImageView GetImageView() const override {
    return image_view_.get();
  }

  impeller::vk::ImageView GetRenderTargetView(
      uint32_t mip_level,
      uint32_t array_layer) const override {
    return image_view_.get();
  }

  bool IsSwapchainImage() const override { return false; }

 private:
  impeller::vk::Image image_;
  impeller::vk::UniqueImageView image_view_;
  fml::closure destruction_callback_;
};
#endif  // defined(SHELL_ENABLE_VULKAN) && defined(IMPELLER_SUPPORTS_RENDERING)

EmbedderExternalTextureVK::EmbedderExternalTextureVK(
    int64_t texture_identifier,
    const ExternalTextureCallback& callback)
    : Texture(texture_identifier), external_texture_callback_(callback) {
  FML_DCHECK(external_texture_callback_);
}

EmbedderExternalTextureVK::~EmbedderExternalTextureVK() = default;

// |flutter::Texture|
void EmbedderExternalTextureVK::Paint(PaintContext& context,
                                      const DlRect& bounds,
                                      bool freeze,
                                      const DlImageSampling sampling) {
  if (last_image_ == nullptr) {
    last_image_ =
        ResolveTexture(Id(),                                                 //
                       context.gr_context,                                   //
                       context.aiks_context,                                 //
                       SkISize::Make(bounds.GetWidth(), bounds.GetHeight())  //
        );
  }

  DlCanvas* canvas = context.canvas;
  const DlPaint* paint = context.paint;

  if (last_image_) {
    DlRect image_bounds = DlRect::Make(last_image_->GetBounds());
    if (bounds != image_bounds) {
      canvas->DrawImageRect(last_image_, image_bounds, bounds, sampling, paint);
    } else {
      canvas->DrawImage(last_image_, bounds.GetOrigin(), sampling, paint);
    }
  }
}

sk_sp<DlImage> EmbedderExternalTextureVK::ResolveTexture(
    int64_t texture_id,
    GrDirectContext* context,
    impeller::AiksContext* aiks_context,
    const SkISize& size) {
  if (aiks_context) {
    return ResolveTextureImpeller(texture_id, aiks_context, size);
  } else if (context) {
    return ResolveTextureSkia(texture_id, context, size);
  }
  return nullptr;
}

sk_sp<DlImage> EmbedderExternalTextureVK::ResolveTextureSkia(
    int64_t texture_id,
    GrDirectContext* context,
    const SkISize& size) {
#ifdef SHELL_ENABLE_VULKAN
  if (!context) {
    return nullptr;
  }
  std::unique_ptr<FlutterVulkanExternalTexture> texture =
      external_texture_callback_(texture_id, size.width(), size.height());

  if (!texture) {
    return nullptr;
  }

  fml::ScopedCleanupClosure scoped_cleanup([&texture]() {
    if (texture->destruction_callback) {
      texture->destruction_callback(texture->user_data);
    }
  });

  if (!texture->image) {
    FML_LOG(ERROR) << "Embedder supplied null Vulkan image handle.";
    return nullptr;
  }

  GrVkImageInfo image_info = {
      .fImage = reinterpret_cast<VkImage>(texture->image),
      .fImageTiling = VK_IMAGE_TILING_OPTIMAL,
      .fImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .fFormat = static_cast<VkFormat>(texture->format),
      .fImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
      .fSampleCount = 1,
      .fLevelCount = 1,
  };

  auto gr_backend_texture =
      GrBackendTextures::MakeVk(texture->width, texture->height, image_info);

  SkImages::TextureReleaseProc release_proc = texture->destruction_callback;
  auto image = SkImages::BorrowTextureFrom(
      context, gr_backend_texture, kTopLeft_GrSurfaceOrigin,
      kRGBA_8888_SkColorType, kPremul_SkAlphaType, nullptr, release_proc,
      texture->user_data);

  if (!image) {
    if (release_proc) {
      release_proc(texture->user_data);
    }
    FML_LOG(ERROR) << "Could not create external texture.";
    return nullptr;
  }

  scoped_cleanup.Release();
  return DlImageSkia::Make(std::move(image));
#else
  return nullptr;
#endif
}

sk_sp<DlImage> EmbedderExternalTextureVK::ResolveTextureImpeller(
    int64_t texture_id,
    impeller::AiksContext* aiks_context,
    const SkISize& size) {
#if defined(SHELL_ENABLE_VULKAN) && defined(IMPELLER_SUPPORTS_RENDERING)
  std::unique_ptr<FlutterVulkanExternalTexture> texture =
      external_texture_callback_(texture_id, size.width(), size.height());

  if (!texture) {
    return nullptr;
  }

  fml::ScopedCleanupClosure scoped_cleanup([&texture]() {
    if (texture->destruction_callback) {
      texture->destruction_callback(texture->user_data);
    }
  });

  if (!texture->image) {
    FML_LOG(ERROR) << "Embedder supplied null Vulkan image handle.";
    return nullptr;
  }

  impeller::vk::Format vk_format =
      static_cast<impeller::vk::Format>(texture->format);
  std::optional<impeller::PixelFormat> format =
      impeller::VkFormatToImpellerFormat(vk_format);
  if (!format.has_value()) {
    FML_LOG(ERROR) << "Unsupported pixel format: "
                   << impeller::vk::to_string(vk_format);
    return nullptr;
  }

  auto& context_vk = impeller::ContextVK::Cast(*aiks_context->GetContext());
  impeller::vk::Image vk_image =
      impeller::vk::Image(reinterpret_cast<VkImage>(texture->image));

  impeller::TextureDescriptor desc;
  desc.format = format.value();
  desc.size = impeller::ISize{
      static_cast<impeller::ISize::DimensionType>(texture->width),
      static_cast<impeller::ISize::DimensionType>(texture->height)};
  desc.storage_mode = impeller::StorageMode::kDevicePrivate;
  desc.mip_count = 1;
  desc.compression_type = impeller::CompressionType::kLossless;
  desc.usage = impeller::TextureUsage::kShaderRead;

  impeller::vk::ImageViewCreateInfo view_info = {};
  view_info.viewType = impeller::vk::ImageViewType::e2D;
  view_info.format = ToVKImageFormat(desc.format);
  view_info.subresourceRange.aspectMask =
      impeller::vk::ImageAspectFlagBits::eColor;
  view_info.subresourceRange.baseMipLevel = 0u;
  view_info.subresourceRange.baseArrayLayer = 0u;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.layerCount = 1;
  view_info.image = vk_image;

  auto [result, image_view] =
      context_vk.GetDevice().createImageViewUnique(view_info);
  if (result != impeller::vk::Result::eSuccess) {
    FML_LOG(ERROR)
        << "Failed to create image view for provided external texture image: "
        << impeller::vk::to_string(result);
    return nullptr;
  }

  fml::closure destruction_callback;
  if (texture->destruction_callback) {
    destruction_callback = [callback = texture->destruction_callback,
                            user_data = texture->user_data]() {
      callback(user_data);
    };
  }

  auto source = std::make_shared<ExternalTextureSourceVK>(
      desc, vk_image, std::move(image_view), std::move(destruction_callback));
  auto texture_vk = std::make_shared<impeller::TextureVK>(
      aiks_context->GetContext(), std::move(source));
  if (!texture_vk) {
    FML_LOG(ERROR) << "Could not wrap external Vulkan texture.";
    return nullptr;
  }

  scoped_cleanup.Release();
  return impeller::DlImageImpeller::Make(std::move(texture_vk));
#else
  return nullptr;
#endif
}

// |flutter::Texture|
void EmbedderExternalTextureVK::OnGrContextCreated() {}

// |flutter::Texture|
void EmbedderExternalTextureVK::OnGrContextDestroyed() {}

// |flutter::Texture|
void EmbedderExternalTextureVK::MarkNewFrameAvailable() {
  last_image_ = nullptr;
}

// |flutter::Texture|
void EmbedderExternalTextureVK::OnTextureUnregistered() {}

}  // namespace flutter
