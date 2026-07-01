// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/file.dart';
import 'package:file/memory.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/migrations/xcode_project_plugin_registrant_migration.dart';
import 'package:flutter_tools/src/xcode_project.dart';
import 'package:test/fake.dart';

import '../../src/common.dart';

void main() {
  group('XcodeProjectPluginRegistrantMigration', () {
    late MemoryFileSystem memoryFileSystem;
    late BufferLogger testLogger;
    late FakeXcodeBasedProject fakeProject;

    setUp(() {
      memoryFileSystem = MemoryFileSystem.test();
      testLogger = BufferLogger.test();
      fakeProject = FakeXcodeBasedProject(fileSystem: memoryFileSystem);
    });

    testWithoutContext('migrates iOS registrant paths', () async {
      const legacyProjectPbxproj = '''
		1498D2321E8E86230040F4C2 /* GeneratedPluginRegistrant.h */ = {isa = PBXFileReference; lastKnownFileType = sourcecode.c.h; path = GeneratedPluginRegistrant.h; sourceTree = "<group>"; };
		1498D2331E8E89220040F4C2 /* GeneratedPluginRegistrant.m */ = {isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = sourcecode.c.objc; path = GeneratedPluginRegistrant.m; sourceTree = "<group>"; };
		1498D2341E8E89220040F4C2 /* Unrelated.h */ = {isa = PBXFileReference; lastKnownFileType = sourcecode.c.h; path = Unrelated.h; sourceTree = "<group>"; };
''';
      const expectedProjectPbxproj = '''
		1498D2321E8E86230040F4C2 /* GeneratedPluginRegistrant.h */ = {isa = PBXFileReference; lastKnownFileType = sourcecode.c.h; path = ../.dart_tool/flutter_build/ios/GeneratedPluginRegistrant.h; sourceTree = SOURCE_ROOT; };
		1498D2331E8E89220040F4C2 /* GeneratedPluginRegistrant.m */ = {isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = sourcecode.c.objc; path = ../.dart_tool/flutter_build/ios/GeneratedPluginRegistrant.m; sourceTree = SOURCE_ROOT; };
		1498D2341E8E89220040F4C2 /* Unrelated.h */ = {isa = PBXFileReference; lastKnownFileType = sourcecode.c.h; path = Unrelated.h; sourceTree = "<group>"; };
''';
      fakeProject.xcodeProjectInfoFile.writeAsStringSync(legacyProjectPbxproj);
      final migration = XcodeProjectPluginRegistrantMigration(fakeProject, testLogger);

      await migration.migrate();

      expect(
        testLogger.statusText,
        contains('Updating GeneratedPluginRegistrant paths in Xcode project.'),
      );
      expect(fakeProject.xcodeProjectInfoFile.readAsStringSync(), expectedProjectPbxproj);
    });

    testWithoutContext('migrates macOS registrant paths', () async {
      const legacyProjectPbxproj = '''
		335BBD1A22A9A15E00E9071D /* GeneratedPluginRegistrant.swift */ = {isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = sourcecode.swift; path = GeneratedPluginRegistrant.swift; sourceTree = "<group>"; };
		335BBD1B22A9A15E00E9071D /* Unrelated.swift */ = {isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = sourcecode.swift; path = Unrelated.swift; sourceTree = "<group>"; };
''';
      const expectedProjectPbxproj = '''
		335BBD1A22A9A15E00E9071D /* GeneratedPluginRegistrant.swift */ = {isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = sourcecode.swift; path = ../.dart_tool/flutter_build/macos/GeneratedPluginRegistrant.swift; sourceTree = SOURCE_ROOT; };
		335BBD1B22A9A15E00E9071D /* Unrelated.swift */ = {isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = sourcecode.swift; path = Unrelated.swift; sourceTree = "<group>"; };
''';
      fakeProject.xcodeProjectInfoFile.writeAsStringSync(legacyProjectPbxproj);
      final migration = XcodeProjectPluginRegistrantMigration(fakeProject, testLogger);

      await migration.migrate();

      expect(
        testLogger.statusText,
        contains('Updating GeneratedPluginRegistrant paths in Xcode project.'),
      );
      expect(fakeProject.xcodeProjectInfoFile.readAsStringSync(), expectedProjectPbxproj);
    });

    testWithoutContext('migration is idempotent', () async {
      const migratedProjectPbxproj = '''
		1498D2321E8E86230040F4C2 /* GeneratedPluginRegistrant.h */ = {isa = PBXFileReference; lastKnownFileType = sourcecode.c.h; path = ../.dart_tool/flutter_build/ios/GeneratedPluginRegistrant.h; sourceTree = SOURCE_ROOT; };
		1498D2331E8E89220040F4C2 /* GeneratedPluginRegistrant.m */ = {isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = sourcecode.c.objc; path = ../.dart_tool/flutter_build/ios/GeneratedPluginRegistrant.m; sourceTree = SOURCE_ROOT; };
		335BBD1A22A9A15E00E9071D /* GeneratedPluginRegistrant.swift */ = {isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = sourcecode.swift; path = ../.dart_tool/flutter_build/macos/GeneratedPluginRegistrant.swift; sourceTree = SOURCE_ROOT; };
''';
      fakeProject.xcodeProjectInfoFile.writeAsStringSync(migratedProjectPbxproj);
      final migration = XcodeProjectPluginRegistrantMigration(fakeProject, testLogger);

      await migration.migrate();

      expect(testLogger.statusText, isEmpty);
      expect(fakeProject.xcodeProjectInfoFile.readAsStringSync(), migratedProjectPbxproj);
    });

    testWithoutContext('skips migration if project.pbxproj does not exist', () async {
      final migration = XcodeProjectPluginRegistrantMigration(fakeProject, testLogger);

      await migration.migrate();

      expect(testLogger.statusText, isEmpty);
      expect(
        testLogger.traceText,
        contains('Xcode project not found, skipping Xcode project plugin registrant migration.'),
      );
    });
  });
}

class FakeXcodeBasedProject extends Fake implements XcodeBasedProject {
  FakeXcodeBasedProject({required MemoryFileSystem fileSystem})
    : xcodeProjectInfoFile = fileSystem.file('project.pbxproj');

  @override
  File xcodeProjectInfoFile;
}
