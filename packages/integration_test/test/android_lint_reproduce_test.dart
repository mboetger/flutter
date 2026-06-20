// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:io';
import 'package:flutter_test/flutter_test.dart';
import 'package:path/path.dart' as path;

void main() {
  test('FlutterTestRunner.java does not use multi-catch with ReflectiveOperationException subclasses', () {
    // Locate the Java source file.
    // When running under "flutter test", Directory.current is the package root.
    String integrationTestDir = Directory.current.path;
    if (!path.split(integrationTestDir).contains('integration_test')) {
      integrationTestDir = path.join(integrationTestDir, 'packages', 'integration_test');
    }
    final String javaFilePath = path.join(
      integrationTestDir,
      'android',
      'src',
      'main',
      'java',
      'dev',
      'flutter',
      'plugins',
      'integration_test',
      'FlutterTestRunner.java',
    );

    final javaFile = File(javaFilePath);
    expect(javaFile.existsSync(), isTrue, reason: 'FlutterTestRunner.java must exist');

    final String content = javaFile.readAsStringSync();

    // List of reflection exceptions that inherit from ReflectiveOperationException (introduced in API 19).
    // Using multi-catch with these when minSdk < 19 triggers the NewApi lint error.
    final reflectionExceptions = <String>[
      'InstantiationException',
      'IllegalAccessException',
      'InvocationTargetException',
      'NoSuchMethodException',
      'ClassNotFoundException',
      'NoSuchFieldException',
    ];

    // Regex to find catch blocks: catch (ExceptionType1 | ExceptionType2 e)
    final catchRegExp = RegExp(r'catch\s*\(([^)]+)\)');
    final Iterable<Match> matches = catchRegExp.allMatches(content);

    for (final match in matches) {
      final String catchContent = match.group(1)!;
      if (catchContent.contains('|')) {
        // It's a multi-catch. Check if it contains any of the forbidden reflection exceptions.
        final List<String> parts = catchContent.split('|').map((String s) => s.trim()).toList();
        final bool containsReflectionException = parts.any((String part) {
          // Extract all words to robustly handle optional modifiers like 'final'
          final List<String> words = part.split(RegExp(r'\s+'));
          return words.any(reflectionExceptions.contains);
        });

        expect(
          containsReflectionException,
          isFalse,
          reason:
              'FlutterTestRunner.java uses a multi-catch containing reflection exceptions: "$catchContent".\n'
              'This requires API level 19 (ReflectiveOperationException), which violates the minSdk 16 requirement.\n'
              'Please split them into individual catch blocks or catch a broader exception like Exception.',
        );
      }
    }
  });
}
