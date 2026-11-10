// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_EXTERNAL_VIEW_EMBEDDER_EXTERNAL_VIEW_EMBEDDER_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_EXTERNAL_VIEW_EMBEDDER_EXTERNAL_VIEW_EMBEDDER_H_

#include <atomic>
#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "flutter/common/task_runners.h"
#include "flutter/flow/embedded_views.h"
#include "flutter/fml/closure.h"
#include "flutter/shell/platform/android/context/android_context.h"
#include "flutter/shell/platform/android/external_view_embedder/surface_pool.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/surface/android_surface.h"

namespace flutter {

//------------------------------------------------------------------------------
/// Allows to embed Android views into a Flutter application.
///
/// This class calls Java methods via |PlatformViewAndroidJNI| to manage the
/// lifecycle of the Android view corresponding to |flutter::PlatformViewLayer|.
///
/// It also orchestrates overlay surfaces. These are Android views
/// that render above (by Z order) the Android view corresponding to
/// |flutter::PlatformViewLayer|.
///
class AndroidExternalViewEmbedder final : public ExternalViewEmbedder {
 public:
  // If `merge_threads` is false, frames are rasterized on the raster thread and
  // presented on the platform thread without merging the two threads.
  // `schedule_frame` is then called on the platform thread to request a new
  // frame after a frame was skipped.
  AndroidExternalViewEmbedder(
      const AndroidContext& android_context,
      std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
      std::shared_ptr<AndroidSurfaceFactory> surface_factory,
      const TaskRunners& task_runners,
      bool merge_threads = true,
      fml::closure schedule_frame = nullptr);

  // |ExternalViewEmbedder|
  void PrerollCompositeEmbeddedView(
      int64_t view_id,
      std::unique_ptr<flutter::EmbeddedViewParams> params) override;

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
  bool ReadyForNextFrame(const fml::closure& resume) override;

  // |ExternalViewEmbedder|
  void Teardown() override;

  // Gets the rect based on the device pixel ratio of a platform view displayed
  // on the screen.
  DlRect GetViewRect(int64_t view_id) const;

 private:
  // The number of frames the rasterizer task runner will continue
  // to run on the platform thread after no platform view is rendered.
  //
  // Note: this is an arbitrary number that attempts to account for cases
  // where the platform view might be momentarily off the screen.
  static const int kDefaultMergedLeaseDuration = 10;

  // Provides metadata to the Android surfaces.
  const AndroidContext& android_context_;

  // Allows to call methods in Java.
  const std::shared_ptr<PlatformViewAndroidJNI> jni_facade_;

  // Allows to create surfaces.
  const std::shared_ptr<AndroidSurfaceFactory> surface_factory_;

  // Holds surfaces. Allows to recycle surfaces or allocate new ones.
  const std::unique_ptr<SurfacePool> surface_pool_;

  // The task runners.
  const TaskRunners task_runners_;

  // The size of the root canvas.
  DlISize frame_size_;

  // The pixel ratio used to determinate the size of a platform view layer
  // relative to the device layout system.
  double device_pixel_ratio_ = 0.0;

  // The order of composition. Each entry contains a unique id for the platform
  // view.
  std::vector<int64_t> composition_order_;

  // The |EmbedderViewSlice| implementation keyed off the platform view id,
  // which contains any subsequent operations until the next platform view or
  // the end of the last leaf node in the layer tree.
  std::unordered_map<int64_t, std::unique_ptr<EmbedderViewSlice>> slices_;

  // The params for a platform view, which contains the size, position and
  // mutation stack.
  std::unordered_map<int64_t, EmbeddedViewParams> view_params_;

  // The number of platform views in the previous frame.
  int64_t previous_frame_view_count_ = 0;

  // Without thread merging, one call into Java that presents a frame on the
  // platform thread. The calls run in z-order.
  using PresentOp = std::function<void(PlatformViewAndroidJNI&)>;

  // Without thread merging, tracks the work that the platform thread does for
  // the raster thread. Only accessed on the raster thread.
  struct PresentState {
    // Whether the platform thread is presenting a frame or creating overlay
    // surfaces. The next frame waits for it without blocking.
    bool pending = false;
    std::vector<fml::closure> resume_callbacks;
    // Overlay surfaces that the platform thread created, with the surface
    // generation they were created in. The next frame adds them to the pool.
    std::vector<
        std::pair<uint64_t,
                  std::unique_ptr<PlatformViewAndroidJNI::OverlayMetadata>>>
        created_overlays;
  };

  // Whether the raster thread is merged into the platform thread while
  // platform views are in the scene.
  const bool merge_threads_;

  // Requests a new frame. Called on the platform thread.
  const fml::closure schedule_frame_;

  // Shared with the raster tasks that finish a present.
  const std::shared_ptr<PresentState> present_state_;

  // Incremented on the platform thread by |Teardown|, so that presents posted
  // for a torn down surface don't call into Java.
  const std::shared_ptr<std::atomic<uint64_t>> surface_generation_;

  // Without thread merging, whether the last present showed platform views,
  // which means that Java renders the root surface to an image reader. Only
  // accessed on the raster thread.
  bool presented_platform_views_ = false;

  // Without thread merging, renders the frame on the raster thread and posts
  // its presentation to the platform thread.
  void SubmitFlutterViewWithoutMerging(GrDirectContext* context,
                                       std::unique_ptr<SurfaceFrame> frame);

  // Without thread merging, adds the overlay surfaces that the platform thread
  // created to the pool, unless the surface was torn down since.
  void AddCreatedOverlayLayers(GrDirectContext* context);

  // Without thread merging, posts work to the platform thread: creating
  // `overlays_to_create` overlay surfaces, then the Java calls that present the
  // current frame if `present` is true, then requesting a new frame if
  // `schedule_frame` is true. The next frame waits for it without blocking the
  // raster thread, see |ReadyForNextFrame|.
  void PostPlatformWork(std::vector<PresentOp> ops,
                        bool present,
                        size_t overlays_to_create,
                        bool schedule_frame);

  // Destroys the surfaces created from the surface factory.
  // This method schedules a task on the platform thread, and waits for
  // the task until it completes. Only call it where the platform thread can't
  // be waiting for the caller, such as from |Teardown|, which the shell calls
  // on the platform thread. Never call it from the raster thread.
  void DestroySurfaces();

  // Takes the overlay layers out of the pool and releases them without waiting
  // for the platform thread. The rendering surfaces are released on the
  // calling thread, and the overlay views are destroyed on the platform
  // thread.
  void ReleaseOverlayLayers();

  // Resets the state.
  void Reset();

  // Whether the layer tree in the current frame has platform layers.
  bool FrameHasPlatformLayers();

  // Creates a Surface when needed or recycles an existing one.
  // Finally, draws the picture on the frame's canvas.
  std::unique_ptr<SurfaceFrame> CreateSurfaceIfNeeded(GrDirectContext* context,
                                                      int64_t view_id,
                                                      EmbedderViewSlice* slice,
                                                      const DlRect& rect);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_EXTERNAL_VIEW_EMBEDDER_EXTERNAL_VIEW_EMBEDDER_H_
