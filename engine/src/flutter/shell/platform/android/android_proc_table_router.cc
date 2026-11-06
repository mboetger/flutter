// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_proc_table_router.h"

#include <cstring>

#include "flutter/fml/logging.h"

extern "C" {
FLUTTER_EXPORT
FlutterEngineResult FlutterEngineGetProcAddresses(
    FlutterEngineProcTable* procs);
}

namespace flutter::android {

FlutterEngineResult AndroidProcTableRouter::BuildProcTable(
    bool enable_embedder_api,
    FlutterEngineProcTable* out_table,
    AndroidSurfaceLifecycleHooks* out_hooks) {
  if (out_table == nullptr) {
    return kInvalidArguments;
  }
  std::memset(out_table, 0, sizeof(FlutterEngineProcTable));
  out_table->struct_size = sizeof(FlutterEngineProcTable);

  const FlutterEngineResult result = FlutterEngineGetProcAddresses(out_table);
  if (result != kSuccess) {
    FML_LOG(ERROR) << "FlutterEngineGetProcAddresses failed: " << result;
    return result;
  }

  if (out_hooks != nullptr) {
    out_hooks->notify_surface_created =
        [table = *out_table](FLUTTER_API_SYMBOL(FlutterEngine) engine,
                             void* /*native_window*/, size_t width,
                             size_t height) -> FlutterEngineResult {
      if (table.SendWindowMetricsEvent == nullptr || engine == nullptr) {
        return kInvalidArguments;
      }
      FlutterWindowMetricsEvent metrics = {};
      metrics.struct_size = sizeof(FlutterWindowMetricsEvent);
      metrics.width = width;
      metrics.height = height;
      metrics.pixel_ratio = 1.0;
      return table.SendWindowMetricsEvent(engine, &metrics);
    };
    out_hooks->notify_surface_changed =
        [table = *out_table](FLUTTER_API_SYMBOL(FlutterEngine) engine,
                             size_t width,
                             size_t height) -> FlutterEngineResult {
      if (table.SendWindowMetricsEvent == nullptr || engine == nullptr) {
        return kInvalidArguments;
      }
      FlutterWindowMetricsEvent metrics = {};
      metrics.struct_size = sizeof(FlutterWindowMetricsEvent);
      metrics.width = width;
      metrics.height = height;
      metrics.pixel_ratio = 1.0;
      return table.SendWindowMetricsEvent(engine, &metrics);
    };
    out_hooks->notify_surface_destroyed =
        [table = *out_table](
            FLUTTER_API_SYMBOL(FlutterEngine) engine) -> FlutterEngineResult {
      if (table.NotifyDisplayUpdate == nullptr || engine == nullptr) {
        return kInvalidArguments;
      }
      return kSuccess;
    };
  }

  (void)enable_embedder_api;
  return kSuccess;
}

}  // namespace flutter::android
