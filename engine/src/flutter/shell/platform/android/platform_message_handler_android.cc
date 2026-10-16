// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/platform_message_handler_android.h"

#include <utility>

#include "flutter/fml/logging.h"

namespace flutter {

PlatformMessageHandlerAndroid::PlatformMessageHandlerAndroid(
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade)
    : jni_facade_(jni_facade) {}

PlatformMessageHandlerAndroid::~PlatformMessageHandlerAndroid() {
  ReleasePendingResponses();
}

void PlatformMessageHandlerAndroid::Attach(
    FLUTTER_API_SYMBOL(FlutterEngine) engine,
    const FlutterEngineProcTable* embedder_api) {
  std::lock_guard lock(pending_responses_mutex_);
  engine_ = engine;
  embedder_api_ = embedder_api;
}

void PlatformMessageHandlerAndroid::ReleasePendingResponses() {
  std::unordered_map<int, const FlutterPlatformMessageResponseHandle*>
      to_release;
  FLUTTER_API_SYMBOL(FlutterEngine) engine = nullptr;
  const FlutterEngineProcTable* api = nullptr;
  {
    std::lock_guard lock(pending_responses_mutex_);
    to_release.swap(pending_responses_);
    engine = engine_;
    api = embedder_api_;
    engine_ = nullptr;
    embedder_api_ = nullptr;
  }
  if (api != nullptr && api->PlatformMessageReleaseResponseHandle != nullptr &&
      engine != nullptr) {
    for (auto& [id, handle] : to_release) {
      if (handle != nullptr) {
        api->PlatformMessageReleaseResponseHandle(
            engine, const_cast<FlutterPlatformMessageResponseHandle*>(handle));
      }
    }
  }
}

void PlatformMessageHandlerAndroid::InvokePlatformMessageResponseCallback(
    int response_id,
    const uint8_t* data,
    size_t size) {
  // Called from any thread.
  if (!response_id) {
    return;
  }
  const FlutterPlatformMessageResponseHandle* handle = nullptr;
  FLUTTER_API_SYMBOL(FlutterEngine) engine = nullptr;
  const FlutterEngineProcTable* api = nullptr;
  {
    std::lock_guard lock(pending_responses_mutex_);
    auto it = pending_responses_.find(response_id);
    if (it == pending_responses_.end()) {
      return;
    }
    handle = it->second;
    pending_responses_.erase(it);
    engine = engine_;
    api = embedder_api_;
  }
  if (api == nullptr || api->SendPlatformMessageResponse == nullptr ||
      handle == nullptr) {
    return;
  }
  // Ensure a 0-byte non-null response passes a non-null pointer so the C-API
  // distinguishes it from `InvokePlatformMessageEmptyResponseCallback` (which
  // passes `nullptr, 0`).
  static constexpr uint8_t kEmptyResponseByte = 0;
  const uint8_t* reply_ptr =
      (size == 0 && data == nullptr) ? &kEmptyResponseByte : data;
  api->SendPlatformMessageResponse(engine, handle, reply_ptr, size);
}

void PlatformMessageHandlerAndroid::InvokePlatformMessageEmptyResponseCallback(
    int response_id) {
  // Called from any thread.
  if (!response_id) {
    return;
  }
  const FlutterPlatformMessageResponseHandle* handle = nullptr;
  FLUTTER_API_SYMBOL(FlutterEngine) engine = nullptr;
  const FlutterEngineProcTable* api = nullptr;
  {
    std::lock_guard lock(pending_responses_mutex_);
    auto it = pending_responses_.find(response_id);
    if (it == pending_responses_.end()) {
      return;
    }
    handle = it->second;
    pending_responses_.erase(it);
    engine = engine_;
    api = embedder_api_;
  }
  if (api == nullptr || api->SendPlatformMessageResponse == nullptr ||
      handle == nullptr) {
    return;
  }
  api->SendPlatformMessageResponse(engine, handle, nullptr, 0);
}

void PlatformMessageHandlerAndroid::HandlePlatformMessage(
    const FlutterPlatformMessage& message) {
  // Called from any thread (UI thread or background isolate thread).
  UniquePlatformMessagePayload payload(new FlutterPlatformMessagePayload{
      .struct_size = sizeof(FlutterPlatformMessagePayload),
  });

  int response_id = next_response_id_.fetch_add(1);
  {
    std::lock_guard lock(pending_responses_mutex_);
    if (engine_ == nullptr || embedder_api_ == nullptr) {
      return;
    }
    if (message.response_handle != nullptr) {
      if (embedder_api_->PlatformMessageTakePayload != nullptr) {
        FlutterEngineResult take_result =
            embedder_api_->PlatformMessageTakePayload(
                engine_, message.response_handle, payload.get());
        if (take_result != kSuccess) {
          FML_DLOG(ERROR) << "FlutterPlatformMessageTakePayload failed: "
                          << take_result;
        }
      }
      pending_responses_[response_id] = message.response_handle;
    } else if (message.message != nullptr) {
      // Fallback when no response handle is attached: copy the bytes into a
      // self-owned buffer so the payload still outlives the callback.
      uint8_t* copy = static_cast<uint8_t*>(std::malloc(message.message_size));
      if (copy != nullptr && message.message_size > 0) {
        std::memcpy(copy, message.message, message.message_size);
      }
      payload->data = copy != nullptr ? copy : message.message;
      payload->data_size = message.message_size;
      payload->user_data = copy;
      payload->destruction_callback = [](void* p) { std::free(p); };
    }
  }

  // This call can re-enter in InvokePlatformMessageXxxResponseCallback.
  jni_facade_->FlutterViewHandlePlatformMessage(
      message.channel != nullptr ? message.channel : "", std::move(payload),
      response_id);
}

}  // namespace flutter
