// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.job.JobParameters;
import android.content.Context;
import android.os.PersistableBundle;
import androidx.annotation.NonNull;
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
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.android.controller.ServiceController;
import org.robolectric.shadows.ShadowJobService;

@RunWith(AndroidJUnit4.class)
public class FlutterJobServiceTest {
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
  public void testJobServiceSpinsUpEngineAndExecutesDart() {
    JobParameters mockParams = mock(JobParameters.class);
    PersistableBundle extras = new PersistableBundle();
    extras.putString(FlutterJobService.EXTRA_DART_ENTRYPOINT, "jobEntrypoint");
    extras.putStringArray(FlutterJobService.EXTRA_DART_ENTRYPOINT_ARGS, new String[] {"jobArg1"});
    when(mockParams.getExtras()).thenReturn(extras);

    ServiceController<TestFlutterJobService> controller =
        Robolectric.buildService(TestFlutterJobService.class);
    TestFlutterJobService service = controller.get();
    controller.create();

    boolean keepAlive = service.onStartJob(mockParams);
    assertTrue(keepAlive);

    assertNotNull(service.flutterEngine);
    verify(service.mockDartExecutor)
        .executeDartEntrypoint(
            any(DartExecutor.DartEntrypoint.class), ArgumentMatchers.<List<String>>any());
  }

  @Test
  public void testJobServiceFinishedOnMethodCall() {
    JobParameters mockParams = mock(JobParameters.class);
    PersistableBundle extras = new PersistableBundle();
    when(mockParams.getExtras()).thenReturn(extras);

    ServiceController<TestFlutterJobService> controller =
        Robolectric.buildService(TestFlutterJobService.class);
    TestFlutterJobService service = controller.get();
    controller.create();

    service.onStartJob(mockParams);

    // Verify MethodChannel registered on mock messenger
    ArgumentCaptor<BinaryMessenger.BinaryMessageHandler> handlerCaptor =
        ArgumentCaptor.forClass(BinaryMessenger.BinaryMessageHandler.class);
    verify(service.mockMessenger)
        .setMessageHandler(eq("io.flutter/background_service"), handlerCaptor.capture());
    BinaryMessenger.BinaryMessageHandler messageHandler = handlerCaptor.getValue();
    assertNotNull(messageHandler);

    // Simulate incoming jobFinished method call
    Map<String, Object> arguments = new HashMap<>();
    arguments.put("needsReschedule", true);
    MethodCall methodCall = new MethodCall("jobFinished", arguments);
    ByteBuffer encodedCall = StandardMethodCodec.INSTANCE.encodeMethodCall(methodCall);
    BinaryMessenger.BinaryReply mockReply = mock(BinaryMessenger.BinaryReply.class);

    messageHandler.onMessage((ByteBuffer) encodedCall.flip(), mockReply);

    ShadowJobService shadowJobService = Shadows.shadowOf(service);
    assertTrue(shadowJobService.getIsJobFinished());
    assertTrue(shadowJobService.getIsRescheduleNeeded());
  }

  @Test
  public void testJobServiceOnStopJobCleansUpEngine() {
    JobParameters mockParams = mock(JobParameters.class);
    when(mockParams.getExtras()).thenReturn(new PersistableBundle());

    ServiceController<TestFlutterJobService> controller =
        Robolectric.buildService(TestFlutterJobService.class);
    TestFlutterJobService service = controller.get();
    controller.create();

    service.onStartJob(mockParams);
    assertNotNull(service.flutterEngine);

    service.onStopJob(mockParams);

    // Verify engine was destroyed
    verify(service.mockEngine).destroy();
  }

  @Test
  public void testConcurrentJob() {
    JobParameters mockParams1 = mock(JobParameters.class);
    when(mockParams1.getJobId()).thenReturn(1);
    when(mockParams1.getExtras()).thenReturn(new PersistableBundle());

    JobParameters mockParams2 = mock(JobParameters.class);
    when(mockParams2.getJobId()).thenReturn(2);
    when(mockParams2.getExtras()).thenReturn(new PersistableBundle());

    ServiceController<TestFlutterJobService> controller =
        Robolectric.buildService(TestFlutterJobService.class);
    TestFlutterJobService service = controller.get();
    controller.create();

    boolean keepAlive1 = service.onStartJob(mockParams1);
    assertTrue(keepAlive1);

    boolean keepAlive2 = service.onStartJob(mockParams2);
    assertFalse(keepAlive2); // Rejected because job 1 is already running
  }

  // Subclass to supply a mocked FlutterEngine and trace JobService lifecycle
  public static class TestFlutterJobService extends FlutterJobService {
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
  }
}
