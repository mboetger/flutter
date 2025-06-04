package io.flutter.embedding.android;

import android.view.SurfaceHolder;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import io.flutter.Build;
import io.flutter.Log;
import io.flutter.embedding.engine.renderer.FlutterRenderer;
import io.flutter.embedding.engine.renderer.FlutterUiDisplayListener;

public class SurfaceHolderCallbackCompat {

  private static final String TAG = "SurfaceHolderCallbackCompat";
  private final FlutterSurfaceView flutterSurfaceView;
  @Nullable private FlutterRenderer flutterRenderer;

  private final SurfaceHolder.Callback innerCallback;

  public void onAttachToRenderer(FlutterRenderer flutterRenderer) {
    if (android.os.Build.VERSION.SDK_INT < Build.API_LEVELS.API_26) {
      if (this.flutterRenderer != null) {
        this.flutterRenderer.removeIsDisplayingFlutterUiListener(flutterUiDisplayListener);
      }
    }
    this.flutterRenderer = flutterRenderer;
  }

  public void onDetachFromRenderer() {
    if (android.os.Build.VERSION.SDK_INT < Build.API_LEVELS.API_26) {
      if (this.flutterRenderer != null) {
        this.flutterRenderer.removeIsDisplayingFlutterUiListener(flutterUiDisplayListener);
      }
    }
    this.flutterRenderer = null;
  }

  public void onResume() {
    if (android.os.Build.VERSION.SDK_INT < Build.API_LEVELS.API_26) {
      if (flutterRenderer != null) {
        this.flutterRenderer.addIsDisplayingFlutterUiListener(flutterUiDisplayListener);
      }
    }
  }

  ///  Listens for a callback from the Flutter Engine to indicate that a frame is ready to display.
  @VisibleForTesting
  final FlutterUiDisplayListener flutterUiDisplayListener =
      new FlutterUiDisplayListener() {
        @Override
        public void onFlutterUiDisplayed() {
          Log.v(TAG, "onFlutterUiDisplayed()");
          // Now that a frame is ready to display, take this SurfaceView from transparent to opaque.
          flutterSurfaceView.setAlpha(1.0f);

          if (flutterRenderer != null) {
            flutterRenderer.removeIsDisplayingFlutterUiListener(this);
          }
        }

        @Override
        public void onFlutterUiNoLongerDisplayed() {
          // no-op
        }
      };

  private class SufaceHolderCallback implements SurfaceHolder.Callback {
    @Override
    public void surfaceCreated(@NonNull SurfaceHolder holder) {
      innerCallback.surfaceCreated(holder);
    }

    @Override
    public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
      innerCallback.surfaceChanged(holder, format, width, height);
    }

    @Override
    public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
      innerCallback.surfaceDestroyed(holder);
    }
  }

  private class SurfaceHolderCallback2Compat extends SufaceHolderCallback
      implements SurfaceHolder.Callback2 {
    @Override
    public void surfaceRedrawNeeded(@NonNull SurfaceHolder holder) {
      Log.v(TAG, "SurfaceHolder.Callback2.surfaceRedrawNeeded()");
      // no-op - instead use surfaceRedrawNeededAsync()
    }
  }

  private class SurfaceHolderCallback2AsyncCompat extends SurfaceHolderCallback2Compat
      implements SurfaceHolder.Callback2 {
    @Override
    @RequiresApi(api = Build.API_LEVELS.API_26)
    public void surfaceRedrawNeededAsync(
        @NonNull SurfaceHolder holder, @NonNull Runnable finishDrawing) {
      Log.v(TAG, "SurfaceHolder.Callback2.surfaceRedrawNeededAsync()");
      if (flutterRenderer == null) {
        return;
      }
      flutterRenderer.addIsDisplayingFlutterUiListener(
          new FlutterUiDisplayListener() {
            @Override
            public void onFlutterUiDisplayed() {
              finishDrawing.run();
              if (flutterRenderer != null) {
                flutterRenderer.removeIsDisplayingFlutterUiListener(this);
              }
            }

            @Override
            public void onFlutterUiNoLongerDisplayed() {
              // no-op
            }
          });
    }
  }

  final SurfaceHolder.Callback callback =
      (android.os.Build.VERSION.SDK_INT >= Build.API_LEVELS.API_26)
          ? new SurfaceHolderCallback2AsyncCompat()
          : new SurfaceHolderCallback2Compat();

  public SurfaceHolderCallbackCompat(
      SurfaceHolder.Callback innerCallback,
      FlutterSurfaceView flutterSurfaceView,
      @Nullable FlutterRenderer flutterRenderer) {
    this.innerCallback = innerCallback;
    this.flutterRenderer = flutterRenderer;
    this.flutterSurfaceView = flutterSurfaceView;

    Log.v(TAG, "SurfaceHolderCallbackCompat()");

    if (android.os.Build.VERSION.SDK_INT < Build.API_LEVELS.API_26) {
      // Keep this SurfaceView transparent until Flutter has a frame ready to render. This avoids
      // displaying a black rectangle in our place.
      this.flutterSurfaceView.setAlpha(0.0f);
    }
  }
}
