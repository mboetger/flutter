// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/semantics.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('Semantics are flushed synchronously when ensureSemantics is called', (
    WidgetTester tester,
  ) async {
    await tester.pumpWidget(
      Directionality(
        textDirection: TextDirection.ltr,
        child: Semantics(
          label: 'Hello A11y',
          explicitChildNodes: true,
          child: const Text('Hello'),
        ),
      ),
    );

    // Semantics should not be enabled yet.
    expect(tester.binding.semanticsEnabled, isFalse);

    // Enable semantics.
    final SemanticsHandle handle = tester.binding.ensureSemantics();

    // Verify semantics are enabled.
    expect(tester.binding.semanticsEnabled, isTrue);

    try {
      // Querying the semantics tree right after ensureSemantics (without pumping another frame)
      // should immediately succeed and find the node with 'Hello A11y'.
      // Under the bug, this expect will fail because the semantics tree has not been flushed yet.
      expect(find.bySemanticsLabel('Hello A11y'), findsOneWidget);
    } finally {
      handle.dispose();
    }
  }, semanticsEnabled: false);
}
