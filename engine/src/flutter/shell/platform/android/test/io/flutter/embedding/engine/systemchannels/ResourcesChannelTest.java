// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine.systemchannels;

import static org.junit.Assert.assertArrayEquals;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.plugin.common.BinaryMessenger;
import java.io.ByteArrayInputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;

@RunWith(AndroidJUnit4.class)
public class ResourcesChannelTest {

  @Test
  public void testResourcesChannelRegistration() {
    BinaryMessenger mockMessenger = mock(BinaryMessenger.class);
    Context mockContext = mock(Context.class);

    new ResourcesChannel(mockMessenger, mockContext);

    verify(mockMessenger, times(1))
        .setMessageHandler(eq("flutter/resources"), any(BinaryMessenger.BinaryMessageHandler.class));
  }

  @Test
  public void testLoadResourceSuccess() throws Exception {
    BinaryMessenger mockMessenger = mock(BinaryMessenger.class);
    Context mockContext = mock(Context.class);
    Resources mockResources = mock(Resources.class);

    when(mockContext.getResources()).thenReturn(mockResources);
    when(mockContext.getPackageName()).thenReturn("com.example");
    when(mockResources.getIdentifier("my_icon", "drawable", "com.example")).thenReturn(123);
    
    byte[] expectedData = "dummy_image_data".getBytes(StandardCharsets.UTF_8);
    when(mockResources.openRawResource(123)).thenReturn(new ByteArrayInputStream(expectedData));

    ArgumentCaptor<BinaryMessenger.BinaryMessageHandler> handlerCaptor =
        ArgumentCaptor.forClass(BinaryMessenger.BinaryMessageHandler.class);

    new ResourcesChannel(mockMessenger, mockContext);

    verify(mockMessenger, times(1)).setMessageHandler(eq("flutter/resources"), handlerCaptor.capture());
    BinaryMessenger.BinaryMessageHandler handler = handlerCaptor.getValue();

    ByteBuffer message = ByteBuffer.wrap("res/drawable/my_icon.png".getBytes(StandardCharsets.UTF_8));
    BinaryMessenger.BinaryReply mockReply = mock(BinaryMessenger.BinaryReply.class);
    
    handler.onMessage(message, mockReply);

    ArgumentCaptor<ByteBuffer> replyCaptor = ArgumentCaptor.forClass(ByteBuffer.class);
    verify(mockReply, times(1)).reply(replyCaptor.capture());

    ByteBuffer replyBuffer = replyCaptor.getValue();
    byte[] actualData = new byte[replyBuffer.remaining()];
    replyBuffer.get(actualData);
    assertArrayEquals(expectedData, actualData);
  }

  @Test
  public void testLoadResource9PatchSuccess() throws Exception {
    BinaryMessenger mockMessenger = mock(BinaryMessenger.class);
    Context mockContext = mock(Context.class);
    Resources mockResources = mock(Resources.class);

    when(mockContext.getResources()).thenReturn(mockResources);
    when(mockContext.getPackageName()).thenReturn("com.example");
    when(mockResources.getIdentifier("bg_tooltip", "drawable", "com.example")).thenReturn(124);
    
    byte[] expectedData = "dummy_9patch_data".getBytes(StandardCharsets.UTF_8);
    when(mockResources.openRawResource(124)).thenReturn(new ByteArrayInputStream(expectedData));

    ArgumentCaptor<BinaryMessenger.BinaryMessageHandler> handlerCaptor =
        ArgumentCaptor.forClass(BinaryMessenger.BinaryMessageHandler.class);

    new ResourcesChannel(mockMessenger, mockContext);

    verify(mockMessenger, times(1)).setMessageHandler(eq("flutter/resources"), handlerCaptor.capture());
    BinaryMessenger.BinaryMessageHandler handler = handlerCaptor.getValue();

    ByteBuffer message = ByteBuffer.wrap("res/drawable/bg_tooltip.9.png".getBytes(StandardCharsets.UTF_8));
    BinaryMessenger.BinaryReply mockReply = mock(BinaryMessenger.BinaryReply.class);
    
    handler.onMessage(message, mockReply);

    ArgumentCaptor<ByteBuffer> replyCaptor = ArgumentCaptor.forClass(ByteBuffer.class);
    verify(mockReply, times(1)).reply(replyCaptor.capture());

    ByteBuffer replyBuffer = replyCaptor.getValue();
    byte[] actualData = new byte[replyBuffer.remaining()];
    replyBuffer.get(actualData);
    assertArrayEquals(expectedData, actualData);
  }

  @Test
  public void testLoadResourceNotFound() {
    BinaryMessenger mockMessenger = mock(BinaryMessenger.class);
    Context mockContext = mock(Context.class);
    Resources mockResources = mock(Resources.class);

    when(mockContext.getResources()).thenReturn(mockResources);
    when(mockContext.getPackageName()).thenReturn("com.example");
    when(mockResources.getIdentifier("non_existent", "drawable", "com.example")).thenReturn(0);

    ArgumentCaptor<BinaryMessenger.BinaryMessageHandler> handlerCaptor =
        ArgumentCaptor.forClass(BinaryMessenger.BinaryMessageHandler.class);

    new ResourcesChannel(mockMessenger, mockContext);

    verify(mockMessenger, times(1)).setMessageHandler(eq("flutter/resources"), handlerCaptor.capture());
    BinaryMessenger.BinaryMessageHandler handler = handlerCaptor.getValue();

    ByteBuffer message = ByteBuffer.wrap("res/drawable/non_existent.png".getBytes(StandardCharsets.UTF_8));
    BinaryMessenger.BinaryReply mockReply = mock(BinaryMessenger.BinaryReply.class);
    
    handler.onMessage(message, mockReply);

    verify(mockReply, times(1)).reply(null);
  }

  @Test
  public void testLoadResourceInvalidKey() {
    BinaryMessenger mockMessenger = mock(BinaryMessenger.class);
    Context mockContext = mock(Context.class);

    ArgumentCaptor<BinaryMessenger.BinaryMessageHandler> handlerCaptor =
        ArgumentCaptor.forClass(BinaryMessenger.BinaryMessageHandler.class);

    new ResourcesChannel(mockMessenger, mockContext);

    verify(mockMessenger, times(1)).setMessageHandler(eq("flutter/resources"), handlerCaptor.capture());
    BinaryMessenger.BinaryMessageHandler handler = handlerCaptor.getValue();

    ByteBuffer message = ByteBuffer.wrap("invalid_key".getBytes(StandardCharsets.UTF_8));
    BinaryMessenger.BinaryReply mockReply = mock(BinaryMessenger.BinaryReply.class);
    
    handler.onMessage(message, mockReply);

    verify(mockReply, times(1)).reply(null);
  }

  @Test
  public void testLoadResourceNestedPathRejected() {
    BinaryMessenger mockMessenger = mock(BinaryMessenger.class);
    Context mockContext = mock(Context.class);

    ArgumentCaptor<BinaryMessenger.BinaryMessageHandler> handlerCaptor =
        ArgumentCaptor.forClass(BinaryMessenger.BinaryMessageHandler.class);

    new ResourcesChannel(mockMessenger, mockContext);

    verify(mockMessenger, times(1)).setMessageHandler(eq("flutter/resources"), handlerCaptor.capture());
    BinaryMessenger.BinaryMessageHandler handler = handlerCaptor.getValue();

    ByteBuffer message = ByteBuffer.wrap("res/drawable/nested/icon.png".getBytes(StandardCharsets.UTF_8));
    BinaryMessenger.BinaryReply mockReply = mock(BinaryMessenger.BinaryReply.class);
    
    handler.onMessage(message, mockReply);

    verify(mockReply, times(1)).reply(null);
  }
}
