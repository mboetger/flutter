// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.plugin.platform;

import android.content.Context;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/** Callback interface to register a platform view factory using a lambda. */
@FunctionalInterface
public interface PlatformViewFactoryCallback {
  /**
   * Creates a new Android view to be embedded in the Flutter hierarchy.
   *
   * @param context the context to be used when creating the view.
   * @param viewId unique identifier for the created instance.
   * @param args arguments sent from the Flutter app.
   */
  @NonNull
  PlatformView create(@NonNull Context context, int viewId, @Nullable Object args);
}
