// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'project.dart';

class HotRestartFfiBlockedTestProject extends Project {
  @override
  final pubspec = '''
  name: test
  environment:
    sdk: ^3.7.0-0

  dependencies:
    flutter:
      sdk: flutter
  ''';

  @override
  final main = r'''
  import 'dart:async';
  import 'dart:ffi';
  import 'dart:io';
  import 'dart:isolate';

  import 'package:flutter/material.dart';

  class _HelperIsolateMessage {
    _HelperIsolateMessage(this.nativeLibraryPath, this.queueId);
    final String nativeLibraryPath;
    final int queueId;
  }

  void _helperIsolate(_HelperIsolateMessage message) {
    print('[_helperIsolate] Started');
    try {
      final lib = DynamicLibrary.open(message.nativeLibraryPath);
      final waitFn = lib.lookupFunction<Uint8 Function(Int32), int Function(int)>('_library_wait_for_callbacks');
      print('[_helperIsolate] Calling blocked FFI function...');
      final result = waitFn(message.queueId);
      print('[_helperIsolate] FFI function returned: $result');
    } catch (e, st) {
      print('[_helperIsolate] Error: $e\n$st');
    }
    print('[_helperIsolate] Exiting');
  }

  void main() {
    runApp(
      const Center(
        child: Text(
          'Hello, world!',
          key: Key('title'),
          textDirection: TextDirection.ltr,
        ),
      ),
    );

    final libName = Platform.isMacOS ? 'libblocking.dylib' : 'libblocking.so';
    final libPath = '${Directory.current.path}/$libName';
    print('[main] Library path: $libPath');

    Isolate.spawn(
      _helperIsolate,
      _HelperIsolateMessage(libPath, 0),
    );
  }
  ''';
}
