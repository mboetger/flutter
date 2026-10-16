// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/embedder/embedder_layers.h"

#include <algorithm>

namespace flutter {

EmbedderLayers::EmbedderLayers(DlISize frame_size,
                               double device_pixel_ratio,
                               DlMatrix root_surface_transformation,
                               uint64_t presentation_time,
                               uint64_t platform_view_mutation_capabilities)
    : frame_size_(frame_size),
      device_pixel_ratio_(device_pixel_ratio),
      root_surface_transformation_(root_surface_transformation),
      platform_view_mutation_capabilities_(platform_view_mutation_capabilities),
      presentation_time_(presentation_time) {}

EmbedderLayers::~EmbedderLayers() = default;

void EmbedderLayers::PushBackingStoreLayer(
    const FlutterBackingStore* store,
    const std::vector<DlIRect>& paint_region_vec) {
  FlutterLayer layer = {};

  layer.struct_size = sizeof(FlutterLayer);
  layer.type = kFlutterLayerContentTypeBackingStore;
  layer.backing_store = store;

  const auto layer_bounds = DlRect::MakeSize(frame_size_);

  const auto transformed_layer_bounds =
      layer_bounds.TransformAndClipBounds(root_surface_transformation_);

  layer.offset.x = transformed_layer_bounds.GetX();
  layer.offset.y = transformed_layer_bounds.GetY();
  layer.size.width = transformed_layer_bounds.GetWidth();
  layer.size.height = transformed_layer_bounds.GetHeight();

  auto paint_region_rects = std::make_unique<std::vector<FlutterRect>>();
  paint_region_rects->reserve(paint_region_vec.size());

  for (const auto& rect : paint_region_vec) {
    auto transformed_rect =
        DlRect::Make(rect).TransformAndClipBounds(root_surface_transformation_);
    paint_region_rects->push_back(FlutterRect{
        .left = transformed_rect.GetLeft(),
        .top = transformed_rect.GetTop(),
        .right = transformed_rect.GetRight(),
        .bottom = transformed_rect.GetBottom(),
    });
  }

  auto paint_region = std::make_unique<FlutterRegion>();
  paint_region->struct_size = sizeof(FlutterRegion);
  paint_region->rects = paint_region_rects->data();
  paint_region->rects_count = paint_region_rects->size();
  rects_referenced_.push_back(std::move(paint_region_rects));

  auto present_info = std::make_unique<FlutterBackingStorePresentInfo>();
  present_info->struct_size = sizeof(FlutterBackingStorePresentInfo);
  present_info->paint_region = paint_region.get();
  present_info->synchronization_fence_fd = -1;
  regions_referenced_.push_back(std::move(paint_region));
  layer.backing_store_present_info = present_info.get();
  layer.presentation_time = presentation_time_;

  present_info_referenced_.push_back(std::move(present_info));
  presented_layers_.push_back(layer);
}

static std::unique_ptr<FlutterPlatformViewMutation> ConvertMutation(
    double opacity) {
  FlutterPlatformViewMutation mutation = {};
  mutation.type = kFlutterPlatformViewMutationTypeOpacity;
  mutation.opacity = opacity;
  return std::make_unique<FlutterPlatformViewMutation>(mutation);
}

static std::unique_ptr<FlutterPlatformViewMutation> ConvertMutation(
    const DlRect& rect) {
  FlutterPlatformViewMutation mutation = {};
  mutation.type = kFlutterPlatformViewMutationTypeClipRect;
  mutation.clip_rect.left = rect.GetLeft();
  mutation.clip_rect.top = rect.GetTop();
  mutation.clip_rect.right = rect.GetRight();
  mutation.clip_rect.bottom = rect.GetBottom();
  return std::make_unique<FlutterPlatformViewMutation>(mutation);
}

static FlutterSize ConvertSize(const DlSize& vector) {
  FlutterSize size = {};
  size.width = vector.width;
  size.height = vector.height;
  return size;
}

static std::unique_ptr<FlutterPlatformViewMutation> ConvertMutation(
    const DlRoundRect& rrect) {
  FlutterPlatformViewMutation mutation = {};
  mutation.type = kFlutterPlatformViewMutationTypeClipRoundedRect;
  const auto& rect = rrect.GetBounds();
  mutation.clip_rounded_rect.rect.left = rect.GetLeft();
  mutation.clip_rounded_rect.rect.top = rect.GetTop();
  mutation.clip_rounded_rect.rect.right = rect.GetRight();
  mutation.clip_rounded_rect.rect.bottom = rect.GetBottom();
  const auto& radii = rrect.GetRadii();
  mutation.clip_rounded_rect.upper_left_corner_radius =
      ConvertSize(radii.top_left);
  mutation.clip_rounded_rect.upper_right_corner_radius =
      ConvertSize(radii.top_right);
  mutation.clip_rounded_rect.lower_right_corner_radius =
      ConvertSize(radii.bottom_right);
  mutation.clip_rounded_rect.lower_left_corner_radius =
      ConvertSize(radii.bottom_left);
  return std::make_unique<FlutterPlatformViewMutation>(mutation);
}

static std::unique_ptr<FlutterPlatformViewMutation> ConvertMutation(
    const DlMatrix& matrix) {
  FlutterPlatformViewMutation mutation = {};
  mutation.type = kFlutterPlatformViewMutationTypeTransformation;
  mutation.transformation.scaleX = matrix.m[0];
  mutation.transformation.skewX = matrix.m[4];
  mutation.transformation.transX = matrix.m[12];
  mutation.transformation.skewY = matrix.m[1];
  mutation.transformation.scaleY = matrix.m[5];
  mutation.transformation.transY = matrix.m[13];
  mutation.transformation.pers0 = matrix.m[3];
  mutation.transformation.pers1 = matrix.m[7];
  mutation.transformation.pers2 = matrix.m[15];
  return std::make_unique<FlutterPlatformViewMutation>(mutation);
}

namespace {

class FlutterPathReceiver final : public DlPathReceiver {
 public:
  FlutterPathReceiver(std::vector<FlutterPathVerb>& verbs,
                      std::vector<FlutterPoint>& points,
                      std::vector<double>& conic_weights,
                      bool allow_conics)
      : verbs_(verbs),
        points_(points),
        conic_weights_(conic_weights),
        allow_conics_(allow_conics) {}

  void MoveTo(const DlPoint& p2, bool will_be_closed) override {
    verbs_.push_back(kFlutterPathVerbMove);
    points_.push_back({static_cast<double>(p2.x), static_cast<double>(p2.y)});
  }

  void LineTo(const DlPoint& p2) override {
    verbs_.push_back(kFlutterPathVerbLine);
    points_.push_back({static_cast<double>(p2.x), static_cast<double>(p2.y)});
  }

  void QuadTo(const DlPoint& cp, const DlPoint& p2) override {
    verbs_.push_back(kFlutterPathVerbQuad);
    points_.push_back({static_cast<double>(cp.x), static_cast<double>(cp.y)});
    points_.push_back({static_cast<double>(p2.x), static_cast<double>(p2.y)});
  }

  bool ConicTo(const DlPoint& cp, const DlPoint& p2, DlScalar weight) override {
    if (!allow_conics_) {
      return false;
    }
    verbs_.push_back(kFlutterPathVerbConic);
    points_.push_back({static_cast<double>(cp.x), static_cast<double>(cp.y)});
    points_.push_back({static_cast<double>(p2.x), static_cast<double>(p2.y)});
    conic_weights_.push_back(static_cast<double>(weight));
    return true;
  }

  void CubicTo(const DlPoint& cp1,
               const DlPoint& cp2,
               const DlPoint& p2) override {
    verbs_.push_back(kFlutterPathVerbCubic);
    points_.push_back({static_cast<double>(cp1.x), static_cast<double>(cp1.y)});
    points_.push_back({static_cast<double>(cp2.x), static_cast<double>(cp2.y)});
    points_.push_back({static_cast<double>(p2.x), static_cast<double>(p2.y)});
  }

  void Close() override { verbs_.push_back(kFlutterPathVerbClose); }

 private:
  std::vector<FlutterPathVerb>& verbs_;
  std::vector<FlutterPoint>& points_;
  std::vector<double>& conic_weights_;
  bool allow_conics_;
};

}  // namespace

void EmbedderLayers::PushPlatformViewLayer(
    FlutterPlatformViewIdentifier identifier,
    const EmbeddedViewParams& params) {
  {
    FlutterPlatformView view = {};
    view.struct_size = sizeof(FlutterPlatformView);
    view.identifier = identifier;

    const auto& mutators = params.mutatorsStack();

    std::vector<const FlutterPlatformViewMutation*> mutations_array;

    for (auto i = mutators.Bottom(); i != mutators.Top(); ++i) {
      const auto& mutator = *i;
      switch (mutator->GetType()) {
        case MutatorType::kClipRect: {
          mutations_array.push_back(
              mutations_referenced_
                  .emplace_back(ConvertMutation(mutator->GetRect()))
                  .get());
        } break;
        case MutatorType::kClipRRect: {
          mutations_array.push_back(
              mutations_referenced_
                  .emplace_back(ConvertMutation(mutator->GetRRect()))
                  .get());
        } break;
        case MutatorType::kClipRSE: {
          mutations_array.push_back(
              mutations_referenced_
                  .emplace_back(ConvertMutation(mutator->GetRSEApproximation()))
                  .get());
        } break;
        case MutatorType::kClipPath: {
          const DlPath& dl_path = mutator->GetPath();
          auto ref_path = std::make_unique<ReferencedPath>();
          ref_path->path.struct_size = sizeof(FlutterPath);
          ref_path->path.fill_type =
              (dl_path.GetFillType() == DlPathFillType::kOdd)
                  ? kFlutterPathFillTypeEvenOdd
                  : kFlutterPathFillTypeNonZero;
          const bool allow_conics =
              (platform_view_mutation_capabilities_ &
               kFlutterPlatformViewMutationCapabilityPathConics) != 0;
          FlutterPathReceiver receiver(ref_path->verbs, ref_path->points,
                                       ref_path->conic_weights, allow_conics);
          dl_path.Dispatch(receiver);
          ref_path->path.verbs_count = ref_path->verbs.size();
          ref_path->path.verbs =
              ref_path->verbs.empty() ? nullptr : ref_path->verbs.data();
          ref_path->path.points_count = ref_path->points.size();
          ref_path->path.points =
              ref_path->points.empty() ? nullptr : ref_path->points.data();
          ref_path->path.conic_weights_count = ref_path->conic_weights.size();
          ref_path->path.conic_weights = ref_path->conic_weights.empty()
                                             ? nullptr
                                             : ref_path->conic_weights.data();
          const DlRect bounds = dl_path.GetBounds();
          ref_path->path.bounds = FlutterRect{
              .left = bounds.GetLeft(),
              .top = bounds.GetTop(),
              .right = bounds.GetRight(),
              .bottom = bounds.GetBottom(),
          };
          const FlutterPath* path_ptr =
              &paths_referenced_.emplace_back(std::move(ref_path))->path;
          FlutterPlatformViewMutation mutation = {};
          mutation.type = kFlutterPlatformViewMutationTypeClipPath;
          mutation.clip_path = path_ptr;
          mutations_array.push_back(
              mutations_referenced_
                  .emplace_back(
                      std::make_unique<FlutterPlatformViewMutation>(mutation))
                  .get());
        } break;
        case MutatorType::kTransform: {
          const auto& matrix = mutator->GetMatrix();
          if (!matrix.IsIdentity()) {
            mutations_array.push_back(
                mutations_referenced_.emplace_back(ConvertMutation(matrix))
                    .get());
          }
        } break;
        case MutatorType::kOpacity: {
          const double opacity =
              std::clamp(mutator->GetAlphaFloat(), 0.0f, 1.0f);
          if (opacity < 1.0) {
            mutations_array.push_back(
                mutations_referenced_.emplace_back(ConvertMutation(opacity))
                    .get());
          }
        } break;
        case MutatorType::kBackdropFilter:
        case MutatorType::kBackdropClipRect:
        case MutatorType::kBackdropClipRRect:
        case MutatorType::kBackdropClipRSuperellipse:
        case MutatorType::kBackdropClipPath:
          break;
      }
    }

    if (!mutations_array.empty()) {
      // If there are going to be any mutations, they must first take into
      // account the root surface transformation.
      if (!root_surface_transformation_.IsIdentity()) {
        mutations_array.push_back(
            mutations_referenced_
                .emplace_back(ConvertMutation(root_surface_transformation_))
                .get());
      }

      auto mutations =
          std::make_unique<std::vector<const FlutterPlatformViewMutation*>>(
              mutations_array.rbegin(), mutations_array.rend());
      mutations_arrays_referenced_.emplace_back(std::move(mutations));

      view.mutations_count = mutations_array.size();
      view.mutations = mutations_arrays_referenced_.back().get()->data();
    }

    platform_views_referenced_.emplace_back(
        std::make_unique<FlutterPlatformView>(view));
  }

  FlutterLayer layer = {};

  layer.struct_size = sizeof(FlutterLayer);
  layer.type = kFlutterLayerContentTypePlatformView;
  layer.platform_view = platform_views_referenced_.back().get();

  const auto layer_bounds =
      DlRect::MakeXYWH(params.finalBoundingRect().GetX(),                //
                       params.finalBoundingRect().GetY(),                //
                       params.sizePoints().width * device_pixel_ratio_,  //
                       params.sizePoints().height * device_pixel_ratio_  //
      );

  const auto transformed_layer_bounds =
      layer_bounds.TransformAndClipBounds(root_surface_transformation_);

  layer.offset.x = transformed_layer_bounds.GetX();
  layer.offset.y = transformed_layer_bounds.GetY();
  layer.size.width = transformed_layer_bounds.GetWidth();
  layer.size.height = transformed_layer_bounds.GetHeight();

  layer.presentation_time = presentation_time_;

  presented_layers_.push_back(layer);
}

void EmbedderLayers::InvokePresentCallback(
    FlutterViewId view_id,
    const PresentCallback& callback) const {
  std::vector<const FlutterLayer*> presented_layers_pointers;
  presented_layers_pointers.reserve(presented_layers_.size());
  for (const auto& layer : presented_layers_) {
    presented_layers_pointers.push_back(&layer);
  }
  callback(view_id, presented_layers_pointers);
}

}  // namespace flutter
