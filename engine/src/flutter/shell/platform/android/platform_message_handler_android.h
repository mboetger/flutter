// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_PLATFORM_MESSAGE_HANDLER_ANDROID_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_PLATFORM_MESSAGE_HANDLER_ANDROID_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

/// Bridges Dart-to-platform messages delivered through the embedder C-API
/// (`FlutterPlatformMessageCallback`) to `PlatformViewAndroidJNI`, and routes
/// Java reply callbacks (`FlutterJNI.invokePlatformMessage*ResponseCallback`)
/// back through `FlutterEngineProcTable::SendPlatformMessageResponse`.
class PlatformMessageHandlerAndroid {
 public:
  explicit PlatformMessageHandlerAndroid(
      const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade);

  ~PlatformMessageHandlerAndroid();

  /// Attaches the C-API engine handle and proc table used to take message
  /// payloads, send replies, and release unanswered handles on shutdown.
  void Attach(FLUTTER_API_SYMBOL(FlutterEngine) engine,
              const FlutterEngineProcTable* embedder_api);

  /// Releases any still-pending `FlutterPlatformMessageResponseHandle`s via
  /// `PlatformMessageReleaseResponseHandle` and detaches from the engine.
  void ReleasePendingResponses();

  /// Handles a Dart-to-platform message from the C-API callback. May be called
  /// from any thread (UI thread or background isolate thread).
  void HandlePlatformMessage(const FlutterPlatformMessage& message);

  /// Sends a non-null reply (including a 0-byte non-null reply) for
  /// `response_id`. May be called from any thread.
  void InvokePlatformMessageResponseCallback(int response_id,
                                             const uint8_t* data,
                                             size_t size);

  /// Sends an empty (null) reply for `response_id`. May be called from any
  /// thread.
  void InvokePlatformMessageEmptyResponseCallback(int response_id);

 private:
  const std::shared_ptr<PlatformViewAndroidJNI> jni_facade_;
  FLUTTER_API_SYMBOL(FlutterEngine) engine_ = nullptr;
  const FlutterEngineProcTable* embedder_api_ = nullptr;
  std::atomic<int> next_response_id_ = 1;
  std::unordered_map<int, const FlutterPlatformMessageResponseHandle*>
      pending_responses_;
  std::mutex pending_responses_mutex_;
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_PLATFORM_MESSAGE_HANDLER_ANDROID_H_
