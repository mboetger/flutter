// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/embedder/embedder_external_texture_vulkan.h"

#include <utility>

#include "flutter/display_list/image/dl_image_skia.h"
#include "flutter/fml/closure.h"
#include "flutter/fml/logging.h"
#include "flutter/shell/gpu/gpu_surface_vulkan.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkSize.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkTypes.h"

#if IMPELLER_SUPPORTS_RENDERING
#include "impeller/core/texture_descriptor.h"
#include "impeller/display_list/aiks_context.h"
#include "impeller/display_list/dl_image_impeller.h"
#include "impeller/geometry/size.h"
#include "impeller/renderer/backend/vulkan/context_vk.h"
#include "impeller/renderer/backend/vulkan/formats_vk.h"
#include "impeller/renderer/backend/vulkan/texture_source_vk.h"
#include "impeller/renderer/backend/vulkan/texture_vk.h"
#endif  // IMPELLER_SUPPORTS_RENDERING

namespace flutter {

EmbedderExternalTextureVulkan::EmbedderExternalTextureVulkan(
    int64_t texture_identifier,
    const ExternalTextureCallback& callback)
    : Texture(texture_identifier), external_texture_callback_(callback) {
  FML_DCHECK(external_texture_callback_);
}

EmbedderExternalTextureVulkan::~EmbedderExternalTextureVulkan() = default;

// |flutter::Texture|
void EmbedderExternalTextureVulkan::Paint(PaintContext& context,
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

sk_sp<DlImage> EmbedderExternalTextureVulkan::ResolveTexture(
    int64_t texture_id,
    GrDirectContext* context,
    impeller::AiksContext* aiks_context,
    const SkISize& size) {
  if (!!aiks_context) {
    return ResolveTextureImpeller(texture_id, aiks_context, size);
  } else if (context) {
    return ResolveTextureSkia(texture_id, context, size);
  }
  return nullptr;
}

sk_sp<DlImage> EmbedderExternalTextureVulkan::ResolveTextureSkia(
    int64_t texture_id,
    GrDirectContext* context,
    const SkISize& size) {
  if (!context) {
    return nullptr;
  }
  context->flushAndSubmit();
  context->resetContext(kAll_GrBackendState);
  std::unique_ptr<FlutterVulkanImage> texture =
      external_texture_callback_(texture_id, size.width(), size.height());

  if (!texture || !texture->image) {
    return nullptr;
  }

  size_t width = texture->width ? texture->width : size.width();
  size_t height = texture->height ? texture->height : size.height();

  GrVkImageInfo image_info = {
      .fImage = reinterpret_cast<VkImage>(texture->image),
      .fImageTiling = VK_IMAGE_TILING_OPTIMAL,
      .fImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .fFormat = static_cast<VkFormat>(texture->format),
      .fImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT |
                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      .fSampleCount = 1,
      .fLevelCount = 1,
  };

  auto backend_texture = GrBackendTextures::MakeVk(width, height, image_info);

  auto color_type = flutter::GPUSurfaceVulkan::ColorTypeFromFormat(
      static_cast<VkFormat>(texture->format));

  SkImages::TextureReleaseProc release_proc = nullptr;
  void* release_context = nullptr;
  if (texture->destruction_callback) {
    release_proc = [](void* user_data) {
      auto* cb = reinterpret_cast<std::pair<VoidCallback, void*>*>(user_data);
      if (cb->first) {
        cb->first(cb->second);
      }
      delete cb;
    };
    release_context = new std::pair<VoidCallback, void*>(
        texture->destruction_callback, texture->user_data);
  }

  auto image = SkImages::BorrowTextureFrom(
      context, backend_texture, kTopLeft_GrSurfaceOrigin, color_type,
      kPremul_SkAlphaType, SkColorSpace::MakeSRGB(), release_proc,
      release_context);

  if (!image) {
    FML_LOG(ERROR)
        << "Could not create Skia image from Vulkan external texture";
    if (release_proc && release_context) {
      release_proc(release_context);
    }
    return nullptr;
  }

  return DlImageSkia::Make(std::move(image));
}

#if IMPELLER_SUPPORTS_RENDERING
namespace {
class ExternalTextureSourceVK : public impeller::TextureSourceVK {
 public:
  explicit ExternalTextureSourceVK(impeller::vk::Image image,
                                   impeller::vk::UniqueImageView image_view,
                                   impeller::TextureDescriptor desc,
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

 private:
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

  impeller::vk::Image image_;
  impeller::vk::UniqueImageView image_view_;
  fml::closure destruction_callback_;
};
}  // namespace
#endif  // IMPELLER_SUPPORTS_RENDERING

sk_sp<DlImage> EmbedderExternalTextureVulkan::ResolveTextureImpeller(
    int64_t texture_id,
    impeller::AiksContext* aiks_context,
    const SkISize& size) {
#if IMPELLER_SUPPORTS_RENDERING
  std::unique_ptr<FlutterVulkanImage> texture =
      external_texture_callback_(texture_id, size.width(), size.height());

  if (!texture || !texture->image) {
    return nullptr;
  }

  fml::ScopedCleanupClosure scoped_cleanup([&texture]() {
    if (texture->destruction_callback) {
      texture->destruction_callback(texture->user_data);
    }
  });

  impeller::vk::Format vk_format =
      static_cast<impeller::vk::Format>(texture->format);
  std::optional<impeller::PixelFormat> pixel_format =
      impeller::VkFormatToImpellerFormat(vk_format);
  if (!pixel_format.has_value()) {
    FML_LOG(ERROR) << "Unsupported pixel format for Vulkan external texture: "
                   << impeller::vk::to_string(vk_format);
    return nullptr;
  }

  size_t width = texture->width ? texture->width : size.width();
  size_t height = texture->height ? texture->height : size.height();

  impeller::ContextVK& context_vk =
      impeller::ContextVK::Cast(*aiks_context->GetContext());

  impeller::vk::Image vk_image =
      impeller::vk::Image(reinterpret_cast<VkImage>(texture->image));

  impeller::TextureDescriptor desc;
  desc.format = pixel_format.value();
  desc.size = impeller::ISize(width, height);
  desc.storage_mode = impeller::StorageMode::kDevicePrivate;
  desc.mip_count = 1;
  desc.usage = impeller::TextureUsage::kShaderRead;

  impeller::vk::ImageViewCreateInfo view_info = {};
  view_info.viewType = impeller::vk::ImageViewType::e2D;
  view_info.format = impeller::ToVKImageFormat(desc.format);
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
    FML_LOG(ERROR) << "Failed to create image view for external Vulkan image: "
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

  auto texture_source = std::make_shared<ExternalTextureSourceVK>(
      vk_image, std::move(image_view), desc, std::move(destruction_callback));
  std::shared_ptr<impeller::Texture> image =
      std::make_shared<impeller::TextureVK>(aiks_context->GetContext(),
                                            texture_source);

  if (!image) {
    FML_LOG(ERROR) << "Could not create Impeller external texture";
    return nullptr;
  }

  scoped_cleanup.Release();
  return impeller::DlImageImpeller::Make(image);
#else
  return nullptr;
#endif
}

// |flutter::Texture|
void EmbedderExternalTextureVulkan::OnGrContextCreated() {}

// |flutter::Texture|
void EmbedderExternalTextureVulkan::OnGrContextDestroyed() {}

// |flutter::Texture|
void EmbedderExternalTextureVulkan::MarkNewFrameAvailable() {
  last_image_ = nullptr;
}

// |flutter::Texture|
void EmbedderExternalTextureVulkan::OnTextureUnregistered() {}

}  // namespace flutter
