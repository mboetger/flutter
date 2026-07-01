package io.flutter.plugin.common;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.*;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import java.nio.ByteBuffer;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;

@RunWith(AndroidJUnit4.class)
public class MethodChannelReproduceTest {

  @Test
  public void testResultCallbackExceptionNotSwallowed_Success() {
    BinaryMessenger mockMessenger = mock(BinaryMessenger.class);
    MethodCodec codec = StandardMethodCodec.INSTANCE;
    MethodChannel channel = new MethodChannel(mockMessenger, "test_channel", codec);

    // Invoke method with a callback that throws an exception in success.
    MethodChannel.Result mockCallback = new MethodChannel.Result() {
      @Override
      public void success(Object result) {
        throw new RuntimeException("Intentional test exception in success");
      }

      @Override
      public void error(String errorCode, String errorMessage, Object errorDetails) {}

      @Override
      public void notImplemented() {}
    };

    channel.invokeMethod("testMethod", null, mockCallback);

    // Capture the BinaryReply sent to the messenger.
    ArgumentCaptor<BinaryMessenger.BinaryReply> replyCaptor = ArgumentCaptor.forClass(BinaryMessenger.BinaryReply.class);
    verify(mockMessenger).send(eq("test_channel"), any(ByteBuffer.class), replyCaptor.capture());

    BinaryMessenger.BinaryReply reply = replyCaptor.getValue();

    ByteBuffer replyBuffer = codec.encodeSuccessEnvelope("dummy");
    replyBuffer.rewind();

    // When we call reply.reply, the callback.success is invoked.
    // If it throws RuntimeException, it should NOT be swallowed.
    RuntimeException thrown = assertThrows(
        RuntimeException.class,
        () -> reply.reply(replyBuffer)
    );
    assertEquals("Intentional test exception in success", thrown.getMessage());
  }

  @Test
  public void testResultCallbackExceptionNotSwallowed_Error() {
    BinaryMessenger mockMessenger = mock(BinaryMessenger.class);
    MethodCodec codec = StandardMethodCodec.INSTANCE;
    MethodChannel channel = new MethodChannel(mockMessenger, "test_channel", codec);

    // Invoke method with a callback that throws an exception in error.
    MethodChannel.Result mockCallback = new MethodChannel.Result() {
      @Override
      public void success(Object result) {}

      @Override
      public void error(String errorCode, String errorMessage, Object errorDetails) {
        throw new RuntimeException("Intentional test exception in error");
      }

      @Override
      public void notImplemented() {}
    };

    channel.invokeMethod("testMethod", null, mockCallback);

    // Capture the BinaryReply sent to the messenger.
    ArgumentCaptor<BinaryMessenger.BinaryReply> replyCaptor = ArgumentCaptor.forClass(BinaryMessenger.BinaryReply.class);
    verify(mockMessenger).send(eq("test_channel"), any(ByteBuffer.class), replyCaptor.capture());

    BinaryMessenger.BinaryReply reply = replyCaptor.getValue();

    ByteBuffer replyBuffer = codec.encodeErrorEnvelope("code", "message", null);
    replyBuffer.rewind();

    // When we call reply.reply, the callback.error is invoked.
    // If it throws RuntimeException, it should NOT be swallowed.
    RuntimeException thrown = assertThrows(
        RuntimeException.class,
        () -> reply.reply(replyBuffer)
    );
    assertEquals("Intentional test exception in error", thrown.getMessage());
  }

  @Test
  public void testResultCallbackExceptionNotSwallowed_NotImplemented() {
    BinaryMessenger mockMessenger = mock(BinaryMessenger.class);
    MethodCodec codec = StandardMethodCodec.INSTANCE;
    MethodChannel channel = new MethodChannel(mockMessenger, "test_channel", codec);

    // Invoke method with a callback that throws an exception in notImplemented.
    MethodChannel.Result mockCallback = new MethodChannel.Result() {
      @Override
      public void success(Object result) {}

      @Override
      public void error(String errorCode, String errorMessage, Object errorDetails) {}

      @Override
      public void notImplemented() {
        throw new RuntimeException("Intentional test exception in notImplemented");
      }
    };

    channel.invokeMethod("testMethod", null, mockCallback);

    // Capture the BinaryReply sent to the messenger.
    ArgumentCaptor<BinaryMessenger.BinaryReply> replyCaptor = ArgumentCaptor.forClass(BinaryMessenger.BinaryReply.class);
    verify(mockMessenger).send(eq("test_channel"), any(ByteBuffer.class), replyCaptor.capture());

    BinaryMessenger.BinaryReply reply = replyCaptor.getValue();

    // When we call reply.reply with null, the callback.notImplemented is invoked.
    // If it throws RuntimeException, it should NOT be swallowed.
    RuntimeException thrown = assertThrows(
        RuntimeException.class,
        () -> reply.reply(null)
    );
    assertEquals("Intentional test exception in notImplemented", thrown.getMessage());
  }

  @Test
  public void testMethodCallHandlerExceptionNotSwallowed() {
    BinaryMessenger mockMessenger = mock(BinaryMessenger.class);
    MethodCodec codec = StandardMethodCodec.INSTANCE;
    MethodChannel channel = new MethodChannel(mockMessenger, "test_channel", codec);

    // Set a handler that throws an exception.
    channel.setMethodCallHandler(new MethodChannel.MethodCallHandler() {
      @Override
      public void onMethodCall(MethodCall call, MethodChannel.Result result) {
        throw new RuntimeException("Intentional test exception in onMethodCall");
      }
    });

    // Capture the BinaryMessageHandler registered with the messenger.
    ArgumentCaptor<BinaryMessenger.BinaryMessageHandler> handlerCaptor = ArgumentCaptor.forClass(BinaryMessenger.BinaryMessageHandler.class);
    verify(mockMessenger).setMessageHandler(eq("test_channel"), handlerCaptor.capture());

    BinaryMessenger.BinaryMessageHandler handler = handlerCaptor.getValue();
    BinaryMessenger.BinaryReply mockReply = mock(BinaryMessenger.BinaryReply.class);

    ByteBuffer messageBuffer = codec.encodeMethodCall(new MethodCall("testMethod", null));
    messageBuffer.rewind();

    // When we call handler.onMessage, the handler.onMethodCall is invoked.
    // If it throws RuntimeException, it should NOT be swallowed.
    RuntimeException thrown = assertThrows(
        RuntimeException.class,
        () -> handler.onMessage(messageBuffer, mockReply)
    );
    assertEquals("Intentional test exception in onMethodCall", thrown.getMessage());
  }
}
