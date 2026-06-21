// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentActivity;
import androidx.test.core.app.ActivityScenario;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.embedding.engine.FlutterJNI;
import io.flutter.embedding.engine.loader.FlutterLoader;
import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class FlutterFragmentReproduceTest {

  public static class MyFlutterFragment extends FlutterFragment {
    public static FlutterEngine mockEngine;

    public MyFlutterFragment() {}

    @Nullable
    @Override
    public FlutterEngine provideFlutterEngine(@NonNull Context context) {
      return mockEngine;
    }
  }

  @After
  public void tearDown() {
    MyFlutterFragment.mockEngine = null;
    MyFlutterActivity.mockEngine = null;
    MyFlutterFragmentActivity.mockEngine = null;
  }

  private FlutterEngine createTestEngine() {
    FlutterLoader mockFlutterLoader = mock(FlutterLoader.class);
    FlutterJNI mockFlutterJni = mock(FlutterJNI.class);
    when(mockFlutterJni.isAttached()).thenReturn(true);
    return new FlutterEngine(
        ApplicationProvider.getApplicationContext(),
        mockFlutterLoader,
        mockFlutterJni,
        new String[] {},
        /*automaticallyRegisterPlugins=*/ false);
  }

  @Test
  public void newEngineFragmentBuilder_respectsHostEngine_whenEngineProvidedByHost() {
    MyFlutterFragment.mockEngine = createTestEngine();

    MyFlutterFragment fragment =
        new FlutterFragment.NewEngineFragmentBuilder(MyFlutterFragment.class)
            .shouldAttachEngineToActivity(false)
            .build();

    try (ActivityScenario<FragmentActivity> scenario =
        ActivityScenario.launch(FragmentActivity.class)) {
      scenario.onActivity(
          activity -> {
            activity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(android.R.id.content, fragment)
                .commitNow();

            // The engine was provided by the host. So shouldDestroyEngineWithHost() should be false.
            // Currently, it will fail because NewEngineFragmentBuilder always sets
            // ARG_DESTROY_ENGINE_WITH_FRAGMENT to true, which overrides the dynamic host check.
            assertFalse(fragment.shouldDestroyEngineWithHost());
          });
    }
  }

  @Test
  public void newEngineInGroupFragmentBuilder_respectsHostEngine_whenEngineProvidedByHost() {
    MyFlutterFragment.mockEngine = createTestEngine();

    MyFlutterFragment fragment =
        new FlutterFragment.NewEngineInGroupFragmentBuilder(MyFlutterFragment.class, "group_id")
            .shouldAttachEngineToActivity(false)
            .build();

    try (ActivityScenario<FragmentActivity> scenario =
        ActivityScenario.launch(FragmentActivity.class)) {
      scenario.onActivity(
          activity -> {
            activity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(android.R.id.content, fragment)
                .commitNow();

            assertFalse(fragment.shouldDestroyEngineWithHost());
          });
    }
  }

  public static class MyFlutterActivity extends FlutterActivity {
    public static FlutterEngine mockEngine;

    public MyFlutterActivity() {}

    @Override
    @SuppressLint("MissingSuperCall")
    protected void onCreate(@Nullable Bundle savedInstanceState) {
      super.delegate = new FlutterActivityAndFragmentDelegate(this);
      super.delegate.setUpFlutterEngine();
    }

    @Nullable
    @Override
    public FlutterEngine provideFlutterEngine(@NonNull Context context) {
      return mockEngine;
    }
  }

  @Test
  public void newEngineIntentBuilder_respectsHostEngine_whenEngineProvidedByHost() {
    MyFlutterActivity.mockEngine = createTestEngine();

    Intent intent =
        new FlutterActivity.NewEngineIntentBuilder(MyFlutterActivity.class)
            .build(ApplicationProvider.getApplicationContext());

    try (ActivityScenario<MyFlutterActivity> scenario = ActivityScenario.launch(intent)) {
      scenario.onActivity(
          activity -> {
            assertFalse(activity.shouldDestroyEngineWithHost());
          });
    }
  }

  @Test
  public void newEngineInGroupIntentBuilder_respectsHostEngine_whenEngineProvidedByHost() {
    MyFlutterActivity.mockEngine = createTestEngine();

    Intent intent =
        new FlutterActivity.NewEngineInGroupIntentBuilder(MyFlutterActivity.class, "group_id")
            .build(ApplicationProvider.getApplicationContext());

    try (ActivityScenario<MyFlutterActivity> scenario = ActivityScenario.launch(intent)) {
      scenario.onActivity(
          activity -> {
            assertFalse(activity.shouldDestroyEngineWithHost());
          });
    }
  }

  public static class MyFlutterFragmentActivity extends FlutterFragmentActivity {
    public static FlutterEngine mockEngine;

    public MyFlutterFragmentActivity() {}

    @Nullable
    @Override
    public FlutterEngine provideFlutterEngine(@NonNull Context context) {
      return mockEngine;
    }
  }

  @Test
  public void fragmentActivity_newEngineIntentBuilder_respectsHostEngine_whenEngineProvidedByHost() {
    MyFlutterFragmentActivity.mockEngine = createTestEngine();

    Intent intent =
        new FlutterFragmentActivity.NewEngineIntentBuilder(MyFlutterFragmentActivity.class)
            .build(ApplicationProvider.getApplicationContext());

    try (ActivityScenario<MyFlutterFragmentActivity> scenario = ActivityScenario.launch(intent)) {
      scenario.onActivity(
          activity -> {
            FlutterFragment fragment = (FlutterFragment) activity.getSupportFragmentManager().findFragmentByTag("flutter_fragment");
            assertNotNull(fragment);
            assertFalse(fragment.shouldDestroyEngineWithHost());
          });
    }
  }
}
