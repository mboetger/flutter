// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/embedder/embedder_external_texture_vk.h"

#include "flutter/display_list/image/dl_image_skia.h"
#include "flutter/fml/closure.h"
#include "flutter/fml/logging.h"
#include "flutter/shell/platform/embedder/embedder_struct_macros.h"

#ifdef SHELL_ENABLE_VULKAN
#include "flutter/shell/gpu/gpu_surface_vulkan.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkTypes.h"

#ifdef IMPELLER_SUPPORTS_RENDERING
#include "flutter/impeller/core/formats.h"
#include "flutter/impeller/core/texture_descriptor.h"
#include "flutter/impeller/display_list/aiks_context.h"
#include "flutter/impeller/display_list/dl_image_impeller.h"
#include "flutter/impeller/geometry/size.h"
#include "flutter/impeller/renderer/backend/vulkan/context_vk.h"
#include "flutter/impeller/renderer/backend/vulkan/device_holder_vk.h"
#include "flutter/impeller/renderer/backend/vulkan/formats_vk.h"
#include "flutter/impeller/renderer/backend/vulkan/texture_source_vk.h"
#include "flutter/impeller/renderer/backend/vulkan/texture_vk.h"
#include "flutter/impeller/renderer/backend/vulkan/vk.h"
#endif  // IMPELLER_SUPPORTS_RENDERING
#endif  // SHELL_ENABLE_VULKAN

namespace flutter {

namespace {

void* GetUserData(const FlutterVulkanImage* image) {
  return SAFE_ACCESS(image, user_data, nullptr);
}

VoidCallback GetDestructionCallback(const FlutterVulkanImage* image) {
  return SAFE_ACCESS(image, destruction_callback, nullptr);
}

uint64_t GetImageHandle(const FlutterVulkanImage* image) {
  return SAFE_ACCESS(image, image, 0);
}

size_t GetImageWidth(const FlutterVulkanImage* image) {
  return SAFE_ACCESS(image, width, 0);
}

size_t GetImageHeight(const FlutterVulkanImage* image) {
  return SAFE_ACCESS(image, height, 0);
}

uint32_t GetImageFormat(const FlutterVulkanImage* image) {
  return SAFE_ACCESS(image, format, 0);
}

}  // namespace

EmbedderExternalTextureVK::EmbedderExternalTextureVK(
    int64_t texture_identifier,
    const ExternalTextureCallback& callback)
    : Texture(texture_identifier), external_texture_callback_(callback) {
  FML_DCHECK(external_texture_callback_);
}

EmbedderExternalTextureVK::~EmbedderExternalTextureVK() = default;

void EmbedderExternalTextureVK::Paint(PaintContext& context,
                                      const DlRect& bounds,
                                      bool freeze,
                                      const DlImageSampling sampling) {
  if (bounds.GetWidth() <= 0 || bounds.GetHeight() <= 0) {
    return;
  }

  if (last_image_ == nullptr) {
    last_image_ =
        ResolveTexture(Id(), context.gr_context, context.aiks_context,
                       SkISize::Make(bounds.GetWidth(), bounds.GetHeight()));
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
  if (size.width() <= 0 || size.height() <= 0) {
    return nullptr;
  }
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
  if (!context || size.width() <= 0 || size.height() <= 0) {
    return nullptr;
  }
  std::unique_ptr<FlutterVulkanImage> image =
      external_texture_callback_(texture_id, size.width(), size.height());
  if (!image) {
    return nullptr;
  }

  void* user_data = GetUserData(image.get());
  VoidCallback destruction_callback = GetDestructionCallback(image.get());
  fml::ScopedCleanupClosure cleanup([destruction_callback, user_data]() {
    if (destruction_callback) {
      destruction_callback(user_data);
    }
  });

  uint64_t image_handle = GetImageHandle(image.get());
  if (image_handle == 0) {
    return nullptr;
  }

  size_t width = size.width();
  size_t height = size.height();
  size_t image_width = GetImageWidth(image.get());
  size_t image_height = GetImageHeight(image.get());
  if (image_width != 0 && image_height != 0) {
    width = image_width;
    height = image_height;
  }

  if (width == 0 || height == 0) {
    return nullptr;
  }

  uint32_t image_format = GetImageFormat(image.get());
  SkColorType color_type = GPUSurfaceVulkan::ColorTypeFromFormat(
      static_cast<VkFormat>(image_format));
  if (color_type == kUnknown_SkColorType) {
    FML_LOG(ERROR) << "Unsupported Vulkan pixel format for Skia: "
                   << image_format;
    return nullptr;
  }

  GrVkImageInfo image_info = {
      .fImage = reinterpret_cast<VkImage>(image_handle),
      .fImageTiling = VK_IMAGE_TILING_OPTIMAL,
      .fImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .fFormat = static_cast<VkFormat>(image_format),
      .fImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                          VK_IMAGE_USAGE_SAMPLED_BIT,
      .fSampleCount = 1,
      .fLevelCount = 1,
      .fCurrentQueueFamily = VK_QUEUE_FAMILY_IGNORED,
  };

  auto gr_backend_texture =
      GrBackendTextures::MakeVk(width, height, image_info);

  SkImages::TextureReleaseProc release_proc = destruction_callback;
  auto sk_image = SkImages::BorrowTextureFrom(
      context, gr_backend_texture, kTopLeft_GrSurfaceOrigin, color_type,
      kPremul_SkAlphaType, nullptr, release_proc, user_data);

  if (!sk_image) {
    FML_LOG(ERROR) << "Could not create external Vulkan texture for Skia.";
    return nullptr;
  }

  cleanup.Release();

  return DlImageSkia::Make(std::move(sk_image));
#else
  return nullptr;
#endif
}

sk_sp<DlImage> EmbedderExternalTextureVK::ResolveTextureImpeller(
    int64_t texture_id,
    impeller::AiksContext* aiks_context,
    const SkISize& size) {
#if defined(SHELL_ENABLE_VULKAN) && defined(IMPELLER_SUPPORTS_RENDERING)
  if (!aiks_context || !aiks_context->GetContext() ||
      !aiks_context->GetContext()->IsValid() || size.width() <= 0 ||
      size.height() <= 0) {
    return nullptr;
  }

  std::unique_ptr<FlutterVulkanImage> image =
      external_texture_callback_(texture_id, size.width(), size.height());
  if (!image) {
    return nullptr;
  }

  void* user_data = GetUserData(image.get());
  VoidCallback destruction_callback = GetDestructionCallback(image.get());
  fml::ScopedCleanupClosure cleanup([destruction_callback, user_data]() {
    if (destruction_callback) {
      destruction_callback(user_data);
    }
  });

  uint64_t image_handle = GetImageHandle(image.get());
  if (image_handle == 0) {
    return nullptr;
  }

  size_t width = size.width();
  size_t height = size.height();
  size_t image_width = GetImageWidth(image.get());
  size_t image_height = GetImageHeight(image.get());
  if (image_width != 0 && image_height != 0) {
    width = image_width;
    height = image_height;
  }

  if (width == 0 || height == 0) {
    return nullptr;
  }

  uint32_t raw_format = GetImageFormat(image.get());
  impeller::vk::Format vk_format =
      static_cast<impeller::vk::Format>(raw_format);
  std::optional<impeller::PixelFormat> format =
      impeller::VkFormatToImpellerFormat(vk_format);
  if (!format.has_value()) {
    FML_LOG(ERROR) << "Unsupported pixel format: "
                   << impeller::vk::to_string(vk_format);
    return nullptr;
  }

  impeller::ContextVK& context_vk =
      impeller::ContextVK::Cast(*aiks_context->GetContext());

  impeller::vk::Image vk_image =
      impeller::vk::Image(reinterpret_cast<VkImage>(image_handle));

  impeller::TextureDescriptor desc;
  desc.format = format.value();
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
    FML_LOG(ERROR) << "Failed to create image view for external image: "
                   << impeller::vk::to_string(result);
    return nullptr;
  }

  class EmbedderExternalTextureSourceVK : public impeller::TextureSourceVK {
   public:
    explicit EmbedderExternalTextureSourceVK(
        std::weak_ptr<impeller::DeviceHolderVK> device_holder,
        impeller::vk::Image image_handle,
        impeller::vk::UniqueImageView view,
        impeller::TextureDescriptor descriptor,
        VoidCallback destruction_cb,
        void* user_data)
        : TextureSourceVK(descriptor),
          device_holder_(std::move(device_holder)),
          image_(image_handle),
          image_view_(std::move(view)),
          destruction_callback_(destruction_cb),
          user_data_(user_data) {}

    ~EmbedderExternalTextureSourceVK() override {
      if (auto device = device_holder_.lock(); !device) {
        image_view_.release();
      } else {
        image_view_.reset();
      }
      if (destruction_callback_) {
        destruction_callback_(user_data_);
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

    std::weak_ptr<impeller::DeviceHolderVK> device_holder_;
    impeller::vk::Image image_;
    impeller::vk::UniqueImageView image_view_;
    VoidCallback destruction_callback_;
    void* user_data_;
  };

  auto texture_source = std::make_shared<EmbedderExternalTextureSourceVK>(
      context_vk.GetDeviceHolder(), vk_image, std::move(image_view), desc,
      destruction_callback, user_data);

  texture_source->SetLayoutWithoutEncoding(
      impeller::vk::ImageLayout::eShaderReadOnlyOptimal);

  auto texture = std::make_shared<impeller::TextureVK>(
      aiks_context->GetContext(), texture_source);

  cleanup.Release();

  return impeller::DlImageImpeller::Make(texture);
#else
  return nullptr;
#endif
}

void EmbedderExternalTextureVK::OnGrContextCreated() {}
void EmbedderExternalTextureVK::OnGrContextDestroyed() {}
void EmbedderExternalTextureVK::MarkNewFrameAvailable() {
  last_image_ = nullptr;
}
void EmbedderExternalTextureVK::OnTextureUnregistered() {}

}  // namespace flutter
