// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:ui';

import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  group('LocaleConfig', () {
    final log = <MethodCall>[];
    List<String>? mockLocalesResponse;
    var shouldThrowPlatformException = false;
    var shouldThrowMissingPluginException = false;

    setUp(() {
      log.clear();
      mockLocalesResponse = null;
      shouldThrowPlatformException = false;
      shouldThrowMissingPluginException = false;

      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(
        SystemChannels.localization,
        (MethodCall methodCall) async {
          log.add(methodCall);
          if (shouldThrowPlatformException) {
            throw PlatformException(code: 'unimplemented', message: 'Not implemented');
          }
          if (shouldThrowMissingPluginException) {
            throw MissingPluginException('No implementation found');
          }
          if (methodCall.method == 'Localization.getApplicationLocales') {
            return mockLocalesResponse;
          }
          return null;
        },
      );
    });

    tearDown(() {
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(
        SystemChannels.localization,
        null,
      );
    });

    test('setApplicationLocales passes locales as BCP-47 tags', () async {
      final locales = <Locale>[
        const Locale('en', 'US'),
        const Locale.fromSubtags(languageCode: 'zh', scriptCode: 'Hans', countryCode: 'CN'),
      ];

      await LocaleConfig.setApplicationLocales(locales);

      expect(log, hasLength(1));
      expect(log.first.method, 'Localization.setApplicationLocales');
      expect(log.first.arguments, <String>['en-US', 'zh-Hans-CN']);
    });

    test('setApplicationLocales handles clearing locales with an empty list', () async {
      await LocaleConfig.setApplicationLocales(<Locale>[]);

      expect(log, hasLength(1));
      expect(log.first.method, 'Localization.setApplicationLocales');
      expect(log.first.arguments, <String>[]);
    });

    test('getApplicationLocales returns parsed locales from BCP-47 tags', () async {
      mockLocalesResponse = <String>['en-US', 'zh-Hans-CN', 'es-419'];

      final List<Locale> locales = await LocaleConfig.getApplicationLocales();

      expect(log, hasLength(1));
      expect(log.first.method, 'Localization.getApplicationLocales');
      expect(log.first.arguments, isNull);

      expect(locales, hasLength(3));
      expect(locales[0], const Locale('en', 'US'));
      expect(
        locales[1],
        const Locale.fromSubtags(languageCode: 'zh', scriptCode: 'Hans', countryCode: 'CN'),
      );
      expect(locales[2], const Locale.fromSubtags(languageCode: 'es', countryCode: '419'));
    });

    test('getApplicationLocales returns empty list when platform channel returns null', () async {
      mockLocalesResponse = null;

      final List<Locale> locales = await LocaleConfig.getApplicationLocales();

      expect(log, hasLength(1));
      expect(log.first.method, 'Localization.getApplicationLocales');
      expect(locales, isEmpty);
    });

    test(
      'getApplicationLocales returns empty list when platform channel throws PlatformException (unimplemented)',
      () async {
        shouldThrowPlatformException = true;

        final List<Locale> locales = await LocaleConfig.getApplicationLocales();

        expect(log, hasLength(1));
        expect(log.first.method, 'Localization.getApplicationLocales');
        expect(locales, isEmpty);
      },
    );

    test(
      'getApplicationLocales returns empty list when platform channel throws MissingPluginException',
      () async {
        shouldThrowMissingPluginException = true;

        final List<Locale> locales = await LocaleConfig.getApplicationLocales();

        expect(log, hasLength(1));
        expect(log.first.method, 'Localization.getApplicationLocales');
        expect(locales, isEmpty);
      },
    );

    test('setApplicationLocales ignores PlatformException (unimplemented)', () async {
      shouldThrowPlatformException = true;

      await LocaleConfig.setApplicationLocales(<Locale>[const Locale('en', 'US')]);

      expect(log, hasLength(1));
      expect(log.first.method, 'Localization.setApplicationLocales');
    });

    test('setApplicationLocales ignores MissingPluginException', () async {
      shouldThrowMissingPluginException = true;

      await LocaleConfig.setApplicationLocales(<Locale>[const Locale('en', 'US')]);

      expect(log, hasLength(1));
      expect(log.first.method, 'Localization.setApplicationLocales');
    });
  });
}
