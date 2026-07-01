// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/gradle_errors.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/project.dart';
import 'package:test/test.dart';

import '../../src/common.dart';
import '../../src/context.dart';
import '../../src/fake_process_manager.dart';
import '../../src/fakes.dart';

void main() {
  late FileSystem fileSystem;

  setUp(() {
    fileSystem = MemoryFileSystem.test();
  });

  group('d8 invoke-customs error', () {
    const String errorLine = 'Message{kind=ERROR, text=Invoke-customs are only supported starting with Android O (--min-api 26), sources=[Unknown source file], tool name=Optional.of(D8)}';
    const String errorLineShort = 'Invoke-customs are only supported starting with Android O (--min-api 26)';

    testWithoutContext('pattern matches', () {
      final List<GradleHandledError> handlers = gradleErrors.where((GradleHandledError error) => error.test(errorLine)).toList();
      expect(handlers, isNotEmpty, reason: 'No handler matched the D8 invoke-customs error line.');
      expect(handlers.length, 1);
      
      final List<GradleHandledError> handlersShort = gradleErrors.where((GradleHandledError error) => error.test(errorLineShort)).toList();
      expect(handlersShort, isNotEmpty, reason: 'No handler matched the short D8 invoke-customs error line.');
    });

    testUsingContext('handler output suggests JavaVersion.VERSION_1_8', () async {
      final List<GradleHandledError> handlers = gradleErrors.where((GradleHandledError error) => error.test(errorLine)).toList();
      expect(handlers, isNotEmpty);
      final GradleHandledError handler = handlers.first;

      final GradleBuildStatus status = await handler.handler(
        line: errorLine,
        project: FlutterProject.fromDirectoryTest(fileSystem.currentDirectory),
        usesAndroidX: true,
      );

      expect(status, GradleBuildStatus.exit);
      expect(
        testLogger.statusText,
        contains('JavaVersion.VERSION_1_8'),
      );
      expect(
        testLogger.statusText,
        contains('compileOptions'),
      );
      expect(
        testLogger.statusText,
        contains('sourceCompatibility'),
      );
      expect(
        testLogger.statusText,
        contains('targetCompatibility'),
      );
    }, overrides: <Type, Generator>{
      FileSystem: () => fileSystem,
      ProcessManager: () => FakeProcessManager.empty(),
    });
  });
}
