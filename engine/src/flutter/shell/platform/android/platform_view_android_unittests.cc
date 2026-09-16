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
      const ViewportMetrics& metrics) override {}
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
