// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.mock;

import android.content.Context;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.engine.loader.FlutterLoader;
import io.flutter.util.TraceSection;
import java.util.Queue;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.shadows.ShadowTrace;

@RunWith(AndroidJUnit4.class)
public class EmbeddingTracingTest {
  private final Context ctx = ApplicationProvider.getApplicationContext();

  @Before
  public void setUp() {
    ShadowTrace.reset();
  }

  @Test
  public void testTraceSectionWorksWithShadowTrace() {
    TraceSection.begin("TestSection");
    TraceSection.end();

    Queue<String> previousSections = ShadowTrace.getPreviousSections();
    assertTrue(previousSections.contains("TestSection"));
  }

  @Test
  public void testFlutterJNIInitIsTraced() {
    FlutterJNI flutterJNI = new FlutterJNI();
    try {
      flutterJNI.init(ctx, new String[0], null, "", "", 0, 0);
    } catch (UnsatisfiedLinkError e) {
      // Expected since native library is not loaded.
    }

    Queue<String> previousSections = ShadowTrace.getPreviousSections();
    assertTrue(
        "FlutterJNI.init should be traced",
        previousSections.contains("FlutterJNI#init"));
  }

  @Test
  public void testFlutterJNIAttachToNativeIsTraced() {
    FlutterJNI flutterJNI = new FlutterJNI();
    try {
      flutterJNI.attachToNative();
    } catch (UnsatisfiedLinkError e) {
      // Expected since native library is not loaded.
    }

    Queue<String> previousSections = ShadowTrace.getPreviousSections();
    assertTrue(
        "FlutterJNI.attachToNative should be traced",
        previousSections.contains("FlutterJNI#attachToNative"));
  }

  @Test
  public void testFlutterJNISpawnIsTraced() {
    FlutterJNI flutterJNI = new FlutterJNI() {
      @Override
      public boolean isAttached() {
        return true;
      }
    };
    try {
      flutterJNI.spawn(null, null, null, null, 0);
    } catch (UnsatisfiedLinkError e) {
      // Expected since native library is not loaded.
    } catch (RuntimeException e) {
      // If some check fails, let's catch it.
    }

    Queue<String> previousSections = ShadowTrace.getPreviousSections();
    assertTrue(
        "FlutterJNI.spawn should be traced",
        previousSections.contains("FlutterJNI#spawn"));
  }

  @Test
  public void testFlutterJNIDetachFromNativeIsTraced() {
    FlutterJNI flutterJNI = new FlutterJNI() {
      @Override
      public boolean isAttached() {
        return true;
      }
    };
    try {
      flutterJNI.detachFromNativeAndReleaseResources();
    } catch (UnsatisfiedLinkError e) {
      // Expected since native library is not loaded.
    } catch (RuntimeException e) {
      // If some check fails, let's catch it.
    }

    Queue<String> previousSections = ShadowTrace.getPreviousSections();
    assertTrue(
        "FlutterJNI.detachFromNativeAndReleaseResources should be traced",
        previousSections.contains("FlutterJNI#detachFromNativeAndReleaseResources"));
  }

  @Test
  public void testFlutterEngineConstructorIsTraced() {
    FlutterLoader mockFlutterLoader = mock(FlutterLoader.class);
    FlutterJNI mockFlutterJNI = mock(FlutterJNI.class);

    try {
      new FlutterEngine(ctx, mockFlutterLoader, mockFlutterJNI);
    } catch (Exception e) {
      // Ignore potential construction setup exceptions.
    }

    Queue<String> previousSections = ShadowTrace.getPreviousSections();
    assertTrue(
        "FlutterEngine constructor should be traced",
        previousSections.contains("FlutterEngine#Constructor"));
  }
}
