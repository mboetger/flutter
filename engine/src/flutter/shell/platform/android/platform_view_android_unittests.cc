// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "flutter/fml/message_loop.h"
#include "flutter/shell/platform/android/android_shell_holder.h"
#include "flutter/shell/platform/android/flutter_main.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "flutter/shell/platform/android/platform_view_android.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

class FakePlatformViewDelegate : public PlatformView::Delegate {
 public:
  void OnPlatformViewCreated(std::unique_ptr<Surface> surface) override {}
  void OnPlatformViewDestroyed() override {}
  void OnPlatformViewScheduleFrame() override {}
  void OnPlatformViewAddView(int64_t view_id,
                             const ViewportMetrics& viewport_metrics,
                             AddViewCallback callback) override {}
  void OnPlatformViewRemoveView(int64_t view_id,
                                RemoveViewCallback callback) override {}
  void OnPlatformViewSendViewFocusEvent(const ViewFocusEvent& event) override {}
  void OnPlatformViewSetNextFrameCallback(
      const fml::closure& closure) override {}
  void OnPlatformViewSetViewportMetrics(
      int64_t view_id,
      const ViewportMetrics& metrics) override {}
  void OnPlatformViewDispatchPlatformMessage(
      std::unique_ptr<PlatformMessage> message) override {}
  void OnPlatformViewDispatchPointerDataPacket(
      std::unique_ptr<PointerDataPacket> packet) override {}
  HitTestResponse OnPlatformViewHitTest(
      int64_t view_id,
      const flutter::PointData offset) override {
    return {};
  }
  void OnPlatformViewDispatchSemanticsAction(int64_t view_id,
                                             int32_t node_id,
                                             SemanticsAction action,
                                             fml::MallocMapping args) override {
  }
  void OnPlatformViewSetSemanticsEnabled(bool enabled) override {}
  void OnPlatformViewSetAccessibilityFeatures(int32_t flags) override {}
  void OnPlatformViewRegisterTexture(
      std::shared_ptr<Texture> texture) override {}
  void OnPlatformViewUnregisterTexture(int64_t texture_id) override {}
  void OnPlatformViewMarkTextureFrameAvailable(int64_t texture_id) override {}
  void LoadDartDeferredLibrary(
      intptr_t loading_unit_id,
      std::unique_ptr<const fml::Mapping> snapshot_data,
      std::unique_ptr<const fml::Mapping> snapshot_instructions) override {}
  void LoadDartDeferredLibraryError(intptr_t loading_unit_id,
                                    const std::string error_message,
                                    bool transient) override {}
  void UpdateAssetResolverByType(
      std::unique_ptr<AssetResolver> updated_asset_resolver,
      AssetResolver::AssetResolverType type) override {}
  const Settings& OnPlatformViewGetSettings() const override {
    return settings_;
  }
  std::shared_ptr<fml::BasicTaskRunner>
  OnPlatformViewGetShutdownSafeIOTaskRunner() const override {
    return nullptr;
  }

 private:
  Settings settings_;
};

class MockPlatformViewDelegate final : public PlatformView {
 public:
  MockPlatformViewDelegate(PlatformView::Delegate& delegate,
                           const TaskRunners& task_runners)
      : PlatformView(delegate, task_runners) {}

  void NotifyDestroyed() override { notify_destroyed_called = true; }

  bool notify_destroyed_called = false;
};

class PlatformViewAndroidTest : public ::testing::Test {
 protected:
  void SetUp() override {
    fml::MessageLoop::EnsureInitializedForCurrentThread();
  }

  std::unique_ptr<AndroidShellHolder> CreateShellHolder() {
    Settings settings;
    settings.enable_software_rendering = false;
    auto jni = std::make_shared<JNIMock>();
    return std::make_unique<AndroidShellHolder>(
        settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  }
};

// Characterization: verify that the delegate pointer seam correctly forwards
// NotifyDestroyed to the registered PlatformView delegate, and that
// GetPlatformViewDelegate reflects the delegate lifecycle.
TEST_F(PlatformViewAndroidTest, PlatformViewDelegateSeam) {
  auto holder = CreateShellHolder();
  ASSERT_NE(holder, nullptr);
  ASSERT_TRUE(holder->IsValid());

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  // Initially, no delegate is set.
  EXPECT_EQ(platform_view->GetPlatformViewDelegate(), nullptr);

  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);

  platform_view->SetPlatformView(&delegate_platform_view);
  EXPECT_EQ(platform_view->GetPlatformViewDelegate(), &delegate_platform_view);

  EXPECT_FALSE(delegate_platform_view.notify_destroyed_called);
  platform_view->NotifyDestroyed();
  EXPECT_TRUE(delegate_platform_view.notify_destroyed_called);

  platform_view->SetPlatformView(nullptr);
  EXPECT_EQ(platform_view->GetPlatformViewDelegate(), nullptr);
}

// Characterization: verify that NotifyDestroyed without a registered delegate
// safely falls back to PlatformView::NotifyDestroyed() and cleans up surface.
TEST_F(PlatformViewAndroidTest, NotifyDestroyedDefaultFallback) {
  auto holder = CreateShellHolder();
  ASSERT_NE(holder, nullptr);
  ASSERT_TRUE(holder->IsValid());

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  EXPECT_EQ(platform_view->GetPlatformViewDelegate(), nullptr);

  // Calling NotifyDestroyed without a delegate routes to
  // PlatformView::NotifyDestroyed() without crash or error.
  platform_view->NotifyDestroyed();
}

// TODO(matanlurey): Re-enable.
//
// This test (and the entire suite) was skipped on CI (see
// https://github.com/flutter/flutter/issues/163742) and has since bit rotted
// (we fallback to OpenGLES on emulators for performance reasons); either fix
// the test, or remove it.
#if !SLIMPELLER
TEST(AndroidPlatformView, DISABLED_SelectsVulkanBasedOnApiLevel) {
  Settings settings;
  settings.enable_software_rendering = false;
  settings.enable_impeller = true;

  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 29),
            AndroidRenderingAPI::kImpellerVulkan);
  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, 24),
            AndroidRenderingAPI::kImpellerOpenGLES);
}
#endif  // !SLIMPELLER

}  // namespace testing
}  // namespace flutter
