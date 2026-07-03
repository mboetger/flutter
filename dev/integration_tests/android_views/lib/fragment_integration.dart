// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import 'page.dart';

class FragmentIntegrationPage extends PageWidget {
  const FragmentIntegrationPage({super.key})
    : super('Fragment Integration Tests', const ValueKey<String>('FragmentIntegrationListTile'));

  @override
  Widget build(BuildContext context) => const FragmentIntegrationBody();
}

class FragmentIntegrationBody extends StatefulWidget {
  const FragmentIntegrationBody({super.key});

  @override
  State<FragmentIntegrationBody> createState() => _FragmentIntegrationBodyState();
}

class _FragmentIntegrationBodyState extends State<FragmentIntegrationBody> {
  static const MethodChannel _channel = MethodChannel('android_views_integration');
  String _status = 'Idle';
  String _editTextText = '';
  bool _popupShown = false;
  String? _popupError;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Fragment Integration')),
      body: Column(
        children: <Widget>[
          const SizedBox(
            height: 300,
            width: 300,
            child: AndroidView(
              key: ValueKey<String>('FragmentPlatformView'),
              viewType: 'fragment_view',
            ),
          ),
          Padding(
            padding: const EdgeInsets.all(8.0),
            key: const ValueKey<String>('StatusContainer'),
            child: Column(
              children: <Widget>[
                Text('Status: $_status', key: const ValueKey<String>('Status')),
                Text('EditText: $_editTextText', key: const ValueKey<String>('EditTextText')),
                Text('PopupShown: $_popupShown', key: const ValueKey<String>('PopupShown')),
                Text('PopupError: $_popupError', key: const ValueKey<String>('PopupError')),
              ],
            ),
          ),
          ElevatedButton(
            key: const ValueKey<String>('RefreshStatus'),
            onPressed: _refreshStatus,
            child: const Text('Refresh Status'),
          ),
          ElevatedButton(
            key: const ValueKey<String>('TriggerPopup'),
            onPressed: _triggerPopup,
            child: const Text('Trigger Popup'),
          ),
        ],
      ),
    );
  }

  Future<void> _refreshStatus() async {
    try {
      final String text = await _channel.invokeMethod<String>('getEditTextText') ?? '';
      final Map<dynamic, dynamic> popupResult =
          await _channel.invokeMethod<Map<dynamic, dynamic>>('getPopupResult') ??
          <dynamic, dynamic>{};
      setState(() {
        _editTextText = text;
        _popupShown = popupResult['shown'] as bool? ?? false;
        _popupError = popupResult['error'] as String?;
        _status = 'Refreshed';
      });
    } catch (e) {
      setState(() {
        _status = 'Error: $e';
      });
    }
  }

  Future<void> _triggerPopup() async {
    try {
      await _channel.invokeMethod<void>('clickPopup');
      await _refreshStatus();
    } catch (e) {
      setState(() {
        _status = 'Error triggering popup: $e';
      });
    }
  }
}
