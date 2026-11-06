// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_PROC_TABLE_ROUTER_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_PROC_TABLE_ROUTER_H_

#include <cstddef>
#include <cstdint>
#include <functional>

#include "flutter/fml/macros.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter::android {

/// Out-of-band surface lifecycle hooks populated alongside
/// `FlutterEngineProcTable` during the Phase 0..2 dual-path strangler seam.
struct AndroidSurfaceLifecycleHooks {
  std::function<FlutterEngineResult(FLUTTER_API_SYMBOL(FlutterEngine) engine,
                                    void* native_window,
                                    size_t width,
                                    size_t height)>
      notify_surface_created;
  std::function<FlutterEngineResult(FLUTTER_API_SYMBOL(FlutterEngine) engine,
                                    size_t width,
                                    size_t height)>
      notify_surface_changed;
  std::function<FlutterEngineResult(FLUTTER_API_SYMBOL(FlutterEngine) engine)>
      notify_surface_destroyed;
};

/// Dual-path `FlutterEngineProcTable` router for the RFC 410.0000 migration.
///
/// When `enable_embedder_api == true`, populates `out_table` directly via
/// `FlutterEngineGetProcAddresses(out_table)`.
/// When `enable_embedder_api == false`, populates `out_table` with a dispatch
/// table backed by the embedder C-API symbol table while allowing incremental
/// domain migration across Phase 1A, Phase 1B, and Phase 2.
class AndroidProcTableRouter {
 public:
  static FlutterEngineResult BuildProcTable(
      bool enable_embedder_api,
      FlutterEngineProcTable* out_table,
      AndroidSurfaceLifecycleHooks* out_hooks = nullptr);

  static bool DefaultEnableEmbedderApi() {
    const bool enable_embedder_api = false;
    return enable_embedder_api;
  }

  FML_DISALLOW_IMPLICIT_CONSTRUCTORS(AndroidProcTableRouter);
};

}  // namespace flutter::android

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_PROC_TABLE_ROUTER_H_
