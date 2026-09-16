// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "flutter/shell/platform/android/android_shell_holder.h"
#include "flutter/shell/platform/android/android_surface_software.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "shell/platform/android/jni/platform_view_android_jni.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"

namespace flutter {
namespace testing {
namespace {
class MockPlatformViewAndroidJNI : public PlatformViewAndroidJNI {
 public:
  MOCK_METHOD(void,
              FlutterViewHandlePlatformMessage,
              (std::unique_ptr<flutter::PlatformMessage> message,
               int responseId),
              (override));
  MOCK_METHOD(void,
              FlutterViewHandlePlatformMessageResponse,
              (int responseId, std::unique_ptr<fml::Mapping> data),
              (override));
  MOCK_METHOD(void,
              FlutterViewUpdateSemantics,
              (std::vector<uint8_t> buffer,
               std::vector<std::string> strings,
               std::vector<std::vector<uint8_t>> string_attribute_args),
              (override));
  MOCK_METHOD(void,
              FlutterViewSetSemanticsTreeEnabled,
              (bool enabled),
              (override));
  MOCK_METHOD(void,
              FlutterViewSetApplicationLocale,
              (const std::string locale),
              (override));
  MOCK_METHOD(void,
              FlutterViewUpdateCustomAccessibilityActions,
              (std::vector<uint8_t> actions_buffer,
               std::vector<std::string> strings),
              (override));
  MOCK_METHOD(void, FlutterViewOnFirstFrame, (), (override));
  MOCK_METHOD(void, FlutterViewOnPreEngineRestart, (), (override));
  MOCK_METHOD(void,
              SurfaceTextureAttachToGLContext,
              (JavaLocalRef surface_texture, int textureId),
              (override));
  MOCK_METHOD(bool,
              SurfaceTextureShouldUpdate,
              (JavaLocalRef surface_texture),
              (override));
  MOCK_METHOD(void,
              SurfaceTextureUpdateTexImage,
              (JavaLocalRef surface_texture),
              (override));
  MOCK_METHOD(SkM44,
              SurfaceTextureGetTransformMatrix,
              (JavaLocalRef surface_texture),
              (override));
  MOCK_METHOD(void,
              SurfaceTextureDetachFromGLContext,
              (JavaLocalRef surface_texture),
              (override));
  MOCK_METHOD(JavaLocalRef,
              ImageProducerTextureEntryAcquireLatestImage,
              (JavaLocalRef image_texture_entry),
              (override));
  MOCK_METHOD(JavaLocalRef,
              ImageGetHardwareBuffer,
              (JavaLocalRef image),
              (override));
  MOCK_METHOD(void, ImageClose, (JavaLocalRef image), (override));
  MOCK_METHOD(void,
              HardwareBufferClose,
              (JavaLocalRef hardware_buffer),
              (override));
  MOCK_METHOD(void,
              FlutterViewOnDisplayPlatformView,
              (int view_id,
               int x,
               int y,
               int width,
               int height,
               int viewWidth,
               int viewHeight,
               MutatorsStack mutators_stack),
              (override));
  MOCK_METHOD(void,
              FlutterViewDisplayOverlaySurface,
              (int surface_id, int x, int y, int width, int height),
              (override));
  MOCK_METHOD(void, FlutterViewBeginFrame, (), (override));
  MOCK_METHOD(void, FlutterViewEndFrame, (), (override));
  MOCK_METHOD(std::unique_ptr<PlatformViewAndroidJNI::OverlayMetadata>,
              FlutterViewCreateOverlaySurface,
              (),
              (override));
  MOCK_METHOD(void, FlutterViewDestroyOverlaySurfaces, (), (override));
  MOCK_METHOD(ASurfaceTransaction*, createTransaction, (), (override));
  MOCK_METHOD(void, swapTransaction, (), (override));
  MOCK_METHOD(void, applyTransaction, (), (override));
  MOCK_METHOD(void, destroyOverlaySurface2, (), (override));
  MOCK_METHOD(std::unique_ptr<PlatformViewAndroidJNI::OverlayMetadata>,
              createOverlaySurface2,
              (),
              (override));
  MOCK_METHOD(void,
              onDisplayPlatformView2,
              (int32_t view_id,
               int32_t x,
               int32_t y,
               int32_t width,
               int32_t height,
               int32_t viewWidth,
               int32_t viewHeight,
               MutatorsStack mutators_stack),
              (override));
  MOCK_METHOD(void, hidePlatformView2, (int32_t view_id), (override));
  MOCK_METHOD(void, onEndFrame2, (), (override));
  MOCK_METHOD(void, showOverlaySurface2, (), (override));
  MOCK_METHOD(void, hideOverlaySurface2, (), (override));
  MOCK_METHOD(std::unique_ptr<std::vector<std::string>>,
              FlutterViewComputePlatformResolvedLocale,
              (std::vector<std::string> supported_locales_data),
              (override));
  MOCK_METHOD(double, GetDisplayRefreshRate, (), (override));
  MOCK_METHOD(double, GetDisplayWidth, (), (override));
  MOCK_METHOD(double, GetDisplayHeight, (), (override));
  MOCK_METHOD(double, GetDisplayDensity, (), (override));
  MOCK_METHOD(bool,
              RequestDartDeferredLibrary,
              (int loading_unit_id),
              (override));
  MOCK_METHOD(double,
              FlutterViewGetScaledFontSize,
              (double font_size, int configuration_id),
              (const, override));
  MOCK_METHOD(void,
              MaybeResizeSurfaceView,
              (int32_t width, int32_t height),
              (const, override));
};

class MockPlatformMessageResponse : public PlatformMessageResponse {
 public:
  static fml::RefPtr<MockPlatformMessageResponse> Create() {
    return fml::AdoptRef(new MockPlatformMessageResponse());
  }
  MOCK_METHOD(void, Complete, (std::unique_ptr<fml::Mapping> data), (override));
  MOCK_METHOD(void, CompleteEmpty, (), (override));
};
}  // namespace

TEST(AndroidShellHolder, Create) {
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  EXPECT_NE(holder.get(), nullptr);
  EXPECT_TRUE(holder->IsValid());
  EXPECT_NE(holder->GetPlatformView().get(), nullptr);
  EXPECT_NE(holder->GetPlatformView()->GetPlatformViewDelegate(), nullptr);
  EXPECT_NE(holder->GetPlatformViewEmbedderForTesting(), nullptr);
  EXPECT_EQ(
      static_cast<const void*>(
          holder->GetPlatformView()->GetPlatformViewDelegate()),
      static_cast<const void*>(holder->GetPlatformViewEmbedderForTesting()));
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->GetPlatformView()->NotifyCreated(window);
}

TEST(AndroidShellHolder, PlatformViewEmbedderDelegateWired) {
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  ASSERT_NE(holder.get(), nullptr);
  ASSERT_TRUE(holder->IsValid());
  ASSERT_NE(holder->GetPlatformView().get(), nullptr);
  EXPECT_NE(holder->GetPlatformView()->GetPlatformViewDelegate(), nullptr);
  EXPECT_NE(holder->GetPlatformViewEmbedderForTesting(), nullptr);
  EXPECT_EQ(
      static_cast<const void*>(
          holder->GetPlatformView()->GetPlatformViewDelegate()),
      static_cast<const void*>(holder->GetPlatformViewEmbedderForTesting()));
}

TEST(AndroidShellHolder, HandlePlatformMessage) {
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  EXPECT_NE(holder.get(), nullptr);
  EXPECT_TRUE(holder->IsValid());
  EXPECT_NE(holder->GetPlatformView().get(), nullptr);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->GetPlatformView()->NotifyCreated(window);
  EXPECT_TRUE(holder->GetPlatformMessageHandler());
  size_t data_size = 4;
  fml::MallocMapping bytes =
      fml::MallocMapping(static_cast<uint8_t*>(malloc(data_size)), data_size);
  fml::RefPtr<MockPlatformMessageResponse> response =
      MockPlatformMessageResponse::Create();
  auto message = std::make_unique<PlatformMessage>(
      /*channel=*/"foo", /*data=*/std::move(bytes), /*response=*/response);
  int response_id = 1;
  EXPECT_CALL(*jni,
              FlutterViewHandlePlatformMessage(::testing::_, response_id));
  EXPECT_CALL(*response, CompleteEmpty());
  holder->GetPlatformMessageHandler()->HandlePlatformMessage(
      std::move(message));
  holder->GetPlatformMessageHandler()
      ->InvokePlatformMessageEmptyResponseCallback(response_id);
}

TEST(AndroidShellHolder, CreateWithMergedPlatformAndUIThread) {
  Settings settings;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->GetPlatformView()->NotifyCreated(window);

  EXPECT_EQ(
      holder->GetShellForTesting()->GetTaskRunners().GetUITaskRunner(),
      holder->GetShellForTesting()->GetTaskRunners().GetPlatformTaskRunner());
}

TEST(AndroidShellHolder, CreateWithUnMergedPlatformAndUIThread) {
  Settings settings;
  settings.merged_platform_ui_thread =
      Settings::MergedPlatformUIThread::kDisabled;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->GetPlatformView()->NotifyCreated(window);

  EXPECT_NE(
      holder->GetShellForTesting()->GetTaskRunners().GetUITaskRunner(),
      holder->GetShellForTesting()->GetTaskRunners().GetPlatformTaskRunner());
}

#if !SLIMPELLER
TEST(AndroidSurfaceSoftware, ScreenshotNonBlank) {
  AndroidSurfaceSoftware surface;
  const DlISize size(80, 60);
  sk_sp<SkSurface> sk_surface = surface.AcquireBackingStore(size);
  ASSERT_NE(sk_surface, nullptr);

  SkPaint paint;
  paint.setColor(SK_ColorRED);
  sk_surface->getCanvas()->drawPaint(paint);

  auto screenshot = surface.Screenshot();
  EXPECT_NE(screenshot.data, nullptr);
  EXPECT_EQ(screenshot.frame_size.width, 80);
  EXPECT_EQ(screenshot.frame_size.height, 60);
  EXPECT_EQ(screenshot.data->size(), static_cast<size_t>(80 * 60 * 4));

  // Assert non-blank bitmap: check for non-zero pixel data.
  const uint8_t* bytes = static_cast<const uint8_t*>(screenshot.data->data());
  bool has_non_zero_pixel = false;
  for (size_t i = 0; i < screenshot.data->size(); ++i) {
    if (bytes[i] != 0) {
      has_non_zero_pixel = true;
      break;
    }
  }
  EXPECT_TRUE(has_non_zero_pixel);
}
#endif  // !SLIMPELLER

TEST(AndroidShellHolder, ScreenshotEmptyWhenNoFrame) {
  Settings settings;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  auto screenshot =
      holder->Screenshot(Rasterizer::ScreenshotType::UncompressedImage, false);
  EXPECT_EQ(screenshot.data, nullptr);
}

TEST(AndroidShellHolder, ProjectArgsConstruction) {
  Settings settings;
  settings.assets_path = "/data/flutter_assets";
  settings.icu_data_path = "/data/icudtl.dat";
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);

  const FlutterProjectArgs* project_args = holder->GetProjectArgsForTesting();
  ASSERT_NE(project_args, nullptr);
  EXPECT_EQ(project_args->struct_size, sizeof(FlutterProjectArgs));
  EXPECT_STREQ(project_args->assets_path, "/data/flutter_assets");
  EXPECT_STREQ(project_args->icu_data_path, "/data/icudtl.dat");

  // Custom task runners verification (from T-2.5)
  ASSERT_NE(project_args->custom_task_runners, nullptr);
  EXPECT_EQ(project_args->custom_task_runners->struct_size,
            sizeof(FlutterCustomTaskRunners));
  EXPECT_NE(project_args->custom_task_runners->platform_task_runner, nullptr);
  EXPECT_NE(project_args->custom_task_runners->render_task_runner, nullptr);
  EXPECT_NE(project_args->custom_task_runners->ui_task_runner, nullptr);
  EXPECT_NE(project_args->custom_task_runners->io_task_runner, nullptr);

  // Callbacks verification
  EXPECT_NE(project_args->on_pre_engine_restart_callback, nullptr);
  EXPECT_NE(project_args->set_application_locale_callback, nullptr);
  EXPECT_NE(project_args->get_scaled_font_size_callback, nullptr);
  EXPECT_NE(project_args->dart_deferred_library_request_callback, nullptr);

  // Before launch, asset resolvers are empty
  EXPECT_EQ(project_args->asset_resolvers_count, 0u);
  EXPECT_EQ(project_args->asset_resolvers, nullptr);
}

TEST(AndroidShellHolder, ProjectArgsCallbacksForwardToJNI) {
  Settings settings;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);

  const FlutterProjectArgs* project_args = holder->GetProjectArgsForTesting();
  ASSERT_NE(project_args, nullptr);

  EXPECT_CALL(*jni, FlutterViewOnPreEngineRestart()).Times(1);
  project_args->on_pre_engine_restart_callback(holder.get());

  EXPECT_CALL(*jni, FlutterViewSetApplicationLocale(std::string("fr-FR")))
      .Times(1);
  project_args->set_application_locale_callback("fr-FR", holder.get());

  EXPECT_CALL(*jni, FlutterViewGetScaledFontSize(16.0, 2))
      .WillOnce(::testing::Return(18.5));
  double scaled =
      project_args->get_scaled_font_size_callback(16.0, 2, holder.get());
  EXPECT_DOUBLE_EQ(scaled, 18.5);

  EXPECT_CALL(*jni, RequestDartDeferredLibrary(123)).Times(1);
  project_args->dart_deferred_library_request_callback(123, holder.get());
}

TEST(AndroidShellHolder, CreateFlutterProjectArgsCustomization) {
  Settings settings;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);

  FlutterProjectArgs custom_args = holder->CreateFlutterProjectArgs(
      "customMain", "package:custom/main.dart", {"--arg1"}, 42);

  EXPECT_EQ(custom_args.struct_size, sizeof(FlutterProjectArgs));
  EXPECT_EQ(custom_args.engine_id, 42);
  EXPECT_STREQ(custom_args.custom_dart_entrypoint, "customMain");
  EXPECT_NE(custom_args.custom_task_runners, nullptr);
}

}  // namespace testing
}  // namespace flutter
