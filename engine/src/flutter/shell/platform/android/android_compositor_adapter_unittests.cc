// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_compositor_adapter.h"

#include "flutter/shell/platform/embedder/embedder.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

namespace {

class MockExternalViewEmbedder : public ExternalViewEmbedder {
 public:
  bool preroll_called = false;
  int64_t preroll_view_id = -1;
  bool composite_called = false;
  int64_t composite_view_id = -1;
  bool submit_called = false;
  int64_t submit_view_id = -1;
  bool post_preroll_called = false;
  bool get_root_canvas_called = false;
  bool begin_frame_called = false;
  bool prepare_called = false;
  bool cancel_called = false;
  bool end_frame_called = false;
  bool supports_dynamic_merging_called = false;
  bool supports_dynamic_merging_return = true;
  bool teardown_called = false;
  bool collect_view_called = false;
  int64_t collect_view_id = -1;
  bool push_visited_called = false;
  bool push_filter_called = false;
  bool push_clip_rect_called = false;
  bool push_clip_rrect_called = false;
  bool push_clip_rse_called = false;
  bool push_clip_path_called = false;

  void PrerollCompositeEmbeddedView(
      int64_t view_id,
      std::unique_ptr<EmbeddedViewParams> params) override {
    preroll_called = true;
    preroll_view_id = view_id;
  }

  DlCanvas* CompositeEmbeddedView(int64_t view_id) override {
    composite_called = true;
    composite_view_id = view_id;
    return nullptr;
  }

  void SubmitFlutterView(
      int64_t flutter_view_id,
      GrDirectContext* context,
      const std::shared_ptr<impeller::AiksContext>& aiks_context,
      std::unique_ptr<SurfaceFrame> frame) override {
    submit_called = true;
    submit_view_id = flutter_view_id;
  }

  PostPrerollResult PostPrerollAction(
      const fml::RefPtr<fml::RasterThreadMerger>& raster_thread_merger)
      override {
    post_preroll_called = true;
    return PostPrerollResult::kResubmitFrame;
  }

  DlCanvas* GetRootCanvas() override {
    get_root_canvas_called = true;
    return nullptr;
  }

  void BeginFrame(GrDirectContext* context,
                  const fml::RefPtr<fml::RasterThreadMerger>&
                      raster_thread_merger) override {
    begin_frame_called = true;
  }

  void PrepareFlutterView(DlISize frame_size,
                          double device_pixel_ratio) override {
    prepare_called = true;
  }

  void CancelFrame() override { cancel_called = true; }

  void EndFrame(bool should_resubmit_frame,
                const fml::RefPtr<fml::RasterThreadMerger>&
                    raster_thread_merger) override {
    end_frame_called = true;
  }

  bool SupportsDynamicThreadMerging() override {
    supports_dynamic_merging_called = true;
    return supports_dynamic_merging_return;
  }

  void Teardown() override { teardown_called = true; }

  void CollectView(int64_t view_id) override {
    collect_view_called = true;
    collect_view_id = view_id;
  }

  void PushVisitedPlatformView(int64_t platform_view_id) override {
    push_visited_called = true;
  }

  void PushFilterToVisitedPlatformViews(
      const std::shared_ptr<DlImageFilter>& filter,
      const DlRect& filter_rect) override {
    push_filter_called = true;
  }

  void PushClipRectToVisitedPlatformViews(const DlRect& clip_rect) override {
    push_clip_rect_called = true;
  }

  void PushClipRRectToVisitedPlatformViews(
      const DlRoundRect& clip_rrect) override {
    push_clip_rrect_called = true;
  }

  void PushClipRSuperellipseToVisitedPlatformViews(
      const DlRoundSuperellipse& clip_rse) override {
    push_clip_rse_called = true;
  }

  void PushClipPathToVisitedPlatformViews(const DlPath& clip_path) override {
    push_clip_path_called = true;
  }
};

}  // namespace

TEST(AndroidCompositorAdapterTest, FlutterCompositorStructShape) {
  auto mock_embedder = std::make_shared<MockExternalViewEmbedder>();
  mock_embedder->supports_dynamic_merging_return = true;

  auto adapter = std::make_shared<AndroidCompositorAdapter>(mock_embedder);
  const FlutterCompositor* compositor = adapter->GetFlutterCompositor();
  ASSERT_NE(compositor, nullptr);

  EXPECT_EQ(compositor->struct_size, sizeof(FlutterCompositor));
  EXPECT_EQ(compositor->user_data, adapter.get());
  EXPECT_NE(compositor->create_backing_store_callback, nullptr);
  EXPECT_NE(compositor->collect_backing_store_callback, nullptr);
  EXPECT_EQ(compositor->present_layers_callback, nullptr);
  EXPECT_FALSE(compositor->avoid_backing_store_cache);
  EXPECT_NE(compositor->present_view_callback, nullptr);
  EXPECT_TRUE(compositor->supports_dynamic_thread_merging);
  EXPECT_NE(compositor->post_preroll_callback, nullptr);
  EXPECT_NE(compositor->begin_frame_callback, nullptr);
  EXPECT_NE(compositor->end_frame_callback, nullptr);
}

TEST(AndroidCompositorAdapterTest,
     SupportsDynamicThreadMergingReflectsEmbedder) {
  auto mock_embedder = std::make_shared<MockExternalViewEmbedder>();
  auto adapter = std::make_shared<AndroidCompositorAdapter>(mock_embedder);

  mock_embedder->supports_dynamic_merging_return = true;
  EXPECT_TRUE(adapter->SupportsDynamicThreadMerging());
  EXPECT_TRUE(adapter->GetFlutterCompositor()->supports_dynamic_thread_merging);

  mock_embedder->supports_dynamic_merging_return = false;
  EXPECT_FALSE(adapter->SupportsDynamicThreadMerging());
  EXPECT_FALSE(
      adapter->GetFlutterCompositor()->supports_dynamic_thread_merging);
}

TEST(AndroidCompositorAdapterTest, TransparentForwarding) {
  auto mock_embedder = std::make_shared<MockExternalViewEmbedder>();
  auto adapter = std::make_shared<AndroidCompositorAdapter>(mock_embedder);

  EXPECT_EQ(adapter->GetViewEmbedder(), mock_embedder);

  adapter->PrerollCompositeEmbeddedView(42, nullptr);
  EXPECT_TRUE(mock_embedder->preroll_called);
  EXPECT_EQ(mock_embedder->preroll_view_id, 42);

  adapter->CompositeEmbeddedView(42);
  EXPECT_TRUE(mock_embedder->composite_called);
  EXPECT_EQ(mock_embedder->composite_view_id, 42);

  adapter->SubmitFlutterView(1, nullptr, nullptr, nullptr);
  EXPECT_TRUE(mock_embedder->submit_called);
  EXPECT_EQ(mock_embedder->submit_view_id, 1);

  PostPrerollResult post_result = adapter->PostPrerollAction(nullptr);
  EXPECT_TRUE(mock_embedder->post_preroll_called);
  EXPECT_EQ(post_result, PostPrerollResult::kResubmitFrame);

  adapter->GetRootCanvas();
  EXPECT_TRUE(mock_embedder->get_root_canvas_called);

  EXPECT_FALSE(adapter->GetUsedThisFrame());
  adapter->BeginFrame(nullptr, nullptr);
  EXPECT_TRUE(mock_embedder->begin_frame_called);
  EXPECT_TRUE(adapter->GetUsedThisFrame());

  adapter->PrepareFlutterView(DlISize(100, 200), 2.0);
  EXPECT_TRUE(mock_embedder->prepare_called);

  adapter->CancelFrame();
  EXPECT_TRUE(mock_embedder->cancel_called);

  adapter->EndFrame(false, nullptr);
  EXPECT_TRUE(mock_embedder->end_frame_called);
  EXPECT_FALSE(adapter->GetUsedThisFrame());

  adapter->Teardown();
  EXPECT_TRUE(mock_embedder->teardown_called);

  adapter->CollectView(99);
  EXPECT_TRUE(mock_embedder->collect_view_called);
  EXPECT_EQ(mock_embedder->collect_view_id, 99);

  adapter->PushVisitedPlatformView(7);
  EXPECT_TRUE(mock_embedder->push_visited_called);

  adapter->PushFilterToVisitedPlatformViews(nullptr, DlRect());
  EXPECT_TRUE(mock_embedder->push_filter_called);

  adapter->PushClipRectToVisitedPlatformViews(DlRect());
  EXPECT_TRUE(mock_embedder->push_clip_rect_called);

  adapter->PushClipRRectToVisitedPlatformViews(DlRoundRect());
  EXPECT_TRUE(mock_embedder->push_clip_rrect_called);

  adapter->PushClipRSuperellipseToVisitedPlatformViews(DlRoundSuperellipse());
  EXPECT_TRUE(mock_embedder->push_clip_rse_called);

  adapter->PushClipPathToVisitedPlatformViews(DlPath());
  EXPECT_TRUE(mock_embedder->push_clip_path_called);
}

TEST(AndroidCompositorAdapterTest, CallbackTrampolinesSafeNullHandling) {
  auto adapter = std::make_shared<AndroidCompositorAdapter>(nullptr);
  const FlutterCompositor* compositor = adapter->GetFlutterCompositor();

  // Passing null user_data or info should safely handle without crashing.
  EXPECT_FALSE(
      compositor->create_backing_store_callback(nullptr, nullptr, nullptr));
  EXPECT_FALSE(compositor->collect_backing_store_callback(nullptr, nullptr));
  EXPECT_FALSE(compositor->present_view_callback(nullptr));
  EXPECT_EQ(compositor->post_preroll_callback(nullptr),
            kFlutterPostPrerollResultSuccess);
  compositor->begin_frame_callback(nullptr);
  compositor->end_frame_callback(nullptr);

  // Calling with valid user_data
  EXPECT_FALSE(compositor->create_backing_store_callback(
      nullptr, nullptr, compositor->user_data));
  EXPECT_TRUE(compositor->collect_backing_store_callback(
      nullptr, compositor->user_data));

  FlutterPresentViewInfo present_info = {};
  present_info.struct_size = sizeof(FlutterPresentViewInfo);
  present_info.user_data = compositor->user_data;
  EXPECT_TRUE(compositor->present_view_callback(&present_info));

  FlutterFrameThreadingInfo threading_info = {};
  threading_info.struct_size = sizeof(FlutterFrameThreadingInfo);
  threading_info.user_data = compositor->user_data;
  EXPECT_EQ(compositor->post_preroll_callback(&threading_info),
            kFlutterPostPrerollResultSuccess);
  compositor->begin_frame_callback(&threading_info);
  compositor->end_frame_callback(&threading_info);
}

}  // namespace testing
}  // namespace flutter
