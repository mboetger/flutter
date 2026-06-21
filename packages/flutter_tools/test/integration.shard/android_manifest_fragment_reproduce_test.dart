// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/file.dart';
import 'package:flutter_tools/src/android/gradle_utils.dart' show getGradlewFileName;
import 'package:flutter_tools/src/base/io.dart';

import '../src/common.dart';
import 'test_utils.dart';

void main() {
  late Directory tempDir;
  late File sdkManifestFragment;
  String? originalManifestContent;
  var existedBefore = false;

  setUp(() async {
    tempDir = createResolvedTempDirectorySync('android_manifest_fragment_test.');

    // Define the path for the manifest fragment in the Flutter SDK.
    final String sdkManifestPath = fileSystem.path.join(
      getFlutterRoot(),
      'packages',
      'flutter_tools',
      'gradle',
      'AndroidManifest.xml',
    );
    sdkManifestFragment = fileSystem.file(sdkManifestPath);

    // Backup any existing manifest fragment to avoid repository pollution or deletion.
    existedBefore = sdkManifestFragment.existsSync();
    if (existedBefore) {
      originalManifestContent = sdkManifestFragment.readAsStringSync();
    }
  });

  tearDown(() async {
    tryToDelete(tempDir);
    // Restore the original state of the SDK.
    if (existedBefore) {
      if (originalManifestContent != null) {
        sdkManifestFragment.writeAsStringSync(originalManifestContent!, flush: true);
      }
    } else {
      if (sdkManifestFragment.existsSync()) {
        sdkManifestFragment.deleteSync();
      }
    }
  });

  testWithoutContext('Android build merges manifest fragments supplied from the Flutter SDK', () async {
    // 1. Write the manifest fragment into the Flutter SDK.
    const fragmentContent = '''
<manifest xmlns:android="http://schemas.android.com/apk/res/android">
    <uses-permission android:name="flutter.reproduce.manifest.fragment.TEST_PERMISSION" />
</manifest>
''';
    sdkManifestFragment.createSync(recursive: true);
    sdkManifestFragment.writeAsStringSync(fragmentContent, flush: true);

    // 2. Create a new Flutter project.
    ProcessResult result = await processManager.run(<String>[
      flutterBin,
      'create',
      tempDir.path,
      '--project-name=testapp',
    ], workingDirectory: tempDir.path);
    expect(result.exitCode, 0, reason: 'flutter create failed: ${result.stderr}\n${result.stdout}');

    // 3. Configure the project so Gradle files are generated.
    result = await processManager.run(<String>[
      flutterBin,
      'build',
      'apk',
      '--config-only',
    ], workingDirectory: tempDir.path);
    expect(
      result.exitCode,
      0,
      reason: 'flutter build apk --config-only failed: ${result.stderr}\n${result.stdout}',
    );

    // 4. Run the manifest merging Gradle task.
    final Directory androidAppDir = tempDir.childDirectory('android');
    result = await processManager.run(<String>[
      '.${platform.pathSeparator}${getGradlewFileName(platform)}',
      ...getLocalEngineArguments(),
      '-q', // quiet output to avoid log pollution.
      'processDebugMainManifest',
    ], workingDirectory: androidAppDir.path);
    expect(
      result.exitCode,
      0,
      reason: 'Gradle processDebugMainManifest failed: ${result.stderr}\n${result.stdout}',
    );

    // 5. Find the merged manifest in the build output.
    final Directory intermediatesDir = tempDir
        .childDirectory('build')
        .childDirectory('app')
        .childDirectory('intermediates');

    expect(intermediatesDir.existsSync(), true, reason: 'intermediates directory does not exist');

    final List<File> mergedManifestFiles = intermediatesDir
        .listSync(recursive: true)
        .whereType<File>()
        .where((File file) => file.basename == 'AndroidManifest.xml')
        .toList();

    expect(
      mergedManifestFiles,
      isNotEmpty,
      reason: 'Could not find any merged AndroidManifest.xml',
    );

    // 6. Verify that the merged manifest contains our unique permission.
    var foundPermission = false;
    for (final manifestFile in mergedManifestFiles) {
      final String content = manifestFile.readAsStringSync();
      if (content.contains('flutter.reproduce.manifest.fragment.TEST_PERMISSION')) {
        foundPermission = true;
        break;
      }
    }

    expect(
      foundPermission,
      true,
      reason:
          'The merged manifest does not contain the permission supplied by the Flutter SDK manifest fragment. '
          'This indicates the Flutter SDK lacks a mechanism to supply Android manifest fragments to the app build.',
    );
  });
}
