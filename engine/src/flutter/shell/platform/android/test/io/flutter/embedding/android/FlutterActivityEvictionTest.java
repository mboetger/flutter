// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.FlutterInjector;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.embedding.engine.FlutterEngineCache;
import io.flutter.embedding.engine.FlutterJNI;
import io.flutter.embedding.engine.loader.FlutterLoader;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

@RunWith(AndroidJUnit4.class)
public class FlutterActivityEvictionTest {
  private final Context ctx = ApplicationProvider.getApplicationContext();

  @Before
  public void setUp() {
    FlutterInjector.reset();
    FlutterJNI mockFlutterJNI = mock(FlutterJNI.class);
    when(mockFlutterJNI.isAttached()).thenReturn(true);
    FlutterJNI.Factory mockFlutterJNIFactory = mock(FlutterJNI.Factory.class);
    when(mockFlutterJNIFactory.provideFlutterJNI()).thenReturn(mockFlutterJNI);
    FlutterInjector.setInstance(
        new FlutterInjector.Builder().setFlutterJNIFactory(mockFlutterJNIFactory).build());
  }

  @After
  public void tearDown() {
    FlutterEngineCache.getInstance().clear();
    FlutterInjector.reset();
  }

  @Test
  public void testEvictionFlow() {
    // 1. Set up a cached FlutterEngine.
    FlutterLoader mockFlutterLoader = mock(FlutterLoader.class);
    FlutterJNI mockFlutterJni = mock(FlutterJNI.class);
    when(mockFlutterJni.isAttached()).thenReturn(true);
    FlutterEngine cachedEngine = new FlutterEngine(ctx, mockFlutterLoader, mockFlutterJni);
    FlutterEngineCache.getInstance().put("my_cached_engine", cachedEngine);

    // 2. Create/launch Activity A with the cached engine.
    Intent intentA = FlutterActivity.withCachedEngine("my_cached_engine").build(ctx);
    ActivityController<FlutterActivity> controllerA =
        Robolectric.buildActivity(FlutterActivity.class, intentA);
    FlutterActivity activityA = controllerA.get();
    controllerA.create().start().resume();

    // Verify activity A is attached and delegate is running.
    assertTrue(activityA.delegate.isAttached());

    // 3. Create/launch Activity B with the same cached engine.
    Intent intentB = FlutterActivity.withCachedEngine("my_cached_engine").build(ctx);
    ActivityController<FlutterActivity> controllerB =
        Robolectric.buildActivity(FlutterActivity.class, intentB);
    FlutterActivity activityB = controllerB.get();
    controllerB.create().start().resume();

    // 4. Verify that Activity A is evicted/detached.
    // When Activity B is resumed/started/attached, it should evict Activity A from the engine.
    assertFalse(activityA.delegate.isAttached());

    // 5. Destroy Activity B.
    controllerB.pause().stop().destroy();

    // 6. Resume/start Activity A again.
    // In Robolectric, we simulate resuming Activity A.
    controllerA.pause().stop();
    controllerA.start().resume();

    // 7. Verify that Activity A is successfully re-attached and responsive.
    assertTrue(activityA.delegate.isAttached());
  }
}
