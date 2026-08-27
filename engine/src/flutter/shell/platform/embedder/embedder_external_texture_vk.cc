// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/embedder/embedder_external_texture_vk.h"

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

#if defined(SHELL_ENABLE_VULKAN) && defined(IMPELLER_SUPPORTS_RENDERING)
#include "impeller/core/texture_descriptor.h"
#include "impeller/display_list/aiks_context.h"
#include "impeller/display_list/dl_image_impeller.h"
#include "impeller/geometry/size.h"
#include "impeller/renderer/backend/vulkan/context_vk.h"
#include "impeller/renderer/backend/vulkan/formats_vk.h"
#include "impeller/renderer/backend/vulkan/texture_source_vk.h"
#include "impeller/renderer/backend/vulkan/texture_vk.h"
#endif

namespace flutter {

#if defined(SHELL_ENABLE_VULKAN) && defined(IMPELLER_SUPPORTS_RENDERING)
namespace {

class EmbedderExternalTextureSourceVK : public impeller::TextureSourceVK {
 public:
  explicit EmbedderExternalTextureSourceVK(
      impeller::vk::Image image,
      impeller::vk::UniqueImageView image_view,
      impeller::TextureDescriptor desc,
      fml::closure destruction_callback)
      : TextureSourceVK(desc),
        image_(image),
        image_view_(std::move(image_view)),
        destruction_callback_(std::move(destruction_callback)) {}

  ~EmbedderExternalTextureSourceVK() override {
    // Explicitly reset the ImageView before invoking the embedder's destruction
    // callback so the VkImageView is destroyed before the underlying VkImage
    // can be freed by the embedder.
    image_view_.reset();
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
    // External textures are sampled from, but return the 2D view if requested.
    return image_view_.get();
  }

  bool IsSwapchainImage() const override { return false; }

  impeller::vk::Image image_;
  impeller::vk::UniqueImageView image_view_;
  fml::closure destruction_callback_;
};

}  // namespace
#endif

EmbedderExternalTextureVK::EmbedderExternalTextureVK(
    int64_t texture_identifier,
    ExternalTextureCallback callback)
    : Texture(texture_identifier),
      external_texture_callback_(std::move(callback)) {
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
  if (aiks_context != nullptr) {
    return ResolveTextureImpeller(texture_id, aiks_context, size);
  } else if (context != nullptr) {
    return ResolveTextureSkia(texture_id, context, size);
  }
  return nullptr;
}

sk_sp<DlImage> EmbedderExternalTextureVK::ResolveTextureSkia(
    int64_t texture_id,
    GrDirectContext* context,
    const SkISize& size) {
  if (!context) {
    return nullptr;
  }
  context->flushAndSubmit();
  context->resetContext(kAll_GrBackendState);
  std::unique_ptr<FlutterVulkanImage> image =
      external_texture_callback_(texture_id, size.width(), size.height());

  if (!image) {
    return nullptr;
  }

  // Call the destruction callback if an error occurs during wrapping.
  fml::ScopedCleanupClosure scoped_cleanup([&image]() {
    if (image->destruction_callback) {
      image->destruction_callback(image->user_data);
    }
  });

  if (image->struct_size < sizeof(FlutterVulkanImage)) {
    FML_LOG(ERROR) << "Invalid struct size for FlutterVulkanImage: "
                   << image->struct_size << " (expected at least "
                   << sizeof(FlutterVulkanImage) << ")";
    return nullptr;
  }

  if (image->image == 0) {
    FML_LOG(ERROR) << "Invalid null VkImage handle in FlutterVulkanImage.";
    return nullptr;
  }

  size_t width = size.width();
  size_t height = size.height();

  if (image->width != 0 && image->height != 0) {
    width = image->width;
    height = image->height;
  }

  if (width == 0 || height == 0) {
    FML_LOG(ERROR) << "Invalid zero dimensions for external texture: " << width
                   << "x" << height;
    return nullptr;
  }

  GrVkImageInfo image_info = {
      .fImage = reinterpret_cast<VkImage>(image->image),
      .fImageTiling = VK_IMAGE_TILING_OPTIMAL,
      .fImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .fFormat = static_cast<VkFormat>(image->format),
      .fImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                          VK_IMAGE_USAGE_SAMPLED_BIT,
      // Sample count: 1 (non-multisampled).
      .fSampleCount = 1,
      // Level count: 1 (single mip level).
      .fLevelCount = 1,
  };

  auto backend_texture =
      GrBackendTextures::MakeVk(width, height, image_info);

  SkImages::TextureReleaseProc release_proc = image->destruction_callback;
  auto sk_image = SkImages::BorrowTextureFrom(
      context,                                                           // context
      backend_texture,                                                   // texture handle
      kTopLeft_GrSurfaceOrigin,                                          // origin
      flutter::GPUSurfaceVulkan::ColorTypeFromFormat(image_info.fFormat), // color type
      kPremul_SkAlphaType,                                               // alpha type
      nullptr,                                                           // colorspace
      release_proc,                                                      // texture release proc
      image->user_data                                                   // texture release context
  );

  if (!sk_image) {
    FML_LOG(ERROR) << "Could not create external texture from Vulkan image.";
    return nullptr;
  }

  scoped_cleanup.Release();

  return DlImageSkia::Make(std::move(sk_image));
}

sk_sp<DlImage> EmbedderExternalTextureVK::ResolveTextureImpeller(
    int64_t texture_id,
    impeller::AiksContext* aiks_context,
    const SkISize& size) {
#if defined(SHELL_ENABLE_VULKAN) && defined(IMPELLER_SUPPORTS_RENDERING)
  std::unique_ptr<FlutterVulkanImage> image =
      external_texture_callback_(texture_id, size.width(), size.height());

  if (!image) {
    return nullptr;
  }

  // Call the destruction callback if an error occurs during wrapping.
  fml::ScopedCleanupClosure scoped_cleanup([&image]() {
    if (image->destruction_callback) {
      image->destruction_callback(image->user_data);
    }
  });

  if (image->struct_size < sizeof(FlutterVulkanImage)) {
    FML_LOG(ERROR) << "Invalid struct size for FlutterVulkanImage: "
                   << image->struct_size << " (expected at least "
                   << sizeof(FlutterVulkanImage) << ")";
    return nullptr;
  }

  if (image->image == 0) {
    FML_LOG(ERROR) << "Invalid null VkImage handle in FlutterVulkanImage.";
    return nullptr;
  }

  size_t width = size.width();
  size_t height = size.height();

  if (image->width != 0 && image->height != 0) {
    width = image->width;
    height = image->height;
  }

  if (width == 0 || height == 0) {
    FML_LOG(ERROR) << "Invalid zero dimensions for external texture: " << width
                   << "x" << height;
    return nullptr;
  }

  std::optional<impeller::PixelFormat> pixel_format =
      impeller::VkFormatToImpellerFormat(
          static_cast<impeller::vk::Format>(image->format));
  if (!pixel_format.has_value() ||
      pixel_format.value() == impeller::PixelFormat::kUnknown) {
    FML_LOG(ERROR)
        << "Unsupported Vulkan format for Impeller external texture: "
        << image->format;
    return nullptr;
  }

  impeller::TextureDescriptor desc;
  desc.size = impeller::ISize(width, height);
  desc.format = pixel_format.value();
  desc.storage_mode = impeller::StorageMode::kDevicePrivate;
  desc.usage = static_cast<impeller::TextureUsageMask>(
      impeller::TextureUsage::kShaderRead);
  // Sample count: 1 (non-multisampled).
  desc.sample_count = impeller::SampleCount::kCount1;

  auto context = aiks_context->GetContext();
  const auto& context_vk = impeller::ContextVK::Cast(*context);

  impeller::vk::Image vk_image =
      impeller::vk::Image(reinterpret_cast<VkImage>(image->image));

  impeller::vk::ImageViewCreateInfo view_info;
  view_info.image = vk_image;
  view_info.viewType = impeller::vk::ImageViewType::e2D;
  view_info.format = static_cast<impeller::vk::Format>(image->format);
  view_info.subresourceRange.aspectMask =
      impeller::vk::ImageAspectFlagBits::eColor;
  // Base mip level 0 and base array layer 0.
  view_info.subresourceRange.baseMipLevel = 0u;
  view_info.subresourceRange.baseArrayLayer = 0u;
  // Single mip level and single array layer.
  view_info.subresourceRange.levelCount = 1u;
  view_info.subresourceRange.layerCount = 1u;

  auto [result, image_view] =
      context_vk.GetDevice().createImageViewUnique(view_info);
  if (result != impeller::vk::Result::eSuccess) {
    FML_LOG(ERROR) << "Failed to create Vulkan image view for external texture: "
                   << impeller::vk::to_string(result);
    return nullptr;
  }

  fml::closure destruction_callback = [callback = image->destruction_callback,
                                       user_data = image->user_data]() {
    if (callback) {
      callback(user_data);
    }
  };

  auto wrapped_texture_source =
      std::make_shared<EmbedderExternalTextureSourceVK>(
          vk_image, std::move(image_view), desc,
          std::move(destruction_callback));

  std::shared_ptr<impeller::Texture> texture =
      std::make_shared<impeller::TextureVK>(context,
                                            std::move(wrapped_texture_source));
  if (!texture) {
    FML_LOG(ERROR) << "Could not create TextureVK from wrapped texture source.";
    return nullptr;
  }

  scoped_cleanup.Release();

  return impeller::DlImageImpeller::Make(std::move(texture));
#else
  return nullptr;
#endif
}

// |flutter::Texture|
void EmbedderExternalTextureVK::OnGrContextCreated() {}

// |flutter::Texture|
void EmbedderExternalTextureVK::OnGrContextDestroyed() {
  last_image_ = nullptr;
}

// |flutter::Texture|
void EmbedderExternalTextureVK::MarkNewFrameAvailable() {
  last_image_ = nullptr;
}

// |flutter::Texture|
void EmbedderExternalTextureVK::OnTextureUnregistered() {
  last_image_ = nullptr;
}

}  // namespace flutter
