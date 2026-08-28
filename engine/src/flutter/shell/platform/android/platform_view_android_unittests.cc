// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include "flutter/fml/make_copyable.h"
#include "flutter/fml/message_loop.h"
#include "flutter/fml/thread.h"
#include "flutter/shell/platform/android/android_shell_holder.h"
#include "flutter/shell/platform/android/flutter_main.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "flutter/shell/platform/android/platform_view_android.h"
#include "flutter/shell/platform/android/platform_view_android_adapter.h"
#include "flutter/shell/platform/embedder/embedder.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "third_party/googletest/googlemock/include/gmock/gmock-nice-strict.h"

namespace flutter {
namespace testing {

#if !SLIMPELLER
TEST(AndroidPlatformView, SelectsRenderingAPIByApiLevel) {
  // Test Android API levels from 21 (minimum supported) up to 35.
  constexpr int kMinSupportedApiLevel = 21;
  constexpr int kImpellerThresholdApiLevel = 29;
  constexpr int kMaxTestedApiLevel = 35;

  // 1. Default Impeller enabled without explicit backend override.
  {
    Settings settings;
    settings.enable_software_rendering = false;
    settings.enable_impeller = true;

    // API levels 21..28 fallback to Skia OpenGLES.
    for (int api = kMinSupportedApiLevel; api < kImpellerThresholdApiLevel;
         ++api) {
      EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, api),
                AndroidRenderingAPI::kSkiaOpenGLES)
          << "Failed for API level " << api;
    }

    // API levels 29..35 autoselect Impeller (Vulkan preferred).
    for (int api = kImpellerThresholdApiLevel; api <= kMaxTestedApiLevel;
         ++api) {
      EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, api),
                AndroidRenderingAPI::kImpellerAutoselect)
          << "Failed for API level " << api;
    }
  }

#ifndef FLUTTER_RELEASE
  // 2. Explicit Impeller OpenGLES requested backend.
  {
    Settings settings;
    settings.enable_software_rendering = false;
    settings.enable_impeller = true;
    settings.requested_rendering_backend = "opengles";

    for (int api = kMinSupportedApiLevel; api <= kMaxTestedApiLevel; ++api) {
      EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, api),
                AndroidRenderingAPI::kImpellerOpenGLES)
          << "Failed for API level " << api;
    }
  }

  // 3. Explicit Impeller Vulkan requested backend.
  {
    Settings settings;
    settings.enable_software_rendering = false;
    settings.enable_impeller = true;
    settings.requested_rendering_backend = "vulkan";

    for (int api = kMinSupportedApiLevel; api <= kMaxTestedApiLevel; ++api) {
      EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, api),
                AndroidRenderingAPI::kImpellerVulkan)
          << "Failed for API level " << api;
    }
  }
#endif  // !FLUTTER_RELEASE

  // 4. Software rendering.
  {
    Settings settings;
    settings.enable_software_rendering = true;
    settings.enable_impeller = false;

    for (int api = kMinSupportedApiLevel; api <= kMaxTestedApiLevel; ++api) {
      EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, api),
                AndroidRenderingAPI::kSoftware)
          << "Failed for API level " << api;
    }
  }

  // 5. Impeller disabled explicitly.
  {
    Settings settings;
    settings.enable_software_rendering = false;
    settings.enable_impeller = false;

    for (int api = kMinSupportedApiLevel; api <= kMaxTestedApiLevel; ++api) {
      EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, api),
                AndroidRenderingAPI::kSkiaOpenGLES)
          << "Failed for API level " << api;
    }
  }
}
#endif  // !SLIMPELLER

namespace {
// Default implicit view ID for single-view Flutter Android applications.
constexpr int64_t kImplicitViewId = 0;

class MockPlatformViewAndroidDelegate : public PlatformViewAndroid::Delegate {
 public:
  explicit MockPlatformViewAndroidDelegate(const flutter::TaskRunners& runners)
      : task_runners_(runners) {
    settings_.enable_software_rendering = true;
    settings_.enable_impeller = false;
  }

  const flutter::TaskRunners& GetTaskRunners() const override {
    return task_runners_;
  }
  const flutter::Settings& OnPlatformViewGetSettings() const override {
    return settings_;
  }
  std::shared_ptr<fml::BasicTaskRunner>
  OnPlatformViewGetShutdownSafeIOTaskRunner() const override {
    return nullptr;
  }

  MOCK_METHOD(void,
              OnPlatformViewCreated,
              (std::unique_ptr<Surface> surface),
              (override));
  MOCK_METHOD(void, OnPlatformViewDestroyed, (), (override));
  MOCK_METHOD(void, OnPlatformViewScheduleFrame, (), (override));
  MOCK_METHOD(void,
              OnPlatformViewSetViewportMetrics,
              (int64_t view_id, const flutter::ViewportMetrics& metrics),
              (override));
  MOCK_METHOD(void,
              OnPlatformViewDispatchPointerDataPacket,
              (std::unique_ptr<flutter::PointerDataPacket> packet),
              (override));
  MOCK_METHOD(void, SetSemanticsEnabled, (bool enabled), (override));
  MOCK_METHOD(void, SetAccessibilityFeatures, (int32_t flags), (override));
  MOCK_METHOD(void,
              OnPlatformViewDispatchPlatformMessage,
              (std::unique_ptr<flutter::PlatformMessage> message),
              (override));
  MOCK_METHOD(void,
              OnPlatformViewSemanticsAction,
              (int64_t view_id,
               int32_t node_id,
               flutter::SemanticsAction action,
               fml::MallocMapping args),
              (override));
  MOCK_METHOD(void,
              OnPlatformViewRegisterTexture,
              (std::shared_ptr<flutter::Texture> texture),
              (override));
  MOCK_METHOD(void,
              OnPlatformViewUnregisterTexture,
              (int64_t texture_id),
              (override));
  MOCK_METHOD(void,
              OnPlatformViewMarkTextureFrameAvailable,
              (int64_t texture_id),
              (override));
  MOCK_METHOD(void,
              SetNextFrameCallback,
              (const fml::closure& closure),
              (override));
  MOCK_METHOD(void,
              LoadDartDeferredLibrary,
              (intptr_t loading_unit_id,
               std::unique_ptr<const fml::Mapping> snapshot_data,
               std::unique_ptr<const fml::Mapping> snapshot_instructions),
              (override));
  MOCK_METHOD(void,
              LoadDartDeferredLibraryError,
              (intptr_t loading_unit_id,
               const std::string error_message,
               bool transient),
              (override));
  MOCK_METHOD(void,
              UpdateAssetResolverByType,
              (std::unique_ptr<AssetResolver> updated_asset_resolver,
               AssetResolver::AssetResolverType type),
              (override));

 private:
  const flutter::TaskRunners task_runners_;
  flutter::Settings settings_;
};

}  // namespace

TEST(PlatformViewAndroidTest, DelegateReceivesLifecycleAndEvents) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();
  auto platform_runner = fml::MessageLoop::GetCurrent().GetTaskRunner();
  fml::Thread thread("test_thread");
  auto loop_runner = thread.GetTaskRunner();
  flutter::TaskRunners task_runners("test_runners", platform_runner,
                                    loop_runner, loop_runner, loop_runner);

  MockPlatformViewAndroidDelegate delegate(task_runners);
  auto jni_facade = std::make_shared<JNIMock>();

  EXPECT_CALL(delegate, SetNextFrameCallback(::testing::_)).Times(1);
  EXPECT_CALL(delegate, OnPlatformViewCreated(::testing::_))
      .WillOnce([&task_runners](std::unique_ptr<Surface> surface) {
        if (surface) {
          task_runners.GetRasterTaskRunner()->PostTask(fml::MakeCopyable(
              [surface = std::move(surface)]() mutable { surface.reset(); }));
        }
      });
  EXPECT_CALL(delegate,
              OnPlatformViewSetViewportMetrics(::testing::_, ::testing::_))
      .Times(1);
  EXPECT_CALL(delegate, OnPlatformViewScheduleFrame()).Times(2);
  EXPECT_CALL(delegate, OnPlatformViewDestroyed()).Times(1);

  PlatformViewAndroid platform_view(delegate, task_runners, jni_facade,
                                    AndroidRenderingAPI::kSoftware);
  ASSERT_NE(platform_view.GetWeakPtr().get(), nullptr);

  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  platform_view.NotifyCreated(window);

  constexpr int kViewportWidth = 800;
  constexpr int kViewportHeight = 600;
  platform_view.NotifyChanged(DlISize(kViewportWidth, kViewportHeight));

  ViewportMetrics metrics;
  metrics.physical_width = kViewportWidth;
  metrics.physical_height = kViewportHeight;
  platform_view.SetViewportMetrics(kImplicitViewId, metrics);

  platform_view.ScheduleFrame();

  platform_view.NotifySurfaceWindowChanged(window);

  platform_view.NotifyDestroyed();
}

TEST(PlatformViewAndroidTest, DelegateReceivesInputsAndMessages) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();
  auto platform_runner = fml::MessageLoop::GetCurrent().GetTaskRunner();
  fml::Thread thread("test_thread");
  auto loop_runner = thread.GetTaskRunner();
  flutter::TaskRunners task_runners("test_runners", platform_runner,
                                    loop_runner, loop_runner, loop_runner);

  MockPlatformViewAndroidDelegate delegate(task_runners);
  auto jni_facade = std::make_shared<JNIMock>();

  PlatformViewAndroid platform_view(delegate, task_runners, jni_facade,
                                    AndroidRenderingAPI::kSoftware);

  // 1. Pointer Data Packet.
  EXPECT_CALL(delegate, OnPlatformViewDispatchPointerDataPacket(::testing::_))
      .Times(1);
  auto packet = std::make_unique<flutter::PointerDataPacket>(1);
  platform_view.DispatchPointerDataPacket(std::move(packet));

  // 2. Semantics and Accessibility Flags.
  EXPECT_CALL(delegate, SetSemanticsEnabled(true)).Times(1);
  platform_view.SetSemanticsEnabled(true);

  constexpr int32_t kAccessibilityFlags = 0x07;
  EXPECT_CALL(delegate, SetAccessibilityFeatures(kAccessibilityFlags)).Times(1);
  platform_view.SetAccessibilityFeatures(kAccessibilityFlags);

  // 3. Texture Lifecycle.
  constexpr int64_t kTextureId = 123;
  EXPECT_CALL(delegate, OnPlatformViewUnregisterTexture(kTextureId)).Times(1);
  platform_view.UnregisterTexture(kTextureId);

  EXPECT_CALL(delegate, OnPlatformViewMarkTextureFrameAvailable(kTextureId))
      .Times(1);
  platform_view.MarkTextureFrameAvailable(kTextureId);

  // 4. Asset Resolver Update.
  EXPECT_CALL(delegate,
              UpdateAssetResolverByType(
                  ::testing::_,
                  AssetResolver::AssetResolverType::kEmbedderAssetResolver))
      .Times(1);
  platform_view.UpdateAssetResolverByType(
      nullptr, AssetResolver::AssetResolverType::kEmbedderAssetResolver);
}

TEST(PlatformViewAndroidTest, DeferredLibraryDelegation) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();
  auto platform_runner = fml::MessageLoop::GetCurrent().GetTaskRunner();
  fml::Thread thread("test_thread");
  auto loop_runner = thread.GetTaskRunner();
  flutter::TaskRunners task_runners("test_runners", platform_runner,
                                    loop_runner, loop_runner, loop_runner);

  MockPlatformViewAndroidDelegate delegate(task_runners);
  auto jni_facade = std::make_shared<JNIMock>();

  PlatformViewAndroid platform_view(delegate, task_runners, jni_facade,
                                    AndroidRenderingAPI::kSoftware);

  constexpr intptr_t kLoadingUnitId = 42;
  EXPECT_CALL(delegate, LoadDartDeferredLibrary(kLoadingUnitId, ::testing::_,
                                                ::testing::_))
      .Times(1);
  platform_view.LoadDartDeferredLibrary(kLoadingUnitId, nullptr, nullptr);

  EXPECT_CALL(delegate, LoadDartDeferredLibraryError(kLoadingUnitId,
                                                     "Failed to load", true))
      .Times(1);
  platform_view.LoadDartDeferredLibraryError(kLoadingUnitId, "Failed to load",
                                             true);
}

TEST(PlatformViewAndroidTest, PlatformViewAndroidAdapterBridgesShell) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();
  Settings settings;
  settings.enable_software_rendering = true;
  settings.enable_impeller = false;
  auto jni = std::make_shared<JNIMock>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kSoftware);
  ASSERT_NE(holder.get(), nullptr);
  ASSERT_TRUE(holder->IsValid());
  ASSERT_NE(holder->GetPlatformView().get(), nullptr);

  // Test that adapter forwards calls to PlatformViewAndroid cleanly.
  holder->GetPlatformView()->SetApplicationLocale("en-US");
  holder->GetPlatformView()->SetSemanticsTreeEnabled(true);
  holder->GetPlatformView()->OnPreEngineRestart();
  holder->GetPlatformView()->ScheduleFrame();

  constexpr int64_t kTextureId = 456;
  holder->GetPlatformView()->UnregisterTexture(kTextureId);
  holder->GetPlatformView()->MarkTextureFrameAvailable(kTextureId);

  EXPECT_NE(holder->GetPlatformView()->GetAndroidContext().get(), nullptr);
  EXPECT_NE(holder->GetPlatformView()->GetPlatformMessageHandler().get(),
            nullptr);
}

TEST(FlutterMainTest, EmbedderAPIEnabledTestingOverrides) {
  FlutterMain::ResetEmbedderAPIEnabledForTesting();
  FlutterMain::ResetSettingsForTesting();
  EXPECT_FALSE(FlutterMain::IsEmbedderAPIEnabled());

  FlutterMain::SetEmbedderAPIEnabledForTesting(true);
  EXPECT_TRUE(FlutterMain::IsEmbedderAPIEnabled());

  FlutterMain::SetEmbedderAPIEnabledForTesting(false);
  EXPECT_FALSE(FlutterMain::IsEmbedderAPIEnabled());

  FlutterMain::ResetEmbedderAPIEnabledForTesting();
  EXPECT_FALSE(FlutterMain::IsEmbedderAPIEnabled());
}

TEST(FlutterMainTest, EmbedderAPIEnabledSettingsFallback) {
  FlutterMain::ResetEmbedderAPIEnabledForTesting();
  FlutterMain::ResetSettingsForTesting();
  EXPECT_FALSE(FlutterMain::IsEmbedderAPIEnabled());

  Settings settings_enabled;
  settings_enabled.enable_embedder_api = true;
  FlutterMain::SetSettingsForTesting(settings_enabled);
  EXPECT_TRUE(FlutterMain::IsEmbedderAPIEnabled());

  Settings settings_disabled;
  settings_disabled.enable_embedder_api = false;
  FlutterMain::SetSettingsForTesting(settings_disabled);
  EXPECT_FALSE(FlutterMain::IsEmbedderAPIEnabled());

  // Test override takes precedence over settings
  FlutterMain::SetEmbedderAPIEnabledForTesting(true);
  EXPECT_TRUE(FlutterMain::IsEmbedderAPIEnabled());

  FlutterMain::ResetEmbedderAPIEnabledForTesting();
  EXPECT_FALSE(FlutterMain::IsEmbedderAPIEnabled());

  FlutterMain::ResetSettingsForTesting();
  EXPECT_FALSE(FlutterMain::IsEmbedderAPIEnabled());
}

struct ScopedCommandLineArgsOverrideReset {
  ~ScopedCommandLineArgsOverrideReset() {
    FlutterMain::ResetCommandLineArgsForTesting();
  }
};

TEST(PlatformViewAndroidTest, FlutterMainCommandLineArgsTestingOverride) {
  ScopedCommandLineArgsOverrideReset reset_on_exit;
  FlutterMain::ResetCommandLineArgsForTesting();

  // Initial state without override or singleton returns default synthetic
  // executable.
  const std::vector<std::string> default_args = {"flutter"};
  EXPECT_EQ(FlutterMain::GetCommandLineArgs(), default_args);

  const std::vector<std::string> test_args = {
      "flutter", "--enable-android-embedder-api", "--enable-impeller=true"};
  FlutterMain::SetCommandLineArgsForTesting(test_args);
  EXPECT_EQ(FlutterMain::GetCommandLineArgs(), test_args);

  // Clear with nullopt reverts to default args.
  FlutterMain::SetCommandLineArgsForTesting(std::nullopt);
  EXPECT_EQ(FlutterMain::GetCommandLineArgs(), default_args);

  // Set again, then reset explicitly.
  FlutterMain::SetCommandLineArgsForTesting(test_args);
  EXPECT_EQ(FlutterMain::GetCommandLineArgs(), test_args);
  FlutterMain::ResetCommandLineArgsForTesting();
  EXPECT_EQ(FlutterMain::GetCommandLineArgs(), default_args);
}

}  // namespace testing
}  // namespace flutter
