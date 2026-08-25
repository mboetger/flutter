// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include "flutter/shell/platform/android/platform_view_android_adapter.h"

#include <memory>
#include <utility>

#include "flutter/common/settings.h"
#include "flutter/fml/message_loop.h"
#include "flutter/shell/common/thread_host.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {
namespace {

class MockPlatformViewDelegate : public PlatformView::Delegate {
 public:
  MOCK_METHOD(void,
              OnPlatformViewCreated,
              (std::unique_ptr<Surface> surface),
              (override));
  MOCK_METHOD(void, OnPlatformViewDestroyed, (), (override));
  MOCK_METHOD(void, OnPlatformViewScheduleFrame, (), (override));
  MOCK_METHOD(void,
              OnPlatformViewAddView,
              (int64_t view_id,
               const ViewportMetrics& viewport_metrics,
               AddViewCallback callback),
              (override));
  MOCK_METHOD(void,
              OnPlatformViewRemoveView,
              (int64_t view_id, RemoveViewCallback callback),
              (override));
  MOCK_METHOD(void,
              OnPlatformViewSendViewFocusEvent,
              (const ViewFocusEvent& event),
              (override));
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
  MOCK_METHOD(HitTestResponse,
              OnPlatformViewHitTest,
              (int64_t view_id, const flutter::PointData offset),
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
  MOCK_METHOD(const Settings&,
              OnPlatformViewGetSettings,
              (),
              (const, override));
  MOCK_METHOD(std::shared_ptr<fml::BasicTaskRunner>,
              OnPlatformViewGetShutdownSafeIOTaskRunner,
              (),
              (const, override));
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

TEST(PlatformViewAndroidAdapterTest, CreateAndAccessUnderlyingPlatformView) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();
  auto task_runner = fml::MessageLoop::GetCurrent().GetTaskRunner();
  TaskRunners task_runners("test", task_runner, task_runner, task_runner,
                           task_runner);

  MockPlatformViewDelegate delegate;
  Settings settings;
  settings.enable_software_rendering = false;
  EXPECT_CALL(delegate, OnPlatformViewGetSettings())
      .WillRepeatedly(::testing::ReturnRef(settings));
  EXPECT_CALL(delegate, OnPlatformViewGetShutdownSafeIOTaskRunner())
      .WillRepeatedly(::testing::Return(nullptr));

  auto jni = std::make_shared<JNIMock>();
  auto adapter = std::make_unique<PlatformViewAndroidAdapter>(
      delegate, task_runners, jni, AndroidRenderingAPI::kImpellerOpenGLES);

  EXPECT_NE(adapter->GetPlatformViewAndroid(), nullptr);
  auto weak_pv = adapter->GetPlatformViewAndroid()->GetWeakPtr();
  EXPECT_TRUE(weak_pv);
  EXPECT_EQ(weak_pv.get(), adapter->GetPlatformViewAndroid());

  adapter.reset();
  EXPECT_FALSE(weak_pv);
}

TEST(PlatformViewAndroidAdapterTest, ForwardsPlatformViewMethodsToAndroidView) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();
  auto task_runner = fml::MessageLoop::GetCurrent().GetTaskRunner();
  TaskRunners task_runners("test", task_runner, task_runner, task_runner,
                           task_runner);

  MockPlatformViewDelegate delegate;
  Settings settings;
  settings.enable_software_rendering = false;
  EXPECT_CALL(delegate, OnPlatformViewGetSettings())
      .WillRepeatedly(::testing::ReturnRef(settings));
  EXPECT_CALL(delegate, OnPlatformViewGetShutdownSafeIOTaskRunner())
      .WillRepeatedly(::testing::Return(nullptr));

  auto jni = std::make_shared<JNIMock>();
  auto adapter = std::make_unique<PlatformViewAndroidAdapter>(
      delegate, task_runners, jni, AndroidRenderingAPI::kImpellerOpenGLES);

  EXPECT_CALL(*jni, FlutterViewSetSemanticsTreeEnabled(true));
  adapter->SetSemanticsTreeEnabled(true);

  EXPECT_CALL(*jni, FlutterViewSetApplicationLocale("en-US"));
  adapter->SetApplicationLocale("en-US");

  EXPECT_CALL(*jni, FlutterViewOnPreEngineRestart());
  adapter->OnPreEngineRestart();

  EXPECT_CALL(*jni, FlutterViewGetScaledFontSize(16.0, 1))
      .WillOnce(::testing::Return(24.0));
  EXPECT_EQ(adapter->GetScaledFontSize(16.0, 1), 24.0);

  EXPECT_NE(adapter->GetPlatformMessageHandler(), nullptr);
}

TEST(PlatformViewAndroidAdapterTest, ForwardsDelegateMethodsToShellDelegate) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();
  auto task_runner = fml::MessageLoop::GetCurrent().GetTaskRunner();
  TaskRunners task_runners("test", task_runner, task_runner, task_runner,
                           task_runner);

  MockPlatformViewDelegate delegate;
  Settings settings;
  settings.enable_software_rendering = false;
  EXPECT_CALL(delegate, OnPlatformViewGetSettings())
      .WillRepeatedly(::testing::ReturnRef(settings));
  EXPECT_CALL(delegate, OnPlatformViewGetShutdownSafeIOTaskRunner())
      .WillRepeatedly(::testing::Return(nullptr));

  auto jni = std::make_shared<JNIMock>();
  auto adapter = std::make_unique<PlatformViewAndroidAdapter>(
      delegate, task_runners, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  auto* platform_view = adapter->GetPlatformViewAndroid();
  ASSERT_TRUE(platform_view);

  EXPECT_CALL(delegate, OnPlatformViewScheduleFrame());
  platform_view->ScheduleFrame();

  EXPECT_CALL(delegate, OnPlatformViewSetSemanticsEnabled(true));
  platform_view->SetSemanticsEnabled(true);

  EXPECT_CALL(delegate, OnPlatformViewSetAccessibilityFeatures(123));
  platform_view->SetAccessibilityFeatures(123);

  EXPECT_CALL(delegate, OnPlatformViewUnregisterTexture(42));
  platform_view->UnregisterTexture(42);

  EXPECT_CALL(delegate, OnPlatformViewMarkTextureFrameAvailable(42));
  platform_view->MarkTextureFrameAvailable(42);

  EXPECT_CALL(delegate, OnPlatformViewSetViewportMetrics(0, ::testing::_));
  ViewportMetrics metrics;
  platform_view->SetViewportMetrics(0, metrics);

  EXPECT_CALL(delegate, OnPlatformViewDispatchPlatformMessage(::testing::_));
  platform_view->DispatchEmptyPlatformMessage(nullptr, "test_channel", 0);

  EXPECT_CALL(delegate, OnPlatformViewDispatchPointerDataPacket(::testing::_));
  platform_view->DispatchPointerDataPacket(
      std::make_unique<flutter::PointerDataPacket>(nullptr, 0));

  EXPECT_CALL(delegate,
              OnPlatformViewDispatchSemanticsAction(
                  0, 10, flutter::SemanticsAction::kTap, ::testing::_));
  platform_view->DispatchSemanticsAction(
      nullptr, 10, static_cast<jint>(flutter::SemanticsAction::kTap), nullptr,
      0);

  EXPECT_CALL(delegate, LoadDartDeferredLibrary(1, ::testing::_, ::testing::_));
  platform_view->LoadDartDeferredLibrary(1, nullptr, nullptr);

  EXPECT_CALL(delegate, LoadDartDeferredLibraryError(1, "error", true));
  platform_view->LoadDartDeferredLibraryError(1, "error", true);

  EXPECT_CALL(
      delegate,
      UpdateAssetResolverByType(
          ::testing::_, AssetResolver::AssetResolverType::kApkAssetProvider));
  platform_view->UpdateAssetResolverByType(
      nullptr, AssetResolver::AssetResolverType::kApkAssetProvider);
}

}  // namespace testing
}  // namespace flutter
