// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/file.dart';
import 'package:file/memory.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/bazel/bep.dart';

import '../../src/common.dart';

void main() {
  group('BepParser', () {
    late BufferLogger logger;
    late BepParser parser;
    late MemoryFileSystem fileSystem;

    setUp(() {
      fileSystem = MemoryFileSystem.test();
      logger = BufferLogger.test();
      parser = BepParser(logger: logger);
    });

    testWithoutContext('parses progress events', () async {
      const progressEvent = '{"progress": {"stdout": "Compiling //foo:bar"}}';
      final File file = fileSystem.file('bep.json')..writeAsStringSync(progressEvent);

      await parser.parseFile(file);
      expect(logger.statusText, contains('Compiling //foo:bar'));
    });

    testWithoutContext('parses stderr progress events', () async {
      const stderrEvent = '{"progress": {"stderr": "Warning: unused variable"}}';
      final File file = fileSystem.file('bep.json')..writeAsStringSync(stderrEvent);

      await parser.parseFile(file);
      expect(logger.errorText, contains('Warning: unused variable'));
    });

    testWithoutContext('handles invalid json gracefully', () async {
      const invalidJson = '{invalid_json}';
      final File file = fileSystem.file('bep.json')..writeAsStringSync(invalidJson);

      // Should not throw
      await parser.parseFile(file);
      expect(logger.statusText, isEmpty);
      expect(logger.errorText, isEmpty);
    });
  });
}
