// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

@TestOn('!windows')
library;

import 'dart:io';
import 'package:engine_repo_tools/engine_repo_tools.dart';
import 'package:path/path.dart' as p;
import 'package:test/test.dart';

void main() {
  final engine = Engine.findWithin();
  final String engineFlutterDir = engine.flutterDir.path;

  late Directory tempDir;
  late Directory tempAndroidLintDir;
  late String mockAndroidDir;

  setUp(() {
    // Create a temporary directory for a mock engine source structure
    tempDir = Directory.systemTemp.createTempSync('android_lint_test_');

    // Copy the entire android_lint tool to the temp directory to maintain 100% hermeticity
    final String realAndroidLintDir = p.join(engineFlutterDir, 'tools', 'android_lint');
    tempAndroidLintDir = Directory(p.join(tempDir.path, 'flutter', 'tools', 'android_lint'));
    tempAndroidLintDir.createSync(recursive: true);

    void copyDirectory(Directory source, Directory destination) {
      for (final FileSystemEntity entity in source.listSync()) {
        final String name = p.basename(entity.path);
        if (name.startsWith('.')) {
          continue; // Skip hidden folders like .dart_tool
        }
        final String newPath = p.join(destination.path, p.basename(entity.path));
        if (entity is Directory) {
          final newDir = Directory(newPath)..createSync();
          copyDirectory(entity, newDir);
        } else if (entity is File) {
          entity.copySync(newPath);
        }
      }
    }

    copyDirectory(Directory(realAndroidLintDir), tempAndroidLintDir);

    // Create the mock flutter/shell/platform/android directory
    mockAndroidDir = p.join(tempDir.path, 'flutter', 'shell', 'platform', 'android');
    Directory(mockAndroidDir).createSync(recursive: true);

    // Write a simple AndroidManifest.xml
    final manifestFile = File(p.join(mockAndroidDir, 'AndroidManifest.xml'));
    manifestFile.writeAsStringSync('''
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="io.flutter.embedding.android">
    <uses-sdk android:minSdkVersion="16" />
</manifest>
''');

    // Write a Java file with UnknownNullness warning (missing annotations on public API)
    final String mockJavaDir = p.join(mockAndroidDir, 'io', 'flutter');
    Directory(mockJavaDir).createSync(recursive: true);
    final javaFile = File(p.join(mockJavaDir, 'TempWarning.java'));
    javaFile.writeAsStringSync('''
package io.flutter;

public class TempWarning {
    // Lacks @NonNull or @Nullable annotations on its parameter and return type,
    // triggering the UnknownNullness warning.
    public String greet(String name) {
        return "Hello, " + name;
    }
}
''');

    // Symlink the real third_party/android_tools and third_party/java into the temp directory
    final String realThirdParty = p.join(engineFlutterDir, 'third_party');
    final String mockThirdParty = p.join(tempDir.path, 'flutter', 'third_party');
    Directory(mockThirdParty).createSync(recursive: true);

    Link(
      p.join(mockThirdParty, 'android_tools'),
    ).createSync(p.join(realThirdParty, 'android_tools'));
    Link(p.join(mockThirdParty, 'java')).createSync(p.join(realThirdParty, 'java'));
  });

  tearDown(() {
    if (tempDir.existsSync()) {
      tempDir.deleteSync(recursive: true);
    }
  });

  test('Linter fails by default on UnknownNullness warning (new violation)', () async {
    // By default, UnknownNullness is not ignored and TempWarning.java is not in the baseline.
    final ProcessResult result = await Process.run(Platform.resolvedExecutable, <String>[
      '--packages=${p.join(engineFlutterDir, '.dart_tool', 'package_config.json')}',
      p.join(tempAndroidLintDir.path, 'bin', 'main.dart'),
      '--in',
      tempDir.path,
    ]);

    expect(
      result.exitCode,
      isNot(0),
      reason:
          'Linter should fail by default on UnknownNullness.\n'
          'Stdout:\n${result.stdout}\n'
          'Stderr:\n${result.stderr}',
    );
    expect(result.stdout, contains('TempWarning.java'));
    expect(result.stdout, contains('[UnknownNullness]'));
  });

  test('Linter passes when UnknownNullness warning is explicitly ignored in lint.xml', () async {
    // 1. Modify the temp lint.xml to ignore UnknownNullness
    final tempLintXmlFile = File(p.join(tempAndroidLintDir.path, 'lint.xml'));
    final String currentContent = tempLintXmlFile.readAsStringSync();
    // Insert the ignore rule before the closing </lint> tag
    final String updatedContent = currentContent.replaceFirst(
      '</lint>',
      '    <issue id="UnknownNullness" severity="ignore" />\n</lint>',
    );
    tempLintXmlFile.writeAsStringSync(updatedContent);

    // 2. Run the linter
    final ProcessResult result = await Process.run(Platform.resolvedExecutable, <String>[
      '--packages=${p.join(engineFlutterDir, '.dart_tool', 'package_config.json')}',
      p.join(tempAndroidLintDir.path, 'bin', 'main.dart'),
      '--in',
      tempDir.path,
    ]);

    expect(
      result.exitCode,
      0,
      reason:
          'Linter should pass when UnknownNullness is ignored in lint.xml.\n'
          'Stdout:\n${result.stdout}\n'
          'Stderr:\n${result.stderr}',
    );
  });

  test('Linter passes after generating a new baseline containing the warning', () async {
    // 1. Run the linter with --rebaseline to capture the warning in a new baseline.xml
    final ProcessResult rebaselineResult = await Process.run(Platform.resolvedExecutable, <String>[
      '--packages=${p.join(engineFlutterDir, '.dart_tool', 'package_config.json')}',
      p.join(tempAndroidLintDir.path, 'bin', 'main.dart'),
      '--in',
      tempDir.path,
      '--rebaseline',
    ]);

    // The rebaseline run will exit with non-zero code to indicate a baseline was created,
    // but the baseline.xml file should now exist.
    final tempBaselineFile = File(p.join(tempAndroidLintDir.path, 'baseline.xml'));
    expect(
      tempBaselineFile.existsSync(),
      isTrue,
      reason:
          'baseline.xml should be created by --rebaseline.\n'
          'Rebaseline Exit Code: ${rebaselineResult.exitCode}\n'
          'Stdout:\n${rebaselineResult.stdout}\n'
          'Stderr:\n${rebaselineResult.stderr}',
    );
    expect(tempBaselineFile.readAsStringSync(), contains('UnknownNullness'));

    // 2. Run the linter again without --rebaseline. It should now use the baseline and pass.
    final ProcessResult runResult = await Process.run(Platform.resolvedExecutable, <String>[
      '--packages=${p.join(engineFlutterDir, '.dart_tool', 'package_config.json')}',
      p.join(tempAndroidLintDir.path, 'bin', 'main.dart'),
      '--in',
      tempDir.path,
    ]);

    expect(
      runResult.exitCode,
      0,
      reason:
          'Linter should pass when warning is filtered by baseline.xml.\n'
          'Stdout:\n${runResult.stdout}\n'
          'Stderr:\n${runResult.stderr}',
    );
    expect(runResult.stdout, contains(RegExp(r'filtered by baseline.*baseline\.xml')));
  });
}
