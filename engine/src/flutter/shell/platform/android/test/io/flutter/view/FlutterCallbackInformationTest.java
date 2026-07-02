// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.view;

import static org.junit.Assert.assertThrows;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class FlutterCallbackInformationTest {
  @Test
  public void testLookupCallbackInformationThrowsBeforeInit() {
    // Calling lookupCallbackInformation before the native library is loaded
    // should throw IllegalStateException.
    assertThrows(
        IllegalStateException.class,
        () -> {
          FlutterCallbackInformation.lookupCallbackInformation(0);
        });
  }
}
