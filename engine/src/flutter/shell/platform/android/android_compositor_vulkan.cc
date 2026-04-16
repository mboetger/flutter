// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_compositor_vulkan.h"
#include "flutter/fml/logging.h"
#include "flutter/shell/platform/embedder/embedder_render_target_impeller.h"
#include "impeller/display_list/aiks_context.h"
#include "impeller/renderer/backend/vulkan/android/ahb_texture_source_vk.h"
#include "impeller/renderer/backend/vulkan/formats_vk.h"
#include "impeller/renderer/backend/vulkan/texture_vk.h"
#include "impeller/renderer/render_target.h"
#include "impeller/toolkit/android/hardware_buffer.h"
#include "impeller/toolkit/android/surface_transaction.h"

namespace flutter {

struct BackingStoreRecord {
  std::shared_ptr<const impeller::AHBTextureSourceVK> source;
  FlutterVulkanImage vulkan_image;
};

AndroidCompositorVulkan::AndroidCompositorVulkan(
    std::shared_ptr<impeller::ContextVK> context)
    : context_(std::move(context)) {}

AndroidCompositorVulkan::~AndroidCompositorVulkan() = default;

void AndroidCompositorVulkan::SetNativeWindow(ANativeWindow* window) {
  if (window) {
    root_surface_control_ = std::make_unique<impeller::android::SurfaceControl>(
        window, "Flutter Root");
  } else {
    root_surface_control_.reset();
  }
}

bool AndroidCompositorVulkan::CreateBackingStoreCallback(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out,
    void* user_data) {
  auto* self = static_cast<AndroidCompositorVulkan*>(user_data);
  return self->CreateBackingStore(config, backing_store_out);
}

bool AndroidCompositorVulkan::CollectBackingStoreCallback(
    const FlutterBackingStore* backing_store,
    void* user_data) {
  auto* self = static_cast<AndroidCompositorVulkan*>(user_data);
  return self->CollectBackingStore(backing_store);
}

bool AndroidCompositorVulkan::PresentViewCallback(
    const FlutterPresentViewInfo* info) {
  auto* self = static_cast<AndroidCompositorVulkan*>(info->user_data);
  return self->PresentView(info);
}

bool AndroidCompositorVulkan::CreateBackingStore(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out) {
  impeller::ISize size(config->size.width, config->size.height);
  auto desc =
      impeller::android::HardwareBufferDescriptor::MakeForSwapchainImage(size);

  auto hardware_buffer =
      std::make_unique<impeller::android::HardwareBuffer>(desc);
  if (!hardware_buffer->IsValid()) {
    FML_LOG(ERROR) << "Failed to allocate AHardwareBuffer.";
    return false;
  }

  auto source = std::make_shared<impeller::AHBTextureSourceVK>(
      context_, std::move(hardware_buffer), true /* is_swapchain_image */);

  if (!source->IsValid()) {
    FML_LOG(ERROR) << "Failed to wrap AHardwareBuffer as Vulkan image.";
    return false;
  }

  auto record = std::make_unique<BackingStoreRecord>();
  record->source = source;
  record->vulkan_image.struct_size = sizeof(FlutterVulkanImage);
  record->vulkan_image.image =
      reinterpret_cast<uint64_t>(static_cast<VkImage>(source->GetImage()));

  auto pixel_format = source->GetTextureDescriptor().format;
  auto vk_format = impeller::ToVKImageFormat(pixel_format);
  record->vulkan_image.format = static_cast<uint32_t>(vk_format);

  backing_store_out->type = kFlutterBackingStoreTypeVulkan;
  backing_store_out->vulkan.struct_size = sizeof(FlutterVulkanBackingStore);
  backing_store_out->vulkan.image = &record->vulkan_image;
  backing_store_out->vulkan.user_data = record.release();
  backing_store_out->vulkan.destruction_callback = [](void* p) {
    delete static_cast<BackingStoreRecord*>(p);
  };

  return true;
}

bool AndroidCompositorVulkan::CollectBackingStore(
    const FlutterBackingStore* backing_store) {
  if (backing_store->type == kFlutterBackingStoreTypeVulkan) {
    if (backing_store->vulkan.destruction_callback) {
      backing_store->vulkan.destruction_callback(
          backing_store->vulkan.user_data);
    }
  }
  return true;
}

std::unique_ptr<EmbedderRenderTarget>
AndroidCompositorVulkan::CreateRenderTarget(
    const std::shared_ptr<impeller::AiksContext>& aiks_context,
    const FlutterBackingStoreConfig& config) {
  impeller::ISize size(config.size.width, config.size.height);
  auto desc =
      impeller::android::HardwareBufferDescriptor::MakeForSwapchainImage(size);

  auto hardware_buffer =
      std::make_unique<impeller::android::HardwareBuffer>(desc);
  if (!hardware_buffer->IsValid()) {
    FML_LOG(ERROR) << "Failed to allocate AHardwareBuffer.";
    return nullptr;
  }

  auto source = std::make_shared<impeller::AHBTextureSourceVK>(
      context_, std::move(hardware_buffer), true /* is_swapchain_image */);

  if (!source->IsValid()) {
    FML_LOG(ERROR) << "Failed to wrap AHardwareBuffer as Vulkan image.";
    return nullptr;
  }

  auto resolve_tex = std::make_shared<impeller::TextureVK>(
      aiks_context->GetContext(), std::move(source));

  static_cast<impeller::Texture*>(resolve_tex.get())
      ->SetLabel("ImpellerBackingStoreResolve");

  impeller::TextureDescriptor msaa_tex_desc;
  msaa_tex_desc.storage_mode = impeller::StorageMode::kDeviceTransient;
  msaa_tex_desc.type = impeller::TextureType::kTexture2DMultisample;
  msaa_tex_desc.sample_count = impeller::SampleCount::kCount4;
  msaa_tex_desc.format = resolve_tex->GetTextureDescriptor().format;
  msaa_tex_desc.size = resolve_tex->GetTextureDescriptor().size;
  msaa_tex_desc.usage = impeller::TextureUsage::kRenderTarget;

  auto msaa_tex =
      aiks_context->GetContext()->GetResourceAllocator()->CreateTexture(
          msaa_tex_desc);
  if (!msaa_tex) {
    FML_LOG(ERROR) << "Could not allocate MSAA color texture.";
    return nullptr;
  }
  msaa_tex->SetLabel("ImpellerBackingStoreColorMSAA");

  impeller::ColorAttachment color0;
  color0.texture = msaa_tex;
  color0.clear_color = impeller::Color::DarkSlateGray();
  color0.load_action = impeller::LoadAction::kClear;
  color0.store_action = impeller::StoreAction::kMultisampleResolve;
  color0.resolve_texture = resolve_tex;

  impeller::RenderTarget render_target_desc;
  render_target_desc.SetColorAttachment(color0, 0u);

  auto record = std::make_unique<BackingStoreRecord>();
  record->source = std::static_pointer_cast<const impeller::AHBTextureSourceVK>(
      resolve_tex->GetTextureSource());
  record->vulkan_image.struct_size = sizeof(FlutterVulkanImage);
  record->vulkan_image.image = reinterpret_cast<uint64_t>(
      static_cast<VkImage>(record->source->GetImage()));

  auto pixel_format = record->source->GetTextureDescriptor().format;
  auto vk_format = impeller::ToVKImageFormat(pixel_format);
  record->vulkan_image.format = static_cast<uint32_t>(vk_format);

  FlutterBackingStore backing_store = {};
  backing_store.struct_size = sizeof(FlutterBackingStore);
  backing_store.type = kFlutterBackingStoreTypeVulkan;
  backing_store.vulkan.struct_size = sizeof(FlutterVulkanBackingStore);
  backing_store.vulkan.image = &record->vulkan_image;

  auto* raw_record = record.release();
  backing_store.vulkan.user_data = raw_record;

  return std::make_unique<EmbedderRenderTargetImpeller>(
      backing_store, aiks_context,
      std::make_unique<impeller::RenderTarget>(std::move(render_target_desc)),
      [raw_record]() { delete raw_record; }, fml::closure());
}

bool AndroidCompositorVulkan::PresentView(const FlutterPresentViewInfo* info) {
  if (!root_surface_control_ || !root_surface_control_->IsValid()) {
    FML_LOG(ERROR) << "No valid root surface control.";
    return false;
  }

  impeller::android::SurfaceTransaction transaction;
  if (!transaction.IsValid()) {
    FML_LOG(ERROR) << "Failed to create surface transaction.";
    return false;
  }

  for (size_t i = 0; i < info->layers_count; ++i) {
    const FlutterLayer* layer = info->layers[i];
    if (layer->type == kFlutterLayerContentTypeBackingStore) {
      const FlutterBackingStore* backing_store = layer->backing_store;
      if (backing_store->type == kFlutterBackingStoreTypeVulkan) {
        auto* record =
            static_cast<BackingStoreRecord*>(backing_store->vulkan.user_data);
        const impeller::android::HardwareBuffer* buffer =
            record->source->GetBackingStore();

        // For now, we only support setting contents of the root surface
        // control. This assumes a single layer filling the screen.
        if (!transaction.SetContents(root_surface_control_.get(), buffer)) {
          FML_LOG(ERROR) << "Failed to set contents of surface control.";
          return false;
        }
      }
    }
  }

  return transaction.Apply();
}

}  // namespace flutter
