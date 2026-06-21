// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import io.flutter.Build;
import io.flutter.embedding.engine.renderer.FlutterRenderer;
import io.flutter.util.ViewUtils;
import java.util.Collections;
import java.util.List;

/**
 * A delegate class that performs the task of retrieving the bounding rect values. Logic that is
 * independent of the engine, or that tests must access in the absence of an engine, shall reside
 * within this class.
 */
public class FlutterViewDelegate {
  /**
   * Return the WindowInsets object for the provided Context. A Context will only have a window if
   * it is an instance of Activity. If context does not have a window, or it is not an activity,
   * this method will return null. Otherwise, this method will return the WindowInsets for the
   * provided activity's window.
   */
  @VisibleForTesting
  public WindowInsets getWindowInsets(Context context) {
    Activity activity = ViewUtils.getActivity(context);
    if (activity == null) {
      return null;
    }
    Window window = activity.getWindow();
    if (window == null) {
      return null;
    }
    return window.getDecorView().getRootWindowInsets();
  }

  @RequiresApi(api = Build.API_LEVELS.API_35)
  public List<Rect> getCaptionBarInsets(Context context) {
    WindowInsets insets = getWindowInsets(context);
    if (insets == null) {
      return Collections.emptyList();
    }
    return insets.getBoundingRects(WindowInsets.Type.captionBar());
  }

  @RequiresApi(api = Build.API_LEVELS.API_35)
  public void growViewportMetricsToCaptionBar(
      View view, FlutterRenderer.ViewportMetrics viewportMetrics) {
    Context context = view.getContext();
    List<Rect> boundingRects = getCaptionBarInsets(context);
    int viewPaddingTop = viewportMetrics.viewPaddingTop;

    int[] location = new int[2];
    view.getLocationOnScreen(location);
    int viewY = location[1];

    int windowY = 0;
    Activity activity = ViewUtils.getActivity(context);
    if (activity != null) {
      Window window = activity.getWindow();
      if (window != null) {
        View decorView = window.getDecorView();
        if (decorView != null) {
          int[] decorLocation = new int[2];
          decorView.getLocationOnScreen(decorLocation);
          windowY = decorLocation[1];
        }
      }
    }

    double density = context.getResources().getDisplayMetrics().density;
    double maxCaptionBarHeight = 150.0 * density;

    for (Rect rect : boundingRects) {
      int localBottom = calculateLocalPaddingTop(rect, viewY, windowY, maxCaptionBarHeight);
      viewPaddingTop = Math.max(viewPaddingTop, localBottom);
    }
    // The value getCaptionBarInset returns is only the bounding rects of the caption bar.
    // When assigning the new value of viewPaddingTop, the maximum is taken with its old value
    // to ensure that any previous top padding that is greater than that from the caption bar
    // is not destroyed by this operation.
    // Any potential update that will allow the caption bar to be positioned somewhere other than
    // the top of the app window will require that this method be rewritten.
    viewportMetrics.viewPaddingTop = viewPaddingTop;
  }

  /**
   * Calculates the view-local top padding required to avoid the caption bar, safely handling both
   * window-relative (standard) and screen-relative (buggy OEM) rects.
   */
  @VisibleForTesting
  static int calculateLocalPaddingTop(
      Rect rect, int viewY, int windowY, double maxCaptionBarHeight) {
    int rectHeight = rect.bottom - rect.top;
    if (rectHeight >= maxCaptionBarHeight) {
      // Ignore corrupted/incorrect rects that cover the entire window or screen.
      return 0;
    }

    // Detect if coordinates are in screen space or window space.
    // If the window is positioned at Y > 0 and the rect starts at or below that,
    // the rect is screen-relative.
    boolean isScreenRelative = windowY > 0 && rect.top >= windowY;
    int windowRelativeBottom = isScreenRelative ? (rect.bottom - windowY) : rect.bottom;

    // Convert window-relative bottom to view-local coordinate space.
    int viewYInWindow = Math.max(0, viewY - windowY);
    return Math.max(0, windowRelativeBottom - viewYInWindow);
  }
}
