// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

class _Clipper extends CustomClipper<Path> {
  const _Clipper();

  @override
  Path getClip(Size size) {
    final double width = size.width;
    return Path()
      ..addPolygon(<Offset>[
        Offset(width * 0, 5),
        Offset(width * 0.5, 20),
        Offset(width * 1, 10),
      ], false)
      ..lineTo(size.width, size.height)
      ..lineTo(0, size.height)
      ..close();
  }

  @override
  bool shouldReclip(_Clipper oldClipper) => false;
}

class _Painter extends CustomPainter {
  const _Painter();

  @override
  void paint(Canvas canvas, Size size) {
    canvas.drawRect(
      Offset.zero & size,
      Paint()
        ..color = Colors.white
        ..blendMode = BlendMode.difference,
    );
  }

  @override
  bool shouldRepaint(_Painter oldDelegate) => false;
}

void main() {
  testWidgets('ClipPath with CustomPaint and BlendMode.difference in Scaffold with AppBar (Issue #72249)', (
    WidgetTester tester,
  ) async {
    final Key scaffoldKey = UniqueKey();

    // 1. Pump the Scaffold WITH an AppBar.
    // In ScaffoldLayout, body is painted before appBar.
    // When appBar is an AppBar widget, it pushes an AnnotatedRegionLayer (and PhysicalModelLayer),
    // which calls PaintingContext.stopRecordingIfNeeded().
    // This terminates the PictureLayer recording the body, isolating the ClipPath + BlendMode.difference
    // into a standalone PictureLayer without the Scaffold background, causing graphics artifacts on GPU renderers.
    await tester.pumpWidget(
      MaterialApp(
        home: RepaintBoundary(
          key: scaffoldKey,
          child: Scaffold(
            appBar: AppBar(title: const Text('Title')),
            body: const SizedBox.expand(
              child: ClipPath(
                clipper: _Clipper(),
                child: CustomPaint(painter: _Painter()),
              ),
            ),
          ),
        ),
      ),
    );
    await tester.pumpAndSettle();

    final RenderBox scaffoldBoxWithAppBar = tester.renderObject(find.byKey(scaffoldKey));
    final scaffoldLayerWithAppBar = scaffoldBoxWithAppBar.debugLayer! as OffsetLayer;

    // Verify that when AppBar is present, the body is recorded into its own PictureLayer (firstChild),
    // and the AppBar pushes an AnnotatedRegionLayer (nextSibling) which caused stopRecordingIfNeeded().
    expect(scaffoldLayerWithAppBar.firstChild, isA<PictureLayer>());
    expect(
      scaffoldLayerWithAppBar.firstChild!.nextSibling,
      isA<AnnotatedRegionLayer<SystemUiOverlayStyle>>(),
    );

    // 2. Pump the Scaffold WITHOUT an AppBar (using Container instead).
    // Container does not create or push any composited layers.
    // Therefore, stopRecordingIfNeeded() is not called between body and appBar, allowing Scaffold background,
    // body (ClipPath + BlendMode.difference), and Container to be recorded into a single unified PictureLayer.
    await tester.pumpWidget(
      MaterialApp(
        home: RepaintBoundary(
          key: scaffoldKey,
          child: Scaffold(
            appBar: PreferredSize(
              preferredSize: const Size.fromHeight(kToolbarHeight),
              child: Container(color: Colors.blue),
            ),
            body: const SizedBox.expand(
              child: ClipPath(
                clipper: _Clipper(),
                child: CustomPaint(painter: _Painter()),
              ),
            ),
          ),
        ),
      ),
    );
    await tester.pumpAndSettle();

    final RenderBox scaffoldBoxWithoutAppBar = tester.renderObject(find.byKey(scaffoldKey));
    final scaffoldLayerWithoutAppBar = scaffoldBoxWithoutAppBar.debugLayer! as OffsetLayer;

    // Verify that without AppBar (with Container), no layer split occurs: all drawing is in a single PictureLayer.
    expect(scaffoldLayerWithoutAppBar.firstChild, isA<PictureLayer>());
    expect(scaffoldLayerWithoutAppBar.firstChild!.nextSibling, isNull);
  });
}
