// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/file.dart';
import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/migrations/android_embedding_migration.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/project.dart';
import 'package:test/test.dart';

import '../../../src/common.dart';
import '../../../src/context.dart';

void main() {
  group('Android embedding migration', () {
    late MemoryFileSystem fs;
    late BufferLogger logger;
    late Directory projectDir;
    late FlutterProject project;

    setUp(() {
      fs = MemoryFileSystem.test();
      logger = BufferLogger.test();
      projectDir = fs.directory('/project');
      projectDir.childFile('pubspec.yaml').createSync(recursive: true);
      projectDir.childDirectory('android').childFile('build.gradle').createSync(recursive: true);
      project = FlutterProject.fromDirectoryTest(projectDir);
    });

    testUsingContext(
      'identifies and migrates V1 Java project to V2',
      () async {
        final Directory androidDir = projectDir.childDirectory('android');
        final File manifestFile = androidDir
            .childDirectory('app')
            .childDirectory('src')
            .childDirectory('main')
            .childFile('AndroidManifest.xml');
        manifestFile.createSync(recursive: true);
        manifestFile.writeAsStringSync('''
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.example.my_app">
    <application
        android:name="io.flutter.app.FlutterApplication"
        android:label="my_app">
        <activity
            android:name=".MainActivity">
        </activity>
    </application>
</manifest>
''');

        final File mainActivityFile = androidDir
            .childDirectory('app')
            .childDirectory('src')
            .childDirectory('main')
            .childDirectory('java')
            .childDirectory('com')
            .childDirectory('example')
            .childDirectory('my_app')
            .childFile('MainActivity.java');
        mainActivityFile.createSync(recursive: true);
        mainActivityFile.writeAsStringSync('''
package com.example.my_app;

import android.os.Bundle;
import io.flutter.app.FlutterActivity;
import io.flutter.plugins.GeneratedPluginRegistrant;

public class MainActivity extends FlutterActivity {
  @override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    GeneratedPluginRegistrant.registerWith(this);
  }
}
''');

        expect(project.android.computeEmbeddingVersion().version, AndroidEmbeddingVersion.v1);

        final migration = AndroidEmbeddingMigration(project.android, logger);
        await migration.migrate();

        expect(project.android.computeEmbeddingVersion().version, AndroidEmbeddingVersion.v2);

        final String manifestContent = manifestFile.readAsStringSync();
        expect(manifestContent, contains('flutterEmbedding'));
        expect(manifestContent, contains('android:value="2"'));
        expect(manifestContent, isNot(contains('io.flutter.app.FlutterApplication')));

        final String mainActivityContent = mainActivityFile.readAsStringSync();
        expect(mainActivityContent, '''
package com.example.my_app;

import android.os.Bundle;
import io.flutter.embedding.android.FlutterActivity;

public class MainActivity extends FlutterActivity {
  @override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
  }
}
''');
      },
      overrides: <Type, Generator>{
        FileSystem: () => fs,
        ProcessManager: () => FakeProcessManager.any(),
      },
    );

    testUsingContext(
      'identifies and migrates V1 Kotlin project to V2',
      () async {
        final Directory androidDir = projectDir.childDirectory('android');
        final File manifestFile = androidDir
            .childDirectory('app')
            .childDirectory('src')
            .childDirectory('main')
            .childFile('AndroidManifest.xml');
        manifestFile.createSync(recursive: true);
        manifestFile.writeAsStringSync('''
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.example.my_app">
    <application
        android:name="io.flutter.app.FlutterApplication"
        android:label="my_app">
        <activity
            android:name=".MainActivity">
        </activity>
    </application>
</manifest>
''');

        final File mainActivityFile = androidDir
            .childDirectory('app')
            .childDirectory('src')
            .childDirectory('main')
            .childDirectory('kotlin')
            .childDirectory('com')
            .childDirectory('example')
            .childDirectory('my_app')
            .childFile('MainActivity.kt');
        mainActivityFile.createSync(recursive: true);
        mainActivityFile.writeAsStringSync('''
package com.example.my_app

import android.os.Bundle
import io.flutter.app.FlutterActivity
import io.flutter.plugins.GeneratedPluginRegistrant

class MainActivity: FlutterActivity() {
  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    GeneratedPluginRegistrant.registerWith(this)
  }
}
''');

        expect(project.android.computeEmbeddingVersion().version, AndroidEmbeddingVersion.v1);

        final migration = AndroidEmbeddingMigration(project.android, logger);
        await migration.migrate();

        expect(project.android.computeEmbeddingVersion().version, AndroidEmbeddingVersion.v2);

        final String manifestContent = manifestFile.readAsStringSync();
        expect(manifestContent, contains('flutterEmbedding'));
        expect(manifestContent, contains('android:value="2"'));
        expect(manifestContent, isNot(contains('io.flutter.app.FlutterApplication')));

        final String mainActivityContent = mainActivityFile.readAsStringSync();
        expect(mainActivityContent, '''
package com.example.my_app

import android.os.Bundle
import io.flutter.embedding.android.FlutterActivity

class MainActivity: FlutterActivity() {
  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
  }
}
''');
      },
      overrides: <Type, Generator>{
        FileSystem: () => fs,
        ProcessManager: () => FakeProcessManager.any(),
      },
    );

    testUsingContext(
      'does not modify already V2 project',
      () async {
        final Directory androidDir = projectDir.childDirectory('android');
        final File manifestFile = androidDir
            .childDirectory('app')
            .childDirectory('src')
            .childDirectory('main')
            .childFile('AndroidManifest.xml');
        manifestFile.createSync(recursive: true);
        const manifestContent = '''
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.example.my_app">
    <application
        android:label="my_app">
        <meta-data
            android:name="flutterEmbedding"
            android:value="2" />
        <activity
            android:name=".MainActivity">
        </activity>
    </application>
</manifest>
''';
        manifestFile.writeAsStringSync(manifestContent);

        final File mainActivityFile = androidDir
            .childDirectory('app')
            .childDirectory('src')
            .childDirectory('main')
            .childDirectory('java')
            .childDirectory('com')
            .childDirectory('example')
            .childDirectory('my_app')
            .childFile('MainActivity.java');
        mainActivityFile.createSync(recursive: true);
        const mainActivityContent = '''
package com.example.my_app;

import io.flutter.embedding.android.FlutterActivity;

public class MainActivity extends FlutterActivity {
}
''';
        mainActivityFile.writeAsStringSync(mainActivityContent);

        expect(project.android.computeEmbeddingVersion().version, AndroidEmbeddingVersion.v2);

        final migration = AndroidEmbeddingMigration(project.android, logger);
        await migration.migrate();

        // Should remain V2 and files should be unchanged
        expect(project.android.computeEmbeddingVersion().version, AndroidEmbeddingVersion.v2);
        expect(manifestFile.readAsStringSync(), manifestContent);
        expect(mainActivityFile.readAsStringSync(), mainActivityContent);
      },
      overrides: <Type, Generator>{
        FileSystem: () => fs,
        ProcessManager: () => FakeProcessManager.any(),
      },
    );

    testUsingContext(
      'skips gracefully if files are missing',
      () async {
        final migration = AndroidEmbeddingMigration(project.android, logger);
        await migration.migrate();

        // Should not crash, and should remain V1 (since no manifest means V1 by default in computeEmbeddingVersion)
        expect(project.android.computeEmbeddingVersion().version, AndroidEmbeddingVersion.v1);
      },
      overrides: <Type, Generator>{
        FileSystem: () => fs,
        ProcessManager: () => FakeProcessManager.any(),
      },
    );

    testUsingContext(
      'handles malformed XML gracefully',
      () async {
        final Directory androidDir = projectDir.childDirectory('android');
        final File manifestFile = androidDir
            .childDirectory('app')
            .childDirectory('src')
            .childDirectory('main')
            .childFile('AndroidManifest.xml');
        manifestFile.createSync(recursive: true);
        manifestFile.writeAsStringSync('malformed xml');

        final migration = AndroidEmbeddingMigration(project.android, logger);

        await expectLater(migration.migrate(), completes);
      },
      overrides: <Type, Generator>{
        FileSystem: () => fs,
        ProcessManager: () => FakeProcessManager.any(),
      },
    );
  });
}
