// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/material.dart';
import 'package:flutter/semantics.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('Semantics identifier is propagated to SemanticsNode', (WidgetTester tester) async {
    final SemanticsHandle handle = tester.ensureSemantics();

    await tester.pumpWidget(
      Directionality(
        textDirection: TextDirection.ltr,
        child: Semantics(identifier: 'test_identifier', child: const Text('Hello')),
      ),
    );

    final SemanticsNode node = tester.getSemantics(find.text('Hello'));
    expect(node, matchesSemantics(identifier: 'test_identifier', label: 'Hello'));

    handle.dispose();
  });

  testWidgets('Semantics.fromProperties propagates identifier to SemanticsNode', (
    WidgetTester tester,
  ) async {
    final SemanticsHandle handle = tester.ensureSemantics();

    await tester.pumpWidget(
      const Directionality(
        textDirection: TextDirection.ltr,
        child: Semantics.fromProperties(
          properties: SemanticsProperties(identifier: 'properties_identifier'),
          child: Text('Hello'),
        ),
      ),
    );

    final SemanticsNode node = tester.getSemantics(find.text('Hello'));
    expect(node, matchesSemantics(identifier: 'properties_identifier', label: 'Hello'));

    handle.dispose();
  });

  testWidgets('Semantics identifier defaults to empty string', (WidgetTester tester) async {
    final SemanticsHandle handle = tester.ensureSemantics();

    await tester.pumpWidget(
      const Directionality(textDirection: TextDirection.ltr, child: Text('Hello')),
    );

    final SemanticsNode node = tester.getSemantics(find.text('Hello'));
    expect(node, matchesSemantics(identifier: ''));

    handle.dispose();
  });

  testWidgets('MergeSemantics preserves parent identifier and ignores child identifier', (
    WidgetTester tester,
  ) async {
    final SemanticsHandle handle = tester.ensureSemantics();

    await tester.pumpWidget(
      Directionality(
        textDirection: TextDirection.ltr,
        child: MergeSemantics(
          child: Semantics(
            identifier: 'parent_identifier',
            child: Semantics(identifier: 'child_identifier', child: const Text('Hello')),
          ),
        ),
      ),
    );

    final SemanticsNode node = tester.getSemantics(find.text('Hello'));
    expect(node, matchesSemantics(identifier: 'parent_identifier', label: 'Hello'));

    handle.dispose();
  });
}
