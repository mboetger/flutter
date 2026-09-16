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

  void UpdateSemantics(int64_t view_id,
                       SemanticsNodeUpdates updates,
                       CustomAccessibilityActionUpdates actions) override {
    update_semantics_called = true;
    last_semantics_view_id = view_id;
  }

  void SetSemanticsTreeEnabled(bool enabled) override {
    set_semantics_tree_enabled_called = true;
    semantics_tree_enabled = enabled;
  }

  void SetApplicationLocale(std::string locale) override {
    set_application_locale_called = true;
    last_locale = std::move(locale);
  }

  std::unique_ptr<std::vector<std::string>> ComputePlatformResolvedLocales(
      const std::vector<std::string>& supported_locale_data) override {
    compute_locales_called = true;
    auto result = std::make_unique<std::vector<std::string>>();
    result->push_back("es");
    result->push_back("ES");
    result->push_back("");
    return result;
  }

  double GetScaledFontSize(double unscaled_font_size,
                           int configuration_id) const override {
    get_scaled_font_size_called = true;
    return unscaled_font_size * 2.0;
  }

  bool notify_destroyed_called = false;
  bool update_semantics_called = false;
  int64_t last_semantics_view_id = -1;
  bool set_semantics_tree_enabled_called = false;
  bool semantics_tree_enabled = false;
  bool set_application_locale_called = false;
  std::string last_locale;
  bool compute_locales_called = false;
  mutable bool get_scaled_font_size_called = false;
};

class PlatformViewAndroidTest : public ::testing::Test {
 protected:
  void SetUp() override {
    fml::MessageLoop::EnsureInitializedForCurrentThread();
  }

  std::unique_ptr<AndroidShellHolder> CreateShellHolder(
      std::shared_ptr<JNIMock> jni = nullptr) {
    Settings settings;
    settings.enable_software_rendering = false;
    if (!jni) {
      jni = std::make_shared<JNIMock>();
    }
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

// Characterization: verify that UpdateSemantics, SetSemanticsTreeEnabled,
// SetApplicationLocale, ComputePlatformResolvedLocales, and GetScaledFontSize
// forward to the registered PlatformView delegate when present.
TEST_F(PlatformViewAndroidTest, PlatformViewDelegateSemanticsAndLocaleSeam) {
  auto holder = CreateShellHolder();
  ASSERT_NE(holder, nullptr);
  ASSERT_TRUE(holder->IsValid());

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);

  platform_view->SetPlatformView(&delegate_platform_view);

  // 1. UpdateSemantics
  EXPECT_FALSE(delegate_platform_view.update_semantics_called);
  platform_view->UpdateSemantics(42, {}, {});
  EXPECT_TRUE(delegate_platform_view.update_semantics_called);
  EXPECT_EQ(delegate_platform_view.last_semantics_view_id, 42);

  // 2. SetSemanticsTreeEnabled
  EXPECT_FALSE(delegate_platform_view.set_semantics_tree_enabled_called);
  platform_view->SetSemanticsTreeEnabled(true);
  EXPECT_TRUE(delegate_platform_view.set_semantics_tree_enabled_called);
  EXPECT_TRUE(delegate_platform_view.semantics_tree_enabled);

  // 3. SetApplicationLocale
  EXPECT_FALSE(delegate_platform_view.set_application_locale_called);
  platform_view->SetApplicationLocale("es-ES");
  EXPECT_TRUE(delegate_platform_view.set_application_locale_called);
  EXPECT_EQ(delegate_platform_view.last_locale, "es-ES");

  // 4. ComputePlatformResolvedLocales
  EXPECT_FALSE(delegate_platform_view.compute_locales_called);
  auto resolved =
      platform_view->ComputePlatformResolvedLocales({"en", "US", ""});
  EXPECT_TRUE(delegate_platform_view.compute_locales_called);
  ASSERT_NE(resolved, nullptr);
  ASSERT_GE(resolved->size(), 2u);
  EXPECT_EQ((*resolved)[0], "es");
  EXPECT_EQ((*resolved)[1], "ES");

  // 5. GetScaledFontSize
  EXPECT_FALSE(delegate_platform_view.get_scaled_font_size_called);
  double scaled = platform_view->GetScaledFontSize(15.0, 1);
  EXPECT_TRUE(delegate_platform_view.get_scaled_font_size_called);
  EXPECT_DOUBLE_EQ(scaled, 30.0);

  platform_view->SetPlatformView(nullptr);
}

// Characterization: verify that UpdateSemantics, SetSemanticsTreeEnabled,
// SetApplicationLocale, ComputePlatformResolvedLocales, and GetScaledFontSize
// fall back to default JNI/delegate implementations when platform_view_ is
// null.
TEST_F(PlatformViewAndroidTest,
       PlatformViewDelegateSemanticsAndLocaleFallback) {
  auto jni = std::make_shared<JNIMock>();

  EXPECT_CALL(*jni, FlutterViewSetSemanticsTreeEnabled(true)).Times(1);
  EXPECT_CALL(*jni, FlutterViewSetApplicationLocale("fr-FR")).Times(1);
  EXPECT_CALL(*jni, FlutterViewComputePlatformResolvedLocale(::testing::_))
      .WillOnce([](std::vector<std::string> locales) {
        auto result = std::make_unique<std::vector<std::string>>();
        result->push_back("fr");
        result->push_back("FR");
        result->push_back("");
        return result;
      });
  EXPECT_CALL(*jni, FlutterViewGetScaledFontSize(12.0, 2))
      .WillOnce(::testing::Return(18.0));

  auto holder = CreateShellHolder(jni);
  ASSERT_NE(holder, nullptr);
  ASSERT_TRUE(holder->IsValid());

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);
  EXPECT_EQ(platform_view->GetPlatformViewDelegate(), nullptr);

  // Fallback calls:
  platform_view->UpdateSemantics(0, {}, {});
  platform_view->SetSemanticsTreeEnabled(true);
  platform_view->SetApplicationLocale("fr-FR");
  auto resolved =
      platform_view->ComputePlatformResolvedLocales({"fr", "FR", ""});
  ASSERT_NE(resolved, nullptr);
  ASSERT_GE(resolved->size(), 2u);
  EXPECT_EQ((*resolved)[0], "fr");
  EXPECT_EQ((*resolved)[1], "FR");
  EXPECT_DOUBLE_EQ(platform_view->GetScaledFontSize(12.0, 2), 18.0);
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
