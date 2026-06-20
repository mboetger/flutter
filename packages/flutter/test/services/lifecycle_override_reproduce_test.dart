// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:ui';

import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  tearDown(() {
    // ignore: invalid_use_of_visible_for_testing_member
    TestWidgetsFlutterBinding.instance.resetInternalState();
  });

  test(
    'overriding SystemChannels.lifecycle.setMessageHandler does not prevent framework from receiving lifecycle events',
    () async {
      final TestWidgetsFlutterBinding binding = TestWidgetsFlutterBinding.instance;

      // Verify that the initial lifecycle state is not paused.
      expect(binding.lifecycleState, isNot(AppLifecycleState.paused));

      // Simulate the developer overriding the lifecycle channel's message handler.
      var developerHandlerCalled = false;
      SystemChannels.lifecycle.setMessageHandler((String? message) async {
        developerHandlerCalled = true;
        return null;
      });

      // Simulate a lifecycle message ('AppLifecycleState.paused') sent from the platform.
      final ByteData? messageBytes = SystemChannels.lifecycle.codec.encodeMessage(
        'AppLifecycleState.paused',
      );
      await binding.defaultBinaryMessenger.handlePlatformMessage(
        SystemChannels.lifecycle.name,
        messageBytes,
        (ByteData? reply) {},
      );

      // Under the buggy implementation, the framework's internal handler is completely
      // overridden and never receives this message. Thus, the binding's lifecycle state
      // remains unchanged (fails to transition to paused).
      // In the fixed implementation, both the framework's state must update, and the
      // developer's custom handler must still be invoked.
      expect(binding.lifecycleState, AppLifecycleState.paused);
      expect(developerHandlerCalled, isTrue);
    },
  );

  test(
    'setting SystemChannels.lifecycle.setMessageHandler to null does not prevent framework from receiving lifecycle events',
    () async {
      final TestWidgetsFlutterBinding binding = TestWidgetsFlutterBinding.instance;

      // Verify that the initial lifecycle state is not paused.
      expect(binding.lifecycleState, isNot(AppLifecycleState.paused));

      // Simulate the developer clearing the lifecycle channel's message handler.
      SystemChannels.lifecycle.setMessageHandler(null);

      // Simulate a lifecycle message ('AppLifecycleState.paused') sent from the platform.
      final ByteData? messageBytes = SystemChannels.lifecycle.codec.encodeMessage(
        'AppLifecycleState.paused',
      );
      await binding.defaultBinaryMessenger.handlePlatformMessage(
        SystemChannels.lifecycle.name,
        messageBytes,
        (ByteData? reply) {},
      );

      // The framework's internal handler should still receive the message even if the
      // developer-facing handler is cleared/set to null.
      expect(binding.lifecycleState, AppLifecycleState.paused);
    },
  );

  test("LifecycleMessageChannel prioritizes the developer's return value", () async {
    final TestWidgetsFlutterBinding binding = TestWidgetsFlutterBinding.instance;

    SystemChannels.lifecycle.setFrameworkHandler((String? message) async {
      return 'framework_reply';
    });

    SystemChannels.lifecycle.setMessageHandler((String? message) async {
      return 'developer_reply';
    });

    final ByteData? messageBytes = SystemChannels.lifecycle.codec.encodeMessage(
      'AppLifecycleState.paused',
    );
    ByteData? capturedReply;
    await binding.defaultBinaryMessenger.handlePlatformMessage(
      SystemChannels.lifecycle.name,
      messageBytes,
      (ByteData? reply) {
        capturedReply = reply;
      },
    );

    final String? decodedReply = SystemChannels.lifecycle.codec.decodeMessage(capturedReply);
    expect(decodedReply, 'developer_reply');
  });

  test(
    'LifecycleMessageChannel propagates framework handler exceptions but still executes developer handler',
    () async {
      final TestWidgetsFlutterBinding binding = TestWidgetsFlutterBinding.instance;

      SystemChannels.lifecycle.setFrameworkHandler((String? message) async {
        throw Exception('framework_error');
      });

      var developerHandlerCalled = false;
      SystemChannels.lifecycle.setMessageHandler((String? message) async {
        developerHandlerCalled = true;
        return null;
      });

      final ByteData? messageBytes = SystemChannels.lifecycle.codec.encodeMessage(
        'AppLifecycleState.paused',
      );

      await expectLater(
        binding.defaultBinaryMessenger.handlePlatformMessage(
          SystemChannels.lifecycle.name,
          messageBytes,
          (ByteData? reply) {},
        ),
        throwsA(
          isA<Exception>().having((e) => e.toString(), 'description', contains('framework_error')),
        ),
      );

      expect(developerHandlerCalled, isTrue);
    },
  );
}
