// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "embedder.h"
#include "embedder_engine.h"
#include "flutter/fml/synchronization/count_down_latch.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/shell/platform/embedder/platform_view_embedder.h"
#include "flutter/shell/platform/embedder/tests/embedder_config_builder.h"
#include "flutter/shell/platform/embedder/tests/embedder_test.h"
#include "flutter/shell/platform/embedder/tests/embedder_test_context_vulkan.h"
#include "flutter/shell/platform/embedder/tests/embedder_unittests_util.h"
#include "flutter/testing/testing.h"

// CREATE_FFI_LAMBDA is leaky by design
// NOLINTBEGIN(clang-analyzer-core.StackAddressEscape)

namespace flutter {
namespace testing {

using EmbedderTest = testing::EmbedderTest;

////////////////////////////////////////////////////////////////////////////////
// Notice: Other Vulkan unit tests exist in embedder_gl_unittests.cc.
//         See https://github.com/flutter/flutter/issues/134322
////////////////////////////////////////////////////////////////////////////////

namespace {

struct VulkanProcInfo {
  decltype(vkGetInstanceProcAddr)* get_instance_proc_addr = nullptr;
  decltype(vkGetDeviceProcAddr)* get_device_proc_addr = nullptr;
  decltype(vkQueueSubmit)* queue_submit_proc_addr = nullptr;
  bool did_call_queue_submit = false;
};

static_assert(std::is_trivially_destructible_v<VulkanProcInfo>);

VulkanProcInfo g_vulkan_proc_info;

VkResult QueueSubmit(VkQueue queue,
                     uint32_t submitCount,
                     const VkSubmitInfo* pSubmits,
                     VkFence fence) {
  FML_DCHECK(g_vulkan_proc_info.queue_submit_proc_addr != nullptr);
  g_vulkan_proc_info.did_call_queue_submit = true;
  return g_vulkan_proc_info.queue_submit_proc_addr(queue, submitCount, pSubmits,
                                                   fence);
}

template <size_t N>
int StrcmpFixed(const char* str1, const char (&str2)[N]) {
  return strncmp(str1, str2, N - 1);
}

PFN_vkVoidFunction GetDeviceProcAddr(VkDevice device, const char* pName) {
  FML_DCHECK(g_vulkan_proc_info.get_device_proc_addr != nullptr);
  if (StrcmpFixed(pName, "vkQueueSubmit") == 0) {
    g_vulkan_proc_info.queue_submit_proc_addr =
        reinterpret_cast<decltype(vkQueueSubmit)*>(
            g_vulkan_proc_info.get_device_proc_addr(device, pName));
    return reinterpret_cast<PFN_vkVoidFunction>(QueueSubmit);
  }
  return g_vulkan_proc_info.get_device_proc_addr(device, pName);
}

PFN_vkVoidFunction GetInstanceProcAddr(VkInstance instance, const char* pName) {
  FML_DCHECK(g_vulkan_proc_info.get_instance_proc_addr != nullptr);
  if (StrcmpFixed(pName, "vkGetDeviceProcAddr") == 0) {
    g_vulkan_proc_info.get_device_proc_addr =
        reinterpret_cast<decltype(vkGetDeviceProcAddr)*>(
            g_vulkan_proc_info.get_instance_proc_addr(instance, pName));
    return reinterpret_cast<PFN_vkVoidFunction>(GetDeviceProcAddr);
  }
  return g_vulkan_proc_info.get_instance_proc_addr(instance, pName);
}

template <typename T, typename U>
struct CheckSameSignature : std::false_type {};

template <typename Ret, typename... Args>
struct CheckSameSignature<Ret(Args...), Ret(Args...)> : std::true_type {};

static_assert(CheckSameSignature<decltype(GetInstanceProcAddr),
                                 decltype(vkGetInstanceProcAddr)>::value);
static_assert(CheckSameSignature<decltype(GetDeviceProcAddr),
                                 decltype(vkGetDeviceProcAddr)>::value);
static_assert(
    CheckSameSignature<decltype(QueueSubmit), decltype(vkQueueSubmit)>::value);
}  // namespace

TEST_F(EmbedderTest, CanGetVulkanEmbedderContext) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();
  EmbedderConfigBuilder builder(context);
}

TEST_F(EmbedderTest, CanSwapOutVulkanCalls) {
  fml::AutoResetWaitableEvent latch;

  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();
  context.AddIsolateCreateCallback([&latch]() { latch.Signal(); });
  context.SetVulkanInstanceProcAddressCallback(
      [](void* user_data, FlutterVulkanInstanceHandle instance,
         const char* name) -> void* {
        if (StrcmpFixed(name, "vkGetInstanceProcAddr") == 0) {
          g_vulkan_proc_info.get_instance_proc_addr =
              reinterpret_cast<decltype(vkGetInstanceProcAddr)*>(
                  EmbedderTestContextVulkan::InstanceProcAddr(user_data,
                                                              instance, name));
          return reinterpret_cast<void*>(GetInstanceProcAddr);
        }
        return EmbedderTestContextVulkan::InstanceProcAddr(user_data, instance,
                                                           name);
      });

  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1024, 1024));
  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());
  // Wait for the root isolate to launch.
  latch.Wait();
  engine.reset();
  EXPECT_TRUE(g_vulkan_proc_info.did_call_queue_submit);
}

TEST_F(EmbedderTest, CanRenderWithVulkanCompositor) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();

  EmbedderConfigBuilder builder(context);
  // Viewport dimensions: 1024x1024.
  builder.SetSurface(DlISize(1024, 1024));
  builder.SetCompositor();
  builder.SetDartEntrypoint("render_implicit_view");
  builder.SetRenderTargetType(
      EmbedderTestBackingStoreProducer::RenderTargetType::kVulkanImage);

  fml::AutoResetWaitableEvent latch;
  context.GetCompositor().SetNextPresentCallback(
      [&](FlutterViewId view_id, const FlutterLayer** layers,
          size_t layers_count) {
        // Assert at least 1 layer composited.
        ASSERT_GE(layers_count, 1u);
        ASSERT_NE(layers[0]->backing_store, nullptr);
        EXPECT_EQ(layers[0]->backing_store->type,
                  kFlutterBackingStoreTypeVulkan);
        EXPECT_NE(layers[0]->backing_store->vulkan.image, nullptr);
        latch.Signal();
      });

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 1024;
  event.height = 1024;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);

  latch.Wait();
  engine.reset();
}

TEST_F(EmbedderTest, CanRenderWithImpellerVulkanCompositor) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();

  EmbedderConfigBuilder builder(context);
  // Viewport dimensions: 1024x1024.
  builder.SetSurface(DlISize(1024, 1024));
  builder.AddCommandLineArgument("--enable-impeller");
  builder.SetCompositor();
  builder.SetDartEntrypoint("render_implicit_view");
  builder.SetRenderTargetType(
      EmbedderTestBackingStoreProducer::RenderTargetType::kVulkanImage);

  fml::AutoResetWaitableEvent latch;
  context.GetCompositor().SetNextPresentCallback(
      [&](FlutterViewId view_id, const FlutterLayer** layers,
          size_t layers_count) {
        // Assert at least 1 layer composited.
        ASSERT_GE(layers_count, 1u);
        ASSERT_NE(layers[0]->backing_store, nullptr);
        EXPECT_EQ(layers[0]->backing_store->type,
                  kFlutterBackingStoreTypeVulkan);
        EXPECT_NE(layers[0]->backing_store->vulkan.image, nullptr);
        latch.Signal();
      });

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 1024;
  event.height = 1024;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);

  latch.Wait();
  engine.reset();
}

TEST_F(EmbedderTest, CanRegisterVulkanExternalTexture) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();

  std::optional<TestVulkanImage> test_image =
      context.CreateImage(DlISize(512, 512));
  ASSERT_TRUE(test_image.has_value());

  std::atomic<size_t> external_texture_frame_called = 0;
  std::atomic<size_t> destruction_called = 0;

  // External texture identifier: 1.
  constexpr int64_t kTextureId = 1;

  context.SetVulkanExternalTextureCallback(
      [&](int64_t texture_identifier, size_t width, size_t height,
          FlutterVulkanImage* image_out) -> bool {
        if (texture_identifier != kTextureId) {
          return false;
        }
        external_texture_frame_called++;
        image_out->struct_size = sizeof(FlutterVulkanImage);
        image_out->image = reinterpret_cast<uint64_t>(test_image->GetImage());
        image_out->format = VK_FORMAT_R8G8B8A8_UNORM;
        image_out->width = 512;
        image_out->height = 512;
        image_out->user_data = &destruction_called;
        image_out->destruction_callback = [](void* user_data) {
          auto* count = reinterpret_cast<std::atomic<size_t>*>(user_data);
          (*count)++;
        };
        return true;
      });

  EmbedderConfigBuilder builder(context);
  // Viewport dimensions: 1024x1024.
  builder.SetSurface(DlISize(1024, 1024));
  builder.SetDartEntrypoint("render_texture");

  auto scene_image = context.GetNextSceneImage();

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  ASSERT_EQ(FlutterEngineRegisterExternalTexture(engine.get(), kTextureId),
            kSuccess);

  ASSERT_EQ(
      FlutterEngineMarkExternalTextureFrameAvailable(engine.get(), kTextureId),
      kSuccess);

  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 1024;
  event.height = 1024;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);

  auto snapshot = scene_image.get();
  EXPECT_NE(snapshot, nullptr);

  EXPECT_GE(external_texture_frame_called.load(), 1u);

  ASSERT_EQ(FlutterEngineUnregisterExternalTexture(engine.get(), kTextureId),
            kSuccess);

  engine.reset();
  EXPECT_GE(destruction_called.load(), 1u);
}

TEST_F(EmbedderTest, CanRegisterImpellerVulkanExternalTexture) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();

  std::optional<TestVulkanImage> test_image =
      context.CreateImage(DlISize(512, 512));
  ASSERT_TRUE(test_image.has_value());

  std::atomic<size_t> external_texture_frame_called = 0;
  std::atomic<size_t> destruction_called = 0;

  // External texture identifier: 1.
  constexpr int64_t kTextureId = 1;

  context.SetVulkanExternalTextureCallback(
      [&](int64_t texture_identifier, size_t width, size_t height,
          FlutterVulkanImage* image_out) -> bool {
        if (texture_identifier != kTextureId) {
          return false;
        }
        external_texture_frame_called++;
        image_out->struct_size = sizeof(FlutterVulkanImage);
        image_out->image = reinterpret_cast<uint64_t>(test_image->GetImage());
        image_out->format = VK_FORMAT_R8G8B8A8_UNORM;
        image_out->width = 512;
        image_out->height = 512;
        image_out->user_data = &destruction_called;
        image_out->destruction_callback = [](void* user_data) {
          auto* count = reinterpret_cast<std::atomic<size_t>*>(user_data);
          (*count)++;
        };
        return true;
      });

  EmbedderConfigBuilder builder(context);
  // Viewport dimensions: 1024x1024.
  builder.SetSurface(DlISize(1024, 1024));
  builder.AddCommandLineArgument("--enable-impeller");
  builder.SetDartEntrypoint("render_texture");

  auto scene_image = context.GetNextSceneImage();

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  ASSERT_EQ(FlutterEngineRegisterExternalTexture(engine.get(), kTextureId),
            kSuccess);

  ASSERT_EQ(
      FlutterEngineMarkExternalTextureFrameAvailable(engine.get(), kTextureId),
      kSuccess);

  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 1024;
  event.height = 1024;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);

  auto snapshot = scene_image.get();
  EXPECT_NE(snapshot, nullptr);

  EXPECT_GE(external_texture_frame_called.load(), 1u);

  ASSERT_EQ(FlutterEngineUnregisterExternalTexture(engine.get(), kTextureId),
            kSuccess);

  engine.reset();
  EXPECT_GE(destruction_called.load(), 1u);
}

TEST_F(EmbedderTest, VulkanSetupCallbackInvokedOnRasterThread) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();
  EmbedderConfigBuilder builder(context);
  // Viewport dimensions: 800x600.
  builder.SetSurface(DlISize(800, 600));

  fml::AutoResetWaitableEvent setup_latch;
  static fml::AutoResetWaitableEvent* s_vk_setup_latch = nullptr;
  s_vk_setup_latch = &setup_latch;

  context.GetRendererConfig().vulkan.setup_callback =
      [](void* user_data) -> bool {
    if (s_vk_setup_latch) {
      s_vk_setup_latch->Signal();
    }
    return true;
  };

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  // Wait for the setup callback to be executed on the raster thread.
  setup_latch.Wait();
  s_vk_setup_latch = nullptr;
}

TEST_F(EmbedderTest, VulkanSetupCallbackBackwardsCompatibleWithOldStructSize) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();
  EmbedderConfigBuilder builder(context);
  // Viewport dimensions: 800x600.
  builder.SetSurface(DlISize(800, 600));

  // Set struct size to exclude setup_callback.
  context.GetRendererConfig().vulkan.struct_size =
      offsetof(FlutterVulkanRendererConfig, setup_callback);
  // Set a failing callback; since struct_size is old, it should never be
  // called.
  context.GetRendererConfig().vulkan.setup_callback =
      [](void* user_data) -> bool { return false; };

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());
}

TEST_F(EmbedderTest, VulkanSetupCallbackFailureInvalidatesSurface) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();
  EmbedderConfigBuilder builder(context);
  // Viewport dimensions: 800x600.
  builder.SetSurface(DlISize(800, 600));

  fml::AutoResetWaitableEvent setup_latch;
  static fml::AutoResetWaitableEvent* s_vk_fail_latch = nullptr;
  s_vk_fail_latch = &setup_latch;

  context.GetRendererConfig().vulkan.setup_callback =
      [](void* user_data) -> bool {
    if (s_vk_fail_latch) {
      s_vk_fail_latch->Signal();
    }
    // Return false to indicate setup failed.
    return false;
  };

  auto engine = builder.LaunchEngine();
  setup_latch.Wait();
  s_vk_fail_latch = nullptr;

  ASSERT_TRUE(engine.is_valid());
  auto* embedder_engine = reinterpret_cast<EmbedderEngine*>(engine.get());
  auto platform_view = embedder_engine->GetShell().GetPlatformView();
  ASSERT_TRUE(platform_view);
  auto* platform_view_embedder =
      static_cast<PlatformViewEmbedder*>(platform_view.get());
  ASSERT_TRUE(platform_view_embedder->GetEmbedderSurface());
  EXPECT_FALSE(platform_view_embedder->GetEmbedderSurface()->IsValid());
  EXPECT_EQ(platform_view->GetImpellerContext(), nullptr);
  EXPECT_EQ(platform_view_embedder->GetEmbedderSurface()->CreateGPUSurface(),
            nullptr);
}

}  // namespace testing
}  // namespace flutter

// NOLINTEND(clang-analyzer-core.StackAddressEscape)
