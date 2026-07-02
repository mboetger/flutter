// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/material.dart';
import 'package:flutter_driver/driver_extension.dart';

import 'motion_events_page.dart';
import 'page.dart';
import 'webview_page.dart';
import 'wm_integrations.dart';

final List<PageWidget> _allPages = <PageWidget>[
  const MotionEventsPage(),
  const WindowManagerIntegrationsPage(),
  const WebViewPage(),
];

void main() {
  enableFlutterDriverExtension(
    handler: (String? message) async {
      if (message == 'get_device_pixel_ratio') {
        return WidgetsBinding.instance.platformDispatcher.views.first.devicePixelRatio.toString();
      }
      if (message == 'focus_webview') {
        if (webViewTestFocus != null) {
          await webViewTestFocus!();
          return 'ok';
        }
        return 'not_found';
      }
      return driverDataHandler.handleMessage(message);
    },
  );
  runApp(
    MaterialApp(
      theme: ThemeData(
        pageTransitionsTheme: const PageTransitionsTheme(
          builders: <TargetPlatform, PageTransitionsBuilder>{
            TargetPlatform.android: ZoomPageTransitionsBuilder(),
          },
        ),
      ),
      home: const Home(),
    ),
  );
}

class Home extends StatelessWidget {
  const Home({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: ListView(
        children: _allPages.map((PageWidget p) => _buildPageListTile(context, p)).toList(),
      ),
    );
  }

  Widget _buildPageListTile(BuildContext context, PageWidget page) {
    return ListTile(
      title: Text(page.title),
      key: page.tileKey,
      onTap: () {
        _pushPage(context, page);
      },
    );
  }

  void _pushPage(BuildContext context, PageWidget page) {
    Navigator.of(context).push(MaterialPageRoute<void>(builder: (_) => Scaffold(body: page)));
  }
}
