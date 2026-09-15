// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "flutter/fml/message_loop.h"
#include "flutter/fml/trace_event.h"
#include "flutter/shell/platform/android/android_shell_holder.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "flutter/shell/platform/android/platform_message_handler_android.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

// PARITY CONTRACT:
// Blocker B-3: Platform Message Thread Affinity Divergence
//
// PlatformMessageHandlerAndroid::DoesHandlePlatformMessageOnPlatformThread()
// returns FALSE on Android.
// In contrast,
// PlatformViewEmbedder::DoesHandlePlatformMessageOnPlatformThread() returns
// TRUE.
//
// Naive adoption of the Embedder API would flip this value to TRUE, forcing
// message dispatch to the platform thread for all channel messages and
// breaking background TaskQueues (documented Flutter feature allowing channel
// handlers on background threads).
//
// Stage 1 / Spike T-0.8 will resolve this architecture gap.
// This test locks down the existing contract on Android.

class MockPlatformMessageResponse : public PlatformMessageResponse {
 public:
  static fml::RefPtr<MockPlatformMessageResponse> Create() {
    return fml::AdoptRef(new MockPlatformMessageResponse());
  }
  MOCK_METHOD(void, Complete, (std::unique_ptr<fml::Mapping> data), (override));
  MOCK_METHOD(void, CompleteEmpty, (), (override));
};

TEST(PlatformMessageAffinityTest,
     DoesHandlePlatformMessageOnPlatformThreadIsFalse) {
  TRACE_EVENT0("flutter", "DoesHandlePlatformMessageOnPlatformThreadIsFalse");
  auto jni = std::make_shared<JNIMock>();
  auto handler = std::make_shared<PlatformMessageHandlerAndroid>(jni);

  // Parity contract: must return false on Android.
  EXPECT_FALSE(handler->DoesHandlePlatformMessageOnPlatformThread());
}

TEST(PlatformMessageAffinityTest, MessageDispatchAndResponseRouting) {
  TRACE_EVENT0("flutter", "MessageDispatchAndResponseRouting");
  fml::MessageLoop::EnsureInitializedForCurrentThread();

  auto jni = std::make_shared<JNIMock>();
  auto handler = std::make_shared<PlatformMessageHandlerAndroid>(jni);

  size_t data_size = 4;
  fml::MallocMapping bytes =
      fml::MallocMapping(static_cast<uint8_t*>(malloc(data_size)), data_size);
  fml::RefPtr<MockPlatformMessageResponse> response =
      MockPlatformMessageResponse::Create();
  auto message = std::make_unique<PlatformMessage>("test_channel",
                                                   std::move(bytes), response);

  int response_id = 1;
  EXPECT_CALL(*jni,
              FlutterViewHandlePlatformMessage(::testing::_, response_id));
  EXPECT_CALL(*response, CompleteEmpty());

  handler->HandlePlatformMessage(std::move(message));
  handler->InvokePlatformMessageEmptyResponseCallback(response_id);
}

}  // namespace testing
}  // namespace flutter
