// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.embedding.engine.FlutterJNI;
import io.flutter.embedding.engine.loader.FlutterLoader;
import io.flutter.embedding.engine.renderer.FlutterRenderer;
import io.flutter.embedding.engine.renderer.FlutterUiDisplayListener;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@RunWith(AndroidJUnit4.class)
public class FlutterViewRevertImageViewReproductionTest {
  private final Context ctx = ApplicationProvider.getApplicationContext();
  @Mock FlutterJNI mockFlutterJni;
  @Mock FlutterLoader mockFlutterLoader;

  @Before
  public void setUp() {
    MockitoAnnotations.openMocks(this);
    when(mockFlutterJni.isAttached()).thenReturn(true);
  }

  @Test
  public void revertImageView_waitsForNewFrameBeforeDetaching_whenAlreadyDisplayingUi() {
    FlutterEngine flutterEngine = spy(new FlutterEngine(ctx, mockFlutterLoader, mockFlutterJni));
    FlutterRenderer flutterRenderer = flutterEngine.getRenderer();

    // 1. Capture the internal listener that FlutterRenderer registers with FlutterJNI,
    // and trigger it to set flutterRenderer.isDisplayingFlutterUi to true.
    ArgumentCaptor<FlutterUiDisplayListener> listenerCaptor =
        ArgumentCaptor.forClass(FlutterUiDisplayListener.class);
    verify(mockFlutterJni).addIsDisplayingFlutterUiListener(listenerCaptor.capture());
    FlutterUiDisplayListener rendererListener = listenerCaptor.getValue();
    rendererListener.onFlutterUiDisplayed();
    assertTrue(flutterRenderer.isDisplayingFlutterUi());

    // 2. Set up the FlutterView and convert it to ImageView.
    FlutterImageView imageViewMock = mock(FlutterImageView.class);
    when(imageViewMock.getAttachedRenderer()).thenReturn(flutterRenderer);

    FlutterView flutterView = spy(new FlutterView(ctx));
    when(flutterView.createImageView()).thenReturn(imageViewMock);

    flutterView.attachToFlutterEngine(flutterEngine);
    flutterView.convertToImageView();
    assertTrue(flutterView.renderSurface instanceof FlutterImageView);

    // 3. Call revertImageView and check if onDone is called synchronously.
    boolean[] onDoneCalled = {false};
    flutterView.revertImageView(() -> onDoneCalled[0] = true);

    // Under the bug, onDone will be called immediately/synchronously, and the imageViewMock
    // will be immediately detached.
    // Assert that it should NOT be called immediately (reproducing the bug).
    assertFalse("onDone should not be called immediately", onDoneCalled[0]);
    verify(imageViewMock, never()).detachFromRenderer();

    // 4. VERIFY EVENTUAL SUCCESS: Simulate the arrival of a new frame.
    // Capture the second listener registered during revertImageView.
    ArgumentCaptor<FlutterUiDisplayListener> revertListenerCaptor =
        ArgumentCaptor.forClass(FlutterUiDisplayListener.class);
    verify(mockFlutterJni, times(3)).addIsDisplayingFlutterUiListener(revertListenerCaptor.capture());

    // The third captured value (index 2) is the listener registered inside revertImageView.
    FlutterUiDisplayListener revertListener = revertListenerCaptor.getAllValues().get(2);

    // Trigger the listener to simulate the new frame being rendered on the resumed surface.
    revertListener.onFlutterUiDisplayed();

    // Verify that the cleanup callback is now successfully executed and the image view is detached.
    assertTrue("onDone should be called after a new frame is rendered", onDoneCalled[0]);
    verify(imageViewMock, times(1)).detachFromRenderer();
  }
}
