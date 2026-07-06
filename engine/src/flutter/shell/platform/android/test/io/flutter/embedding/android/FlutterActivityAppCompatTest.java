// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import androidx.appcompat.app.AppCompatActivity;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.FlutterInjector;
import io.flutter.embedding.engine.FlutterJNI;
import io.flutter.plugins.GeneratedPluginRegistrant;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

/**
 * Reproduction test for GitHub issue flutter/flutter#71208: FlutterActivity and
 * FlutterFragmentActivity should extend AppCompatActivity instead of Activity / FragmentActivity to
 * support custom view inflation with themes and AppCompat features.
 */
@RunWith(AndroidJUnit4.class)
public class FlutterActivityAppCompatTest {
  private final Context ctx = ApplicationProvider.getApplicationContext();

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
  }

  @After
  public void tearDown() {
    GeneratedPluginRegistrant.clearRegisteredEngines();
    FlutterInjector.reset();
  }

  @Test
  public void flutterActivity_extendsAppCompatActivity() {
    assertTrue(
        "FlutterActivity should extend AppCompatActivity to support AppCompat themes and custom views (issue #71208)",
        AppCompatActivity.class.isAssignableFrom(FlutterActivity.class));
  }

  @Test
  public void flutterFragmentActivity_extendsAppCompatActivity() {
    assertTrue(
        "FlutterFragmentActivity should extend AppCompatActivity to support AppCompat themes and custom views (issue #71208)",
        AppCompatActivity.class.isAssignableFrom(FlutterFragmentActivity.class));
  }

  @Test
  public void flutterActivityInstance_isInstanceOfAppCompatActivity() {
    Intent intent = FlutterActivity.createDefaultIntent(ctx);
    ActivityController<FlutterActivity> activityController =
        Robolectric.buildActivity(FlutterActivity.class, intent);
    FlutterActivity activity = activityController.get();

    assertTrue(
        "FlutterActivity instance should be an instance of AppCompatActivity (issue #71208)",
        AppCompatActivity.class.isInstance(activity));
  }

  @Test
  public void flutterFragmentActivityInstance_isInstanceOfAppCompatActivity() {
    Intent intent = FlutterFragmentActivity.createDefaultIntent(ctx);
    ActivityController<FlutterFragmentActivity> activityController =
        Robolectric.buildActivity(FlutterFragmentActivity.class, intent);
    FlutterFragmentActivity activity = activityController.get();

    assertTrue(
        "FlutterFragmentActivity instance should be an instance of AppCompatActivity (issue #71208)",
        AppCompatActivity.class.isInstance(activity));
  }
}
