// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/jni/jni_mock.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

TEST(JNIMock, FlutterViewHandlePlatformMessage) {
  JNIMock mock;

  UniquePlatformMessagePayload payload(new FlutterPlatformMessagePayload{
      .struct_size = sizeof(FlutterPlatformMessagePayload),
  });
  auto* raw_payload = payload.get();
  auto response_id = 1;

  EXPECT_CALL(mock, FlutterViewHandlePlatformMessage(
                        "<channel-name>",
                        ::testing::Property(&UniquePlatformMessagePayload::get,
                                            raw_payload),
                        response_id));

  mock.FlutterViewHandlePlatformMessage("<channel-name>", std::move(payload),
                                        response_id);
}

}  // namespace testing
}  // namespace flutter
