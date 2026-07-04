// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine;

import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import android.app.Activity;
import androidx.lifecycle.Lifecycle;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.android.ExclusiveAppComponent;
import io.flutter.embedding.android.FlutterView;
import io.flutter.embedding.engine.loader.FlutterLoader;
import io.flutter.plugin.platform.PlatformViewsController;
import io.flutter.plugin.platform.PlatformViewsController2;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

@RunWith(AndroidJUnit4.class)
public class FlutterEngineUIAttachmentConsistencyTest {
  @Mock FlutterJNI mockFlutterJni;
  @Mock FlutterLoader mockFlutterLoader;
  @Mock PlatformViewsController platformViewsController;
  @Mock PlatformViewsController2 platformViewsController2;
  @Mock Lifecycle mockLifecycle;

  @Before
  public void setUp() {
    MockitoAnnotations.openMocks(this);
    when(mockFlutterJni.isAttached()).thenReturn(true);
  }

  @Test
  public void attachingFlutterViewToEngineWithMismatchedActivityThrowsException() {
    Activity activityA = Robolectric.buildActivity(Activity.class).get();
    Activity activityB = Robolectric.buildActivity(Activity.class).get();

    FlutterEngine flutterEngine =
        spy(new FlutterEngine(activityA, mockFlutterLoader, mockFlutterJni));
    when(flutterEngine.getPlatformViewsController()).thenReturn(platformViewsController);
    when(flutterEngine.getPlatformViewsController2()).thenReturn(platformViewsController2);

    @SuppressWarnings("unchecked")
    ExclusiveAppComponent<Activity> exclusiveActivityA = mock(ExclusiveAppComponent.class);
    when(exclusiveActivityA.getAppComponent()).thenReturn(activityA);

    // Track 1: Register Activity A's lifecycle via ActivityControlSurface.
    flutterEngine.getActivityControlSurface().attachToActivity(exclusiveActivityA, mockLifecycle);

    // Track 2: Attempt to register View B's lifecycle (hosted in Activity B) via
    // attachToFlutterEngine. Currently, nothing enforces that these two parallel tracks apply
    // against the same activity/view group.
    FlutterView flutterViewB = new FlutterView(activityB);

    // This should throw an IllegalStateException (or AssertionError) enforcing consistency between
    // ActivityControlSurface and FlutterView, but currently fails due to lack of assertions.
    assertThrows(
        IllegalStateException.class, () -> flutterViewB.attachToFlutterEngine(flutterEngine));
  }

  @Test
  public void attachingActivityToEngineWithMismatchedFlutterViewThrowsException() {
    Activity activityA = Robolectric.buildActivity(Activity.class).get();
    Activity activityB = Robolectric.buildActivity(Activity.class).get();

    FlutterEngine flutterEngine =
        spy(new FlutterEngine(activityA, mockFlutterLoader, mockFlutterJni));
    when(flutterEngine.getPlatformViewsController()).thenReturn(platformViewsController);
    when(flutterEngine.getPlatformViewsController2()).thenReturn(platformViewsController2);

    // Track 2: Register View A's lifecycle (hosted in Activity A) via attachToFlutterEngine.
    FlutterView flutterViewA = new FlutterView(activityA);
    flutterViewA.attachToFlutterEngine(flutterEngine);

    // Track 1: Attempt to register Activity B's lifecycle via ActivityControlSurface while View A
    // is attached. Currently, nothing enforces that these two parallel tracks apply against the
    // same activity/view group.
    @SuppressWarnings("unchecked")
    ExclusiveAppComponent<Activity> exclusiveActivityB = mock(ExclusiveAppComponent.class);
    when(exclusiveActivityB.getAppComponent()).thenReturn(activityB);

    // This should throw an IllegalStateException (or AssertionError) enforcing consistency between
    // ActivityControlSurface and FlutterView, but currently fails due to lack of assertions.
    assertThrows(
        IllegalStateException.class,
        () ->
            flutterEngine
                .getActivityControlSurface()
                .attachToActivity(exclusiveActivityB, mockLifecycle));
  }
}
