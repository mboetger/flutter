// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

import '../widgets/clipboard_utils.dart';

void main() {
  final mockClipboard = MockClipboard();
  TestWidgetsFlutterBinding.ensureInitialized().defaultBinaryMessenger.setMockMethodCallHandler(
    SystemChannels.platform,
    mockClipboard.handleMethodCall,
  );

  test('Clipboard.getData returns text', () async {
    mockClipboard.clipboardData = <String, dynamic>{'text': 'Hello world'};

    final ClipboardData? data = await Clipboard.getData(Clipboard.kTextPlain);

    expect(data, isNotNull);
    expect(data!.text, equals('Hello world'));
  });

  test('Clipboard.getData returns null', () async {
    mockClipboard.clipboardData = null;

    final ClipboardData? data = await Clipboard.getData(Clipboard.kTextPlain);

    expect(data, isNull);
  });

  test('Clipboard.getData throws if text is missing', () async {
    mockClipboard.clipboardData = <String, dynamic>{};

    expect(() => Clipboard.getData(Clipboard.kTextPlain), throwsA(isA<TypeError>()));
  });

  test('Clipboard.getData throws if text is null', () async {
    mockClipboard.clipboardData = <String, dynamic>{'text': null};

    expect(() => Clipboard.getData(Clipboard.kTextPlain), throwsA(isA<TypeError>()));
  });

  test('Clipboard.setData sets text', () async {
    await Clipboard.setData(const ClipboardData(text: 'Hello world'));

    expect(mockClipboard.clipboardData, <String, dynamic>{
      'text': 'Hello world',
      'isSensitive': false,
    });
  });

  test('ClipboardData defaults isSensitive to false', () async {
    const data = ClipboardData(text: 'normal text');
    expect(data.isSensitive, isFalse);

    await Clipboard.setData(data);

    expect(mockClipboard.clipboardData, isA<Map<String, dynamic>>());
    final sentData = mockClipboard.clipboardData as Map<String, dynamic>;
    expect(sentData['text'], 'normal text');
    expect(sentData['isSensitive'], isFalse);
  });

  test('ClipboardData supports isSensitive: false explicitly', () async {
    // ignore: avoid_redundant_argument_values
    const data = ClipboardData(text: 'normal text', isSensitive: false);
    expect(data.isSensitive, isFalse);

    await Clipboard.setData(data);

    expect(mockClipboard.clipboardData, isA<Map<String, dynamic>>());
    final sentData = mockClipboard.clipboardData as Map<String, dynamic>;
    expect(sentData['text'], 'normal text');
    expect(sentData['isSensitive'], isFalse);
  });

  test('ClipboardData supports isSensitive: true explicitly and propagates it', () async {
    const data = ClipboardData(text: 'sensitive text', isSensitive: true);
    expect(data.isSensitive, isTrue);

    await Clipboard.setData(data);

    expect(mockClipboard.clipboardData, isA<Map<String, dynamic>>());
    final sentData = mockClipboard.clipboardData as Map<String, dynamic>;
    expect(sentData['text'], 'sensitive text');
    expect(sentData['isSensitive'], isTrue);
  });
}
