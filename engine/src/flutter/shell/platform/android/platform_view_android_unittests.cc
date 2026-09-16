// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "flutter/fml/message_loop.h"
#include "flutter/shell/platform/android/android_compositor_adapter.h"
#include "flutter/shell/platform/android/android_external_texture_adapter.h"
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
      const ViewportMetrics& metrics) override {
    set_viewport_metrics_called = true;
    last_view_id = view_id;
    last_viewport_metrics = metrics;
  }
  bool set_viewport_metrics_called = false;
  int64_t last_view_id = -1;
  ViewportMetrics last_viewport_metrics = {};
  void OnPlatformViewDispatchPlatformMessage(
      std::unique_ptr<PlatformMessage> message) override {}
  void OnPlatformViewDispatchPointerDataPacket(
      std::unique_ptr<PointerDataPacket> packet) override {
    dispatch_pointer_data_packet_called = true;
    last_pointer_data_packet_length = packet ? packet->GetLength() : 0;
  }
  bool dispatch_pointer_data_packet_called = false;
  size_t last_pointer_data_packet_length = 0;
  HitTestResponse OnPlatformViewHitTest(
      int64_t view_id,
      const flutter::PointData offset) override {
    return {};
  }
  void OnPlatformViewDispatchSemanticsAction(int64_t view_id,
                                             int32_t node_id,
                                             SemanticsAction action,
                                             fml::MallocMapping args) override {
    dispatch_semantics_action_called = true;
    last_semantics_view_id = view_id;
    last_semantics_node_id = node_id;
    last_semantics_action = action;
    last_semantics_args_size = args.GetSize();
  }
  bool dispatch_semantics_action_called = false;
  int64_t last_semantics_view_id = -1;
  int32_t last_semantics_node_id = -1;
  SemanticsAction last_semantics_action = SemanticsAction::kTap;
  size_t last_semantics_args_size = 0;
  void OnPlatformViewSetSemanticsEnabled(bool enabled) override {
    set_semantics_enabled_called = true;
    last_semantics_enabled = enabled;
  }
  bool set_semantics_enabled_called = false;
  bool last_semantics_enabled = false;
  void OnPlatformViewSetAccessibilityFeatures(int32_t flags) override {
    set_accessibility_features_called = true;
    last_accessibility_features = flags;
  }
  bool set_accessibility_features_called = false;
  int32_t last_accessibility_features = 0;
  void OnPlatformViewRegisterTexture(
      std::shared_ptr<Texture> texture) override {}
  void OnPlatformViewUnregisterTexture(int64_t texture_id) override {
    unregister_texture_called = true;
    last_unregistered_texture_id = texture_id;
  }
  bool unregister_texture_called = false;
  int64_t last_unregistered_texture_id = -1;
  void OnPlatformViewMarkTextureFrameAvailable(int64_t texture_id) override {
    mark_texture_frame_available_called = true;
    last_frame_available_texture_id = texture_id;
  }
  bool mark_texture_frame_available_called = false;
  int64_t last_frame_available_texture_id = -1;
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

  void HandlePlatformMessage(
      std::unique_ptr<flutter::PlatformMessage> message) override {
    handle_platform_message_called = true;
    last_platform_message_channel = message ? message->channel() : "";
  }

  void OnPreEngineRestart() const override {
    on_pre_engine_restart_called = true;
  }

  void RequestDartDeferredLibrary(intptr_t loading_unit_id) override {
    request_dart_deferred_library_called = true;
    last_loading_unit_id = loading_unit_id;
  }

  void LoadDartDeferredLibrary(
      intptr_t loading_unit_id,
      std::unique_ptr<const fml::Mapping> snapshot_data,
      std::unique_ptr<const fml::Mapping> snapshot_instructions) override {
    load_dart_deferred_library_called = true;
    last_load_loading_unit_id = loading_unit_id;
  }

  void LoadDartDeferredLibraryError(intptr_t loading_unit_id,
                                    const std::string error_message,
                                    bool transient) override {
    load_dart_deferred_library_error_called = true;
    last_error_loading_unit_id = loading_unit_id;
    last_error_message = error_message;
    last_error_transient = transient;
  }

  void UpdateAssetResolverByType(
      std::unique_ptr<AssetResolver> updated_asset_resolver,
      AssetResolver::AssetResolverType type) override {
    update_asset_resolver_by_type_called = true;
    last_asset_resolver_type = type;
  }

  std::shared_ptr<PlatformMessageHandler> GetPlatformMessageHandler()
      const override {
    get_platform_message_handler_called = true;
    return nullptr;
  }

  void SetupImpellerContext() override { setup_impeller_context_called = true; }

  std::unique_ptr<VsyncWaiter> CreateVSyncWaiter() override {
    create_vsync_waiter_called = true;
    return nullptr;
  }

  std::unique_ptr<Surface> CreateRenderingSurface() override {
    create_rendering_surface_called = true;
    return nullptr;
  }

  std::shared_ptr<ExternalViewEmbedder> CreateExternalViewEmbedder() override {
    create_external_view_embedder_called = true;
    return nullptr;
  }

  std::unique_ptr<SnapshotSurfaceProducer> CreateSnapshotSurfaceProducer()
      override {
    create_snapshot_surface_producer_called = true;
    return nullptr;
  }

  sk_sp<GrDirectContext> CreateResourceContext() const override {
    create_resource_context_called = true;
    return nullptr;
  }

  void ReleaseResourceContext() const override {
    release_resource_context_called = true;
  }

  std::shared_ptr<impeller::Context> GetImpellerContext() const override {
    get_impeller_context_called = true;
    return nullptr;
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
  bool handle_platform_message_called = false;
  std::string last_platform_message_channel;
  mutable bool on_pre_engine_restart_called = false;
  bool request_dart_deferred_library_called = false;
  intptr_t last_loading_unit_id = -1;
  bool load_dart_deferred_library_called = false;
  intptr_t last_load_loading_unit_id = -1;
  bool load_dart_deferred_library_error_called = false;
  intptr_t last_error_loading_unit_id = -1;
  std::string last_error_message;
  bool last_error_transient = false;
  bool update_asset_resolver_by_type_called = false;
  AssetResolver::AssetResolverType last_asset_resolver_type =
      AssetResolver::AssetResolverType::kApkAssetProvider;
  mutable bool get_platform_message_handler_called = false;
  bool setup_impeller_context_called = false;
  bool create_vsync_waiter_called = false;
  bool create_rendering_surface_called = false;
  bool create_external_view_embedder_called = false;
  bool create_snapshot_surface_producer_called = false;
  mutable bool create_resource_context_called = false;
  mutable bool release_resource_context_called = false;
  mutable bool get_impeller_context_called = false;
};

class PlatformViewAndroidTest : public ::testing::Test {
 protected:
  void SetUp() override {
    fml::MessageLoop::EnsureInitializedForCurrentThread();
  }

  std::unique_ptr<AndroidShellHolder> CreateShellHolder(
      std::shared_ptr<JNIMock> jni = nullptr,
      Settings settings = {}) {
    settings.enable_software_rendering = false;
    if (!jni) {
      jni = std::make_shared<JNIMock>();
    }
    auto holder = std::make_unique<AndroidShellHolder>(
        settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
    holder->GetPlatformView()->SetPlatformView(nullptr);
    return holder;
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

// Characterization: verify that HandlePlatformMessage, OnPreEngineRestart,
// RequestDartDeferredLibrary, LoadDartDeferredLibrary,
// LoadDartDeferredLibraryError, and UpdateAssetResolverByType forward to the
// registered PlatformView delegate when present.
TEST_F(PlatformViewAndroidTest, PlatformViewDelegateMessagingAndDeferredSeam) {
  auto holder = CreateShellHolder();
  ASSERT_NE(holder, nullptr);
  ASSERT_TRUE(holder->IsValid());

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);

  platform_view->SetPlatformView(&delegate_platform_view);

  // 1. HandlePlatformMessage
  EXPECT_FALSE(delegate_platform_view.handle_platform_message_called);
  platform_view->HandlePlatformMessage(
      std::make_unique<flutter::PlatformMessage>("test_channel", nullptr));
  EXPECT_TRUE(delegate_platform_view.handle_platform_message_called);
  EXPECT_EQ(delegate_platform_view.last_platform_message_channel,
            "test_channel");

  // 2. OnPreEngineRestart
  EXPECT_FALSE(delegate_platform_view.on_pre_engine_restart_called);
  platform_view->OnPreEngineRestart();
  EXPECT_TRUE(delegate_platform_view.on_pre_engine_restart_called);

  // 3. RequestDartDeferredLibrary
  EXPECT_FALSE(delegate_platform_view.request_dart_deferred_library_called);
  platform_view->RequestDartDeferredLibrary(42);
  EXPECT_TRUE(delegate_platform_view.request_dart_deferred_library_called);
  EXPECT_EQ(delegate_platform_view.last_loading_unit_id, 42);

  // 4. LoadDartDeferredLibrary
  EXPECT_FALSE(delegate_platform_view.load_dart_deferred_library_called);
  platform_view->LoadDartDeferredLibrary(43, nullptr, nullptr);
  EXPECT_TRUE(delegate_platform_view.load_dart_deferred_library_called);
  EXPECT_EQ(delegate_platform_view.last_load_loading_unit_id, 43);

  // 5. LoadDartDeferredLibraryError
  EXPECT_FALSE(delegate_platform_view.load_dart_deferred_library_error_called);
  platform_view->LoadDartDeferredLibraryError(44, "failure", true);
  EXPECT_TRUE(delegate_platform_view.load_dart_deferred_library_error_called);
  EXPECT_EQ(delegate_platform_view.last_error_loading_unit_id, 44);
  EXPECT_EQ(delegate_platform_view.last_error_message, "failure");
  EXPECT_TRUE(delegate_platform_view.last_error_transient);

  // 6. UpdateAssetResolverByType
  EXPECT_FALSE(delegate_platform_view.update_asset_resolver_by_type_called);
  platform_view->UpdateAssetResolverByType(
      nullptr, AssetResolver::AssetResolverType::kApkAssetProvider);
  EXPECT_TRUE(delegate_platform_view.update_asset_resolver_by_type_called);
  EXPECT_EQ(delegate_platform_view.last_asset_resolver_type,
            AssetResolver::AssetResolverType::kApkAssetProvider);

  platform_view->SetPlatformView(nullptr);
}

// Characterization: verify that HandlePlatformMessage, OnPreEngineRestart,
// RequestDartDeferredLibrary, LoadDartDeferredLibrary,
// LoadDartDeferredLibraryError, and UpdateAssetResolverByType fall back
// to default implementations when platform_view_ is null.
TEST_F(PlatformViewAndroidTest,
       PlatformViewDelegateMessagingAndDeferredFallback) {
  auto jni = std::make_shared<JNIMock>();

  EXPECT_CALL(*jni,
              FlutterViewHandlePlatformMessage(::testing::_, ::testing::_))
      .Times(1);
  EXPECT_CALL(*jni, FlutterViewOnPreEngineRestart()).Times(1);
  EXPECT_CALL(*jni, RequestDartDeferredLibrary(101))
      .WillOnce(::testing::Return(true));

  auto holder = CreateShellHolder(jni);
  ASSERT_NE(holder, nullptr);
  ASSERT_TRUE(holder->IsValid());

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);
  EXPECT_EQ(platform_view->GetPlatformViewDelegate(), nullptr);

  // Fallback calls:
  platform_view->HandlePlatformMessage(
      std::make_unique<flutter::PlatformMessage>("fallback_channel", nullptr));
  platform_view->OnPreEngineRestart();
  platform_view->RequestDartDeferredLibrary(101);
  platform_view->LoadDartDeferredLibrary(102, nullptr, nullptr);
  platform_view->LoadDartDeferredLibraryError(103, "test_err", false);
  platform_view->UpdateAssetResolverByType(
      nullptr, AssetResolver::AssetResolverType::kApkAssetProvider);
}

// Characterization: verify that GetPlatformMessageHandler,
// SetupImpellerContext, CreateVSyncWaiter, CreateRenderingSurface,
// CreateExternalViewEmbedder, CreateSnapshotSurfaceProducer,
// CreateResourceContext, ReleaseResourceContext, and GetImpellerContext forward
// to the registered PlatformView delegate when present.
TEST_F(PlatformViewAndroidTest, PlatformViewDelegateSurfaceAndContextSeam) {
  auto holder = CreateShellHolder();
  ASSERT_NE(holder, nullptr);
  ASSERT_TRUE(holder->IsValid());

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);

  platform_view->SetPlatformView(&delegate_platform_view);

  // 1. GetPlatformMessageHandler
  EXPECT_FALSE(delegate_platform_view.get_platform_message_handler_called);
  platform_view->GetPlatformMessageHandler();
  EXPECT_TRUE(delegate_platform_view.get_platform_message_handler_called);

  // 2. SetupImpellerContext
  EXPECT_FALSE(delegate_platform_view.setup_impeller_context_called);
  platform_view->SetupImpellerContext();
  EXPECT_TRUE(delegate_platform_view.setup_impeller_context_called);

  // 3. CreateVSyncWaiter
  EXPECT_FALSE(delegate_platform_view.create_vsync_waiter_called);
  platform_view->CreateVSyncWaiter();
  EXPECT_TRUE(delegate_platform_view.create_vsync_waiter_called);

  // 4. CreateRenderingSurface
  EXPECT_FALSE(delegate_platform_view.create_rendering_surface_called);
  platform_view->CreateRenderingSurface();
  EXPECT_TRUE(delegate_platform_view.create_rendering_surface_called);

  // 5. CreateExternalViewEmbedder
  EXPECT_FALSE(delegate_platform_view.create_external_view_embedder_called);
  platform_view->CreateExternalViewEmbedder();
  EXPECT_TRUE(delegate_platform_view.create_external_view_embedder_called);

  // 6. CreateSnapshotSurfaceProducer
  EXPECT_FALSE(delegate_platform_view.create_snapshot_surface_producer_called);
  platform_view->CreateSnapshotSurfaceProducer();
  EXPECT_TRUE(delegate_platform_view.create_snapshot_surface_producer_called);

  // 7. CreateResourceContext
  EXPECT_FALSE(delegate_platform_view.create_resource_context_called);
  platform_view->CreateResourceContext();
  EXPECT_TRUE(delegate_platform_view.create_resource_context_called);

  // 8. ReleaseResourceContext
  EXPECT_FALSE(delegate_platform_view.release_resource_context_called);
  platform_view->ReleaseResourceContext();
  EXPECT_TRUE(delegate_platform_view.release_resource_context_called);

  // 9. GetImpellerContext
  EXPECT_FALSE(delegate_platform_view.get_impeller_context_called);
  platform_view->GetImpellerContext();
  EXPECT_TRUE(delegate_platform_view.get_impeller_context_called);

  platform_view->SetPlatformView(nullptr);
}

// Characterization: verify that GetPlatformMessageHandler,
// SetupImpellerContext, CreateVSyncWaiter, CreateExternalViewEmbedder,
// ReleaseResourceContext, and GetImpellerContext execute default fallback
// behavior when platform_view_ is null.
TEST_F(PlatformViewAndroidTest, PlatformViewDelegateSurfaceAndContextFallback) {
  auto holder = CreateShellHolder();
  ASSERT_NE(holder, nullptr);
  ASSERT_TRUE(holder->IsValid());

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);
  EXPECT_EQ(platform_view->GetPlatformViewDelegate(), nullptr);

  // Fallback calls:
  EXPECT_NE(platform_view->GetPlatformMessageHandler(), nullptr);
  EXPECT_NE(platform_view->CreateVSyncWaiter(), nullptr);
  EXPECT_NE(platform_view->CreateExternalViewEmbedder(), nullptr);
  EXPECT_NE(platform_view->GetCompositorAdapterForTesting(), nullptr);
  EXPECT_NE(
      platform_view->GetCompositorAdapterForTesting()->GetFlutterCompositor(),
      nullptr);
  EXPECT_NE(platform_view->GetExternalTextureAdapterForTesting(), nullptr);
  platform_view->ReleaseResourceContext();
  EXPECT_NE(platform_view->GetImpellerContext(), nullptr);
  platform_view->SetupImpellerContext();
}

TEST_F(PlatformViewAndroidTest, AndroidEmbedderApiFlagState) {
  // Flag off by default:
  {
    auto holder = CreateShellHolder();
    ASSERT_NE(holder, nullptr);
    EXPECT_FALSE(holder->IsAndroidEmbedderApiEnabled());
    auto platform_view = holder->GetPlatformView();
    ASSERT_TRUE(platform_view);
    EXPECT_FALSE(platform_view->IsAndroidEmbedderApiEnabled());
  }

  // Flag on when set in Settings:
  {
    Settings settings;
    settings.android_embedder_api = true;
    auto holder = CreateShellHolder(nullptr, settings);
    ASSERT_NE(holder, nullptr);
    EXPECT_TRUE(holder->IsAndroidEmbedderApiEnabled());
    auto platform_view = holder->GetPlatformView();
    ASSERT_TRUE(platform_view);
    EXPECT_TRUE(platform_view->IsAndroidEmbedderApiEnabled());
  }
}

TEST_F(PlatformViewAndroidTest, PointerDispatchLegacyPath) {
  Settings settings;
  settings.android_embedder_api = false;
  auto holder = CreateShellHolder(nullptr, settings);
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);

  platform_view->SetPlatformView(&delegate_platform_view);

  auto packet = std::make_unique<PointerDataPacket>(1);
  PointerData pointer_data;
  pointer_data.Clear();
  pointer_data.physical_x = 100.0;
  pointer_data.physical_y = 200.0;
  packet->SetPointerData(0, pointer_data);

  EXPECT_FALSE(fake_delegate.dispatch_pointer_data_packet_called);
  platform_view->DispatchPointerDataPacket(std::move(packet));
  EXPECT_TRUE(fake_delegate.dispatch_pointer_data_packet_called);
  EXPECT_EQ(fake_delegate.last_pointer_data_packet_length, 1ul);

  platform_view->SetPlatformView(nullptr);
}

TEST_F(PlatformViewAndroidTest, PointerDispatchEmbedderApiPath) {
  Settings settings;
  settings.android_embedder_api = true;
  auto holder = CreateShellHolder(nullptr, settings);
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);

  platform_view->SetPlatformView(&delegate_platform_view);

  auto packet = std::make_unique<PointerDataPacket>(1);
  PointerData pointer_data;
  pointer_data.Clear();
  pointer_data.physical_x = 150.0;
  pointer_data.physical_y = 250.0;
  pointer_data.change = PointerData::Change::kDown;
  pointer_data.kind = PointerData::DeviceKind::kTouch;
  packet->SetPointerData(0, pointer_data);

  // Dispatch through PlatformViewAndroid. In embedder-api mode, it routes
  // through SendPointerEvents to the underlying platform view.
  platform_view->DispatchPointerDataPacket(std::move(packet));
  EXPECT_TRUE(fake_delegate.dispatch_pointer_data_packet_called);
  EXPECT_EQ(fake_delegate.last_pointer_data_packet_length, 1ul);

  platform_view->SetPlatformView(nullptr);
}

TEST_F(PlatformViewAndroidTest, SendPointerEventsValidation) {
  auto holder = CreateShellHolder();
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  // Null events returns kInvalidArguments.
  EXPECT_EQ(platform_view->SendPointerEvents(nullptr, 1), kInvalidArguments);

  // Zero count returns kInvalidArguments.
  FlutterPointerEvent event = {};
  event.struct_size = sizeof(FlutterPointerEvent);
  EXPECT_EQ(platform_view->SendPointerEvents(&event, 0), kInvalidArguments);

  // Valid event returns kSuccess.
  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);
  platform_view->SetPlatformView(&delegate_platform_view);

  event.phase = FlutterPointerPhase::kDown;
  event.x = 50.0;
  event.y = 75.0;
  event.device_kind = kFlutterPointerDeviceKindTouch;
  EXPECT_EQ(platform_view->SendPointerEvents(&event, 1), kSuccess);
  EXPECT_TRUE(fake_delegate.dispatch_pointer_data_packet_called);
  EXPECT_EQ(fake_delegate.last_pointer_data_packet_length, 1ul);

  platform_view->SetPlatformView(nullptr);
}

TEST_F(PlatformViewAndroidTest, WindowMetricsDispatchLegacyPath) {
  Settings settings;
  settings.android_embedder_api = false;
  auto holder = CreateShellHolder(nullptr, settings);
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);

  platform_view->SetPlatformView(&delegate_platform_view);

  ViewportMetrics metrics;
  metrics.physical_width = 1080;
  metrics.physical_height = 1920;
  metrics.device_pixel_ratio = 2.0;

  EXPECT_FALSE(fake_delegate.set_viewport_metrics_called);
  platform_view->SetViewportMetrics(0, metrics);
  EXPECT_TRUE(fake_delegate.set_viewport_metrics_called);
  EXPECT_EQ(fake_delegate.last_view_id, 0);
  EXPECT_EQ(fake_delegate.last_viewport_metrics.physical_width, 1080);
  EXPECT_EQ(fake_delegate.last_viewport_metrics.physical_height, 1920);
  EXPECT_EQ(fake_delegate.last_viewport_metrics.device_pixel_ratio, 2.0);

  platform_view->SetPlatformView(nullptr);
}

TEST_F(PlatformViewAndroidTest, WindowMetricsDispatchEmbedderApiPath) {
  Settings settings;
  settings.android_embedder_api = true;
  auto holder = CreateShellHolder(nullptr, settings);
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);

  platform_view->SetPlatformView(&delegate_platform_view);

  ViewportMetrics metrics;
  metrics.physical_width = 1200;
  metrics.physical_height = 2000;
  metrics.device_pixel_ratio = 2.5;

  EXPECT_FALSE(fake_delegate.set_viewport_metrics_called);
  platform_view->SetViewportMetrics(0, metrics);
  EXPECT_TRUE(fake_delegate.set_viewport_metrics_called);
  EXPECT_EQ(fake_delegate.last_view_id, 0);
  EXPECT_EQ(fake_delegate.last_viewport_metrics.physical_width, 1200);
  EXPECT_EQ(fake_delegate.last_viewport_metrics.physical_height, 2000);
  EXPECT_EQ(fake_delegate.last_viewport_metrics.device_pixel_ratio, 2.5);

  platform_view->SetPlatformView(nullptr);
}

TEST_F(PlatformViewAndroidTest, SendWindowMetricsEventValidation) {
  auto holder = CreateShellHolder();
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  // Null event returns kInvalidArguments.
  EXPECT_EQ(platform_view->SendWindowMetricsEvent(nullptr), kInvalidArguments);

  // Invalid pixel ratio returns kInvalidArguments.
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(FlutterWindowMetricsEvent);
  event.width = 100;
  event.height = 100;
  event.pixel_ratio = 0.0;
  EXPECT_EQ(platform_view->SendWindowMetricsEvent(&event), kInvalidArguments);

  // Negative insets return kInvalidArguments.
  event.pixel_ratio = 1.0;
  event.physical_view_inset_top = -1.0;
  EXPECT_EQ(platform_view->SendWindowMetricsEvent(&event), kInvalidArguments);

  // Valid event returns kSuccess.
  event.physical_view_inset_top = 10.0;
  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);
  platform_view->SetPlatformView(&delegate_platform_view);

  EXPECT_EQ(platform_view->SendWindowMetricsEvent(&event), kSuccess);
  EXPECT_TRUE(fake_delegate.set_viewport_metrics_called);
  EXPECT_EQ(fake_delegate.last_viewport_metrics.physical_width, 100);
  EXPECT_EQ(fake_delegate.last_viewport_metrics.physical_height, 100);
  EXPECT_EQ(fake_delegate.last_viewport_metrics.physical_view_inset_top, 10.0);

  platform_view->SetPlatformView(nullptr);
}

TEST_F(PlatformViewAndroidTest, SemanticsActionDispatchValidation) {
  auto holder = CreateShellHolder();
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  // Null data with positive length returns kInvalidArguments.
  EXPECT_EQ(platform_view->DispatchSemanticsAction(
                1, kFlutterSemanticsActionTap, nullptr, 10),
            kInvalidArguments);

  // Null data with zero length returns kSuccess.
  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);
  platform_view->SetPlatformView(&delegate_platform_view);

  EXPECT_FALSE(fake_delegate.dispatch_semantics_action_called);
  EXPECT_EQ(platform_view->DispatchSemanticsAction(
                42, kFlutterSemanticsActionTap, nullptr, 0),
            kSuccess);
  EXPECT_TRUE(fake_delegate.dispatch_semantics_action_called);
  EXPECT_EQ(fake_delegate.last_semantics_view_id, 0);
  EXPECT_EQ(fake_delegate.last_semantics_node_id, 42);
  EXPECT_EQ(fake_delegate.last_semantics_action, SemanticsAction::kTap);
  EXPECT_EQ(fake_delegate.last_semantics_args_size, 0ul);

  platform_view->SetPlatformView(nullptr);
}

TEST_F(PlatformViewAndroidTest, SemanticsActionDispatchWithPayload) {
  auto holder = CreateShellHolder();
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);
  platform_view->SetPlatformView(&delegate_platform_view);

  const uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
  EXPECT_FALSE(fake_delegate.dispatch_semantics_action_called);
  EXPECT_EQ(
      platform_view->DispatchSemanticsAction(
          100, kFlutterSemanticsActionCustomAction, payload, sizeof(payload)),
      kSuccess);
  EXPECT_TRUE(fake_delegate.dispatch_semantics_action_called);
  EXPECT_EQ(fake_delegate.last_semantics_view_id, 0);
  EXPECT_EQ(fake_delegate.last_semantics_node_id, 100);
  EXPECT_EQ(fake_delegate.last_semantics_action,
            SemanticsAction::kCustomAction);
  EXPECT_EQ(fake_delegate.last_semantics_args_size, sizeof(payload));

  platform_view->SetPlatformView(nullptr);
}

TEST_F(PlatformViewAndroidTest, SemanticsEnabledDispatchLegacyPath) {
  Settings settings;
  settings.android_embedder_api = false;
  auto holder = CreateShellHolder(nullptr, settings);
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);
  platform_view->SetPlatformView(&delegate_platform_view);

  EXPECT_FALSE(fake_delegate.set_semantics_enabled_called);
  platform_view->SetSemanticsEnabled(true);
  EXPECT_TRUE(fake_delegate.set_semantics_enabled_called);
  EXPECT_TRUE(fake_delegate.last_semantics_enabled);

  platform_view->SetSemanticsEnabled(false);
  EXPECT_FALSE(fake_delegate.last_semantics_enabled);

  platform_view->SetPlatformView(nullptr);
}

TEST_F(PlatformViewAndroidTest, SemanticsEnabledDispatchEmbedderApiPath) {
  Settings settings;
  settings.android_embedder_api = true;
  auto holder = CreateShellHolder(nullptr, settings);
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);
  platform_view->SetPlatformView(&delegate_platform_view);

  EXPECT_FALSE(fake_delegate.set_semantics_enabled_called);
  platform_view->SetSemanticsEnabled(true);
  EXPECT_TRUE(fake_delegate.set_semantics_enabled_called);
  EXPECT_TRUE(fake_delegate.last_semantics_enabled);

  platform_view->SetSemanticsEnabled(false);
  EXPECT_FALSE(fake_delegate.last_semantics_enabled);

  platform_view->SetPlatformView(nullptr);
}

TEST_F(PlatformViewAndroidTest, UpdateSemanticsEnabledDirect) {
  auto holder = CreateShellHolder();
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);
  platform_view->SetPlatformView(&delegate_platform_view);

  EXPECT_FALSE(fake_delegate.set_semantics_enabled_called);
  EXPECT_EQ(platform_view->UpdateSemanticsEnabled(true), kSuccess);
  EXPECT_TRUE(fake_delegate.set_semantics_enabled_called);
  EXPECT_TRUE(fake_delegate.last_semantics_enabled);

  EXPECT_EQ(platform_view->UpdateSemanticsEnabled(false), kSuccess);
  EXPECT_FALSE(fake_delegate.last_semantics_enabled);

  platform_view->SetPlatformView(nullptr);
}

TEST_F(PlatformViewAndroidTest, AccessibilityFeaturesDispatchLegacyPath) {
  Settings settings;
  settings.android_embedder_api = false;
  auto holder = CreateShellHolder(nullptr, settings);
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);
  platform_view->SetPlatformView(&delegate_platform_view);

  EXPECT_FALSE(fake_delegate.set_accessibility_features_called);
  platform_view->SetAccessibilityFeatures(
      kFlutterAccessibilityFeatureAccessibleNavigation |
      kFlutterAccessibilityFeatureInvertColors);
  EXPECT_TRUE(fake_delegate.set_accessibility_features_called);
  EXPECT_EQ(fake_delegate.last_accessibility_features,
            kFlutterAccessibilityFeatureAccessibleNavigation |
                kFlutterAccessibilityFeatureInvertColors);

  platform_view->SetAccessibilityFeatures(0);
  EXPECT_EQ(fake_delegate.last_accessibility_features, 0);

  platform_view->SetPlatformView(nullptr);
}

TEST_F(PlatformViewAndroidTest, AccessibilityFeaturesDispatchEmbedderApiPath) {
  Settings settings;
  settings.android_embedder_api = true;
  auto holder = CreateShellHolder(nullptr, settings);
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);
  platform_view->SetPlatformView(&delegate_platform_view);

  EXPECT_FALSE(fake_delegate.set_accessibility_features_called);
  platform_view->SetAccessibilityFeatures(
      kFlutterAccessibilityFeatureAccessibleNavigation |
      kFlutterAccessibilityFeatureBoldText);
  EXPECT_TRUE(fake_delegate.set_accessibility_features_called);
  EXPECT_EQ(fake_delegate.last_accessibility_features,
            kFlutterAccessibilityFeatureAccessibleNavigation |
                kFlutterAccessibilityFeatureBoldText);

  platform_view->SetAccessibilityFeatures(0);
  EXPECT_EQ(fake_delegate.last_accessibility_features, 0);

  platform_view->SetPlatformView(nullptr);
}

TEST_F(PlatformViewAndroidTest, UpdateAccessibilityFeaturesDirect) {
  auto holder = CreateShellHolder();
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);
  platform_view->SetPlatformView(&delegate_platform_view);

  EXPECT_FALSE(fake_delegate.set_accessibility_features_called);
  EXPECT_EQ(platform_view->UpdateAccessibilityFeatures(
                kFlutterAccessibilityFeatureReduceMotion),
            kSuccess);
  EXPECT_TRUE(fake_delegate.set_accessibility_features_called);
  EXPECT_EQ(fake_delegate.last_accessibility_features,
            kFlutterAccessibilityFeatureReduceMotion);

  EXPECT_EQ(platform_view->UpdateAccessibilityFeatures(
                static_cast<FlutterAccessibilityFeature>(0)),
            kSuccess);
  EXPECT_EQ(fake_delegate.last_accessibility_features, 0);

  platform_view->SetPlatformView(nullptr);
}

TEST_F(PlatformViewAndroidTest, UnregisterTextureDispatchLegacyPath) {
  Settings settings;
  settings.android_embedder_api = false;
  auto holder = CreateShellHolder(nullptr, settings);
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);
  platform_view->SetPlatformView(&delegate_platform_view);

  EXPECT_FALSE(fake_delegate.unregister_texture_called);
  platform_view->UnregisterTexture(101);
  EXPECT_TRUE(fake_delegate.unregister_texture_called);
  EXPECT_EQ(fake_delegate.last_unregistered_texture_id, 101);

  platform_view->SetPlatformView(nullptr);
}

TEST_F(PlatformViewAndroidTest, UnregisterTextureDispatchEmbedderApiPath) {
  Settings settings;
  settings.android_embedder_api = true;
  auto holder = CreateShellHolder(nullptr, settings);
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);
  platform_view->SetPlatformView(&delegate_platform_view);

  EXPECT_FALSE(fake_delegate.unregister_texture_called);
  platform_view->UnregisterTexture(102);
  EXPECT_TRUE(fake_delegate.unregister_texture_called);
  EXPECT_EQ(fake_delegate.last_unregistered_texture_id, 102);

  platform_view->SetPlatformView(nullptr);
}

TEST_F(PlatformViewAndroidTest, UnregisterExternalTextureDirect) {
  auto holder = CreateShellHolder();
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);
  platform_view->SetPlatformView(&delegate_platform_view);

  EXPECT_FALSE(fake_delegate.unregister_texture_called);
  EXPECT_EQ(platform_view->UnregisterExternalTexture(103), kSuccess);
  EXPECT_TRUE(fake_delegate.unregister_texture_called);
  EXPECT_EQ(fake_delegate.last_unregistered_texture_id, 103);

  platform_view->SetPlatformView(nullptr);
}

TEST_F(PlatformViewAndroidTest, MarkTextureFrameAvailableDispatchLegacyPath) {
  Settings settings;
  settings.android_embedder_api = false;
  auto holder = CreateShellHolder(nullptr, settings);
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);
  platform_view->SetPlatformView(&delegate_platform_view);

  EXPECT_FALSE(fake_delegate.mark_texture_frame_available_called);
  platform_view->MarkTextureFrameAvailable(201);
  EXPECT_TRUE(fake_delegate.mark_texture_frame_available_called);
  EXPECT_EQ(fake_delegate.last_frame_available_texture_id, 201);

  platform_view->SetPlatformView(nullptr);
}

TEST_F(PlatformViewAndroidTest,
       MarkTextureFrameAvailableDispatchEmbedderApiPath) {
  Settings settings;
  settings.android_embedder_api = true;
  auto holder = CreateShellHolder(nullptr, settings);
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);
  platform_view->SetPlatformView(&delegate_platform_view);

  EXPECT_FALSE(fake_delegate.mark_texture_frame_available_called);
  platform_view->MarkTextureFrameAvailable(202);
  EXPECT_TRUE(fake_delegate.mark_texture_frame_available_called);
  EXPECT_EQ(fake_delegate.last_frame_available_texture_id, 202);

  platform_view->SetPlatformView(nullptr);
}

TEST_F(PlatformViewAndroidTest, MarkExternalTextureFrameAvailableDirect) {
  auto holder = CreateShellHolder();
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  FakePlatformViewDelegate fake_delegate;
  const auto& task_runners = holder->GetShellForTesting()->GetTaskRunners();
  MockPlatformViewDelegate delegate_platform_view(fake_delegate, task_runners);
  platform_view->SetPlatformView(&delegate_platform_view);

  EXPECT_FALSE(fake_delegate.mark_texture_frame_available_called);
  EXPECT_EQ(platform_view->MarkExternalTextureFrameAvailable(203), kSuccess);
  EXPECT_TRUE(fake_delegate.mark_texture_frame_available_called);
  EXPECT_EQ(fake_delegate.last_frame_available_texture_id, 203);

  platform_view->SetPlatformView(nullptr);
}

TEST_F(PlatformViewAndroidTest, TextureSeamInvalidArguments) {
  auto holder = CreateShellHolder();
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  EXPECT_EQ(platform_view->UnregisterExternalTexture(0), kInvalidArguments);
  EXPECT_EQ(platform_view->UnregisterExternalTexture(-1), kInvalidArguments);
  EXPECT_EQ(platform_view->MarkExternalTextureFrameAvailable(0),
            kInvalidArguments);
  EXPECT_EQ(platform_view->MarkExternalTextureFrameAvailable(-1),
            kInvalidArguments);
}

TEST_F(PlatformViewAndroidTest, RegisterImageTextureLegacyPath) {
  Settings settings;
  settings.android_embedder_api = false;
  auto holder = CreateShellHolder(nullptr, settings);
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  fml::jni::ScopedJavaGlobalRef<jobject> null_ref;
  platform_view->RegisterImageTexture(
      301, null_ref, ImageExternalTexture::ImageLifecycle::kKeepAlive);
}

TEST_F(PlatformViewAndroidTest, RegisterImageTextureEmbedderApiPath) {
  Settings settings;
  settings.android_embedder_api = true;
  auto holder = CreateShellHolder(nullptr, settings);
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  fml::jni::ScopedJavaGlobalRef<jobject> null_ref;
  platform_view->RegisterImageTexture(
      302, null_ref, ImageExternalTexture::ImageLifecycle::kKeepAlive);
}

TEST_F(PlatformViewAndroidTest, RegisterImageExternalTextureDirect) {
  auto holder = CreateShellHolder();
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  fml::jni::ScopedJavaGlobalRef<jobject> null_ref;
  // Null image_texture_entry returns kInvalidArguments.
  EXPECT_EQ(
      platform_view->RegisterImageExternalTexture(
          303, null_ref, ImageExternalTexture::ImageLifecycle::kKeepAlive),
      kInvalidArguments);
}

TEST_F(PlatformViewAndroidTest, RegisterImageExternalTextureInvalidArguments) {
  auto holder = CreateShellHolder();
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  fml::jni::ScopedJavaGlobalRef<jobject> null_ref;
  EXPECT_EQ(platform_view->RegisterImageExternalTexture(
                0, null_ref, ImageExternalTexture::ImageLifecycle::kKeepAlive),
            kInvalidArguments);
  EXPECT_EQ(platform_view->RegisterImageExternalTexture(
                -1, null_ref, ImageExternalTexture::ImageLifecycle::kKeepAlive),
            kInvalidArguments);
}

TEST_F(PlatformViewAndroidTest, RegisterSurfaceTextureLegacyPath) {
  Settings settings;
  settings.android_embedder_api = false;
  auto holder = CreateShellHolder(nullptr, settings);
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  fml::jni::ScopedJavaGlobalRef<jobject> null_ref;
  platform_view->RegisterExternalTexture(401, null_ref);
}

TEST_F(PlatformViewAndroidTest, RegisterSurfaceTextureEmbedderApiPath) {
  Settings settings;
  settings.android_embedder_api = true;
  auto holder = CreateShellHolder(nullptr, settings);
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  fml::jni::ScopedJavaGlobalRef<jobject> null_ref;
  platform_view->RegisterExternalTexture(402, null_ref);
}

TEST_F(PlatformViewAndroidTest, RegisterSurfaceExternalTextureDirect) {
  auto holder = CreateShellHolder();
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  fml::jni::ScopedJavaGlobalRef<jobject> null_ref;
  // Null surface_texture returns kInvalidArguments.
  EXPECT_EQ(platform_view->RegisterSurfaceExternalTexture(403, null_ref),
            kInvalidArguments);
}

TEST_F(PlatformViewAndroidTest,
       RegisterSurfaceExternalTextureInvalidArguments) {
  auto holder = CreateShellHolder();
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  fml::jni::ScopedJavaGlobalRef<jobject> null_ref;
  EXPECT_EQ(platform_view->RegisterSurfaceExternalTexture(0, null_ref),
            kInvalidArguments);
  EXPECT_EQ(platform_view->RegisterSurfaceExternalTexture(-1, null_ref),
            kInvalidArguments);
}

TEST_F(PlatformViewAndroidTest, DisplayPlatformViewLegacyPath) {
  auto jni = std::make_shared<JNIMock>();
  EXPECT_CALL(*jni, FlutterViewOnDisplayPlatformView(1, 10, 20, 100, 200, 300,
                                                     400, ::testing::_))
      .Times(1);

  Settings settings;
  settings.android_embedder_api = false;
  auto holder = CreateShellHolder(jni, settings);
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  MutatorsStack stack;
  platform_view->OnDisplayPlatformView(1, 10, 20, 100, 200, 300, 400, stack);
}

TEST_F(PlatformViewAndroidTest, DisplayPlatformViewEmbedderApiPath) {
  auto jni = std::make_shared<JNIMock>();
  EXPECT_CALL(*jni, FlutterViewOnDisplayPlatformView(2, 10, 20, 100, 200, 300,
                                                     400, ::testing::_))
      .Times(1);

  Settings settings;
  settings.android_embedder_api = true;
  auto holder = CreateShellHolder(jni, settings);
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  MutatorsStack stack;
  platform_view->OnDisplayPlatformView(2, 10, 20, 100, 200, 300, 400, stack);
}

TEST_F(PlatformViewAndroidTest, DisplayPlatformViewDirectSeam) {
  auto jni = std::make_shared<JNIMock>();
  EXPECT_CALL(*jni, FlutterViewOnDisplayPlatformView(3, 10, 20, 100, 200, 300,
                                                     400, ::testing::_))
      .Times(2);

  auto holder = CreateShellHolder(jni);
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  MutatorsStack stack1;
  EXPECT_EQ(
      platform_view->DisplayPlatformView(3, 10, 20, 100, 200, 300, 400, stack1),
      kSuccess);

  MutatorsStack stack2;
  EXPECT_EQ(platform_view->DisplayPlatformViewEmbedder(3, 10, 20, 100, 200, 300,
                                                       400, stack2),
            kSuccess);
}

TEST_F(PlatformViewAndroidTest, DisplayPlatformViewInvalidArguments) {
  auto holder = CreateShellHolder();
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  MutatorsStack stack;
  EXPECT_EQ(
      platform_view->DisplayPlatformView(-1, 0, 0, 100, 100, 100, 100, stack),
      kInvalidArguments);
  EXPECT_EQ(
      platform_view->DisplayPlatformView(1, 0, 0, -1, 100, 100, 100, stack),
      kInvalidArguments);
  EXPECT_EQ(
      platform_view->DisplayPlatformView(1, 0, 0, 100, -1, 100, 100, stack),
      kInvalidArguments);
  EXPECT_EQ(
      platform_view->DisplayPlatformView(1, 0, 0, 100, 100, -1, 100, stack),
      kInvalidArguments);
  EXPECT_EQ(
      platform_view->DisplayPlatformView(1, 0, 0, 100, 100, 100, -1, stack),
      kInvalidArguments);
}

TEST_F(PlatformViewAndroidTest, DisplayOverlaySurfaceLegacyPath) {
  auto jni = std::make_shared<JNIMock>();
  EXPECT_CALL(*jni, FlutterViewDisplayOverlaySurface(10, 5, 15, 50, 60))
      .Times(1);

  Settings settings;
  settings.android_embedder_api = false;
  auto holder = CreateShellHolder(jni, settings);
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  platform_view->OnDisplayOverlaySurface(10, 5, 15, 50, 60);
}

TEST_F(PlatformViewAndroidTest, DisplayOverlaySurfaceEmbedderApiPath) {
  auto jni = std::make_shared<JNIMock>();
  EXPECT_CALL(*jni, FlutterViewDisplayOverlaySurface(20, 5, 15, 50, 60))
      .Times(1);

  Settings settings;
  settings.android_embedder_api = true;
  auto holder = CreateShellHolder(jni, settings);
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  platform_view->OnDisplayOverlaySurface(20, 5, 15, 50, 60);
}

TEST_F(PlatformViewAndroidTest, DisplayOverlaySurfaceDirectSeam) {
  auto jni = std::make_shared<JNIMock>();
  EXPECT_CALL(*jni, FlutterViewDisplayOverlaySurface(30, 5, 15, 50, 60))
      .Times(1);

  auto holder = CreateShellHolder(jni);
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  EXPECT_EQ(platform_view->DisplayOverlaySurface(30, 5, 15, 50, 60), kSuccess);
}

TEST_F(PlatformViewAndroidTest, DisplayOverlaySurfaceInvalidArguments) {
  auto holder = CreateShellHolder();
  ASSERT_NE(holder, nullptr);

  auto platform_view = holder->GetPlatformView();
  ASSERT_TRUE(platform_view);

  EXPECT_EQ(platform_view->DisplayOverlaySurface(-1, 0, 0, 50, 60),
            kInvalidArguments);
  EXPECT_EQ(platform_view->DisplayOverlaySurface(1, 0, 0, -1, 60),
            kInvalidArguments);
  EXPECT_EQ(platform_view->DisplayOverlaySurface(1, 0, 0, 50, -1),
            kInvalidArguments);
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
