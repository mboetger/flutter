// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;
import static org.mockito.Mockito.*;

import android.content.Context;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.FlutterInjector;
import io.flutter.embedding.engine.dart.DartExecutor.DartEntrypoint;
import io.flutter.embedding.engine.loader.FlutterLoader;
import io.flutter.plugin.common.BinaryMessenger;
import io.flutter.plugin.platform.PlatformViewsController;
import io.flutter.plugins.GeneratedPluginRegistrant;
import java.nio.ByteBuffer;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@RunWith(AndroidJUnit4.class)
public class FlutterEngineSpawningRaceReproduceTest {
  private final Context ctx = ApplicationProvider.getApplicationContext();
  @Mock FlutterJNI mockFlutterJNI;
  @Mock FlutterLoader mockFlutterLoader;
  
  private FlutterEngineGroup engineGroup;
  private FlutterEngine firstEngine;
  private FlutterEngine secondEngine;
  private boolean jniAttached;

  @Before
  public void setUp() {
    FlutterInjector.reset();
    MockitoAnnotations.openMocks(this);
    jniAttached = false;
    
    when(mockFlutterJNI.isAttached()).thenAnswer(invocation -> jniAttached);
    doAnswer(invocation -> jniAttached = true).when(mockFlutterJNI).attachToNative();
    GeneratedPluginRegistrant.clearRegisteredEngines();
    FlutterEngine.resetNextEngineId();

    when(mockFlutterLoader.findAppBundlePath()).thenReturn("some/path/to/flutter_assets");
    when(mockFlutterLoader.automaticallyRegisterPlugins()).thenReturn(true);

    FlutterJNI.Factory jniFactory = new FlutterJNI.Factory() {
      @Override
      public FlutterJNI provideFlutterJNI() {
        return mockFlutterJNI;
      }
    };

    FlutterInjector.setInstance(
        new FlutterInjector.Builder()
            .setFlutterLoader(mockFlutterLoader)
            .setFlutterJNIFactory(jniFactory)
            .build());

    engineGroup = new FlutterEngineGroup(ctx);
  }

  @After
  public void tearDown() {
    if (firstEngine != null) {
      firstEngine.destroy();
      firstEngine = null;
    }
    if (secondEngine != null) {
      secondEngine.destroy();
      secondEngine = null;
    }
    GeneratedPluginRegistrant.clearRegisteredEngines();
    engineGroup = null;
  }

  @Test
  public void reproduceSpawningRaceConditionPlatformMessageDropped() {
    // 1. Create the first engine.
    firstEngine = engineGroup.createAndRunEngine(ctx, mock(DartEntrypoint.class));
    assertEquals(1, engineGroup.activeEngines.size());

    // 2. Set up the second FlutterJNI as a spy of a real FlutterJNI instance to test real behavior.
    FlutterJNI secondSpyFlutterJNI = spy(new FlutterJNI());
    
    // Stub native-calling methods on the spy to prevent UnsatisfiedLinkError/RuntimeException.
    when(secondSpyFlutterJNI.isAttached()).thenReturn(true);
    doNothing().when(secondSpyFlutterJNI).dispatchPlatformMessage(any(String.class), any(ByteBuffer.class), anyInt(), anyInt());
    doNothing().when(secondSpyFlutterJNI).cleanupMessageData(anyLong());
    doNothing().when(secondSpyFlutterJNI).detachFromNativeAndReleaseResources();
    
    // We want to track if the message is dropped or handled.
    final AtomicBoolean firstMessageDropped = new AtomicBoolean(false);
    final AtomicBoolean secondMessageHandled = new AtomicBoolean(false);

    // Mock spawn on the first JNI to return the second JNI.
    // Crucially, simulate that during spawn (before the second FlutterEngine is constructed),
    // a platform message is received from Dart.
    when(mockFlutterJNI.isAttached()).thenReturn(true);
    doAnswer(invocation -> {
      // This runs during the spawn call. At this moment, spawn has not returned yet,
      // and the second FlutterEngine constructor has NOT run, so the platformMessageHandler
      // on secondSpyFlutterJNI is still null.
      try {
        secondSpyFlutterJNI.handlePlatformMessage("foo_channel", null, 1, 12345);
      } catch (UnsatisfiedLinkError e) {
        // UnsatisfiedLinkError is expected because secondSpyFlutterJNI.platformMessageHandler
        // is null, which forces it to call the native method nativeCleanupMessageData(messageData).
        // Since native libraries are not loaded in Robolectric, this throws UnsatisfiedLinkError.
        firstMessageDropped.set(true);
      }
      return secondSpyFlutterJNI;
    }).when(mockFlutterJNI).spawn(
        nullable(String.class),
        nullable(String.class),
        nullable(String.class),
        nullable(List.class),
        anyLong()
    );

    // 3. Set up the dynamic plugin registration callback on the fake GeneratedPluginRegistrant.
    // This will register the mockHandler *during* the construction of the second engine,
    // before Layer 2 buffering is disabled and flushed!
    BinaryMessenger.BinaryMessageHandler mockHandler = mock(BinaryMessenger.BinaryMessageHandler.class);
    GeneratedPluginRegistrant.registrationCallback = engine -> {
      engine.getDartExecutor().getBinaryMessenger().setMessageHandler("foo_channel", mockHandler);
    };

    // 4. Spawn the second engine.
    // This will invoke the mocked spawn() above, triggering the immediate platform message.
    secondEngine = engineGroup.createAndRunEngine(ctx, mock(DartEntrypoint.class));
    assertEquals(2, engineGroup.activeEngines.size());

    // Run the Robolectric looper to flush any asynchronously dispatched tasks
    org.robolectric.shadows.ShadowLooper.idleMainLooper();

    // Verify that the FIRST platform message (sent during spawn) was successfully delivered!
    verify(mockHandler, times(1)).onMessage(eq(null), any());

    // 5. Send a second platform message to verify that subsequent messages work normally too.
    try {
      secondSpyFlutterJNI.handlePlatformMessage("foo_channel", null, 2, 12345);
      secondMessageHandled.set(true);
    } catch (UnsatisfiedLinkError e) {
      secondMessageHandled.set(false);
    }

    assertTrue("The second platform message sent after construction should be successfully handled", secondMessageHandled.get());
    org.robolectric.shadows.ShadowLooper.idleMainLooper();
    verify(mockHandler, times(2)).onMessage(any(), any());
  }
}
