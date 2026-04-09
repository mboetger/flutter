// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/jni/android_mutators.h"

#include <algorithm>

#include "flutter/display_list/geometry/dl_path.h"
#include "flutter/display_list/geometry/dl_geometry_types.h"
#include "flutter/flow/embedded_views.h"

namespace flutter {

std::vector<AndroidMutator> ToAndroidMutators(const MutatorsStack& mutators_stack) {
  std::vector<AndroidMutator> mutators;
  for (auto iter = mutators_stack.Begin(); iter != mutators_stack.End(); ++iter) {
    AndroidMutator mutator;
    switch ((*iter)->GetType()) {
      case MutatorType::kTransform: {
        mutator.type = AndroidMutatorType::kTransform;
        const DlMatrix& matrix = (*iter)->GetMatrix();
        // matrix.m is 16 floats.
        std::copy(matrix.m, matrix.m + 16, mutator.transformation);
        break;
      }
      case MutatorType::kClipRect: {
        mutator.type = AndroidMutatorType::kClipRect;
        const DlRect& rect = (*iter)->GetRect();
        mutator.rect.left = rect.GetLeft();
        mutator.rect.top = rect.GetTop();
        mutator.rect.right = rect.GetRight();
        mutator.rect.bottom = rect.GetBottom();
        break;
      }
      case MutatorType::kClipRRect: {
        mutator.type = AndroidMutatorType::kClipRRect;
        const DlRoundRect& rrect = (*iter)->GetRRect();
        const DlRect& rect = rrect.GetBounds();
        const DlRoundingRadii radii = rrect.GetRadii();
        mutator.rrect.left = rect.GetLeft();
        mutator.rrect.top = rect.GetTop();
        mutator.rrect.right = rect.GetRight();
        mutator.rrect.bottom = rect.GetBottom();
        mutator.rrect.radii[0] = radii.top_left.width;
        mutator.rrect.radii[1] = radii.top_left.height;
        mutator.rrect.radii[2] = radii.top_right.width;
        mutator.rrect.radii[3] = radii.top_right.height;
        mutator.rrect.radii[4] = radii.bottom_right.width;
        mutator.rrect.radii[5] = radii.bottom_right.height;
        mutator.rrect.radii[6] = radii.bottom_left.width;
        mutator.rrect.radii[7] = radii.bottom_left.height;
        break;
      }
      case MutatorType::kClipRSE: {
        mutator.type = AndroidMutatorType::kClipRRect;
        const DlRoundRect& rrect = (*iter)->GetRSEApproximation();
        const DlRect& rect = rrect.GetBounds();
        const DlRoundingRadii radii = rrect.GetRadii();
        mutator.rrect.left = rect.GetLeft();
        mutator.rrect.top = rect.GetTop();
        mutator.rrect.right = rect.GetRight();
        mutator.rrect.bottom = rect.GetBottom();
        mutator.rrect.radii[0] = radii.top_left.width;
        mutator.rrect.radii[1] = radii.top_left.height;
        mutator.rrect.radii[2] = radii.top_right.width;
        mutator.rrect.radii[3] = radii.top_right.height;
        mutator.rrect.radii[4] = radii.bottom_right.width;
        mutator.rrect.radii[5] = radii.bottom_right.height;
        mutator.rrect.radii[6] = radii.bottom_left.width;
        mutator.rrect.radii[7] = radii.bottom_left.height;
        break;
      }
      case MutatorType::kOpacity: {
        mutator.type = AndroidMutatorType::kOpacity;
        mutator.opacity = (*iter)->GetAlphaFloat();
        break;
      }
      case MutatorType::kClipPath: {
        mutator.type = AndroidMutatorType::kClipPath;
        // DlPath is passed as void*
        mutator.path = const_cast<DlPath*>(&(*iter)->GetPath());
        break;
      }
      default:
        // Ignore other types for now.
        continue;
    }
    mutators.push_back(mutator);
  }
  return mutators;
}

}  // namespace flutter
