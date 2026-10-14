// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>

#include "flutter/testing/testing.h"  // IWYU pragma: keep
#include "gtest/gtest.h"
#include "impeller/core/allocator.h"
#include "impeller/core/formats.h"
#include "impeller/core/texture_descriptor.h"
#include "impeller/renderer/backend/vulkan/command_buffer_vk.h"
#include "impeller/renderer/backend/vulkan/context_vk.h"
#include "impeller/renderer/backend/vulkan/test/mock_vulkan.h"
#include "impeller/renderer/backend/vulkan/texture_vk.h"

namespace impeller {
namespace testing {
namespace {

// Qualcomm devices generate corrupt mip levels; ARM ones do not.
constexpr std::string_view kAdrenoName = "Adreno (TM) 750";
constexpr std::string_view kMaliName = "Mali-G51";
constexpr uint32_t kQualcommVendorID = 0x168C;
constexpr uint32_t kARMVendorID = 0x13B5;

void SetAdrenoProperties(VkPhysicalDevice device,
                         VkPhysicalDeviceProperties* prop) {
  prop->vendorID = kQualcommVendorID;
  kAdrenoName.copy(prop->deviceName, kAdrenoName.size());
  prop->deviceType = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
}

void SetMaliProperties(VkPhysicalDevice device,
                       VkPhysicalDeviceProperties* prop) {
  prop->vendorID = kARMVendorID;
  kMaliName.copy(prop->deviceName, kMaliName.size());
  prop->deviceType = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
}

std::shared_ptr<ContextVK> MakeContext(
    void (*set_properties)(VkPhysicalDevice, VkPhysicalDeviceProperties*)) {
  MockVulkanContextBuilder builder;
  builder.SetPhysicalPropertiesCallback(set_properties);
  return builder.Build();
}

std::shared_ptr<Texture> MakeTexture(const std::shared_ptr<ContextVK>& context,
                                     size_t mip_count) {
  return context->GetResourceAllocator()->CreateTexture(TextureDescriptor{
      .storage_mode = StorageMode::kDevicePrivate,
      .format = PixelFormat::kR8G8B8A8UNormInt,
      .size = {8, 8},
      .mip_count = mip_count,
      .usage = TextureUsage::kShaderRead,
  });
}

}  // namespace

TEST(TextureVKTest, SampledViewDropsMipsGeneratedByABrokenDriver) {
  auto const context = MakeContext(SetAdrenoProperties);
  auto texture = MakeTexture(context, /*mip_count=*/4u);
  ASSERT_TRUE(texture);
  auto& texture_vk = TextureVK::Cast(*texture);

  // Levels uploaded by hand are trustworthy, so the full view is used.
  EXPECT_EQ(texture_vk.GetSampledImageView(), texture_vk.GetImageView());

  // Levels this driver generated are not.
  texture_vk.SetMipMapGenerated();
  EXPECT_NE(texture_vk.GetSampledImageView(), texture_vk.GetImageView());
  EXPECT_NE(texture_vk.GetSampledImageView(), vk::ImageView{});
}

TEST(TextureVKTest, SampledViewKeepsMipsOnAWorkingDriver) {
  auto const context = MakeContext(SetMaliProperties);
  auto texture = MakeTexture(context, /*mip_count=*/4u);
  ASSERT_TRUE(texture);
  auto& texture_vk = TextureVK::Cast(*texture);

  texture_vk.SetMipMapGenerated();
  EXPECT_EQ(texture_vk.GetSampledImageView(), texture_vk.GetImageView());
}

TEST(TextureVKTest, SampledViewIsTheFullViewWithoutMips) {
  auto const context = MakeContext(SetAdrenoProperties);
  auto texture = MakeTexture(context, /*mip_count=*/1u);
  ASSERT_TRUE(texture);
  auto& texture_vk = TextureVK::Cast(*texture);

  texture_vk.SetMipMapGenerated();
  EXPECT_EQ(texture_vk.GetSampledImageView(), texture_vk.GetImageView());
}

TEST(TextureVKTest, SetLayoutPropagatesQueueFamilyIndices) {
  auto const context = MakeContext(SetMaliProperties);
  auto texture = MakeTexture(context, /*mip_count=*/1u);
  ASSERT_TRUE(texture);
  auto& texture_vk = TextureVK::Cast(*texture);

  auto buffer = context->CreateCommandBuffer();
  ASSERT_TRUE(buffer);
  auto& buffer_vk = CommandBufferVK::Cast(*buffer);

  BarrierVK barrier;
  barrier.cmd_buffer = buffer_vk.GetCommandBuffer();
  barrier.src_access = vk::AccessFlagBits::eColorAttachmentWrite;
  barrier.src_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  barrier.dst_access = vk::AccessFlagBits::eShaderRead;
  barrier.dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
  barrier.new_layout = vk::ImageLayout::eShaderReadOnlyOptimal;
  barrier.src_queue_family_index = VK_QUEUE_FAMILY_EXTERNAL;
  barrier.dst_queue_family_index =
      context->GetGraphicsQueue()->GetIndex().family;

  ASSERT_TRUE(texture_vk.SetLayout(barrier));

  auto const& barriers = GetImageMemoryBarriers(buffer_vk.GetCommandBuffer());
  ASSERT_FALSE(barriers.empty());
  EXPECT_EQ(barriers.back().srcQueueFamilyIndex, VK_QUEUE_FAMILY_EXTERNAL);
  EXPECT_EQ(barriers.back().dstQueueFamilyIndex,
            context->GetGraphicsQueue()->GetIndex().family);
}

}  // namespace testing
}  // namespace impeller
