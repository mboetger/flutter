// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.plugin.platform;

import android.content.Context;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * Registry for platform view factories.
 *
 * <p>Plugins can register factories for specific view types.
 */
public interface PlatformViewRegistry {

  /**
   * Registers a factory for a platform view.
   *
   * @param viewTypeId unique identifier for the platform view's type.
   * @param factory factory for creating platform views of the specified type.
   * @return true if succeeded, false if a factory is already registered for viewTypeId.
   */
  boolean registerViewFactory(@NonNull String viewTypeId, @NonNull PlatformViewFactory factory);

  /**
   * Registers a factory for a platform view using a callback.
   *
   * @param viewTypeId unique identifier for the platform view's type.
   * @param factoryCallback callback for creating platform views of the specified type.
   * @return true if succeeded, false if a factory is already registered for viewTypeId.
   */
  default boolean registerViewFactory(
      @NonNull String viewTypeId, @NonNull PlatformViewFactoryCallback factoryCallback) {
    return registerViewFactory(
        viewTypeId,
        new PlatformViewFactory(null) {
          @NonNull
          @Override
          public PlatformView create(
              @NonNull Context context, int viewId, @Nullable Object args) {
            return factoryCallback.create(context, viewId, args);
          }
        });
  }
}
