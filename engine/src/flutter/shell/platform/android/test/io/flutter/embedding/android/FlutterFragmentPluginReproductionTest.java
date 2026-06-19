// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import androidx.fragment.app.FragmentActivity;
import androidx.test.core.app.ActivityScenario;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.FlutterInjector;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.embedding.engine.FlutterJNI;
import io.flutter.plugins.GeneratedPluginRegistrant;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class FlutterFragmentPluginReproductionTest {
  private FlutterJNI mockFlutterJNI;

  @Before
  public void setUp() {
    FlutterInjector.reset();
    GeneratedPluginRegistrant.clearRegisteredEngines();
    mockFlutterJNI = mock(FlutterJNI.class);
    when(mockFlutterJNI.isAttached()).thenReturn(true);
    FlutterJNI.Factory mockFlutterJNIFactory = mock(FlutterJNI.Factory.class);
    when(mockFlutterJNIFactory.provideFlutterJNI()).thenReturn(mockFlutterJNI);
    FlutterInjector.setInstance(
        new FlutterInjector.Builder().setFlutterJNIFactory(mockFlutterJNIFactory).build());
  }

  @After
  public void tearDown() {
    GeneratedPluginRegistrant.clearRegisteredEngines();
    FlutterInjector.reset();
  }

  @Test
  public void itRegistersPluginsOnNewEngineWhenFragmentActivityIsNotConfigurator() {
    // 1. Create the FlutterFragment with a new engine.
    FlutterFragment fragment = FlutterFragment.withNewEngine().build();

    // 2. Launch a standard FragmentActivity (which does NOT implement FlutterEngineConfigurator)
    // and attach the fragment.
    try (ActivityScenario<FragmentActivity> scenario =
        ActivityScenario.launch(FragmentActivity.class)) {
      scenario.onActivity(
          activity -> {
            activity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(android.R.id.content, fragment)
                .commitNow();

            // 3. Retrieve the engine.
            FlutterEngine engine = fragment.getFlutterEngine();
            org.junit.Assert.assertNotNull("FlutterEngine should not be null", engine);

            // 4. Assert that the engine has been registered with GeneratedPluginRegistrant.
            // Under the bug, this assertion will FAIL because the standard FragmentActivity
            // does not implement FlutterEngineConfigurator, and FlutterFragment fails to
            // register plugins itself.
            assertTrue(
                "Plugins should have been automatically registered on the new engine",
                GeneratedPluginRegistrant.getRegisteredEngines().contains(engine));
          });
    }
  }

  @Test
  public void itDoesNotRegisterPluginsOnInjectedEngineWhenFragmentActivityIsNotConfigurator() {
    // 1. Create and cache a FlutterEngine.
    android.content.Context ctx = androidx.test.core.app.ApplicationProvider.getApplicationContext();
    FlutterEngine cachedEngine =
        new FlutterEngine(ctx, new io.flutter.embedding.engine.loader.FlutterLoader(), mockFlutterJNI, null, false);
    io.flutter.embedding.engine.FlutterEngineCache.getInstance().put("my_cached_engine", cachedEngine);

    try {
      // 2. Create the FlutterFragment with the cached engine.
      FlutterFragment fragment = FlutterFragment.withCachedEngine("my_cached_engine").build();

      // 3. Launch a standard FragmentActivity and attach the fragment.
      try (ActivityScenario<FragmentActivity> scenario =
          ActivityScenario.launch(FragmentActivity.class)) {
        scenario.onActivity(
            activity -> {
              activity
                  .getSupportFragmentManager()
                  .beginTransaction()
                  .add(android.R.id.content, fragment)
                  .commitNow();

              // 4. Assert that the engine has NOT been registered automatically.
              org.junit.Assert.assertFalse(
                  "Plugins should not have been automatically registered on the injected engine",
                  GeneratedPluginRegistrant.getRegisteredEngines().contains(cachedEngine));
            });
      }
    } finally {
      // Clean up the cache to avoid leaking state.
      io.flutter.embedding.engine.FlutterEngineCache.getInstance().remove("my_cached_engine");
    }
  }
}
