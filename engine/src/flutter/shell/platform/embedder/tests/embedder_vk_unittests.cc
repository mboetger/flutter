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
#include "flutter/shell/platform/embedder/tests/embedder_assertions.h"
#include "flutter/shell/platform/embedder/tests/embedder_config_builder.h"
#include "flutter/shell/platform/embedder/tests/embedder_test.h"
#include "flutter/shell/platform/embedder/tests/embedder_test_context_vulkan.h"
#include "flutter/shell/platform/embedder/tests/embedder_unittests_util.h"
#include "flutter/testing/testing.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkTypes.h"

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

TEST_F(EmbedderTest, CanRenderWithImpellerVulkan) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();

  EmbedderConfigBuilder builder(context);

  builder.AddCommandLineArgument("--enable-impeller");
  builder.SetDartEntrypoint("render_impeller_test");
  builder.SetSurface(DlISize(800, 600));

  auto rendered_scene = context.GetNextSceneImage();

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  // Send a window metrics event so frames may be scheduled.
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 800;
  event.height = 600;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);

  ASSERT_TRUE(ImageMatchesFixture("impeller_test.png", rendered_scene));
}

TEST_F(EmbedderTest, CanRenderWithImpellerVulkanCompositor) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();

  EmbedderConfigBuilder builder(context);

  builder.AddCommandLineArgument("--enable-impeller");
  builder.SetDartEntrypoint("render_impeller_test");
  builder.SetSurface(DlISize(800, 600));
  builder.SetCompositor();

  builder.SetRenderTargetType(
      EmbedderTestBackingStoreProducer::RenderTargetType::kVulkanImage);

  auto rendered_scene = context.GetNextSceneImage();

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  // Send a window metrics event so frames may be scheduled.
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 800;
  event.height = 600;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);

  ASSERT_TRUE(ImageMatchesFixture("impeller_test.png", rendered_scene));
}

TEST_F(EmbedderTest, CanRenderTextWithImpellerAndCompositorVulkan) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();

  EmbedderConfigBuilder builder(context);

  builder.AddCommandLineArgument("--enable-impeller");
  builder.SetDartEntrypoint("render_impeller_text_test");
  builder.SetSurface(DlISize(800, 600));
  builder.SetCompositor();

  builder.SetRenderTargetType(
      EmbedderTestBackingStoreProducer::RenderTargetType::kVulkanImage);

  auto rendered_scene = context.GetNextSceneImage();

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  // Send a window metrics event so frames may be scheduled.
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 800;
  event.height = 600;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);

  ASSERT_TRUE(
      ImageMatchesFixture("impeller_text_test.png", rendered_scene, 500));
}

TEST_F(EmbedderTest, CanRenderPlatformViewWithImpellerVulkan) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();

  EmbedderConfigBuilder builder(context);
  builder.AddCommandLineArgument("--enable-impeller");
  builder.SetSurface(DlISize(800, 600));
  builder.SetCompositor();
  builder.SetDartEntrypoint("render_impeller_platform_view");

  builder.SetRenderTargetType(
      EmbedderTestBackingStoreProducer::RenderTargetType::kVulkanImage);

  auto rendered_scene = context.GetNextSceneImage();

  fml::CountDownLatch latch(3);

  context.AddFfiNativeCallback(
      "SignalNativeTest", CREATE_FFI_LAMBDA([&latch]() { latch.CountDown(); }));

  context.GetCompositor().SetPlatformViewRendererCallback(
      [&](const FlutterLayer& layer,
          GrDirectContext* context) -> sk_sp<SkImage> {
        auto surface = CreateRenderSurface(layer, context);
        auto canvas = surface->getCanvas();
        FML_CHECK(canvas != nullptr);

        switch (layer.platform_view->identifier) {
          case 1: {
            SkPaint paint;
            paint.setColor(SK_ColorGREEN);
            const auto& rect =
                SkRect::MakeWH(layer.size.width, layer.size.height);
            canvas->drawRect(rect, paint);
            latch.CountDown();
          } break;
          default:
            FML_CHECK(false)
                << "Test was asked to composite an unknown platform view.";
        }

        return surface->makeImageSnapshot();
      });

  auto engine = builder.LaunchEngine();

  // Send a window metrics event so frames may be scheduled.
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 800;
  event.height = 600;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);
  ASSERT_TRUE(engine.is_valid());

  latch.Wait();

  ASSERT_TRUE(
      ImageMatchesFixture("impeller_render_platform_view.png", rendered_scene));
}

TEST_F(EmbedderTest, CreateInvalidBackingStoreVulkanImage) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();
  EmbedderConfigBuilder builder(context);
  builder.AddCommandLineArgument("--enable-impeller");
  builder.SetSurface(DlISize(800, 600));
  builder.SetCompositor();
  builder.SetRenderTargetType(
      EmbedderTestBackingStoreProducer::RenderTargetType::kVulkanImage);
  builder.SetDartEntrypoint("invalid_backingstore");

  static bool collected = false;
  static fml::AutoResetWaitableEvent* s_collect_latch = nullptr;
  fml::AutoResetWaitableEvent collect_latch;
  collected = false;
  s_collect_latch = &collect_latch;

  builder.GetCompositor().create_backing_store_callback =
      [](const FlutterBackingStoreConfig* config,
         FlutterBackingStore* backing_store_out, void* user_data) -> bool {
    backing_store_out->type = kFlutterBackingStoreTypeVulkan;
    backing_store_out->user_data = nullptr;
    backing_store_out->vulkan.struct_size = sizeof(FlutterVulkanBackingStore);
    backing_store_out->vulkan.image = nullptr;
    return true;
  };

  builder.GetCompositor().collect_backing_store_callback =
      [](const FlutterBackingStore* backing_store, void* user_data) -> bool {
    collected = true;
    if (s_collect_latch) {
      s_collect_latch->Signal();
    }
    return reinterpret_cast<EmbedderTestCompositor*>(user_data)
        ->CollectBackingStore(backing_store);
  };

  fml::AutoResetWaitableEvent latch;
  context.AddFfiNativeCallback(
      "SignalNativeTest", CREATE_FFI_LAMBDA([&latch]() { latch.Signal(); }));

  auto engine = builder.LaunchEngine();

  // Send a window metrics event so frames may be scheduled.
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 800;
  event.height = 600;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);
  ASSERT_TRUE(engine.is_valid());
  latch.Wait();
  collect_latch.Wait();
  EXPECT_TRUE(collected);
  s_collect_latch = nullptr;
}

TEST_F(EmbedderTest, CanRenderExternalTextureVulkanSkia) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();

  DlISize texture_size(800, 600);
  int64_t texture_id = 1;

  auto test_vk_context = context.GetTestVulkanContext();
  auto optional_image = test_vk_context->CreateImage(texture_size);
  ASSERT_TRUE(optional_image.has_value());
  auto test_image =
      std::make_shared<TestVulkanImage>(std::move(*optional_image));

  GrVkImageInfo image_info = {
      .fImage = test_image->GetImage(),
      .fImageTiling = VK_IMAGE_TILING_OPTIMAL,
      .fImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .fFormat = VK_FORMAT_R8G8B8A8_UNORM,
      .fImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                          VK_IMAGE_USAGE_SAMPLED_BIT,
      .fSampleCount = 1,
      .fLevelCount = 1,
  };
  auto backend_texture = GrBackendTextures::MakeVk(
      texture_size.width, texture_size.height, image_info);

  SkSurfaceProps surface_properties(0, kUnknown_SkPixelGeometry);
  auto gr_context = test_vk_context->GetGrDirectContext();
  sk_sp<SkSurface> surface = SkSurfaces::WrapBackendTexture(
      gr_context.get(), backend_texture, kTopLeft_GrSurfaceOrigin, 1,
      kRGBA_8888_SkColorType, SkColorSpace::MakeSRGB(), &surface_properties);
  ASSERT_TRUE(surface);
  surface->getCanvas()->clear(SK_ColorRED);
  gr_context->flushAndSubmit();

  context.SetExternalTextureCallback(
      [test_image, texture_size](int64_t id, size_t w, size_t h,
                                 FlutterVulkanImage* output) -> bool {
        output->struct_size = sizeof(FlutterVulkanImage);
        output->image = reinterpret_cast<uint64_t>(test_image->GetImage());
        output->format = VK_FORMAT_R8G8B8A8_UNORM;
        output->width = texture_size.width;
        output->height = texture_size.height;
        output->user_data = nullptr;
        output->destruction_callback = nullptr;
        return true;
      });

  EmbedderConfigBuilder builder(context);
  builder.SetDartEntrypoint("render_texture");
  builder.SetSurface(texture_size);

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  ASSERT_EQ(FlutterEngineRegisterExternalTexture(engine.get(), texture_id),
            kSuccess);

  auto rendered_scene = context.GetNextSceneImage();

  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = texture_size.width;
  event.height = texture_size.height;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);

  ASSERT_TRUE(
      ImageMatchesFixture("external_texture_vulkan.png", rendered_scene));
}

TEST_F(EmbedderTest, CanRenderExternalTextureVulkanImpeller) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();

  DlISize texture_size(800, 600);
  int64_t texture_id = 1;

  auto test_vk_context = context.GetTestVulkanContext();
  auto optional_image = test_vk_context->CreateImage(texture_size);
  ASSERT_TRUE(optional_image.has_value());
  auto test_image =
      std::make_shared<TestVulkanImage>(std::move(*optional_image));

  GrVkImageInfo image_info = {
      .fImage = test_image->GetImage(),
      .fImageTiling = VK_IMAGE_TILING_OPTIMAL,
      .fImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .fFormat = VK_FORMAT_R8G8B8A8_UNORM,
      .fImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                          VK_IMAGE_USAGE_SAMPLED_BIT,
      .fSampleCount = 1,
      .fLevelCount = 1,
  };
  auto backend_texture = GrBackendTextures::MakeVk(
      texture_size.width, texture_size.height, image_info);

  SkSurfaceProps surface_properties(0, kUnknown_SkPixelGeometry);
  auto gr_context = test_vk_context->GetGrDirectContext();
  sk_sp<SkSurface> surface = SkSurfaces::WrapBackendTexture(
      gr_context.get(), backend_texture, kTopLeft_GrSurfaceOrigin, 1,
      kRGBA_8888_SkColorType, SkColorSpace::MakeSRGB(), &surface_properties);
  ASSERT_TRUE(surface);
  surface->getCanvas()->clear(SK_ColorRED);
  gr_context->flushAndSubmit();

  context.SetExternalTextureCallback(
      [test_image, texture_size](int64_t id, size_t w, size_t h,
                                 FlutterVulkanImage* output) -> bool {
        output->struct_size = sizeof(FlutterVulkanImage);
        output->image = reinterpret_cast<uint64_t>(test_image->GetImage());
        output->format = VK_FORMAT_R8G8B8A8_UNORM;
        output->width = texture_size.width;
        output->height = texture_size.height;
        output->user_data = nullptr;
        output->destruction_callback = nullptr;
        return true;
      });

  EmbedderConfigBuilder builder(context);
  builder.AddCommandLineArgument("--enable-impeller");
  builder.SetDartEntrypoint("render_texture");
  builder.SetSurface(texture_size);

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  ASSERT_EQ(FlutterEngineRegisterExternalTexture(engine.get(), texture_id),
            kSuccess);

  auto rendered_scene = context.GetNextSceneImage();

  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = texture_size.width;
  event.height = texture_size.height;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);

  ASSERT_TRUE(
      ImageMatchesFixture("external_texture_vulkan.png", rendered_scene));
}

TEST_F(EmbedderTest, ExternalTextureVulkanDestructionCallbackInvoked) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();

  DlISize texture_size(800, 600);
  int64_t texture_id = 1;

  auto test_vk_context = context.GetTestVulkanContext();
  auto optional_image = test_vk_context->CreateImage(texture_size);
  ASSERT_TRUE(optional_image.has_value());
  auto test_image =
      std::make_shared<TestVulkanImage>(std::move(*optional_image));

  GrVkImageInfo image_info = {
      .fImage = test_image->GetImage(),
      .fImageTiling = VK_IMAGE_TILING_OPTIMAL,
      .fImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .fFormat = VK_FORMAT_R8G8B8A8_UNORM,
      .fImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                          VK_IMAGE_USAGE_SAMPLED_BIT,
      .fSampleCount = 1,
      .fLevelCount = 1,
  };
  auto backend_texture = GrBackendTextures::MakeVk(
      texture_size.width, texture_size.height, image_info);

  SkSurfaceProps surface_properties(0, kUnknown_SkPixelGeometry);
  auto gr_context = test_vk_context->GetGrDirectContext();
  sk_sp<SkSurface> surface = SkSurfaces::WrapBackendTexture(
      gr_context.get(), backend_texture, kTopLeft_GrSurfaceOrigin, 1,
      kRGBA_8888_SkColorType, SkColorSpace::MakeSRGB(), &surface_properties);
  ASSERT_TRUE(surface);
  surface->getCanvas()->clear(SK_ColorRED);
  gr_context->flushAndSubmit();

  std::atomic<int> destruction_count{0};

  context.SetExternalTextureCallback(
      [test_image, texture_size, &destruction_count](
          int64_t id, size_t w, size_t h, FlutterVulkanImage* output) -> bool {
        output->struct_size = sizeof(FlutterVulkanImage);
        output->image = reinterpret_cast<uint64_t>(test_image->GetImage());
        output->format = VK_FORMAT_R8G8B8A8_UNORM;
        output->width = texture_size.width;
        output->height = texture_size.height;
        output->user_data = &destruction_count;
        output->destruction_callback = [](void* user_data) {
          reinterpret_cast<std::atomic<int>*>(user_data)->fetch_add(1);
        };
        return true;
      });

  EmbedderConfigBuilder builder(context);
  builder.AddCommandLineArgument("--enable-impeller");
  builder.SetDartEntrypoint("render_texture");
  builder.SetSurface(texture_size);

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  ASSERT_EQ(FlutterEngineRegisterExternalTexture(engine.get(), texture_id),
            kSuccess);

  auto rendered_scene = context.GetNextSceneImage();

  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = texture_size.width;
  event.height = texture_size.height;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);

  ASSERT_TRUE(
      ImageMatchesFixture("external_texture_vulkan.png", rendered_scene));

  ASSERT_EQ(FlutterEngineUnregisterExternalTexture(engine.get(), texture_id),
            kSuccess);
  engine.reset();

  EXPECT_GE(destruction_count.load(), 1);
}

TEST_F(EmbedderTest, ExternalTextureVulkanDestructionCallbackInvokedSkia) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();

  DlISize texture_size(800, 600);
  int64_t texture_id = 1;

  auto test_vk_context = context.GetTestVulkanContext();
  auto optional_image = test_vk_context->CreateImage(texture_size);
  ASSERT_TRUE(optional_image.has_value());
  auto test_image =
      std::make_shared<TestVulkanImage>(std::move(*optional_image));

  GrVkImageInfo image_info = {
      .fImage = test_image->GetImage(),
      .fImageTiling = VK_IMAGE_TILING_OPTIMAL,
      .fImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .fFormat = VK_FORMAT_R8G8B8A8_UNORM,
      .fImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                          VK_IMAGE_USAGE_SAMPLED_BIT,
      .fSampleCount = 1,
      .fLevelCount = 1,
  };
  auto backend_texture = GrBackendTextures::MakeVk(
      texture_size.width, texture_size.height, image_info);

  SkSurfaceProps surface_properties(0, kUnknown_SkPixelGeometry);
  auto gr_context = test_vk_context->GetGrDirectContext();
  sk_sp<SkSurface> surface = SkSurfaces::WrapBackendTexture(
      gr_context.get(), backend_texture, kTopLeft_GrSurfaceOrigin, 1,
      kRGBA_8888_SkColorType, SkColorSpace::MakeSRGB(), &surface_properties);
  ASSERT_TRUE(surface);
  surface->getCanvas()->clear(SK_ColorRED);
  gr_context->flushAndSubmit();

  std::atomic<int> destruction_count{0};

  context.SetExternalTextureCallback(
      [test_image, texture_size, &destruction_count](
          int64_t id, size_t w, size_t h, FlutterVulkanImage* output) -> bool {
        output->struct_size = sizeof(FlutterVulkanImage);
        output->image = reinterpret_cast<uint64_t>(test_image->GetImage());
        output->format = VK_FORMAT_R8G8B8A8_UNORM;
        output->width = texture_size.width;
        output->height = texture_size.height;
        output->user_data = &destruction_count;
        output->destruction_callback = [](void* user_data) {
          reinterpret_cast<std::atomic<int>*>(user_data)->fetch_add(1);
        };
        return true;
      });

  EmbedderConfigBuilder builder(context);
  builder.SetDartEntrypoint("render_texture");
  builder.SetSurface(texture_size);

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  ASSERT_EQ(FlutterEngineRegisterExternalTexture(engine.get(), texture_id),
            kSuccess);

  auto rendered_scene = context.GetNextSceneImage();

  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = texture_size.width;
  event.height = texture_size.height;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);

  ASSERT_TRUE(
      ImageMatchesFixture("external_texture_vulkan.png", rendered_scene));

  ASSERT_EQ(FlutterEngineUnregisterExternalTexture(engine.get(), texture_id),
            kSuccess);
  engine.reset();

  EXPECT_GE(destruction_count.load(), 1);
}

TEST_F(EmbedderTest, UnsupportedPixelFormatExternalTextureVulkan) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();

  DlISize texture_size(800, 600);
  int64_t texture_id = 1;

  std::atomic<int> destruction_count{0};

  context.SetExternalTextureCallback(
      [texture_size, &destruction_count](int64_t id, size_t w, size_t h,
                                         FlutterVulkanImage* output) -> bool {
        output->struct_size = sizeof(FlutterVulkanImage);
        output->image = 0x1234;  // Non-null handle
        output->format = VK_FORMAT_UNDEFINED;
        output->width = texture_size.width;
        output->height = texture_size.height;
        output->user_data = &destruction_count;
        output->destruction_callback = [](void* user_data) {
          reinterpret_cast<std::atomic<int>*>(user_data)->fetch_add(1);
        };
        return true;
      });

  EmbedderConfigBuilder builder(context);
  builder.AddCommandLineArgument("--enable-impeller");
  builder.SetDartEntrypoint("render_texture");
  builder.SetSurface(texture_size);

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  ASSERT_EQ(FlutterEngineRegisterExternalTexture(engine.get(), texture_id),
            kSuccess);

  auto rendered_scene = context.GetNextSceneImage();

  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = texture_size.width;
  event.height = texture_size.height;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);

  // Wait for frame to be presented so external texture callbacks execute.
  rendered_scene.get();

  ASSERT_EQ(FlutterEngineUnregisterExternalTexture(engine.get(), texture_id),
            kSuccess);
  engine.reset();

  EXPECT_GE(destruction_count.load(), 1);
}

TEST_F(EmbedderTest, NullAndZeroDimensionExternalTextureVulkan) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();

  DlISize texture_size(800, 600);
  int64_t texture_id = 1;

  std::atomic<int> destruction_count{0};

  context.SetExternalTextureCallback(
      [&destruction_count](int64_t id, size_t w, size_t h,
                           FlutterVulkanImage* output) -> bool {
        output->struct_size = sizeof(FlutterVulkanImage);
        output->image = 0;  // Null handle
        output->format = VK_FORMAT_R8G8B8A8_UNORM;
        output->width = 0;
        output->height = 0;
        output->user_data = &destruction_count;
        output->destruction_callback = [](void* user_data) {
          reinterpret_cast<std::atomic<int>*>(user_data)->fetch_add(1);
        };
        return true;
      });

  EmbedderConfigBuilder builder(context);
  builder.AddCommandLineArgument("--enable-impeller");
  builder.SetDartEntrypoint("render_texture");
  builder.SetSurface(texture_size);

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  ASSERT_EQ(FlutterEngineRegisterExternalTexture(engine.get(), texture_id),
            kSuccess);

  auto rendered_scene = context.GetNextSceneImage();

  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = texture_size.width;
  event.height = texture_size.height;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);

  // Wait for frame to be presented so external texture callbacks execute.
  rendered_scene.get();

  ASSERT_EQ(FlutterEngineUnregisterExternalTexture(engine.get(), texture_id),
            kSuccess);
  engine.reset();

  EXPECT_GE(destruction_count.load(), 1);
}

TEST_F(EmbedderTest, InvalidExternalTextureVulkanHandling) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();

  DlISize texture_size(800, 600);
  int64_t texture_id = 1;

  context.SetExternalTextureCallback(
      [](int64_t id, size_t w, size_t h, FlutterVulkanImage* output) -> bool {
        return false;
      });

  EmbedderConfigBuilder builder(context);
  builder.AddCommandLineArgument("--enable-impeller");
  builder.SetDartEntrypoint("render_texture");
  builder.SetSurface(texture_size);

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  ASSERT_EQ(FlutterEngineRegisterExternalTexture(engine.get(), texture_id),
            kSuccess);

  auto rendered_scene = context.GetNextSceneImage();

  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = texture_size.width;
  event.height = texture_size.height;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);

  ASSERT_TRUE(engine.is_valid());
}

TEST_F(EmbedderTest, VulkanSetupCallbackInvoked) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();

  static std::atomic<int> s_setup_callback_count{0};
  static std::atomic<int> s_teardown_callback_count{0};
  static std::atomic<void*> s_setup_user_data{nullptr};
  static std::atomic<void*> s_teardown_user_data{nullptr};
  static std::thread::id s_setup_thread_id;
  s_setup_callback_count = 0;
  s_teardown_callback_count = 0;
  s_setup_user_data = nullptr;
  s_teardown_user_data = nullptr;

  context.GetRendererConfig().vulkan.setup_callback =
      [](void* user_data) -> bool {
    s_setup_user_data.store(user_data);
    s_setup_thread_id = std::this_thread::get_id();
    s_setup_callback_count.fetch_add(1);
    return true;
  };
  context.GetRendererConfig().vulkan.teardown_callback = [](void* user_data) {
    s_teardown_user_data.store(user_data);
    s_teardown_callback_count.fetch_add(1);
  };

  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  EXPECT_GE(s_setup_callback_count.load(), 1);
  EXPECT_EQ(s_setup_user_data.load(), &context);
  EXPECT_EQ(s_teardown_callback_count.load(), 0);

  fml::AutoResetWaitableEvent latch;
  ToEmbedderEngine(engine.get())
      ->GetShell()
      .GetTaskRunners()
      .GetRasterTaskRunner()
      ->PostTask([&]() {
        EXPECT_EQ(s_setup_thread_id, std::this_thread::get_id());
        latch.Signal();
      });
  latch.Wait();

  engine.reset();
  EXPECT_GE(s_teardown_callback_count.load(), 1);
  EXPECT_EQ(s_teardown_user_data.load(), &context);
}

TEST_F(EmbedderTest, VulkanSetupCallbackFailureAbortsSurface) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();

  static std::atomic<int> s_setup_callback_count{0};
  static std::atomic<int> s_teardown_callback_count{0};
  static std::atomic<void*> s_setup_user_data{nullptr};
  s_setup_callback_count = 0;
  s_teardown_callback_count = 0;
  s_setup_user_data = nullptr;

  context.GetRendererConfig().vulkan.setup_callback =
      [](void* user_data) -> bool {
    s_setup_user_data.store(user_data);
    s_setup_callback_count.fetch_add(1);
    return false;
  };
  context.GetRendererConfig().vulkan.teardown_callback = [](void* user_data) {
    s_teardown_callback_count.fetch_add(1);
  };

  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());
  EXPECT_GE(s_setup_callback_count.load(), 1);
  EXPECT_EQ(s_setup_user_data.load(), &context);

  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 1;
  event.height = 1;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);

  fml::AutoResetWaitableEvent latch;
  ToEmbedderEngine(engine.get())
      ->GetShell()
      .GetTaskRunners()
      .GetRasterTaskRunner()
      ->PostTask([&]() { latch.Signal(); });
  latch.Wait();

  EXPECT_EQ(context.GetSurfacePresentCount(), 0u);

  engine.reset();
  EXPECT_EQ(s_teardown_callback_count.load(), 0);
}

TEST_F(EmbedderTest, VulkanImpellerSetupCallbackInvoked) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();

  static std::atomic<int> s_setup_callback_count{0};
  static std::atomic<int> s_teardown_callback_count{0};
  static std::atomic<void*> s_setup_user_data{nullptr};
  static std::atomic<void*> s_teardown_user_data{nullptr};
  static std::thread::id s_setup_thread_id;
  s_setup_callback_count = 0;
  s_teardown_callback_count = 0;
  s_setup_user_data = nullptr;
  s_teardown_user_data = nullptr;

  context.GetRendererConfig().vulkan.setup_callback =
      [](void* user_data) -> bool {
    s_setup_user_data.store(user_data);
    s_setup_thread_id = std::this_thread::get_id();
    s_setup_callback_count.fetch_add(1);
    return true;
  };
  context.GetRendererConfig().vulkan.teardown_callback = [](void* user_data) {
    s_teardown_user_data.store(user_data);
    s_teardown_callback_count.fetch_add(1);
  };

  EmbedderConfigBuilder builder(context);
  builder.AddCommandLineArgument("--enable-impeller");
  builder.SetSurface(DlISize(1, 1));
  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  EXPECT_GE(s_setup_callback_count.load(), 1);
  EXPECT_EQ(s_setup_user_data.load(), &context);
  EXPECT_EQ(s_teardown_callback_count.load(), 0);

  fml::AutoResetWaitableEvent latch;
  ToEmbedderEngine(engine.get())
      ->GetShell()
      .GetTaskRunners()
      .GetRasterTaskRunner()
      ->PostTask([&]() {
        EXPECT_EQ(s_setup_thread_id, std::this_thread::get_id());
        latch.Signal();
      });
  latch.Wait();

  engine.reset();
  EXPECT_GE(s_teardown_callback_count.load(), 1);
  EXPECT_EQ(s_teardown_user_data.load(), &context);
}

TEST_F(EmbedderTest, VulkanImpellerSetupCallbackFailureAbortsSurface) {
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();

  static std::atomic<int> s_setup_callback_count{0};
  static std::atomic<int> s_teardown_callback_count{0};
  static std::atomic<void*> s_setup_user_data{nullptr};
  s_setup_callback_count = 0;
  s_teardown_callback_count = 0;
  s_setup_user_data = nullptr;

  context.GetRendererConfig().vulkan.setup_callback =
      [](void* user_data) -> bool {
    s_setup_user_data.store(user_data);
    s_setup_callback_count.fetch_add(1);
    return false;
  };
  context.GetRendererConfig().vulkan.teardown_callback = [](void* user_data) {
    s_teardown_callback_count.fetch_add(1);
  };

  EmbedderConfigBuilder builder(context);
  builder.AddCommandLineArgument("--enable-impeller");
  builder.SetSurface(DlISize(1, 1));
  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());
  EXPECT_GE(s_setup_callback_count.load(), 1);
  EXPECT_EQ(s_setup_user_data.load(), &context);

  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 1;
  event.height = 1;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);

  fml::AutoResetWaitableEvent latch;
  ToEmbedderEngine(engine.get())
      ->GetShell()
      .GetTaskRunners()
      .GetRasterTaskRunner()
      ->PostTask([&]() { latch.Signal(); });
  latch.Wait();

  EXPECT_EQ(context.GetSurfacePresentCount(), 0u);

  engine.reset();
  EXPECT_EQ(s_teardown_callback_count.load(), 0);
}

}  // namespace testing
}  // namespace flutter

// NOLINTEND(clang-analyzer-core.StackAddressEscape)
