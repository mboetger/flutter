// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/image_external_texture_vk_impeller.h"

#include <cstdint>

#include "flutter/fml/closure.h"
#include "flutter/impeller/core/formats.h"
#include "flutter/impeller/core/texture_descriptor.h"
#include "flutter/impeller/display_list/dl_image_impeller.h"
#include "flutter/impeller/renderer/backend/vulkan/android/ahb_texture_source_vk.h"
#include "flutter/impeller/renderer/backend/vulkan/command_buffer_vk.h"
#include "flutter/impeller/renderer/backend/vulkan/texture_vk.h"
#include "flutter/impeller/toolkit/android/hardware_buffer.h"

namespace flutter {

ImageExternalTextureVKImpeller::ImageExternalTextureVKImpeller(
    const std::shared_ptr<impeller::ContextVK>& impeller_context,
    int64_t id,
    const fml::jni::ScopedJavaGlobalRef<jobject>& image_texture_entry,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
    ImageExternalTexture::ImageLifecycle lifecycle)
    : ImageExternalTexture(id, image_texture_entry, jni_facade, lifecycle),
      impeller_context_(impeller_context) {}

ImageExternalTextureVKImpeller::~ImageExternalTextureVKImpeller() {}

void ImageExternalTextureVKImpeller::Attach(PaintContext& context) {
  if (state_ == AttachmentState::kUninitialized) {
    // First processed frame we are attached.
    state_ = AttachmentState::kAttached;
  }
}

void ImageExternalTextureVKImpeller::Detach() {}

void ImageExternalTextureVKImpeller::ProcessFrame(PaintContext& context,
                                                  const SkRect& bounds) {
  JavaLocalRef image = AcquireLatestImage();
  if (image.is_null()) {
    return;
  }
  JavaLocalRef hardware_buffer = HardwareBufferFor(image);
  if (hardware_buffer.is_null()) {
    return;
  }
  fml::ScopedCleanupClosure close_hardware_buffer(
      [this, &hardware_buffer]() { CloseHardwareBuffer(hardware_buffer); });
  AHardwareBuffer* latest_hardware_buffer = AHardwareBufferFor(hardware_buffer);

  auto hb_desc =
      impeller::android::HardwareBuffer::Describe(latest_hardware_buffer);
  if (!hb_desc.has_value()) {
    return;
  }
  std::optional<HardwareBufferKey> key =
      impeller::android::HardwareBuffer::GetSystemUniqueID(
          latest_hardware_buffer);
  auto existing_image = image_lru_.FindImage(key);
  std::shared_ptr<impeller::TextureVK> texture;
  if (existing_image != nullptr && existing_image->asImpellerImage()) {
    auto existing_texture = existing_image->asImpellerImage()->GetTexture();
    if (existing_texture) {
      texture = std::static_pointer_cast<impeller::TextureVK>(existing_texture);
    }
  }
  if (!texture) {
    auto texture_source = std::make_shared<impeller::AHBTextureSourceVK>(
        impeller_context_, latest_hardware_buffer, hb_desc.value());
    if (!texture_source->IsValid()) {
      return;
    }
    texture = std::make_shared<impeller::TextureVK>(impeller_context_,
                                                    texture_source);
  }

  // Transition the layout to shader read and acquire external queue family
  // ownership from the producer (e.g. SurfaceFlinger / VirtualDisplay).
  {
    auto buffer = impeller_context_->CreateCommandBuffer();
    impeller::CommandBufferVK& buffer_vk =
        impeller::CommandBufferVK::Cast(*buffer);
    if (!buffer_vk.Track(texture->GetTextureSource())) {
      return;
    }

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
    barrier.src_queue_family_index = VK_QUEUE_FAMILY_EXTERNAL;
    barrier.dst_queue_family_index =
        impeller_context_->GetGraphicsQueue()->GetIndex().family;

    if (!texture->SetLayout(barrier)) {
      return;
    }
    if (!impeller_context_->GetCommandQueue()->Submit({buffer}).ok()) {
      return;
    }
  }

  if (existing_image != nullptr) {
    dl_image_ = existing_image;
  } else {
    dl_image_ = impeller::DlImageImpeller::Make(texture);
    if (key.has_value()) {
      image_lru_.AddImage(dl_image_, key.value());
    }
  }
}

}  // namespace flutter
