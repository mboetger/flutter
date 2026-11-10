// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/external_view_embedder/external_view_embedder.h"

#include <cmath>

#include "flow/view_slicer.h"
#include "flutter/common/constants.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/fml/trace_event.h"
#include "fml/make_copyable.h"

namespace flutter {

namespace {

// Converts a size in logical pixels to the whole number of physical pixels the
// platform view should be laid out at.
//
// Android view geometry is integral, so this conversion is lossy no matter
// what, but it must *round* rather than truncate. `size_points * dpr` is
// computed in double precision from a float `size_points`, so a platform view
// that exactly covers the screen lands a hair under the physical size (e.g.
// 1079.99996 for a 1080px wide screen at a device pixel ratio of 2.625).
// Truncating that drops a pixel off the right and bottom edges, and the Flutter
// content behind the platform view shows through as a thin line.
//
// See https://github.com/flutter/flutter/issues/189834.
int32_t ToPhysicalPixels(DlScalar size_points, double device_pixel_ratio) {
  return static_cast<int32_t>(std::round(size_points * device_pixel_ratio));
}

}  // namespace

AndroidExternalViewEmbedder::AndroidExternalViewEmbedder(
    const AndroidContext& android_context,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    std::shared_ptr<AndroidSurfaceFactory> surface_factory,
    const TaskRunners& task_runners,
    bool merge_threads,
    fml::closure schedule_frame)
    : ExternalViewEmbedder(),
      android_context_(android_context),
      jni_facade_(std::move(jni_facade)),
      surface_factory_(std::move(surface_factory)),
      surface_pool_(
          std::make_unique<SurfacePool>(/*use_new_surface_methods=*/false)),
      task_runners_(task_runners),
      merge_threads_(merge_threads),
      schedule_frame_(std::move(schedule_frame)),
      present_state_(std::make_shared<PresentState>()),
      surface_generation_(std::make_shared<std::atomic<uint64_t>>(0)) {}

// |ExternalViewEmbedder|
void AndroidExternalViewEmbedder::PrerollCompositeEmbeddedView(
    int64_t view_id,
    std::unique_ptr<EmbeddedViewParams> params) {
  TRACE_EVENT0("flutter",
               "AndroidExternalViewEmbedder::PrerollCompositeEmbeddedView");

  DlRect view_bounds = DlRect::MakeSize(frame_size_);
  std::unique_ptr<EmbedderViewSlice> view;
  view = std::make_unique<DisplayListEmbedderViewSlice>(view_bounds);
  slices_.insert_or_assign(view_id, std::move(view));

  composition_order_.push_back(view_id);
  // Update params only if they changed.
  if (view_params_.count(view_id) == 1 &&
      view_params_.at(view_id) == *params.get()) {
    return;
  }
  view_params_.insert_or_assign(view_id, EmbeddedViewParams(*params.get()));
}

// |ExternalViewEmbedder|
DlCanvas* AndroidExternalViewEmbedder::CompositeEmbeddedView(int64_t view_id) {
  if (slices_.count(view_id) == 1) {
    return slices_.at(view_id)->canvas();
  }
  return nullptr;
}

DlRect AndroidExternalViewEmbedder::GetViewRect(int64_t view_id) const {
  const EmbeddedViewParams& params = view_params_.at(view_id);
  // TODO(egarciad): The rect should be computed from the mutator stack.
  // (Clipping is missing)
  // https://github.com/flutter/flutter/issues/59821
  return params.finalBoundingRect();
}

// |ExternalViewEmbedder|
void AndroidExternalViewEmbedder::SubmitFlutterView(
    int64_t flutter_view_id,
    GrDirectContext* context,
    const std::shared_ptr<impeller::AiksContext>& aiks_context,
    std::unique_ptr<SurfaceFrame> frame) {
  TRACE_EVENT0("flutter", "AndroidExternalViewEmbedder::SubmitFlutterView");
  // TODO(dkwingsmt): This class only supports rendering into the implicit view.
  // Properly support multi-view in the future.
  FML_DCHECK(flutter_view_id == kFlutterImplicitViewId);

  if (!merge_threads_) {
    SubmitFlutterViewWithoutMerging(context, std::move(frame));
    return;
  }

  if (!FrameHasPlatformLayers()) {
    frame->Submit();
    return;
  }

  std::unordered_map<int64_t, DlRect> view_rects;
  for (auto platform_id : composition_order_) {
    view_rects[platform_id] = GetViewRect(platform_id);
  }

  std::unordered_map<int64_t, DlRect> overlay_layers =
      SliceViews(frame->Canvas(),     //
                 composition_order_,  //
                 slices_,             //
                 view_rects           //
      );

  // Submit the background canvas frame before switching the GL context to
  // the overlay surfaces.
  //
  // Skip a frame if the embedding is switching surfaces, and indicate in
  // `PostPrerollAction` that this frame must be resubmitted.
  auto should_submit_current_frame = previous_frame_view_count_ > 0;
  if (should_submit_current_frame) {
    frame->Submit();
  }

  for (int64_t view_id : composition_order_) {
    // Round, rather than truncate, the bounds onto the pixel grid. See
    // `ToPhysicalPixels` for why.
    const DlIRect view_rect = DlIRect::Round(GetViewRect(view_id));
    const EmbeddedViewParams& params = view_params_.at(view_id);
    // Display the platform view. If it's already displayed, then it's
    // just positioned and sized.
    jni_facade_->FlutterViewOnDisplayPlatformView(
        view_id,                //
        view_rect.GetX(),       //
        view_rect.GetY(),       //
        view_rect.GetWidth(),   //
        view_rect.GetHeight(),  //
        ToPhysicalPixels(params.sizePoints().width, device_pixel_ratio_),
        ToPhysicalPixels(params.sizePoints().height, device_pixel_ratio_),
        params.mutatorsStack()  //
    );
    std::unordered_map<int64_t, DlRect>::const_iterator overlay =
        overlay_layers.find(view_id);
    if (overlay == overlay_layers.end()) {
      continue;
    }
    std::unique_ptr<SurfaceFrame> frame =
        CreateSurfaceIfNeeded(context,                    //
                              view_id,                    //
                              slices_.at(view_id).get(),  //
                              overlay->second             //
        );
    if (should_submit_current_frame) {
      frame->Submit();
    }
  }
}

// |ExternalViewEmbedder|
std::unique_ptr<SurfaceFrame>
AndroidExternalViewEmbedder::CreateSurfaceIfNeeded(GrDirectContext* context,
                                                   int64_t view_id,
                                                   EmbedderViewSlice* slice,
                                                   const DlRect& rect) {
  std::shared_ptr<OverlayLayer> layer = surface_pool_->GetLayer(
      context, android_context_, jni_facade_, surface_factory_);

  std::unique_ptr<SurfaceFrame> frame =
      layer->surface->AcquireFrame(frame_size_);
  // Display the overlay surface. If it's already displayed, then it's
  // just positioned and sized.
  jni_facade_->FlutterViewDisplayOverlaySurface(layer->id,        //
                                                rect.GetX(),      //
                                                rect.GetY(),      //
                                                rect.GetWidth(),  //
                                                rect.GetHeight()  //
  );
  DlCanvas* overlay_canvas = frame->Canvas();
  overlay_canvas->Clear(DlColor::kTransparent());
  // Offset the picture since its absolute position on the scene is determined
  // by the position of the overlay view.
  overlay_canvas->Translate(-rect.GetX(), -rect.GetY());
  slice->render_into(overlay_canvas);
  return frame;
}

void AndroidExternalViewEmbedder::SubmitFlutterViewWithoutMerging(
    GrDirectContext* context,
    std::unique_ptr<SurfaceFrame> frame) {
  AddCreatedOverlayLayers(context);

  if (!FrameHasPlatformLayers()) {
    frame->Submit();
    if (presented_platform_views_) {
      // Lets the platform thread hide the platform views and switch the root
      // surface back from the image reader.
      presented_platform_views_ = false;
      PostPlatformWork({}, /*present=*/true, /*overlays_to_create=*/0,
                       /*schedule_frame=*/false);
    }
    return;
  }

  std::unordered_map<int64_t, DlRect> view_rects;
  for (auto platform_id : composition_order_) {
    view_rects[platform_id] = GetViewRect(platform_id);
  }

  std::unordered_map<int64_t, DlRect> overlay_layers =
      SliceViews(frame->Canvas(),     //
                 composition_order_,  //
                 slices_,             //
                 view_rects           //
      );

  const size_t available_layers = surface_pool_->GetAvailableLayerCount();
  const size_t missing_layers = overlay_layers.size() > available_layers
                                    ? overlay_layers.size() - available_layers
                                    : 0;

  // Overlay views can only be created on the platform thread. Rather than
  // wait for it, skip this frame, create the overlays, and request a new frame.
  if (presented_platform_views_ && missing_layers > 0) {
    TRACE_EVENT_INSTANT0("flutter",
                         "AndroidExternalViewEmbedder::SkipFrameForOverlays");
    PostPlatformWork({}, /*present=*/false, missing_layers,
                     /*schedule_frame=*/true);
    return;
  }

  // Presenting the first frame with platform views switches the root surface
  // to an image reader, but this frame would still render to the previous
  // surface. Skip rendering it, as merged mode does before it resubmits the
  // frame, and create the overlays it needs while presenting it. Switching
  // surfaces schedules another frame.
  const bool should_submit_current_frame = presented_platform_views_;
  if (should_submit_current_frame) {
    frame->Submit();
  }

  std::vector<PresentOp> ops;
  for (int64_t view_id : composition_order_) {
    // Round, rather than truncate, the bounds onto the pixel grid. See
    // `ToPhysicalPixels` for why.
    const DlIRect view_rect = DlIRect::Round(GetViewRect(view_id));
    const EmbeddedViewParams& params = view_params_.at(view_id);
    const int32_t view_width =
        ToPhysicalPixels(params.sizePoints().width, device_pixel_ratio_);
    const int32_t view_height =
        ToPhysicalPixels(params.sizePoints().height, device_pixel_ratio_);
    ops.push_back([view_id, view_rect, view_width, view_height,
                   mutators_stack = params.mutatorsStack()](
                      PlatformViewAndroidJNI& jni_facade) {
      jni_facade.FlutterViewOnDisplayPlatformView(
          view_id, view_rect.GetX(), view_rect.GetY(), view_rect.GetWidth(),
          view_rect.GetHeight(), view_width, view_height, mutators_stack);
    });

    std::unordered_map<int64_t, DlRect>::const_iterator overlay =
        overlay_layers.find(view_id);
    if (!should_submit_current_frame || overlay == overlay_layers.end()) {
      continue;
    }
    const DlRect rect = overlay->second;
    std::shared_ptr<OverlayLayer> layer = surface_pool_->GetLayer(
        context, android_context_, jni_facade_, surface_factory_);
    std::unique_ptr<SurfaceFrame> overlay_frame =
        layer->surface->AcquireFrame(frame_size_);
    DlCanvas* overlay_canvas = overlay_frame->Canvas();
    overlay_canvas->Clear(DlColor::kTransparent());
    // Offset the picture since its absolute position on the scene is
    // determined by the position of the overlay view.
    overlay_canvas->Translate(-rect.GetX(), -rect.GetY());
    slices_.at(view_id)->render_into(overlay_canvas);
    overlay_frame->Submit();
    ops.push_back([id = layer->id, rect](PlatformViewAndroidJNI& jni_facade) {
      jni_facade.FlutterViewDisplayOverlaySurface(
          id, rect.GetX(), rect.GetY(), rect.GetWidth(), rect.GetHeight());
    });
  }
  presented_platform_views_ = true;
  PostPlatformWork(std::move(ops), /*present=*/true,
                   should_submit_current_frame ? 0 : missing_layers,
                   /*schedule_frame=*/false);
}

void AndroidExternalViewEmbedder::AddCreatedOverlayLayers(
    GrDirectContext* context) {
  if (present_state_->created_overlays.empty()) {
    return;
  }
  auto created_overlays = std::move(present_state_->created_overlays);
  present_state_->created_overlays.clear();
  const uint64_t generation =
      surface_generation_->load(std::memory_order_relaxed);
  for (auto& [created_generation, overlay] : created_overlays) {
    // |Teardown| destroyed the overlay views that were created before it.
    if (created_generation == generation) {
      surface_pool_->AddLayer(std::move(overlay), context, jni_facade_,
                              surface_factory_);
    }
  }
}

void AndroidExternalViewEmbedder::PostPlatformWork(std::vector<PresentOp> ops,
                                                   bool present,
                                                   size_t overlays_to_create,
                                                   bool schedule_frame) {
  present_state_->pending = true;
  const uint64_t flow_id = fml::tracing::TraceNonce();
  TRACE_FLOW_BEGIN("flutter", "HybridCompositionPresent", flow_id);
  task_runners_.GetPlatformTaskRunner()->PostTask(fml::MakeCopyable(
      [jni_facade = jni_facade_, ops = std::move(ops), present,
       overlays_to_create, schedule_frame,
       schedule_frame_callback = schedule_frame_,
       generation = surface_generation_->load(std::memory_order_relaxed),
       surface_generation = surface_generation_,
       raster_task_runner = task_runners_.GetRasterTaskRunner(),
       weak_present_state = std::weak_ptr<PresentState>(present_state_),
       flow_id]() mutable {
        TRACE_EVENT0("flutter", "AndroidExternalViewEmbedder::Present");
        TRACE_FLOW_STEP("flutter", "HybridCompositionPresent", flow_id);
        const bool torn_down =
            surface_generation->load(std::memory_order_relaxed) != generation;
        std::vector<std::unique_ptr<PlatformViewAndroidJNI::OverlayMetadata>>
            overlays;
        if (!torn_down) {
          for (size_t i = 0; i < overlays_to_create; i++) {
            overlays.push_back(jni_facade->FlutterViewCreateOverlaySurface());
          }
          if (present) {
            jni_facade->FlutterViewBeginFrame();
            for (const PresentOp& op : ops) {
              op(*jni_facade);
            }
            jni_facade->FlutterViewEndFrame();
          }
        }
        // Resume drawing from a raster task posted after any surface swap that
        // the present requested, so that the next frame renders to the new
        // surface.
        raster_task_runner->PostTask(fml::MakeCopyable(
            [weak_present_state, generation, overlays = std::move(overlays),
             flow_id]() mutable {
              TRACE_EVENT0("flutter",
                           "AndroidExternalViewEmbedder::PresentDone");
              TRACE_FLOW_END("flutter", "HybridCompositionPresent", flow_id);
              std::shared_ptr<PresentState> present_state =
                  weak_present_state.lock();
              if (!present_state) {
                return;
              }
              for (auto& overlay : overlays) {
                present_state->created_overlays.emplace_back(
                    generation, std::move(overlay));
              }
              present_state->pending = false;
              std::vector<fml::closure> resume_callbacks;
              resume_callbacks.swap(present_state->resume_callbacks);
              for (const fml::closure& resume : resume_callbacks) {
                resume();
              }
            }));
        if (schedule_frame && !torn_down && schedule_frame_callback) {
          schedule_frame_callback();
        }
      }));
}

// |ExternalViewEmbedder|
PostPrerollResult AndroidExternalViewEmbedder::PostPrerollAction(
    const fml::RefPtr<fml::RasterThreadMerger>& raster_thread_merger) {
  if (!merge_threads_ || !FrameHasPlatformLayers()) {
    return PostPrerollResult::kSuccess;
  }
  if (!raster_thread_merger->IsMerged()) {
    // The raster thread merger may be disabled if the rasterizer is being
    // created or teared down.
    //
    // In such cases, the current frame is dropped, and a new frame is attempted
    // with the same layer tree.
    //
    // Eventually, the frame is submitted once this method returns `kSuccess`.
    // At that point, the raster tasks are handled on the platform thread.
    CancelFrame();
    raster_thread_merger->MergeWithLease(kDefaultMergedLeaseDuration);
    return PostPrerollResult::kSkipAndRetryFrame;
  }
  raster_thread_merger->ExtendLeaseTo(kDefaultMergedLeaseDuration);
  // Surface switch requires to resubmit the frame.
  // TODO(egarciad): https://github.com/flutter/flutter/issues/65652
  if (previous_frame_view_count_ == 0) {
    return PostPrerollResult::kResubmitFrame;
  }
  return PostPrerollResult::kSuccess;
}

bool AndroidExternalViewEmbedder::FrameHasPlatformLayers() {
  return !composition_order_.empty();
}

// |ExternalViewEmbedder|
DlCanvas* AndroidExternalViewEmbedder::GetRootCanvas() {
  // On Android, the root surface is created from the on-screen render target.
  return nullptr;
}

void AndroidExternalViewEmbedder::Reset() {
  previous_frame_view_count_ = composition_order_.size();

  composition_order_.clear();
  slices_.clear();
}

// |ExternalViewEmbedder|
void AndroidExternalViewEmbedder::BeginFrame(
    GrDirectContext* context,
    const fml::RefPtr<fml::RasterThreadMerger>& raster_thread_merger) {
  // JNI method must be called on the platform thread. Without thread merging,
  // the present posted to the platform thread calls it.
  if (merge_threads_ && raster_thread_merger->IsOnPlatformThread()) {
    jni_facade_->FlutterViewBeginFrame();
  }
}

// |ExternalViewEmbedder|
void AndroidExternalViewEmbedder::PrepareFlutterView(
    DlISize frame_size,
    double device_pixel_ratio) {
  Reset();

  // Overlay layers are allocated for the current frame size, so the layers in
  // the pool can't be recycled after the size changes. Release them without
  // waiting for the platform thread: this runs on the raster thread, which is
  // only merged into the platform thread while platform views are in the
  // scene, and the platform thread may be waiting for the raster thread, for
  // example in `PlatformViewAndroid::NotifyChanged` during `surfaceChanged`.
  if (frame_size_ != frame_size) {
    ReleaseOverlayLayers();

    // This should not block to prevent deadlocks with
    // setViewportMetrics.
    task_runners_.GetPlatformTaskRunner()->PostTask(fml::MakeCopyable(
        [jni_facade = jni_facade_, frame_size = frame_size]() {
          jni_facade->MaybeResizeSurfaceView(frame_size.width,
                                             frame_size.height);
        }));
  }
  surface_pool_->SetFrameSize(frame_size);

  frame_size_ = frame_size;
  device_pixel_ratio_ = device_pixel_ratio;
}

// |ExternalViewEmbedder|
void AndroidExternalViewEmbedder::CancelFrame() {
  Reset();
}

// |ExternalViewEmbedder|
void AndroidExternalViewEmbedder::EndFrame(
    bool should_resubmit_frame,
    const fml::RefPtr<fml::RasterThreadMerger>& raster_thread_merger) {
  surface_pool_->RecycleLayers();
  // JNI method must be called on the platform thread. Without thread merging,
  // the present posted to the platform thread calls it.
  if (merge_threads_ && raster_thread_merger->IsOnPlatformThread()) {
    jni_facade_->FlutterViewEndFrame();
  }
}

// |ExternalViewEmbedder|
bool AndroidExternalViewEmbedder::SupportsDynamicThreadMerging() {
  return merge_threads_;
}

// |ExternalViewEmbedder|
bool AndroidExternalViewEmbedder::ReadyForNextFrame(
    const fml::closure& resume) {
  if (merge_threads_ || !present_state_->pending) {
    return true;
  }
  // The platform thread is still presenting the previous frame, and the image
  // readers hold that frame until it does. Defer instead of blocking.
  TRACE_EVENT_INSTANT0("flutter", "AndroidExternalViewEmbedder::DeferFrame");
  if (resume) {
    present_state_->resume_callbacks.push_back(resume);
  }
  return false;
}

// |ExternalViewEmbedder|
void AndroidExternalViewEmbedder::Teardown() {
  // Presents that are still queued on the platform thread are for the surface
  // that is being torn down.
  surface_generation_->fetch_add(1, std::memory_order_relaxed);
  if (!merge_threads_ && !surface_pool_->HasLayers()) {
    // The platform thread may have created overlay views that the pool doesn't
    // hold yet. The shell calls this method on the platform thread.
    jni_facade_->FlutterViewDestroyOverlaySurfaces();
    return;
  }
  DestroySurfaces();
}

// |ExternalViewEmbedder|
void AndroidExternalViewEmbedder::DestroySurfaces() {
  if (!surface_pool_->HasLayers()) {
    return;
  }
  fml::AutoResetWaitableEvent latch;
  fml::TaskRunner::RunNowOrPostTask(task_runners_.GetPlatformTaskRunner(),
                                    [&]() {
                                      surface_pool_->DestroyLayers(jni_facade_);
                                      latch.Signal();
                                    });
  latch.Wait();
}

void AndroidExternalViewEmbedder::ReleaseOverlayLayers() {
  // Overlay surfaces that the platform thread created but that the pool
  // doesn't hold yet have the previous frame size too.
  const bool had_created_overlays = !present_state_->created_overlays.empty();
  present_state_->created_overlays.clear();
  std::vector<std::shared_ptr<OverlayLayer>> layers =
      surface_pool_->TakeLayers();
  if (layers.empty() && !had_created_overlays) {
    return;
  }
  // Release the rendering surfaces on the thread that renders into them.
  layers.clear();

  // Destroy the overlay views on the platform thread without waiting. Tasks run
  // in the order they were posted, so this runs before a later frame can
  // allocate new overlay surfaces, which only happens on the platform thread.
  fml::TaskRunner::RunNowOrPostTask(
      task_runners_.GetPlatformTaskRunner(), [jni_facade = jni_facade_]() {
        jni_facade->FlutterViewDestroyOverlaySurfaces();
      });
}

}  // namespace flutter
