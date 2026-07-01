// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:args/command_runner.dart';
import 'package:file/file.dart';
import 'package:flutter_tools/src/cache.dart';
import 'package:flutter_tools/src/commands/create.dart';
import 'package:flutter_tools/src/globals.dart' as globals;
import 'package:flutter_tools/src/project.dart';

import '../../src/common.dart';
import '../../src/context.dart';
import '../../src/test_flutter_command_runner.dart';
import 'utils/project_testing_utils.dart';

void main() {
  late Directory tempDir;
  late Directory projectDir;

  setUpAll(() async {
    Cache.disableLocking();
    await ensureFlutterToolsSnapshot();
  });

  setUp(() {
    tempDir = globals.fs.systemTempDirectory.createTempSync('flutter_tools_create_reproduce_test.');
    projectDir = tempDir.childDirectory('flutter_project');
    Cache.flutterRoot = '../..';
  });

  tearDown(() {
    tryToDelete(tempDir);
  });

  tearDownAll(() async {
    await restoreFlutterToolsSnapshot();
  });

  testUsingContext(
    'flutter create generates strings.xml and references it in AndroidManifest.xml',
    () async {
      final command = CreateCommand();
      final CommandRunner<void> runner = createTestCommandRunner(command);

      await runner.run(<String>['create', '--no-pub', '--template=app', projectDir.path]);

      final File stringsXml = projectDir
          .childDirectory('android')
          .childDirectory('app')
          .childDirectory('src')
          .childDirectory('main')
          .childDirectory('res')
          .childDirectory('values')
          .childFile('strings.xml');

      final File androidManifest = projectDir
          .childDirectory('android')
          .childDirectory('app')
          .childDirectory('src')
          .childDirectory('main')
          .childFile('AndroidManifest.xml');

      expect(stringsXml.existsSync(), isTrue, reason: 'strings.xml should be generated');
      final String stringsContent = stringsXml.readAsStringSync();
      expect(stringsContent, contains('<string name="app_name">flutter_project</string>'));

      expect(
        androidManifest.existsSync(),
        isTrue,
        reason: 'AndroidManifest.xml should be generated',
      );
      final String manifestContent = androidManifest.readAsStringSync();
      expect(manifestContent, contains('android:label="@string/app_name"'));
    },
  );

  testUsingContext(
    'flutter create --template=module generates strings.xml and references it in AndroidManifest.xml',
    () async {
      final command = CreateCommand();
      final CommandRunner<void> runner = createTestCommandRunner(command);

      await runner.run(<String>['create', '--no-pub', '--template=module', projectDir.path]);

      // Write a dummy package_config.json to satisfy the plugin resolution during tooling generation.
      final File packageConfigFile = projectDir
          .childDirectory('.dart_tool')
          .childFile('package_config.json');
      packageConfigFile.createSync(recursive: true);
      packageConfigFile.writeAsStringSync(
        '{"configVersion": 2, "packages": [{"name": "flutter_project", "rootUri": "../", "packageUri": "lib/", "languageVersion": "3.0"}]}',
      );

      // Write a dummy package_graph.json to satisfy the package graph loader.
      final File packageGraphFile = projectDir
          .childDirectory('.dart_tool')
          .childFile('package_graph.json');
      packageGraphFile.createSync(recursive: true);
      packageGraphFile.writeAsStringSync(
        '{"configVersion": 1, "packages": [{"name": "flutter_project", "dependencies": [], "devDependencies": []}], "roots": ["flutter_project"]}',
      );

      final FlutterProject project = FlutterProject.fromDirectory(projectDir);
      await project.ensureReadyForPlatformSpecificTooling(
        releaseMode: false,
        androidPlatform: true,
      );

      final File stringsXml = projectDir
          .childDirectory('.android')
          .childDirectory('app')
          .childDirectory('src')
          .childDirectory('main')
          .childDirectory('res')
          .childDirectory('values')
          .childFile('strings.xml');

      final File androidManifest = projectDir
          .childDirectory('.android')
          .childDirectory('app')
          .childDirectory('src')
          .childDirectory('main')
          .childFile('AndroidManifest.xml');

      expect(stringsXml.existsSync(), isTrue, reason: 'strings.xml should be generated');
      final String stringsContent = stringsXml.readAsStringSync();
      expect(stringsContent, contains('<string name="app_name">flutter_project</string>'));

      expect(
        androidManifest.existsSync(),
        isTrue,
        reason: 'AndroidManifest.xml should be generated',
      );
      final String manifestContent = androidManifest.readAsStringSync();
      expect(manifestContent, contains('android:label="@string/app_name"'));
    },
  );
}
