// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.plugin.platform;

import static org.junit.Assert.*;
import static org.mockito.Mockito.*;

import android.app.Activity;
import android.content.Context;
import android.content.MutableContextWrapper;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.engine.FlutterJNI;
import io.flutter.embedding.engine.dart.DartExecutor;
import io.flutter.embedding.engine.systemchannels.PlatformViewCreationRequest;
import io.flutter.plugin.common.StandardMessageCodec;
import io.flutter.view.TextureRegistry;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

@RunWith(AndroidJUnit4.class)
public class PlatformViewsActivityLifecycleReproduceTest {

  @Test
  @Config(
      shadows = {
        PlatformViewsControllerTest.ShadowFlutterJNI.class,
        PlatformViewsControllerTest.ShadowPlatformTaskQueue.class
      }
  )
  public void testPlatformViewContextNotUpdatedOnActivityLifecycle() {
    // 1. Initialize PlatformViewsController.
    PlatformViewsController platformViewsController = new PlatformViewsController();
    FlutterJNI jni = mock(FlutterJNI.class);
    platformViewsController.setFlutterJNI(jni);

    // Create two fake activities.
    Activity activityA = Robolectric.buildActivity(Activity.class).create().get();
    Activity activityB = Robolectric.buildActivity(Activity.class).create().get();

    // 2. Attach Activity A.
    TextureRegistry textureRegistry = mock(TextureRegistry.class);
    DartExecutor dartExecutor = mock(DartExecutor.class);
    platformViewsController.attach(activityA, textureRegistry, dartExecutor);

    // 3. Register a view factory that captures the context.
    final Context[] capturedContext = new Context[1];
    PlatformViewRegistry registry = platformViewsController.getRegistry();
    registry.registerViewFactory(
        "test_view",
        new PlatformViewFactory(StandardMessageCodec.INSTANCE) {
          @Override
          public PlatformView create(Context context, int viewId, Object args) {
            capturedContext[0] = context;
            return new PlatformView() {
              @Override
              public android.view.View getView() {
                return new android.view.View(context);
              }
              @Override
              public void dispose() {}
            };
          }
        });

    // 4. Create the platform view.
    PlatformViewCreationRequest request =
        new PlatformViewCreationRequest(
            /*viewId=*/ 0,
            /*viewType=*/ "test_view",
            /*logicalTop=*/ 0.0,
            /*logicalLeft=*/ 0.0,
            /*logicalWidth=*/ 100.0,
            /*logicalHeight=*/ 100.0,
            /*direction=*/ 0,
            /*params=*/ null);

    // We pass wrapContext = true.
    platformViewsController.createPlatformView(request, /*wrapContext=*/ true);

    assertNotNull(capturedContext[0]);
    assertTrue(capturedContext[0] instanceof MutableContextWrapper);
    MutableContextWrapper contextWrapper = (MutableContextWrapper) capturedContext[0];

    // The base context should initially be Activity A.
    assertEquals(activityA, contextWrapper.getBaseContext());

    // 5. Detach Activity A.
    platformViewsController.detach();

    // Verify that Activity A is no longer referenced to prevent memory leaks
    // during the detached state (it should be the application context).
    assertNotEquals(activityA, contextWrapper.getBaseContext());

    // 6. Attach Activity B.
    platformViewsController.attach(activityB, textureRegistry, dartExecutor);

    // 7. Verify if the context wrapper's base context has been updated to Activity B.
    assertEquals(activityB, contextWrapper.getBaseContext());
  }

  @Test
  @Config(
      sdk = 30,
      shadows = {
        PlatformViewsControllerTest.ShadowFlutterJNI.class,
        PlatformViewsControllerTest.ShadowPlatformTaskQueue.class
      }
  )
  public void testPlatformViewContextUpdatedOnActivityLifecycle_virtualDisplay() throws Exception {
    PlatformViewsController platformViewsController = new PlatformViewsController();
    FlutterJNI jni = mock(FlutterJNI.class);
    platformViewsController.setFlutterJNI(jni);

    Activity activityA = Robolectric.buildActivity(Activity.class).create().get();
    Activity activityB = Robolectric.buildActivity(Activity.class).create().get();

    TextureRegistry textureRegistry = mock(TextureRegistry.class);
    TextureRegistry.SurfaceTextureEntry surfaceTextureEntry = mock(TextureRegistry.SurfaceTextureEntry.class);
    android.graphics.SurfaceTexture surfaceTexture = mock(android.graphics.SurfaceTexture.class);
    when(surfaceTextureEntry.surfaceTexture()).thenReturn(surfaceTexture);
    when(textureRegistry.createSurfaceTexture()).thenReturn(surfaceTextureEntry);

    TextureRegistry.SurfaceProducer surfaceProducer = mock(TextureRegistry.SurfaceProducer.class);
    android.view.Surface surface = mock(android.view.Surface.class);
    when(surfaceProducer.getSurface()).thenReturn(surface);
    when(textureRegistry.createSurfaceProducer(any())).thenReturn(surfaceProducer);

    DartExecutor dartExecutor = mock(DartExecutor.class);
    platformViewsController.attach(activityA, textureRegistry, dartExecutor);
    platformViewsController.attachToView(mock(io.flutter.embedding.android.FlutterView.class));

    // Register a view factory that returns a SurfaceView to force Virtual Display.
    PlatformViewRegistry registry = platformViewsController.getRegistry();
    registry.registerViewFactory(
        "test_view",
        new PlatformViewFactory(StandardMessageCodec.INSTANCE) {
          @Override
          public PlatformView create(Context context, int viewId, Object args) {
            return new PlatformView() {
              private final android.view.SurfaceView surfaceView = new android.view.SurfaceView(context);
              @Override
              public android.view.View getView() {
                return surfaceView;
              }
              @Override
              public void dispose() {}
            };
          }
        });

    PlatformViewCreationRequest request =
        new PlatformViewCreationRequest(
            /*viewId=*/ 0,
            /*viewType=*/ "test_view",
            /*logicalTop=*/ 0.0,
            /*logicalLeft=*/ 0.0,
            /*logicalWidth=*/ 100.0,
            /*logicalHeight=*/ 100.0,
            /*direction=*/ 0,
            /*params=*/ null);

    // Create the platform view. This should trigger configureForVirtualDisplay.
    platformViewsController.channelHandler.createForTextureLayer(request);

    // Get the vdContextWrapper using reflection.
    VirtualDisplayController vdController = platformViewsController.vdControllers.get(0);
    assertNotNull(vdController);
    SingleViewPresentation presentation = vdController.presentation;
    assertNotNull(presentation);

    java.lang.reflect.Field outerContextField = SingleViewPresentation.class.getDeclaredField("outerContext");
    outerContextField.setAccessible(true);
    Context outerContext = (Context) outerContextField.get(presentation);

    assertTrue(outerContext instanceof MutableContextWrapper);
    MutableContextWrapper vdContextWrapper = (MutableContextWrapper) outerContext;

    // The base context of the Virtual Display's context wrapper should initially be Activity A.
    assertEquals(activityA, vdContextWrapper.getBaseContext());

    // Detach Activity A.
    platformViewsController.detach();

    // Verify that Activity A is no longer referenced to prevent memory leaks.
    assertNotEquals(activityA, vdContextWrapper.getBaseContext());

    // Attach Activity B.
    platformViewsController.attach(activityB, textureRegistry, dartExecutor);

    // Verify that the context wrapper's base context has been updated to Activity B.
    assertEquals(activityB, vdContextWrapper.getBaseContext());
  }
}
