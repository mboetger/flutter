// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:io' as io;

import 'package:dev_tools/migration_verify.dart';
import 'package:file/local.dart';

void main(List<String> args) async {
  final executor = RealSystemCommandExecutor();
  const fs = LocalFileSystem();
  final runner = MigrationVerifyRunner(executor: executor, fs: fs);

  final int exitCode = await runner.run(args);
  io.exit(exitCode);
}
