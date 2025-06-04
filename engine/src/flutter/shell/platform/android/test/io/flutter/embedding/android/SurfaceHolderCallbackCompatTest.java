package io.flutter.embedding.android;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.annotation.TargetApi;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.Build;
import io.flutter.embedding.engine.renderer.FlutterRenderer;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class SurfaceHolderCallbackCompatTest {

  @Test
  @TargetApi(Build.API_LEVELS.API_25)
  public void onAttachToRendererShouldRemoveListenerBelowApi26() {
    FlutterSurfaceView fakeSurfaceView = mock(FlutterSurfaceView.class);
    FlutterRenderer fakeFlutterRenderer = mock(FlutterRenderer.class);
    SurfaceHolderCallbackCompat test =
        new SurfaceHolderCallbackCompat(null, fakeSurfaceView, fakeFlutterRenderer);
    test.onAttachToRenderer(null);
    verify(fakeFlutterRenderer, times(1))
        .removeIsDisplayingFlutterUiListener(test.flutterUiDisplayListener);
  }

  @Test
  @TargetApi(Build.API_LEVELS.API_26)
  public void onAttachToRendererShouldNotRemoveListenerApi26OrAbove() {
    FlutterSurfaceView fakeSurfaceView = mock(FlutterSurfaceView.class);
    FlutterRenderer fakeFlutterRenderer = mock(FlutterRenderer.class);
    SurfaceHolderCallbackCompat test =
        new SurfaceHolderCallbackCompat(null, fakeSurfaceView, fakeFlutterRenderer);
    test.onAttachToRenderer(null);
    verify(fakeFlutterRenderer, never())
        .removeIsDisplayingFlutterUiListener(test.flutterUiDisplayListener);
  }
}