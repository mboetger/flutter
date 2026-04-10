// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_ENGINE_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_ENGINE_H_

#include <memory>
#include <string>
#include <vector>

#include "flutter/lib/ui/painting/image_generator_registry.h"
#include "flutter/shell/common/rasterizer.h"
#include "flutter/shell/common/run_configuration.h"
#include "flutter/shell/common/shell.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

/**
 * @brief An interface for the Android embedder to interact with the Flutter
 * engine.
 *
 * This abstraction allows the Android embedder to switch between the internal
 * flutter::Shell implementation and the public embedder.h API.
 */
class AndroidEngine {
 public:
  virtual ~AndroidEngine() = default;

  virtual bool IsValid() const = 0;

  virtual bool IsSetup() const = 0;

  virtual void RunEngine(RunConfiguration run_configuration) = 0;

  virtual std::unique_ptr<AndroidEngine> Spawn(
      RunConfiguration run_configuration,
      const std::string& initial_route,
      Shell::CreateCallback<PlatformView> on_create_platform_view,
      Shell::CreateCallback<Rasterizer> on_create_rasterizer) const = 0;

  virtual Rasterizer::Screenshot Screenshot(Rasterizer::ScreenshotType type,
                                            bool base64_encode) = 0;

  virtual void NotifyLowMemoryWarning() = 0;

  virtual void OnDisplayUpdates(
      std::vector<std::unique_ptr<Display>> displays) = 0;

  virtual std::shared_ptr<PlatformMessageHandler> GetPlatformMessageHandler()
      const = 0;

  virtual void RegisterImageDecoder(ImageGeneratorFactory factory,
                                    int32_t priority) = 0;

  virtual const TaskRunners& GetTaskRunners() const = 0;

  virtual Shell& GetShell() = 0;
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_ENGINE_H_
