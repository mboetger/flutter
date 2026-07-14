// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.plugin.platform;

import static io.flutter.Build.API_LEVELS;
import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import android.annotation.TargetApi;
import android.content.Context;
import android.hardware.display.DisplayManager;
import android.view.Display;
import android.view.inputmethod.InputMethodManager;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.Build.API_LEVELS;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowDisplayManager;

@RunWith(AndroidJUnit4.class)
@TargetApi(API_LEVELS.API_28)
public class SingleViewPresentationTest {
  @Test
  @Config(minSdk = API_LEVELS.FLUTTER_MIN, maxSdk = API_LEVELS.API_30)
  public void returnsOuterContextInputMethodManager() {
    // There's a bug in Android Q caused by the IMM being instanced per display.
    // https://github.com/flutter/flutter/issues/38375. We need the context returned by
    // SingleViewPresentation to be consistent from its instantiation instead of defaulting to
    // what the system would have returned at call time.

    // It's not possible to set up the exact same conditions as the unit test in the bug here,
    // but we can make sure that we're wrapping the Context passed in at instantiation time and
    // returning the same InputMethodManager from it. This test passes in a Spy context instance
    // that initially returns a mock. Without the bugfix this test falls back to Robolectric's
    // system service instead of the spy's and fails.

    // Create an SVP under test with a Context that returns a local IMM mock.
    Context context = spy(ApplicationProvider.getApplicationContext());
    InputMethodManager expected = mock(InputMethodManager.class);
    when(context.getSystemService(Context.INPUT_METHOD_SERVICE)).thenReturn(expected);
    DisplayManager dm = (DisplayManager) context.getSystemService(Context.DISPLAY_SERVICE);
    SingleViewPresentation svp =
        new SingleViewPresentation(context, dm.getDisplay(0), null, null, null, false);

    // Get the IMM from the SVP's context.
    InputMethodManager actual =
        (InputMethodManager) svp.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);

    // This should be the mocked instance from construction, not the IMM from the greater
    // Android OS (or Robolectric's shadow, in this case).
    assertEquals(expected, actual);
  }

  @Test
  @Config(minSdk = API_LEVELS.FLUTTER_MIN, maxSdk = API_LEVELS.API_30)
  public void returnsOuterContextInputMethodManager_createDisplayContext() {
    // The IMM should also persist across display contexts created from the base context.

    // Create an SVP under test with a Context that returns a local IMM mock.
    Context context = spy(ApplicationProvider.getApplicationContext());
    InputMethodManager expected = mock(InputMethodManager.class);
    when(context.getSystemService(Context.INPUT_METHOD_SERVICE)).thenReturn(expected);
    Display display =
        ((DisplayManager) context.getSystemService(Context.DISPLAY_SERVICE)).getDisplay(0);
    SingleViewPresentation svp =
        new SingleViewPresentation(context, display, null, null, null, false);

    // Get the IMM from the SVP's context.
    InputMethodManager actual =
        (InputMethodManager)
            svp.getContext()
                .createDisplayContext(display)
                .getSystemService(Context.INPUT_METHOD_SERVICE);

    // This should be the mocked instance from construction, not the IMM from the greater
    // Android OS (or Robolectric's shadow, in this case).
    assertEquals(expected, actual);
  }

  @Test
  @Config(sdk = API_LEVELS.API_31)
  public void immContext_returnsDisplayContextInputMethodManagerOnApi31() throws Exception {
    // Create outer context (display 0)
    Context outerContext = spy(ApplicationProvider.getApplicationContext());
    InputMethodManager immDisplay0 = mock(InputMethodManager.class);
    when(outerContext.getSystemService(Context.INPUT_METHOD_SERVICE)).thenReturn(immDisplay0);

    // Get a real Display
    DisplayManager displayManager =
        (DisplayManager) outerContext.getSystemService(Context.DISPLAY_SERVICE);
    ShadowDisplayManager shadowDisplayManager = Shadows.shadowOf(displayManager);
    int displayId = shadowDisplayManager.addDisplay("w1024dp-h768dp");
    Display display = displayManager.getDisplay(displayId);

    // Create display context (display 1)
    Context realDisplayContext = outerContext.createDisplayContext(display);
    Context displayContextSpy = spy(realDisplayContext);
    InputMethodManager immDisplay1 = mock(InputMethodManager.class);
    when(displayContextSpy.getSystemService(Context.INPUT_METHOD_SERVICE)).thenReturn(immDisplay1);

    // Mock createDisplayContext to return the spy displayContext
    when(outerContext.createDisplayContext(display)).thenReturn(displayContextSpy);

    // Instantiate ImmContext using reflection
    Class<?> immContextClass =
        Class.forName("io.flutter.plugin.platform.SingleViewPresentation$ImmContext");
    java.lang.reflect.Constructor<?> constructor =
        immContextClass.getDeclaredConstructor(Context.class);
    constructor.setAccessible(true);
    Context immContext = (Context) constructor.newInstance(outerContext);

    // Call createDisplayContext on ImmContext
    Context displayImmContext = immContext.createDisplayContext(display);

    // Call getSystemService on the returned display context
    InputMethodManager actual =
        (InputMethodManager) displayImmContext.getSystemService(Context.INPUT_METHOD_SERVICE);

    // On API 31+, we expect the display context's IMM.
    assertEquals(immDisplay1, actual);
  }

  @Test
  @Config(sdk = API_LEVELS.API_30)
  public void immContext_returnsOuterContextInputMethodManagerOnApi30() throws Exception {
    // Create outer context (display 0)
    Context outerContext = spy(ApplicationProvider.getApplicationContext());
    InputMethodManager immDisplay0 = mock(InputMethodManager.class);
    when(outerContext.getSystemService(Context.INPUT_METHOD_SERVICE)).thenReturn(immDisplay0);

    // Get a real Display
    DisplayManager displayManager =
        (DisplayManager) outerContext.getSystemService(Context.DISPLAY_SERVICE);
    ShadowDisplayManager shadowDisplayManager = Shadows.shadowOf(displayManager);
    int displayId = shadowDisplayManager.addDisplay("w1024dp-h768dp");
    Display display = displayManager.getDisplay(displayId);

    // Create display context (display 1)
    Context realDisplayContext = outerContext.createDisplayContext(display);
    Context displayContextSpy = spy(realDisplayContext);
    InputMethodManager immDisplay1 = mock(InputMethodManager.class);
    when(displayContextSpy.getSystemService(Context.INPUT_METHOD_SERVICE)).thenReturn(immDisplay1);

    // Mock createDisplayContext to return the spy displayContext
    when(outerContext.createDisplayContext(display)).thenReturn(displayContextSpy);

    // Instantiate ImmContext using reflection
    Class<?> immContextClass =
        Class.forName("io.flutter.plugin.platform.SingleViewPresentation$ImmContext");
    java.lang.reflect.Constructor<?> constructor =
        immContextClass.getDeclaredConstructor(Context.class);
    constructor.setAccessible(true);
    Context immContext = (Context) constructor.newInstance(outerContext);

    // Call createDisplayContext on ImmContext
    Context displayImmContext = immContext.createDisplayContext(display);

    // Call getSystemService on the returned display context
    InputMethodManager actual =
        (InputMethodManager) displayImmContext.getSystemService(Context.INPUT_METHOD_SERVICE);

    // On API 30, we expect the outer context's IMM.
    assertEquals(immDisplay0, actual);
  }
}

