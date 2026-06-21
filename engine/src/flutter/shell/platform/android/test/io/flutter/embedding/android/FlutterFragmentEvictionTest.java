// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import androidx.fragment.app.FragmentActivity;
import androidx.test.core.app.ActivityScenario;
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

@RunWith(AndroidJUnit4.class)
public class FlutterFragmentEvictionTest {
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
  public void testFragmentEvictionFlow() {
    // 1. Set up a cached FlutterEngine.
    FlutterLoader mockFlutterLoader = mock(FlutterLoader.class);
    FlutterJNI mockFlutterJni = mock(FlutterJNI.class);
    when(mockFlutterJni.isAttached()).thenReturn(true);
    FlutterEngine cachedEngine = new FlutterEngine(ctx, mockFlutterLoader, mockFlutterJni);
    FlutterEngineCache.getInstance().put("my_cached_engine", cachedEngine);

    // 2. Create Fragment A.
    FlutterFragment fragmentA =
        FlutterFragment.withCachedEngine("my_cached_engine").build();

    // 3. Create Fragment B.
    FlutterFragment fragmentB =
        FlutterFragment.withCachedEngine("my_cached_engine").build();

    try (ActivityScenario<FragmentActivity> scenario =
        ActivityScenario.launch(FragmentActivity.class)) {
      scenario.onActivity(
          activity -> {
            // 4. Add/Launch Fragment A.
            activity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(android.R.id.content, fragmentA, "A")
                .commitNow();

            // Verify Fragment A is attached.
            assertTrue(fragmentA.delegate.isAttached());

            // 5. Add/Launch Fragment B (this will evict Fragment A from the cached engine).
            activity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(android.R.id.content, fragmentB, "B")
                .commitNow();

            // Verify Fragment B is attached.
            assertTrue(fragmentB.delegate.isAttached());

            // Verify Fragment A is evicted/detached from the engine.
            assertFalse(fragmentA.delegate.isAttached());

            // 6. Remove Fragment B.
            activity
                .getSupportFragmentManager()
                .beginTransaction()
                .remove(fragmentB)
                .commitNow();
          });

      // 7. Stop and start the host activity to trigger Fragment A's onStart() and verify recovery.
      scenario.moveToState(androidx.lifecycle.Lifecycle.State.CREATED);
      scenario.moveToState(androidx.lifecycle.Lifecycle.State.RESUMED);

      scenario.onActivity(
          activity -> {
            // 8. Verify that Fragment A is successfully re-attached.
            assertTrue(fragmentA.delegate.isAttached());
          });
    }
  }
}
