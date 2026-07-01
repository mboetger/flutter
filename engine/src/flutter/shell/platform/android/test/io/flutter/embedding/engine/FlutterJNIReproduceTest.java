// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.robolectric.Shadows.shadowOf;

import android.content.res.AssetManager;
import android.os.Looper;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.engine.dart.DartExecutor;
import io.flutter.plugin.common.EventChannel;
import io.flutter.plugin.common.MethodCall;
import io.flutter.plugin.common.MethodChannel;
import io.flutter.plugin.common.StandardMethodCodec;
import java.nio.ByteBuffer;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class FlutterJNIReproduceTest {

  private static class TestFlutterJNI extends FlutterJNI {
    @Override
    public void cleanupMessageData(long messageData) {
      // Do nothing to avoid UnsatisfiedLinkError since the native library is not loaded.
    }
  }

  @Test
  public void dispatchPlatformMessage_onBackgroundThread() throws Throwable {
    final FlutterJNI flutterJNI = new TestFlutterJNI();
    final AtomicReference<Throwable> thrown = new AtomicReference<>();
    final CountDownLatch latch = new CountDownLatch(1);

    Thread thread = new Thread(new Runnable() {
      @Override
      public void run() {
        try {
          flutterJNI.dispatchPlatformMessage("test_channel", ByteBuffer.allocate(0), 0, 0);
        } catch (Throwable t) {
          thrown.set(t);
        } finally {
          latch.countDown();
        }
      }
    });
    thread.start();
    assertTrue("Timed out waiting for thread execution", latch.await(5, TimeUnit.SECONDS));

    if (thrown.get() != null) {
      throw thrown.get();
    }
  }

  @Test
  public void methodChannelInvokeMethod_onBackgroundThread() throws Throwable {
    final FlutterJNI flutterJNI = new TestFlutterJNI();
    final DartExecutor dartExecutor = new DartExecutor(flutterJNI, mock(AssetManager.class));
    final MethodChannel methodChannel = new MethodChannel(dartExecutor.getBinaryMessenger(), "test_channel");
    final AtomicReference<Throwable> thrown = new AtomicReference<>();
    final CountDownLatch latch = new CountDownLatch(1);

    Thread thread = new Thread(new Runnable() {
      @Override
      public void run() {
        try {
          methodChannel.invokeMethod("test_method", null);
        } catch (Throwable t) {
          thrown.set(t);
        } finally {
          latch.countDown();
        }
      }
    });
    thread.start();
    assertTrue("Timed out waiting for thread execution", latch.await(5, TimeUnit.SECONDS));

    if (thrown.get() != null) {
      throw thrown.get();
    }
  }

  @Test
  public void eventChannelSuccess_onBackgroundThread() throws Throwable {
    final FlutterJNI flutterJNI = new TestFlutterJNI();
    final DartExecutor dartExecutor = new DartExecutor(flutterJNI, mock(AssetManager.class));
    dartExecutor.onAttachedToJNI();

    final EventChannel eventChannel = new EventChannel(dartExecutor.getBinaryMessenger(), "test_event_channel");
    final AtomicReference<EventChannel.EventSink> eventSinkRef = new AtomicReference<>();
    final CountDownLatch setupLatch = new CountDownLatch(1);

    eventChannel.setStreamHandler(new EventChannel.StreamHandler() {
      @Override
      public void onListen(Object arguments, EventChannel.EventSink events) {
        eventSinkRef.set(events);
        setupLatch.countDown();
      }

      @Override
      public void onCancel(Object arguments) {}
    });

    // Simulate Dart listening to the event channel.
    // StandardMethodCodec encodes a MethodCall('listen', arguments).
    ByteBuffer listenMessage = StandardMethodCodec.INSTANCE.encodeMethodCall(new MethodCall("listen", null));
    // Reset the position to 0 so the Java-side decoder can read it.
    listenMessage.position(0);
    
    flutterJNI.handlePlatformMessage("test_event_channel", listenMessage, 1, 0);

    // Idle the main looper so Robolectric processes the handlePlatformMessage call.
    shadowOf(Looper.getMainLooper()).idle();

    assertTrue("Timed out waiting for stream handler setup", setupLatch.await(5, TimeUnit.SECONDS));
    final EventChannel.EventSink eventSink = eventSinkRef.get();
    assertTrue("EventSink should have been initialized", eventSink != null);

    final AtomicReference<Throwable> thrown = new AtomicReference<>();
    final CountDownLatch testLatch = new CountDownLatch(1);

    Thread thread = new Thread(new Runnable() {
      @Override
      public void run() {
        try {
          eventSink.success("test_event");
        } catch (Throwable t) {
          thrown.set(t);
        } finally {
          testLatch.countDown();
        }
      }
    });
    thread.start();
    assertTrue("Timed out waiting for thread execution", testLatch.await(5, TimeUnit.SECONDS));

    if (thrown.get() != null) {
      throw thrown.get();
    }
  }
}
