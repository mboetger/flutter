// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('SliverFillViewport with shrinkWrap and padEnds throws assertion error in debug mode', (
    WidgetTester tester,
  ) async {
    final errors = <FlutterErrorDetails>[];
    final void Function(FlutterErrorDetails)? oldHandler = FlutterError.onError;
    FlutterError.onError = (FlutterErrorDetails error) => errors.add(error);

    try {
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: SingleChildScrollView(
              child: Column(
                children: <Widget>[
                  CustomScrollView(
                    shrinkWrap: true,
                    slivers: <Widget>[
                      SliverFillViewport(
                        delegate: SliverChildListDelegate(const <Widget>[
                          Text('Page 1'),
                          Text('Page 2'),
                        ]),
                      ),
                    ],
                  ),
                ],
              ),
            ),
          ),
        ),
      );
    } finally {
      FlutterError.onError = oldHandler;
    }

    expect(errors, isNotEmpty);
    expect(errors.first.exception, isAssertionError);
    expect(
      errors.first.exception.toString(),
      contains(
        'SliverFillViewport (commonly used by PageView) was given an infinite viewportMainAxisExtent',
      ),
    );
  });

  testWidgets('CarouselView throws assertion error in debug mode under unbounded constraints', (
    WidgetTester tester,
  ) async {
    final errors = <FlutterErrorDetails>[];
    final void Function(FlutterErrorDetails)? oldHandler = FlutterError.onError;
    FlutterError.onError = (FlutterErrorDetails error) => errors.add(error);

    try {
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: SizedBox(
              height: 300,
              child: Row(
                children: <Widget>[
                  CarouselView(
                    itemExtent: 200,
                    children: List<Widget>.generate(10, (int index) {
                      return Center(child: Text('Item $index'));
                    }),
                  ),
                ],
              ),
            ),
          ),
        ),
      );
    } finally {
      FlutterError.onError = oldHandler;
    }

    expect(errors, isNotEmpty);
    expect(errors.first.exception, isAssertionError);
    expect(
      errors.first.exception.toString(),
      contains('Horizontal viewport was given unbounded width.'),
    );
  });

  testWidgets(
    'CarouselView.weighted throws assertion error in debug mode under unbounded constraints',
    (WidgetTester tester) async {
      final errors = <FlutterErrorDetails>[];
      final void Function(FlutterErrorDetails)? oldHandler = FlutterError.onError;
      FlutterError.onError = (FlutterErrorDetails error) => errors.add(error);

      try {
        await tester.pumpWidget(
          MaterialApp(
            home: Scaffold(
              body: SizedBox(
                height: 300,
                child: Row(
                  children: <Widget>[
                    CarouselView.weighted(
                      flexWeights: const <int>[1, 7, 1],
                      children: List<Widget>.generate(10, (int index) {
                        return Center(child: Text('Item $index'));
                      }),
                    ),
                  ],
                ),
              ),
            ),
          ),
        );
      } finally {
        FlutterError.onError = oldHandler;
      }

      expect(errors, isNotEmpty);
      expect(errors.first.exception, isAssertionError);
      expect(
        errors.first.exception.toString(),
        contains('Horizontal viewport was given unbounded width.'),
      );
    },
  );
}
