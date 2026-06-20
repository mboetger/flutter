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
  friend class MockJNIEnvProvider2;
  static MockJavaVM jvm_;
  static void SetUpJVM();
};

MockJavaVM PlatformViewAndroidJNIImplReproduceTest::jvm_;

class MockJNIEnvProvider2 {
 public:
  MockJNIEnvProvider2() {
    PlatformViewAndroidJNIImplReproduceTest::jvm_.SetJNIEnv(&env_);
  }
  ~MockJNIEnvProvider2() {
    PlatformViewAndroidJNIImplReproduceTest::jvm_.SetJNIEnv(nullptr);
  }
  MockJNIEnv& env() { return env_; }

 private:
  MockJNIEnv env_;
};

void PlatformViewAndroidJNIImplReproduceTest::SetUpJVM() {
  fml::jni::InitJavaVM(&jvm_);

  MockJNIEnvProvider2 env_provider;
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

TEST_F(PlatformViewAndroidJNIImplReproduceTest, SurfaceTextureUpdateTexImageGracefulFailureOnException) {
  MockJNIEnvProvider2 env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  // Setup basic mock expectations for JNI operations
  EXPECT_CALL(mock_env, GetObjectRefType(_))
      .WillRepeatedly(Return(JNILocalRefType));
  EXPECT_CALL(mock_env, NewLocalRef(_)).WillRepeatedly(ReturnArg<0>());
  EXPECT_CALL(mock_env, DeleteLocalRef(_)).WillRepeatedly(Return());

  // 1. WeakReference.get() call: returns a non-null surface texture local ref
  const jobject kSurfaceTextureWeakRef = reinterpret_cast<jobject>(111);
  const jobject kSurfaceTextureLocalRef = reinterpret_cast<jobject>(222);
  EXPECT_CALL(mock_env, CallObjectMethodV(kSurfaceTextureWeakRef, _, _))
      .WillOnce(Return(kSurfaceTextureLocalRef));

  // 2. SurfaceTextureWrapper.updateTexImage() call
  EXPECT_CALL(mock_env, CallVoidMethodV(kSurfaceTextureLocalRef, _, _))
      .Times(1);

  // 3. ExceptionCheck returns true for the first check, and false for subsequent checks
  EXPECT_CALL(mock_env, ExceptionCheck())
      .WillOnce(Return(JNI_TRUE))
      .WillRepeatedly(Return(JNI_FALSE));

  // 4. ExceptionOccurred returns a mock exception object
  const jthrowable kMockException = reinterpret_cast<jthrowable>(333);
  EXPECT_CALL(mock_env, ExceptionOccurred())
      .WillOnce(Return(kMockException));

  // 5. ExceptionClear is called to clear the JNI pending exception status
  EXPECT_CALL(mock_env, ExceptionClear())
      .Times(1);

  // 6. Mock expectations for GetJavaExceptionInfo to avoid null pointer dereferences/crashes
  // inside exception formatting.
  // NewObjectV is called twice to create ByteArrayOutputStream and PrintStream.
  const jobject kMockStream = reinterpret_cast<jobject>(444);
  EXPECT_CALL(mock_env, NewObjectV(_, _, _))
      .WillRepeatedly(Return(kMockStream));

  // printStackTrace is called on the exception
  EXPECT_CALL(mock_env, CallVoidMethodV(kMockException, _, _))
      .Times(1);

  // toString is called on the ByteArrayOutputStream to get the stack trace string
  EXPECT_CALL(mock_env, CallObjectMethodV(kMockStream, _, _))
      .WillRepeatedly(Return(nullptr)); // Return null string to avoid further JNI string calls

  // DeleteLocalRef is called on the exception object
  EXPECT_CALL(mock_env, DeleteLocalRef(kMockException))
      .Times(1);

  // Instantiate JNI Implementation
  fml::jni::JavaObjectWeakGlobalRef flutter_jni_object;
  PlatformViewAndroidJNIImpl android_jni(flutter_jni_object);

  JavaLocalRef surface_texture(
      &mock_env, kSurfaceTextureWeakRef);

  // Call the method. We expect it to return false (failure) and not crash.
  bool result = android_jni.SurfaceTextureUpdateTexImage(surface_texture);
  EXPECT_FALSE(result);
}

TEST_F(PlatformViewAndroidJNIImplReproduceTest, SurfaceTextureGetTransformMatrixGracefulFailureOnException) {
  MockJNIEnvProvider2 env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  // Setup basic mock expectations for JNI operations
  EXPECT_CALL(mock_env, GetObjectRefType(_))
      .WillRepeatedly(Return(JNILocalRefType));
  EXPECT_CALL(mock_env, NewLocalRef(_)).WillRepeatedly(ReturnArg<0>());
  EXPECT_CALL(mock_env, DeleteLocalRef(_)).WillRepeatedly(Return());

  // 1. WeakReference.get() call: returns a non-null surface texture local ref
  const jobject kSurfaceTextureWeakRef = reinterpret_cast<jobject>(111);
  const jobject kSurfaceTextureLocalRef = reinterpret_cast<jobject>(222);
  EXPECT_CALL(mock_env, CallObjectMethodV(kSurfaceTextureWeakRef, _, _))
      .WillOnce(Return(kSurfaceTextureLocalRef));

  // 2. NewFloatArray is called to allocate the matrix array
  const jfloatArray kMockFloatArray = reinterpret_cast<jfloatArray>(555);
  EXPECT_CALL(mock_env, NewFloatArray(16))
      .WillOnce(Return(kMockFloatArray));

  // 3. SurfaceTextureWrapper.getTransformMatrix() call
  EXPECT_CALL(mock_env, CallVoidMethodV(kSurfaceTextureLocalRef, _, _))
      .Times(1);

  // 4. ExceptionCheck returns true for the first check, and false for subsequent checks
  EXPECT_CALL(mock_env, ExceptionCheck())
      .WillOnce(Return(JNI_TRUE))
      .WillRepeatedly(Return(JNI_FALSE));

  // 5. ExceptionOccurred returns a mock exception object
  const jthrowable kMockException = reinterpret_cast<jthrowable>(333);
  EXPECT_CALL(mock_env, ExceptionOccurred())
      .WillOnce(Return(kMockException));

  // 6. ExceptionClear is called to clear the JNI pending exception status
  EXPECT_CALL(mock_env, ExceptionClear())
      .Times(1);

  // 7. Mock expectations for GetJavaExceptionInfo to avoid null pointer dereferences/crashes
  // inside exception formatting.
  const jobject kMockStream = reinterpret_cast<jobject>(444);
  EXPECT_CALL(mock_env, NewObjectV(_, _, _))
      .WillRepeatedly(Return(kMockStream));

  // printStackTrace is called on the exception
  EXPECT_CALL(mock_env, CallVoidMethodV(kMockException, _, _))
      .Times(1);

  // toString is called on the ByteArrayOutputStream to get the stack trace string
  EXPECT_CALL(mock_env, CallObjectMethodV(kMockStream, _, _))
      .WillRepeatedly(Return(nullptr)); // Return null string to avoid further JNI string calls

  // DeleteLocalRef is called on the exception object
  EXPECT_CALL(mock_env, DeleteLocalRef(kMockException))
      .Times(1);

  // DeleteLocalRef is called on the float array object
  EXPECT_CALL(mock_env, DeleteLocalRef(kMockFloatArray))
      .Times(1);

  // Instantiate JNI Implementation
  fml::jni::JavaObjectWeakGlobalRef flutter_jni_object;
  PlatformViewAndroidJNIImpl android_jni(flutter_jni_object);

  JavaLocalRef surface_texture(
      &mock_env, kSurfaceTextureWeakRef);

  // Call the method. We expect it to return an empty SkM44 and not crash.
  SkM44 result = android_jni.SurfaceTextureGetTransformMatrix(surface_texture);
  EXPECT_EQ(result, SkM44());
}

}  // namespace testing
}  // namespace flutter
