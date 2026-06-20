// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.view.View;
import android.view.ViewTreeObserver;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

@RunWith(AndroidJUnit4.class)
public class FlutterActivityFocusReproductionTest {
  private final Context ctx = ApplicationProvider.getApplicationContext();

  @Test
  public void testFlutterActivity_onWindowFocusChanged_propagatesToDelegate() {
    Intent intent = FlutterActivity.createDefaultIntent(ctx);
    ActivityController<FlutterActivity> activityController =
        Robolectric.buildActivity(FlutterActivity.class, intent);
    FlutterActivity activity = activityController.get();

    FlutterActivityAndFragmentDelegate mockDelegate = mock(FlutterActivityAndFragmentDelegate.class);
    when(mockDelegate.isAttached()).thenReturn(true);
    activity.setDelegate(mockDelegate);

    // Simulate window focus change on the Activity
    activity.onWindowFocusChanged(false);

    // Verify that it is propagated to the delegate
    verify(mockDelegate, times(1)).onWindowFocusChanged(false);

    activity.onWindowFocusChanged(true);
    verify(mockDelegate, times(1)).onWindowFocusChanged(true);
  }

  @Test
  public void testFlutterFragment_onWindowFocusChanged_propagatesToDelegate() {
    // For FlutterFragment, the window focus change listener is registered on the view tree observer.
    FlutterFragment fragment = new FlutterFragment();
    FlutterActivityAndFragmentDelegate mockDelegate = mock(FlutterActivityAndFragmentDelegate.class);
    when(mockDelegate.isAttached()).thenReturn(true);
    fragment.delegate = mockDelegate;

    View mockView = mock(View.class);
    ViewTreeObserver mockObserver = mock(ViewTreeObserver.class);
    when(mockView.getViewTreeObserver()).thenReturn(mockObserver);

    // Simulate onViewCreated
    fragment.onViewCreated(mockView, null);

    // Capture the registered listener
    org.mockito.ArgumentCaptor<ViewTreeObserver.OnWindowFocusChangeListener> listenerCaptor =
        org.mockito.ArgumentCaptor.forClass(ViewTreeObserver.OnWindowFocusChangeListener.class);
    verify(mockObserver, times(1)).addOnWindowFocusChangeListener(listenerCaptor.capture());

    ViewTreeObserver.OnWindowFocusChangeListener listener = listenerCaptor.getValue();

    // Trigger focus change event on the listener
    listener.onWindowFocusChanged(false);
    verify(mockDelegate, times(1)).onWindowFocusChanged(false);

    listener.onWindowFocusChanged(true);
    verify(mockDelegate, times(1)).onWindowFocusChanged(true);
  }
}
