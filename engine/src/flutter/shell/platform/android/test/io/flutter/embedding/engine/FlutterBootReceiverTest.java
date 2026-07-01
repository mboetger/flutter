// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine;

import static org.junit.Assert.fail;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.*;

import android.app.job.JobInfo;
import android.app.job.JobParameters;
import android.app.job.JobScheduler;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.FlutterInjector;
import io.flutter.embedding.engine.loader.FlutterLoader;
import io.flutter.view.FlutterCallbackInformation;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.util.List;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockedStatic;
import org.mockito.ArgumentCaptor;
import io.flutter.embedding.engine.dart.PlatformMessageHandler;
import org.robolectric.Robolectric;

@RunWith(AndroidJUnit4.class)
public class FlutterBootReceiverTest {

  private static final String RECEIVER_CLASS_NAME = "io.flutter.embedding.engine.FlutterBootReceiver";
  private static final String SERVICE_CLASS_NAME = "io.flutter.embedding.engine.FlutterBootJobService";

  @Before
  public void setUp() {
    FlutterInjector.reset();
  }

  @Test
  public void testBootReceiverClassExists() throws Exception {
    // This will throw ClassNotFoundException and fail the test if the class is missing.
    Class.forName(RECEIVER_CLASS_NAME);
  }

  @Test
  public void testBootJobServiceClassExists() throws Exception {
    // This will throw ClassNotFoundException and fail the test if the class is missing.
    Class.forName(SERVICE_CLASS_NAME);
  }

  @Test
  public void testBootReceiverRegisteredInManifestButDisabledByDefault() throws Exception {
    Context context = ApplicationProvider.getApplicationContext();
    Intent intent = new Intent(Intent.ACTION_BOOT_COMPLETED);
    intent.setPackage(context.getPackageName());

    // Use MATCH_DISABLED_COMPONENTS to find it even if it is disabled.
    List<ResolveInfo> receivers = context.getPackageManager().queryBroadcastReceivers(
        intent, PackageManager.MATCH_DISABLED_COMPONENTS
    );

    ResolveInfo bootReceiverInfo = null;
    for (ResolveInfo resolveInfo : receivers) {
      if (resolveInfo.activityInfo.name.equals(RECEIVER_CLASS_NAME)) {
        bootReceiverInfo = resolveInfo;
        break;
      }
    }

    assertTrue("FlutterBootReceiver is not registered in the manifest for ACTION_BOOT_COMPLETED", bootReceiverInfo != null);
    
    // Assert it is disabled by default in the manifest.
    assertFalse("FlutterBootReceiver should be disabled by default in the manifest for performance", bootReceiverInfo.activityInfo.enabled);
  }

  @Test
  public void testJobServiceRegisteredInManifestWithPermission() throws Exception {
    Context context = ApplicationProvider.getApplicationContext();
    ComponentName componentName = new ComponentName(context, SERVICE_CLASS_NAME);
    
    ServiceInfo serviceInfo = context.getPackageManager().getServiceInfo(componentName, 0);
    assertNotNull("FlutterBootJobService is not registered in the manifest", serviceInfo);
    assertEquals("FlutterBootJobService must require BIND_JOB_SERVICE permission", 
        "android.permission.BIND_JOB_SERVICE", serviceInfo.permission);
  }

  @Test
  public void testRegisterCallbackEnablesReceiver() throws Exception {
    Class<?> receiverClass = Class.forName(RECEIVER_CLASS_NAME);
    Context context = ApplicationProvider.getApplicationContext();
    ComponentName componentName = new ComponentName(context, receiverClass);

    // Call the public API to set the callback.
    Method setCallbackMethod = receiverClass.getMethod("setBootCallback", Context.class, long.class);
    setCallbackMethod.invoke(null, context, 12345L);

    // Verify it is programmatically enabled.
    int enabledSetting = context.getPackageManager().getComponentEnabledSetting(componentName);
    assertEquals(PackageManager.COMPONENT_ENABLED_STATE_ENABLED, enabledSetting);
  }

  @Test
  public void testUnregisterCallbackDisablesReceiver() throws Exception {
    Class<?> receiverClass = Class.forName(RECEIVER_CLASS_NAME);
    Context context = ApplicationProvider.getApplicationContext();
    ComponentName componentName = new ComponentName(context, receiverClass);

    // Call the public API to clear the callback.
    Method clearCallbackMethod = receiverClass.getMethod("clearBootCallback", Context.class);
    clearCallbackMethod.invoke(null, context);

    // Verify it is programmatically disabled.
    int enabledSetting = context.getPackageManager().getComponentEnabledSetting(componentName);
    assertEquals(PackageManager.COMPONENT_ENABLED_STATE_DISABLED, enabledSetting);
  }

  @Test
  public void testReceiverSchedulesJob() throws Exception {
    Class<?> receiverClass = Class.forName(RECEIVER_CLASS_NAME);
    Context context = ApplicationProvider.getApplicationContext();

    // Set the callback first.
    Method setCallbackMethod = receiverClass.getMethod("setBootCallback", Context.class, long.class);
    setCallbackMethod.invoke(null, context, 12345L);

    // Instantiate the receiver and call onReceive.
    Object receiver = receiverClass.getDeclaredConstructor().newInstance();
    Intent intent = new Intent(Intent.ACTION_BOOT_COMPLETED);

    receiverClass.getMethod("onReceive", Context.class, Intent.class).invoke(receiver, context, intent);

    // Verify that a job was scheduled.
    JobScheduler jobScheduler = (JobScheduler) context.getSystemService(Context.JOB_SCHEDULER_SERVICE);
    List<JobInfo> pendingJobs = jobScheduler.getAllPendingJobs();

    assertEquals("One job should be scheduled", 1, pendingJobs.size());
    JobInfo job = pendingJobs.get(0);
    assertEquals(SERVICE_CLASS_NAME, job.getService().getClassName());
  }

  @Test
  public void testJobServiceStartsEngine() throws Exception {
    Class<?> serviceClass = Class.forName(SERVICE_CLASS_NAME);
    Context context = ApplicationProvider.getApplicationContext();

    // Simulate storing the callback handle.
    Class<?> receiverClass = Class.forName(RECEIVER_CLASS_NAME);
    Method setCallbackMethod = receiverClass.getMethod("setBootCallback", Context.class, long.class);
    setCallbackMethod.invoke(null, context, 12345L);

    // Mock the callback information lookup.
    Constructor<FlutterCallbackInformation> constructor = FlutterCallbackInformation.class.getDeclaredConstructor(
        String.class, String.class, String.class
    );
    constructor.setAccessible(true);
    FlutterCallbackInformation fakeInfo = constructor.newInstance("myBootCallback", "MyClass", "package:my_app/main.dart");

    // Mock FlutterJNI and FlutterLoader.
    FlutterJNI mockFlutterJNI = mock(FlutterJNI.class);
    when(mockFlutterJNI.isAttached()).thenReturn(true);

    FlutterJNI.Factory mockFactory = mock(FlutterJNI.Factory.class);
    when(mockFactory.provideFlutterJNI()).thenReturn(mockFlutterJNI);

    FlutterLoader mockLoader = mock(FlutterLoader.class);
    when(mockLoader.automaticallyRegisterPlugins()).thenReturn(false);

    FlutterInjector.setInstance(
        new FlutterInjector.Builder()
            .setFlutterJNIFactory(mockFactory)
            .setFlutterLoader(mockLoader)
            .build()
    );

    try (MockedStatic<FlutterCallbackInformation> mockedCallbackInfo = mockStatic(FlutterCallbackInformation.class)) {
      mockedCallbackInfo.when(() -> FlutterCallbackInformation.lookupCallbackInformation(12345L))
          .thenReturn(fakeInfo);

      // Instantiate the JobService.
      FlutterBootJobService service = Robolectric.buildService(FlutterBootJobService.class).create().get();
      
      // Simulate onStartJob.
      JobParameters mockParams = mock(JobParameters.class);
      boolean hasAsyncWork = service.onStartJob(mockParams);

      // It should return true if it started asynchronous work (which it should, to run the engine).
      assertTrue("onStartJob should return true to keep the service running for async work", hasAsyncWork);



      // Verify FlutterLoader was initialized.
      verify(mockLoader, times(1)).startInitialization(any());
      verify(mockLoader, times(1)).ensureInitializationComplete(any(), any());

      // Verify that the callback was executed.
      assertTrue("Dart executor should be executing Dart", service.flutterEngine.getDartExecutor().isExecutingDart());
    }
  }

  @Test
  public void testJobFinishedChannelCallStopsServiceAndDestroysEngine() throws Exception {
    Context context = ApplicationProvider.getApplicationContext();
    
    Class<?> receiverClass = Class.forName(RECEIVER_CLASS_NAME);
    Method setCallbackMethod = receiverClass.getMethod("setBootCallback", Context.class, long.class);
    setCallbackMethod.invoke(null, context, 12345L);

    Constructor<FlutterCallbackInformation> constructor = FlutterCallbackInformation.class.getDeclaredConstructor(
        String.class, String.class, String.class
    );
    constructor.setAccessible(true);
    FlutterCallbackInformation fakeInfo = constructor.newInstance("myBootCallback", "MyClass", "package:my_app/main.dart");

    FlutterJNI mockFlutterJNI = mock(FlutterJNI.class);
    when(mockFlutterJNI.isAttached()).thenReturn(true);
    FlutterJNI.Factory mockFactory = mock(FlutterJNI.Factory.class);
    when(mockFactory.provideFlutterJNI()).thenReturn(mockFlutterJNI);
    FlutterLoader mockLoader = mock(FlutterLoader.class);
    when(mockLoader.automaticallyRegisterPlugins()).thenReturn(false);

    FlutterInjector.setInstance(
        new FlutterInjector.Builder()
            .setFlutterJNIFactory(mockFactory)
            .setFlutterLoader(mockLoader)
            .build()
    );

    try (MockedStatic<FlutterCallbackInformation> mockedCallbackInfo = mockStatic(FlutterCallbackInformation.class)) {
      mockedCallbackInfo.when(() -> FlutterCallbackInformation.lookupCallbackInformation(12345L))
          .thenReturn(fakeInfo);

      FlutterBootJobService service = Robolectric.buildService(FlutterBootJobService.class).create().get();

      JobParameters mockParams = mock(JobParameters.class);
      
      service.onStartJob(mockParams);

      assertNotNull("FlutterEngine should be created", service.flutterEngine);
      
      ArgumentCaptor<PlatformMessageHandler> messageHandlerCaptor = ArgumentCaptor.forClass(PlatformMessageHandler.class);
      verify(mockFlutterJNI).setPlatformMessageHandler(messageHandlerCaptor.capture());
      PlatformMessageHandler messenger = messageHandlerCaptor.getValue();

      io.flutter.plugin.common.MethodCall call = new io.flutter.plugin.common.MethodCall("jobFinished", null);
      java.nio.ByteBuffer encodedCall = io.flutter.plugin.common.StandardMethodCodec.INSTANCE.encodeMethodCall(call);
      encodedCall.position(0);
      
      messenger.handleMessageFromDart("io.flutter/boot", encodedCall, 0, 0);

      org.robolectric.shadows.ShadowLooper.idleMainLooper();

      org.robolectric.shadows.ShadowJobService shadow = org.robolectric.Shadows.shadowOf(service);
      assertTrue("jobFinished should be called", shadow.getIsJobFinished());
      assertFalse("needsReschedule should be false", shadow.getIsRescheduleNeeded());
      assertTrue("Engine should be destroyed (nullified)", service.flutterEngine == null);
    }
  }

  @Test
  public void testOnStopJobDestroysEngine() throws Exception {
    Context context = ApplicationProvider.getApplicationContext();
    
    Class<?> receiverClass = Class.forName(RECEIVER_CLASS_NAME);
    Method setCallbackMethod = receiverClass.getMethod("setBootCallback", Context.class, long.class);
    setCallbackMethod.invoke(null, context, 12345L);

    Constructor<FlutterCallbackInformation> constructor = FlutterCallbackInformation.class.getDeclaredConstructor(
        String.class, String.class, String.class
    );
    constructor.setAccessible(true);
    FlutterCallbackInformation fakeInfo = constructor.newInstance("myBootCallback", "MyClass", "package:my_app/main.dart");

    FlutterJNI mockFlutterJNI = mock(FlutterJNI.class);
    when(mockFlutterJNI.isAttached()).thenReturn(true);
    FlutterJNI.Factory mockFactory = mock(FlutterJNI.Factory.class);
    when(mockFactory.provideFlutterJNI()).thenReturn(mockFlutterJNI);
    FlutterLoader mockLoader = mock(FlutterLoader.class);
    when(mockLoader.automaticallyRegisterPlugins()).thenReturn(false);

    FlutterInjector.setInstance(
        new FlutterInjector.Builder()
            .setFlutterJNIFactory(mockFactory)
            .setFlutterLoader(mockLoader)
            .build()
    );

    try (MockedStatic<FlutterCallbackInformation> mockedCallbackInfo = mockStatic(FlutterCallbackInformation.class)) {
      mockedCallbackInfo.when(() -> FlutterCallbackInformation.lookupCallbackInformation(12345L))
          .thenReturn(fakeInfo);

      FlutterBootJobService service = Robolectric.buildService(FlutterBootJobService.class).create().get();

      JobParameters mockParams = mock(JobParameters.class);
      
      service.onStartJob(mockParams);
      assertNotNull("FlutterEngine should be created", service.flutterEngine);

      boolean reschedule = service.onStopJob(mockParams);
      assertFalse("onStopJob should return false", reschedule);
      assertTrue("Engine should be destroyed (nullified)", service.flutterEngine == null);
    }
  }

  @Test
  public void testTimeoutForceStopsService() throws Exception {
    Context context = ApplicationProvider.getApplicationContext();
    
    Class<?> receiverClass = Class.forName(RECEIVER_CLASS_NAME);
    Method setCallbackMethod = receiverClass.getMethod("setBootCallback", Context.class, long.class);
    setCallbackMethod.invoke(null, context, 12345L);

    Constructor<FlutterCallbackInformation> constructor = FlutterCallbackInformation.class.getDeclaredConstructor(
        String.class, String.class, String.class
    );
    constructor.setAccessible(true);
    FlutterCallbackInformation fakeInfo = constructor.newInstance("myBootCallback", "MyClass", "package:my_app/main.dart");

    FlutterJNI mockFlutterJNI = mock(FlutterJNI.class);
    when(mockFlutterJNI.isAttached()).thenReturn(true);
    FlutterJNI.Factory mockFactory = mock(FlutterJNI.Factory.class);
    when(mockFactory.provideFlutterJNI()).thenReturn(mockFlutterJNI);
    FlutterLoader mockLoader = mock(FlutterLoader.class);
    when(mockLoader.automaticallyRegisterPlugins()).thenReturn(false);

    FlutterInjector.setInstance(
        new FlutterInjector.Builder()
            .setFlutterJNIFactory(mockFactory)
            .setFlutterLoader(mockLoader)
            .build()
    );

    try (MockedStatic<FlutterCallbackInformation> mockedCallbackInfo = mockStatic(FlutterCallbackInformation.class)) {
      mockedCallbackInfo.when(() -> FlutterCallbackInformation.lookupCallbackInformation(12345L))
          .thenReturn(fakeInfo);

      FlutterBootJobService service = Robolectric.buildService(FlutterBootJobService.class).create().get();

      JobParameters mockParams = mock(JobParameters.class);
      
      service.onStartJob(mockParams);
      assertNotNull("FlutterEngine should be created", service.flutterEngine);

      org.robolectric.shadows.ShadowLooper.idleMainLooper(61, java.util.concurrent.TimeUnit.SECONDS);

      org.robolectric.shadows.ShadowJobService shadow = org.robolectric.Shadows.shadowOf(service);
      assertTrue("jobFinished should be called on timeout", shadow.getIsJobFinished());
      assertFalse("needsReschedule should be false on timeout", shadow.getIsRescheduleNeeded());
      assertTrue("Engine should be destroyed (nullified) on timeout", service.flutterEngine == null);
    }
  }

  @Test
  public void testInvalidCallbackHandleDoesNotStartEngine() throws Exception {
    Context context = ApplicationProvider.getApplicationContext();
    
    Class<?> receiverClass = Class.forName(RECEIVER_CLASS_NAME);
    Method setCallbackMethod = receiverClass.getMethod("setBootCallback", Context.class, long.class);
    setCallbackMethod.invoke(null, context, 0L);

    FlutterJNI mockFlutterJNI = mock(FlutterJNI.class);
    FlutterJNI.Factory mockFactory = mock(FlutterJNI.Factory.class);
    when(mockFactory.provideFlutterJNI()).thenReturn(mockFlutterJNI);
    FlutterLoader mockLoader = mock(FlutterLoader.class);

    FlutterInjector.setInstance(
        new FlutterInjector.Builder()
            .setFlutterJNIFactory(mockFactory)
            .setFlutterLoader(mockLoader)
            .build()
    );

    FlutterBootJobService service = Robolectric.buildService(FlutterBootJobService.class).create().get();

    JobParameters mockParams = mock(JobParameters.class);
    
    boolean started = service.onStartJob(mockParams);
    assertFalse("onStartJob should return false for invalid callback", started);
    assertTrue("Engine should not be created", service.flutterEngine == null);
  }
}
