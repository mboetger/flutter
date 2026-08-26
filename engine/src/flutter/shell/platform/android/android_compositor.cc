// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_compositor.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "flutter/display_list/geometry/dl_geometry_types.h"

namespace flutter {

static bool CreateBackingStoreCallback(const FlutterBackingStoreConfig* config,
                                       FlutterBackingStore* backing_store_out,
                                       void* user_data) {
  if (user_data == nullptr) {
    return false;
  }
  return reinterpret_cast<AndroidCompositor*>(user_data)->CreateBackingStore(
      config, backing_store_out);
}

static bool CollectBackingStoreCallback(const FlutterBackingStore* renderer,
                                        void* user_data) {
  if (user_data == nullptr) {
    return false;
  }
  return reinterpret_cast<AndroidCompositor*>(user_data)->CollectBackingStore(
      renderer);
}

static bool PresentViewCallback(const FlutterPresentViewInfo* info) {
  if (info == nullptr || info->user_data == nullptr) {
    return false;
  }
  return reinterpret_cast<AndroidCompositor*>(info->user_data)
      ->Present(info->view_id, info->layers, info->layers_count);
}

AndroidCompositor::AndroidCompositor(
    std::shared_ptr<AndroidSurfaceManager> surface_manager,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade)
    : surface_manager_(std::move(surface_manager)),
      jni_facade_(std::move(jni_facade)) {}

AndroidCompositor::~AndroidCompositor() = default;

FlutterCompositor AndroidCompositor::GetCompositor() {
  FlutterCompositor compositor = {};
  compositor.struct_size = sizeof(FlutterCompositor);
  compositor.user_data = this;
  compositor.create_backing_store_callback = CreateBackingStoreCallback;
  compositor.collect_backing_store_callback = CollectBackingStoreCallback;
  compositor.present_view_callback = PresentViewCallback;
  compositor.avoid_backing_store_cache = false;
  return compositor;
}

bool AndroidCompositor::CreateBackingStore(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out) {
  if (surface_manager_ == nullptr) {
    return false;
  }
  return surface_manager_->CreateBackingStore(config, backing_store_out);
}

bool AndroidCompositor::CollectBackingStore(const FlutterBackingStore* store) {
  if (surface_manager_ == nullptr) {
    return false;
  }
  return surface_manager_->CollectBackingStore(store);
}

MutatorsStack AndroidCompositor::ConvertMutators(
    const FlutterPlatformView* platform_view) {
  MutatorsStack stack;
  if (platform_view == nullptr || platform_view->mutations == nullptr ||
      platform_view->mutations_count == 0) {
    return stack;
  }

  for (size_t i = 0; i < platform_view->mutations_count; ++i) {
    const FlutterPlatformViewMutation* mutation = platform_view->mutations[i];
    if (mutation == nullptr) {
      continue;
    }

    switch (mutation->type) {
      case kFlutterPlatformViewMutationTypeTransformation: {
        const FlutterTransformation& trans = mutation->transformation;
        // FlutterTransformation stores a 3x3 perspective/affine matrix in
        // row-major order:
        // [scaleX, skewX, transX]
        // [skewY,  scaleY, transY]
        // [pers0,  pers1,  pers2 ]
        // Map into a 4x4 column-major DlMatrix.
        constexpr DlScalar kDefaultZ = 0.0f;
        constexpr DlScalar kIdentityW = 1.0f;
        DlMatrix matrix = DlMatrix::MakeColumn(
            static_cast<DlScalar>(trans.scaleX),
            static_cast<DlScalar>(trans.skewY), kDefaultZ,
            static_cast<DlScalar>(trans.pers0),
            static_cast<DlScalar>(trans.skewX),
            static_cast<DlScalar>(trans.scaleY), kDefaultZ,
            static_cast<DlScalar>(trans.pers1), kDefaultZ, kDefaultZ,
            kIdentityW, kDefaultZ, static_cast<DlScalar>(trans.transX),
            static_cast<DlScalar>(trans.transY), kDefaultZ,
            static_cast<DlScalar>(trans.pers2));
        stack.PushTransform(matrix);
        break;
      }
      case kFlutterPlatformViewMutationTypeClipRect: {
        const FlutterRect& rect = mutation->clip_rect;
        DlRect dl_rect = DlRect::MakeLTRB(static_cast<DlScalar>(rect.left),
                                          static_cast<DlScalar>(rect.top),
                                          static_cast<DlScalar>(rect.right),
                                          static_cast<DlScalar>(rect.bottom));
        stack.PushClipRect(dl_rect);
        break;
      }
      case kFlutterPlatformViewMutationTypeClipRoundedRect: {
        const FlutterRoundedRect& rrect = mutation->clip_rounded_rect;
        DlRect dl_bounds =
            DlRect::MakeLTRB(static_cast<DlScalar>(rrect.rect.left),
                             static_cast<DlScalar>(rrect.rect.top),
                             static_cast<DlScalar>(rrect.rect.right),
                             static_cast<DlScalar>(rrect.rect.bottom));
        DlRoundingRadii dl_radii = {
            .top_left = DlSize(
                static_cast<DlScalar>(rrect.upper_left_corner_radius.width),
                static_cast<DlScalar>(rrect.upper_left_corner_radius.height)),
            .top_right = DlSize(
                static_cast<DlScalar>(rrect.upper_right_corner_radius.width),
                static_cast<DlScalar>(rrect.upper_right_corner_radius.height)),
            .bottom_left = DlSize(
                static_cast<DlScalar>(rrect.lower_left_corner_radius.width),
                static_cast<DlScalar>(rrect.lower_left_corner_radius.height)),
            .bottom_right = DlSize(
                static_cast<DlScalar>(rrect.lower_right_corner_radius.width),
                static_cast<DlScalar>(rrect.lower_right_corner_radius.height)),
        };
        stack.PushClipRRect(DlRoundRect::MakeRectRadii(dl_bounds, dl_radii));
        break;
      }
      case kFlutterPlatformViewMutationTypeOpacity: {
        // Clamp normalized opacity in [0.0, 1.0] and map to 8-bit alpha [0,
        // 255].
        constexpr double kMinOpacity = 0.0;
        constexpr double kMaxOpacity = 1.0;
        constexpr double kAlphaScale = 255.0;
        double clamped =
            std::clamp(mutation->opacity, kMinOpacity, kMaxOpacity);
        uint8_t alpha = static_cast<uint8_t>(std::round(clamped * kAlphaScale));
        stack.PushOpacity(alpha);
        break;
      }
    }
  }

  return stack;
}

void AndroidCompositor::PresentPlatformView(const FlutterLayer* layer,
                                            MutatorsStack mutators_stack) {
  if (layer == nullptr || layer->platform_view == nullptr) {
    return;
  }

  if (jni_facade_ != nullptr) {
    int view_id = static_cast<int>(layer->platform_view->identifier);
    int x = static_cast<int>(std::round(layer->offset.x));
    int y = static_cast<int>(std::round(layer->offset.y));
    int width = static_cast<int>(std::round(layer->size.width));
    int height = static_cast<int>(std::round(layer->size.height));
    jni_facade_->FlutterViewOnDisplayPlatformView(
        view_id, x, y, width, height, width, height, std::move(mutators_stack));
  }
}

bool AndroidCompositor::Present(FlutterViewId view_id,
                                const FlutterLayer** layers,
                                size_t layers_count) {
  if (layers == nullptr && layers_count > 0) {
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  PresentedFrame frame;
  frame.view_id = view_id;

  for (size_t i = 0; i < layers_count; ++i) {
    const FlutterLayer* layer = layers[i];
    if (layer == nullptr) {
      continue;
    }

    if (i == 0) {
      frame.presentation_time = layer->presentation_time;
    }

    switch (layer->type) {
      case kFlutterLayerContentTypeBackingStore:
        frame.backing_store_count++;
        break;
      case kFlutterLayerContentTypePlatformView:
        frame.platform_view_count++;
        if (layer->platform_view != nullptr) {
          frame.platform_view_ids.push_back(layer->platform_view->identifier);
          MutatorsStack mutators = ConvertMutators(layer->platform_view);
          frame.platform_view_mutators.push_back(mutators);
          PresentPlatformView(layer, std::move(mutators));
        }
        break;
    }
  }

  last_presented_frame_ = std::move(frame);
  return true;
}

void AndroidCompositor::AddView(FlutterViewId view_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  active_views_[view_id] = true;
}

void AndroidCompositor::RemoveView(FlutterViewId view_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  active_views_.erase(view_id);
}

AndroidCompositor::PresentedFrame AndroidCompositor::GetLastPresentedFrame()
    const {
  std::lock_guard<std::mutex> lock(mutex_);
  return last_presented_frame_;
}

size_t AndroidCompositor::GetViewCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return active_views_.size();
}

}  // namespace flutter
