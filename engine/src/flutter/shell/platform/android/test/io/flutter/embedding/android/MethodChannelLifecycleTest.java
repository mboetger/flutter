// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static android.os.Looper.getMainLooper;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Context;
import androidx.lifecycle.Lifecycle;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.FlutterInjector;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.embedding.engine.FlutterJNI;
import io.flutter.embedding.engine.FlutterShellArgs;
import io.flutter.embedding.engine.dart.PlatformMessageHandler;
import io.flutter.embedding.engine.loader.FlutterLoader;
import io.flutter.plugin.common.MethodCall;
import io.flutter.plugin.common.MethodChannel;
import io.flutter.plugin.common.MethodChannel.MethodCallHandler;
import io.flutter.plugin.common.MethodChannel.Result;
import io.flutter.plugin.common.StandardMethodCodec;
import java.nio.ByteBuffer;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@RunWith(AndroidJUnit4.class)
public class MethodChannelLifecycleTest {
  private final Context ctx = ApplicationProvider.getApplicationContext();
  @Mock FlutterJNI flutterJNI;
  @Mock FlutterActivityAndFragmentDelegate.Host mockHost;
  private boolean jniAttached;
  private boolean methodCalledInActiveState;
  private boolean methodCalledInDetachedState;
  private PlatformMessageHandler platformMessageHandler;

  @Before
  @SuppressWarnings("deprecation")
  public void setUp() {
    FlutterInjector.reset();
    MockitoAnnotations.openMocks(this);
    jniAttached = false;
    when(flutterJNI.isAttached()).thenAnswer(invocation -> jniAttached);
    doAnswer(
            invocation -> {
              jniAttached = true;
              return null;
            })
        .when(flutterJNI)
        .attachToNative();
    methodCalledInActiveState = false;
    methodCalledInDetachedState = false;
    platformMessageHandler = null;

    doAnswer(
            invocation -> {
              platformMessageHandler = invocation.getArgument(0);
              return null;
            })
        .when(flutterJNI)
        .setPlatformMessageHandler(any());
    doAnswer(
            invocation -> {
              String channel = invocation.getArgument(0);
              ByteBuffer message = invocation.getArgument(1);
              int replyId = invocation.getArgument(2);
              long messageData = invocation.getArgument(3);
              if (platformMessageHandler != null) {
                platformMessageHandler.handleMessageFromDart(
                    channel, message, replyId, messageData);
              }
              return null;
            })
        .when(flutterJNI)
        .handlePlatformMessage(any(), any(), anyInt(), anyLong());

    when(mockHost.getContext()).thenReturn(ctx);
    when(mockHost.getActivity()).thenReturn(mock(Activity.class));
    when(mockHost.getLifecycle()).thenReturn(mock(Lifecycle.class));
    when(mockHost.getFlutterShellArgs()).thenReturn(new FlutterShellArgs(new String[] {}));
    when(mockHost.getDartEntrypointFunctionName()).thenReturn("main");
    when(mockHost.getDartEntrypointArgs()).thenReturn(null);
    when(mockHost.getAppBundlePath()).thenReturn("/fake/path");
    when(mockHost.getInitialRoute()).thenReturn("/");
    when(mockHost.getRenderMode()).thenReturn(RenderMode.surface);
    when(mockHost.getTransparencyMode()).thenReturn(TransparencyMode.transparent);
    when(mockHost.shouldAttachEngineToActivity()).thenReturn(true);
    when(mockHost.shouldHandleDeeplinking()).thenReturn(false);
    when(mockHost.shouldDestroyEngineWithHost()).thenReturn(true);
    when(mockHost.shouldDispatchAppLifecycleState()).thenReturn(true);
    when(mockHost.attachToEngineAutomatically()).thenReturn(true);
  }

  @Test
  public void methodChannelInvokeMethodWorksInActiveState() {
    FlutterLoader flutterLoader = new FlutterLoader(flutterJNI);
    FlutterEngine flutterEngine = new FlutterEngine(ctx, flutterLoader, flutterJNI);
    when(mockHost.provideFlutterEngine(any(Context.class))).thenReturn(flutterEngine);

    MethodChannel channel =
        new MethodChannel(flutterEngine.getDartExecutor(), "flutter/test_channel");
    channel.setMethodCallHandler(
        new MethodCallHandler() {
          @Override
          public void onMethodCall(MethodCall call, Result result) {
            if ("check".equals(call.method)) {
              methodCalledInActiveState = true;
              result.success(null);
            }
          }
        });

    FlutterActivityAndFragmentDelegate delegate =
        new FlutterActivityAndFragmentDelegate(mockHost);
    delegate.onAttach(ctx);
    delegate.onResume();

    // Simulate Dart sending a method call over JNI while in active/resumed state.
    ByteBuffer message =
        StandardMethodCodec.INSTANCE.encodeMethodCall(new MethodCall("check", null));
    message.rewind();
    flutterJNI.handlePlatformMessage("flutter/test_channel", message, 1, 0);
    shadowOf(getMainLooper()).idle();

    // Verify that the MethodCallHandler was executed successfully.
    assertTrue(
        "MethodCallHandler should be executed in active/resumed state",
        methodCalledInActiveState);
  }

  @Test
  public void methodChannelInvokeMethodWorksInDetachedState() {
    FlutterLoader flutterLoader = new FlutterLoader(flutterJNI);
    FlutterEngine flutterEngine = new FlutterEngine(ctx, flutterLoader, flutterJNI);
    when(mockHost.provideFlutterEngine(any(Context.class))).thenReturn(flutterEngine);

    MethodChannel channel =
        new MethodChannel(flutterEngine.getDartExecutor(), "flutter/test_channel");
    channel.setMethodCallHandler(
        new MethodCallHandler() {
          @Override
          public void onMethodCall(MethodCall call, Result result) {
            if ("unbind".equals(call.method)) {
              methodCalledInDetachedState = true;
              result.success(null);
            }
          }
        });

    FlutterActivityAndFragmentDelegate delegate =
        new FlutterActivityAndFragmentDelegate(mockHost);
    delegate.onAttach(ctx);
    delegate.onResume();

    // When quitting the app (e.g. Recent Apps or Back button), onDetach is called.
    // In onDetach, delegate sends AppLifecycleState.detached over the lifecycle channel.
    // When Dart receives AppLifecycleState.detached, it invokes channel.invokeMethod("unbind").
    delegate.onDetach();

    // Simulate Dart sending the "unbind" method call over JNI in response to
    // AppLifecycleState.detached. Because flutterEngine.destroy() is called synchronously in
    // onDetach() right after sending AppLifecycleState.detached, the plugin registry is cleared
    // and JNI detached.
    ByteBuffer message =
        StandardMethodCodec.INSTANCE.encodeMethodCall(new MethodCall("unbind", null));
    message.rewind();
    flutterJNI.handlePlatformMessage("flutter/test_channel", message, 2, 0);
    shadowOf(getMainLooper()).idle();

    // Assert that the Kotlin/Java handler was executed.
    // This assertion FAILS as expected because MethodChannel does not work in Detached state.
    assertTrue(
        "MethodCallHandler should be executed when invoked during detached state",
        methodCalledInDetachedState);
  }
}
