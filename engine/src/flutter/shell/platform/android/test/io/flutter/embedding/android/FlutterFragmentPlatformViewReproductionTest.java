// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.SurfaceView;
import androidx.fragment.app.FragmentActivity;
import androidx.test.core.app.ActivityScenario;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.embedding.engine.FlutterEngineCache;
import io.flutter.embedding.engine.FlutterJNI;
import io.flutter.embedding.engine.loader.FlutterLoader;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class FlutterFragmentPlatformViewReproductionTest {
  private static final String CACHED_ENGINE_ID = "my_cached_engine";
  private final Context ctx = ApplicationProvider.getApplicationContext();
  private FlutterJNI mockFlutterJNI;
  private FlutterEngine flutterEngine;

  @Before
  public void setUp() {
    mockFlutterJNI = mock(FlutterJNI.class);
    when(mockFlutterJNI.isAttached()).thenReturn(true);
    flutterEngine = new FlutterEngine(ctx, mock(FlutterLoader.class), mockFlutterJNI, null, false);
    FlutterEngineCache.getInstance().put(CACHED_ENGINE_ID, flutterEngine);
  }

  @After
  public void tearDown() {
    // Clean up static state and destroy the engine to prevent resource leaks
    // and side-effects on other Robolectric tests running in the same JVM.
    FlutterEngineCache.getInstance().remove(CACHED_ENGINE_ID);
    if (flutterEngine != null) {
      flutterEngine.destroy();
      flutterEngine = null;
    }
  }

  @Test
  public void testTransparentTransparencyModeSetsZOrderOnTopByDefault() {
    // Under transparent mode (default), the fragment should request setZOrderOnTop(true)
    // by default to ensure the activity background shows through the transparent parts.
    FlutterFragment fragment =
        FlutterFragment.withCachedEngine(CACHED_ENGINE_ID)
            .transparencyMode(TransparencyMode.transparent)
            .build();

    try (ActivityScenario<FragmentActivity> scenario =
        ActivityScenario.launch(FragmentActivity.class)) {
      scenario.onActivity(
          activity -> {
            activity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(android.R.id.content, fragment)
                .commitNow();

            FlutterView flutterView = (FlutterView) fragment.getView();
            assertTrue(flutterView.renderSurface instanceof FlutterSurfaceView);
            FlutterSurfaceView surfaceView = (FlutterSurfaceView) flutterView.renderSurface;

            try {
              int requestedSubLayer = getRequestedSubLayer(surfaceView);
              assertEquals(
                  "Expected transparent mode to set Z-order on top by default (sub-layer should be 1) for backward compatibility",
                  1,
                  requestedSubLayer
              );
            } catch (Exception e) {
              throw new RuntimeException(e);
            }
          });
    }
  }

  @Test
  public void testTransparentTransparencyModeWithZOrderOnTopFalse() {
    // Under transparent mode with zOrderOnTop(false), the fragment should NOT request setZOrderOnTop(true).
    // This places the FlutterSurfaceView's surface window behind the parent window, preventing it
    // from covering native PlatformViews in the parent window.
    FlutterFragment fragment =
        FlutterFragment.withCachedEngine(CACHED_ENGINE_ID)
            .transparencyMode(TransparencyMode.transparent)
            .zOrderOnTop(false)
            .build();

    try (ActivityScenario<FragmentActivity> scenario =
        ActivityScenario.launch(FragmentActivity.class)) {
      scenario.onActivity(
          activity -> {
            activity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(android.R.id.content, fragment)
                .commitNow();

            FlutterView flutterView = (FlutterView) fragment.getView();
            assertTrue(flutterView.renderSurface instanceof FlutterSurfaceView);
            FlutterSurfaceView surfaceView = (FlutterSurfaceView) flutterView.renderSurface;

            try {
              int requestedSubLayer = getRequestedSubLayer(surfaceView);
              assertEquals(
                  "Expected transparent mode with zOrderOnTop(false) to not set Z-order on top (sub-layer should be -2)",
                  -2,
                  requestedSubLayer
              );
            } catch (Exception e) {
              throw new RuntimeException(e);
            }
          });
    }
  }

  @Test
  public void testOpaqueTransparencyModeDoesNotSetZOrderOnTop() {
    // Under opaque mode, the fragment does NOT call setZOrderOnTop(true).
    // This keeps the FlutterSurfaceView behind the window (sub-layer is -2), allowing
    // native PlatformViews to render on top of it in the normal window layer.
    // This test passes, validating that the opaque mode workaround behaves correctly.
    FlutterFragment fragment =
        FlutterFragment.withCachedEngine(CACHED_ENGINE_ID)
            .transparencyMode(TransparencyMode.opaque)
            .build();

    try (ActivityScenario<FragmentActivity> scenario =
        ActivityScenario.launch(FragmentActivity.class)) {
      scenario.onActivity(
          activity -> {
            activity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(android.R.id.content, fragment)
                .commitNow();

            FlutterView flutterView = (FlutterView) fragment.getView();
            assertTrue(flutterView.renderSurface instanceof FlutterSurfaceView);
            FlutterSurfaceView surfaceView = (FlutterSurfaceView) flutterView.renderSurface;

            try {
              int requestedSubLayer = getRequestedSubLayer(surfaceView);
              assertEquals(
                  "Expected requested sub-layer to be negative (-2) when opaque",
                  -2,
                  requestedSubLayer
              );
            } catch (Exception e) {
              throw new RuntimeException(e);
            }
          });
    }
  }

  /**
   * Helper method using reflection to access the private 'mRequestedSubLayer' field
   * in SurfaceView. Since Android's SurfaceView does not expose a public getter for the
   * Z-order or sub-layer state, reflection is required to inspect this internal layout state.
   */
  private int getRequestedSubLayer(SurfaceView surfaceView) throws Exception {
    java.lang.reflect.Field field = SurfaceView.class.getDeclaredField("mRequestedSubLayer");
    field.setAccessible(true);
    return (int) field.get(surfaceView);
  }
}
