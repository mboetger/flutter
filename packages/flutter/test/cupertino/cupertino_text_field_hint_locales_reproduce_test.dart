// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/cupertino.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('CupertinoTextField supports hintLocales', (WidgetTester tester) async {
    const hintLocales = <Locale>[Locale('en'), Locale('fr')];
    await tester.pumpWidget(
      const CupertinoApp(
        home: CupertinoPageScaffold(
          child: Center(child: CupertinoTextField(hintLocales: hintLocales)),
        ),
      ),
    );

    final EditableText editableText = tester.widget(find.byType(EditableText));
    expect(editableText.hintLocales, hintLocales);
  });

  testWidgets('CupertinoTextField.borderless supports hintLocales', (WidgetTester tester) async {
    const hintLocales = <Locale>[Locale('en'), Locale('fr')];
    await tester.pumpWidget(
      const CupertinoApp(
        home: CupertinoPageScaffold(
          child: Center(child: CupertinoTextField.borderless(hintLocales: hintLocales)),
        ),
      ),
    );

    final EditableText editableText = tester.widget(find.byType(EditableText));
    expect(editableText.hintLocales, hintLocales);
  });

  testWidgets('CupertinoTextFormFieldRow supports hintLocales', (WidgetTester tester) async {
    const hintLocales = <Locale>[Locale('en'), Locale('fr')];
    await tester.pumpWidget(
      CupertinoApp(
        home: CupertinoPageScaffold(
          child: Center(
            child: Form(child: CupertinoTextFormFieldRow(hintLocales: hintLocales)),
          ),
        ),
      ),
    );

    final EditableText editableText = tester.widget(find.byType(EditableText));
    expect(editableText.hintLocales, hintLocales);
  });

  testWidgets('CupertinoTextField hintLocales is null by default', (WidgetTester tester) async {
    await tester.pumpWidget(
      const CupertinoApp(
        home: CupertinoPageScaffold(child: Center(child: CupertinoTextField())),
      ),
    );

    final EditableText editableText = tester.widget(find.byType(EditableText));
    expect(editableText.hintLocales, isNull);
  });

  testWidgets('CupertinoTextField diagnostics includes hintLocales', (WidgetTester tester) async {
    final builder = DiagnosticPropertiesBuilder();
    const CupertinoTextField(
      hintLocales: <Locale>[Locale('en'), Locale('fr')],
    ).debugFillProperties(builder);

    final List<String> description = builder.properties
        .where((DiagnosticsNode node) => !node.isFiltered(DiagnosticLevel.info))
        .map((DiagnosticsNode node) => node.toString())
        .toList();

    expect(description, contains('hintLocales: [en, fr]'));
  });
}
