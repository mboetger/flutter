// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_MUTATORS_STACK_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_MUTATORS_STACK_H_

#include <memory>
#include <variant>
#include <vector>

#include "flutter/display_list/geometry/dl_geometry_types.h"
#include "flutter/display_list/geometry/dl_path.h"
#include "flutter/fml/logging.h"

namespace flutter {

enum class AndroidMutatorType {
  kClipRect,
  kClipRRect,
  kClipRSE,
  kClipPath,
  kTransform,
  kOpacity,
};

class AndroidMutator {
 public:
  AndroidMutator(const AndroidMutator& other) : data_(other.data_) {}

  explicit AndroidMutator(const DlRect& rect) : data_(rect) {}
  explicit AndroidMutator(const DlRoundRect& rrect) : data_(rrect) {}
  explicit AndroidMutator(const DlRoundSuperellipse& rse) : data_(rse) {}
  explicit AndroidMutator(const DlPath& path) : data_(path) {}
  explicit AndroidMutator(const DlMatrix& matrix) : data_(matrix) {}
  explicit AndroidMutator(const uint8_t& alpha) : data_(alpha) {}

  AndroidMutatorType GetType() const {
    switch (data_.index()) {
      case 0:
        return AndroidMutatorType::kClipRect;
      case 1:
        return AndroidMutatorType::kClipRRect;
      case 2:
        return AndroidMutatorType::kClipRSE;
      case 3:
        return AndroidMutatorType::kClipPath;
      case 4:
        return AndroidMutatorType::kTransform;
      case 5:
        return AndroidMutatorType::kOpacity;
      default:
        FML_CHECK(false);
        return AndroidMutatorType::kTransform;
    }
  }

  const DlRect& GetRect() const { return std::get<DlRect>(data_); }
  const DlRoundRect& GetRRect() const { return std::get<DlRoundRect>(data_); }
  const DlRoundSuperellipse& GetRSE() const {
    return std::get<DlRoundSuperellipse>(data_);
  }
  DlRoundRect GetRSEApproximation() const {
    const auto& rse = GetRSE();
    return DlRoundRect::MakeRectRadii(rse.GetBounds(), rse.GetRadii());
  }
  const DlPath& GetPath() const { return std::get<DlPath>(data_); }
  const DlMatrix& GetMatrix() const { return std::get<DlMatrix>(data_); }
  const uint8_t& GetAlpha() const { return std::get<uint8_t>(data_); }
  float GetAlphaFloat() const {
    return static_cast<float>(GetAlpha()) / 255.0f;
  }

  bool operator==(const AndroidMutator& other) const {
    return data_ == other.data_;
  }
  bool operator!=(const AndroidMutator& other) const {
    return !(*this == other);
  }

  bool IsClipType() const {
    AndroidMutatorType type = GetType();
    return type == AndroidMutatorType::kClipRect ||
           type == AndroidMutatorType::kClipRRect ||
           type == AndroidMutatorType::kClipRSE ||
           type == AndroidMutatorType::kClipPath;
  }

 private:
  std::variant<DlRect,
               DlRoundRect,
               DlRoundSuperellipse,
               DlPath,
               DlMatrix,
               uint8_t>
      data_;
};

class AndroidMutatorsStack {
 public:
  AndroidMutatorsStack() = default;

  void PushClipRect(const DlRect& rect) {
    stack_.push_back(std::make_shared<AndroidMutator>(rect));
  }

  void PushClipRRect(const DlRoundRect& rrect) {
    stack_.push_back(std::make_shared<AndroidMutator>(rrect));
  }

  void PushClipRSE(const DlRoundSuperellipse& rse) {
    stack_.push_back(std::make_shared<AndroidMutator>(rse));
  }

  void PushClipPath(const DlPath& path) {
    stack_.push_back(std::make_shared<AndroidMutator>(path));
  }

  void PushTransform(const DlMatrix& matrix) {
    stack_.push_back(std::make_shared<AndroidMutator>(matrix));
  }

  void PushOpacity(const uint8_t& alpha) {
    stack_.push_back(std::make_shared<AndroidMutator>(alpha));
  }

  void Pop() {
    if (!stack_.empty()) {
      stack_.pop_back();
    }
  }

  auto Bottom() const { return stack_.crbegin(); }
  auto Top() const { return stack_.crend(); }
  auto Begin() const { return stack_.cbegin(); }
  auto End() const { return stack_.cend(); }

  bool is_empty() const { return stack_.empty(); }
  size_t size() const { return stack_.size(); }
  size_t stack_count() const { return stack_.size(); }

  bool operator==(const AndroidMutatorsStack& other) const {
    if (stack_.size() != other.stack_.size()) {
      return false;
    }
    for (size_t i = 0; i < stack_.size(); ++i) {
      if (*stack_[i] != *other.stack_[i]) {
        return false;
      }
    }
    return true;
  }

  bool operator!=(const AndroidMutatorsStack& other) const {
    return !(*this == other);
  }

 private:
  std::vector<std::shared_ptr<AndroidMutator>> stack_;
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_MUTATORS_STACK_H_
