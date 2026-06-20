// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import java.lang.reflect.Field;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class FlutterJNIReproduceTest {
  @Test
  public void testUpdateRefreshRateHandlesUnsatisfiedLinkErrorGracefully() throws Exception {
    // Force FlutterJNI.loadLibraryCalled to be true using reflection,
    // but save the original value to restore it in the finally block.
    Field field = FlutterJNI.class.getDeclaredField("loadLibraryCalled");
    field.setAccessible(true);
    boolean originalValue = (boolean) field.get(null);
    field.set(null, true);

    try {
      // Call updateRefreshRate() on a real FlutterJNI instance.
      FlutterJNI flutterJNI = new FlutterJNI();

      // Since the native library is not loaded on the host JVM, this call
      // will throw UnsatisfiedLinkError if not handled gracefully.
      flutterJNI.updateRefreshRate();
    } finally {
      // Restore the original value to prevent test pollution in other tests
      // running in the same JVM process.
      field.set(null, originalValue);
    }
  }
}
