// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_ENGINE_BRIDGE_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_ENGINE_BRIDGE_H_

#include <memory>
#include <string>
#include <vector>

#include "flutter/common/settings.h"
#include "flutter/fml/macros.h"
#include "flutter/fml/memory/weak_ptr.h"
#include "flutter/shell/common/rasterizer.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/apk_asset_provider.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"

namespace flutter {

class PlatformViewAndroid;
class EmbedderSurfaceAndroid;
class PlatformMessageHandler;
class Shell;

//----------------------------------------------------------------------------
/// @brief      Abstract interface representing an engine instance for the
///             Android platform embedder.
///
/// @details    AndroidEngineBridge abstracts engine lifecycle, execution,
///             rendering, surface management, and platform message dispatch
///             behind a unified facade. This decouples AndroidShellHolder from
///             internal C++ engine implementations (e.g. flutter::Shell) and
///             enables swapping between legacy internal shell and public
///             Embedder API implementations.
///
class AndroidEngineBridge {
 public:
  virtual ~AndroidEngineBridge() = default;

  //----------------------------------------------------------------------------
  /// @brief      Factory function to create an AndroidEngineBridge instance.
  ///
  /// @param[in]  settings              The Flutter engine settings.
  /// @param[in]  jni_facade            The JNI callback facade.
  /// @param[in]  android_rendering_api The selected rendering API.
  ///
  /// @returns    A unique_ptr to the constructed AndroidEngineBridge.
  ///
  static std::unique_ptr<AndroidEngineBridge> Create(
      const flutter::Settings& settings,
      std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
      AndroidRenderingAPI android_rendering_api);

  //----------------------------------------------------------------------------
  /// @brief      Returns whether the underlying engine instance is valid.
  ///
  virtual bool IsValid() const = 0;

  //----------------------------------------------------------------------------
  /// @brief      Spawns a new AndroidEngineBridge sharing resources where
  /// possible.
  ///
  virtual std::unique_ptr<AndroidEngineBridge> Spawn(
      std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
      const std::string& entrypoint,
      const std::string& libraryUrl,
      const std::string& initial_route,
      const std::vector<std::string>& entrypoint_args,
      int64_t engine_id) const = 0;

  //----------------------------------------------------------------------------
  /// @brief      Launches Dart execution for the engine.
  ///
  virtual void Launch(std::unique_ptr<APKAssetProvider> apk_asset_provider,
                      const std::string& entrypoint,
                      const std::string& libraryUrl,
                      const std::vector<std::string>& entrypoint_args,
                      int64_t engine_id) = 0;

  //----------------------------------------------------------------------------
  /// @brief      Returns the Flutter settings for this engine.
  ///
  virtual const flutter::Settings& GetSettings() const = 0;

  //----------------------------------------------------------------------------
  /// @brief      Returns a WeakPtr to the PlatformViewAndroid.
  ///
  virtual fml::WeakPtr<PlatformViewAndroid> GetPlatformView() = 0;

  //----------------------------------------------------------------------------
  /// @brief      Returns a raw pointer to the PlatformViewAndroid.
  ///
  virtual PlatformViewAndroid* GetPlatformViewAndroid() = 0;

  //----------------------------------------------------------------------------
  /// @brief      Returns a raw pointer to the EmbedderSurfaceAndroid.
  ///
  virtual EmbedderSurfaceAndroid* GetEmbedderSurfaceAndroid() = 0;

  //----------------------------------------------------------------------------
  /// @brief      Returns whether SurfaceControl is enabled.
  ///
  virtual bool IsSurfaceControlEnabled() = 0;

  //----------------------------------------------------------------------------
  /// @brief      Takes a screenshot of the rasterized content.
  ///
  virtual Rasterizer::Screenshot Screenshot(Rasterizer::ScreenshotType type,
                                            bool base64_encode) = 0;

  //----------------------------------------------------------------------------
  /// @brief      Notifies the engine that a low memory warning occurred.
  ///
  virtual void NotifyLowMemoryWarning() = 0;

  //----------------------------------------------------------------------------
  /// @brief      Returns the platform message handler.
  ///
  virtual const std::shared_ptr<PlatformMessageHandler>&
  GetPlatformMessageHandler() const = 0;

  //----------------------------------------------------------------------------
  /// @brief      Updates display metrics.
  ///
  virtual void UpdateDisplayMetrics() = 0;

  //----------------------------------------------------------------------------
  /// @brief      Returns the underlying Shell instance, if available.
  ///             Visible for testing.
  ///
  virtual const std::unique_ptr<Shell>& GetShellForTesting() const;
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_ENGINE_BRIDGE_H_
