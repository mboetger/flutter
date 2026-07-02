// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:android_driver_extensions/extension.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_driver/driver_extension.dart';

import '../src/allow_list_devices.dart';
import '_shared.dart';

void main() async {
  ensureAndroidDevice();
  enableFlutterDriverExtension(commands: <CommandExtension>[nativeDriverCommands]);

  // Run on full screen.
  await SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersive);
  runApp(const ContinuousGlSurfaceViewApp());
}

class ContinuousGlSurfaceViewApp extends StatefulWidget {
  const ContinuousGlSurfaceViewApp({super.key});

  @override
  State<ContinuousGlSurfaceViewApp> createState() => _ContinuousGlSurfaceViewAppState();
}

class _ContinuousGlSurfaceViewAppState extends State<ContinuousGlSurfaceViewApp>
    with SingleTickerProviderStateMixin {
  late final AnimationController _controller = AnimationController(
    duration: const Duration(seconds: 5),
    vsync: this,
  )..repeat();

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        body: Stack(
          children: <Widget>[
            const AndroidView(viewType: 'continuous_gl_surface_view_platform_view'),
            Center(
              child: RotationTransition(
                turns: _controller,
                child: Container(
                  width: 200,
                  height: 200,
                  color: Colors.green,
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
