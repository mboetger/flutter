// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:ffi' show Abi;
import 'package:file/memory.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/os.dart';
import 'package:flutter_tools/src/base/platform.dart';

import '../../src/common.dart';
import '../../src/fake_process_manager.dart';

void main() {
  testWithoutContext('OperatingSystemUtils.hostPlatform throws ToolExit on 32-bit Windows', () async {
    final utils = OperatingSystemUtils(
      fileSystem: MemoryFileSystem.test(),
      logger: BufferLogger.test(),
      platform: FakePlatform(operatingSystem: 'windows'),
      processManager: FakeProcessManager.empty(),
      currentAbi: Abi.windowsIA32,
    );

    // This should throw a ToolExit because 32-bit Windows is not supported.
    // Currently, it will throw UnsupportedError, so this expectation will fail.
    expect(() => utils.hostPlatform, throwsToolExit(message: '32-bit Windows is not supported'));
  });
}
