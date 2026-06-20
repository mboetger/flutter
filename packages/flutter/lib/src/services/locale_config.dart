// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:ui';

import 'message_codec.dart';
import 'system_channels.dart';

/// Class for configuring application-specific locales, such as per-app language
/// preferences.
///
/// Under Android 13 (API 33) and higher, these APIs will read and write the
/// per-app language preference, allowing programmatically changing the
/// language of the individual application without affecting the global system
/// language.
///
/// On other platforms, or on older Android versions, these methods degrade
/// gracefully, either doing nothing or returning empty values.
class LocaleConfig {
  LocaleConfig._();

  /// Sets the application-specific locales.
  ///
  /// On Android 13 (API 33) or higher, this sets the per-app language preference.
  /// If [locales] is empty, it resets the app-specific language preference,
  /// causing the app to fall back to the system default language.
  ///
  /// On other platforms or older Android versions, this is a silent no-op.
  static Future<void> setApplicationLocales(List<Locale> locales) async {
    try {
      final List<String> localeTags = locales
          .map((Locale locale) => locale.toLanguageTag())
          .toList();
      await SystemChannels.localization.invokeMethod<void>(
        'Localization.setApplicationLocales',
        localeTags,
      );
    } on PlatformException catch (e) {
      if (e.code != 'unimplemented') {
        rethrow;
      }
    } on MissingPluginException {
      // Ignore if the channel is missing.
    }
  }

  /// Gets the application-specific locales.
  ///
  /// On Android 13 (API 33) or higher, this gets the per-app language preference.
  ///
  /// On other platforms or older Android versions, or if the platform returns
  /// null, this returns an empty list.
  static Future<List<Locale>> getApplicationLocales() async {
    try {
      final List<dynamic>? localeTags = await SystemChannels.localization
          .invokeMethod<List<dynamic>>('Localization.getApplicationLocales');
      if (localeTags == null) {
        return const <Locale>[];
      }
      return localeTags.cast<String>().map(_localeFromString).toList();
    } on PlatformException catch (e) {
      if (e.code != 'unimplemented') {
        rethrow;
      }
    } on MissingPluginException {
      // Ignore if the channel is missing.
    }
    return const <Locale>[];
  }

  static Locale _localeFromString(String localeString) {
    final List<String> parts = localeString.replaceAll('_', '-').split('-');
    final String languageCode = parts[0];
    String? scriptCode;
    String? countryCode;
    var index = 1;
    if (parts.length > index && parts[index].length == 4) {
      scriptCode = parts[index];
      index++;
    }
    if (parts.length > index && parts[index].length >= 2 && parts[index].length <= 3) {
      countryCode = parts[index];
      index++;
    }
    return Locale.fromSubtags(
      languageCode: languageCode,
      scriptCode: scriptCode,
      countryCode: countryCode,
    );
  }
}
