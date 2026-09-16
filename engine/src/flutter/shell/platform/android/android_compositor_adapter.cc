// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_compositor_adapter.h"

#include <utility>

namespace flutter {

AndroidCompositorAdapter::AndroidCompositorAdapter(
    std::shared_ptr<ExternalViewEmbedder> view_embedder)
    : view_embedder_(std::move(view_embedder)) {
  InitializeCompositor();
}

void AndroidCompositorAdapter::InitializeCompositor() {
  compositor_.struct_size = sizeof(FlutterCompositor);
  compositor_.user_data = this;
  compositor_.create_backing_store_callback = &OnCreateBackingStore;
  compositor_.collect_backing_store_callback = &OnCollectBackingStore;
  compositor_.present_layers_callback = nullptr;
  compositor_.avoid_backing_store_cache = false;
  compositor_.present_view_callback = &OnPresentView;
  compositor_.supports_dynamic_thread_merging =
      view_embedder_ ? view_embedder_->SupportsDynamicThreadMerging() : false;
  compositor_.post_preroll_callback = &OnPostPreroll;
  compositor_.begin_frame_callback = &OnBeginFrame;
  compositor_.end_frame_callback = &OnEndFrame;
}

const FlutterCompositor* AndroidCompositorAdapter::GetFlutterCompositor() {
  if (view_embedder_) {
    compositor_.supports_dynamic_thread_merging =
        view_embedder_->SupportsDynamicThreadMerging();
  }
  return &compositor_;
}

// |ExternalViewEmbedder|
void AndroidCompositorAdapter::PrerollCompositeEmbeddedView(
    int64_t view_id,
    std::unique_ptr<EmbeddedViewParams> params) {
  if (view_embedder_) {
    view_embedder_->PrerollCompositeEmbeddedView(view_id, std::move(params));
  }
}

// |ExternalViewEmbedder|
DlCanvas* AndroidCompositorAdapter::CompositeEmbeddedView(int64_t view_id) {
  if (view_embedder_) {
    return view_embedder_->CompositeEmbeddedView(view_id);
  }
  return nullptr;
}

// |ExternalViewEmbedder|
void AndroidCompositorAdapter::SubmitFlutterView(
    int64_t flutter_view_id,
    GrDirectContext* context,
    const std::shared_ptr<impeller::AiksContext>& aiks_context,
    std::unique_ptr<SurfaceFrame> frame) {
  if (view_embedder_) {
    view_embedder_->SubmitFlutterView(flutter_view_id, context, aiks_context,
                                      std::move(frame));
  }
}

// |ExternalViewEmbedder|
PostPrerollResult AndroidCompositorAdapter::PostPrerollAction(
    const fml::RefPtr<fml::RasterThreadMerger>& raster_thread_merger) {
  if (view_embedder_) {
    return view_embedder_->PostPrerollAction(raster_thread_merger);
  }
  return PostPrerollResult::kSuccess;
}

// |ExternalViewEmbedder|
DlCanvas* AndroidCompositorAdapter::GetRootCanvas() {
  if (view_embedder_) {
    return view_embedder_->GetRootCanvas();
  }
  return nullptr;
}

// |ExternalViewEmbedder|
void AndroidCompositorAdapter::BeginFrame(
    GrDirectContext* context,
    const fml::RefPtr<fml::RasterThreadMerger>& raster_thread_merger) {
  SetUsedThisFrame(true);
  if (view_embedder_) {
    view_embedder_->BeginFrame(context, raster_thread_merger);
  }
}

// |ExternalViewEmbedder|
void AndroidCompositorAdapter::PrepareFlutterView(DlISize frame_size,
                                                  double device_pixel_ratio) {
  if (view_embedder_) {
    view_embedder_->PrepareFlutterView(frame_size, device_pixel_ratio);
  }
}

// |ExternalViewEmbedder|
void AndroidCompositorAdapter::CancelFrame() {
  if (view_embedder_) {
    view_embedder_->CancelFrame();
  }
}

// |ExternalViewEmbedder|
void AndroidCompositorAdapter::EndFrame(
    bool should_resubmit_frame,
    const fml::RefPtr<fml::RasterThreadMerger>& raster_thread_merger) {
  SetUsedThisFrame(false);
  if (view_embedder_) {
    view_embedder_->EndFrame(should_resubmit_frame, raster_thread_merger);
  }
}

// |ExternalViewEmbedder|
bool AndroidCompositorAdapter::SupportsDynamicThreadMerging() {
  if (view_embedder_) {
    return view_embedder_->SupportsDynamicThreadMerging();
  }
  return false;
}

// |ExternalViewEmbedder|
void AndroidCompositorAdapter::Teardown() {
  if (view_embedder_) {
    view_embedder_->Teardown();
  }
}

// |ExternalViewEmbedder|
void AndroidCompositorAdapter::CollectView(int64_t view_id) {
  if (view_embedder_) {
    view_embedder_->CollectView(view_id);
  }
}

// |ExternalViewEmbedder|
void AndroidCompositorAdapter::PushVisitedPlatformView(
    int64_t platform_view_id) {
  if (view_embedder_) {
    view_embedder_->PushVisitedPlatformView(platform_view_id);
  }
}

// |ExternalViewEmbedder|
void AndroidCompositorAdapter::PushFilterToVisitedPlatformViews(
    const std::shared_ptr<DlImageFilter>& filter,
    const DlRect& filter_rect) {
  if (view_embedder_) {
    view_embedder_->PushFilterToVisitedPlatformViews(filter, filter_rect);
  }
}

// |ExternalViewEmbedder|
void AndroidCompositorAdapter::PushClipRectToVisitedPlatformViews(
    const DlRect& clip_rect) {
  if (view_embedder_) {
    view_embedder_->PushClipRectToVisitedPlatformViews(clip_rect);
  }
}

// |ExternalViewEmbedder|
void AndroidCompositorAdapter::PushClipRRectToVisitedPlatformViews(
    const DlRoundRect& clip_rrect) {
  if (view_embedder_) {
    view_embedder_->PushClipRRectToVisitedPlatformViews(clip_rrect);
  }
}

// |ExternalViewEmbedder|
void AndroidCompositorAdapter::PushClipRSuperellipseToVisitedPlatformViews(
    const DlRoundSuperellipse& clip_rse) {
  if (view_embedder_) {
    view_embedder_->PushClipRSuperellipseToVisitedPlatformViews(clip_rse);
  }
}

// |ExternalViewEmbedder|
void AndroidCompositorAdapter::PushClipPathToVisitedPlatformViews(
    const DlPath& clip_path) {
  if (view_embedder_) {
    view_embedder_->PushClipPathToVisitedPlatformViews(clip_path);
  }
}

// Static C-ABI callback trampolines for FlutterCompositor.
bool AndroidCompositorAdapter::OnCreateBackingStore(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out,
    void* user_data) {
  auto* adapter = static_cast<AndroidCompositorAdapter*>(user_data);
  if (!adapter) {
    return false;
  }
  return adapter->HandleCreateBackingStore(config, backing_store_out);
}

bool AndroidCompositorAdapter::OnCollectBackingStore(
    const FlutterBackingStore* backing_store,
    void* user_data) {
  auto* adapter = static_cast<AndroidCompositorAdapter*>(user_data);
  if (!adapter) {
    return false;
  }
  return adapter->HandleCollectBackingStore(backing_store);
}

bool AndroidCompositorAdapter::OnPresentView(
    const FlutterPresentViewInfo* info) {
  if (!info) {
    return false;
  }
  auto* adapter = static_cast<AndroidCompositorAdapter*>(info->user_data);
  if (!adapter) {
    return false;
  }
  return adapter->HandlePresentView(info);
}

FlutterPostPrerollResult AndroidCompositorAdapter::OnPostPreroll(
    const FlutterFrameThreadingInfo* info) {
  if (!info) {
    return kFlutterPostPrerollResultSuccess;
  }
  auto* adapter = static_cast<AndroidCompositorAdapter*>(info->user_data);
  if (!adapter) {
    return kFlutterPostPrerollResultSuccess;
  }
  return adapter->HandlePostPreroll(info);
}

void AndroidCompositorAdapter::OnBeginFrame(
    const FlutterFrameThreadingInfo* info) {
  if (!info) {
    return;
  }
  auto* adapter = static_cast<AndroidCompositorAdapter*>(info->user_data);
  if (!adapter) {
    return;
  }
  adapter->HandleBeginFrame(info);
}

void AndroidCompositorAdapter::OnEndFrame(
    const FlutterFrameThreadingInfo* info) {
  if (!info) {
    return;
  }
  auto* adapter = static_cast<AndroidCompositorAdapter*>(info->user_data);
  if (!adapter) {
    return;
  }
  adapter->HandleEndFrame(info);
}

bool AndroidCompositorAdapter::HandleCreateBackingStore(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out) {
  return false;
}

bool AndroidCompositorAdapter::HandleCollectBackingStore(
    const FlutterBackingStore* backing_store) {
  return true;
}

bool AndroidCompositorAdapter::HandlePresentView(
    const FlutterPresentViewInfo* info) {
  return true;
}

FlutterPostPrerollResult AndroidCompositorAdapter::HandlePostPreroll(
    const FlutterFrameThreadingInfo* info) {
  return kFlutterPostPrerollResultSuccess;
}

void AndroidCompositorAdapter::HandleBeginFrame(
    const FlutterFrameThreadingInfo* info) {}

void AndroidCompositorAdapter::HandleEndFrame(
    const FlutterFrameThreadingInfo* info) {}

}  // namespace flutter
