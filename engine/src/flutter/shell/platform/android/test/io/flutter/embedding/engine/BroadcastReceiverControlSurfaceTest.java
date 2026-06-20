// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine;

import static org.junit.Assert.*;
import static org.mockito.Mockito.*;

import android.content.BroadcastReceiver;
import android.content.Context;
import androidx.annotation.NonNull;
import androidx.lifecycle.Lifecycle;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.engine.loader.FlutterLoader;
import io.flutter.embedding.engine.plugins.FlutterPlugin;
import io.flutter.embedding.engine.plugins.broadcastreceiver.BroadcastReceiverAware;
import io.flutter.embedding.engine.plugins.broadcastreceiver.BroadcastReceiverPluginBinding;
import io.flutter.plugin.platform.PlatformViewsController;
import io.flutter.plugin.platform.PlatformViewsController2;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class BroadcastReceiverControlSurfaceTest {

  @Test
  public void testBroadcastReceiverAttachmentWithoutLifecycle() {
    Context context = mock(Context.class);
    FlutterEngine flutterEngine = mock(FlutterEngine.class);
    PlatformViewsController platformViewsController = mock(PlatformViewsController.class);
    when(flutterEngine.getPlatformViewsController()).thenReturn(platformViewsController);
    PlatformViewsController2 platformViewsController2 = mock(PlatformViewsController2.class);
    when(flutterEngine.getPlatformViewsController2()).thenReturn(platformViewsController2);
    FlutterLoader flutterLoader = mock(FlutterLoader.class);

    FlutterEngineConnectionRegistry registry =
        new FlutterEngineConnectionRegistry(context, flutterEngine, flutterLoader, null);

    FakeBroadcastReceiverAwarePlugin plugin = new FakeBroadcastReceiverAwarePlugin();
    registry.add(plugin);

    BroadcastReceiver receiver = mock(BroadcastReceiver.class);
    
    // Call the new single-parameter method directly (compile-time safety)
    registry.attachToBroadcastReceiver(receiver);

    assertTrue("Plugin should be attached to the BroadcastReceiver", plugin.isAttached);
    assertEquals("Plugin should receive the correct BroadcastReceiver instance", receiver, plugin.capturedReceiver);
    
    registry.detachFromBroadcastReceiver();
    assertFalse("Plugin should be detached", plugin.isAttached);
    assertNull("Plugin's captured receiver should be cleared", plugin.capturedReceiver);
  }

  @Test
  @SuppressWarnings("deprecation")
  public void testDeprecatedBroadcastReceiverAttachmentWithLifecycle() {
    Context context = mock(Context.class);
    FlutterEngine flutterEngine = mock(FlutterEngine.class);
    PlatformViewsController platformViewsController = mock(PlatformViewsController.class);
    when(flutterEngine.getPlatformViewsController()).thenReturn(platformViewsController);
    PlatformViewsController2 platformViewsController2 = mock(PlatformViewsController2.class);
    when(flutterEngine.getPlatformViewsController2()).thenReturn(platformViewsController2);
    FlutterLoader flutterLoader = mock(FlutterLoader.class);

    FlutterEngineConnectionRegistry registry =
        new FlutterEngineConnectionRegistry(context, flutterEngine, flutterLoader, null);

    FakeBroadcastReceiverAwarePlugin plugin = new FakeBroadcastReceiverAwarePlugin();
    registry.add(plugin);

    BroadcastReceiver receiver = mock(BroadcastReceiver.class);
    Lifecycle lifecycle = mock(Lifecycle.class);
    
    // Call the deprecated two-parameter method directly to ensure backward compatibility
    registry.attachToBroadcastReceiver(receiver, lifecycle);

    assertTrue("Plugin should be attached to the BroadcastReceiver", plugin.isAttached);
    assertEquals("Plugin should receive the correct BroadcastReceiver instance", receiver, plugin.capturedReceiver);
    
    registry.detachFromBroadcastReceiver();
    assertFalse("Plugin should be detached", plugin.isAttached);
    assertNull("Plugin's captured receiver should be cleared", plugin.capturedReceiver);
  }

  private static class FakeBroadcastReceiverAwarePlugin implements FlutterPlugin, BroadcastReceiverAware {
    boolean isAttached = false;
    BroadcastReceiver capturedReceiver;

    @Override
    public void onAttachedToEngine(@NonNull FlutterPluginBinding binding) {}

    @Override
    public void onDetachedFromEngine(@NonNull FlutterPluginBinding binding) {}

    @Override
    public void onAttachedToBroadcastReceiver(@NonNull BroadcastReceiverPluginBinding binding) {
      isAttached = true;
      capturedReceiver = binding.getBroadcastReceiver();
    }

    @Override
    public void onDetachedFromBroadcastReceiver() {
      isAttached = false;
      capturedReceiver = null;
    }
  }
}
