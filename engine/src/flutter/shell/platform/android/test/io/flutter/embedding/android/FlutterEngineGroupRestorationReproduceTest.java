// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.FlutterInjector;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.embedding.engine.FlutterEngineCache;
import io.flutter.embedding.engine.FlutterEngineGroup;
import io.flutter.embedding.engine.FlutterJNI;
import io.flutter.embedding.engine.loader.FlutterLoader;
import io.flutter.plugins.GeneratedPluginRegistrant;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;

@RunWith(AndroidJUnit4.class)
public class FlutterEngineGroupRestorationReproduceTest {
  private final Context ctx = ApplicationProvider.getApplicationContext();
  private static final String CACHED_ENGINE_ID = "my_cached_engine";
  private static FlutterEngineGroup engineGroup;

  // Track engines to prevent memory leaks in Robolectric
  private static FlutterEngine initialEngine;
  private static FlutterEngine recreatedEngine;

  @Before
  public void setUp() {
    FlutterInjector.reset();
    GeneratedPluginRegistrant.clearRegisteredEngines();
    FlutterJNI mockFlutterJNI = mock(FlutterJNI.class);
    when(mockFlutterJNI.isAttached()).thenReturn(true);
    FlutterJNI.Factory mockFlutterJNIFactory = mock(FlutterJNI.Factory.class);
    when(mockFlutterJNIFactory.provideFlutterJNI()).thenReturn(mockFlutterJNI);
    FlutterInjector.setInstance(
        new FlutterInjector.Builder().setFlutterJNIFactory(mockFlutterJNIFactory).build());

    engineGroup = new FlutterEngineGroup(ctx);
  }

  @After
  public void tearDown() {
    // Explicitly destroy engines to prevent leaks
    if (initialEngine != null) {
      initialEngine.destroy();
      initialEngine = null;
    }
    if (recreatedEngine != null) {
      recreatedEngine.destroy();
      recreatedEngine = null;
    }

    GeneratedPluginRegistrant.clearRegisteredEngines();
    FlutterInjector.reset();
    FlutterEngineCache.getInstance().clear();
    engineGroup = null;
  }

  static class RestorationTestActivity extends FlutterActivity {
    @Override
    public boolean shouldRestoreAndSaveState() {
      return true;
    }

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
      // Workaround: Recreate engine and put it in cache if missing
      if (!FlutterEngineCache.getInstance().contains(CACHED_ENGINE_ID)) {
        FlutterEngine engine = engineGroup.createAndRunEngine(
            getApplicationContext(),
            null
        );
        FlutterEngineCache.getInstance().put(CACHED_ENGINE_ID, engine);
        recreatedEngine = engine;
      }
      super.onCreate(savedInstanceState);
    }
  }

  @Test
  public void testRestorationWithCachedEngineRecreation() {
    // 1. Create the initial engine and cache it
    initialEngine = engineGroup.createAndRunEngine(ctx, null);
    FlutterEngineCache.getInstance().put(CACHED_ENGINE_ID, initialEngine);

    // 2. Launch the activity
    Intent intent = FlutterActivity.withCachedEngine(CACHED_ENGINE_ID).build(ctx);
    ActivityController<RestorationTestActivity> controller =
        Robolectric.buildActivity(RestorationTestActivity.class, intent);
    controller.create().start().resume().visible();

    // 3. Save instance state and destroy activity (simulating process death)
    Bundle savedInstanceState = new Bundle();
    controller.saveInstanceState(savedInstanceState);
    controller.pause().stop().destroy();

    // Destroy initial engine and clear cache to simulate process death
    if (initialEngine != null) {
      initialEngine.destroy();
      initialEngine = null;
    }
    FlutterEngineCache.getInstance().clear();

    // 4. Recreate the activity with the saved state.
    // This will trigger RestorationTestActivity.onCreate to recreate the engine
    // and put it in the cache, but with waitForRestorationData = false.
    ActivityController<RestorationTestActivity> controller2 =
        Robolectric.buildActivity(RestorationTestActivity.class, intent);

    try {
      controller2.create(savedInstanceState).start().restoreInstanceState(savedInstanceState).resume().visible();

      // 5. Verify the recreated engine's RestorationChannel has waitForRestorationData = true.
      // We expect this assertion to FAIL under the current bug (because it is actually false),
      // proving the bug is reproduced.
      assertNotNull("Expected recreated engine to be in cache", recreatedEngine);

      assertTrue("Expected waitForRestorationData to be true for state restoration",
          recreatedEngine.getRestorationChannel().waitForRestorationData);
    } finally {
      controller2.destroy();
    }
  }
}
