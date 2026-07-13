// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/material.dart';

void main() => runApp(const PointerTrackingDelayApp());

class PointerTrackingDelayApp extends StatelessWidget {
  const PointerTrackingDelayApp({super.key});

  @override
  Widget build(BuildContext context) {
    return const MaterialApp(
      home: Scaffold(
        body: PointerTrackerWidget(),
      ),
    );
  }
}

class PointerTrackerWidget extends StatefulWidget {
  const PointerTrackerWidget({super.key});

  @override
  State<PointerTrackerWidget> createState() => _PointerTrackerWidgetState();
}

class _PointerTrackerWidgetState extends State<PointerTrackerWidget> {
  Offset? _pointerOffset;

  @override
  Widget build(BuildContext context) {
    return Listener(
      onPointerDown: (PointerDownEvent event) {
        setState(() {
          _pointerOffset = event.localPosition;
        });
      },
      onPointerMove: (PointerMoveEvent event) {
        setState(() {
          _pointerOffset = event.localPosition;
        });
      },
      onPointerUp: (PointerUpEvent event) {
        setState(() {
          _pointerOffset = null;
        });
      },
      child: CustomPaint(
        painter: CirclePainter(pointerOffset: _pointerOffset),
        child: const SizedBox.expand(),
      ),
    );
  }
}

class CirclePainter extends CustomPainter {
  CirclePainter({required this.pointerOffset});

  final Offset? pointerOffset;

  @override
  void paint(Canvas canvas, Size size) {
    if (pointerOffset != null) {
      final Paint paint = Paint()
        ..color = Colors.red
        ..style = PaintingStyle.fill;
      canvas.drawCircle(pointerOffset!, 30.0, paint);
    }
  }

  @override
  bool shouldRepaint(CirclePainter oldDelegate) {
    return oldDelegate.pointerOffset != pointerOffset;
  }
}
