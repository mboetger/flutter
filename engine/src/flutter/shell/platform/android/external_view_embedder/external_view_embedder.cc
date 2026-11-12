// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/external_view_embedder/external_view_embedder.h"

#include <cmath>
#include <utility>

#include "flow/view_slicer.h"
#include "flutter/common/constants.h"
#include "flutter/display_list/dl_builder.h"
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
    fml::closure schedule_frame)
    : ExternalViewEmbedder(),
      android_context_(android_context),
      jni_facade_(std::move(jni_facade)),
      surface_factory_(std::move(surface_factory)),
      surface_pool_(
          std::make_unique<SurfacePool>(/*use_new_surface_methods=*/false)),
      task_runners_(task_runners),
      schedule_frame_(std::move(schedule_frame)),
      present_state_(std::make_shared<PresentState>()),
      surface_generation_(std::make_shared<std::atomic<uint64_t>>(0)) {}

AndroidExternalViewEmbedder::~AndroidExternalViewEmbedder() {
  surface_generation_->fetch_add(1, std::memory_order_acq_rel);
}

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
  for (int64_t platform_id : composition_order_) {
    view_rects[platform_id] = GetViewRect(platform_id);
  }

  // Presenting the first frame with platform views switches the root surface
  // to an image reader while keeping the platform views hidden until the next
  // frame renders into that image reader. Slice against a scratch builder and
  // composite the unclipped foreground slices onto `frame->Canvas()` so the
  // current FlutterSurfaceView/FlutterTextureView frame has no holes punched
  // out of it.
  DisplayListBuilder first_frame_slice_builder;
  DlCanvas* slice_target_canvas =
      presented_platform_views_ ? frame->Canvas() : &first_frame_slice_builder;
  std::unordered_map<int64_t, DlRect> overlay_layers =
      SliceViews(slice_target_canvas,  //
                 composition_order_,   //
                 slices_,              //
                 view_rects            //
      );
  if (!presented_platform_views_) {
    if (DlCanvas* root_canvas = frame->Canvas()) {
      for (int64_t view_id : composition_order_) {
        slices_.at(view_id)->render_into(root_canvas);
      }
    }
  }

  const size_t available_layers = surface_pool_->GetAvailableLayerCount();
  const size_t missing_layers = overlay_layers.size() > available_layers
                                    ? overlay_layers.size() - available_layers
                                    : 0;

  // Submit the acquired root frame before returning or switching the GL context
  // to overlay surfaces. Impeller's Vulkan swapchain requires every acquired
  // SurfaceFrame to be submitted before the next AcquireFrame call.
  frame->Submit();

  // Overlay views can only be created on the platform thread. Rather than
  // wait for it, skip presenting this frame, create the overlays (draining the
  // unpresented root buffer on the platform thread), and request a new frame.
  if (presented_platform_views_ && missing_layers > 0) {
    TRACE_EVENT0("flutter",
                 "AndroidExternalViewEmbedder::SkipFrameForOverlays");
    PostPlatformWork({}, /*present=*/false, missing_layers,
                     /*schedule_frame=*/true);
    return;
  }

  const bool should_submit_overlays = presented_platform_views_;
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
    if (!should_submit_overlays || overlay == overlay_layers.end()) {
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
                   should_submit_overlays ? 0 : missing_layers,
                   /*schedule_frame=*/!should_submit_overlays);
}

void AndroidExternalViewEmbedder::AddCreatedOverlayLayers(
    GrDirectContext* context) {
  if (present_state_->created_overlays.empty()) {
    return;
  }
  auto created_overlays = std::move(present_state_->created_overlays);
  present_state_->created_overlays.clear();
  const uint64_t surface_gen =
      surface_generation_->load(std::memory_order_acquire);
  const uint64_t size_gen = present_state_->size_generation;
  for (CreatedOverlay& created : created_overlays) {
    if (created.surface_generation == surface_gen &&
        created.size_generation == size_gen) {
      surface_pool_->AddLayer(std::move(created.metadata), context, jni_facade_,
                              surface_factory_);
    }
  }
}

void AndroidExternalViewEmbedder::PostPlatformWork(std::vector<PresentOp> ops,
                                                   bool present,
                                                   size_t overlays_to_create,
                                                   bool schedule_frame) {
  present_state_->pending = true;
  const uint64_t surface_gen =
      surface_generation_->load(std::memory_order_acquire);
  const uint64_t size_gen = present_state_->size_generation;
  const bool had_overlay_creations = overlays_to_create > 0;
  if (had_overlay_creations) {
    ++present_state_->in_flight_overlay_creations;
  }
  const uint64_t flow_id = fml::tracing::TraceNonce();
  TRACE_FLOW_BEGIN("flutter", "HybridCompositionPresent", flow_id);
  task_runners_.GetPlatformTaskRunner()->PostTask(fml::MakeCopyable(
      [jni_facade = jni_facade_, ops = std::move(ops), present,
       overlays_to_create, had_overlay_creations, schedule_frame,
       schedule_frame_callback = schedule_frame_, surface_gen, size_gen,
       surface_generation = surface_generation_,
       raster_task_runner = task_runners_.GetRasterTaskRunner(),
       weak_present_state = std::weak_ptr<PresentState>(present_state_),
       flow_id]() mutable {
        TRACE_EVENT0("flutter", "AndroidExternalViewEmbedder::Present");
        TRACE_FLOW_STEP("flutter", "HybridCompositionPresent", flow_id);
        const bool torn_down =
            surface_generation->load(std::memory_order_acquire) != surface_gen;
        std::vector<std::unique_ptr<PlatformViewAndroidJNI::OverlayMetadata>>
            overlays;
        if (!torn_down) {
          for (size_t i = 0; i < overlays_to_create; i++) {
            auto metadata = jni_facade->FlutterViewCreateOverlaySurface();
            if (metadata) {
              overlays.push_back(std::move(metadata));
            }
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
            [weak_present_state, surface_generation, surface_gen, size_gen,
             had_overlay_creations, overlays = std::move(overlays),
             flow_id]() mutable {
              TRACE_EVENT0("flutter",
                           "AndroidExternalViewEmbedder::PresentDone");
              TRACE_FLOW_END("flutter", "HybridCompositionPresent", flow_id);
              std::shared_ptr<PresentState> present_state =
                  weak_present_state.lock();
              if (!present_state) {
                return;
              }
              if (had_overlay_creations) {
                FML_DCHECK(present_state->in_flight_overlay_creations > 0);
                --present_state->in_flight_overlay_creations;
              }
              if (surface_generation->load(std::memory_order_acquire) ==
                      surface_gen &&
                  present_state->size_generation == size_gen) {
                for (auto& overlay : overlays) {
                  present_state->created_overlays.push_back(CreatedOverlay{
                      .surface_generation = surface_gen,
                      .size_generation = size_gen,
                      .metadata = std::move(overlay),
                  });
                }
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
  composition_order_.clear();
  slices_.clear();
}

// |ExternalViewEmbedder|
void AndroidExternalViewEmbedder::BeginFrame(
    GrDirectContext* context,
    const fml::RefPtr<fml::RasterThreadMerger>& raster_thread_merger) {
  // JNI presentation methods run on the platform thread in the task posted by
  // |PostPlatformWork|.
}

// |ExternalViewEmbedder|
void AndroidExternalViewEmbedder::PrepareFlutterView(
    DlISize frame_size,
    double device_pixel_ratio) {
  Reset();

  // Overlay layers are allocated for the current frame size, so the layers in
  // the pool can't be recycled after the size changes. Release them without
  // waiting for the platform thread, which may be waiting for the raster
  // thread, for example in `PlatformViewAndroid::NotifyChanged` during
  // `surfaceChanged`.
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
}

// |ExternalViewEmbedder|
bool AndroidExternalViewEmbedder::SupportsDynamicThreadMerging() {
  return false;
}

// |ExternalViewEmbedder|
bool AndroidExternalViewEmbedder::ReadyForNextFrame(
    const fml::closure& resume) {
  if (!present_state_->pending) {
    return true;
  }
  // The platform thread is still presenting the previous frame, and the image
  // readers hold that frame until it does. Defer instead of blocking.
  TRACE_EVENT0("flutter", "AndroidExternalViewEmbedder::DeferFrame");
  if (resume) {
    present_state_->resume_callbacks.push_back(resume);
  }
  return false;
}

// |ExternalViewEmbedder|
void AndroidExternalViewEmbedder::Teardown() {
  // Presents that are still queued on the platform thread are for the surface
  // that is being torn down.
  surface_generation_->fetch_add(1, std::memory_order_acq_rel);

  bool had_overlays = false;
  fml::AutoResetWaitableEvent raster_latch;
  fml::TaskRunner::RunNowOrPostTask(task_runners_.GetRasterTaskRunner(), [&]() {
    presented_platform_views_ = false;
    ++present_state_->size_generation;
    const bool had_created_overlays = !present_state_->created_overlays.empty();
    present_state_->created_overlays.clear();
    std::vector<std::shared_ptr<OverlayLayer>> layers =
        surface_pool_->TakeLayers();
    had_overlays = !layers.empty() || had_created_overlays ||
                   present_state_->in_flight_overlay_creations > 0;
    layers.clear();
    raster_latch.Signal();
  });
  raster_latch.Wait();

  if (!had_overlays) {
    return;
  }
  fml::AutoResetWaitableEvent platform_latch;
  fml::TaskRunner::RunNowOrPostTask(
      task_runners_.GetPlatformTaskRunner(), [&]() {
        jni_facade_->FlutterViewDestroyOverlaySurfaces();
        platform_latch.Signal();
      });
  platform_latch.Wait();
}

void AndroidExternalViewEmbedder::ReleaseOverlayLayers() {
  // Overlay surfaces that the platform thread created (or is still creating)
  // for an earlier size generation have the previous frame size too.
  ++present_state_->size_generation;
  const bool had_created_overlays = !present_state_->created_overlays.empty();
  present_state_->created_overlays.clear();
  std::vector<std::shared_ptr<OverlayLayer>> layers =
      surface_pool_->TakeLayers();
  if (layers.empty() && !had_created_overlays &&
      present_state_->in_flight_overlay_creations == 0) {
    return;
  }
  // Release the rendering surfaces on the thread that renders into them.
  layers.clear();

  // Destroy the overlay views on the platform thread without waiting. Tasks run
  // in the order they were posted, so this runs after any in-flight overlay
  // creation and before a later frame can allocate new overlay surfaces.
  fml::TaskRunner::RunNowOrPostTask(
      task_runners_.GetPlatformTaskRunner(), [jni_facade = jni_facade_]() {
        jni_facade->FlutterViewDestroyOverlaySurfaces();
      });
}

}  // namespace flutter
