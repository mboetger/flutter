// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.plugin.platform;

import static org.mockito.Mockito.*;

import android.content.Context;
import android.view.Surface;
import android.view.View;
import androidx.annotation.NonNull;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.embedding.engine.FlutterJNI;
import io.flutter.embedding.engine.dart.DartExecutor;
import io.flutter.embedding.engine.systemchannels.PlatformViewCreationRequest;
import io.flutter.view.TextureRegistry;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

@RunWith(AndroidJUnit4.class)
public class SurfaceOutOfResourcesReproduceTest {

  @Test
  @Config(
      sdk = 29,
      shadows = {
        PlatformViewsControllerTest.ShadowFlutterJNI.class,
        PlatformViewsControllerTest.ShadowPlatformTaskQueue.class
      })
  public void platformViewCreation_throwsOutOfResourcesException_bubblesUp() {
    PlatformViewsController platformViewsController = new PlatformViewsController();
    FlutterJNI jni = new FlutterJNI();
    platformViewsController.setFlutterJNI(jni);

    final Context context = ApplicationProvider.getApplicationContext();
    final TextureRegistry registry = mock(TextureRegistry.class);
    final TextureRegistry.SurfaceProducer surfaceProducer = mock(TextureRegistry.SurfaceProducer.class);

    when(registry.createSurfaceProducer(any())).thenReturn(surfaceProducer);

    // Make setSize throw Surface.OutOfResourcesException
    doThrow(new Surface.OutOfResourcesException("Out of resources simulation"))
        .when(surfaceProducer)
        .setSize(anyInt(), anyInt());

    // Simple platform view setup
    PlatformViewFactory viewFactory = mock(PlatformViewFactory.class);
    PlatformView platformView = mock(PlatformView.class);
    View view = new View(context);
    when(platformView.getView()).thenReturn(view);
    when(viewFactory.create(any(), anyInt(), any())).thenReturn(platformView);
    platformViewsController.getRegistry().registerViewFactory("reproduceType", viewFactory);

    // Attach to controller delegator
    PlatformViewsController2 secondController = new PlatformViewsController2();
    secondController.setRegistry(new PlatformViewRegistryImpl());
    PlatformViewsControllerDelegator platformViewsControllerDelegator =
        new PlatformViewsControllerDelegator(platformViewsController, secondController);

    final FlutterEngine engine = mock(FlutterEngine.class);
    when(engine.getRenderer()).thenReturn(mock(io.flutter.embedding.engine.renderer.FlutterRenderer.class));
    when(engine.getPlatformViewsController()).thenReturn(platformViewsController);
    when(engine.getPlatformViewsController2()).thenReturn(secondController);
    when(engine.getPlatformViewsControllerDelegator()).thenReturn(platformViewsControllerDelegator);
    when(engine.getDartExecutor()).thenReturn(mock(DartExecutor.class));

    // Initialize/attach
    platformViewsController.attachToView(mock(io.flutter.embedding.android.FlutterView.class));
    platformViewsControllerDelegator.attach(context, registry, mock(DartExecutor.class));

    // Try to create the platform view.
    final PlatformViewCreationRequest request =
        PlatformViewCreationRequest.createTLHCWithFallbackRequest(
            0,
            "reproduceType",
            0.0,
            0.0,
            128.0,
            128.0,
            View.LAYOUT_DIRECTION_LTR,
            true,
            null);

    // This calls configureForVirtualDisplay / configureForTextureLayerComposition, which tries to resize,
    // causing setSize to throw.
    platformViewsController.channelHandler.createForTextureLayer(request);
  }
}
