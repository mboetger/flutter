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
#include "flutter/display_list/dl_builder.h"
#include "flutter/display_list/skia/dl_sk_canvas.h"
#include "flutter/fml/synchronization/count_down_latch.h"
#include "flutter/shell/platform/embedder/embedder_external_texture_vulkan.h"
#include "flutter/shell/platform/embedder/tests/embedder_config_builder.h"
#include "flutter/shell/platform/embedder/tests/embedder_test.h"
#include "flutter/shell/platform/embedder/tests/embedder_test_context_vulkan.h"
#include "flutter/shell/platform/embedder/tests/embedder_unittests_util.h"
#include "flutter/testing/testing.h"
#include "third_party/skia/include/core/SkPaint.h"

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

  // Send a window metrics events so frames may be scheduled.
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 800;
  event.height = 600;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);

  ASSERT_TRUE(ImageMatchesFixture("impeller_test.png", rendered_scene));
}

TEST_F(EmbedderTest, ExternalTextureVulkanRefreshedTooOften) {
  auto& embedder_context = GetEmbedderContext<EmbedderTestContextVulkan>();
  auto render_surface = TestVulkanSurface::Create(
      *embedder_context.GetVulkanContext(), DlISize(100, 100));
  ASSERT_TRUE(render_surface && render_surface->IsValid());
  auto texture_surface = TestVulkanSurface::Create(
      *embedder_context.GetVulkanContext(), DlISize(100, 100));
  ASSERT_TRUE(texture_surface && texture_surface->IsValid());
  auto context = embedder_context.GetVulkanContext()->GetGrDirectContext();

  bool resolve_called = false;

  EmbedderExternalTextureVulkan::ExternalTextureCallback callback(
      [&](int64_t, size_t, size_t) {
        resolve_called = true;
        auto res = std::make_unique<FlutterVulkanImage>();
        res->struct_size = sizeof(FlutterVulkanImage);
        res->image = reinterpret_cast<uint64_t>(texture_surface->GetImage());
        res->format = VK_FORMAT_R8G8B8A8_UNORM;
        res->user_data = nullptr;
        res->destruction_callback = [](void*) {};
        res->width = 100;
        res->height = 100;
        return res;
      });
  EmbedderExternalTextureVulkan texture(1, callback);

  auto skia_surface = render_surface->GetSkSurface();
  DlSkCanvasAdapter canvas(skia_surface->getCanvas());

  Texture* texture_ = &texture;
  Texture::PaintContext ctx{
      .canvas = &canvas,
      .gr_context = context.get(),
  };
  texture_->Paint(ctx, DlRect::MakeXYWH(0, 0, 100, 100), false,
                  DlImageSampling::kLinear);

  EXPECT_TRUE(resolve_called);
  resolve_called = false;

  texture_->Paint(ctx, DlRect::MakeXYWH(0, 0, 100, 100), false,
                  DlImageSampling::kLinear);

  EXPECT_FALSE(resolve_called);

  texture_->MarkNewFrameAvailable();
  texture_->Paint(ctx, DlRect::MakeXYWH(0, 0, 100, 100), false,
                  DlImageSampling::kLinear);

  EXPECT_TRUE(resolve_called);

  context->flushAndSubmit(GrSyncCpu::kYes);
}

TEST_F(EmbedderTest, ExternalTextureVulkanNullContextDoesNotCrash) {
  EmbedderExternalTextureVulkan::ExternalTextureCallback callback(
      [](int64_t, size_t, size_t) {
        auto res = std::make_unique<FlutterVulkanImage>();
        res->struct_size = sizeof(FlutterVulkanImage);
        res->image = 1;
        res->format = VK_FORMAT_R8G8B8A8_UNORM;
        res->user_data = nullptr;
        res->destruction_callback = [](void*) {};
        res->width = 100;
        res->height = 100;
        return res;
      });
  EmbedderExternalTextureVulkan texture(1, callback);

  DisplayListBuilder builder;
  Texture::PaintContext ctx{
      .canvas = &builder,
      .gr_context = nullptr,
      .aiks_context = nullptr,
  };
  // Should not crash even when last_image_ is null and contexts are null.
  texture.Paint(ctx, DlRect::MakeXYWH(0, 0, 100, 100), false,
                DlImageSampling::kLinear);
}

TEST_F(EmbedderTest, RenderTextureWithImpellerVulkan) {
  constexpr int kWidth = 800;
  constexpr int kHeight = 600;
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();
  EmbedderConfigBuilder builder(context);

  builder.AddCommandLineArgument("--enable-impeller");
  builder.SetDartEntrypoint("render_texture_impeller_test");
  builder.SetSurface(DlISize(kWidth, kHeight));

  auto external_surface = TestVulkanSurface::Create(*context.GetVulkanContext(),
                                                    DlISize(kWidth, kHeight));
  ASSERT_TRUE(external_surface && external_surface->IsValid());
  auto sk_surface = external_surface->GetSkSurface();
  auto canvas = sk_surface->getCanvas();
  SkPaint top_paint;
  top_paint.setColor(SK_ColorRED);
  canvas->drawRect(SkRect::MakeXYWH(0, 0, kWidth, kHeight / 2), top_paint);
  SkPaint bottom_paint;
  bottom_paint.setColor(SK_ColorBLUE);
  canvas->drawRect(SkRect::MakeXYWH(0, kHeight / 2, kWidth, kHeight / 2),
                   bottom_paint);
  context.GetVulkanContext()->GetGrDirectContext()->flushAndSubmit();

  auto rendered_scene = context.GetNextSceneImage();
  context.SetExternalTextureCallback([&](int64_t texture_id, size_t width,
                                         size_t height,
                                         FlutterVulkanImage* output) -> bool {
    output->struct_size = sizeof(FlutterVulkanImage);
    output->image = reinterpret_cast<uint64_t>(external_surface->GetImage());
    output->format = VK_FORMAT_R8G8B8A8_UNORM;
    output->destruction_callback = nullptr;
    output->user_data = nullptr;
    output->width = width;
    output->height = height;
    return true;
  });

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  constexpr int texture_id = 1;
  ASSERT_EQ(FlutterEngineRegisterExternalTexture(engine.get(), texture_id),
            kSuccess);

  // Send a window metrics events so frames may be scheduled.
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = kWidth;
  event.height = kHeight;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);

  ASSERT_TRUE(
      ImageMatchesFixture("external_texture_impeller.png", rendered_scene));

  constexpr int kFrameCount = 5;
  for (int i = 0; i < kFrameCount; i++) {
    rendered_scene = context.GetNextSceneImage();
    ASSERT_EQ(FlutterEngineMarkExternalTextureFrameAvailable(engine.get(),
                                                             texture_id),
              kSuccess);
    ASSERT_TRUE(
        ImageMatchesFixture("external_texture_impeller.png", rendered_scene));
  }
}

TEST_F(EmbedderTest, RenderTextureWithImpellerVulkanDestructCallback) {
  constexpr int kWidth = 800;
  constexpr int kHeight = 600;
  auto& context = GetEmbedderContext<EmbedderTestContextVulkan>();
  EmbedderConfigBuilder builder(context);

  builder.AddCommandLineArgument("--enable-impeller");
  builder.SetDartEntrypoint("render_texture_impeller_test");
  builder.SetSurface(DlISize(kWidth, kHeight));

  auto external_surface = TestVulkanSurface::Create(*context.GetVulkanContext(),
                                                    DlISize(kWidth, kHeight));
  ASSERT_TRUE(external_surface && external_surface->IsValid());
  auto sk_surface = external_surface->GetSkSurface();
  auto canvas = sk_surface->getCanvas();
  SkPaint top_paint;
  top_paint.setColor(SK_ColorRED);
  canvas->drawRect(SkRect::MakeXYWH(0, 0, kWidth, kHeight / 2), top_paint);
  SkPaint bottom_paint;
  bottom_paint.setColor(SK_ColorBLUE);
  canvas->drawRect(SkRect::MakeXYWH(0, kHeight / 2, kWidth, kHeight / 2),
                   bottom_paint);
  context.GetVulkanContext()->GetGrDirectContext()->flushAndSubmit();

  auto rendered_scene = context.GetNextSceneImage();

  static bool destruction_callback_called = false;
  static auto destruction_callback = [](void* user_data) {
    destruction_callback_called = true;
  };
  context.SetExternalTextureCallback([&](int64_t texture_id, size_t width,
                                         size_t height,
                                         FlutterVulkanImage* output) -> bool {
    output->struct_size = sizeof(FlutterVulkanImage);
    output->image = reinterpret_cast<uint64_t>(external_surface->GetImage());
    output->format = VK_FORMAT_R8G8B8A8_UNORM;
    output->destruction_callback = destruction_callback;
    output->user_data = nullptr;
    output->width = width;
    output->height = height;
    return true;
  });

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  constexpr int texture_id = 1;
  ASSERT_EQ(FlutterEngineRegisterExternalTexture(engine.get(), texture_id),
            kSuccess);

  // Send a window metrics events so frames may be scheduled.
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = kWidth;
  event.height = kHeight;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);

  ASSERT_TRUE(
      ImageMatchesFixture("external_texture_impeller.png", rendered_scene));

  rendered_scene = context.GetNextSceneImage();
  ASSERT_EQ(
      FlutterEngineMarkExternalTextureFrameAvailable(engine.get(), texture_id),
      kSuccess);
  ASSERT_TRUE(
      ImageMatchesFixture("external_texture_impeller.png", rendered_scene));

  ASSERT_TRUE(destruction_callback_called);
}

}  // namespace testing
}  // namespace flutter

// NOLINTEND(clang-analyzer-core.StackAddressEscape)
