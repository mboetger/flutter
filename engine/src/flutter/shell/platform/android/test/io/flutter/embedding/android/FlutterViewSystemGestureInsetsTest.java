// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.Insets;
import android.view.WindowInsets;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.Build.API_LEVELS;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.embedding.engine.FlutterJNI;
import io.flutter.embedding.engine.loader.FlutterLoader;
import io.flutter.embedding.engine.renderer.FlutterRenderer;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

@RunWith(AndroidJUnit4.class)
@TargetApi(30)
public class FlutterViewSystemGestureInsetsTest {
  private final Context ctx = ApplicationProvider.getApplicationContext();
  @Mock FlutterJNI mockFlutterJni;
  @Mock FlutterLoader mockFlutterLoader;

  private FlutterView flutterView;
  private FlutterEngine flutterEngine;
  private FlutterRenderer flutterRenderer;

  @Before
  public void setUp() {
    MockitoAnnotations.openMocks(this);
    when(mockFlutterJni.isAttached()).thenReturn(true);
  }

  private void setUpViewAndEngine() {
    flutterView = new FlutterView(ctx);
    flutterEngine = spy(new FlutterEngine(ctx, mockFlutterLoader, mockFlutterJni));
    flutterRenderer = spy(new FlutterRenderer(mockFlutterJni));
    when(flutterEngine.getRenderer()).thenReturn(flutterRenderer);
    flutterView.attachToFlutterEngine(flutterEngine);
  }

  @Test
  @Config(sdk = API_LEVELS.API_28)
  @SuppressWarnings("deprecation")
  public void systemGestureInsets_api28_defaultsToZero() {
    setUpViewAndEngine();

    WindowInsets windowInsets = mock(WindowInsets.class);
    when(windowInsets.getSystemWindowInsetLeft()).thenReturn(0);
    when(windowInsets.getSystemWindowInsetTop()).thenReturn(0);
    when(windowInsets.getSystemWindowInsetRight()).thenReturn(0);
    when(windowInsets.getSystemWindowInsetBottom()).thenReturn(0);

    flutterView.onApplyWindowInsets(windowInsets);

    ArgumentCaptor<FlutterRenderer.ViewportMetrics> viewportMetricsCaptor =
        ArgumentCaptor.forClass(FlutterRenderer.ViewportMetrics.class);
    verify(flutterRenderer, atLeastOnce()).setViewportMetrics(viewportMetricsCaptor.capture());

    FlutterRenderer.ViewportMetrics metrics = viewportMetricsCaptor.getValue();
    assertEquals(0, metrics.systemGestureInsetLeft);
    assertEquals(0, metrics.systemGestureInsetTop);
    assertEquals(0, metrics.systemGestureInsetRight);
    assertEquals(0, metrics.systemGestureInsetBottom);
  }

  @Test
  @Config(sdk = API_LEVELS.API_29)
  @SuppressWarnings("deprecation")
  public void systemGestureInsets_api29() {
    setUpViewAndEngine();

    WindowInsets windowInsets = mock(WindowInsets.class);
    when(windowInsets.getSystemWindowInsetLeft()).thenReturn(0);
    when(windowInsets.getSystemWindowInsetTop()).thenReturn(0);
    when(windowInsets.getSystemWindowInsetRight()).thenReturn(0);
    when(windowInsets.getSystemWindowInsetBottom()).thenReturn(0);
    when(windowInsets.getSystemGestureInsets()).thenReturn(Insets.of(10, 20, 30, 40));

    flutterView.onApplyWindowInsets(windowInsets);

    ArgumentCaptor<FlutterRenderer.ViewportMetrics> viewportMetricsCaptor =
        ArgumentCaptor.forClass(FlutterRenderer.ViewportMetrics.class);
    verify(flutterRenderer, atLeastOnce()).setViewportMetrics(viewportMetricsCaptor.capture());

    FlutterRenderer.ViewportMetrics metrics = viewportMetricsCaptor.getValue();
    assertEquals(10, metrics.systemGestureInsetLeft);
    assertEquals(20, metrics.systemGestureInsetTop);
    assertEquals(30, metrics.systemGestureInsetRight);
    assertEquals(40, metrics.systemGestureInsetBottom);
  }

  @Test
  @Config(sdk = API_LEVELS.API_30)
  @SuppressWarnings("deprecation")
  public void systemGestureInsets_api30() {
    setUpViewAndEngine();

    WindowInsets windowInsets =
        new WindowInsets.Builder()
            .setInsets(android.view.WindowInsets.Type.systemGestures(), Insets.of(15, 25, 35, 45))
            .build();

    flutterView.onApplyWindowInsets(windowInsets);

    ArgumentCaptor<FlutterRenderer.ViewportMetrics> viewportMetricsCaptor =
        ArgumentCaptor.forClass(FlutterRenderer.ViewportMetrics.class);
    verify(flutterRenderer, atLeastOnce()).setViewportMetrics(viewportMetricsCaptor.capture());

    FlutterRenderer.ViewportMetrics metrics = viewportMetricsCaptor.getValue();
    assertEquals(15, metrics.systemGestureInsetLeft);
    assertEquals(25, metrics.systemGestureInsetTop);
    assertEquals(35, metrics.systemGestureInsetRight);
    assertEquals(45, metrics.systemGestureInsetBottom);
  }
}
