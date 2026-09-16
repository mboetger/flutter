// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_COMPOSITOR_ADAPTER_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_COMPOSITOR_ADAPTER_H_

#include <memory>

#include "flutter/shell/platform/android/external_view_embedder/external_view_embedder_wrapper.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

//------------------------------------------------------------------------------
/// An adapter that wraps an ExternalViewEmbedder (such as
/// AndroidExternalViewEmbedderWrapper) and presents a FlutterCompositor C-ABI
/// struct matching the Embedder API shape.
///
/// In Stage 2, it implements ExternalViewEmbedder by transparently forwarding
/// all calls to the wrapped embedder, while exposing a populated
/// FlutterCompositor struct for Stage 3 preparation.
class AndroidCompositorAdapter : public ExternalViewEmbedder {
 public:
  explicit AndroidCompositorAdapter(
      std::shared_ptr<ExternalViewEmbedder> view_embedder);
  ~AndroidCompositorAdapter() override = default;

  AndroidCompositorAdapter(const AndroidCompositorAdapter&) = delete;
  AndroidCompositorAdapter& operator=(const AndroidCompositorAdapter&) = delete;

  // Returns the underlying wrapped ExternalViewEmbedder.
  std::shared_ptr<ExternalViewEmbedder> GetViewEmbedder() const {
    return view_embedder_;
  }

  // Returns a pointer to the populated FlutterCompositor struct.
  const FlutterCompositor* GetFlutterCompositor();

  // |ExternalViewEmbedder|
  void PrerollCompositeEmbeddedView(
      int64_t view_id,
      std::unique_ptr<EmbeddedViewParams> params) override;

  // |ExternalViewEmbedder|
  DlCanvas* CompositeEmbeddedView(int64_t view_id) override;

  // |ExternalViewEmbedder|
  void SubmitFlutterView(
      int64_t flutter_view_id,
      GrDirectContext* context,
      const std::shared_ptr<impeller::AiksContext>& aiks_context,
      std::unique_ptr<SurfaceFrame> frame) override;

  // |ExternalViewEmbedder|
  PostPrerollResult PostPrerollAction(
      const fml::RefPtr<fml::RasterThreadMerger>& raster_thread_merger)
      override;

  // |ExternalViewEmbedder|
  DlCanvas* GetRootCanvas() override;

  // |ExternalViewEmbedder|
  void BeginFrame(GrDirectContext* context,
                  const fml::RefPtr<fml::RasterThreadMerger>&
                      raster_thread_merger) override;

  // |ExternalViewEmbedder|
  void PrepareFlutterView(DlISize frame_size,
                          double device_pixel_ratio) override;

  // |ExternalViewEmbedder|
  void CancelFrame() override;

  // |ExternalViewEmbedder|
  void EndFrame(bool should_resubmit_frame,
                const fml::RefPtr<fml::RasterThreadMerger>&
                    raster_thread_merger) override;

  // |ExternalViewEmbedder|
  bool SupportsDynamicThreadMerging() override;

  // |ExternalViewEmbedder|
  void Teardown() override;

  // |ExternalViewEmbedder|
  void CollectView(int64_t view_id) override;

  // |ExternalViewEmbedder|
  void PushVisitedPlatformView(int64_t platform_view_id) override;

  // |ExternalViewEmbedder|
  void PushFilterToVisitedPlatformViews(
      const std::shared_ptr<DlImageFilter>& filter,
      const DlRect& filter_rect) override;

  // |ExternalViewEmbedder|
  void PushClipRectToVisitedPlatformViews(const DlRect& clip_rect) override;

  // |ExternalViewEmbedder|
  void PushClipRRectToVisitedPlatformViews(
      const DlRoundRect& clip_rrect) override;

  // |ExternalViewEmbedder|
  void PushClipRSuperellipseToVisitedPlatformViews(
      const DlRoundSuperellipse& clip_rse) override;

  // |ExternalViewEmbedder|
  void PushClipPathToVisitedPlatformViews(const DlPath& clip_path) override;

 private:
  void InitializeCompositor();

  // Static C-ABI callback trampolines for FlutterCompositor.
  static bool OnCreateBackingStore(const FlutterBackingStoreConfig* config,
                                   FlutterBackingStore* backing_store_out,
                                   void* user_data);
  static bool OnCollectBackingStore(const FlutterBackingStore* backing_store,
                                    void* user_data);
  static bool OnPresentView(const FlutterPresentViewInfo* info);
  static FlutterPostPrerollResult OnPostPreroll(
      const FlutterFrameThreadingInfo* info);
  static void OnBeginFrame(const FlutterFrameThreadingInfo* info);
  static void OnEndFrame(const FlutterFrameThreadingInfo* info);

  // Instance callback handlers.
  bool HandleCreateBackingStore(const FlutterBackingStoreConfig* config,
                                FlutterBackingStore* backing_store_out);
  bool HandleCollectBackingStore(const FlutterBackingStore* backing_store);
  bool HandlePresentView(const FlutterPresentViewInfo* info);
  FlutterPostPrerollResult HandlePostPreroll(
      const FlutterFrameThreadingInfo* info);
  void HandleBeginFrame(const FlutterFrameThreadingInfo* info);
  void HandleEndFrame(const FlutterFrameThreadingInfo* info);

  std::shared_ptr<ExternalViewEmbedder> view_embedder_;
  FlutterCompositor compositor_ = {};
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_COMPOSITOR_ADAPTER_H_
