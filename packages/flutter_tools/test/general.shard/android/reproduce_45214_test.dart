// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';

import 'package:flutter_tools/src/android/android_device.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/vmservice.dart';
import 'package:test/fake.dart';
import 'package:vm_service/vm_service.dart';

import '../../src/common.dart';
import '../../src/fake_process_manager.dart';

const String kDummyLine = 'Contents are not important\n';

class _FakeVm extends Fake implements VM {
  _FakeVm(this._appPid);

  final int _appPid;

  @override
  int? get pid => _appPid;
}

class _FakeFlutterVmService extends Fake implements FlutterVmService {
  _FakeFlutterVmService(this._appPid);

  final int _appPid;

  @override
  Future<VM?> getVmGuarded() async => _FakeVm(_appPid);
}

void main() {
  testWithoutContext('AdbLogReader filters out ViewPostIme pointer logs (reproduce #45214)', () async {
    const appPid = 1234;
    final processManager = FakeProcessManager.list(<FakeCommand>[
      FakeCommand(
        command: const <String>[
          'adb',
          '-s',
          '1234',
          'shell',
          '-x',
          'logcat',
          '-v',
          'time',
          '-T',
          "'11-27 15:39:04.506'",
        ],
        completer: Completer<void>.sync(),
        stdout:
            '$kDummyLine'
            '05-11 12:54:46.665 W/flutter($appPid): Hello there!\n'
            '05-11 12:54:46.665 I/ViewRootImpl@bd2a991[MainActivity]($appPid): ViewPostIme pointer 0\n'
            '05-11 12:54:46.665 I/ViewRootImpl@bd2a991[MainActivity]($appPid): ViewPostIme pointer 1\n',
      ),
    ]);

    final AdbLogReader logReader = await AdbLogReader.createLogReader(
      createFakeDevice(),
      processManager,
      BufferLogger.test(),
    );
    await logReader.provideVmService(_FakeFlutterVmService(appPid));

    final onDone = Completer<void>.sync();
    final emittedLines = <String>[];
    logReader.logLines.listen((String line) {
      emittedLines.add(line);
    }, onDone: onDone.complete);

    await null; // Allow stream to process
    logReader.dispose();
    await onDone.future;

    // We expect the ViewPostIme logs to be filtered out, so only the flutter log should remain.
    expect(emittedLines, const <String>['W/flutter(1234): Hello there!']);
  });
}

AndroidDevice createFakeDevice() {
  return FakeAndroidDevice();
}

class FakeAndroidDevice extends Fake implements AndroidDevice {
  @override
  String get name => 'test-device';

  @override
  String get displayName => name;

  @override
  Future<String> get apiVersion => Future<String>.value('21');

  @override
  Future<String> lastLogcatTimestamp() async => '11-27 15:39:04.506';

  @override
  List<String> adbCommandForDevice(List<String> command) {
    return <String>['adb', '-s', '1234', ...command];
  }
}
