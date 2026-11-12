// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_EXTERNAL_VIEW_EMBEDDER_EXTERNAL_VIEW_EMBEDDER_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_EXTERNAL_VIEW_EMBEDDER_EXTERNAL_VIEW_EMBEDDER_H_

#include <atomic>
#include <functional>
#include <memory>
#include <unordered_map>
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
  // Frames are rasterized on the raster thread and presented on the platform
  // thread without merging the two threads. `schedule_frame` is called on the
  // platform thread to request a new frame after surface conversion or async
  // overlay surface creation.
  AndroidExternalViewEmbedder(
      const AndroidContext& android_context,
      std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
      std::shared_ptr<AndroidSurfaceFactory> surface_factory,
      const TaskRunners& task_runners,
      fml::closure schedule_frame = nullptr);

  ~AndroidExternalViewEmbedder() override;

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

  // Requests a new frame. Called on the platform thread.
  const fml::closure schedule_frame_;

  // One call into Java that presents a frame on the platform thread. The calls
  // run in z-order.
  using PresentOp = std::function<void(PlatformViewAndroidJNI&)>;

  struct CreatedOverlay {
    uint64_t surface_generation = 0;
    uint64_t size_generation = 0;
    std::unique_ptr<PlatformViewAndroidJNI::OverlayMetadata> metadata;
  };

  // Tracks the work that the platform thread does for the raster thread. Only
  // accessed on the raster thread.
  struct PresentState {
    // Whether the platform thread is presenting a frame or creating overlay
    // surfaces. The next frame waits for it without blocking.
    bool pending = false;
    std::vector<fml::closure> resume_callbacks;
    // Incremented whenever overlay layers are released (on resize or teardown)
    // so overlays created for an earlier size generation are discarded even if
    // the frame size later reverts (S1 -> S2 -> S1).
    uint64_t size_generation = 0;
    // Number of posted platform tasks with `overlays_to_create > 0` whose
    // `PresentDone` callback has not yet executed on the raster thread.
    size_t in_flight_overlay_creations = 0;
    // Overlay surfaces that the platform thread created, tagged with the
    // surface and size generations they were requested under.
    std::vector<CreatedOverlay> created_overlays;
  };

  // Shared with the raster tasks that finish a present.
  const std::shared_ptr<PresentState> present_state_;

  // Incremented by |Teardown| and the destructor so that presents posted for a
  // torn-down surface don't call into Java.
  const std::shared_ptr<std::atomic<uint64_t>> surface_generation_;

  // Whether the last present showed platform views, which means that Java
  // renders the root surface to an image reader. Only accessed on the raster
  // thread.
  bool presented_platform_views_ = false;

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

  // Adds the overlay surfaces that the platform thread created to the pool,
  // unless the surface was torn down or resized since.
  void AddCreatedOverlayLayers(GrDirectContext* context);

  // Posts work to the platform thread: creating `overlays_to_create` overlay
  // surfaces, then the Java calls that present the current frame if `present`
  // is true, then requesting a new frame if `schedule_frame` is true. The next
  // frame waits for it without blocking the raster thread, see
  // |ReadyForNextFrame|.
  void PostPlatformWork(std::vector<PresentOp> ops,
                        bool present,
                        size_t overlays_to_create,
                        bool schedule_frame);

  // Takes the overlay layers out of the pool and releases them without waiting
  // for the platform thread. The rendering surfaces are released on the
  // calling thread, and the overlay views are destroyed on the platform
  // thread.
  void ReleaseOverlayLayers();

  // Resets the state.
  void Reset();

  // Whether the layer tree in the current frame has platform layers.
  bool FrameHasPlatformLayers();
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_EXTERNAL_VIEW_EMBEDDER_EXTERNAL_VIEW_EMBEDDER_H_
