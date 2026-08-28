// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/embedder/embedder_external_texture_vk.h"

#include <utility>

#include "flutter/display_list/image/dl_image_skia.h"
#include "flutter/fml/closure.h"
#include "flutter/fml/logging.h"

#if IMPELLER_SUPPORTS_RENDERING
#include "impeller/core/formats.h"
#include "impeller/core/texture_descriptor.h"
#include "impeller/display_list/aiks_context.h"
#include "impeller/display_list/dl_image_impeller.h"
#include "impeller/geometry/size.h"
#include "impeller/renderer/backend/vulkan/command_buffer_vk.h"
#include "impeller/renderer/backend/vulkan/context_vk.h"
#include "impeller/renderer/backend/vulkan/formats_vk.h"
#include "impeller/renderer/backend/vulkan/texture_vk.h"
#include "impeller/renderer/backend/vulkan/texture_wrapper_vk.h"
#if defined(FML_OS_ANDROID)
#include "impeller/renderer/backend/vulkan/android/ahb_texture_source_vk.h"
#include "impeller/toolkit/android/hardware_buffer.h"
#endif
#endif  // IMPELLER_SUPPORTS_RENDERING

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
#if defined(FML_OS_ANDROID) && defined(SK_BUILD_FOR_ANDROID) && \
    __ANDROID_API__ >= 26
#include "third_party/skia/include/android/GrAHardwareBufferUtils.h"
#include "third_party/skia/include/android/SkImageAndroid.h"
#endif

namespace flutter {

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
  if (!!aiks_context) {
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
  if (!context) {
    return nullptr;
  }
  context->flushAndSubmit();
  context->resetContext(kAll_GrBackendState);

  std::unique_ptr<FlutterVulkanExternalTexture> texture =
      external_texture_callback_(texture_id, size.width(), size.height());

  if (!texture) {
    return nullptr;
  }

  size_t width = size.width();
  size_t height = size.height();
  if (texture->width != 0 && texture->height != 0) {
    width = texture->width;
    height = texture->height;
  }

  if (width == 0 || height == 0) {
    if (texture->destruction_callback) {
      texture->destruction_callback(texture->user_data);
    }
    FML_LOG(ERROR) << "Invalid external texture dimensions: " << width << "x"
                   << height;
    return nullptr;
  }

  SkColorType color_type = kRGBA_8888_SkColorType;
  if (static_cast<VkFormat>(texture->format) == VK_FORMAT_B8G8R8A8_UNORM) {
    color_type = kBGRA_8888_SkColorType;
  }

  if (texture->type == kFlutterVulkanExternalTextureTypeVkImage) {
    GrVkImageInfo gr_image_info = {};
    gr_image_info.fImage = reinterpret_cast<VkImage>(texture->vk_image);
    gr_image_info.fImageTiling = VK_IMAGE_TILING_OPTIMAL;
    gr_image_info.fImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    gr_image_info.fFormat = static_cast<VkFormat>(texture->format);
    gr_image_info.fImageUsageFlags =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    gr_image_info.fSampleCount = 1;
    gr_image_info.fLevelCount = 1;

    auto gr_backend_texture =
        GrBackendTextures::MakeVk(width, height, gr_image_info);
    SkImages::TextureReleaseProc release_proc = texture->destruction_callback;
    auto image = SkImages::BorrowTextureFrom(
        context, gr_backend_texture, kTopLeft_GrSurfaceOrigin, color_type,
        kPremul_SkAlphaType, nullptr, release_proc, texture->user_data);

    if (!image) {
      if (release_proc) {
        release_proc(texture->user_data);
      }
      FML_LOG(ERROR) << "Could not create Skia Vulkan external texture.";
      return nullptr;
    }

    return DlImageSkia::Make(std::move(image));
  } else if (texture->type ==
             kFlutterVulkanExternalTextureTypeAHardwareBuffer) {
#if defined(FML_OS_ANDROID) && defined(SK_BUILD_FOR_ANDROID) && \
    __ANDROID_API__ >= 26
    AHardwareBuffer* ahb =
        reinterpret_cast<AHardwareBuffer*>(texture->hardware_buffer);
    if (!ahb) {
      if (texture->destruction_callback) {
        texture->destruction_callback(texture->user_data);
      }
      FML_LOG(ERROR) << "Embedder supplied null AHardwareBuffer.";
      return nullptr;
    }

    GrAHardwareBufferUtils::DeleteImageProc delete_proc = nullptr;
    GrAHardwareBufferUtils::UpdateImageProc update_proc = nullptr;
    GrAHardwareBufferUtils::TexImageCtx image_ctx = nullptr;
    auto backend_format = GrAHardwareBufferUtils::GetVulkanBackendFormat(
        context, ahb, texture->format, false);
    auto backend_texture = GrAHardwareBufferUtils::MakeVulkanBackendTexture(
        context, ahb, width, height, &delete_proc, &update_proc, &image_ctx,
        false, backend_format, false);
    if (!backend_texture.isValid()) {
      if (texture->destruction_callback) {
        texture->destruction_callback(texture->user_data);
      }
      FML_LOG(ERROR)
          << "Could not make Vulkan backend texture from AHardwareBuffer.";
      return nullptr;
    }

    struct ReleaseContext {
      GrAHardwareBufferUtils::DeleteImageProc delete_proc;
      GrAHardwareBufferUtils::TexImageCtx image_ctx;
      VoidCallback destruction_callback;
      void* user_data;
    };
    auto* release_ctx = new ReleaseContext{
        .delete_proc = delete_proc,
        .image_ctx = image_ctx,
        .destruction_callback = texture->destruction_callback,
        .user_data = texture->user_data,
    };

    auto image = SkImages::BorrowTextureFrom(
        context, backend_texture, kTopLeft_GrSurfaceOrigin, color_type,
        kPremul_SkAlphaType, nullptr,
        [](void* ctx) {
          auto* rc = static_cast<ReleaseContext*>(ctx);
          if (rc->delete_proc && rc->image_ctx) {
            rc->delete_proc(rc->image_ctx);
          }
          if (rc->destruction_callback) {
            rc->destruction_callback(rc->user_data);
          }
          delete rc;
        },
        release_ctx);

    if (!image) {
      if (delete_proc && image_ctx) {
        delete_proc(image_ctx);
      }
      if (texture->destruction_callback) {
        texture->destruction_callback(texture->user_data);
      }
      delete release_ctx;
      FML_LOG(ERROR)
          << "Could not create Skia AHardwareBuffer external texture.";
      return nullptr;
    }
    return DlImageSkia::Make(std::move(image));
#else
    if (texture->destruction_callback) {
      texture->destruction_callback(texture->user_data);
    }
    FML_LOG(ERROR) << "AHardwareBuffer external textures are only supported on "
                      "Android API 26+.";
    return nullptr;
#endif
  }

  if (texture->destruction_callback) {
    texture->destruction_callback(texture->user_data);
  }
  return nullptr;
}

#if IMPELLER_SUPPORTS_RENDERING && defined(FML_OS_ANDROID)
namespace {
class WrappedAHBTextureSourceVK final : public impeller::TextureSourceVK {
 public:
  WrappedAHBTextureSourceVK(
      std::shared_ptr<impeller::AHBTextureSourceVK> source,
      std::function<void()> deletion_proc)
      : TextureSourceVK(source->GetTextureDescriptor()),
        source_(std::move(source)),
        deletion_proc_(std::move(deletion_proc)) {}

  ~WrappedAHBTextureSourceVK() override {
    if (deletion_proc_) {
      deletion_proc_();
    }
  }

  impeller::vk::Image GetImage() const override { return source_->GetImage(); }

  impeller::vk::ImageView GetImageView() const override {
    return source_->GetImageView();
  }

  impeller::vk::ImageView GetRenderTargetView(
      uint32_t mip_level,
      uint32_t array_layer) const override {
    return source_->GetRenderTargetView(mip_level, array_layer);
  }

  bool IsSwapchainImage() const override { return source_->IsSwapchainImage(); }

  std::shared_ptr<impeller::YUVConversionVK> GetYUVConversion() const override {
    return source_->GetYUVConversion();
  }

 private:
  std::shared_ptr<impeller::AHBTextureSourceVK> source_;
  std::function<void()> deletion_proc_;
};
}  // namespace
#endif  // IMPELLER_SUPPORTS_RENDERING && defined(FML_OS_ANDROID)

sk_sp<DlImage> EmbedderExternalTextureVK::ResolveTextureImpeller(
    int64_t texture_id,
    impeller::AiksContext* aiks_context,
    const SkISize& size) {
#if IMPELLER_SUPPORTS_RENDERING
  if (!aiks_context || !aiks_context->GetContext()) {
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

  size_t width = size.width();
  size_t height = size.height();
  if (texture->width != 0 && texture->height != 0) {
    width = texture->width;
    height = texture->height;
  }

  if (width == 0 || height == 0) {
    FML_LOG(ERROR) << "Invalid external texture dimensions: " << width << "x"
                   << height;
    return nullptr;
  }

  if (texture->type == kFlutterVulkanExternalTextureTypeVkImage) {
    if (!texture->vk_image) {
      FML_LOG(ERROR) << "Embedder supplied null VkImage handle.";
      return nullptr;
    }

    auto vk_format = static_cast<impeller::vk::Format>(texture->format);
    auto format = impeller::VkFormatToImpellerFormat(vk_format);
    if (!format.has_value()) {
      FML_LOG(ERROR) << "Unsupported pixel format for Vulkan external texture: "
                     << impeller::vk::to_string(vk_format);
      return nullptr;
    }

    impeller::TextureDescriptor desc;
    desc.format = format.value();
    desc.size = impeller::ISize(width, height);
    desc.storage_mode = impeller::StorageMode::kDevicePrivate;
    desc.mip_count = 1;
    desc.usage = impeller::TextureUsage::kShaderRead;

    auto wrapped_texture = impeller::WrapTextureVK(
        aiks_context->GetContext(), desc,
        impeller::vk::Image(reinterpret_cast<VkImage>(texture->vk_image)),
        [callback = texture->destruction_callback,
         user_data = texture->user_data]() {
          if (callback) {
            callback(user_data);
          }
        });

    if (!wrapped_texture) {
      FML_LOG(ERROR)
          << "Could not wrap embedder supplied Vulkan external texture.";
      return nullptr;
    }

    scoped_cleanup.Release();
    return impeller::DlImageImpeller::Make(wrapped_texture);
  } else if (texture->type ==
             kFlutterVulkanExternalTextureTypeAHardwareBuffer) {
#if defined(FML_OS_ANDROID)
    if (!texture->hardware_buffer) {
      FML_LOG(ERROR) << "Embedder supplied null AHardwareBuffer handle.";
      return nullptr;
    }
    AHardwareBuffer* ahb =
        reinterpret_cast<AHardwareBuffer*>(texture->hardware_buffer);
    auto hb_desc = impeller::android::HardwareBuffer::Describe(ahb);
    if (!hb_desc.has_value()) {
      FML_LOG(ERROR) << "Could not describe AHardwareBuffer.";
      return nullptr;
    }

    auto raw_source = std::make_shared<impeller::AHBTextureSourceVK>(
        aiks_context->GetContext(), ahb, hb_desc.value());
    if (!raw_source->IsValid()) {
      FML_LOG(ERROR) << "Could not create AHBTextureSourceVK.";
      return nullptr;
    }

    std::shared_ptr<impeller::TextureSourceVK> wrapped_source =
        std::make_shared<WrappedAHBTextureSourceVK>(
            raw_source, [callback = texture->destruction_callback,
                         user_data = texture->user_data]() {
              if (callback) {
                callback(user_data);
              }
            });

    auto impeller_texture = std::make_shared<impeller::TextureVK>(
        aiks_context->GetContext(), wrapped_source);

    // Transition layout to shader read optimal.
    auto context_vk = std::static_pointer_cast<impeller::ContextVK>(
        aiks_context->GetContext());
    auto buffer = context_vk->CreateCommandBuffer();
    if (!buffer) {
      FML_LOG(ERROR) << "Could not create command buffer for AHB transition.";
      return nullptr;
    }
    impeller::CommandBufferVK& buffer_vk =
        impeller::CommandBufferVK::Cast(*buffer);
    impeller::BarrierVK barrier;
    barrier.cmd_buffer = buffer_vk.GetCommandBuffer();
    barrier.src_access = impeller::vk::AccessFlagBits::eColorAttachmentWrite |
                         impeller::vk::AccessFlagBits::eTransferWrite;
    barrier.src_stage =
        impeller::vk::PipelineStageFlagBits::eColorAttachmentOutput |
        impeller::vk::PipelineStageFlagBits::eTransfer;
    barrier.dst_access = impeller::vk::AccessFlagBits::eShaderRead;
    barrier.dst_stage = impeller::vk::PipelineStageFlagBits::eFragmentShader;
    barrier.new_layout = impeller::vk::ImageLayout::eShaderReadOnlyOptimal;

    if (!impeller_texture->SetLayout(barrier) ||
        !context_vk->GetCommandQueue()->Submit({buffer}).ok()) {
      FML_LOG(ERROR) << "Failed to transition AHardwareBuffer layout to "
                        "shader read optimal.";
      return nullptr;
    }

    scoped_cleanup.Release();
    return impeller::DlImageImpeller::Make(impeller_texture);
#else
    FML_LOG(ERROR)
        << "AHardwareBuffer external textures are only supported on Android.";
    return nullptr;
#endif
  }

  return nullptr;
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
