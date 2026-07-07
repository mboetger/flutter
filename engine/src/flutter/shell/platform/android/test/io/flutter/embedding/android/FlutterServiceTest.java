// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Notification;
import android.content.Context;
import android.content.Intent;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.FlutterInjector;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.embedding.engine.FlutterJNI;
import io.flutter.embedding.engine.dart.DartExecutor;
import io.flutter.embedding.engine.loader.FlutterLoader;
import io.flutter.plugin.common.BinaryMessenger;
import io.flutter.plugin.common.MethodCall;
import io.flutter.plugin.common.StandardMethodCodec;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.android.controller.ServiceController;
import org.robolectric.shadows.ShadowService;

@RunWith(AndroidJUnit4.class)
public class FlutterServiceTest {
  private final Context ctx = ApplicationProvider.getApplicationContext();

  @Before
  public void setUp() {
    FlutterInjector.reset();
    FlutterJNI mockFlutterJNI = mock(FlutterJNI.class);
    when(mockFlutterJNI.isAttached()).thenReturn(true);
    FlutterJNI.Factory mockFlutterJNIFactory = mock(FlutterJNI.Factory.class);
    when(mockFlutterJNIFactory.provideFlutterJNI()).thenReturn(mockFlutterJNI);
    FlutterLoader mockFlutterLoader = mock(FlutterLoader.class);
    when(mockFlutterLoader.findAppBundlePath()).thenReturn("dummy_path");
    FlutterInjector.setInstance(
        new FlutterInjector.Builder()
            .setFlutterJNIFactory(mockFlutterJNIFactory)
            .setFlutterLoader(mockFlutterLoader)
            .build());
  }

  @After
  public void tearDown() {
    FlutterInjector.reset();
  }

  @Test
  public void testServiceSpinsUpEngineAndExecutesDart() {
    Intent intent = new Intent(ctx, TestFlutterService.class);
    intent.putExtra(FlutterService.EXTRA_DART_ENTRYPOINT, "myEntrypoint");
    ArrayList<String> args = new ArrayList<>();
    args.add("arg1");
    intent.putExtra(FlutterService.EXTRA_DART_ENTRYPOINT_ARGS, args);

    ServiceController<TestFlutterService> controller =
        Robolectric.buildService(TestFlutterService.class, intent);

    TestFlutterService service = controller.get();
    controller.create();
    controller.startCommand(0, 0);

    assertNotNull(service.flutterEngine);
    verify(service.mockDartExecutor)
        .executeDartEntrypoint(any(DartExecutor.DartEntrypoint.class), eq(args));
  }

  @Test
  public void testServiceForegroundMode() {
    Intent intent = new Intent(ctx, TestFlutterService.class);
    intent.putExtra(FlutterService.EXTRA_FOREGROUND_SERVICE, true);
    intent.putExtra(FlutterService.EXTRA_FOREGROUND_NOTIFICATION_ID, 42);

    ServiceController<TestFlutterService> controller =
        Robolectric.buildService(TestFlutterService.class, intent);

    TestFlutterService service = controller.get();
    controller.create();
    controller.startCommand(0, 0);

    ShadowService shadowService = Shadows.shadowOf(service);
    assertNotNull(shadowService.getLastForegroundNotification());
    assertEquals(42, shadowService.getLastForegroundNotificationId());
  }

  @Test
  public void testServiceForegroundModeWithServiceType() {
    Intent intent = new Intent(ctx, TestFlutterService.class);
    intent.putExtra(FlutterService.EXTRA_FOREGROUND_SERVICE, true);
    intent.putExtra(FlutterService.EXTRA_FOREGROUND_NOTIFICATION_ID, 42);
    intent.putExtra(
        FlutterService.EXTRA_FOREGROUND_SERVICE_TYPE,
        8); // e.g. FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK

    ServiceController<TestFlutterService> controller =
        Robolectric.buildService(TestFlutterService.class, intent);

    TestFlutterService service = controller.get();
    controller.create();
    controller.startCommand(0, 0);

    ShadowService shadowService = Shadows.shadowOf(service);
    assertNotNull(shadowService.getLastForegroundNotification());
    assertEquals(42, shadowService.getLastForegroundNotificationId());
    assertEquals(8, service.getForegroundServiceType());
  }

  @Test
  public void testServiceStopSelfOnMethodCall() {
    Intent intent = new Intent(ctx, TestFlutterService.class);
    ServiceController<TestFlutterService> controller =
        Robolectric.buildService(TestFlutterService.class, intent);

    TestFlutterService service = controller.get();
    controller.create();
    controller.startCommand(0, 0);

    // Verify MethodChannel registered on mock messenger
    ArgumentCaptor<BinaryMessenger.BinaryMessageHandler> handlerCaptor =
        ArgumentCaptor.forClass(BinaryMessenger.BinaryMessageHandler.class);
    verify(service.mockMessenger)
        .setMessageHandler(eq("io.flutter/background_service"), handlerCaptor.capture());
    BinaryMessenger.BinaryMessageHandler messageHandler = handlerCaptor.getValue();
    assertNotNull(messageHandler);

    // Simulate incoming stopService method call
    MethodCall methodCall = new MethodCall("stopService", null);
    ByteBuffer encodedCall = StandardMethodCodec.INSTANCE.encodeMethodCall(methodCall);
    BinaryMessenger.BinaryReply mockReply = mock(BinaryMessenger.BinaryReply.class);

    messageHandler.onMessage((ByteBuffer) encodedCall.flip(), mockReply);

    assertTrue(Shadows.shadowOf(service).isStoppedBySelf());
  }

  @Test
  public void testMultipleOnStartCommandCallsDoesNotCreateMultipleEngines() {
    Intent intent = new Intent(ctx, TestFlutterService.class);
    ServiceController<TestFlutterService> controller =
        Robolectric.buildService(TestFlutterService.class, intent);
    TestFlutterService service = controller.get();
    controller.create();

    controller.startCommand(0, 0);
    FlutterEngine firstEngine = service.flutterEngine;
    assertNotNull(firstEngine);

    controller.startCommand(0, 0);
    FlutterEngine secondEngine = service.flutterEngine;

    assertEquals(firstEngine, secondEngine);
  }

  @Test
  public void testServiceOnDestroyDestroysEngine() {
    Intent intent = new Intent(ctx, TestFlutterService.class);
    ServiceController<TestFlutterService> controller =
        Robolectric.buildService(TestFlutterService.class, intent);
    TestFlutterService service = controller.get();
    controller.create();
    controller.startCommand(0, 0);

    assertNotNull(service.flutterEngine);
    controller.destroy();

    // Verify engine was destroyed
    verify(service.mockEngine).destroy();
  }

  @Test
  public void testNullIntent() {
    ServiceController<TestFlutterService> controller =
        Robolectric.buildService(TestFlutterService.class, null);
    TestFlutterService service = controller.get();
    controller.create();
    int result = controller.startCommand(0, 0).get().onStartCommand(null, 0, 0);

    assertEquals(android.app.Service.START_NOT_STICKY, result);
    assertTrue(Shadows.shadowOf(service).isStoppedBySelf());
  }

  @Test
  public void testNullNotification() {
    Intent intent = new Intent(ctx, TestFlutterServiceWithNullNotification.class);
    intent.putExtra(FlutterService.EXTRA_FOREGROUND_SERVICE, true);
    intent.putExtra(FlutterService.EXTRA_FOREGROUND_NOTIFICATION_ID, 42);

    ServiceController<TestFlutterServiceWithNullNotification> controller =
        Robolectric.buildService(TestFlutterServiceWithNullNotification.class, intent);
    TestFlutterServiceWithNullNotification service = controller.get();
    controller.create();
    controller.startCommand(0, 0);

    ShadowService shadowService = Shadows.shadowOf(service);
    assertNull(
        shadowService.getLastForegroundNotification()); // Should not have started in foreground
  }

  // Subclass to supply a mocked FlutterEngine and trace Service lifecycle
  public static class TestFlutterService extends FlutterService {
    public DartExecutor mockDartExecutor;
    public FlutterEngine mockEngine;
    public BinaryMessenger mockMessenger;

    @Override
    @NonNull
    protected FlutterEngine provideFlutterEngine() {
      mockEngine = mock(FlutterEngine.class);
      mockDartExecutor = mock(DartExecutor.class);
      mockMessenger = mock(BinaryMessenger.class);
      when(mockEngine.getDartExecutor()).thenReturn(mockDartExecutor);
      when(mockDartExecutor.getBinaryMessenger()).thenReturn(mockMessenger);
      return mockEngine;
    }

    @Override
    protected Notification getForegroundNotification() {
      return mock(Notification.class);
    }
  }

  // Subclass with null notification
  public static class TestFlutterServiceWithNullNotification extends TestFlutterService {
    @Override
    @Nullable
    protected Notification getForegroundNotification() {
      return null;
    }
  }
}
