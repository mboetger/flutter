// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import android.os.Build;
import android.view.Window;
import androidx.annotation.NonNull;
import io.flutter.Build.API_LEVELS;

/**
 * Captures and restores window visual configurations (flags, system UI visibility, colors, etc.).
 *
 * <p>Used during Activity initialization when transitioning from a launch theme (splash screen) to a
 * normal theme to ensure that launch screen configurations (such as fullscreen mode and status bar
 * styling) are preserved until Flutter renders its UI.
 */
@SuppressWarnings("deprecation")
class SplashScreenWindowConfig {
  private final int flags;
  private final int systemUiVisibility;
  private final int statusBarColor;
  private final int navigationBarColor;
  private int navigationBarDividerColor;
  private boolean statusBarContrastEnforced;
  private boolean navigationBarContrastEnforced;

  public SplashScreenWindowConfig(@NonNull Window window) {
    this.flags = window.getAttributes().flags;
    this.systemUiVisibility = window.getDecorView().getSystemUiVisibility();
    this.statusBarColor = window.getStatusBarColor();
    this.navigationBarColor = window.getNavigationBarColor();
    if (Build.VERSION.SDK_INT >= API_LEVELS.API_28) {
      this.navigationBarDividerColor = window.getNavigationBarDividerColor();
    }
    if (Build.VERSION.SDK_INT >= API_LEVELS.API_29) {
      this.statusBarContrastEnforced = window.isStatusBarContrastEnforced();
      this.navigationBarContrastEnforced = window.isNavigationBarContrastEnforced();
    }
  }

  public void restore(@NonNull Window window) {
    window.setFlags(flags, ~0);
    window.getDecorView().setSystemUiVisibility(systemUiVisibility);
    window.setStatusBarColor(statusBarColor);
    window.setNavigationBarColor(navigationBarColor);
    if (Build.VERSION.SDK_INT >= API_LEVELS.API_28) {
      window.setNavigationBarDividerColor(navigationBarDividerColor);
    }
    if (Build.VERSION.SDK_INT >= API_LEVELS.API_29) {
      window.setStatusBarContrastEnforced(statusBarContrastEnforced);
      window.setNavigationBarContrastEnforced(navigationBarContrastEnforced);
    }
  }
}
