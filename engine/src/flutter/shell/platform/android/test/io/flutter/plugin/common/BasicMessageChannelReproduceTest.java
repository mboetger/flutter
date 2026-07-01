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
public class BasicMessageChannelReproduceTest {

  @Test
  public void testReplyCallbackExceptionNotSwallowed() {
    BinaryMessenger mockMessenger = mock(BinaryMessenger.class);
    MessageCodec<String> codec = StringCodec.INSTANCE;
    BasicMessageChannel<String> channel = new BasicMessageChannel<>(mockMessenger, "test_channel", codec);

    // Send message with a callback that throws an exception.
    BasicMessageChannel.Reply<String> mockCallback = new BasicMessageChannel.Reply<String>() {
      @Override
      public void reply(String reply) {
        throw new RuntimeException("Intentional test exception in reply");
      }
    };

    channel.send("testMessage", mockCallback);

    // Capture the BinaryReply sent to the messenger.
    ArgumentCaptor<BinaryMessenger.BinaryReply> replyCaptor = ArgumentCaptor.forClass(BinaryMessenger.BinaryReply.class);
    verify(mockMessenger).send(eq("test_channel"), any(ByteBuffer.class), replyCaptor.capture());

    BinaryMessenger.BinaryReply reply = replyCaptor.getValue();

    ByteBuffer replyBuffer = codec.encodeMessage("dummy");
    replyBuffer.rewind();

    // When we call reply.reply, the callback.reply is invoked.
    // If it throws RuntimeException, it should NOT be swallowed.
    RuntimeException thrown = assertThrows(
        RuntimeException.class,
        () -> reply.reply(replyBuffer)
    );
    assertEquals("Intentional test exception in reply", thrown.getMessage());
  }

  @Test
  public void testMessageHandlerExceptionNotSwallowed() {
    BinaryMessenger mockMessenger = mock(BinaryMessenger.class);
    MessageCodec<String> codec = StringCodec.INSTANCE;
    BasicMessageChannel<String> channel = new BasicMessageChannel<>(mockMessenger, "test_channel", codec);

    // Set a handler that throws an exception.
    channel.setMessageHandler(new BasicMessageChannel.MessageHandler<String>() {
      @Override
      public void onMessage(String message, BasicMessageChannel.Reply<String> reply) {
        throw new RuntimeException("Intentional test exception in onMessage");
      }
    });

    // Capture the BinaryMessageHandler registered with the messenger.
    ArgumentCaptor<BinaryMessenger.BinaryMessageHandler> handlerCaptor = ArgumentCaptor.forClass(BinaryMessenger.BinaryMessageHandler.class);
    verify(mockMessenger).setMessageHandler(eq("test_channel"), handlerCaptor.capture());

    BinaryMessenger.BinaryMessageHandler handler = handlerCaptor.getValue();
    BinaryMessenger.BinaryReply mockReply = mock(BinaryMessenger.BinaryReply.class);

    ByteBuffer messageBuffer = codec.encodeMessage("testMessage");
    messageBuffer.rewind();

    // When we call handler.onMessage, the handler.onMessage is invoked.
    // If it throws RuntimeException, it should NOT be swallowed.
    RuntimeException thrown = assertThrows(
        RuntimeException.class,
        () -> handler.onMessage(messageBuffer, mockReply)
    );
    assertEquals("Intentional test exception in onMessage", thrown.getMessage());
  }
}
