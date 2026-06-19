// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.*;

import android.app.Activity;
import android.content.Context;
import androidx.annotation.NonNull;
import androidx.lifecycle.Lifecycle;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.embedding.engine.FlutterEngineCache;
import io.flutter.embedding.engine.FlutterJNI;
import io.flutter.embedding.engine.loader.FlutterLoader;
import io.flutter.embedding.engine.plugins.FlutterPlugin;
import io.flutter.embedding.engine.plugins.activity.ActivityAware;
import io.flutter.embedding.engine.plugins.activity.ActivityPluginBinding;
import io.flutter.embedding.engine.FlutterShellArgs;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

@RunWith(AndroidJUnit4.class)
@SuppressWarnings("deprecation")
public class SingleEngineMultiFragmentReproduceTest {
  private final Context ctx = ApplicationProvider.getApplicationContext();
  private static final String CACHED_ENGINE_ID = "cache_engine";

  private static class TrackingPlugin implements FlutterPlugin, ActivityAware {
    public Activity attachedActivity = null;
    public int attachCount = 0;
    public int detachCount = 0;

    @Override
    public void onAttachedToEngine(@NonNull FlutterPluginBinding binding) {}

    @Override
    public void onDetachedFromEngine(@NonNull FlutterPluginBinding binding) {}

    @Override
    public void onAttachedToActivity(@NonNull ActivityPluginBinding binding) {
      attachedActivity = binding.getActivity();
      attachCount++;
    }

    @Override
    public void onDetachedFromActivityForConfigChanges() {
      attachedActivity = null;
      detachCount++;
    }

    @Override
    public void onReattachedToActivityForConfigChanges(@NonNull ActivityPluginBinding binding) {
      attachedActivity = binding.getActivity();
      attachCount++;
    }

    @Override
    public void onDetachedFromActivity() {
      attachedActivity = null;
      detachCount++;
    }
  }

  private FlutterActivityAndFragmentDelegate.Host mockHost(
      Context context, FlutterEngine engine, Activity activity, String cachedEngineId) {
    FlutterActivityAndFragmentDelegate.Host host = mock(FlutterActivityAndFragmentDelegate.Host.class);
    when(host.getContext()).thenReturn(context);
    when(host.getActivity()).thenReturn(activity);
    when(host.getLifecycle()).thenReturn(mock(Lifecycle.class));
    when(host.getFlutterShellArgs()).thenReturn(new FlutterShellArgs(new String[] {}));
    when(host.getDartEntrypointFunctionName()).thenReturn("main");
    when(host.getDartEntrypointArgs()).thenReturn(null);
    when(host.getAppBundlePath()).thenReturn("/fake/path");
    when(host.getInitialRoute()).thenReturn("/");
    when(host.getRenderMode()).thenReturn(RenderMode.surface);
    when(host.getTransparencyMode()).thenReturn(TransparencyMode.transparent);
    when(host.provideFlutterEngine(any(Context.class))).thenReturn(engine);
    when(host.shouldAttachEngineToActivity()).thenReturn(true);
    when(host.shouldHandleDeeplinking()).thenReturn(false);
    when(host.shouldDestroyEngineWithHost()).thenReturn(false);
    when(host.shouldDispatchAppLifecycleState()).thenReturn(true);
    when(host.attachToEngineAutomatically()).thenReturn(true);
    when(host.getCachedEngineId()).thenReturn(cachedEngineId);
    return host;
  }

  @After
  public void tearDown() {
    FlutterEngineCache.getInstance().remove(CACHED_ENGINE_ID);
  }

  @Test
  public void testSingleEngineMultiFragmentLifecycleRace() {
    // 1. Create a single FlutterEngine with mocked JNI/Loader
    FlutterJNI mockFlutterJNI = mock(FlutterJNI.class);
    final java.util.concurrent.atomic.AtomicBoolean jniAttached = new java.util.concurrent.atomic.AtomicBoolean(false);
    when(mockFlutterJNI.isAttached()).thenAnswer(invocation -> jniAttached.get());
    doAnswer(invocation -> {
      jniAttached.set(true);
      return null;
    }).when(mockFlutterJNI).attachToNative();

    FlutterLoader mockFlutterLoader = mock(FlutterLoader.class);
    when(mockFlutterLoader.automaticallyRegisterPlugins()).thenReturn(false);

    FlutterEngine engine = new FlutterEngine(ctx, mockFlutterLoader, mockFlutterJNI);

    // Put the engine into the cache so that the delegate can find it.
    FlutterEngineCache.getInstance().put(CACHED_ENGINE_ID, engine);

    // Register our tracking plugin
    TrackingPlugin plugin = new TrackingPlugin();
    engine.getPlugins().add(plugin);

    // Create two Robolectric activities
    ActivityController<Activity> activityController1 = Robolectric.buildActivity(Activity.class).setup();
    Activity activity1 = activityController1.get();

    ActivityController<Activity> activityController2 = Robolectric.buildActivity(Activity.class).setup();
    Activity activity2 = activityController2.get();

    // 2. Setup first delegate (Activity 1)
    FlutterActivityAndFragmentDelegate.Host host1 = mockHost(ctx, engine, activity1, CACHED_ENGINE_ID);
    final FlutterActivityAndFragmentDelegate delegate1 = new FlutterActivityAndFragmentDelegate(host1);
    doAnswer(invocation -> {
      delegate1.onDestroyView();
      delegate1.onDetach();
      return null;
    }).when(host1).detachFromFlutterEngine();

    // 3. Setup second delegate (Activity 2)
    FlutterActivityAndFragmentDelegate.Host host2 = mockHost(ctx, engine, activity2, CACHED_ENGINE_ID);
    final FlutterActivityAndFragmentDelegate delegate2 = new FlutterActivityAndFragmentDelegate(host2);
    doAnswer(invocation -> {
      delegate2.onDestroyView();
      delegate2.onDetach();
      return null;
    }).when(host2).detachFromFlutterEngine();

    // --- Start Lifecycle Sequence ---

    // Step A: Activity 1 starts and attaches
    delegate1.onAttach(ctx);
    assertEquals(activity1, plugin.attachedActivity);
    assertEquals(1, plugin.attachCount);
    assertEquals(0, plugin.detachCount);

    delegate1.onCreateView(null, null, null, 0, true);
    delegate1.onStart();
    delegate1.onResume();

    // Step B: Activity 1 pauses, stops, and destroys view (goes to background, but NOT detached)
    delegate1.onPause();
    delegate1.onStop();
    delegate1.onDestroyView();

    // Step C: Activity 2 starts and attaches (causes Activity 1 to be evicted from engine)
    delegate2.onAttach(ctx);
    // Eviction should have run: host1.detachFromFlutterEngine() is called,
    // which calls delegate1.onDestroyView() and delegate1.onDetach().
    // So delegate1 is now detached, and the plugin is attached to Activity 2.
    assertEquals(activity2, plugin.attachedActivity);
    assertEquals(2, plugin.attachCount);
    assertEquals(1, plugin.detachCount);

    delegate2.onCreateView(null, null, null, 0, true);
    delegate2.onStart();
    delegate2.onResume();

    // Step D: Activity 2 pauses (user finishes Activity 2)
    delegate2.onPause();

    // Step E: Activity 1 starts and resumes (non-recreation flow: onAttach is NOT called again!)
    delegate1.onCreateView(null, null, null, 0, true);
    delegate1.onStart();
    delegate1.onResume();

    // Step F: Activity 2 is stopped, view destroyed, and detached
    delegate2.onStop();
    delegate2.onDestroyView();
    delegate2.onDetach();

    // --- Assertions ---
    // Under the bug, delegate1 is never re-attached on resume/start, and then delegate2's onDetach()
    // detaches the engine completely, leaving the plugin detached (attachedActivity is null).
    //
    // In a correct implementation, returning to Activity 1 should re-attach it to the engine,
    // and the plugin should STILL be attached to Activity 1.
    assertEquals("Plugin should be attached to Activity 1", activity1, plugin.attachedActivity);
  }
}
