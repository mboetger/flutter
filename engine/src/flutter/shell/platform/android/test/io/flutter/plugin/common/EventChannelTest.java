// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.plugin.common;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.plugin.common.BinaryMessenger.BinaryMessageHandler;
import io.flutter.plugin.common.BinaryMessenger.BinaryReply;
import io.flutter.plugin.common.EventChannel.EventSink;
import io.flutter.plugin.common.EventChannel.StreamHandler;
import java.nio.ByteBuffer;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;

@RunWith(AndroidJUnit4.class)
public class EventChannelTest {
  @Test
  public void setStreamHandlerNullCancelsActiveStream() {
    BinaryMessenger messenger = mock(BinaryMessenger.class);
    final String channelName = "test_channel";
    EventChannel eventChannel = new EventChannel(messenger, channelName, StandardMethodCodec.INSTANCE);

    final AtomicBoolean onListenCalled = new AtomicBoolean(false);
    final AtomicBoolean onCancelCalled = new AtomicBoolean(false);
    final AtomicInteger activeSinkCount = new AtomicInteger(0);

    StreamHandler streamHandler = new StreamHandler() {
      @Override
      public void onListen(Object arguments, EventSink events) {
        onListenCalled.set(true);
        activeSinkCount.incrementAndGet();
      }

      @Override
      public void onCancel(Object arguments) {
        onCancelCalled.set(true);
        activeSinkCount.decrementAndGet();
      }
    };

    // Capture the registered BinaryMessageHandler when setStreamHandler is called.
    final ArgumentCaptor<BinaryMessageHandler> handlerCaptor =
        ArgumentCaptor.forClass(BinaryMessageHandler.class);

    eventChannel.setStreamHandler(streamHandler);

    verify(messenger).setMessageHandler(eq(channelName), handlerCaptor.capture());
    BinaryMessageHandler binaryMessageHandler = handlerCaptor.getValue();
    assertNotNull(binaryMessageHandler);

    // Simulate Dart listening to the stream.
    MethodCall listenCall = new MethodCall("listen", null);
    ByteBuffer encodedListen = StandardMethodCodec.INSTANCE.encodeMethodCall(listenCall);
    encodedListen.rewind();
    binaryMessageHandler.onMessage(encodedListen, mock(BinaryReply.class));

    assertTrue(onListenCalled.get());
    assertEquals(1, activeSinkCount.get());
    assertFalse(onCancelCalled.get());

    // Call setStreamHandler(null) which should cancel the active stream.
    eventChannel.setStreamHandler(null);

    // Verify that the handler was unregistered from the messenger.
    verify(messenger).setMessageHandler(eq(channelName), eq((BinaryMessageHandler) null));

    // Verify that the stream was cancelled internally.
    assertTrue(onCancelCalled.get());
    assertEquals(0, activeSinkCount.get());
  }

  @Test
  public void setStreamHandlerNewCancelsPreviousActiveStream() {
    BinaryMessenger messenger = mock(BinaryMessenger.class);
    final String channelName = "test_channel";
    EventChannel eventChannel = new EventChannel(messenger, channelName, StandardMethodCodec.INSTANCE);

    final AtomicBoolean onCancel1Called = new AtomicBoolean(false);
    StreamHandler streamHandler1 = new StreamHandler() {
      @Override
      public void onListen(Object arguments, EventSink events) {}

      @Override
      public void onCancel(Object arguments) {
        onCancel1Called.set(true);
      }
    };

    StreamHandler streamHandler2 = new StreamHandler() {
      @Override
      public void onListen(Object arguments, EventSink events) {}

      @Override
      public void onCancel(Object arguments) {}
    };

    // Capture the registered BinaryMessageHandler for streamHandler1.
    final ArgumentCaptor<BinaryMessageHandler> handlerCaptor =
        ArgumentCaptor.forClass(BinaryMessageHandler.class);

    eventChannel.setStreamHandler(streamHandler1);

    verify(messenger).setMessageHandler(eq(channelName), handlerCaptor.capture());
    BinaryMessageHandler binaryMessageHandler = handlerCaptor.getValue();

    // Simulate Dart listening to the stream.
    MethodCall listenCall = new MethodCall("listen", null);
    ByteBuffer encodedListen = StandardMethodCodec.INSTANCE.encodeMethodCall(listenCall);
    encodedListen.rewind();
    binaryMessageHandler.onMessage(encodedListen, mock(BinaryReply.class));

    // Setting a new stream handler should cancel the first one.
    eventChannel.setStreamHandler(streamHandler2);

    assertTrue(onCancel1Called.get());
  }

  @Test
  public void setStreamHandlerNullDoesNotCancelInactiveStream() {
    BinaryMessenger messenger = mock(BinaryMessenger.class);
    final String channelName = "test_channel";
    EventChannel eventChannel = new EventChannel(messenger, channelName, StandardMethodCodec.INSTANCE);

    final AtomicBoolean onCancelCalled = new AtomicBoolean(false);
    StreamHandler streamHandler = new StreamHandler() {
      @Override
      public void onListen(Object arguments, EventSink events) {}

      @Override
      public void onCancel(Object arguments) {
        onCancelCalled.set(true);
      }
    };

    eventChannel.setStreamHandler(streamHandler);

    // Call setStreamHandler(null) without ever listening.
    eventChannel.setStreamHandler(null);

    // Verify that the stream was NOT cancelled because it was never active.
    assertFalse(onCancelCalled.get());
  }

  @Test
  public void setStreamHandlerNullDoesNotCancelAlreadyCancelledStream() {
    BinaryMessenger messenger = mock(BinaryMessenger.class);
    final String channelName = "test_channel";
    EventChannel eventChannel = new EventChannel(messenger, channelName, StandardMethodCodec.INSTANCE);

    final AtomicInteger cancelCallCount = new AtomicInteger(0);
    StreamHandler streamHandler = new StreamHandler() {
      @Override
      public void onListen(Object arguments, EventSink events) {}

      @Override
      public void onCancel(Object arguments) {
        cancelCallCount.incrementAndGet();
      }
    };

    final ArgumentCaptor<BinaryMessageHandler> handlerCaptor =
        ArgumentCaptor.forClass(BinaryMessageHandler.class);

    eventChannel.setStreamHandler(streamHandler);

    verify(messenger).setMessageHandler(eq(channelName), handlerCaptor.capture());
    BinaryMessageHandler binaryMessageHandler = handlerCaptor.getValue();

    // Simulate Dart listening to the stream.
    MethodCall listenCall = new MethodCall("listen", null);
    ByteBuffer encodedListen = StandardMethodCodec.INSTANCE.encodeMethodCall(listenCall);
    encodedListen.rewind();
    binaryMessageHandler.onMessage(encodedListen, mock(BinaryReply.class));

    // Simulate Dart cancelling the stream.
    MethodCall cancelCall = new MethodCall("cancel", null);
    ByteBuffer encodedCancel = StandardMethodCodec.INSTANCE.encodeMethodCall(cancelCall);
    encodedCancel.rewind();
    binaryMessageHandler.onMessage(encodedCancel, mock(BinaryReply.class));

    assertEquals(1, cancelCallCount.get());

    // Call setStreamHandler(null) after it was already cancelled.
    eventChannel.setStreamHandler(null);

    // Verify that onCancel was NOT called a second time.
    assertEquals(1, cancelCallCount.get());
  }

  @Test
  public void setStreamHandlerNewDoesNotCancelInactiveStream() {
    BinaryMessenger messenger = mock(BinaryMessenger.class);
    final String channelName = "test_channel";
    EventChannel eventChannel = new EventChannel(messenger, channelName, StandardMethodCodec.INSTANCE);

    final AtomicBoolean onCancel1Called = new AtomicBoolean(false);
    StreamHandler streamHandler1 = new StreamHandler() {
      @Override
      public void onListen(Object arguments, EventSink events) {}

      @Override
      public void onCancel(Object arguments) {
        onCancel1Called.set(true);
      }
    };

    StreamHandler streamHandler2 = new StreamHandler() {
      @Override
      public void onListen(Object arguments, EventSink events) {}

      @Override
      public void onCancel(Object arguments) {}
    };

    eventChannel.setStreamHandler(streamHandler1);

    // Replace with streamHandler2 without ever listening to streamHandler1.
    eventChannel.setStreamHandler(streamHandler2);

    // Verify that streamHandler1's onCancel was NOT called because it was never active.
    assertFalse(onCancel1Called.get());
  }

  @Test
  public void setStreamHandlerNewDoesNotCancelAlreadyCancelledStream() {
    BinaryMessenger messenger = mock(BinaryMessenger.class);
    final String channelName = "test_channel";
    EventChannel eventChannel = new EventChannel(messenger, channelName, StandardMethodCodec.INSTANCE);

    final AtomicInteger cancelCallCount = new AtomicInteger(0);
    StreamHandler streamHandler1 = new StreamHandler() {
      @Override
      public void onListen(Object arguments, EventSink events) {}

      @Override
      public void onCancel(Object arguments) {
        cancelCallCount.incrementAndGet();
      }
    };

    StreamHandler streamHandler2 = new StreamHandler() {
      @Override
      public void onListen(Object arguments, EventSink events) {}

      @Override
      public void onCancel(Object arguments) {}
    };

    final ArgumentCaptor<BinaryMessageHandler> handlerCaptor =
        ArgumentCaptor.forClass(BinaryMessageHandler.class);

    eventChannel.setStreamHandler(streamHandler1);

    verify(messenger).setMessageHandler(eq(channelName), handlerCaptor.capture());
    BinaryMessageHandler binaryMessageHandler = handlerCaptor.getValue();

    // Simulate Dart listening to the stream.
    MethodCall listenCall = new MethodCall("listen", null);
    ByteBuffer encodedListen = StandardMethodCodec.INSTANCE.encodeMethodCall(listenCall);
    encodedListen.rewind();
    binaryMessageHandler.onMessage(encodedListen, mock(BinaryReply.class));

    // Simulate Dart cancelling the stream.
    MethodCall cancelCall = new MethodCall("cancel", null);
    ByteBuffer encodedCancel = StandardMethodCodec.INSTANCE.encodeMethodCall(cancelCall);
    encodedCancel.rewind();
    binaryMessageHandler.onMessage(encodedCancel, mock(BinaryReply.class));

    assertEquals(1, cancelCallCount.get());

    // Replace with streamHandler2 after streamHandler1 was already cancelled.
    eventChannel.setStreamHandler(streamHandler2);

    // Verify that onCancel was NOT called a second time.
    assertEquals(1, cancelCallCount.get());
  }
}
