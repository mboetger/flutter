// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'page.dart';

/// Callback to trigger focus on the WebView's input field.
Future<void> Function()? webViewTestFocus;

class WebViewPage extends PageWidget {
  const WebViewPage({super.key})
    : super('WebView Keyboard Test', const ValueKey<String>('WebViewListTile'));

  @override
  Widget build(BuildContext context) => const WebViewBody();
}

class WebViewBody extends StatefulWidget {
  const WebViewBody({super.key});

  @override
  State<WebViewBody> createState() => _WebViewBodyState();
}

class _WebViewBodyState extends State<WebViewBody> {
  String _currentText = '';
  bool _isLoaded = false;
  bool _isFocused = false;
  MethodChannel? _viewChannel;

  @override
  void initState() {
    super.initState();
    webViewTestFocus = _focus;
  }

  @override
  void dispose() {
    if (webViewTestFocus == _focus) {
      webViewTestFocus = null;
    }
    _viewChannel?.setMethodCallHandler(null);
    super.dispose();
  }

  Future<void> _focus() async {
    if (_viewChannel != null) {
      await _viewChannel!.invokeMethod<void>('focus');
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('WebView Keyboard'),
        leading: IconButton(
          key: const ValueKey<String>('back'),
          icon: const Icon(Icons.arrow_back),
          onPressed: () => Navigator.pop(context),
        ),
      ),
      body: Column(
        children: <Widget>[
          SizedBox(
            height: 300,
            child: AndroidView(
              key: const ValueKey<String>('WebViewPlatformView'),
              viewType: 'web_view',
              onPlatformViewCreated: _onPlatformViewCreated,
            ),
          ),
          if (_isLoaded)
            const SizedBox(key: ValueKey<String>('WebViewLoaded'), width: 1, height: 1),
          if (_isFocused)
            const SizedBox(key: ValueKey<String>('WebViewFocused'), width: 1, height: 1),
          Container(
            padding: const EdgeInsets.all(16.0),
            color: Colors.grey[200],
            child: Text(
              _currentText,
              key: const ValueKey<String>('WebViewText'),
              style: const TextStyle(fontSize: 24, fontWeight: FontWeight.bold),
            ),
          ),
        ],
      ),
    );
  }

  void _onPlatformViewCreated(int id) {
    _viewChannel = MethodChannel('platform_views_integration/webview_$id');
    _viewChannel!.setMethodCallHandler((MethodCall call) async {
      if (call.method == 'onTextChange') {
        if (!mounted) {
          return;
        }
        setState(() {
          _currentText = call.arguments as String;
        });
      } else if (call.method == 'onPageFinished') {
        if (!mounted) {
          return;
        }
        setState(() {
          _isLoaded = true;
        });
      } else if (call.method == 'onFocus') {
        if (!mounted) {
          return;
        }
        setState(() {
          _isFocused = true;
        });
      }
    });
  }
}
