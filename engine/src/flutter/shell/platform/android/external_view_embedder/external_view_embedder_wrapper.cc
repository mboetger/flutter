// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/external_view_embedder/external_view_embedder_wrapper.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "flutter/impeller/renderer/backend/vulkan/context_vk.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/external_view_embedder/external_view_embedder.h"
#include "flutter/shell/platform/android/external_view_embedder/external_view_embedder_2.h"

#if FML_OS_ANDROID
#include <android/native_window.h>
#endif  // FML_OS_ANDROID

namespace flutter {

namespace {

#define SAFE_COMPOSITOR_MEMBER(c, member, default_val)                     \
  (((c) != nullptr &&                                                      \
    (c)->struct_size >= offsetof(FlutterNativeWindowCompositor, member) +  \
                            sizeof(FlutterNativeWindowCompositor::member)) \
       ? (c)->member                                                       \
       : (default_val))

class PathReceiver final : public DlPathReceiver {
 public:
  PathReceiver(std::vector<FlutterPathVerb>& verbs,
               std::vector<FlutterPoint>& points,
               std::vector<double>& conic_weights)
      : verbs_(verbs), points_(points), conic_weights_(conic_weights) {}
  void MoveTo(const DlPoint& p2, bool) override {
    verbs_.push_back(kFlutterPathVerbMove);
    points_.push_back({p2.x, p2.y});
  }
  void LineTo(const DlPoint& p2) override {
    verbs_.push_back(kFlutterPathVerbLine);
    points_.push_back({p2.x, p2.y});
  }
  void QuadTo(const DlPoint& cp, const DlPoint& p2) override {
    verbs_.push_back(kFlutterPathVerbQuad);
    points_.insert(points_.end(), {{cp.x, cp.y}, {p2.x, p2.y}});
  }
  bool ConicTo(const DlPoint& cp, const DlPoint& p2, DlScalar weight) override {
    verbs_.push_back(kFlutterPathVerbConic);
    points_.insert(points_.end(), {{cp.x, cp.y}, {p2.x, p2.y}});
    conic_weights_.push_back(weight);
    return true;
  }
  void CubicTo(const DlPoint& c1,
               const DlPoint& c2,
               const DlPoint& p2) override {
    verbs_.push_back(kFlutterPathVerbCubic);
    points_.insert(points_.end(), {{c1.x, c1.y}, {c2.x, c2.y}, {p2.x, p2.y}});
  }
  void Close() override { verbs_.push_back(kFlutterPathVerbClose); }

 private:
  std::vector<FlutterPathVerb>& verbs_;
  std::vector<FlutterPoint>& points_;
  std::vector<double>& conic_weights_;
};

struct ConvertedPath {
  std::vector<FlutterPathVerb> verbs;
  std::vector<FlutterPoint> points;
  std::vector<double> conic_weights;
  FlutterPath path = {};
};

struct ConvertedMutations {
  std::vector<std::unique_ptr<ConvertedPath>> paths;
  std::vector<FlutterPlatformViewMutation> storage;
  std::vector<const FlutterPlatformViewMutation*> ptrs;
};

ConvertedMutations ConvertMutatorsStack(const MutatorsStack& mutators_stack) {
  ConvertedMutations out;
  for (auto it = mutators_stack.Begin(); it != mutators_stack.End(); ++it) {
    switch ((*it)->GetType()) {
      case MutatorType::kTransform: {
        const DlMatrix& m = (*it)->GetMatrix();
        FlutterPlatformViewMutation mut = {};
        mut.type = kFlutterPlatformViewMutationTypeTransformation;
        mut.transformation = {m.m[0],  m.m[4], m.m[12], m.m[1], m.m[5],
                              m.m[13], m.m[3], m.m[7],  m.m[15]};
        out.storage.push_back(mut);
        break;
      }
      case MutatorType::kClipRect: {
        const DlRect& r = (*it)->GetRect();
        FlutterPlatformViewMutation mut = {};
        mut.type = kFlutterPlatformViewMutationTypeClipRect;
        mut.clip_rect = {r.GetLeft(), r.GetTop(), r.GetRight(), r.GetBottom()};
        out.storage.push_back(mut);
        break;
      }
      case MutatorType::kClipRRect:
      case MutatorType::kClipRSE: {
        const DlRoundRect rr = (*it)->GetType() == MutatorType::kClipRRect
                                   ? (*it)->GetRRect()
                                   : (*it)->GetRSEApproximation();
        const DlRect& r = rr.GetBounds();
        const DlRoundingRadii& rad = rr.GetRadii();
        FlutterPlatformViewMutation mut = {};
        mut.type = kFlutterPlatformViewMutationTypeClipRoundedRect;
        mut.clip_rounded_rect = {
            {r.GetLeft(), r.GetTop(), r.GetRight(), r.GetBottom()},
            {rad.top_left.width, rad.top_left.height},
            {rad.top_right.width, rad.top_right.height},
            {rad.bottom_right.width, rad.bottom_right.height},
            {rad.bottom_left.width, rad.bottom_left.height}};
        out.storage.push_back(mut);
        break;
      }
      case MutatorType::kOpacity: {
        FlutterPlatformViewMutation mut = {};
        mut.type = kFlutterPlatformViewMutationTypeOpacity;
        mut.opacity = (*it)->GetAlphaFloat();
        out.storage.push_back(mut);
        break;
      }
      case MutatorType::kClipPath: {
        const DlPath& dl_path = (*it)->GetPath();
        auto holder = std::make_unique<ConvertedPath>();
        PathReceiver recv(holder->verbs, holder->points, holder->conic_weights);
        dl_path.Dispatch(recv);
        holder->path = {
            sizeof(FlutterPath),
            dl_path.GetFillType() == DlPathFillType::kOdd
                ? kFlutterPathFillTypeEvenOdd
                : kFlutterPathFillTypeNonZero,
            holder->verbs.size(),
            holder->verbs.empty() ? nullptr : holder->verbs.data(),
            holder->points.size(),
            holder->points.empty() ? nullptr : holder->points.data(),
            holder->conic_weights.size(),
            holder->conic_weights.empty() ? nullptr
                                          : holder->conic_weights.data()};
        FlutterPlatformViewMutation mut = {};
        mut.type = kFlutterPlatformViewMutationTypeClipPath;
        mut.clip_path = &holder->path;
        out.paths.push_back(std::move(holder));
        out.storage.push_back(mut);
        break;
      }
      case MutatorType::kBackdropFilter:
      case MutatorType::kBackdropClipRect:
      case MutatorType::kBackdropClipRRect:
      case MutatorType::kBackdropClipRSuperellipse:
      case MutatorType::kBackdropClipPath:
        break;
    }
  }
  out.ptrs.reserve(out.storage.size());
  for (const FlutterPlatformViewMutation& m : out.storage) {
    out.ptrs.push_back(&m);
  }
  return out;
}

class NativeWindowCompositorDelegate final
    : public AndroidExternalViewEmbedderDelegate {
 public:
  NativeWindowCompositorDelegate(const FlutterNativeWindowCompositor* c,
                                 void* fallback_user_data) {
    compositor_.struct_size = sizeof(FlutterNativeWindowCompositor);
    void* ud = SAFE_COMPOSITOR_MEMBER(c, user_data, nullptr);
    compositor_.user_data = ud != nullptr ? ud : fallback_user_data;
#define COPY_CB(field) \
  compositor_.field = SAFE_COMPOSITOR_MEMBER(c, field, nullptr)
    COPY_CB(begin_frame_callback);
    COPY_CB(end_frame_callback);
    COPY_CB(create_overlay_surface_callback);
    COPY_CB(destroy_overlay_surfaces_callback);
    COPY_CB(display_overlay_surface_callback);
    COPY_CB(display_platform_view_callback);
    COPY_CB(swap_transaction_callback);
    COPY_CB(create_overlay_surface2_callback);
    COPY_CB(destroy_overlay_surface2_callback);
    COPY_CB(end_frame2_callback);
    COPY_CB(show_overlay_surface2_callback);
    COPY_CB(hide_overlay_surface2_callback);
    COPY_CB(display_platform_view2_callback);
    COPY_CB(hide_platform_view2_callback);
    COPY_CB(maybe_resize_surface_view_callback);
#undef COPY_CB
  }

  void FlutterViewBeginFrame() override {
    CallVoid(compositor_.begin_frame_callback);
  }
  void FlutterViewEndFrame() override {
    CallVoid(compositor_.end_frame_callback);
  }
  std::unique_ptr<OverlayMetadata> FlutterViewCreateOverlaySurface() override {
    return InvokeCreateOverlay(compositor_.create_overlay_surface_callback);
  }
  void FlutterViewDestroyOverlaySurfaces() override {
    CallVoid(compositor_.destroy_overlay_surfaces_callback);
  }
  void FlutterViewDisplayOverlaySurface(int id,
                                        int x,
                                        int y,
                                        int w,
                                        int h) override {
    if (compositor_.display_overlay_surface_callback) {
      compositor_.display_overlay_surface_callback(compositor_.user_data, id, x,
                                                   y, w, h);
    }
  }
  void FlutterViewOnDisplayPlatformView(int id,
                                        int x,
                                        int y,
                                        int w,
                                        int h,
                                        int vw,
                                        int vh,
                                        MutatorsStack s) override {
    InvokeDisplayView(compositor_.display_platform_view_callback, id, x, y, w,
                      h, vw, vh, s);
  }
  void swapTransaction() override {
    CallVoid(compositor_.swap_transaction_callback);
  }
  std::unique_ptr<OverlayMetadata> createOverlaySurface2() override {
    return InvokeCreateOverlay(compositor_.create_overlay_surface2_callback);
  }
  void destroyOverlaySurface2() override {
    CallVoid(compositor_.destroy_overlay_surface2_callback);
  }
  void onEndFrame2() override { CallVoid(compositor_.end_frame2_callback); }
  void showOverlaySurface2() override {
    CallVoid(compositor_.show_overlay_surface2_callback);
  }
  void hideOverlaySurface2() override {
    CallVoid(compositor_.hide_overlay_surface2_callback);
  }
  void onDisplayPlatformView2(int32_t id,
                              int32_t x,
                              int32_t y,
                              int32_t w,
                              int32_t h,
                              int32_t vw,
                              int32_t vh,
                              MutatorsStack s) override {
    InvokeDisplayView(compositor_.display_platform_view2_callback, id, x, y, w,
                      h, vw, vh, s);
  }
  void hidePlatformView2(int32_t view_id) override {
    if (compositor_.hide_platform_view2_callback) {
      compositor_.hide_platform_view2_callback(compositor_.user_data, view_id);
    }
  }
  void MaybeResizeSurfaceView(int32_t width, int32_t height) const override {
    if (compositor_.maybe_resize_surface_view_callback) {
      compositor_.maybe_resize_surface_view_callback(compositor_.user_data,
                                                     width, height);
    }
  }

 private:
  void CallVoid(VoidCallback cb) const {
    if (cb) {
      cb(compositor_.user_data);
    }
  }
  using DisplayCallback =
      decltype(FlutterNativeWindowCompositor::display_platform_view_callback);

  void InvokeDisplayView(DisplayCallback cb,
                         int32_t id,
                         int32_t x,
                         int32_t y,
                         int32_t w,
                         int32_t h,
                         int32_t vw,
                         int32_t vh,
                         const MutatorsStack& s) const {
    if (!cb) {
      return;
    }
    ConvertedMutations converted = ConvertMutatorsStack(s);
    cb(compositor_.user_data, id, x, y, w, h, vw, vh,
       converted.ptrs.empty() ? nullptr : converted.ptrs.data(),
       converted.ptrs.size());
  }

  std::unique_ptr<OverlayMetadata> InvokeCreateOverlay(
      bool (*cb)(void*, FlutterNativeWindowOverlay*)) const {
    if (!cb) {
      return nullptr;
    }
    FlutterNativeWindowOverlay overlay = {sizeof(FlutterNativeWindowOverlay), 0,
                                          nullptr};
    if (!cb(compositor_.user_data, &overlay)) {
      return nullptr;
    }
#if FML_OS_ANDROID
    auto* raw = static_cast<AndroidNativeWindow::Handle>(overlay.window);
    if (raw != nullptr) {
      ANativeWindow_acquire(raw);
    }
#else
    AndroidNativeWindow::Handle raw = nullptr;
#endif  // FML_OS_ANDROID
    return std::make_unique<OverlayMetadata>(
        overlay.id, fml::MakeRefCounted<AndroidNativeWindow>(raw));
  }

  FlutterNativeWindowCompositor compositor_ = {};
};

#undef SAFE_COMPOSITOR_MEMBER

}  // namespace

std::shared_ptr<AndroidExternalViewEmbedderDelegate>
MakeNativeWindowCompositorDelegate(
    const FlutterNativeWindowCompositor* compositor,
    void* fallback_user_data) {
  if (compositor == nullptr) {
    return nullptr;
  }
  return std::make_shared<NativeWindowCompositorDelegate>(compositor,
                                                          fallback_user_data);
}

AndroidExternalViewEmbedderWrapper::AndroidExternalViewEmbedderWrapper(
    bool meets_hcpp_criteria,
    const AndroidContext& android_context,
    std::shared_ptr<AndroidExternalViewEmbedderDelegate> jni_facade,
    std::shared_ptr<AndroidSurfaceFactory> surface_factory,
    const TaskRunners& task_runners)
    : ExternalViewEmbedder(),
      meets_hcpp_criteria_(meets_hcpp_criteria),
      android_context_(android_context),
      task_runners_(task_runners),
      jni_facade_(std::move(jni_facade)),
      surface_factory_(std::move(surface_factory)) {}

void AndroidExternalViewEmbedderWrapper::EnsureInitialized() {
  if (non_hcpp_view_embedder_ || hcpp_view_embedder_) {
    return;
  }
  if (meets_hcpp_criteria_ &&
      android_context_.RenderingApi() == AndroidRenderingAPI::kImpellerVulkan &&
      impeller::ContextVK::Cast(*android_context_.GetImpellerContext())
          .GetShouldEnableSurfaceControlSwapchain()) {
    hcpp_view_embedder_ = std::make_unique<AndroidExternalViewEmbedder2>(
        android_context_, jni_facade_, surface_factory_, task_runners_);
  } else {
    non_hcpp_view_embedder_ = std::make_unique<AndroidExternalViewEmbedder>(
        android_context_, jni_facade_, surface_factory_, task_runners_);
  }
}

// |ExternalViewEmbedder|
void AndroidExternalViewEmbedderWrapper::PrerollCompositeEmbeddedView(
    int64_t view_id,
    std::unique_ptr<EmbeddedViewParams> params) {
  EnsureInitialized();
  if (hcpp_view_embedder_) {
    hcpp_view_embedder_->PrerollCompositeEmbeddedView(view_id,
                                                      std::move(params));
  } else {
    non_hcpp_view_embedder_->PrerollCompositeEmbeddedView(view_id,
                                                          std::move(params));
  }
}

// |ExternalViewEmbedder|
DlCanvas* AndroidExternalViewEmbedderWrapper::CompositeEmbeddedView(
    int64_t view_id) {
  EnsureInitialized();
  if (hcpp_view_embedder_) {
    return hcpp_view_embedder_->CompositeEmbeddedView(view_id);
  } else {
    return non_hcpp_view_embedder_->CompositeEmbeddedView(view_id);
  }
}

// |ExternalViewEmbedder|
void AndroidExternalViewEmbedderWrapper::SubmitFlutterView(
    int64_t flutter_view_id,
    GrDirectContext* context,
    const std::shared_ptr<impeller::AiksContext>& aiks_context,
    std::unique_ptr<SurfaceFrame> frame) {
  EnsureInitialized();
  if (hcpp_view_embedder_) {
    hcpp_view_embedder_->SubmitFlutterView(flutter_view_id, context,
                                           aiks_context, std::move(frame));
  } else {
    non_hcpp_view_embedder_->SubmitFlutterView(flutter_view_id, context,
                                               aiks_context, std::move(frame));
  }
}

// |ExternalViewEmbedder|
PostPrerollResult AndroidExternalViewEmbedderWrapper::PostPrerollAction(
    const fml::RefPtr<fml::RasterThreadMerger>& raster_thread_merger) {
  EnsureInitialized();
  if (hcpp_view_embedder_) {
    return hcpp_view_embedder_->PostPrerollAction(raster_thread_merger);
  } else {
    return non_hcpp_view_embedder_->PostPrerollAction(raster_thread_merger);
  }
}

DlCanvas* AndroidExternalViewEmbedderWrapper::GetRootCanvas() {
  // On Android, the root surface is created from the on-screen render target.
  return nullptr;
}

// |ExternalViewEmbedder|
void AndroidExternalViewEmbedderWrapper::BeginFrame(
    GrDirectContext* context,
    const fml::RefPtr<fml::RasterThreadMerger>& raster_thread_merger) {
  EnsureInitialized();
  if (hcpp_view_embedder_) {
    hcpp_view_embedder_->BeginFrame(context, raster_thread_merger);
  } else {
    non_hcpp_view_embedder_->BeginFrame(context, raster_thread_merger);
  }
}

// |ExternalViewEmbedder|
void AndroidExternalViewEmbedderWrapper::PrepareFlutterView(
    DlISize frame_size,
    double device_pixel_ratio) {
  EnsureInitialized();
  if (hcpp_view_embedder_) {
    hcpp_view_embedder_->PrepareFlutterView(frame_size, device_pixel_ratio);
  } else {
    non_hcpp_view_embedder_->PrepareFlutterView(frame_size, device_pixel_ratio);
  }
}

// |ExternalViewEmbedder|
void AndroidExternalViewEmbedderWrapper::CancelFrame() {
  EnsureInitialized();
  if (hcpp_view_embedder_) {
    hcpp_view_embedder_->CancelFrame();
  } else {
    non_hcpp_view_embedder_->CancelFrame();
  }
}

// |ExternalViewEmbedder|
void AndroidExternalViewEmbedderWrapper::EndFrame(
    bool should_resubmit_frame,
    const fml::RefPtr<fml::RasterThreadMerger>& raster_thread_merger) {
  EnsureInitialized();
  if (hcpp_view_embedder_) {
    hcpp_view_embedder_->EndFrame(should_resubmit_frame, raster_thread_merger);
  } else {
    non_hcpp_view_embedder_->EndFrame(should_resubmit_frame,
                                      raster_thread_merger);
  }
}

// |ExternalViewEmbedder|
bool AndroidExternalViewEmbedderWrapper::SupportsDynamicThreadMerging() {
  EnsureInitialized();
  if (hcpp_view_embedder_) {
    return hcpp_view_embedder_->SupportsDynamicThreadMerging();
  } else {
    return non_hcpp_view_embedder_->SupportsDynamicThreadMerging();
  }
}

// |ExternalViewEmbedder|
void AndroidExternalViewEmbedderWrapper::Teardown() {
  EnsureInitialized();
  if (hcpp_view_embedder_) {
    hcpp_view_embedder_->Teardown();
  } else {
    non_hcpp_view_embedder_->Teardown();
  }
}

}  // namespace flutter
