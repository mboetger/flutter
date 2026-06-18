// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('FocusNode loses focus on mobile when app becomes inactive', (
    WidgetTester tester,
  ) async {
    final focusNode = FocusNode(debugLabel: 'Focus Node');
    addTearDown(focusNode.dispose);

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: Focus(focusNode: focusNode, child: const SizedBox(width: 10, height: 10)),
        ),
      ),
    );

    focusNode.requestFocus();
    await tester.pump();
    expect(focusNode.hasPrimaryFocus, isTrue);

    // Simulate app becoming inactive (e.g. split screen focusing other app)
    tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.inactive);
    await tester.pump();

    // Focus should be reset (lost focus)
    expect(focusNode.hasPrimaryFocus, isFalse);

    // Resume app
    tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.resumed);
    await tester.pump();

    // Focus is restored
    expect(focusNode.hasPrimaryFocus, isTrue);
  }, variant: TargetPlatformVariant.mobile());

  testWidgets('EditableText preserves connection during inactive/paused lifecycle states', (
    WidgetTester tester,
  ) async {
    final controller = TextEditingController(text: 'Hello');
    addTearDown(controller.dispose);
    final focusNode = FocusNode();
    addTearDown(focusNode.dispose);

    final log = <MethodCall>[];
    tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(SystemChannels.textInput, (
      MethodCall methodCall,
    ) async {
      log.add(methodCall);
      return null;
    });

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: EditableText(
            controller: controller,
            focusNode: focusNode,
            style: const TextStyle(),
            cursorColor: const Color(0xff000000),
            backgroundCursorColor: const Color(0xff000000),
          ),
        ),
      ),
    );

    // Focus and show keyboard
    await tester.tap(find.byType(EditableText));
    await tester.pump();
    expect(focusNode.hasPrimaryFocus, isTrue);
    expect(log.any((MethodCall m) => m.method == 'TextInput.show'), isTrue);
    log.clear();

    // Simulate app going inactive
    tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.inactive);
    await tester.pump();

    // Focus is lost
    expect(focusNode.hasPrimaryFocus, isFalse);

    // Connection remains open (no clearClient call)
    expect(log.any((MethodCall m) => m.method == 'TextInput.clearClient'), isFalse);

    // Resume app
    tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.resumed);
    await tester.pump();

    // Focus is restored
    expect(focusNode.hasPrimaryFocus, isTrue);

    // Unfocus normally by requesting focus to a different node
    final otherNode = FocusNode();
    addTearDown(otherNode.dispose);
    FocusManager.instance.rootScope.requestFocus(otherNode);
    await tester.pump();

    // Connection is closed when unfocused normally
    expect(focusNode.hasPrimaryFocus, isFalse);
    expect(log.any((MethodCall m) => m.method == 'TextInput.clearClient'), isTrue);
  }, variant: TargetPlatformVariant.mobile());
}
