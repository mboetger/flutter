// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:convert';
import 'dart:io' as io;

import 'package:file/file.dart';
import 'package:flutter_tools/src/android/gradle_utils.dart' show getGradlewFileName;
import 'package:flutter_tools/src/base/io.dart';
import 'package:xml/xml.dart';

import '../src/common.dart';
import 'test_utils.dart';

final XmlElement issue74784IntentFilter = XmlElement(
  XmlName('intent-filter'),
  <XmlAttribute>[XmlAttribute(XmlName('autoVerify', 'android'), 'true')],
  <XmlElement>[
    XmlElement(XmlName('action'), <XmlAttribute>[
      XmlAttribute(XmlName('name', 'android'), 'android.intent.action.VIEW'),
    ]),
    XmlElement(XmlName('category'), <XmlAttribute>[
      XmlAttribute(XmlName('name', 'android'), 'android.intent.category.DEFAULT'),
    ]),
    XmlElement(XmlName('category'), <XmlAttribute>[
      XmlAttribute(XmlName('name', 'android'), 'android.intent.category.BROWSABLE'),
    ]),
    XmlElement(XmlName('data'), <XmlAttribute>[
      XmlAttribute(XmlName('scheme', 'android'), 'https'),
      XmlAttribute(XmlName('host', 'android'), 'example.com'),
    ]),
  ],
);

void main() {
  late Directory tempDir;

  setUp(() async {
    tempDir = createResolvedTempDirectorySync('run_test.');
  });

  tearDown(() async {
    tryToDelete(tempDir);
  });

  testWithoutContext(
    'gradle task outputsReleaseAppLinkSettings and release build signing config demonstrate issue #74784',
    () async {
      // Create a new flutter project (step 1 of issue #74784).
      ProcessResult result = await processManager.run(<String>[
        flutterBin,
        'create',
        tempDir.path,
        '--project-name=testapp',
      ], workingDirectory: tempDir.path);
      printOnFailure('Created testapp in $tempDir');
      expect(result, const ProcessResultMatcher());

      // Verify that by default in Flutter templates, the release build type uses debug signing config.
      // This demonstrates the root cause of issue #74784: when a developer runs `flutter build apk`
      // (which defaults to --release) without overriding signingConfig in build.gradle/build.gradle.kts,
      // the APK is signed with debug.keystore. When the developer configures assetlinks.json on their web
      // server with the SHA-256 fingerprint of their release keystore, Android App Links verification fails
      // because the APK's certificate fingerprint (debug) mismatches the web server's assetlinks.json (release).
      final buildGradleFile = io.File(
        fileSystem.path.join(tempDir.path, 'android', 'app', 'build.gradle'),
      );
      final buildGradleKtsFile = io.File(
        fileSystem.path.join(tempDir.path, 'android', 'app', 'build.gradle.kts'),
      );
      final bool hasGradle = buildGradleFile.existsSync();
      final bool hasGradleKts = buildGradleKtsFile.existsSync();
      expect(hasGradle || hasGradleKts, isTrue);

      final String buildScriptContent = hasGradle
          ? buildGradleFile.readAsStringSync()
          : buildGradleKtsFile.readAsStringSync();
      expect(buildScriptContent, contains('signingConfig'));
      expect(buildScriptContent, contains('debug'));
      expect(buildScriptContent, contains('assetlinks.json'));

      // Add the exact intent-filter from issue #74784 to AndroidManifest.xml (step 3 of issue #74784).
      final String androidManifestPath = fileSystem.path.join(
        tempDir.path,
        'android',
        'app',
        'src',
        'main',
        'AndroidManifest.xml',
      );
      final androidManifestFile = io.File(androidManifestPath);
      final androidManifest = XmlDocument.parse(androidManifestFile.readAsStringSync());
      final XmlElement activity = androidManifest.findAllElements('activity').first;
      activity.children.add(issue74784IntentFilter.copy());
      androidManifestFile.writeAsStringSync(androidManifest.toString(), flush: true);

      // Ensure that gradle files exist and config is generated for release mode.
      result = await processManager.run(<String>[
        flutterBin,
        'build',
        'apk',
        '--release',
        '--config-only',
      ], workingDirectory: tempDir.path);
      expect(result, const ProcessResultMatcher());

      final Directory androidApp = tempDir.childDirectory('android');
      final io.File fileDump = tempDir
          .childDirectory('build')
          .childDirectory('app')
          .childFile('app-link-settings-release.json');

      // Run the Gradle task to output release App Link settings.
      result = await processManager.run(<String>[
        '.${platform.pathSeparator}${getGradlewFileName(platform)}',
        ...getLocalEngineArguments(),
        '-q', // quiet output.
        '-PoutputPath=${fileDump.path}',
        'outputReleaseAppLinkSettings',
      ], workingDirectory: androidApp.path);

      expect(result, const ProcessResultMatcher());
      expect(fileDump.existsSync(), true);

      final json = jsonDecode(fileDump.readAsStringSync()) as Map<String, dynamic>;
      expect(json['applicationId'], 'com.example.testapp');
      final deeplinks = json['deeplinks']! as List<dynamic>;
      expect(deeplinks.length, 1);

      // Verify that the manifest parsing and Gradle tooling correctly preserve and extract
      // android:autoVerify="true" and all intent filter attributes in release mode without stripping them.
      final deeplink = deeplinks[0] as Map<String, dynamic>;
      expect(deeplink['scheme'], 'https');
      expect(deeplink['host'], 'example.com');
      expect(deeplink['path'], '.*');

      final intentFilterCheck = deeplink['intentFilterCheck'] as Map<String, dynamic>;
      expect(intentFilterCheck['hasAutoVerify'], true);
      expect(intentFilterCheck['hasActionView'], true);
      expect(intentFilterCheck['hasDefaultCategory'], true);
      expect(intentFilterCheck['hasBrowsableCategory'], true);
    },
  );

  testWithoutContext(
    'gradle task outputsDebugAppLinkSettings verifies autoVerify and intent filter parsing for local development',
    () async {
      // Create a new flutter project.
      ProcessResult result = await processManager.run(<String>[
        flutterBin,
        'create',
        tempDir.path,
        '--project-name=testapp',
      ], workingDirectory: tempDir.path);
      expect(result, const ProcessResultMatcher());

      // Add the exact intent-filter from issue #74784 to AndroidManifest.xml.
      final String androidManifestPath = fileSystem.path.join(
        tempDir.path,
        'android',
        'app',
        'src',
        'main',
        'AndroidManifest.xml',
      );
      final androidManifestFile = io.File(androidManifestPath);
      final androidManifest = XmlDocument.parse(androidManifestFile.readAsStringSync());
      final XmlElement activity = androidManifest.findAllElements('activity').first;
      activity.children.add(issue74784IntentFilter.copy());
      androidManifestFile.writeAsStringSync(androidManifest.toString(), flush: true);

      // Ensure that gradle files exist and config is generated for debug mode.
      result = await processManager.run(<String>[
        flutterBin,
        'build',
        'apk',
        '--debug',
        '--config-only',
      ], workingDirectory: tempDir.path);
      expect(result, const ProcessResultMatcher());

      final Directory androidApp = tempDir.childDirectory('android');
      final io.File fileDump = tempDir
          .childDirectory('build')
          .childDirectory('app')
          .childFile('app-link-settings-debug.json');

      // Run the Gradle task to output debug App Link settings.
      result = await processManager.run(<String>[
        '.${platform.pathSeparator}${getGradlewFileName(platform)}',
        ...getLocalEngineArguments(),
        '-q', // quiet output.
        '-PoutputPath=${fileDump.path}',
        'outputDebugAppLinkSettings',
      ], workingDirectory: androidApp.path);

      expect(result, const ProcessResultMatcher());
      expect(fileDump.existsSync(), true);

      final json = jsonDecode(fileDump.readAsStringSync()) as Map<String, dynamic>;
      expect(json['applicationId'], 'com.example.testapp');
      final deeplinks = json['deeplinks']! as List<dynamic>;
      expect(deeplinks.length, 1);

      final deeplink = deeplinks[0] as Map<String, dynamic>;
      expect(deeplink['scheme'], 'https');
      expect(deeplink['host'], 'example.com');
      expect(deeplink['path'], '.*');

      final intentFilterCheck = deeplink['intentFilterCheck'] as Map<String, dynamic>;
      expect(intentFilterCheck['hasAutoVerify'], true);
      expect(intentFilterCheck['hasActionView'], true);
      expect(intentFilterCheck['hasDefaultCategory'], true);
      expect(intentFilterCheck['hasBrowsableCategory'], true);
    },
  );
}
