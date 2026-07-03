// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import androidx.annotation.NonNull;
import androidx.lifecycle.Lifecycle;
import io.flutter.embedding.engine.FlutterEngine;

/**
 * Configures a {@link io.flutter.embedding.engine.FlutterEngine} after it is created, e.g., adds
 * plugins.
 *
 * <p>This interface may be applied to a {@link androidx.fragment.app.FragmentActivity} that owns a
 * {@code FlutterFragment}.
 */
public interface FlutterEngineConfigurator {
  /**
   * Configures the given {@link io.flutter.embedding.engine.FlutterEngine}.
   *
   * <p>This method is called after the given {@link io.flutter.embedding.engine.FlutterEngine} has
   * been attached to the owning {@code FragmentActivity}. See {@link
   * io.flutter.embedding.engine.plugins.activity.ActivityControlSurface#attachToActivity(
   * ExclusiveAppComponent, Lifecycle)}.
   *
   * <p>It is possible that the owning {@code FragmentActivity} opted not to connect itself as an
   * {@link io.flutter.embedding.engine.plugins.activity.ActivityControlSurface}. In that case, any
   * configuration, e.g., plugins, must not expect or depend upon an available {@code Activity} at
   * the time that this method is invoked.
   *
   * <p><b>Warning:</b> When using a cached {@link io.flutter.embedding.engine.FlutterEngine} (e.g.
   * via {@link io.flutter.embedding.engine.FlutterEngineCache}), if the engine's Dart entrypoint is
   * executed before the Activity or Fragment is attached, any platform channels or plugins
   * registered in {@code configureFlutterEngine} will not be available to the Dart code that ran
   * prior to the attachment. In such cases, these configurations/registrations should be performed
   * when the {@link io.flutter.embedding.engine.FlutterEngine} is first created and cached (e.g.,
   * in {@code Application.onCreate}).
   *
   * @param flutterEngine The Flutter engine.
   */
  void configureFlutterEngine(@NonNull FlutterEngine flutterEngine);

  /**
   * Cleans up references that were established in {@link #configureFlutterEngine(FlutterEngine)}
   * before the host is destroyed or detached.
   *
   * @param flutterEngine The Flutter engine.
   */
  void cleanUpFlutterEngine(@NonNull FlutterEngine flutterEngine);
}
