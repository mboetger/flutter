// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';
import 'dart:io';

import 'package:file/file.dart';
import 'package:vm_service/vm_service.dart';
import 'package:vm_service/vm_service_io.dart';

import '../src/common.dart';
import 'test_data/hot_restart_ffi_blocked_project.dart';
import 'test_driver.dart';
import 'test_utils.dart';

void main() {
  late Directory tempDir;
  final project = HotRestartFfiBlockedTestProject();
  late FlutterRunTestDriver flutter;

  setUp(() async {
    tempDir = createResolvedTempDirectorySync('hot_restart_ffi_blocked_test.');
    await project.setUpIn(tempDir);
    flutter = FlutterRunTestDriver(tempDir);
  });

  tearDown(() async {
    await flutter.stop();
    tryToDelete(tempDir);
  });

  Future<bool> compileLibrary(Directory tempDir) async {
    final compiler = Platform.isMacOS ? 'clang++' : 'g++';
    final libName = Platform.isMacOS ? 'libblocking.dylib' : 'libblocking.so';
    final File cppFile = tempDir.childFile('blocking_library.cpp');
    await cppFile.writeAsString('''
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdint.h>

#ifdef _WIN32
#define DART_EXPORT __declspec(dllexport)
#else
#define DART_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {

std::mutex m_mutex;
std::condition_variable m_notified;
int m_current_queue = 0;

DART_EXPORT uint8_t _library_wait_for_callbacks(int32_t queue_id) {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_notified.wait(lock, [queue_id] { return m_current_queue != queue_id; });
    return 1;
}

}
''');

    try {
      final ProcessResult result = await Process.run(compiler, <String>[
        '-shared',
        '-fPIC',
        '-o',
        tempDir.childFile(libName).path,
        cppFile.path,
      ]);

      if (result.exitCode != 0) {
        // ignore: avoid_print
        print('Failed to compile library: ${result.stderr}');
        return false;
      }
      return true;
    } on Object catch (e) {
      // ignore: avoid_print
      print('Compiler not available: $e');
      return false;
    }
  }

  testWithoutContext('Hot restart succeeds even when an isolate is blocked in FFI', () async {
    if (!Platform.isLinux && !Platform.isMacOS) {
      // Skip on Windows for simplicity of compilation in this test
      return;
    }

    final bool compiled = await compileLibrary(tempDir);
    if (!compiled) {
      // If we can't compile (e.g. no compiler), skip the test
      return;
    }

    final Future<void> ffiCalled = flutter.stdout.firstWhere(
      (String line) => line.contains('Calling blocked FFI function'),
    );

    await flutter.run(withDebugger: true);

    // Connect to VM Service to resume the helper isolate which starts paused.
    final Uri vmServiceUri = flutter.vmServiceWsUri!;
    final VmService vmService = await vmServiceConnectUri(vmServiceUri.toString());
    final StreamSubscription<Event> debugSubscription = vmService.onDebugEvent.listen((
      Event event,
    ) async {
      if (event.kind == EventKind.kPauseStart) {
        try {
          await vmService.resume(event.isolate!.id!);
        } on Object {
          // Ignore errors if the connection is closing.
        }
      }
    });
    await vmService.streamListen(EventStreams.kDebug);

    await ffiCalled;

    // Perform hot restart. If the bug is present, this will hang.
    // We set a timeout to fail the test if it hangs.
    await flutter.hotRestart().timeout(
      const Duration(seconds: 15),
      onTimeout: () => throw TimeoutException('First hot restart hung!'),
    );

    // Wait for the app to restart and the helper isolate to block again.
    final Future<void> ffiCalledSecond = flutter.stdout.firstWhere(
      (String line) => line.contains('Calling blocked FFI function'),
    );
    await ffiCalledSecond;

    // Perform second hot restart.
    await flutter.hotRestart().timeout(
      const Duration(seconds: 15),
      onTimeout: () => throw TimeoutException('Second hot restart hung!'),
    );

    await debugSubscription.cancel();
    await vmService.dispose();
  }, skip: !Platform.isLinux && !Platform.isMacOS);
}
