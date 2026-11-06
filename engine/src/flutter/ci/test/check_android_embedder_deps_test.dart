// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:io';

import 'package:path/path.dart' as path;
import 'package:test/test.dart';

import '../bin/check_android_embedder_deps.dart' as ratchet;

void main() {
  late Directory tempDir;
  late Directory androidDir;
  late File allowedFile;
  late File buildGnFile;

  setUp(() {
    tempDir = Directory.systemTemp.createTempSync('check_android_embedder_deps_test_');
    androidDir = Directory(path.join(tempDir.path, 'android'))..createSync();
    allowedFile = File(path.join(androidDir.path, 'allowed_internal_headers.yaml'));
    buildGnFile = File(path.join(androidDir.path, 'BUILD.gn'));
  });

  tearDown(() {
    if (tempDir.existsSync()) {
      tempDir.deleteSync(recursive: true);
    }
  });

  test('passes when zero-ratchet baseline and GN firewall are clean', () {
    allowedFile.writeAsStringSync('''
# Comment header
{
  "allowed_legacy_files": {},
  "allowed_unique_headers": [],
  "governing_adr": "ADR-0010",
  "rfc": "RFC-410.0000",
  "total_allowed_internal_headers": 0
}
''');
    buildGnFile.writeAsStringSync('''
source_set("flutter_embedder_native_src") {
  sources = [
    "flutter_embedder_native.cc",
    "flutter_embedder_native.h",
  ]
  check_includes = true
  defines = [ "FLUTTER_ENGINE_NO_PROTOTYPES" ]
  deps = [
    "//flutter/common",
    "//flutter/fml",
    "//flutter/shell/platform/embedder:embedder_headers",
  ]
}
''');
    File(path.join(androidDir.path, 'flutter_embedder_native.cc')).writeAsStringSync('''
#include "flutter/shell/platform/android/flutter_embedder_native.h"
#include "flutter/shell/platform/embedder/embedder.h"
#include "flutter/fml/logging.h"
''');

    final out = StringBuffer();
    final err = StringBuffer();
    final int code = ratchet.runCheck(
      <String>[
        '--allowed-file=${allowedFile.path}',
        '--android-dir=${androidDir.path}',
        '--build-gn=${buildGnFile.path}',
      ],
      outSink: out,
      errSink: err,
    );
    expect(code, 0, reason: err.toString());
    expect(out.toString(), contains('PASSED'));
  });

  test('fails when a file in flutter_embedder_native_src includes shell/common/shell.h', () {
    allowedFile.writeAsStringSync('''
{
  "allowed_legacy_files": {},
  "allowed_unique_headers": [],
  "governing_adr": "ADR-0010",
  "rfc": "RFC-410.0000",
  "total_allowed_internal_headers": 0
}
''');
    buildGnFile.writeAsStringSync('''
source_set("flutter_embedder_native_src") {
  sources = [
    "flutter_embedder_native.cc",
  ]
  check_includes = true
  defines = [ "FLUTTER_ENGINE_NO_PROTOTYPES" ]
  deps = [
    "//flutter/common",
    "//flutter/fml",
    "//flutter/shell/platform/embedder:embedder_headers",
  ]
}
''');
    File(path.join(androidDir.path, 'flutter_embedder_native.cc')).writeAsStringSync('''
#include "flutter/shell/common/shell.h"
''');

    final err = StringBuffer();
    final int code = ratchet.runCheck(
      <String>[
        '--allowed-file=${allowedFile.path}',
        '--android-dir=${androidDir.path}',
        '--build-gn=${buildGnFile.path}',
      ],
      outSink: StringBuffer(),
      errSink: err,
    );
    expect(code, 1);
    expect(err.toString(), contains('flutter/shell/common/shell.h'));
  });

  test(
    'fails when BUILD.gn firewall is missing FLUTTER_ENGINE_NO_PROTOTYPES or adds forbidden dep',
    () {
      buildGnFile.writeAsStringSync('''
source_set("flutter_embedder_native_src") {
  sources = [ "jni_router.cc" ]
  check_includes = true
  defines = []
  deps = [
    "//flutter/common",
    "//flutter/fml",
    "//flutter/shell/common",
    "//flutter/shell/platform/embedder:embedder_headers",
  ]
}
''');
      final List<String> errors = ratchet.verifyBuildGnFirewall(buildGnFile);
      expect(
        errors,
        contains(predicate<String>((String s) => s.contains('FLUTTER_ENGINE_NO_PROTOTYPES'))),
      );
      expect(
        errors,
        contains(predicate<String>((String s) => s.contains('//flutter/shell/common'))),
      );
    },
  );

  test(
    'supports transitional baseline generation and monotonically enforces per-file allowlist',
    () {
      File(path.join(androidDir.path, 'legacy_view.cc')).writeAsStringSync('''
#include "flutter/shell/common/shell.h"
#include "flutter/fml/logging.h"
''');
      buildGnFile.writeAsStringSync('# transitional BUILD.gn without native_src yet\n');

      final int genCode = ratchet.runCheck(
        <String>[
          '--generate-baseline',
          '--allowed-file=${allowedFile.path}',
          '--android-dir=${androidDir.path}',
          '--build-gn=${buildGnFile.path}',
        ],
        outSink: StringBuffer(),
        errSink: StringBuffer(),
      );
      expect(genCode, 0);

      final int checkCode = ratchet.runCheck(
        <String>[
          '--allowed-file=${allowedFile.path}',
          '--android-dir=${androidDir.path}',
          '--build-gn=${buildGnFile.path}',
        ],
        outSink: StringBuffer(),
        errSink: StringBuffer(),
      );
      expect(checkCode, 0);

      // Adding an unallowed internal header to a new file must fail.
      File(path.join(androidDir.path, 'new_file.cc')).writeAsStringSync('''
#include "impeller/renderer/renderer.h"
''');
      final err = StringBuffer();
      final int failCode = ratchet.runCheck(
        <String>[
          '--allowed-file=${allowedFile.path}',
          '--android-dir=${androidDir.path}',
          '--build-gn=${buildGnFile.path}',
        ],
        outSink: StringBuffer(),
        errSink: err,
      );
      expect(failCode, 1);
      expect(err.toString(), contains('new_file.cc'));
    },
  );
}
