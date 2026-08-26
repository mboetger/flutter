#define FML_USED_ON_EMBEDDER

#include "flutter/common/settings.h"
#include "flutter/fml/message_loop.h"
#include "flutter/shell/platform/android/android_engine.h"
#include "flutter/shell/platform/android/flutter_main.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "flutter/shell/platform/android/platform_view_android.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {
namespace {

class MockPlatformViewAndroidDelegate : public PlatformViewAndroid::Delegate {
 public:
  MOCK_METHOD(const Settings&,
              OnPlatformViewGetSettings,
              (),
              (const, override));
  MOCK_METHOD(std::shared_ptr<fml::BasicTaskRunner>,
              OnPlatformViewGetShutdownSafeIOTaskRunner,
              (),
              (const, override));
  MOCK_METHOD(void,
              OnPlatformViewCreated,
              (std::unique_ptr<Surface> surface),
              (override));
  MOCK_METHOD(void, OnPlatformViewDestroyed, (), (override));
  MOCK_METHOD(void, OnPlatformViewScheduleFrame, (), (override));
  MOCK_METHOD(void,
              OnPlatformViewSetNextFrameCallback,
              (const fml::closure& closure),
              (override));
  MOCK_METHOD(void,
              OnPlatformViewSetViewportMetrics,
              (int64_t view_id, const ViewportMetrics& metrics),
              (override));
  MOCK_METHOD(void,
              OnPlatformViewDispatchPlatformMessage,
              (std::unique_ptr<flutter::PlatformMessage> message),
              (override));
  MOCK_METHOD(void,
              OnPlatformViewDispatchPointerDataPacket,
              (std::unique_ptr<flutter::PointerDataPacket> packet),
              (override));
  MOCK_METHOD(void,
              OnPlatformViewDispatchSemanticsAction,
              (int64_t view_id,
               int32_t node_id,
               flutter::SemanticsAction action,
               fml::MallocMapping args),
              (override));
  MOCK_METHOD(void,
              OnPlatformViewSetSemanticsEnabled,
              (bool enabled),
              (override));
  MOCK_METHOD(void,
              OnPlatformViewSetAccessibilityFeatures,
              (int32_t flags),
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
};

}  // namespace

TEST(PlatformViewAndroidTest, CreateAndInvokeLifecycleHooks) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();
  auto task_runner = fml::MessageLoop::GetCurrent().GetTaskRunner();
  TaskRunners task_runners("test", task_runner, task_runner, task_runner,
                           task_runner);

  MockPlatformViewAndroidDelegate delegate;
  Settings settings;
  settings.enable_software_rendering = true;
  EXPECT_CALL(delegate, OnPlatformViewGetSettings())
      .WillRepeatedly(::testing::ReturnRef(settings));
  EXPECT_CALL(delegate, OnPlatformViewGetShutdownSafeIOTaskRunner())
      .WillRepeatedly(::testing::Return(nullptr));

  auto jni = std::make_shared<JNIMock>();
  auto platform_view = std::make_unique<PlatformViewAndroid>(
      delegate, task_runners, jni, AndroidRenderingAPI::kSoftware);

  EXPECT_TRUE(platform_view->GetWeakPtr());
  EXPECT_NE(platform_view->GetAndroidContext(), nullptr);
  EXPECT_NE(platform_view->GetPlatformMessageHandler(), nullptr);

  auto window1 = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  EXPECT_CALL(delegate, OnPlatformViewSetNextFrameCallback(::testing::_));
  EXPECT_CALL(delegate, OnPlatformViewScheduleFrame()).Times(2);
  platform_view->NotifyCreated(window1);

  platform_view->NotifyChanged(DlISize(800, 600));

  auto window2 = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  EXPECT_CALL(delegate, OnPlatformViewScheduleFrame());
  platform_view->NotifySurfaceWindowChanged(window2);

  EXPECT_CALL(delegate, OnPlatformViewDestroyed());
  platform_view->NotifyDestroyed();
}

TEST(PlatformViewAndroidTest, DispatchesEventsToDelegate) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();
  auto task_runner = fml::MessageLoop::GetCurrent().GetTaskRunner();
  TaskRunners task_runners("test", task_runner, task_runner, task_runner,
                           task_runner);

  MockPlatformViewAndroidDelegate delegate;
  Settings settings;
  settings.enable_software_rendering = false;
  EXPECT_CALL(delegate, OnPlatformViewGetSettings())
      .WillRepeatedly(::testing::ReturnRef(settings));
  EXPECT_CALL(delegate, OnPlatformViewGetShutdownSafeIOTaskRunner())
      .WillRepeatedly(::testing::Return(nullptr));

  auto jni = std::make_shared<JNIMock>();
  auto platform_view = std::make_unique<PlatformViewAndroid>(
      delegate, task_runners, jni, AndroidRenderingAPI::kImpellerOpenGLES);

  EXPECT_CALL(delegate, OnPlatformViewSetViewportMetrics(0, ::testing::_));
  ViewportMetrics metrics;
  platform_view->SetViewportMetrics(0, metrics);

  EXPECT_CALL(delegate, OnPlatformViewDispatchPlatformMessage(::testing::_));
  platform_view->DispatchEmptyPlatformMessage(nullptr, "test_channel", 0);

  EXPECT_CALL(delegate, OnPlatformViewDispatchPointerDataPacket(::testing::_));
  platform_view->DispatchPointerDataPacket(
      std::make_unique<flutter::PointerDataPacket>(nullptr, 0));

  EXPECT_CALL(delegate, OnPlatformViewSetSemanticsEnabled(true));
  platform_view->SetSemanticsEnabled(true);

  EXPECT_CALL(delegate, OnPlatformViewSetAccessibilityFeatures(7));
  platform_view->SetAccessibilityFeatures(7);

  EXPECT_CALL(delegate, OnPlatformViewUnregisterTexture(100));
  platform_view->UnregisterTexture(100);

  EXPECT_CALL(delegate, OnPlatformViewMarkTextureFrameAvailable(100));
  platform_view->MarkTextureFrameAvailable(100);

  EXPECT_CALL(delegate, OnPlatformViewScheduleFrame());
  platform_view->ScheduleFrame();

  EXPECT_CALL(delegate, LoadDartDeferredLibrary(5, ::testing::_, ::testing::_));
  platform_view->LoadDartDeferredLibrary(5, nullptr, nullptr);

  EXPECT_CALL(delegate, LoadDartDeferredLibraryError(5, "failed", false));
  platform_view->LoadDartDeferredLibraryError(5, "failed", false);

  EXPECT_CALL(
      delegate,
      UpdateAssetResolverByType(
          ::testing::_, AssetResolver::AssetResolverType::kApkAssetProvider));
  platform_view->UpdateAssetResolverByType(
      nullptr, AssetResolver::AssetResolverType::kApkAssetProvider);
}

#if !SLIMPELLER
TEST(FlutterMainSelectedRenderingAPI, SelectsImpellerAutoselectOnApi29Plus) {
  Settings settings;
  settings.enable_software_rendering = false;
  settings.enable_impeller = true;

  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 29),
            AndroidRenderingAPI::kImpellerAutoselect);
  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 34),
            AndroidRenderingAPI::kImpellerAutoselect);
}

TEST(FlutterMainSelectedRenderingAPI, SelectsSkiaOpenGLESOnApiBelow29) {
  Settings settings;
  settings.enable_software_rendering = false;
  settings.enable_impeller = true;

  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 28),
            AndroidRenderingAPI::kSkiaOpenGLES);
  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 24),
            AndroidRenderingAPI::kSkiaOpenGLES);
}

TEST(FlutterMainSelectedRenderingAPI,
     SelectsSkiaOpenGLESWhenImpellerDisabledOnApi29Plus) {
  Settings settings;
  settings.enable_software_rendering = false;
  settings.enable_impeller = false;

  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 29),
            AndroidRenderingAPI::kSkiaOpenGLES);
  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 34),
            AndroidRenderingAPI::kSkiaOpenGLES);
}

#ifndef FLUTTER_RELEASE
TEST(FlutterMainSelectedRenderingAPI,
     SelectsExplicitImpellerBackendWhenRequested) {
  Settings settings;
  settings.enable_software_rendering = false;
  settings.enable_impeller = true;

  settings.requested_rendering_backend = "vulkan";
  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 29),
            AndroidRenderingAPI::kImpellerVulkan);

  settings.requested_rendering_backend = "opengles";
  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 29),
            AndroidRenderingAPI::kImpellerOpenGLES);
}

TEST(FlutterMainSelectedRenderingAPI,
     SelectsSkiaOpenGLESWhenBackendRequestedWithImpellerDisabled) {
  Settings settings;
  settings.enable_software_rendering = false;
  settings.enable_impeller = false;

  settings.requested_rendering_backend = "vulkan";
  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 29),
            AndroidRenderingAPI::kSkiaOpenGLES);

  settings.requested_rendering_backend = "opengles";
  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 29),
            AndroidRenderingAPI::kSkiaOpenGLES);
}

TEST(FlutterMainSelectedRenderingAPI,
     SelectsAutoselectWhenUnrecognizedBackendRequested) {
  Settings settings;
  settings.enable_software_rendering = false;
  settings.enable_impeller = true;

  settings.requested_rendering_backend = "unknown_backend";
  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 29),
            AndroidRenderingAPI::kImpellerAutoselect);
}
#endif  // !FLUTTER_RELEASE

TEST(FlutterMainSelectedRenderingAPI, SelectsSoftwareRenderingWhenRequested) {
  Settings settings;
  settings.enable_software_rendering = true;
  settings.enable_impeller = false;

  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 29),
            AndroidRenderingAPI::kSoftware);
}
#endif  // !SLIMPELLER

TEST(FlutterMainTest, IsEmbedderAPIEnabledReflectsSettings) {
  {
    Settings settings;
    EXPECT_TRUE(settings.enable_embedder_api);
    FlutterMain flutter_main(settings, AndroidRenderingAPI::kSoftware);
    EXPECT_TRUE(flutter_main.IsEmbedderAPIEnabled());
  }
  {
    Settings settings;
    settings.enable_embedder_api = true;
    FlutterMain flutter_main(settings, AndroidRenderingAPI::kSoftware);
    EXPECT_TRUE(flutter_main.IsEmbedderAPIEnabled());
  }
  {
    Settings settings;
    settings.enable_embedder_api = false;
    FlutterMain flutter_main(settings, AndroidRenderingAPI::kSoftware);
    EXPECT_FALSE(flutter_main.IsEmbedderAPIEnabled());
  }
}

TEST(PlatformViewAndroidTest, CreateAndRegisterExternalTextures) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();
  auto task_runner = fml::MessageLoop::GetCurrent().GetTaskRunner();
  TaskRunners task_runners("test", task_runner, task_runner, task_runner,
                           task_runner);

  MockPlatformViewAndroidDelegate delegate;
  Settings settings;
  settings.enable_software_rendering = false;
  EXPECT_CALL(delegate, OnPlatformViewGetSettings())
      .WillRepeatedly(::testing::ReturnRef(settings));
  EXPECT_CALL(delegate, OnPlatformViewGetShutdownSafeIOTaskRunner())
      .WillRepeatedly(::testing::Return(nullptr));

  auto jni = std::make_shared<JNIMock>();
  auto platform_view = std::make_unique<PlatformViewAndroid>(
      delegate, task_runners, jni, AndroidRenderingAPI::kImpellerOpenGLES);

  // Surface texture creation and registration.
  fml::jni::ScopedJavaGlobalRef<jobject> null_surface_texture;
  auto surface_texture =
      platform_view->CreateSurfaceTexture(101, null_surface_texture);
  ASSERT_NE(surface_texture, nullptr);
  EXPECT_EQ(surface_texture->Id(), 101);

  EXPECT_CALL(delegate, OnPlatformViewRegisterTexture(::testing::_))
      .WillOnce([](std::shared_ptr<flutter::Texture> tex) {
        ASSERT_NE(tex, nullptr);
        EXPECT_EQ(tex->Id(), 101);
      });
  platform_view->RegisterExternalTexture(101, null_surface_texture);

  // Image texture creation and registration.
  fml::jni::ScopedJavaGlobalRef<jobject> null_image_texture;
  auto image_texture = platform_view->CreateImageTexture(
      202, null_image_texture,
      ImageExternalTexture::ImageLifecycle::kKeepAlive);
  ASSERT_NE(image_texture, nullptr);
  EXPECT_EQ(image_texture->Id(), 202);

  EXPECT_CALL(delegate, OnPlatformViewRegisterTexture(::testing::_))
      .WillOnce([](std::shared_ptr<flutter::Texture> tex) {
        ASSERT_NE(tex, nullptr);
        EXPECT_EQ(tex->Id(), 202);
      });
  platform_view->RegisterImageTexture(
      202, null_image_texture,
      ImageExternalTexture::ImageLifecycle::kKeepAlive);
}

TEST(PlatformViewAndroidTest, SoftwareRenderingReturnsNullTextures) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();
  auto task_runner = fml::MessageLoop::GetCurrent().GetTaskRunner();
  TaskRunners task_runners("test", task_runner, task_runner, task_runner,
                           task_runner);

  MockPlatformViewAndroidDelegate delegate;
  Settings settings;
  settings.enable_software_rendering = true;
  EXPECT_CALL(delegate, OnPlatformViewGetSettings())
      .WillRepeatedly(::testing::ReturnRef(settings));
  EXPECT_CALL(delegate, OnPlatformViewGetShutdownSafeIOTaskRunner())
      .WillRepeatedly(::testing::Return(nullptr));

  auto jni = std::make_shared<JNIMock>();
  auto platform_view = std::make_unique<PlatformViewAndroid>(
      delegate, task_runners, jni, AndroidRenderingAPI::kSoftware);

  fml::jni::ScopedJavaGlobalRef<jobject> null_ref;
  EXPECT_EQ(platform_view->CreateSurfaceTexture(301, null_ref), nullptr);
  EXPECT_EQ(platform_view->CreateImageTexture(
                302, null_ref, ImageExternalTexture::ImageLifecycle::kReset),
            nullptr);

  // RegisterExternalTexture / RegisterImageTexture should safely no-op and not
  // call delegate.
  EXPECT_CALL(delegate, OnPlatformViewRegisterTexture(::testing::_)).Times(0);
  platform_view->RegisterExternalTexture(301, null_ref);
  platform_view->RegisterImageTexture(
      302, null_ref, ImageExternalTexture::ImageLifecycle::kReset);
}

TEST(PlatformViewAndroidTest, CreateAndRegisterExternalTexturesVulkan) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();
  auto task_runner = fml::MessageLoop::GetCurrent().GetTaskRunner();
  TaskRunners task_runners("test", task_runner, task_runner, task_runner,
                           task_runner);

  MockPlatformViewAndroidDelegate delegate;
  Settings settings;
  settings.enable_software_rendering = false;
  EXPECT_CALL(delegate, OnPlatformViewGetSettings())
      .WillRepeatedly(::testing::ReturnRef(settings));
  EXPECT_CALL(delegate, OnPlatformViewGetShutdownSafeIOTaskRunner())
      .WillRepeatedly(::testing::Return(nullptr));

  auto jni = std::make_shared<JNIMock>();
  auto platform_view = std::make_unique<PlatformViewAndroid>(
      delegate, task_runners, jni, AndroidRenderingAPI::kImpellerVulkan);

  // Vulkan Surface Texture creation & registration.
  fml::jni::ScopedJavaGlobalRef<jobject> null_surface_texture;
  auto surface_texture =
      platform_view->CreateSurfaceTexture(401, null_surface_texture);
  ASSERT_NE(surface_texture, nullptr);
  EXPECT_EQ(surface_texture->Id(), 401);

  EXPECT_CALL(delegate, OnPlatformViewRegisterTexture(::testing::_))
      .WillOnce([](std::shared_ptr<flutter::Texture> tex) {
        ASSERT_NE(tex, nullptr);
        EXPECT_EQ(tex->Id(), 401);
      });
  platform_view->RegisterExternalTexture(401, null_surface_texture);

  // Vulkan Image Texture creation & registration.
  fml::jni::ScopedJavaGlobalRef<jobject> null_image_texture;
  auto image_texture = platform_view->CreateImageTexture(
      402, null_image_texture, ImageExternalTexture::ImageLifecycle::kReset);
  ASSERT_NE(image_texture, nullptr);
  EXPECT_EQ(image_texture->Id(), 402);

  EXPECT_CALL(delegate, OnPlatformViewRegisterTexture(::testing::_))
      .WillOnce([](std::shared_ptr<flutter::Texture> tex) {
        ASSERT_NE(tex, nullptr);
        EXPECT_EQ(tex->Id(), 402);
      });
  platform_view->RegisterImageTexture(
      402, null_image_texture, ImageExternalTexture::ImageLifecycle::kReset);
}

TEST(PlatformViewAndroidTest, TextureLifecycleUnregisterAndMarkFrameAvailable) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();
  auto task_runner = fml::MessageLoop::GetCurrent().GetTaskRunner();
  TaskRunners task_runners("test", task_runner, task_runner, task_runner,
                           task_runner);

  MockPlatformViewAndroidDelegate delegate;
  Settings settings;
  settings.enable_software_rendering = false;
  EXPECT_CALL(delegate, OnPlatformViewGetSettings())
      .WillRepeatedly(::testing::ReturnRef(settings));
  EXPECT_CALL(delegate, OnPlatformViewGetShutdownSafeIOTaskRunner())
      .WillRepeatedly(::testing::Return(nullptr));

  auto jni = std::make_shared<JNIMock>();
  auto platform_view = std::make_unique<PlatformViewAndroid>(
      delegate, task_runners, jni, AndroidRenderingAPI::kImpellerOpenGLES);

  EXPECT_CALL(delegate, OnPlatformViewUnregisterTexture(501)).Times(1);
  platform_view->UnregisterTexture(501);

  EXPECT_CALL(delegate, OnPlatformViewMarkTextureFrameAvailable(501)).Times(1);
  platform_view->MarkTextureFrameAvailable(501);
}

TEST(PlatformViewAndroidTest, JNIDispatchSwitchingUnderEmbedderAPI) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();

  Settings settings;
  settings.enable_embedder_api = true;
  settings.enable_software_rendering = true;

  FlutterMain flutter_main(settings, AndroidRenderingAPI::kSoftware);
  EXPECT_TRUE(flutter_main.IsEmbedderAPIEnabled());

  auto jni = std::make_shared<JNIMock>();
  auto engine = std::make_unique<AndroidEngine>(settings, jni,
                                                AndroidRenderingAPI::kSoftware);
  ASSERT_TRUE(engine->IsValid());

  auto platform_view = engine->GetPlatformView();
  ASSERT_TRUE(platform_view);

  // Verify platform view methods work correctly through the engine delegate.
  platform_view->ScheduleFrame();
  platform_view->SetSemanticsEnabled(false);
  platform_view->SetAccessibilityFeatures(0);
}

}  // namespace testing
}  // namespace flutter
