// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter_test/flutter_test.dart';

import 'semantics_tester.dart';

void main() {
  setUp(() {
    debugResetSemanticsIdCounter();
  });

  testWidgets('ListView has list role and items have listItem role even when fitting on screen', (
    WidgetTester tester,
  ) async {
    final semantics = SemanticsTester(tester);
    await tester.pumpWidget(
      Directionality(
        textDirection: TextDirection.ltr,
        child: MediaQuery(
          data: const MediaQueryData(size: Size(800.0, 600.0)),
          child: ListView(
            children: const <Widget>[
              SizedBox(height: 100.0, child: Text('Item 1')),
              SizedBox(height: 100.0, child: Text('Item 2')),
            ],
          ),
        ),
      ),
    );

    expect(
      semantics,
      hasSemantics(
        TestSemantics.root(
          children: <TestSemantics>[
            TestSemantics(
              children: <TestSemantics>[
                TestSemantics(
                  role: SemanticsRole.list,
                  flags: <SemanticsFlag>[SemanticsFlag.hasImplicitScrolling],
                  children: <TestSemantics>[
                    TestSemantics(label: 'Item 1', role: SemanticsRole.listItem),
                    TestSemantics(label: 'Item 2', role: SemanticsRole.listItem),
                  ],
                ),
              ],
            ),
          ],
        ),
        ignoreTransform: true,
        ignoreRect: true,
        ignoreId: true,
      ),
    );

    semantics.dispose();
  });
}
