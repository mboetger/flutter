// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:io';

import 'package:test/test.dart';

void main() {
  test('Clang revision from DEPS does not fail to link compressed sections', () async {
    // Only run on Linux and macOS where we can download the Fuchsia Clang toolchain.
    if (!Platform.isLinux && !Platform.isMacOS) {
      return;
    }

    // Check if host compiler (gcc or clang) is available.
    final bool hasGcc = _hasExecutable('gcc');
    final bool hasClang = _hasExecutable('clang');
    if (!hasGcc && !hasClang) {
      fail('Test requires either gcc or clang to be installed on the host.');
    }

    final Directory tempDir = Directory.systemTemp.createTempSync('repro_53782_');
    try {
      final platformName = Platform.isLinux ? 'linux-amd64' : 'mac-amd64';
      final package = 'fuchsia/third_party/clang/$platformName';
      final String version = _getClangVersionFromDeps();

      // 1. Download the buggy clang toolchain using CIPD.
      final ensureFile = File('${tempDir.path}/clang.ensure');
      ensureFile.writeAsStringSync('$package $version\n');

      final clangDir = Directory('${tempDir.path}/clang');
      clangDir.createSync();

      final ProcessResult cipdResult = Process.runSync('cipd', <String>[
        'ensure',
        '-root',
        clangDir.path,
        '-ensure-file',
        ensureFile.path,
      ]);
      expect(
        cipdResult.exitCode,
        0,
        reason: 'Failed to download clang via CIPD: ${cipdResult.stderr}',
      );

      // 2. Create a dummy C file.
      final cFile = File('${tempDir.path}/test.c');
      cFile.writeAsStringSync('int main() { return 0; }\n');

      // 3. Compile the C file with compressed debug sections.
      final objFile = File('${tempDir.path}/test.o');
      ProcessResult compileResult;
      if (hasGcc) {
        compileResult = Process.runSync('gcc', <String>[
          '-c',
          '-gz=zlib',
          '-g',
          '-o',
          objFile.path,
          cFile.path,
        ]);
      } else {
        compileResult = Process.runSync('clang', <String>[
          '-c',
          '-gz=zlib',
          '-g',
          '-o',
          objFile.path,
          cFile.path,
        ]);
      }

      if (compileResult.exitCode != 0) {
        fail(
          'Failed to compile dummy program with compressed debug sections. '
          'Host compiler might not support -gz=zlib. Error: ${compileResult.stderr}',
        );
      }

      // 4. Try to link it using the downloaded buggy lld.
      final lldPath = '${clangDir.path}/bin/ld.lld';
      final ProcessResult linkResult = Process.runSync(lldPath, <String>[
        objFile.path,
        '-o',
        '${tempDir.path}/test.out',
      ]);

      // 5. Verify it succeeds. If it fails with the specific zlib error, we have reproduced the bug.
      if (linkResult.exitCode != 0) {
        final String output = linkResult.stderr.toString() + linkResult.stdout.toString();
        if (output.contains('contains a compressed section, but zlib is not available')) {
          fail('BUG REPRODUCED: Linker failed with the expected error:\n$output');
        }
      }
      expect(
        linkResult.exitCode,
        0,
        reason: 'Linker failed with unexpected error:\n${linkResult.stderr}',
      );
    } finally {
      try {
        tempDir.deleteSync(recursive: true);
      } on Exception catch (_) {
        // Ignore cleanup errors.
      }
    }
  });
}

bool _hasExecutable(String name) {
  try {
    final ProcessResult result = Process.runSync('which', <String>[name]);
    return result.exitCode == 0;
  } on Exception catch (_) {
    return false;
  }
}

String _getClangVersionFromDeps() {
  Directory dir = Directory.current;
  File? depsFile;
  while (dir.path != dir.parent.path) {
    final file = File('${dir.path}/DEPS');
    if (file.existsSync()) {
      depsFile = file;
      break;
    }
    dir = dir.parent;
  }
  if (depsFile == null) {
    throw StateError('Could not find DEPS file starting from ${Directory.current.path}');
  }
  final String content = depsFile.readAsStringSync();
  final regExp = RegExp(r"'clang_version':\s*'([^']+)'");
  final Match? match = regExp.firstMatch(content);
  if (match == null) {
    throw StateError('Could not find clang_version in DEPS');
  }
  return match.group(1)!;
}
