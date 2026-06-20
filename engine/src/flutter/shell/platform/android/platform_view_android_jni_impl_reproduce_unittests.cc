// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "flutter/fml/platform/android/jni_util.h"
#include "flutter/fml/platform/android/jni_weak_ref.h"
#include "flutter/fml/platform/android/scoped_java_ref.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "flutter/shell/platform/android/jni/mock_jni_env.h"
#include "flutter/shell/platform/android/platform_view_android.h"
#include "flutter/shell/platform/android/platform_view_android_jni_impl.h"

namespace flutter {
namespace testing {

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnArg;

class PlatformViewAndroidJNIImplReproduceTest : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    static std::once_flag jvm_init_flag;
    std::call_once(jvm_init_flag, SetUpJVM);
  }

 private:
  friend class MockJNIEnvProvider;
  static MockJavaVM jvm_;
  static void SetUpJVM();
};

MockJavaVM PlatformViewAndroidJNIImplReproduceTest::jvm_;

class MockJNIEnvProvider {
 public:
  MockJNIEnvProvider() {
    PlatformViewAndroidJNIImplReproduceTest::jvm_.SetJNIEnv(&env_);
  }
  ~MockJNIEnvProvider() {
    PlatformViewAndroidJNIImplReproduceTest::jvm_.SetJNIEnv(nullptr);
  }
  MockJNIEnv& env() { return env_; }

 private:
  MockJNIEnv env_;
};

void PlatformViewAndroidJNIImplReproduceTest::SetUpJVM() {
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
  EXPECT_CALL(mock_env, RegisterNatives(_, _, _)).WillRepeatedly(Return(0));

  PlatformViewAndroid::Register(&mock_env);
}

TEST_F(PlatformViewAndroidJNIImplReproduceTest,
       FlutterViewOnDisplayPlatformViewException) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  jobject mock_object = reinterpret_cast<jobject>(0x1234);
  jweak mock_weak_ref = reinterpret_cast<jweak>(0x5678);

  // Set up mock weak reference lifecycle.
  EXPECT_CALL(mock_env, NewWeakGlobalRef(_))
      .WillRepeatedly(Return(mock_weak_ref));
  EXPECT_CALL(mock_env, DeleteWeakGlobalRef(mock_weak_ref))
      .WillRepeatedly(Return());

  // Construct non-empty JavaObjectWeakGlobalRef.
  fml::jni::JavaObjectWeakGlobalRef flutter_jni_object(&mock_env, mock_object);
  PlatformViewAndroidJNIImpl android_jni(flutter_jni_object);

  // Set up expectations for the method execution.
  const jclass kPlaceholderClass = reinterpret_cast<jclass>(100);
  const jmethodID kPlaceholderMethodID = reinterpret_cast<jmethodID>(300);
  const jobject kPlaceholderObject = reinterpret_cast<jobject>(400);

  EXPECT_CALL(mock_env, FindClass(_)).WillRepeatedly(Return(kPlaceholderClass));
  EXPECT_CALL(mock_env, GetMethodID(_, _, _))
      .WillRepeatedly(Return(kPlaceholderMethodID));
  EXPECT_CALL(mock_env, CallObjectMethodV(_, _, _))
      .WillRepeatedly(Return(kPlaceholderObject));

  EXPECT_CALL(mock_env, GetObjectRefType(_))
      .WillRepeatedly(Return(JNILocalRefType));
  EXPECT_CALL(mock_env, NewLocalRef(mock_weak_ref))
      .WillRepeatedly(Return(mock_object));
  EXPECT_CALL(mock_env, NewLocalRef(mock_object))
      .WillRepeatedly(ReturnArg<0>());
  EXPECT_CALL(mock_env, DeleteLocalRef(_)).WillRepeatedly(Return());
  EXPECT_CALL(mock_env, NewObjectV(_, _, _))
      .WillRepeatedly(Return(reinterpret_cast<jobject>(456)));

  // Mock CallVoidMethodV to do nothing, but simulate JNI exception.
  EXPECT_CALL(mock_env, CallVoidMethodV(_, _, _)).WillRepeatedly(Return());
  EXPECT_CALL(mock_env, ExceptionCheck()).WillRepeatedly(Return(JNI_TRUE));
  EXPECT_CALL(mock_env, ExceptionOccurred())
      .WillRepeatedly(Return(reinterpret_cast<jthrowable>(789)));
  EXPECT_CALL(mock_env, ExceptionClear()).WillRepeatedly(Return());

  MutatorsStack mutators_stack;
  android_jni.FlutterViewOnDisplayPlatformView(1, 0, 0, 100, 100, 100, 100,
                                               mutators_stack);
}

TEST_F(PlatformViewAndroidJNIImplReproduceTest,
       OnDisplayPlatformView2Exception) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  jobject mock_object = reinterpret_cast<jobject>(0x1234);
  jweak mock_weak_ref = reinterpret_cast<jweak>(0x5678);

  EXPECT_CALL(mock_env, NewWeakGlobalRef(_))
      .WillRepeatedly(Return(mock_weak_ref));
  EXPECT_CALL(mock_env, DeleteWeakGlobalRef(mock_weak_ref))
      .WillRepeatedly(Return());

  fml::jni::JavaObjectWeakGlobalRef flutter_jni_object(&mock_env, mock_object);
  PlatformViewAndroidJNIImpl android_jni(flutter_jni_object);

  const jclass kPlaceholderClass = reinterpret_cast<jclass>(100);
  const jmethodID kPlaceholderMethodID = reinterpret_cast<jmethodID>(300);
  const jobject kPlaceholderObject = reinterpret_cast<jobject>(400);

  EXPECT_CALL(mock_env, FindClass(_)).WillRepeatedly(Return(kPlaceholderClass));
  EXPECT_CALL(mock_env, GetMethodID(_, _, _))
      .WillRepeatedly(Return(kPlaceholderMethodID));
  EXPECT_CALL(mock_env, CallObjectMethodV(_, _, _))
      .WillRepeatedly(Return(kPlaceholderObject));

  EXPECT_CALL(mock_env, GetObjectRefType(_))
      .WillRepeatedly(Return(JNILocalRefType));
  EXPECT_CALL(mock_env, NewLocalRef(mock_weak_ref))
      .WillRepeatedly(Return(mock_object));
  EXPECT_CALL(mock_env, NewLocalRef(mock_object))
      .WillRepeatedly(ReturnArg<0>());
  EXPECT_CALL(mock_env, DeleteLocalRef(_)).WillRepeatedly(Return());
  EXPECT_CALL(mock_env, NewObjectV(_, _, _))
      .WillRepeatedly(Return(reinterpret_cast<jobject>(456)));

  EXPECT_CALL(mock_env, CallVoidMethodV(_, _, _)).WillRepeatedly(Return());
  EXPECT_CALL(mock_env, ExceptionCheck()).WillRepeatedly(Return(JNI_TRUE));
  EXPECT_CALL(mock_env, ExceptionOccurred())
      .WillRepeatedly(Return(reinterpret_cast<jthrowable>(789)));
  EXPECT_CALL(mock_env, ExceptionClear()).WillRepeatedly(Return());

  MutatorsStack mutators_stack;
  android_jni.onDisplayPlatformView2(1, 0, 0, 100, 100, 100, 100,
                                     mutators_stack);
}

TEST_F(PlatformViewAndroidJNIImplReproduceTest,
       FlutterViewDisplayOverlaySurfaceException) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  jobject mock_object = reinterpret_cast<jobject>(0x1234);
  jweak mock_weak_ref = reinterpret_cast<jweak>(0x5678);

  EXPECT_CALL(mock_env, NewWeakGlobalRef(_))
      .WillRepeatedly(Return(mock_weak_ref));
  EXPECT_CALL(mock_env, DeleteWeakGlobalRef(mock_weak_ref))
      .WillRepeatedly(Return());

  fml::jni::JavaObjectWeakGlobalRef flutter_jni_object(&mock_env, mock_object);
  PlatformViewAndroidJNIImpl android_jni(flutter_jni_object);

  const jclass kPlaceholderClass = reinterpret_cast<jclass>(100);
  const jmethodID kPlaceholderMethodID = reinterpret_cast<jmethodID>(300);
  const jobject kPlaceholderObject = reinterpret_cast<jobject>(400);

  EXPECT_CALL(mock_env, FindClass(_)).WillRepeatedly(Return(kPlaceholderClass));
  EXPECT_CALL(mock_env, GetMethodID(_, _, _))
      .WillRepeatedly(Return(kPlaceholderMethodID));
  EXPECT_CALL(mock_env, CallObjectMethodV(_, _, _))
      .WillRepeatedly(Return(kPlaceholderObject));

  EXPECT_CALL(mock_env, GetObjectRefType(_))
      .WillRepeatedly(Return(JNILocalRefType));
  EXPECT_CALL(mock_env, NewLocalRef(mock_weak_ref))
      .WillRepeatedly(Return(mock_object));
  EXPECT_CALL(mock_env, NewLocalRef(mock_object))
      .WillRepeatedly(ReturnArg<0>());
  EXPECT_CALL(mock_env, DeleteLocalRef(_)).WillRepeatedly(Return());

  EXPECT_CALL(mock_env, CallVoidMethodV(_, _, _)).WillRepeatedly(Return());
  EXPECT_CALL(mock_env, ExceptionCheck()).WillRepeatedly(Return(JNI_TRUE));
  EXPECT_CALL(mock_env, ExceptionOccurred())
      .WillRepeatedly(Return(reinterpret_cast<jthrowable>(789)));
  EXPECT_CALL(mock_env, ExceptionClear()).WillRepeatedly(Return());

  android_jni.FlutterViewDisplayOverlaySurface(1, 0, 0, 100, 100);
}

TEST_F(PlatformViewAndroidJNIImplReproduceTest, CreateOverlaySurfaceException) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  jobject mock_object = reinterpret_cast<jobject>(0x1234);
  jweak mock_weak_ref = reinterpret_cast<jweak>(0x5678);

  EXPECT_CALL(mock_env, NewWeakGlobalRef(_))
      .WillRepeatedly(Return(mock_weak_ref));
  EXPECT_CALL(mock_env, DeleteWeakGlobalRef(mock_weak_ref))
      .WillRepeatedly(Return());

  fml::jni::JavaObjectWeakGlobalRef flutter_jni_object(&mock_env, mock_object);
  PlatformViewAndroidJNIImpl android_jni(flutter_jni_object);

  const jclass kPlaceholderClass = reinterpret_cast<jclass>(100);
  const jmethodID kPlaceholderMethodID = reinterpret_cast<jmethodID>(300);

  EXPECT_CALL(mock_env, FindClass(_)).WillRepeatedly(Return(kPlaceholderClass));
  EXPECT_CALL(mock_env, GetMethodID(_, _, _))
      .WillRepeatedly(Return(kPlaceholderMethodID));
  EXPECT_CALL(mock_env, CallObjectMethodV(_, _, _))
      .WillRepeatedly(Return(nullptr));

  EXPECT_CALL(mock_env, GetObjectRefType(_))
      .WillRepeatedly(Return(JNILocalRefType));
  EXPECT_CALL(mock_env, NewLocalRef(mock_weak_ref))
      .WillRepeatedly(Return(mock_object));
  EXPECT_CALL(mock_env, NewLocalRef(mock_object))
      .WillRepeatedly(ReturnArg<0>());
  EXPECT_CALL(mock_env, DeleteLocalRef(_)).WillRepeatedly(Return());

  EXPECT_CALL(mock_env, ExceptionCheck()).WillRepeatedly(Return(JNI_TRUE));
  EXPECT_CALL(mock_env, ExceptionOccurred())
      .WillRepeatedly(Return(reinterpret_cast<jthrowable>(789)));
  EXPECT_CALL(mock_env, ExceptionClear()).WillRepeatedly(Return());

  auto metadata = android_jni.FlutterViewCreateOverlaySurface();
  ASSERT_NE(metadata, nullptr);
  EXPECT_EQ(metadata->id, 0);
  EXPECT_EQ(metadata->window.get(), nullptr);
}

TEST_F(PlatformViewAndroidJNIImplReproduceTest,
       CreateOverlaySurface2Exception) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  jobject mock_object = reinterpret_cast<jobject>(0x1234);
  jweak mock_weak_ref = reinterpret_cast<jweak>(0x5678);

  EXPECT_CALL(mock_env, NewWeakGlobalRef(_))
      .WillRepeatedly(Return(mock_weak_ref));
  EXPECT_CALL(mock_env, DeleteWeakGlobalRef(mock_weak_ref))
      .WillRepeatedly(Return());

  fml::jni::JavaObjectWeakGlobalRef flutter_jni_object(&mock_env, mock_object);
  PlatformViewAndroidJNIImpl android_jni(flutter_jni_object);

  const jclass kPlaceholderClass = reinterpret_cast<jclass>(100);
  const jmethodID kPlaceholderMethodID = reinterpret_cast<jmethodID>(300);

  EXPECT_CALL(mock_env, FindClass(_)).WillRepeatedly(Return(kPlaceholderClass));
  EXPECT_CALL(mock_env, GetMethodID(_, _, _))
      .WillRepeatedly(Return(kPlaceholderMethodID));
  EXPECT_CALL(mock_env, CallObjectMethodV(_, _, _))
      .WillRepeatedly(Return(nullptr));

  EXPECT_CALL(mock_env, GetObjectRefType(_))
      .WillRepeatedly(Return(JNILocalRefType));
  EXPECT_CALL(mock_env, NewLocalRef(mock_weak_ref))
      .WillRepeatedly(Return(mock_object));
  EXPECT_CALL(mock_env, NewLocalRef(mock_object))
      .WillRepeatedly(ReturnArg<0>());
  EXPECT_CALL(mock_env, DeleteLocalRef(_)).WillRepeatedly(Return());

  EXPECT_CALL(mock_env, ExceptionCheck()).WillRepeatedly(Return(JNI_TRUE));
  EXPECT_CALL(mock_env, ExceptionOccurred())
      .WillRepeatedly(Return(reinterpret_cast<jthrowable>(789)));
  EXPECT_CALL(mock_env, ExceptionClear()).WillRepeatedly(Return());

  auto metadata = android_jni.createOverlaySurface2();
  ASSERT_NE(metadata, nullptr);
  EXPECT_EQ(metadata->id, 0);
  EXPECT_EQ(metadata->window.get(), nullptr);
}

}  // namespace testing
}  // namespace flutter
