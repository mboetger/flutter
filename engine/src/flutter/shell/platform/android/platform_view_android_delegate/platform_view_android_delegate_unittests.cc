// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/platform_view_android_delegate/platform_view_android_delegate.h"

#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "flutter/shell/platform/embedder/embedder_semantics_update.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

using ::testing::_;

TEST(PlatformViewShell, UpdateSemanticsDoesFlutterViewUpdateSemantics) {
  auto jni_mock = std::make_shared<JNIMock>();
  auto delegate = std::make_unique<PlatformViewAndroidDelegate>(jni_mock);

  flutter::SemanticsNodeUpdates update;
  flutter::SemanticsNode node0;
  node0.id = 0;
  node0.identifier = "identifier";
  node0.label = "label";
  node0.tooltip = "tooltip";
  update.insert(std::make_pair(0, node0));

  flutter::CustomAccessibilityActionUpdates actions;
  EmbedderSemanticsUpdate2 embedder_update(0, update, actions);

  std::vector<uint8_t> expected_buffer(
      PlatformViewAndroidDelegate::kBytesPerNode);
  std::vector<std::vector<uint8_t>> expected_string_attribute_args(0);
  size_t position = 0;
  int32_t* buffer_int32 = reinterpret_cast<int32_t*>(&expected_buffer[0]);
  float* buffer_float32 = reinterpret_cast<float*>(&expected_buffer[0]);
  std::vector<std::string> expected_strings;
  buffer_int32[position++] = node0.id;
  int64_t expected_flags = 0;
  std::memcpy(&buffer_int32[position], &expected_flags, 8);
  position += 2;
  buffer_int32[position++] = node0.actions;
  buffer_int32[position++] = node0.maxValueLength;
  buffer_int32[position++] = node0.currentValueLength;
  buffer_int32[position++] = node0.textSelectionBase;
  buffer_int32[position++] = node0.textSelectionExtent;
  buffer_int32[position++] = node0.platformViewId;
  buffer_int32[position++] = node0.scrollChildren;
  buffer_int32[position++] = node0.scrollIndex;
  buffer_int32[position++] = node0.traversalParent;
  buffer_float32[position++] = static_cast<float>(node0.scrollPosition);
  buffer_float32[position++] = static_cast<float>(node0.scrollExtentMax);
  buffer_float32[position++] = static_cast<float>(node0.scrollExtentMin);
  buffer_int32[position++] = static_cast<int32_t>(node0.role);

  // identifier
  buffer_int32[position++] = expected_strings.size();
  expected_strings.push_back(node0.identifier);
  // label
  buffer_int32[position++] = expected_strings.size();
  expected_strings.push_back(node0.label);
  buffer_int32[position++] = -1;  // labelAttributes
  buffer_int32[position++] = -1;  // value
  buffer_int32[position++] = -1;  // valueAttributes
  buffer_int32[position++] = -1;  // increasedValue
  buffer_int32[position++] = -1;  // increasedValueAttributes
  buffer_int32[position++] = -1;  // decreasedValue
  buffer_int32[position++] = -1;  // decreasedValueAttributes
  buffer_int32[position++] = -1;  // hint
  buffer_int32[position++] = -1;  // hintAttributes
  // tooltip
  buffer_int32[position++] = expected_strings.size();
  expected_strings.push_back(node0.tooltip);
  buffer_int32[position++] = -1;  // linkUrl
  buffer_int32[position++] = -1;  // locale
  buffer_int32[position++] = -1;  // minValue
  buffer_int32[position++] = -1;  // maxValue

  buffer_int32[position++] = node0.headingLevel;
  buffer_int32[position++] = node0.textDirection;
  buffer_float32[position++] = node0.rect.left();
  buffer_float32[position++] = node0.rect.top();
  buffer_float32[position++] = node0.rect.right();
  buffer_float32[position++] = node0.rect.bottom();
  // Transform (16 floats)
  buffer_float32[position++] = 1.0;  // scaleX
  for (int i = 0; i < 8; i++)
    buffer_float32[position++] = 0.0;
  buffer_float32[position++] = 1.0;  // scaleY
  for (int i = 0; i < 3; i++)
    buffer_float32[position++] = 0.0;
  buffer_float32[position++] = 1.0;  // persp2
  for (int i = 0; i < 2; i++)
    buffer_float32[position++] = 0.0;

  position += 16;  // Skip hitTestTransform

  buffer_int32[position++] = 0;  // child_count
  buffer_int32[position++] = 0;  // child_count (hit test)
  buffer_int32[position++] = 0;  // customAccessibilityActions.size();

  EXPECT_CALL(*jni_mock,
              FlutterViewUpdateSemantics(expected_buffer, expected_strings,
                                         expected_string_attribute_args));
  delegate->UpdateSemantics(embedder_update.get());
}

TEST(PlatformViewShell, UpdateSemanticsDoesUpdateLinkUrl) {
  auto jni_mock = std::make_shared<JNIMock>();
  auto delegate = std::make_unique<PlatformViewAndroidDelegate>(jni_mock);

  flutter::SemanticsNodeUpdates update;
  flutter::SemanticsNode node0;
  node0.id = 0;
  node0.linkUrl = "url";
  update.insert(std::make_pair(0, node0));

  flutter::CustomAccessibilityActionUpdates actions;
  EmbedderSemanticsUpdate2 embedder_update(0, update, actions);

  std::vector<std::string> expected_strings;
  // identifier, label, labelAttributes, value, valueAttributes, increasedValue,
  // increasedValueAttributes, decreasedValue, decreasedValueAttributes, hint,
  // hintAttributes, tooltip linkUrl is the 13th string if all others are
  // provided, but here they are mostly empty. In the implementation: identifier
  // -> putStringIntoBuffer label -> putStringIntoBuffer labelAttributes ->
  // putStringAttributesIntoBuffer value -> putStringIntoBuffer valueAttributes
  // -> putStringAttributesIntoBuffer increasedValue -> putStringIntoBuffer
  // increasedValueAttributes -> putStringAttributesIntoBuffer
  // decreasedValue -> putStringIntoBuffer
  // decreasedValueAttributes -> putStringAttributesIntoBuffer
  // hint -> putStringIntoBuffer
  // hintAttributes -> putStringAttributesIntoBuffer
  // tooltip -> putStringIntoBuffer
  // linkUrl -> putStringIntoBuffer

  // Just check that it doesn't crash and calls JNI with non-empty strings.
  EXPECT_CALL(*jni_mock,
              FlutterViewUpdateSemantics(_, ::testing::Contains("url"), _));
  delegate->UpdateSemantics(embedder_update.get());
}

TEST(PlatformViewShell, UpdateSemanticsDoesUpdateLocale) {
  auto jni_mock = std::make_shared<JNIMock>();
  auto delegate = std::make_unique<PlatformViewAndroidDelegate>(jni_mock);

  flutter::SemanticsNodeUpdates update;
  flutter::SemanticsNode node0;
  node0.id = 0;
  node0.locale = "es-MX";
  update.insert(std::make_pair(0, node0));

  flutter::CustomAccessibilityActionUpdates actions;
  EmbedderSemanticsUpdate2 embedder_update(0, update, actions);

  EXPECT_CALL(*jni_mock,
              FlutterViewUpdateSemantics(_, ::testing::Contains("es-MX"), _));
  delegate->UpdateSemantics(embedder_update.get());
}

TEST(PlatformViewShell,
     UpdateSemanticsDoesFlutterViewUpdateCustomAccessibilityActions) {
  auto jni_mock = std::make_shared<JNIMock>();
  auto delegate = std::make_unique<PlatformViewAndroidDelegate>(jni_mock);

  flutter::SemanticsNodeUpdates update;
  flutter::CustomAccessibilityActionUpdates actions;
  flutter::CustomAccessibilityAction action0;
  action0.id = 0;
  action0.overrideId = 1;
  action0.label = "label";
  action0.hint = "hint";
  actions.insert(std::make_pair(0, action0));

  EmbedderSemanticsUpdate2 embedder_update(0, update, actions);

  EXPECT_CALL(*jni_mock, FlutterViewUpdateCustomAccessibilityActions(
                             _, ::testing::ElementsAre("label", "hint")));
  delegate->UpdateSemantics(embedder_update.get());
}

}  // namespace testing
}  // namespace flutter
