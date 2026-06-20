// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package dev.flutter.plugins.integration_test;

import android.app.Instrumentation;
import android.app.UiAutomation;
import android.graphics.Bitmap;
import android.os.Bundle;
import androidx.test.platform.app.InstrumentationRegistry;
import org.junit.Test;
import java.io.IOException;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

public class FlutterDeviceScreenshotReproductionTest {

    @Test
    public void testHasInstrumentationReturnsTrueUnderInstrumentation() {
        // Register a mock Instrumentation to simulate running under instrumentation.
        InstrumentationRegistry.registerInstance(mock(Instrumentation.class), mock(Bundle.class));

        // Under instrumentation, hasInstrumentation() should be true.
        // Currently, it is hardcoded to return false, so this assertion will FAIL.
        assertTrue("Expected hasInstrumentation() to be true when running under instrumentation",
                FlutterDeviceScreenshot.hasInstrumentation());
    }

    @Test
    public void testCaptureWithUiAutomationThrowsIOExceptionWhenScreenshotFails() {
        // Set up mock Instrumentation and UiAutomation.
        Instrumentation mockInstrumentation = mock(Instrumentation.class);
        UiAutomation mockUiAutomation = mock(UiAutomation.class);

        when(mockInstrumentation.getUiAutomation()).thenReturn(mockUiAutomation);
        // Simulate a screenshot failure by returning null.
        when(mockUiAutomation.takeScreenshot()).thenReturn(null);

        InstrumentationRegistry.registerInstance(mockInstrumentation, mock(Bundle.class));

        // If captureWithUiAutomation is implemented correctly, it should attempt to take
        // a screenshot and throw an IOException if it fails (returns null).
        // Currently, it is a stub returning new byte[0], so this will FAIL (no exception thrown).
        assertThrows("Expected captureWithUiAutomation to throw IOException when takeScreenshot returns null",
                IOException.class, () -> {
                    FlutterDeviceScreenshot.captureWithUiAutomation();
                });
    }

    @Test
    public void testCaptureWithUiAutomationSuccess() throws IOException {
        // Set up mock Instrumentation, UiAutomation, and Bitmap.
        Instrumentation mockInstrumentation = mock(Instrumentation.class);
        UiAutomation mockUiAutomation = mock(UiAutomation.class);
        Bitmap mockBitmap = mock(Bitmap.class);

        when(mockInstrumentation.getUiAutomation()).thenReturn(mockUiAutomation);
        when(mockUiAutomation.takeScreenshot()).thenReturn(mockBitmap);

        // Mock bitmap compression to write some dummy bytes
        when(mockBitmap.compress(
                org.mockito.ArgumentMatchers.any(Bitmap.CompressFormat.class),
                org.mockito.ArgumentMatchers.anyInt(),
                org.mockito.ArgumentMatchers.any(java.io.OutputStream.class)))
            .thenAnswer(invocation -> {
                java.io.OutputStream os = invocation.getArgument(2);
                os.write(new byte[]{1, 2, 3, 4});
                return true;
            });

        InstrumentationRegistry.registerInstance(mockInstrumentation, mock(Bundle.class));

        byte[] result = FlutterDeviceScreenshot.captureWithUiAutomation();

        // Verify result contains the expected bytes
        org.junit.Assert.assertArrayEquals(new byte[]{1, 2, 3, 4}, result);

        // Verify bitmap was recycled
        org.mockito.Mockito.verify(mockBitmap).recycle();
    }
}
