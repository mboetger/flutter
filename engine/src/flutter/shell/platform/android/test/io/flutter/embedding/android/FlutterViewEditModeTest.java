// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;

import android.content.Context;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

@RunWith(AndroidJUnit4.class)
public class FlutterViewEditModeTest {

  @Test
  public void testViewInEditModeBypassesInit() {
    // Use a spy on a real Robolectric context to avoid NPEs in View/FrameLayout constructors,
    // while still simulating the Layout Editor environment where getApplicationContext() is null.
    Context context = ApplicationProvider.getApplicationContext();
    Context spyContext = spy(context);
    doReturn(null).when(spyContext).getApplicationContext();

    // Create a subclassed FlutterSurfaceView that also returns true for isInEditMode.
    // This is necessary because the FlutterView constructor will instantiate a FlutterSurfaceView,
    // which would otherwise crash when attempting initialization.
    FlutterSurfaceView editModeSurfaceView = new FlutterSurfaceViewInEditMode(spyContext);

    // Instantiate the subclassed FlutterView with the edit-mode surface view.
    FlutterView flutterView = new FlutterViewInEditMode(spyContext, editModeSurfaceView);

    // Verify that the view is in edit mode.
    assertTrue(flutterView.isInEditMode());

    // Verify that the initialization was actually bypassed (no children added).
    assertEquals(0, flutterView.getChildCount());
  }

  private static class FlutterSurfaceViewInEditMode extends FlutterSurfaceView {
    public FlutterSurfaceViewInEditMode(Context context) {
      super(context);
    }

    @Override
    public boolean isInEditMode() {
      return true;
    }
  }

  private static class FlutterViewInEditMode extends FlutterView {
    public FlutterViewInEditMode(Context context, FlutterSurfaceView surfaceView) {
      super(context, surfaceView);
    }

    @Override
    public boolean isInEditMode() {
      return true;
    }
  }

  @Test
  public void testContentSizingFlag_nullContext() {
    assertTrue(!ContentSizingFlag.isEnabled(null));
  }

  @Test
  public void testContentSizingFlag_nullApplicationContext() {
    Context context = spy(ApplicationProvider.getApplicationContext());
    doReturn(null).when(context).getApplicationContext();
    assertTrue(!ContentSizingFlag.isEnabled(context));
  }

  @Test
  public void testContentSizingFlag_nullPackageManager() {
    Context context = spy(ApplicationProvider.getApplicationContext());
    Context spyContext = spy(context.getApplicationContext());
    doReturn(spyContext).when(context).getApplicationContext();
    doReturn(null).when(spyContext).getPackageManager();
    assertTrue(!ContentSizingFlag.isEnabled(context));
  }

  @Test
  public void testTextureViewInEditModeBypassesInit() {
    Context context = ApplicationProvider.getApplicationContext();
    Context spyContext = spy(context);
    doReturn(null).when(spyContext).getApplicationContext();

    FlutterTextureView editModeTextureView = new FlutterTextureViewInEditMode(spyContext);
    assertTrue(editModeTextureView.isInEditMode());
  }

  @Test
  public void testImageViewInEditModeBypassesInit() {
    Context context = ApplicationProvider.getApplicationContext();
    Context spyContext = spy(context);
    doReturn(null).when(spyContext).getApplicationContext();

    FlutterImageView editModeImageView = new FlutterImageViewInEditMode(spyContext);
    assertTrue(editModeImageView.isInEditMode());
  }

  private static class FlutterTextureViewInEditMode extends FlutterTextureView {
    public FlutterTextureViewInEditMode(Context context) {
      super(context);
    }

    @Override
    public boolean isInEditMode() {
      return true;
    }
  }

  private static class FlutterImageViewInEditMode extends FlutterImageView {
    public FlutterImageViewInEditMode(Context context) {
      super(context);
    }

    @Override
    public boolean isInEditMode() {
      return true;
    }
  }
}
