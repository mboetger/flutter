// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static io.flutter.Build.API_LEVELS;
import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.*;

import android.annotation.TargetApi;
import android.content.Context;
import android.media.ImageReader;
import android.view.Surface;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.engine.renderer.FlutterRenderer;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;

@RunWith(AndroidJUnit4.class)
@TargetApi(API_LEVELS.API_29)
public class FlutterImageViewTest {
  private final Context ctx = ApplicationProvider.getApplicationContext();

  @Test
  public void onSizeChanged_swapsSurfaceBeforeClosingOldReader() {
    // Setup test.
    final ImageReader mockImageReader = mock(ImageReader.class);
    final Surface mockSurface = mock(Surface.class);
    when(mockImageReader.getSurface()).thenReturn(mockSurface);

    final java.util.List<String> callOrder = new java.util.ArrayList<>();

    doAnswer(invocation -> {
      callOrder.add("imageReader.close");
      return null;
    }).when(mockImageReader).close();

    final FlutterImageView imageView =
        spy(new FlutterImageView(ctx, mockImageReader, FlutterImageView.SurfaceKind.background));

    final FlutterRenderer mockRenderer = mock(FlutterRenderer.class);
    doAnswer(invocation -> {
      callOrder.add("renderer.swapSurface");
      return null;
    }).when(mockRenderer).swapSurface(any());

    imageView.attachToRenderer(mockRenderer);

    // Clear callOrder after attachment to only track events during onSizeChanged.
    callOrder.clear();

    // Trigger onSizeChanged to simulate rotation/resizing.
    imageView.onSizeChanged(100, 100, 0, 0);

    // Assert that the calls happened, and print them.
    System.out.println("DEBUG: Call order: " + callOrder);
    assertEquals(2, callOrder.size());
    // Correct lifecycle sequence: the renderer must be notified/swapped to a new surface
    // before the old image reader is closed.
    assertEquals("renderer.swapSurface", callOrder.get(0));
    assertEquals("imageReader.close", callOrder.get(1));
  }
}
