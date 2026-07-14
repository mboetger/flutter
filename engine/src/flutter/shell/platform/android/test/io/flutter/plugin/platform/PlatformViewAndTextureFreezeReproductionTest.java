// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.plugin.platform;

import static org.junit.Assert.*;
import static org.mockito.Mockito.*;

import android.graphics.SurfaceTexture;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.engine.renderer.SurfaceTextureWrapper;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class PlatformViewAndTextureFreezeReproductionTest {

  @Test
  public void testSurfaceTextureWrapperRecoveryFromDetachException() {
    SurfaceTexture mockSurfaceTexture = mock(SurfaceTexture.class);

    // Configure the mock to simulate the Android framework behavior:
    // detachFromGLContext() throws IllegalStateException if called in the wrong state/context.
    doThrow(new IllegalStateException("Error during detachFromGLContext"))
        .when(mockSurfaceTexture)
        .detachFromGLContext();

    SurfaceTextureWrapper wrapper = new SurfaceTextureWrapper(mockSurfaceTexture);

    // 1. Initial attachment on the old context.
    wrapper.attachToGLContext(10);

    // 2. Try to attach to a new context (e.g. after thread merge).
    // Because attached is true, the wrapper tries to call detachFromGLContext() first.
    // This will throw IllegalStateException.
    //
    // If the bug is present, this exception propagates, and attachToGLContext(20) is never called.
    // If the bug is fixed, the exception is caught, and attachToGLContext(20) is successfully called.
    try {
      wrapper.attachToGLContext(20);
    } catch (IllegalStateException e) {
      fail("attachToGLContext threw exception and failed to recover: " + e);
    }

    // Verify that attachToGLContext(20) was actually called on the underlying SurfaceTexture.
    verify(mockSurfaceTexture, times(1)).attachToGLContext(20);
  }
}
