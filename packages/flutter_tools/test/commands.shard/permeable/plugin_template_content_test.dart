// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/file.dart';
import 'package:flutter_tools/src/cache.dart';
import 'package:flutter_tools/src/commands/create.dart';
import 'package:flutter_tools/src/globals.dart' as globals;

import '../../src/common.dart';
import '../../src/context.dart';
import '../../src/test_flutter_command_runner.dart';

void main() {
  late Directory tempDir;
  late Directory projectDir;

  setUpAll(() async {
    Cache.disableLocking();
  });

  setUp(() {
    tempDir = globals.fs.systemTempDirectory.createTempSync('flutter_tools_plugin_reproduce_test.');
    projectDir = tempDir.childDirectory('flutter_project');
    
    Directory repoRoot = globals.fs.directory(globals.fs.currentDirectory.path);
    while (repoRoot.path != repoRoot.parent.path &&
           !repoRoot.childDirectory('packages').childDirectory('flutter_tools').existsSync()) {
      repoRoot = repoRoot.parent;
    }
    Cache.flutterRoot = repoRoot.path;
  });

  tearDown(() {
    tryToDelete(tempDir);
  });

  testUsingContext('plugin template uses new embedding and separate MethodCallHandler (Java)', () async {
    final command = CreateCommand();
    await createTestCommandRunner(command).run(<String>[
      'create',
      '--template=plugin',
      '--no-pub',
      '--platforms=android',
      '-a',
      'java',
      projectDir.path,
    ]);

    final pluginFile = projectDir.childDirectory('android')
        .childDirectory('src')
        .childDirectory('main')
        .childDirectory('java')
        .childDirectory('com')
        .childDirectory('example')
        .childDirectory('flutter_project')
        .childFile('FlutterProjectPlugin.java');

    final handlerFile = projectDir.childDirectory('android')
        .childDirectory('src')
        .childDirectory('main')
        .childDirectory('java')
        .childDirectory('com')
        .childDirectory('example')
        .childDirectory('flutter_project')
        .childFile('FlutterProjectPluginMethodCallHandler.java');

    expect(pluginFile.existsSync(), isTrue);
    final pluginContent = pluginFile.readAsStringSync();
    
    // The main plugin class should NOT implement MethodCallHandler.
    expect(
      pluginContent,
      isNot(matches(RegExp(r'class\s+FlutterProjectPlugin\s+implements\s+[^\{]*\bMethodCallHandler\b'))),
      reason: 'The main plugin class should not implement MethodCallHandler',
    );
    expect(pluginContent, contains('implements FlutterPlugin'));
    expect(pluginContent, isNot(contains('onMethodCall')));

    // The separate MethodCallHandler file should exist and implement MethodCallHandler.
    expect(handlerFile.existsSync(), isTrue, reason: 'Expected FlutterProjectPluginMethodCallHandler.java to exist');
    final handlerContent = handlerFile.readAsStringSync();
    expect(handlerContent, contains('implements MethodCallHandler'));
    expect(handlerContent, contains('onMethodCall'));
  });

  testUsingContext('plugin template uses new embedding and separate MethodCallHandler (Kotlin)', () async {
    final command = CreateCommand();
    await createTestCommandRunner(command).run(<String>[
      'create',
      '--template=plugin',
      '--no-pub',
      '--platforms=android',
      '-a',
      'kotlin',
      projectDir.path,
    ]);

    final pluginFile = projectDir.childDirectory('android')
        .childDirectory('src')
        .childDirectory('main')
        .childDirectory('kotlin')
        .childDirectory('com')
        .childDirectory('example')
        .childDirectory('flutter_project')
        .childFile('FlutterProjectPlugin.kt');

    final handlerFile = projectDir.childDirectory('android')
        .childDirectory('src')
        .childDirectory('main')
        .childDirectory('kotlin')
        .childDirectory('com')
        .childDirectory('example')
        .childDirectory('flutter_project')
        .childFile('FlutterProjectPluginMethodCallHandler.kt');

    expect(pluginFile.existsSync(), isTrue);
    final pluginContent = pluginFile.readAsStringSync();

    // The main plugin class should NOT implement MethodCallHandler.
    expect(
      pluginContent,
      isNot(matches(RegExp(r'class\s+FlutterProjectPlugin\s*:[^\{]*\bMethodCallHandler\b'))),
      reason: 'The main plugin class should not implement MethodCallHandler',
    );
    expect(pluginContent, contains('FlutterPlugin'));
    expect(pluginContent, isNot(contains('onMethodCall')));

    // The separate MethodCallHandler file should exist and implement MethodCallHandler.
    expect(handlerFile.existsSync(), isTrue, reason: 'Expected FlutterProjectPluginMethodCallHandler.kt to exist');
    final handlerContent = handlerFile.readAsStringSync();
    expect(handlerContent, contains('MethodCallHandler'));
    expect(handlerContent, contains('onMethodCall'));
  });
}
