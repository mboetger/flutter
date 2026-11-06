// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/jni/mock_jni_env.h"
#include "flutter/shell/platform/android/jni_router.h"
#include "flutter/shell/platform/android/jvm_invoker.h"
#include "flutter/shell/platform/embedder/embedder.h"
#include "gtest/gtest.h"

namespace flutter::android::testing {

TEST(FlutterEmbedderNativeFirewallTest, JniRouterHandlesNullAndMockEnv) {
  EXPECT_FALSE(JniRouter::RegisterNatives(nullptr));

  MockJniEnv mock_env;
  EXPECT_TRUE(JniRouter::RegisterNatives(mock_env.env()));
}

TEST(FlutterEmbedderNativeFirewallTest, JvmInvokerNullSafeInitialization) {
  JvmInvoker invoker;
  EXPECT_FALSE(invoker.IsValid());
  invoker.OnFlutterEngineLaunched(1);
  invoker.OnPreEngineRestart();
}

}  // namespace flutter::android::testing
