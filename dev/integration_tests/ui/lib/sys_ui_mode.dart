// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/material.dart';

void main() {
  runApp(const SysUiModeApp());
}

class SysUiModeApp extends StatelessWidget {
  const SysUiModeApp({super.key});

  static const Key topPaddingKey = Key('top_padding');
  static const Key bottomPaddingKey = Key('bottom_padding');

  @override
  Widget build(BuildContext context) {
    final EdgeInsets padding = MediaQuery.paddingOf(context);
    return MaterialApp(
      home: Scaffold(
        body: Center(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: <Widget>[
              Text('Top Padding: ${padding.top}', key: topPaddingKey),
              Text('Bottom Padding: ${padding.bottom}', key: bottomPaddingKey),
            ],
          ),
        ),
      ),
    );
  }
}
