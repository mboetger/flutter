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

}  // namespace testing
}  // namespace flutter

// NOLINTEND(clang-analyzer-core.StackAddressEscape)
