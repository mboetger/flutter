// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:io';
import 'package:flutter_driver/flutter_driver.dart';
import 'package:test/test.dart' hide TypeMatcher, isInstanceOf;

Future<void> main() async {
  FlutterDriver? driver;

  setUpAll(() async {
    driver = await FlutterDriver.connect();
  });

  tearDownAll(() async {
    await driver?.close();
  });

  // Each test below must return back to the home page after finishing.

  test('MotionEvent recomposition', () async {
    final SerializableFinder motionEventsListTile = find.byValueKey('MotionEventsListTile');
    await driver?.tap(motionEventsListTile);
    await driver?.runUnsynchronized(() async {
      await driver?.waitFor(find.byValueKey('PlatformView'));
    });
    final String errorMessage = (await driver?.requestData('run test'))!;
    expect(errorMessage, '');
    final SerializableFinder backButton = find.byValueKey('back');
    await driver?.tap(backButton);
  }, timeout: Timeout.none);

  test('WebView keyboard input', () async {
    final SerializableFinder webViewListTile = find.byValueKey('WebViewListTile');
    await driver?.tap(webViewListTile);

    await driver?.runUnsynchronized(() async {
      await driver?.waitFor(find.byValueKey('WebViewPlatformView'));
    });

    // Wait for the WebView to fully load the HTML.
    await driver?.waitFor(find.byValueKey('WebViewLoaded'));

    // Focus the input field inside WebView.
    final String focusResult = await driver!.requestData('focus_webview');
    expect(focusResult, 'ok');

    // Wait for the input field to be focused.
    await driver?.waitFor(find.byValueKey('WebViewFocused'));

    final String? deviceId = Platform.environment['FLUTTER_DEVICE_ID_NUMBER'];
    final String? adbPath = Platform.environment['FLUTTER_ADB_PATH'];

    // Use adb to input text
    final ProcessResult result = await Process.run(adbPath ?? 'adb', <String>[
      if (deviceId != null) ...<String>['-s', deviceId],
      'shell',
      'input',
      'text',
      'hello-webview',
    ]);

    expect(result.exitCode, equals(0), reason: 'adb input text failed: ${result.stderr}');

    // Wait and verify that the text was received by Flutter
    var text = '';
    for (var i = 0; i < 10; i++) {
      text = (await driver?.getText(find.byValueKey('WebViewText')))!;
      if (text == 'hello-webview') {
        break;
      }
      await Future<void>.delayed(const Duration(milliseconds: 500));
    }

    expect(text, 'hello-webview');

    // Go back to home page
    final SerializableFinder backButton = find.byValueKey('back');
    await driver?.tap(backButton);
  }, timeout: Timeout.none);

  group('WindowManager', () {
    setUpAll(() async {
      final SerializableFinder wmListTile = find.byValueKey('WmIntegrationsListTile');
      await driver?.tap(wmListTile);
    });

    tearDownAll(() async {
      await driver?.waitFor(find.pageBack());
      await driver?.tap(find.pageBack());
    });

    test('AlertDialog from platform view context', () async {
      final SerializableFinder showAlertDialog = find.byValueKey('ShowAlertDialog');
      await driver?.waitFor(showAlertDialog);
      await driver?.tap(showAlertDialog);
      final String status = (await driver?.getText(find.byValueKey('Status')))!;
      expect(status, 'Success');
    }, timeout: Timeout.none);

    test(
      'Child windows can handle touches',
      () async {
        final SerializableFinder addWindow = find.byValueKey('AddWindow');
        await driver?.waitFor(addWindow);
        await driver?.tap(addWindow);
        final SerializableFinder tapWindow = find.byValueKey('TapWindow');
        await driver?.tap(tapWindow);
        final String windowClickCount = (await driver?.getText(
          find.byValueKey('WindowClickCount'),
        ))!;
        expect(windowClickCount, 'Click count: 1');
      },
      timeout: Timeout.none,
      // TODO(garyq): Skipped, see https://github.com/flutter/flutter/issues/88479
      skip: true,
    );
  });
}
