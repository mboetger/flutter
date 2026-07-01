// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/android_sdk.dart';
import 'package:flutter_tools/src/android/android_studio.dart';
import 'package:flutter_tools/src/android/gradle.dart';
import 'package:flutter_tools/src/android/gradle_utils.dart';
import 'package:flutter_tools/src/android/java.dart';
import 'package:flutter_tools/src/artifacts.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/platform.dart';
import 'package:flutter_tools/src/base/version.dart';
import 'package:flutter_tools/src/build_info.dart';
import 'package:flutter_tools/src/cache.dart';
import 'package:flutter_tools/src/project.dart';
import 'package:test/fake.dart';
import 'package:unified_analytics/unified_analytics.dart';

import '../../src/common.dart';
import '../../src/context.dart';
import '../../src/fakes.dart';

const minimalV2EmbeddingManifest = r'''
<manifest xmlns:android="http://schemas.android.com/apk/res/android">
    <application
        android:name="${applicationName}">
        <meta-data
            android:name="flutterEmbedding"
            android:value="2" />
    </application>
</manifest>
''';

void main() {
  group('Android Studio Gradle Service Directory', () {
    late BufferLogger logger;
    late FakeAnalytics fakeAnalytics;
    late MemoryFileSystem fileSystem;
    late FakeProcessManager processManager;
    late Platform platform;

    setUp(() {
      processManager = FakeProcessManager.empty();
      logger = BufferLogger.test();
      fileSystem = MemoryFileSystem.test();
      Cache.flutterRoot = '';
      fakeAnalytics = getInitializedFakeAnalyticsInstance(
        fs: fileSystem,
        fakeFlutterVersion: FakeFlutterVersion(),
      );
      platform = FakePlatform(environment: <String, String>{'HOME': '/home/me'});
    });

    testUsingContext(
      'Flutter build honors Android Studio Gradle service directory setting',
      () async {
        // 1. Set up a fake Android Studio installation on Linux.
        const homeLinux = '/home/me';
        const studioHomeFilePath = '$homeLinux/.cache/Google/AndroidStudio4.1/.home';
        const studioInstallPath = '$homeLinux/AndroidStudio';

        fileSystem.file(studioHomeFilePath)
          ..createSync(recursive: true)
          ..writeAsStringSync(studioInstallPath);
        fileSystem.directory(studioInstallPath).createSync(recursive: true);

        // 2. Set up the gradle.settings.xml with a custom service directory path.
        const customGradleServiceDir = '/custom/gradle/service/directory';
        fileSystem.file('$homeLinux/.config/Google/AndroidStudio4.1/options/gradle.settings.xml')
          ..createSync(recursive: true)
          ..writeAsStringSync('''
<?xml version="1.0" encoding="UTF-8"?>
<application>
  <component name="GradleSystemSettings">
    <option name="serviceDirectoryPath" value="$customGradleServiceDir" />
  </component>
</application>
''');

        // 3. Set up a minimal Flutter project.
        fileSystem.directory('android').childFile('build.gradle').createSync(recursive: true);
        fileSystem.directory('android').childFile('gradle.properties').createSync(recursive: true);
        fileSystem.directory('android').childDirectory('app').childFile('build.gradle')
          ..createSync(recursive: true)
          ..writeAsStringSync('apply from: irrelevant/flutter.gradle');

        final FlutterProject project = FlutterProject.fromDirectoryTest(
          fileSystem.currentDirectory,
        );
        project.android.appManifestFile
          ..createSync(recursive: true)
          ..writeAsStringSync(minimalV2EmbeddingManifest);

        // 4. Setup process manager expectations.
        processManager.addCommand(
          const FakeCommand(
            command: <String>['/home/me/AndroidStudio/jre/bin/java', '-version'],
            stderr:
                'openjdk version "1.8.0_242"\nOpenJDK Runtime Environment (build 1.8.0_242-b08)',
          ),
        );
        processManager.addCommand(
          FakeCommand(
            command: const <String>[
              'gradlew',
              '-q',
              '-Ptarget-platform=android-arm,android-arm64,android-x64',
              '-Ptarget=lib/main.dart',
              '-Pbase-application-name=android.app.Application',
              '-Pdart-obfuscation=false',
              '-Ptrack-widget-creation=false',
              '-Ptree-shake-icons=false',
              'assembleRelease',
            ],
            environment: const <String, String>{
              'JAVA_HOME': '/android-studio/jbr',
              'PATH': '/android-studio/jbr/bin',
              'GRADLE_USER_HOME': '/custom/gradle/service/directory',
              'FLUTTER_ALREADY_LOCKED': 'true',
            },
            onRun: (_) {
              fileSystem.file('build/app/outputs/flutter-apk/app-release.apk')
                ..createSync(recursive: true)
                ..writeAsStringSync('fake apk');
            },
          ),
        );

        // 5. Detect Android Studio.
        final AndroidStudio? androidStudio = AndroidStudio.latestValid();
        expect(androidStudio, isNotNull);
        expect(androidStudio!.version, equals(Version(4, 1, null)));

        // 6. Run the Gradle builder.
        final builder = AndroidGradleBuilder(
          java: FakeJava(),
          logger: logger,
          processManager: processManager,
          fileSystem: fileSystem,
          artifacts: Artifacts.test(),
          analytics: fakeAnalytics,
          gradleUtils: FakeGradleUtils(),
          platform: platform,
          androidStudio: androidStudio,
        );

        await builder.buildApk(
          project: project,
          androidBuildInfo: const AndroidBuildInfo(
            BuildInfo(
              BuildMode.release,
              null,
              treeShakeIcons: false,
              packageConfigPath: '.dart_tool/package_config.json',
            ),
          ),
          target: 'lib/main.dart',
        );
      },
      overrides: <Type, Generator>{
        FileSystem: () => fileSystem,
        ProcessManager: () => processManager,
        Platform: () => platform,
        Java: () => FakeJava(),
        AndroidSdk: () => LocalFakeAndroidSdk(fileSystem.directory('/sdk')),
      },
    );

    testUsingContext(
      'Flutter build AAR honors Android Studio Gradle service directory setting',
      () async {
        // 1. Set up a fake Android Studio installation on Linux.
        const homeLinux = '/home/me';
        const studioHomeFilePath = '$homeLinux/.cache/Google/AndroidStudio4.1/.home';
        const studioInstallPath = '$homeLinux/AndroidStudio';

        fileSystem.file(studioHomeFilePath)
          ..createSync(recursive: true)
          ..writeAsStringSync(studioInstallPath);
        fileSystem.directory(studioInstallPath).createSync(recursive: true);

        // 2. Set up the gradle.settings.xml with a custom service directory path.
        const customGradleServiceDir = '/custom/gradle/service/directory';
        fileSystem.file('$homeLinux/.config/Google/AndroidStudio4.1/options/gradle.settings.xml')
          ..createSync(recursive: true)
          ..writeAsStringSync('''
<?xml version="1.0" encoding="UTF-8"?>
<application>
  <component name="GradleSystemSettings">
    <option name="serviceDirectoryPath" value="$customGradleServiceDir" />
  </component>
</application>
''');

        // 3. Set up a minimal Flutter module project.
        fileSystem.file('pubspec.yaml').writeAsStringSync('''
name: test_module
flutter:
  module:
    androidPackage: com.example.test_module
''');
        fileSystem.directory('.android').childFile('gradlew').createSync(recursive: true);
        fileSystem.directory('.android').childFile('gradle.properties').createSync(recursive: true);
        fileSystem.directory('.android').childFile('build.gradle').createSync(recursive: true);
        fileSystem.directory('build/outputs/repo').createSync(recursive: true);

        final FlutterProject project = FlutterProject.fromDirectoryTest(
          fileSystem.currentDirectory,
        );
        expect(project.isModule, isTrue);

        // 4. Setup process manager expectations.
        processManager.addCommand(
          const FakeCommand(
            command: <String>['/home/me/AndroidStudio/jre/bin/java', '-version'],
            stderr:
                'openjdk version "1.8.0_242"\nOpenJDK Runtime Environment (build 1.8.0_242-b08)',
          ),
        );
        processManager.addCommand(
          FakeCommand(
            command: const <String>[
              'gradlew',
              '-I=/packages/flutter_tools/gradle/aar_init_script.gradle',
              '-Pflutter-root=/',
              '-Poutput-dir=/build/host',
              '-Pis-plugin=false',
              '-PbuildNumber=1.0',
              '-q',
              '-Pdart-obfuscation=false',
              '-Ptrack-widget-creation=false',
              '-Ptree-shake-icons=false',
              '-Ptarget-platform=android-arm,android-arm64,android-x64',
              'assembleAarRelease',
            ],
            environment: const <String, String>{
              'JAVA_HOME': '/android-studio/jbr',
              'PATH': '/android-studio/jbr/bin',
              'GRADLE_USER_HOME': '/custom/gradle/service/directory',
              'FLUTTER_ALREADY_LOCKED': 'true',
            },
            onRun: (_) {
              fileSystem.directory('build/host/outputs/repo').createSync(recursive: true);
            },
          ),
        );

        // 5. Detect Android Studio.
        final AndroidStudio? androidStudio = AndroidStudio.latestValid();
        expect(androidStudio, isNotNull);

        // 6. Run the Gradle builder.
        final builder = AndroidGradleBuilder(
          java: FakeJava(),
          logger: logger,
          processManager: processManager,
          fileSystem: fileSystem,
          artifacts: Artifacts.test(),
          analytics: fakeAnalytics,
          gradleUtils: FakeGradleUtils(),
          platform: platform,
          androidStudio: androidStudio,
        );

        await builder.buildAar(
          project: project,
          androidBuildInfo: <AndroidBuildInfo>{
            const AndroidBuildInfo(
              BuildInfo(
                BuildMode.release,
                null,
                treeShakeIcons: false,
                packageConfigPath: '.dart_tool/package_config.json',
              ),
            ),
          },
          target: '',
          buildNumber: '1.0',
          generateTooling: (_, {required bool releaseMode}) async {},
        );
      },
      overrides: <Type, Generator>{
        FileSystem: () => fileSystem,
        ProcessManager: () => processManager,
        Platform: () => platform,
        Java: () => FakeJava(),
        AndroidSdk: () => LocalFakeAndroidSdk(fileSystem.directory('/sdk')),
      },
    );
  });
}

class FakeGradleUtils extends Fake implements GradleUtils {
  @override
  String getExecutable(FlutterProject project) {
    return 'gradlew';
  }
}

class LocalFakeAndroidSdk extends FakeAndroidSdk {
  LocalFakeAndroidSdk(this._directory);

  final Directory _directory;

  @override
  Directory get directory => _directory;
}
