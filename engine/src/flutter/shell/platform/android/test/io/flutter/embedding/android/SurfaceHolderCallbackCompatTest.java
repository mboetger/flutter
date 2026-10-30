// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.view.SurfaceHolder;
import io.flutter.Build.API_LEVELS;
import io.flutter.embedding.engine.renderer.FlutterRenderer;
import io.flutter.embedding.engine.renderer.FlutterUiDisplayListener;
import java.util.concurrent.TimeUnit;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

@RunWith(RobolectricTestRunner.class)
public class SurfaceHolderCallbackCompatTest {

  @Test
  @Config(minSdk = API_LEVELS.FLUTTER_MIN, maxSdk = API_LEVELS.API_25)
  public void onAttachToRendererShouldRemoveListenerBelowApi26() {
    FlutterSurfaceView fakeSurfaceView = mock(FlutterSurfaceView.class);
    FlutterRenderer fakeFlutterRenderer = mock(FlutterRenderer.class);
    SurfaceHolderCallbackCompat test =
        new SurfaceHolderCallbackCompat(null, fakeSurfaceView, fakeFlutterRenderer);
    test.onAttachToRenderer(null);
    verify(fakeFlutterRenderer, times(1)).removeIsDisplayingFlutterUiListener(test.alphaCallback);
  }

  @Test
  @Config(minSdk = API_LEVELS.API_26)
  public void onAttachToRendererShouldNotRemoveListenerApi26OrAbove() {
    FlutterSurfaceView fakeSurfaceView = mock(FlutterSurfaceView.class);
    FlutterRenderer fakeFlutterRenderer = mock(FlutterRenderer.class);
    SurfaceHolderCallbackCompat test =
        new SurfaceHolderCallbackCompat(null, fakeSurfaceView, fakeFlutterRenderer);
    test.onAttachToRenderer(null);
    verify(fakeFlutterRenderer, never()).removeIsDisplayingFlutterUiListener(test.alphaCallback);
  }

  @Test
  @Config(minSdk = API_LEVELS.FLUTTER_MIN, maxSdk = API_LEVELS.API_25)
  public void onResumeShouldAddListenerBelowApi26() {
    FlutterSurfaceView fakeSurfaceView = mock(FlutterSurfaceView.class);
    FlutterRenderer fakeFlutterRenderer = mock(FlutterRenderer.class);
    SurfaceHolderCallbackCompat test =
        new SurfaceHolderCallbackCompat(null, fakeSurfaceView, fakeFlutterRenderer);
    test.onResume();
    verify(fakeFlutterRenderer, times(1)).addIsDisplayingFlutterUiListener(test.alphaCallback);
  }

  @Test
  @Config(minSdk = API_LEVELS.API_26)
  public void onResumeShouldAddListenerOnApi26OrAbove() {
    FlutterSurfaceView fakeSurfaceView = mock(FlutterSurfaceView.class);
    FlutterRenderer fakeFlutterRenderer = mock(FlutterRenderer.class);
    SurfaceHolderCallbackCompat test =
        new SurfaceHolderCallbackCompat(null, fakeSurfaceView, fakeFlutterRenderer);
    test.onResume();
    verify(fakeFlutterRenderer, never()).addIsDisplayingFlutterUiListener(test.alphaCallback);
  }

  @Test
  @Config(minSdk = API_LEVELS.FLUTTER_MIN, maxSdk = API_LEVELS.API_25)
  public void onDetachFromRendererShouldRemoveListenerAndSetAlphaBelowApi26() {
    FlutterSurfaceView fakeSurfaceView = mock(FlutterSurfaceView.class);
    FlutterRenderer fakeFlutterRenderer = mock(FlutterRenderer.class);
    SurfaceHolderCallbackCompat test =
        new SurfaceHolderCallbackCompat(null, fakeSurfaceView, fakeFlutterRenderer);
    test.onDetachFromRenderer();
    verify(fakeFlutterRenderer, times(1)).removeIsDisplayingFlutterUiListener(test.alphaCallback);
    verify(fakeSurfaceView, times(2)).setAlpha(0.0f);
  }

  @Test
  @Config(minSdk = API_LEVELS.API_26)
  public void onDetachFromRendererShouldNotRemoveListenerOnApi26OrAbove() {
    FlutterSurfaceView fakeSurfaceView = mock(FlutterSurfaceView.class);
    FlutterRenderer fakeFlutterRenderer = mock(FlutterRenderer.class);
    SurfaceHolderCallbackCompat test =
        new SurfaceHolderCallbackCompat(null, fakeSurfaceView, fakeFlutterRenderer);
    test.onDetachFromRenderer();
    verify(fakeFlutterRenderer, never()).removeIsDisplayingFlutterUiListener(test.alphaCallback);
  }

  @Test
  @Config(minSdk = API_LEVELS.FLUTTER_MIN, maxSdk = API_LEVELS.API_25)
  public void alphaCallbackShouldSetAlphaOnSurfaceViewBelowApi26() {
    FlutterSurfaceView fakeSurfaceView = mock(FlutterSurfaceView.class);
    FlutterRenderer fakeFlutterRenderer = mock(FlutterRenderer.class);
    SurfaceHolderCallbackCompat test =
        new SurfaceHolderCallbackCompat(null, fakeSurfaceView, fakeFlutterRenderer);
    test.alphaCallback.onFlutterUiDisplayed();
    verify(fakeSurfaceView, times(1)).setAlpha(0.0f);
    verify(fakeSurfaceView, times(1)).setAlpha(1.0f);
    verify(fakeFlutterRenderer, times(1)).removeIsDisplayingFlutterUiListener(test.alphaCallback);
  }

  @Test
  @Config(minSdk = API_LEVELS.FLUTTER_MIN)
  public void testSurfaceHolderCallbackPassesThroughToInnerCallback() {
    FlutterSurfaceView fakeSurfaceView = mock(FlutterSurfaceView.class);
    FlutterRenderer fakeFlutterRenderer = mock(FlutterRenderer.class);
    SurfaceHolder.Callback innerCallback = mock(SurfaceHolder.Callback.class);
    SurfaceHolderCallbackCompat test =
        new SurfaceHolderCallbackCompat(innerCallback, fakeSurfaceView, fakeFlutterRenderer);
    test.surfaceCreated(null);
    verify(innerCallback, times(1)).surfaceCreated(null);
    test.surfaceChanged(null, 0, 0, 0);
    verify(innerCallback, times(1)).surfaceChanged(null, 0, 0, 0);
    test.surfaceDestroyed(null);
    verify(innerCallback, times(1)).surfaceDestroyed(null);
  }

  @Test
  @Config(minSdk = API_LEVELS.API_26)
  public void surfaceRedrawNeededAsyncShouldRunFinishDrawingWhenRendererIsNull() {
    FlutterSurfaceView fakeSurfaceView = mock(FlutterSurfaceView.class);
    SurfaceHolder fakeSurfaceHolder = mock(SurfaceHolder.class);
    Runnable finishDrawing = mock(Runnable.class);
    SurfaceHolderCallbackCompat test = new SurfaceHolderCallbackCompat(null, fakeSurfaceView, null);

    test.surfaceRedrawNeededAsync(fakeSurfaceHolder, finishDrawing);

    verify(finishDrawing, times(1)).run();
  }

  @Test
  @Config(minSdk = API_LEVELS.API_26)
  public void surfaceRedrawNeededAsyncShouldTimeOutAndRunFinishDrawingOnceWhenUiNotDisplayed() {
    FlutterSurfaceView fakeSurfaceView = mock(FlutterSurfaceView.class);
    FlutterRenderer fakeFlutterRenderer = mock(FlutterRenderer.class);
    SurfaceHolder fakeSurfaceHolder = mock(SurfaceHolder.class);
    Runnable finishDrawing = mock(Runnable.class);
    SurfaceHolderCallbackCompat test =
        new SurfaceHolderCallbackCompat(null, fakeSurfaceView, fakeFlutterRenderer);

    test.surfaceRedrawNeededAsync(fakeSurfaceHolder, finishDrawing);

    ArgumentCaptor<FlutterUiDisplayListener> listenerCaptor =
        ArgumentCaptor.forClass(FlutterUiDisplayListener.class);
    verify(fakeFlutterRenderer, times(1))
        .addIsDisplayingFlutterUiListener(listenerCaptor.capture());
    verify(finishDrawing, never()).run();

    // Before the 1000ms timeout elapses, finishDrawing must still be waiting on the frame.
    ShadowLooper.idleMainLooper(
        SurfaceHolderCallbackCompat.FINISH_DRAWING_TIMEOUT_MS - 1, TimeUnit.MILLISECONDS);
    verify(finishDrawing, never()).run();

    // At 1000ms, the timeout fires, runs finishDrawing once, and unregisters the listener.
    ShadowLooper.idleMainLooper(1, TimeUnit.MILLISECONDS);
    verify(finishDrawing, times(1)).run();
    verify(fakeFlutterRenderer, times(1))
        .removeIsDisplayingFlutterUiListener(listenerCaptor.getValue());

    // A late frame arriving after the timeout must not invoke finishDrawing a second time.
    listenerCaptor.getValue().onFlutterUiDisplayed();
    verify(finishDrawing, times(1)).run();
  }

  @Test
  @Config(minSdk = API_LEVELS.API_26)
  public void surfaceRedrawNeededAsyncShouldRunFinishDrawingOnceWhenUiDisplayedBeforeTimeout() {
    FlutterSurfaceView fakeSurfaceView = mock(FlutterSurfaceView.class);
    FlutterRenderer fakeFlutterRenderer = mock(FlutterRenderer.class);
    SurfaceHolder fakeSurfaceHolder = mock(SurfaceHolder.class);
    Runnable finishDrawing = mock(Runnable.class);
    SurfaceHolderCallbackCompat test =
        new SurfaceHolderCallbackCompat(null, fakeSurfaceView, fakeFlutterRenderer);

    test.surfaceRedrawNeededAsync(fakeSurfaceHolder, finishDrawing);

    ArgumentCaptor<FlutterUiDisplayListener> listenerCaptor =
        ArgumentCaptor.forClass(FlutterUiDisplayListener.class);
    verify(fakeFlutterRenderer, times(1))
        .addIsDisplayingFlutterUiListener(listenerCaptor.capture());
    verify(finishDrawing, never()).run();

    listenerCaptor.getValue().onFlutterUiDisplayed();
    verify(finishDrawing, times(1)).run();
    verify(fakeFlutterRenderer, times(1))
        .removeIsDisplayingFlutterUiListener(listenerCaptor.getValue());

    // Advancing past the 1000ms timeout must not invoke finishDrawing a second time.
    ShadowLooper.idleMainLooper(
        SurfaceHolderCallbackCompat.FINISH_DRAWING_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    verify(finishDrawing, times(1)).run();
  }

  @Test
  @Config(minSdk = API_LEVELS.API_26)
  public void surfaceRedrawNeededAsyncShouldRunFinishDrawingImmediatelyWhenUiAlreadyDisplayed() {
    FlutterSurfaceView fakeSurfaceView = mock(FlutterSurfaceView.class);
    FlutterRenderer fakeFlutterRenderer = mock(FlutterRenderer.class);
    SurfaceHolder fakeSurfaceHolder = mock(SurfaceHolder.class);
    Runnable finishDrawing = mock(Runnable.class);
    doAnswer(
            invocation -> {
              FlutterUiDisplayListener listener = invocation.getArgument(0);
              listener.onFlutterUiDisplayed();
              return null;
            })
        .when(fakeFlutterRenderer)
        .addIsDisplayingFlutterUiListener(any());

    SurfaceHolderCallbackCompat test =
        new SurfaceHolderCallbackCompat(null, fakeSurfaceView, fakeFlutterRenderer);

    test.surfaceRedrawNeededAsync(fakeSurfaceHolder, finishDrawing);

    ArgumentCaptor<FlutterUiDisplayListener> listenerCaptor =
        ArgumentCaptor.forClass(FlutterUiDisplayListener.class);
    verify(fakeFlutterRenderer, times(1))
        .addIsDisplayingFlutterUiListener(listenerCaptor.capture());
    verify(finishDrawing, times(1)).run();
    verify(fakeFlutterRenderer, times(1))
        .removeIsDisplayingFlutterUiListener(listenerCaptor.getValue());

    ShadowLooper.idleMainLooper(
        SurfaceHolderCallbackCompat.FINISH_DRAWING_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    verify(finishDrawing, times(1)).run();
  }

  @Test
  @Config(minSdk = API_LEVELS.API_26)
  public void
      surfaceRedrawNeededAsyncShouldCleanUpAndRunFinishDrawingWhenDetachedBeforeUiDisplayed() {
    FlutterSurfaceView fakeSurfaceView = mock(FlutterSurfaceView.class);
    FlutterRenderer fakeFlutterRenderer = mock(FlutterRenderer.class);
    SurfaceHolder fakeSurfaceHolder = mock(SurfaceHolder.class);
    Runnable finishDrawing = mock(Runnable.class);
    SurfaceHolderCallbackCompat test =
        new SurfaceHolderCallbackCompat(null, fakeSurfaceView, fakeFlutterRenderer);

    test.surfaceRedrawNeededAsync(fakeSurfaceHolder, finishDrawing);

    ArgumentCaptor<FlutterUiDisplayListener> listenerCaptor =
        ArgumentCaptor.forClass(FlutterUiDisplayListener.class);
    verify(fakeFlutterRenderer, times(1))
        .addIsDisplayingFlutterUiListener(listenerCaptor.capture());

    test.onDetachFromRenderer();
    ShadowLooper.idleMainLooper(
        SurfaceHolderCallbackCompat.FINISH_DRAWING_TIMEOUT_MS, TimeUnit.MILLISECONDS);

    verify(finishDrawing, times(1)).run();
    verify(fakeFlutterRenderer, times(1))
        .removeIsDisplayingFlutterUiListener(listenerCaptor.getValue());
  }

  @Test
  @Config(minSdk = API_LEVELS.API_26)
  public void
      surfaceRedrawNeededAsyncShouldCleanUpAndRunFinishDrawingWhenReattachedToRendererBeforeUiDisplayed() {
    FlutterSurfaceView fakeSurfaceView = mock(FlutterSurfaceView.class);
    FlutterRenderer firstRenderer = mock(FlutterRenderer.class);
    FlutterRenderer secondRenderer = mock(FlutterRenderer.class);
    SurfaceHolder fakeSurfaceHolder = mock(SurfaceHolder.class);
    Runnable finishDrawing = mock(Runnable.class);
    SurfaceHolderCallbackCompat test =
        new SurfaceHolderCallbackCompat(null, fakeSurfaceView, firstRenderer);

    test.surfaceRedrawNeededAsync(fakeSurfaceHolder, finishDrawing);

    ArgumentCaptor<FlutterUiDisplayListener> listenerCaptor =
        ArgumentCaptor.forClass(FlutterUiDisplayListener.class);
    verify(firstRenderer, times(1)).addIsDisplayingFlutterUiListener(listenerCaptor.capture());

    test.onAttachToRenderer(secondRenderer);
    ShadowLooper.idleMainLooper(
        SurfaceHolderCallbackCompat.FINISH_DRAWING_TIMEOUT_MS, TimeUnit.MILLISECONDS);

    verify(finishDrawing, times(1)).run();
    verify(firstRenderer, times(1)).removeIsDisplayingFlutterUiListener(listenerCaptor.getValue());
  }

  @Test
  @Config(minSdk = API_LEVELS.API_26)
  public void
      surfaceRedrawNeededAsyncShouldCleanUpAndRunFinishDrawingWhenSurfaceDestroyedBeforeUiDisplayed() {
    FlutterSurfaceView fakeSurfaceView = mock(FlutterSurfaceView.class);
    FlutterRenderer fakeFlutterRenderer = mock(FlutterRenderer.class);
    SurfaceHolder fakeSurfaceHolder = mock(SurfaceHolder.class);
    Runnable finishDrawing = mock(Runnable.class);
    SurfaceHolderCallbackCompat test =
        new SurfaceHolderCallbackCompat(null, fakeSurfaceView, fakeFlutterRenderer);

    test.surfaceRedrawNeededAsync(fakeSurfaceHolder, finishDrawing);

    ArgumentCaptor<FlutterUiDisplayListener> listenerCaptor =
        ArgumentCaptor.forClass(FlutterUiDisplayListener.class);
    verify(fakeFlutterRenderer, times(1))
        .addIsDisplayingFlutterUiListener(listenerCaptor.capture());

    test.surfaceDestroyed(fakeSurfaceHolder);
    ShadowLooper.idleMainLooper(
        SurfaceHolderCallbackCompat.FINISH_DRAWING_TIMEOUT_MS, TimeUnit.MILLISECONDS);

    verify(finishDrawing, times(1)).run();
    verify(fakeFlutterRenderer, times(1))
        .removeIsDisplayingFlutterUiListener(listenerCaptor.getValue());
  }
}
