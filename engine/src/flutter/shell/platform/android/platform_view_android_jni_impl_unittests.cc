// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <map>
#include <string>
#include "flutter/fml/platform/android/jni_util.h"
#include "flutter/fml/platform/android/jni_weak_ref.h"

#include "flutter/shell/platform/android/android_engine.h"
#include "flutter/shell/platform/android/android_shell_holder.h"
#include "flutter/shell/platform/android/flutter_main.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "flutter/shell/platform/android/jni/mock_jni_env.h"
#include "flutter/shell/platform/android/platform_view_android.h"
#include "flutter/shell/platform/android/platform_view_android_jni_impl.h"

namespace flutter {
namespace testing {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Return;
using ::testing::ReturnArg;

static std::map<std::string, void*> g_registered_methods;

class PlatformViewAndroidJNIImplTest : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    static std::once_flag jvm_init_flag;
    std::call_once(jvm_init_flag, SetUpJVM);
  }

 protected:
  void SetUp() override { FlutterMain::ResetForTesting(); }

  void TearDown() override { FlutterMain::ResetForTesting(); }

 private:
  friend class MockJNIEnvProvider;
  static MockJavaVM jvm_;
  static void SetUpJVM();
};

MockJavaVM PlatformViewAndroidJNIImplTest::jvm_;

class MockJNIEnvProvider {
 public:
  MockJNIEnvProvider() {
    PlatformViewAndroidJNIImplTest::jvm_.SetJNIEnv(&env_);
  }
  ~MockJNIEnvProvider() {
    PlatformViewAndroidJNIImplTest::jvm_.SetJNIEnv(nullptr);
  }
  MockJNIEnv& env() { return env_; }

 private:
  MockJNIEnv env_;
};

void PlatformViewAndroidJNIImplTest::SetUpJVM() {
  fml::jni::InitJavaVM(&jvm_);

  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  const jclass kPlaceholderClass = reinterpret_cast<jclass>(100);
  const jfieldID kPlaceholderFieldID = reinterpret_cast<jfieldID>(200);
  const jmethodID kPlaceholderMethodID = reinterpret_cast<jmethodID>(300);

  EXPECT_CALL(mock_env, GetObjectRefType(_))
      .WillRepeatedly(Return(JNILocalRefType));
  EXPECT_CALL(mock_env, NewLocalRef(_)).WillRepeatedly(ReturnArg<0>());
  EXPECT_CALL(mock_env, DeleteLocalRef(_)).WillRepeatedly(Return());
  EXPECT_CALL(mock_env, NewGlobalRef(_)).WillRepeatedly(ReturnArg<0>());
  EXPECT_CALL(mock_env, DeleteGlobalRef(_)).WillRepeatedly(Return());
  EXPECT_CALL(mock_env, FindClass(_)).WillRepeatedly(Return(kPlaceholderClass));
  EXPECT_CALL(mock_env, GetFieldID(_, _, _))
      .WillRepeatedly(Return(kPlaceholderFieldID));
  EXPECT_CALL(mock_env, GetMethodID(_, _, _))
      .WillRepeatedly(Return(kPlaceholderMethodID));
  EXPECT_CALL(mock_env, GetStaticFieldID(_, _, _))
      .WillRepeatedly(Return(kPlaceholderFieldID));
  EXPECT_CALL(mock_env, GetStaticMethodID(_, _, _))
      .WillRepeatedly(Return(kPlaceholderMethodID));
  EXPECT_CALL(mock_env, ExceptionCheck()).WillRepeatedly(Return(JNI_FALSE));
  EXPECT_CALL(mock_env, RegisterNatives(_, _, _))
      .WillRepeatedly(
          [&](jclass clazz, const JNINativeMethod* methods, jint nMethods) {
            for (jint i = 0; i < nMethods; ++i) {
              g_registered_methods[methods[i].name] = methods[i].fnPtr;
            }
            return 0;
          });

  PlatformViewAndroid::Register(&mock_env);
}

TEST_F(PlatformViewAndroidJNIImplTest, ImageGetHardwareBufferException) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  // Call ImageGetHardwareBuffer and simulate throwing an exception.
  // Verify that it clears the exception and does not abort the process.
  EXPECT_CALL(mock_env, GetObjectRefType(_))
      .WillRepeatedly(Return(JNILocalRefType));
  EXPECT_CALL(mock_env, NewLocalRef(_)).WillRepeatedly(ReturnArg<0>());
  EXPECT_CALL(mock_env, DeleteLocalRef(_)).WillRepeatedly(Return());
  EXPECT_CALL(mock_env, CallObjectMethodV(_, _, _))
      .WillRepeatedly(Return(nullptr));
  EXPECT_CALL(mock_env, ExceptionCheck()).WillOnce(Return(JNI_TRUE));
  EXPECT_CALL(mock_env, ExceptionDescribe()).WillOnce(Return());
  EXPECT_CALL(mock_env, ExceptionClear()).Times(1).WillOnce(Return());

  fml::jni::JavaObjectWeakGlobalRef flutter_jni_object;
  PlatformViewAndroidJNIImpl android_jni(flutter_jni_object);

  fml::jni::ScopedJavaLocalRef<jobject> image(&mock_env,
                                              reinterpret_cast<jobject>(123));
  android_jni.ImageGetHardwareBuffer(image);
}

TEST_F(PlatformViewAndroidJNIImplTest, GatedAttachAndDestroy) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  auto nativeAttach = reinterpret_cast<jlong (*)(JNIEnv*, jclass, jobject)>(
      g_registered_methods["nativeAttach"]);
  auto nativeDestroy = reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
      g_registered_methods["nativeDestroy"]);

  ASSERT_NE(nativeAttach, nullptr);
  ASSERT_NE(nativeDestroy, nullptr);

  jobject flutter_jni_obj = reinterpret_cast<jobject>(0x1234);

  // Test Embedder API mode (flag = true)
  FlutterMain::SetEmbedderAPIEnabledForTesting(true);
  jlong engine_handle = nativeAttach(&mock_env, nullptr, flutter_jni_obj);
  EXPECT_NE(engine_handle, 0);
  auto* engine = reinterpret_cast<AndroidEngine*>(engine_handle);
  EXPECT_TRUE(engine->IsValid());
  nativeDestroy(&mock_env, nullptr, engine_handle);

  // Test Legacy Shell mode (flag = false)
  FlutterMain::SetEmbedderAPIEnabledForTesting(false);
  jlong shell_handle = nativeAttach(&mock_env, nullptr, flutter_jni_obj);
  EXPECT_NE(shell_handle, 0);
  auto* holder = reinterpret_cast<AndroidShellHolder*>(shell_handle);
  EXPECT_TRUE(holder->IsValid());
  nativeDestroy(&mock_env, nullptr, shell_handle);

  FlutterMain::SetEmbedderAPIEnabledForTesting(std::nullopt);
}

TEST_F(PlatformViewAndroidJNIImplTest, GatedSurfaceLifecycle) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  auto nativeAttach = reinterpret_cast<jlong (*)(JNIEnv*, jclass, jobject)>(
      g_registered_methods["nativeAttach"]);
  auto nativeDestroy = reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
      g_registered_methods["nativeDestroy"]);
  auto nativeSurfaceCreated =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong, jobject)>(
          g_registered_methods["nativeSurfaceCreated"]);
  auto nativeSurfaceWindowChanged =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong, jobject)>(
          g_registered_methods["nativeSurfaceWindowChanged"]);
  auto nativeSurfaceChanged =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong, jint, jint)>(
          g_registered_methods["nativeSurfaceChanged"]);
  auto nativeSurfaceDestroyed =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
          g_registered_methods["nativeSurfaceDestroyed"]);

  ASSERT_NE(nativeSurfaceCreated, nullptr);
  ASSERT_NE(nativeSurfaceWindowChanged, nullptr);
  ASSERT_NE(nativeSurfaceChanged, nullptr);
  ASSERT_NE(nativeSurfaceDestroyed, nullptr);

  jobject flutter_jni_obj = reinterpret_cast<jobject>(0x1234);
  jobject surface_obj = reinterpret_cast<jobject>(0x5678);

  for (bool embedder_enabled : {true, false}) {
    FlutterMain::SetEmbedderAPIEnabledForTesting(embedder_enabled);
    jlong handle = nativeAttach(&mock_env, nullptr, flutter_jni_obj);
    ASSERT_NE(handle, 0);

    nativeSurfaceCreated(&mock_env, nullptr, handle, surface_obj);
    nativeSurfaceWindowChanged(&mock_env, nullptr, handle, surface_obj);
    nativeSurfaceChanged(&mock_env, nullptr, handle, 800, 600);
    nativeSurfaceDestroyed(&mock_env, nullptr, handle);

    nativeDestroy(&mock_env, nullptr, handle);
  }

  FlutterMain::SetEmbedderAPIEnabledForTesting(std::nullopt);
}

TEST_F(PlatformViewAndroidJNIImplTest, GatedPlatformMessages) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  auto nativeAttach = reinterpret_cast<jlong (*)(JNIEnv*, jclass, jobject)>(
      g_registered_methods["nativeAttach"]);
  auto nativeDestroy = reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
      g_registered_methods["nativeDestroy"]);
  auto nativeDispatchPlatformMessage = reinterpret_cast<void (*)(
      JNIEnv*, jobject, jlong, jstring, jobject, jint, jint)>(
      g_registered_methods["nativeDispatchPlatformMessage"]);
  auto nativeDispatchEmptyPlatformMessage =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong, jstring, jint)>(
          g_registered_methods["nativeDispatchEmptyPlatformMessage"]);
  auto nativeInvokePlatformMessageResponseCallback =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong, jint, jobject, jint)>(
          g_registered_methods["nativeInvokePlatformMessageResponseCallback"]);
  auto nativeInvokePlatformMessageEmptyResponseCallback = reinterpret_cast<
      void (*)(JNIEnv*, jobject, jlong, jint)>(
      g_registered_methods["nativeInvokePlatformMessageEmptyResponseCallback"]);

  ASSERT_NE(nativeDispatchPlatformMessage, nullptr);
  ASSERT_NE(nativeDispatchEmptyPlatformMessage, nullptr);
  ASSERT_NE(nativeInvokePlatformMessageResponseCallback, nullptr);
  ASSERT_NE(nativeInvokePlatformMessageEmptyResponseCallback, nullptr);

  jobject flutter_jni_obj = reinterpret_cast<jobject>(0x1234);
  jstring channel = reinterpret_cast<jstring>(0x5678);
  std::vector<uint8_t> msg_data = {1, 2, 3, 4};
  jobject buffer = reinterpret_cast<jobject>(0x9abc);

  EXPECT_CALL(mock_env, GetDirectBufferAddress(buffer))
      .WillRepeatedly(Return(msg_data.data()));

  for (bool embedder_enabled : {true, false}) {
    FlutterMain::SetEmbedderAPIEnabledForTesting(embedder_enabled);
    jlong handle = nativeAttach(&mock_env, nullptr, flutter_jni_obj);
    ASSERT_NE(handle, 0);

    nativeDispatchPlatformMessage(&mock_env, nullptr, handle, channel, buffer,
                                  4, 1);
    nativeDispatchEmptyPlatformMessage(&mock_env, nullptr, handle, channel, 2);
    nativeInvokePlatformMessageResponseCallback(&mock_env, nullptr, handle, 1,
                                                buffer, 4);
    nativeInvokePlatformMessageEmptyResponseCallback(&mock_env, nullptr, handle,
                                                     2);

    nativeDestroy(&mock_env, nullptr, handle);
  }

  FlutterMain::SetEmbedderAPIEnabledForTesting(std::nullopt);
}

TEST_F(PlatformViewAndroidJNIImplTest, GatedPointerDataPacket) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  auto nativeAttach = reinterpret_cast<jlong (*)(JNIEnv*, jclass, jobject)>(
      g_registered_methods["nativeAttach"]);
  auto nativeDestroy = reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
      g_registered_methods["nativeDestroy"]);
  auto nativeDispatchPointerDataPacket =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong, jobject, jint)>(
          g_registered_methods["nativeDispatchPointerDataPacket"]);

  ASSERT_NE(nativeDispatchPointerDataPacket, nullptr);

  jobject flutter_jni_obj = reinterpret_cast<jobject>(0x1234);
  jobject buffer = reinterpret_cast<jobject>(0x5678);

  PointerData data = {};
  data.embedder_id = 1;
  data.change = PointerData::Change::kDown;
  data.physical_x = 100.0;
  data.physical_y = 200.0;

  EXPECT_CALL(mock_env, GetDirectBufferAddress(buffer))
      .WillRepeatedly(Return(&data));

  for (bool embedder_enabled : {true, false}) {
    FlutterMain::SetEmbedderAPIEnabledForTesting(embedder_enabled);
    jlong handle = nativeAttach(&mock_env, nullptr, flutter_jni_obj);
    ASSERT_NE(handle, 0);

    nativeDispatchPointerDataPacket(&mock_env, nullptr, handle, buffer,
                                    sizeof(data));

    nativeDestroy(&mock_env, nullptr, handle);
  }

  FlutterMain::SetEmbedderAPIEnabledForTesting(std::nullopt);
}

TEST_F(PlatformViewAndroidJNIImplTest, GatedSemanticsAndAccessibility) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  auto nativeAttach = reinterpret_cast<jlong (*)(JNIEnv*, jclass, jobject)>(
      g_registered_methods["nativeAttach"]);
  auto nativeDestroy = reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
      g_registered_methods["nativeDestroy"]);
  auto nativeDispatchSemanticsAction = reinterpret_cast<void (*)(
      JNIEnv*, jobject, jlong, jint, jint, jobject, jint)>(
      g_registered_methods["nativeDispatchSemanticsAction"]);
  auto nativeSetSemanticsEnabled =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong, jboolean)>(
          g_registered_methods["nativeSetSemanticsEnabled"]);
  auto nativeSetAccessibilityFeatures =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong, jint)>(
          g_registered_methods["nativeSetAccessibilityFeatures"]);

  ASSERT_NE(nativeDispatchSemanticsAction, nullptr);
  ASSERT_NE(nativeSetSemanticsEnabled, nullptr);
  ASSERT_NE(nativeSetAccessibilityFeatures, nullptr);

  jobject flutter_jni_obj = reinterpret_cast<jobject>(0x1234);

  for (bool embedder_enabled : {true, false}) {
    FlutterMain::SetEmbedderAPIEnabledForTesting(embedder_enabled);
    jlong handle = nativeAttach(&mock_env, nullptr, flutter_jni_obj);
    ASSERT_NE(handle, 0);

    nativeSetSemanticsEnabled(&mock_env, nullptr, handle, JNI_TRUE);
    nativeSetAccessibilityFeatures(&mock_env, nullptr, handle, 0x3);
    nativeDispatchSemanticsAction(&mock_env, nullptr, handle, 1, 1, nullptr, 0);

    nativeDestroy(&mock_env, nullptr, handle);
  }

  FlutterMain::SetEmbedderAPIEnabledForTesting(std::nullopt);
}

TEST_F(PlatformViewAndroidJNIImplTest, GatedTextures) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  auto nativeAttach = reinterpret_cast<jlong (*)(JNIEnv*, jclass, jobject)>(
      g_registered_methods["nativeAttach"]);
  auto nativeDestroy = reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
      g_registered_methods["nativeDestroy"]);
  auto nativeRegisterTexture =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong, jlong, jobject)>(
          g_registered_methods["nativeRegisterTexture"]);
  auto nativeRegisterImageTexture = reinterpret_cast<void (*)(
      JNIEnv*, jobject, jlong, jlong, jobject, jboolean)>(
      g_registered_methods["nativeRegisterImageTexture"]);
  auto nativeUnregisterTexture =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong, jlong)>(
          g_registered_methods["nativeUnregisterTexture"]);
  auto nativeMarkTextureFrameAvailable =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong, jlong)>(
          g_registered_methods["nativeMarkTextureFrameAvailable"]);

  ASSERT_NE(nativeRegisterTexture, nullptr);
  ASSERT_NE(nativeRegisterImageTexture, nullptr);
  ASSERT_NE(nativeUnregisterTexture, nullptr);
  ASSERT_NE(nativeMarkTextureFrameAvailable, nullptr);

  jobject flutter_jni_obj = reinterpret_cast<jobject>(0x1234);
  jobject texture_obj = reinterpret_cast<jobject>(0x5678);

  for (bool embedder_enabled : {true, false}) {
    FlutterMain::SetEmbedderAPIEnabledForTesting(embedder_enabled);
    jlong handle = nativeAttach(&mock_env, nullptr, flutter_jni_obj);
    ASSERT_NE(handle, 0);

    nativeRegisterTexture(&mock_env, nullptr, handle, 42, texture_obj);
    nativeRegisterImageTexture(&mock_env, nullptr, handle, 43, texture_obj,
                               JNI_TRUE);
    nativeMarkTextureFrameAvailable(&mock_env, nullptr, handle, 42);
    nativeUnregisterTexture(&mock_env, nullptr, handle, 42);
    nativeUnregisterTexture(&mock_env, nullptr, handle, 43);

    nativeDestroy(&mock_env, nullptr, handle);
  }

  FlutterMain::SetEmbedderAPIEnabledForTesting(std::nullopt);
}

TEST_F(PlatformViewAndroidJNIImplTest, GatedLowMemoryAndScheduleFrame) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  auto nativeAttach = reinterpret_cast<jlong (*)(JNIEnv*, jclass, jobject)>(
      g_registered_methods["nativeAttach"]);
  auto nativeDestroy = reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
      g_registered_methods["nativeDestroy"]);
  auto nativeNotifyLowMemoryWarning =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
          g_registered_methods["nativeNotifyLowMemoryWarning"]);
  auto nativeScheduleFrame =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
          g_registered_methods["nativeScheduleFrame"]);

  ASSERT_NE(nativeNotifyLowMemoryWarning, nullptr);
  ASSERT_NE(nativeScheduleFrame, nullptr);

  jobject flutter_jni_obj = reinterpret_cast<jobject>(0x1234);

  for (bool embedder_enabled : {true, false}) {
    FlutterMain::SetEmbedderAPIEnabledForTesting(embedder_enabled);
    jlong handle = nativeAttach(&mock_env, nullptr, flutter_jni_obj);
    ASSERT_NE(handle, 0);

    nativeNotifyLowMemoryWarning(&mock_env, nullptr, handle);
    nativeScheduleFrame(&mock_env, nullptr, handle);

    nativeDestroy(&mock_env, nullptr, handle);
  }

  FlutterMain::SetEmbedderAPIEnabledForTesting(std::nullopt);
}

TEST_F(PlatformViewAndroidJNIImplTest, GatedSetViewportMetrics) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  auto nativeAttach = reinterpret_cast<jlong (*)(JNIEnv*, jclass, jobject)>(
      g_registered_methods["nativeAttach"]);
  auto nativeDestroy = reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
      g_registered_methods["nativeDestroy"]);
  typedef void (*SetViewportMetricsFn)(
      JNIEnv*, jobject, jlong, jfloat, jint, jint, jint, jint, jint, jint, jint,
      jint, jint, jint, jint, jint, jint, jint, jint, jintArray, jintArray,
      jintArray, jint, jint, jint, jint, jint, jint, jint, jint);

  auto nativeSetViewportMetrics = reinterpret_cast<SetViewportMetricsFn>(
      g_registered_methods["nativeSetViewportMetrics"]);
  ASSERT_NE(nativeSetViewportMetrics, nullptr);

  EXPECT_CALL(mock_env, GetArrayLength(_)).WillRepeatedly(Return(0));

  jobject flutter_jni_obj = reinterpret_cast<jobject>(0x1234);
  jintArray bounds = reinterpret_cast<jintArray>(456);
  jintArray type = reinterpret_cast<jintArray>(789);
  jintArray state = reinterpret_cast<jintArray>(1011);

  for (bool embedder_enabled : {true, false}) {
    FlutterMain::SetEmbedderAPIEnabledForTesting(embedder_enabled);
    jlong handle = nativeAttach(&mock_env, nullptr, flutter_jni_obj);
    ASSERT_NE(handle, 0);

    nativeSetViewportMetrics(&mock_env, nullptr, handle, 2.0f, 1080, 1920, 0, 0,
                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, bounds, type,
                             state, 0, 0, 0, 0, 0, 0, 0, 0);

    nativeDestroy(&mock_env, nullptr, handle);
  }

  FlutterMain::SetEmbedderAPIEnabledForTesting(std::nullopt);
}

struct PlatformViewAndroidJNIMatrixParam {
  bool embedder_api_enabled;
  AndroidRenderingAPI rendering_api;
  const char* name;
};

class PlatformViewAndroidJNIParameterizedTest
    : public PlatformViewAndroidJNIImplTest,
      public ::testing::WithParamInterface<PlatformViewAndroidJNIMatrixParam> {
 protected:
  void SetUp() override {
    const auto& param = GetParam();
    FlutterMain::SetEmbedderAPIEnabledForTesting(param.embedder_api_enabled);
    Settings settings;
    settings.enable_software_rendering =
        (param.rendering_api == AndroidRenderingAPI::kSoftware);
    FlutterMain::InitForTesting(settings, param.rendering_api);
  }

  void TearDown() override { FlutterMain::ResetForTesting(); }
};

TEST_P(PlatformViewAndroidJNIParameterizedTest, MatrixLifecycle) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  auto nativeAttach = reinterpret_cast<jlong (*)(JNIEnv*, jclass, jobject)>(
      g_registered_methods["nativeAttach"]);
  auto nativeDestroy = reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
      g_registered_methods["nativeDestroy"]);
  auto nativeSurfaceCreated =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong, jobject)>(
          g_registered_methods["nativeSurfaceCreated"]);
  auto nativeSurfaceWindowChanged =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong, jobject)>(
          g_registered_methods["nativeSurfaceWindowChanged"]);
  auto nativeSurfaceChanged =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong, jint, jint)>(
          g_registered_methods["nativeSurfaceChanged"]);
  auto nativeSurfaceDestroyed =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
          g_registered_methods["nativeSurfaceDestroyed"]);

  ASSERT_NE(nativeAttach, nullptr);
  ASSERT_NE(nativeDestroy, nullptr);
  ASSERT_NE(nativeSurfaceCreated, nullptr);
  ASSERT_NE(nativeSurfaceWindowChanged, nullptr);
  ASSERT_NE(nativeSurfaceChanged, nullptr);
  ASSERT_NE(nativeSurfaceDestroyed, nullptr);

  jobject flutter_jni_obj = reinterpret_cast<jobject>(0x1234);
  jobject surface_obj = reinterpret_cast<jobject>(0x5678);

  jlong handle = nativeAttach(&mock_env, nullptr, flutter_jni_obj);
  ASSERT_NE(handle, 0);

  nativeSurfaceCreated(&mock_env, nullptr, handle, surface_obj);
  nativeSurfaceWindowChanged(&mock_env, nullptr, handle, surface_obj);
  nativeSurfaceChanged(&mock_env, nullptr, handle, 1080, 1920);
  nativeSurfaceDestroyed(&mock_env, nullptr, handle);

  nativeDestroy(&mock_env, nullptr, handle);
}

TEST_P(PlatformViewAndroidJNIParameterizedTest, MatrixPlatformMessages) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  auto nativeAttach = reinterpret_cast<jlong (*)(JNIEnv*, jclass, jobject)>(
      g_registered_methods["nativeAttach"]);
  auto nativeDestroy = reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
      g_registered_methods["nativeDestroy"]);
  auto nativeDispatchPlatformMessage = reinterpret_cast<void (*)(
      JNIEnv*, jobject, jlong, jstring, jobject, jint, jint)>(
      g_registered_methods["nativeDispatchPlatformMessage"]);
  auto nativeDispatchEmptyPlatformMessage =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong, jstring, jint)>(
          g_registered_methods["nativeDispatchEmptyPlatformMessage"]);
  auto nativeInvokePlatformMessageResponseCallback =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong, jint, jobject, jint)>(
          g_registered_methods["nativeInvokePlatformMessageResponseCallback"]);
  auto nativeInvokePlatformMessageEmptyResponseCallback = reinterpret_cast<
      void (*)(JNIEnv*, jobject, jlong, jint)>(
      g_registered_methods["nativeInvokePlatformMessageEmptyResponseCallback"]);

  ASSERT_NE(nativeDispatchPlatformMessage, nullptr);
  ASSERT_NE(nativeDispatchEmptyPlatformMessage, nullptr);
  ASSERT_NE(nativeInvokePlatformMessageResponseCallback, nullptr);
  ASSERT_NE(nativeInvokePlatformMessageEmptyResponseCallback, nullptr);

  jobject flutter_jni_obj = reinterpret_cast<jobject>(0x1234);
  jstring channel = reinterpret_cast<jstring>(0x5678);
  std::vector<uint8_t> msg_data = {1, 2, 3, 4};
  jobject buffer = reinterpret_cast<jobject>(0x9abc);

  EXPECT_CALL(mock_env, GetDirectBufferAddress(buffer))
      .WillRepeatedly(Return(msg_data.data()));

  jlong handle = nativeAttach(&mock_env, nullptr, flutter_jni_obj);
  ASSERT_NE(handle, 0);

  nativeDispatchPlatformMessage(&mock_env, nullptr, handle, channel, buffer, 4,
                                1);
  nativeDispatchEmptyPlatformMessage(&mock_env, nullptr, handle, channel, 2);
  nativeInvokePlatformMessageResponseCallback(&mock_env, nullptr, handle, 1,
                                              buffer, 4);
  nativeInvokePlatformMessageEmptyResponseCallback(&mock_env, nullptr, handle,
                                                   2);

  nativeDestroy(&mock_env, nullptr, handle);
}

TEST_P(PlatformViewAndroidJNIParameterizedTest, MatrixPointerDataPacket) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  auto nativeAttach = reinterpret_cast<jlong (*)(JNIEnv*, jclass, jobject)>(
      g_registered_methods["nativeAttach"]);
  auto nativeDestroy = reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
      g_registered_methods["nativeDestroy"]);
  auto nativeDispatchPointerDataPacket =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong, jobject, jint)>(
          g_registered_methods["nativeDispatchPointerDataPacket"]);

  ASSERT_NE(nativeDispatchPointerDataPacket, nullptr);

  jobject flutter_jni_obj = reinterpret_cast<jobject>(0x1234);
  jobject buffer = reinterpret_cast<jobject>(0x5678);

  PointerData data = {};
  data.embedder_id = 1;
  data.change = PointerData::Change::kDown;
  data.physical_x = 100.0;
  data.physical_y = 200.0;

  EXPECT_CALL(mock_env, GetDirectBufferAddress(buffer))
      .WillRepeatedly(Return(&data));

  jlong handle = nativeAttach(&mock_env, nullptr, flutter_jni_obj);
  ASSERT_NE(handle, 0);

  nativeDispatchPointerDataPacket(&mock_env, nullptr, handle, buffer,
                                  sizeof(data));

  nativeDestroy(&mock_env, nullptr, handle);
}

TEST_P(PlatformViewAndroidJNIParameterizedTest,
       MatrixSemanticsAndAccessibility) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  auto nativeAttach = reinterpret_cast<jlong (*)(JNIEnv*, jclass, jobject)>(
      g_registered_methods["nativeAttach"]);
  auto nativeDestroy = reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
      g_registered_methods["nativeDestroy"]);
  auto nativeDispatchSemanticsAction = reinterpret_cast<void (*)(
      JNIEnv*, jobject, jlong, jint, jint, jobject, jint)>(
      g_registered_methods["nativeDispatchSemanticsAction"]);
  auto nativeSetSemanticsEnabled =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong, jboolean)>(
          g_registered_methods["nativeSetSemanticsEnabled"]);
  auto nativeSetAccessibilityFeatures =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong, jint)>(
          g_registered_methods["nativeSetAccessibilityFeatures"]);

  ASSERT_NE(nativeDispatchSemanticsAction, nullptr);
  ASSERT_NE(nativeSetSemanticsEnabled, nullptr);
  ASSERT_NE(nativeSetAccessibilityFeatures, nullptr);

  jobject flutter_jni_obj = reinterpret_cast<jobject>(0x1234);

  jlong handle = nativeAttach(&mock_env, nullptr, flutter_jni_obj);
  ASSERT_NE(handle, 0);

  nativeSetSemanticsEnabled(&mock_env, nullptr, handle, JNI_TRUE);
  nativeSetAccessibilityFeatures(&mock_env, nullptr, handle, 0x3);
  nativeDispatchSemanticsAction(&mock_env, nullptr, handle, 1, 1, nullptr, 0);

  nativeDestroy(&mock_env, nullptr, handle);
}

TEST_P(PlatformViewAndroidJNIParameterizedTest, MatrixTextures) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  auto nativeAttach = reinterpret_cast<jlong (*)(JNIEnv*, jclass, jobject)>(
      g_registered_methods["nativeAttach"]);
  auto nativeDestroy = reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
      g_registered_methods["nativeDestroy"]);
  auto nativeRegisterTexture =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong, jlong, jobject)>(
          g_registered_methods["nativeRegisterTexture"]);
  auto nativeRegisterImageTexture = reinterpret_cast<void (*)(
      JNIEnv*, jobject, jlong, jlong, jobject, jboolean)>(
      g_registered_methods["nativeRegisterImageTexture"]);
  auto nativeUnregisterTexture =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong, jlong)>(
          g_registered_methods["nativeUnregisterTexture"]);
  auto nativeMarkTextureFrameAvailable =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong, jlong)>(
          g_registered_methods["nativeMarkTextureFrameAvailable"]);

  ASSERT_NE(nativeRegisterTexture, nullptr);
  ASSERT_NE(nativeRegisterImageTexture, nullptr);
  ASSERT_NE(nativeUnregisterTexture, nullptr);
  ASSERT_NE(nativeMarkTextureFrameAvailable, nullptr);

  jobject flutter_jni_obj = reinterpret_cast<jobject>(0x1234);
  jobject texture_obj = reinterpret_cast<jobject>(0x5678);

  jlong handle = nativeAttach(&mock_env, nullptr, flutter_jni_obj);
  ASSERT_NE(handle, 0);

  nativeRegisterTexture(&mock_env, nullptr, handle, 42, texture_obj);
  nativeRegisterImageTexture(&mock_env, nullptr, handle, 43, texture_obj,
                             JNI_TRUE);
  nativeMarkTextureFrameAvailable(&mock_env, nullptr, handle, 42);
  nativeUnregisterTexture(&mock_env, nullptr, handle, 42);
  nativeUnregisterTexture(&mock_env, nullptr, handle, 43);

  nativeDestroy(&mock_env, nullptr, handle);
}

TEST_P(PlatformViewAndroidJNIParameterizedTest, MatrixViewportMetrics) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  auto nativeAttach = reinterpret_cast<jlong (*)(JNIEnv*, jclass, jobject)>(
      g_registered_methods["nativeAttach"]);
  auto nativeDestroy = reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
      g_registered_methods["nativeDestroy"]);
  typedef void (*SetViewportMetricsFn)(
      JNIEnv*, jobject, jlong, jfloat, jint, jint, jint, jint, jint, jint, jint,
      jint, jint, jint, jint, jint, jint, jint, jint, jintArray, jintArray,
      jintArray, jint, jint, jint, jint, jint, jint, jint, jint);

  auto nativeSetViewportMetrics = reinterpret_cast<SetViewportMetricsFn>(
      g_registered_methods["nativeSetViewportMetrics"]);
  ASSERT_NE(nativeSetViewportMetrics, nullptr);

  EXPECT_CALL(mock_env, GetArrayLength(_)).WillRepeatedly(Return(0));

  jobject flutter_jni_obj = reinterpret_cast<jobject>(0x1234);
  jintArray bounds = reinterpret_cast<jintArray>(456);
  jintArray type = reinterpret_cast<jintArray>(789);
  jintArray state = reinterpret_cast<jintArray>(1011);

  jlong handle = nativeAttach(&mock_env, nullptr, flutter_jni_obj);
  ASSERT_NE(handle, 0);

  nativeSetViewportMetrics(&mock_env, nullptr, handle, 2.0f, 1080, 1920, 0, 0,
                           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, bounds, type, state,
                           0, 0, 0, 0, 0, 0, 0, 0);

  nativeDestroy(&mock_env, nullptr, handle);
}

TEST_P(PlatformViewAndroidJNIParameterizedTest,
       MatrixLowMemoryAndScheduleFrame) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  auto nativeAttach = reinterpret_cast<jlong (*)(JNIEnv*, jclass, jobject)>(
      g_registered_methods["nativeAttach"]);
  auto nativeDestroy = reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
      g_registered_methods["nativeDestroy"]);
  auto nativeNotifyLowMemoryWarning =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
          g_registered_methods["nativeNotifyLowMemoryWarning"]);
  auto nativeScheduleFrame =
      reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
          g_registered_methods["nativeScheduleFrame"]);

  ASSERT_NE(nativeNotifyLowMemoryWarning, nullptr);
  ASSERT_NE(nativeScheduleFrame, nullptr);

  jobject flutter_jni_obj = reinterpret_cast<jobject>(0x1234);

  jlong handle = nativeAttach(&mock_env, nullptr, flutter_jni_obj);
  ASSERT_NE(handle, 0);

  nativeNotifyLowMemoryWarning(&mock_env, nullptr, handle);
  nativeScheduleFrame(&mock_env, nullptr, handle);

  nativeDestroy(&mock_env, nullptr, handle);
}

TEST_P(PlatformViewAndroidJNIParameterizedTest, MatrixDeferredLibraryLoading) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  auto nativeAttach = reinterpret_cast<jlong (*)(JNIEnv*, jclass, jobject)>(
      g_registered_methods["nativeAttach"]);
  auto nativeDestroy = reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
      g_registered_methods["nativeDestroy"]);
  typedef void (*LoadDartDeferredLibraryFn)(JNIEnv*, jobject, jlong, jint,
                                            jobjectArray);
  auto nativeLoadDartDeferredLibrary =
      reinterpret_cast<LoadDartDeferredLibraryFn>(
          g_registered_methods["nativeLoadDartDeferredLibrary"]);

  ASSERT_NE(nativeAttach, nullptr);
  ASSERT_NE(nativeDestroy, nullptr);
  ASSERT_NE(nativeLoadDartDeferredLibrary, nullptr);

  jobject flutter_jni_obj = reinterpret_cast<jobject>(0x1234);
  jobjectArray search_paths = reinterpret_cast<jobjectArray>(0x5678);

  EXPECT_CALL(mock_env, GetArrayLength(search_paths)).WillRepeatedly(Return(0));

  jlong handle = nativeAttach(&mock_env, nullptr, flutter_jni_obj);
  ASSERT_NE(handle, 0);

  nativeLoadDartDeferredLibrary(&mock_env, nullptr, handle, 42, search_paths);

  nativeDestroy(&mock_env, nullptr, handle);
}

TEST_P(PlatformViewAndroidJNIParameterizedTest, MatrixSpawning) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  auto nativeAttach = reinterpret_cast<jlong (*)(JNIEnv*, jclass, jobject)>(
      g_registered_methods["nativeAttach"]);
  auto nativeDestroy = reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
      g_registered_methods["nativeDestroy"]);
  typedef jobject (*SpawnFn)(JNIEnv*, jobject, jlong, jstring, jstring, jstring,
                             jobject, jlong);
  auto nativeSpawn =
      reinterpret_cast<SpawnFn>(g_registered_methods["nativeSpawn"]);

  ASSERT_NE(nativeAttach, nullptr);
  ASSERT_NE(nativeDestroy, nullptr);
  ASSERT_NE(nativeSpawn, nullptr);

  jobject flutter_jni_obj = reinterpret_cast<jobject>(0x1234);
  jstring entrypoint = reinterpret_cast<jstring>(0x5678);

  jlong handle = nativeAttach(&mock_env, nullptr, flutter_jni_obj);
  ASSERT_NE(handle, 0);

  nativeSpawn(&mock_env, nullptr, handle, entrypoint, nullptr, nullptr, nullptr,
              0);

  nativeDestroy(&mock_env, nullptr, handle);
}

TEST_P(PlatformViewAndroidJNIParameterizedTest, MatrixGetBitmap) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  auto nativeAttach = reinterpret_cast<jlong (*)(JNIEnv*, jclass, jobject)>(
      g_registered_methods["nativeAttach"]);
  auto nativeDestroy = reinterpret_cast<void (*)(JNIEnv*, jobject, jlong)>(
      g_registered_methods["nativeDestroy"]);
  typedef jobject (*GetBitmapFn)(JNIEnv*, jobject, jlong);
  auto nativeGetBitmap =
      reinterpret_cast<GetBitmapFn>(g_registered_methods["nativeGetBitmap"]);

  ASSERT_NE(nativeAttach, nullptr);
  ASSERT_NE(nativeDestroy, nullptr);
  ASSERT_NE(nativeGetBitmap, nullptr);

  jobject flutter_jni_obj = reinterpret_cast<jobject>(0x1234);

  jlong handle = nativeAttach(&mock_env, nullptr, flutter_jni_obj);
  ASSERT_NE(handle, 0);

  nativeGetBitmap(&mock_env, nullptr, handle);

  nativeDestroy(&mock_env, nullptr, handle);
}

INSTANTIATE_TEST_SUITE_P(
    PlatformViewAndroidJNIMatrix,
    PlatformViewAndroidJNIParameterizedTest,
    ::testing::Values(
        PlatformViewAndroidJNIMatrixParam{
            true, AndroidRenderingAPI::kImpellerOpenGLES,
            "Embedder_ImpellerOpenGLES"},
        PlatformViewAndroidJNIMatrixParam{true,
                                          AndroidRenderingAPI::kImpellerVulkan,
                                          "Embedder_ImpellerVulkan"},
        PlatformViewAndroidJNIMatrixParam{
            true, AndroidRenderingAPI::kSkiaOpenGLES, "Embedder_SkiaOpenGLES"},
        PlatformViewAndroidJNIMatrixParam{
            true, AndroidRenderingAPI::kImpellerAutoselect,
            "Embedder_ImpellerAutoselect"},
        PlatformViewAndroidJNIMatrixParam{true, AndroidRenderingAPI::kSoftware,
                                          "Embedder_Software"},
        PlatformViewAndroidJNIMatrixParam{
            false, AndroidRenderingAPI::kImpellerOpenGLES,
            "Legacy_ImpellerOpenGLES"},
        PlatformViewAndroidJNIMatrixParam{false,
                                          AndroidRenderingAPI::kImpellerVulkan,
                                          "Legacy_ImpellerVulkan"},
        PlatformViewAndroidJNIMatrixParam{
            false, AndroidRenderingAPI::kSkiaOpenGLES, "Legacy_SkiaOpenGLES"},
        PlatformViewAndroidJNIMatrixParam{
            false, AndroidRenderingAPI::kImpellerAutoselect,
            "Legacy_ImpellerAutoselect"},
        PlatformViewAndroidJNIMatrixParam{false, AndroidRenderingAPI::kSoftware,
                                          "Legacy_Software"}),
    [](const ::testing::TestParamInfo<PlatformViewAndroidJNIMatrixParam>&
           info) { return info.param.name; });

// The load order is exercised with an injected loader rather than real
// dlopen(): the property under test is purely the ordering (first-to-last,
// stop at the first that loads), and a fake loader makes that deterministic
// and free of any platform- or system-library-specific behavior. Whether a
// given path format is actually loadable is covered end-to-end by
// dev/integration_tests/deferred_components_test.
TEST(FindFirstLoadableLibraryTest, TriesInOrderAndStopsAtFirstSuccess) {
  std::vector<std::string> attempted;
  void* const handle = reinterpret_cast<void*>(0x1234);
  auto opener = [&](const std::string& path) -> void* {
    attempted.push_back(path);
    return path == "b" ? handle : nullptr;
  };
  EXPECT_EQ(FindFirstLoadableLibrary({"a", "b", "c"}, opener), handle);
  // "c" is never attempted because "b" already loaded.
  EXPECT_THAT(attempted, ElementsAre("a", "b"));
}

TEST(FindFirstLoadableLibraryTest, TriesAllAndReturnsNullWhenNoneLoad) {
  std::vector<std::string> attempted;
  auto opener = [&](const std::string& path) -> void* {
    attempted.push_back(path);
    return nullptr;
  };
  EXPECT_EQ(FindFirstLoadableLibrary({"a", "b"}, opener), nullptr);
  EXPECT_THAT(attempted, ElementsAre("a", "b"));
}

TEST(FindFirstLoadableLibraryTest, EmptyReturnsNull) {
  EXPECT_EQ(FindFirstLoadableLibrary(
                {}, [](const std::string&) -> void* { return nullptr; }),
            nullptr);
}

}  // namespace testing
}  // namespace flutter
