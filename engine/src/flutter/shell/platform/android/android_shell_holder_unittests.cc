// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "flutter/runtime/runtime_controller.h"
#include "flutter/shell/common/engine.h"
#include "flutter/shell/common/shell.h"
#include "flutter/shell/platform/android/android_shell_holder.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "shell/platform/android/jni/platform_view_android_jni.h"

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
  MOCK_METHOD(ASurfaceTransaction*,
              createTransaction,
              (std::function<void()>&),
              (override));
  MOCK_METHOD(void, swapTransaction, (), (override));
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
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->NotifySurfaceCreated(window);
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
  holder->NotifySurfaceCreated(window);
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
  holder->NotifySurfaceCreated(window);

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
  holder->NotifySurfaceCreated(window);

  EXPECT_NE(
      holder->GetShellForTesting()->GetTaskRunners().GetUITaskRunner(),
      holder->GetShellForTesting()->GetTaskRunners().GetPlatformTaskRunner());
}

TEST(AndroidShellHolder, StartsWithDetachedLifecycleState) {
  Settings settings;
  settings.enable_software_rendering = false;
  // With merged threads the UI thread is the test thread, so the runtime
  // controller can be read directly.
  ASSERT_EQ(settings.merged_platform_ui_thread,
            Settings::MergedPlatformUIThread::kEnabled);
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  ASSERT_TRUE(holder->IsValid());

  // Frames stay off until Java reports a lifecycle state over
  // flutter/lifecycle.
  EXPECT_EQ(holder->GetShellForTesting()
                ->GetEngine()
                ->GetRuntimeController()
                ->GetPlatformData()
                .lifecycle_state,
            "AppLifecycleState.detached");
}

TEST(AndroidShellHolder, SurfaceLifecycleRoutesThroughEmbedderApi) {
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  ASSERT_TRUE(holder->IsValid());

  static int notify_created_calls;
  static int set_next_frame_callback_calls;
  static int schedule_frame_calls;
  static int notify_destroyed_calls;
  notify_created_calls = 0;
  set_next_frame_callback_calls = 0;
  schedule_frame_calls = 0;
  notify_destroyed_calls = 0;

  FlutterEngineProcTable& api = holder->GetEmbedderApiForTesting();
  auto real_notify_created = api.NotifyCreated;
  auto real_set_next_frame_callback = api.SetNextFrameCallback;
  auto real_schedule_frame = api.ScheduleFrame;
  auto real_notify_destroyed = api.NotifyDestroyed;
  static FlutterEngineNotifyCreatedFnPtr saved_notify_created;
  static FlutterEngineSetNextFrameCallbackFnPtr saved_set_next_frame_callback;
  static FlutterEngineScheduleFrameFnPtr saved_schedule_frame;
  static FlutterEngineNotifyDestroyedFnPtr saved_notify_destroyed;
  saved_notify_created = real_notify_created;
  saved_set_next_frame_callback = real_set_next_frame_callback;
  saved_schedule_frame = real_schedule_frame;
  saved_notify_destroyed = real_notify_destroyed;

  api.NotifyCreated = [](FLUTTER_API_SYMBOL(FlutterEngine) engine) {
    notify_created_calls++;
    return saved_notify_created(engine);
  };
  api.SetNextFrameCallback = [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
                                VoidCallback callback, void* user_data) {
    set_next_frame_callback_calls++;
    return saved_set_next_frame_callback(engine, callback, user_data);
  };
  api.ScheduleFrame = [](FLUTTER_API_SYMBOL(FlutterEngine) engine) {
    schedule_frame_calls++;
    return saved_schedule_frame(engine);
  };
  api.NotifyDestroyed = [](FLUTTER_API_SYMBOL(FlutterEngine) engine) {
    notify_destroyed_calls++;
    return saved_notify_destroyed(engine);
  };

  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->NotifySurfaceCreated(window);
  EXPECT_EQ(notify_created_calls, 1);
  EXPECT_EQ(set_next_frame_callback_calls, 1);

  holder->NotifySurfaceWindowChanged(window);
  EXPECT_EQ(schedule_frame_calls, 1);

  holder->NotifySurfaceDestroyed();
  EXPECT_EQ(notify_destroyed_calls, 1);
}

TEST(AndroidShellHolder, TextureLifecycleRoutesThroughEmbedderApi) {
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  ASSERT_TRUE(holder->IsValid());

  static int mark_frame_available_calls;
  static int unregister_texture_calls;
  static int64_t last_marked_texture_id;
  static int64_t last_unregistered_texture_id;
  mark_frame_available_calls = 0;
  unregister_texture_calls = 0;
  last_marked_texture_id = 0;
  last_unregistered_texture_id = 0;

  FlutterEngineProcTable& api = holder->GetEmbedderApiForTesting();
  static FlutterEngineMarkExternalTextureFrameAvailableFnPtr
      saved_mark_frame_available;
  static FlutterEngineUnregisterExternalTextureFnPtr saved_unregister_texture;
  saved_mark_frame_available = api.MarkExternalTextureFrameAvailable;
  saved_unregister_texture = api.UnregisterExternalTexture;

  api.MarkExternalTextureFrameAvailable =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine, int64_t texture_identifier) {
        mark_frame_available_calls++;
        last_marked_texture_id = texture_identifier;
        return saved_mark_frame_available(engine, texture_identifier);
      };
  api.UnregisterExternalTexture = [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
                                     int64_t texture_identifier) {
    unregister_texture_calls++;
    last_unregistered_texture_id = texture_identifier;
    return saved_unregister_texture(engine, texture_identifier);
  };

  constexpr int64_t kTestTextureId = 42;
  holder->MarkTextureFrameAvailable(kTestTextureId);
  EXPECT_EQ(mark_frame_available_calls, 1);
  EXPECT_EQ(last_marked_texture_id, kTestTextureId);

  holder->UnregisterTexture(kTestTextureId);
  EXPECT_EQ(unregister_texture_calls, 1);
  EXPECT_EQ(last_unregistered_texture_id, kTestTextureId);
}

}  // namespace testing
}  // namespace flutter
