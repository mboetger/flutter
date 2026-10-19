package io.flutter.embedding.android;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.annotation.TargetApi;
import android.graphics.Bitmap;
import android.hardware.HardwareBuffer;
import android.media.Image;
import android.media.ImageReader;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.engine.renderer.FlutterRenderer;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.util.ReflectionHelpers;

@RunWith(AndroidJUnit4.class)
@TargetApi(29)
public class FlutterImageViewTest {

  @Test
  public void itRecyclesBitmapWhenDetached() {
    ImageReader mockReader = mock(ImageReader.class);
    Image mockImage = mock(Image.class);
    HardwareBuffer mockBuffer = mock(HardwareBuffer.class);
    when(mockImage.getHardwareBuffer()).thenReturn(mockBuffer);
    when(mockReader.acquireLatestImage()).thenReturn(mockImage);
    when(mockImage.getPlanes()).thenReturn(new Image.Plane[0]);

    FlutterImageView imageView =
        new FlutterImageView(
            ApplicationProvider.getApplicationContext(),
            mockReader,
            FlutterImageView.SurfaceKind.background);

    FlutterRenderer mockRenderer = mock(FlutterRenderer.class);
    imageView.attachToRenderer(mockRenderer);

    assertTrue((Boolean) ReflectionHelpers.getField(imageView, "isAttachedToFlutterRenderer"));

    // Provide a mocked currentBitmap
    Bitmap mockBitmap = mock(Bitmap.class);
    ReflectionHelpers.setField(imageView, "currentBitmap", mockBitmap);

    imageView.detachFromRenderer();

    // Verify recycle was called
    verify(mockBitmap, times(1)).recycle();
  }

  @Test
  public void itRecyclesPreviousBitmapWhenUpdating() {
    ImageReader mockReader = mock(ImageReader.class);
    Image mockImage = mock(Image.class);
    HardwareBuffer mockBuffer = mock(HardwareBuffer.class);
    when(mockImage.getHardwareBuffer()).thenReturn(mockBuffer);
    when(mockReader.acquireLatestImage()).thenReturn(mockImage);
    when(mockImage.getPlanes()).thenReturn(new Image.Plane[0]);

    FlutterImageView imageView =
        new FlutterImageView(
            ApplicationProvider.getApplicationContext(),
            mockReader,
            FlutterImageView.SurfaceKind.background);

    Bitmap mockBitmap = mock(Bitmap.class);
    ReflectionHelpers.setField(imageView, "currentBitmap", mockBitmap);
    ReflectionHelpers.setField(imageView, "currentImage", mockImage);

    ReflectionHelpers.callInstanceMethod(imageView, "updateCurrentBitmap");

    // Verify recycle was called on the previous bitmap
    verify(mockBitmap, times(1)).recycle();
  }
}
